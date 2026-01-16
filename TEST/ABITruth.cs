csharp RANE_Today/src/CIAM/ABITruth.cs
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace RANE.CIAM
{
    // ABI truth stage: produce deterministic, conservative ABI metadata for Frame Planner / Codegen.
    // - Maps TypedCIL types -> sizes/alignments
    // - Emits per-function calling-convention decisions (registers vs stack), estimated frame size
    // - Produces deterministic audit records and writes a {module}.abi.json artifact for CI inspection
    public static class ABITruth
    {
        // High-level ABI summary for a module
        public sealed record AbiModule(string ModuleName, IReadOnlyList<AbiFunction> Functions, DateTime Generated);

        // Per-function ABI decision
        public sealed record AbiFunction(
            string Name,
            string CallingConvention,
            IReadOnlyList<AbiParam> Params,
            AbiReturn Return,
            long StackBytes,
            IReadOnlyList<string> AssignedArgRegisters,
            IReadOnlyList<string> Notes);

        public sealed record AbiParam(string Name, string Type, long Size, long Align, string Passing); // Passing: Reg/Stack/Indirect
        public sealed record AbiReturn(string Type, long Size, long Align, string Passing);

        // Main entry
        public static AbiModule Analyze(TypedCilModule module)
        {
            if (module == null) throw new ArgumentNullException(nameof(module));

            var functions = new List<AbiFunction>();
            var audits = new List<AuditRecord>();
            foreach (var proc in module.Procs ?? Array.Empty<TypedCilProc>())
            {
                var fn = AnalyzeFunction(module, proc, out var fnAudits);
                functions.Add(fn);
                audits.AddRange(fnAudits);
            }

            var abi = new AbiModule(module.ModuleName, functions, GetDeterministicTimestamp());

            // write artifact
            try
            {
                var outPath = $"{module.ModuleName}.abi.json";
                var opts = new JsonSerializerOptions { WriteIndented = true };
                File.WriteAllText(outPath, JsonSerializer.Serialize(abi, opts));
            }
            catch
            {
                // best-effort
            }

            // write audits
            try
            {
                var auditPath = $"{module.ModuleName}.abi.audits.json";
                var doc = new
                {
                    module = module.ModuleName,
                    generated = GetDeterministicTimestamp().ToString("o"),
                    audits = audits.Select(a => new { rule = a.RuleId, matchedText = a.MatchedText, timestamp = a.Timestamp.ToString("o"), summary = a.Summary }).ToArray()
                };
                var opts = new JsonSerializerOptions { WriteIndented = true };
                File.WriteAllText(auditPath, JsonSerializer.Serialize(doc, opts));
            }
            catch { /* best-effort */ }

            return abi;
        }

        private static AbiFunction AnalyzeFunction(TypedCilModule module, TypedCilProc proc, out List<AuditRecord> audits)
        {
            audits = new List<AuditRecord>();
            var callingConvention = "windows_x64"; // default for PE x64
            var argRegs = new[] { "RCX", "RDX", "R8", "R9" }; // Windows x64 integer register order
            var assignedRegs = new List<string>();
            var paramsInfo = new List<AbiParam>();
            long stackBytes = 0;
            var notes = new List<string>();

            // Parse parameter signature from proc.Params "(a:i64, b:i32)" or "()" fallback
            var paramNames = ParseParamNames(proc.Params);

            // Walk parameters from signature; if none, try to infer from body uses (conservative: none)
            for (int i = 0; i < paramNames.Count; i++)
            {
                var pname = paramNames[i].Item1;
                var ptype = paramNames[i].Item2;
                var size = ResolveTypeSize(module, ptype);
                var align = ResolveTypeAlign(module, ptype);
                string passing;
                if (i < argRegs.Length)
                {
                    assignedRegs.Add(argRegs[i]);
                    passing = "Reg";
                }
                else
                {
                    // place on stack: round up to 8 for slot
                    var slot = AlignTo(size, 8);
                    stackBytes += slot;
                    passing = "Stack";
                }
                paramsInfo.Add(new AbiParam(pname, ptype, size, align, passing));
            }

            // Return handling: simple heuristic
            var retType = string.IsNullOrEmpty(proc.RetType) ? "void" : proc.RetType;
            var retSize = ResolveTypeSize(module, retType);
            var retAlign = ResolveTypeAlign(module, retType);
            string retPassing = retSize <= 8 ? "Reg" : "Indirect"; // large returns via hidden pointer (conservative)
            if (retPassing == "Indirect")
            {
                notes.Add("Return passed indirectly via caller-allocated space (hidden pointer)");
                // hidden pointer consumes one implicit first argument register on Windows x64
                if (assignedRegs.Count < argRegs.Length)
                {
                    assignedRegs.Insert(0, "HiddenRetPtr");
                }
                else
                {
                    stackBytes += AlignTo(8, 8); // hidden pointer on stack slot
                }
            }

            // Round stackBytes up to 16 bytes for ABI stack alignment
            stackBytes = AlignTo(stackBytes, 16);

            audits.Add(AuditHelpers.MakeAudit(
                "CIAM.ABITruth.Function",
                $"proc:{proc.Name}",
                0, 0,
                proc.Name,
                $"ABI for '{proc.Name}': convention={callingConvention} params={paramsInfo.Count} stack={stackBytes}"));

            return new AbiFunction(
                proc.Name ?? "anon",
                callingConvention,
                paramsInfo,
                new AbiReturn(retType, retSize, retAlign, retPassing),
                stackBytes,
                assignedRegs,
                notes);
        }

        // Resolve size (bytes) of a typedCil type; conservative defaults for unknown types.
        private static long ResolveTypeSize(TypedCilModule module, string t)
        {
            if (string.IsNullOrEmpty(t)) return 8;
            t = t.Trim();
            return t switch
            {
                "i64" or "int64" or "i64_t" => 8,
                "i32" or "int32" or "i32_t" => 4,
                "u32" => 4,
                "i16" => 2,
                "i8" => 1,
                "string" => 8, // pointer
                "void" => 0,
                _ => ResolveNamedTypeSize(module, t)
            };
        }

        private static long ResolveTypeAlign(TypedCilModule module, string t)
        {
            if (string.IsNullOrEmpty(t)) return 8;
            t = t.Trim();
            return t switch
            {
                "i64" or "int64" or "i64_t" => 8,
                "i32" or "int32" or "i32_t" => 4,
                "u32" => 4,
                "i16" => 2,
                "i8" => 1,
                "string" => 8,
                "void" => 1,
                _ => Math.Min(8, ResolveNamedTypeSize(module, t))
            };
        }

        private static long ResolveNamedTypeSize(TypedCilModule module, string t)
        {
            // Lookup struct size
            if (module?.Structs != null)
            {
                var s = module.Structs.FirstOrDefault(x => string.Equals(x.Name, t, StringComparison.OrdinalIgnoreCase));
                if (s != null)
                {
                    long total = 0;
                    long maxAlign = 1;
                    foreach (var f in s.Fields ?? Array.Empty<TypedCilField>())
                    {
                        var fsize = ResolveTypeSize(module, f.Type);
                        var falign = ResolveTypeAlign(module, f.Type);
                        maxAlign = Math.Max(maxAlign, falign);
                        total = AlignTo(total, falign);
                        total += fsize;
                    }
                    total = AlignTo(total, maxAlign);
                    return Math.Max(1, total);
                }
            }

            // Variant size: tag + max payload
            if (module?.Variants != null)
            {
                var v = module.Variants.FirstOrDefault(x => string.Equals(x.Name, t, StringComparison.OrdinalIgnoreCase));
                if (v != null)
                {
                    long maxPayload = 0;
                    foreach (var c in v.Cases ?? Array.Empty<TypedCilVariantCase>())
                    {
                        var payloadFields = c.PayloadFields ?? Array.Empty<(string FieldName, string FieldType)>();
                        long ptotal = 0;
                        long palign = 1;
                        foreach (var pf in payloadFields)
                        {
                            var fs = ResolveTypeSize(module, pf.FieldType);
                            var fa = ResolveTypeAlign(module, pf.FieldType);
                            palign = Math.Max(palign, fa);
                            ptotal = AlignTo(ptotal, fa);
                            ptotal += fs;
                        }
                        ptotal = AlignTo(ptotal, palign);
                        maxPayload = Math.Max(maxPayload, ptotal);
                    }
                    // tag size conservative 4, align to 8
                    var tagSize = 4;
                    var total = AlignTo(maxPayload, 4) + tagSize;
                    return AlignTo(total, 8);
                }
            }

            // Unknown type: conservative pointer-sized
            return 8;
        }

        private static List<(string, string)> ParseParamNames(string paramsSig)
        {
            var list = new List<(string, string)>();
            if (string.IsNullOrEmpty(paramsSig)) return list;
            var s = paramsSig.Trim();
            if (s.StartsWith("(") && s.EndsWith(")")) s = s[1..^1];
            if (string.IsNullOrWhiteSpace(s)) return list;
            foreach (var part in s.Split(',', StringSplitOptions.RemoveEmptyEntries))
            {
                var kv = part.Split(':', 2);
                if (kv.Length == 2) list.Add((kv[0].Trim(), kv[1].Trim()));
                else list.Add((kv[0].Trim(), "int"));
            }
            return list;
        }

        private static long AlignTo(long v, long align)
        {
            if (align <= 0) return v;
            var rem = v % align;
            return rem == 0 ? v : v + (align - rem);
        }

        private static DateTime GetDeterministicTimestamp()
        {
            try
            {
                var s = Environment.GetEnvironmentVariable("SOURCE_DATE_EPOCH");
                if (!string.IsNullOrEmpty(s) && long.TryParse(s, out var epoch))
                    return DateTimeOffset.FromUnixTimeSeconds(epoch).UtcDateTime;
            }
            catch { }
            return DateTimeOffset.FromUnixTimeSeconds(0).UtcDateTime;
        }
    }
}

