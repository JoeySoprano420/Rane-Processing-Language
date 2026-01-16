using RANE.CIAM;
using System;
using System.Collections.Generic;

csharp RANE_Today/src/CIAM/FramePlanner.cs
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace RANE.CIAM
{
    // Frame Planner
    // - Consumes TypedCilModule + ABITruth outputs and produces deterministic frame plans per function.
    // - Assigns stack offsets for params (stack-passed), locals, spills; estimates prologue/epilogue sizes.
    // - Produces conservative register-save sets and prologue/epilogue notes.
    // - Emits deterministic JSON artifacts and audit records for CI inspection.
    //
    // This implementation intentionally preserves all existing behavior and files; it performs
    // deterministic, conservative planning useful to downstream Codegen.
    public static class FramePlanner
    {
        // Frame model records emitted as JSON
        public sealed record FrameModule(string ModuleName, IReadOnlyList<FrameFunction> Functions, DateTime Generated);
        public sealed record FrameFunction(
            string Name,
            long FrameSize,
            long ShadowSpace,
            long LocalsSize,
            long SpillSize,
            IReadOnlyList<FrameParam> Params,
            IReadOnlyList<FrameLocal> Locals,
            IReadOnlyList<string> CalleeSaved,
            IReadOnlyList<string> PrologueEmits,
            IReadOnlyList<string> EpilogueEmits,
            IReadOnlyDictionary<string,string>? Notes,
            // New: prologue/epilogue machine-code templates (PE and ELF). JSON serializer will base64 encode byte arrays.
            byte[] PrologueTemplatePE,
            byte[] EpilogueTemplatePE,
            byte[] PrologueTemplateELF,
            byte[] EpilogueTemplateELF);

        public sealed record FrameParam(string Name, string Type, string Passing, long? StackOffset, string? Reg);
        public sealed record FrameLocal(string Name, string Type, long Size, long Align, long StackOffset);

        // Main entry: build frames for a module. Optionally accept precomputed ABI module (otherwise we compute).
        public static FrameModule BuildAndWrite(TypedCilModule module, ABITruth.AbiModule? abi = null)
        {
            if (module == null) throw new ArgumentNullException(nameof(module));
            abi ??= ABITruth.Analyze(module);

            var frames = new List<FrameFunction>();
            var audits = new List<AuditRecord>();

            // Build map for quick ABI lookup
            var abiMap = abi.Functions.ToDictionary(f => f.Name, StringComparer.OrdinalIgnoreCase);

            foreach (var proc in module.Procs ?? Array.Empty<TypedCilProc>())
            {
                var fnName = proc.Name ?? "anon";
                abiMap.TryGetValue(fnName, out var fnAbi);

                // Construct params list (preserve order from ABI or fall back to parsed signature)
                var abiParams = fnAbi?.Params?.ToList() ?? new List<ABITruth.AbiParam>();
                var paramsList = new List<FrameParam>();
                long maxParamStackOffset = 0;
                foreach (var p in abiParams)
                {
                    // Map single assigned register deterministically if present
                    string? reg = null;
                    if (p.Passing == "Reg" && fnAbi != null && fnAbi.AssignedArgRegisters != null && fnAbi.AssignedArgRegisters.Count > 0)
                        reg = fnAbi.AssignedArgRegisters.First();
                    paramsList.Add(new FrameParam(p.Name, p.Type, p.Passing, p.Size > 0 ? (long?)p.Size : p.Size == 0 ? 0 : null, reg));
                    // Use ABI's reported stack offsets if present (we can't rely on them here), keep max param sensed size for layout
                    // Keep determinism: compute a conservative param area
                    // (fnAbi earlier reported StackBytes rather than offsets; we bound by sum)
                    if (p.Passing != "Reg")
                    {
                        maxParamStackOffset = Math.Max(maxParamStackOffset, p.Size);
                    }
                }

                // Identify locals: conservative extraction of let names in proc body
                var lets = (proc.Body ?? Array.Empty<TypedCilStmt>()).OfType<TypedCilLet>().ToList();
                var locals = new List<FrameLocal>();
                long localsRegionSize = 0;
                // allocate locals from high addresses downward within locals region
                foreach (var lt in lets)
                {
                    var lsize = ABITruth.ResolveTypeSize(module, lt.Type);
                    var lalign = ABITruth.ResolveTypeAlign(module, lt.Type);
                    localsRegionSize = AlignTo(localsRegionSize, lalign);
                    var offset = localsRegionSize;
                    locals.Add(new FrameLocal(lt.Name, lt.Type, lsize, lalign, offset));
                    localsRegionSize += AlignTo(lsize, lalign);
                }

                // Conservative spills estimate: number of expr statements that are call results or temporaries
                var spillCount = EstimateSpillCount(proc);
                long spillSlotSize = 8;
                long spillRegionSize = spillCount * spillSlotSize;

                // Shadow space from ABI (Windows x64 default 32). Use ABI value if present.
                long shadowSpace = fnAbi != null ? fnAbi.ShadowSpaceBytes : 32;

                // Callee-saved from ABI or conservative default
                var calleeSaved = fnAbi?.CalleeSavedUsed?.ToList() ?? new List<string>();
                if (!calleeSaved.Any())
                {
                    // Conservative fallback: mark standard callee-saved if function has calls or locals
                    if (lets.Count > 0 || (proc.Body ?? Array.Empty<TypedCilStmt>()).Any(s => s is TypedCilCall || s is TypedCilExprStmt es && es.Expr is TypedCilCallExpr))
                        calleeSaved.AddRange(new[] { "RBX", "RBP", "RDI", "RSI", "R12", "R13", "R14", "R15" });
                }

                // Canary: if try/finally or large frame, reserve a canary slot
                bool requiresCanary = (proc.Body ?? Array.Empty<TypedCilStmt>()).Any(s => s is TypedCilTryFinally) || (localsRegionSize + spillRegionSize) > 1024;
                long canarySize = requiresCanary ? 8 : 0;

                // Compute frame layout ordering:
                long offset = 0;
                // shadow space sits at top of caller's frame; keep it accounted separately in FrameSize, not within our offset scheme for locals
                offset += shadowSpace;

                // Reserve space for parameter stack area previously computed
                offset += AlignTo(maxParamStackOffset, 8);

                // Callee-saved area (push area) - conservative size: number of registers * 8
                long calleeSavedArea = calleeSaved.Count * 8;
                offset += calleeSavedArea;

                // Canary
                offset += canarySize;

                // Locals region: place next
                long localsBase = offset;
                offset += AlignTo(localsRegionSize, 8);

                // Spill region
                long spillBase = offset;
                offset += AlignTo(spillRegionSize, 8);

                // Final frame size aligned to 16
                long frameSize = AlignTo(offset, 16);

                // Normalize local offsets to be relative to frame base (positive offsets)
                var normalizedLocals = new List<FrameLocal>();
                foreach (var l in locals)
                {
                    var absOff = localsBase + l.StackOffset;
                    normalizedLocals.Add(new FrameLocal(l.Name, l.Type, l.Size, l.Align, absOff));
                }

                // Build prologue/epilogue emits deterministically (assembly mnemonics)
                var prologue = new List<string>();
                var epilogue = new List<string>();
                // push callee-saved in canonical order
                foreach (var r in calleeSaved)
                {
                    prologue.Add($"push {r}");
                    epilogue.Insert(0, $"pop {r}"); // popped in reverse
                }
                if (requiresCanary)
                {
                    prologue.Add("alloc_canary_slot");
                    epilogue.Insert(0, "check_canary");
                }
                prologue.Add($"sub rsp, {frameSize}  ; allocate frame");
                epilogue.Add($"add rsp, {frameSize}   ; deallocate frame");
                epilogue.Add("ret");

                // Build machine-code prologue/epilogue templates for PE (Windows x64) and ELF (SystemV x64).
                // Templates are conservative minimal prologues; specific immediate stack size adjustments are left for codegen to patch.
                // Prologue: push rbp; mov rbp, rsp
                var prologueBytes = new byte[] { 0x55, 0x48, 0x89, 0xE5 }; // push rbp; mov rbp,rsp
                // Epilogue: leave; ret
                var epilogueBytes = new byte[] { 0xC9, 0xC3 }; // leave; ret

                // For Windows/PE we might add an instruction to reserve space (sub rsp, imm32) - codegen can insert imm32.
                var prologueBytesPE = prologueBytes;
                var epilogueBytesPE = epilogueBytes;

                // For ELF/SystemV x64 the same minimal templates work
                var prologueBytesELF = prologueBytes;
                var epilogueBytesELF = epilogueBytes;

                // Collect notes from ABI or proc annotations
                var notes = new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
                if (proc.Annotations != null)
                {
                    foreach (var kv in proc.Annotations) notes[kv.Key] = kv.Value;
                }
                if (fnAbi != null && fnAbi.Notes != null && fnAbi.Notes.Count > 0)
                    notes["abi.notes"] = string.Join(", ", fnAbi.Notes);

                // Add audit
                audits.Add(AuditHelpers.MakeAudit("CIAM.FramePlanner.FunctionPlanned",
                    $"proc:{fnName}",
                    0, 0,
                    fnName,
                    $"Frame planned: frame={frameSize} locals={localsRegionSize} spills={spillRegionSize} callee_saved={calleeSaved.Count}"));

                // Build FrameFunction record (including machine-code templates)
                frames.Add(new FrameFunction(
                    fnName,
                    frameSize,
                    shadowSpace,
                    localsRegionSize,
                    spillRegionSize,
                    paramsList,
                    normalizedLocals,
                    calleeSaved,
                    prologue,
                    epilogue,
                    notes.Count > 0 ? notes : null,
                    PrologueTemplatePE: prologueBytesPE,
                    EpilogueTemplatePE: epilogueBytesPE,
                    PrologueTemplateELF: prologueBytesELF,
                    EpilogueTemplateELF: epilogueBytesELF));
            }

            var frameModule = new FrameModule(module.ModuleName, frames, GetDeterministicTimestamp());

            // Write artifact JSON
            try
            {
                var outPath = $"{module.ModuleName}.frames.json";
                var opts = new JsonSerializerOptions { WriteIndented = true };
                File.WriteAllText(outPath, JsonSerializer.Serialize(frameModule, opts));
            }
            catch
            {
                // best-effort
            }

            // Write audits
            try
            {
                var auditPath = $"{module.ModuleName}.frames.audits.json";
                var doc = new
                {
                    module = module.ModuleName,
                    generated = GetDeterministicTimestamp().ToString("o"),
                    audits = audits.Select(a => new { rule = a.RuleId, matchedText = a.MatchedText, timestamp = a.Timestamp.ToString("o"), summary = a.Summary }).ToArray()
                };
                var opts = new JsonSerializerOptions { WriteIndented = true };
                File.WriteAllText(auditPath, JsonSerializer.Serialize(doc, opts));
            }
            catch
            {
                // best-effort
            }

            return frameModule;
        }

        // Conservative estimate of spills required: number of call sites + number of expression stmts that are not literals
        private static int EstimateSpillCount(TypedCilProc proc)
        {
            if (proc == null || proc.Body == null) return 0;
            int count = 0;
            foreach (var s in proc.Body)
            {
                if (s is TypedCilCall) count++;
                if (s is TypedCilExprStmt es && !(es.Expr is TypedCilLiteral)) count++;
                if (s is TypedCilReturn r && r.Expr is TypedCilCallExpr) count++;
            }
            // keep deterministic cap to avoid extreme frames
            return Math.Min(count, 256);
        }

        // Reuse same type size/align helpers as ABITruth for determinism
        private static long ResolveTypeSize(TypedCilModule module, string t) => ABITruth_compat.ResolveTypeSize(module, t);
        private static long ResolveTypeAlign(TypedCilModule module, string t) => ABITruth_compat.ResolveTypeAlign(module, t);

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

        // Small compatibility shim that calls ABITruth internal helpers (keeps ABITruth implementation unchanged)
        // If ABITruth helpers become public later, this shim can be removed.
        private static class ABITruth_compat
        {
            public static long ResolveTypeSize(TypedCilModule m, string t) => ABITruth_ReflectInvoke<long>("ResolveTypeSize", m, t);
            public static long ResolveTypeAlign(TypedCilModule m, string t) => ABITruth_ReflectInvoke<long>("ResolveTypeAlign", m, t);

            private static T ABITruth_ReflectInvoke<T>(string methodName, TypedCilModule m, string t)
            {
                try
                {
                    var type = typeof(ABITruth);
                    var mi = type.GetMethod(methodName, System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Static);
                    if (mi != null)
                    {
                        var val = mi.Invoke(null, new object[] { m, t });
                        if (val is T tv) return tv;
                        return (T)Convert.ChangeType(val, typeof(T));
                    }
                }
                catch { }
                // fallback conservative
                if (typeof(T) == typeof(long)) return (T)(object)8L;
                return default!;
            }
        }
    }
}

csharp RANE_Today/src/CIAM/FramePlanner.cs
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace RANE.CIAM
{
    // Frame Planner
    // - Consumes TypedCilModule + ABITruth outputs and produces deterministic frame plans per function.
    // - Assigns stack offsets for params (stack-passed), locals, spills; estimates prologue/epilogue sizes.
    // - Produces conservative register-save sets and prologue/epilogue notes.
    // - Emits deterministic JSON artifacts and audit records for CI inspection.
    //
    // This implementation intentionally preserves all existing behavior and files; it performs
    // deterministic, conservative planning useful to downstream Codegen.
    public static class FramePlanner
    {
        // Frame model records emitted as JSON
        public sealed record FrameModule(string ModuleName, IReadOnlyList<FrameFunction> Functions, DateTime Generated);
        public sealed record FrameFunction(
            string Name,
            long FrameSize,
            long ShadowSpace,
            long LocalsSize,
            long SpillSize,
            IReadOnlyList<FrameParam> Params,
            IReadOnlyList<FrameLocal> Locals,
            IReadOnlyList<string> CalleeSaved,
            IReadOnlyList<string> PrologueEmits,
            IReadOnlyList<string> EpilogueEmits,
            IReadOnlyDictionary<string, string>? Notes,
            // New: prologue/epilogue machine-code templates (PE and ELF). JSON serializer will base64 encode byte arrays.
            byte[] PrologueTemplatePE,
            byte[] EpilogueTemplatePE,
            byte[] PrologueTemplateELF,
            byte[] EpilogueTemplateELF);

        public sealed record FrameParam(string Name, string Type, string Passing, long? StackOffset, string? Reg);
        public sealed record FrameLocal(string Name, string Type, long Size, long Align, long StackOffset);

        // Main entry: build frames for a module. Optionally accept precomputed ABI module (otherwise we compute).
        public static FrameModule BuildAndWrite(TypedCilModule module, ABITruth.AbiModule? abi = null)
        {
            if (module == null) throw new ArgumentNullException(nameof(module));
            abi ??= ABITruth.Analyze(module);

            var frames = new List<FrameFunction>();
            var audits = new List<AuditRecord>();

            // Build map for quick ABI lookup
            var abiMap = abi.Functions.ToDictionary(f => f.Name, StringComparer.OrdinalIgnoreCase);

            foreach (var proc in module.Procs ?? Array.Empty<TypedCilProc>())
            {
                var fnName = proc.Name ?? "anon";
                abiMap.TryGetValue(fnName, out var fnAbi);

                // Construct params list (preserve order from ABI or fall back to parsed signature)
                var abiParams = fnAbi?.Params?.ToList() ?? new List<ABITruth.AbiParam>();
                var paramsList = new List<FrameParam>();
                long maxParamStackOffset = 0;
                foreach (var p in abiParams)
                {
                    // Map single assigned register deterministically if present
                    string? reg = null;
                    if (p.Passing == "Reg" && fnAbi != null && fnAbi.AssignedArgRegisters != null && fnAbi.AssignedArgRegisters.Count > 0)
                        reg = fnAbi.AssignedArgRegisters.First();
                    paramsList.Add(new FrameParam(p.Name, p.Type, p.Passing, p.Size > 0 ? (long?)p.Size : p.Size == 0 ? 0 : null, reg));
                    // Use ABI's reported stack offsets if present (we can't rely on them here), keep max param sensed size for layout
                    // Keep determinism: compute a conservative param area
                    // (fnAbi earlier reported StackBytes rather than offsets; we bound by sum)
                    if (p.Passing != "Reg")
                    {
                        maxParamStackOffset = Math.Max(maxParamStackOffset, p.Size);
                    }
                }

                // Identify locals: conservative extraction of let names in proc body
                var lets = (proc.Body ?? Array.Empty<TypedCilStmt>()).OfType<TypedCilLet>().ToList();
                var locals = new List<FrameLocal>();
                long localsRegionSize = 0;
                // allocate locals from high addresses downward within locals region
                foreach (var lt in lets)
                {
                    var lsize = ABITruth.ResolveTypeSize(module, lt.Type);
                    var lalign = ABITruth.ResolveTypeAlign(module, lt.Type);
                    localsRegionSize = AlignTo(localsRegionSize, lalign);
                    var offset = localsRegionSize;
                    locals.Add(new FrameLocal(lt.Name, lt.Type, lsize, lalign, offset));
                    localsRegionSize += AlignTo(lsize, lalign);
                }

                // Conservative spills estimate: number of expr statements that are call results or temporaries
                var spillCount = EstimateSpillCount(proc);
                long spillSlotSize = 8;
                long spillRegionSize = spillCount * spillSlotSize;

                // Shadow space from ABI (Windows x64 default 32). Use ABI value if present.
                long shadowSpace = fnAbi != null ? fnAbi.ShadowSpaceBytes : 32;

                // Callee-saved from ABI or conservative default
                var calleeSaved = fnAbi?.CalleeSavedUsed?.ToList() ?? new List<string>();
                if (!calleeSaved.Any())
                {
                    // Conservative fallback: mark standard callee-saved if function has calls or locals
                    if (lets.Count > 0 || (proc.Body ?? Array.Empty<TypedCilStmt>()).Any(s => s is TypedCilCall || s is TypedCilExprStmt es && es.Expr is TypedCilCallExpr))
                        calleeSaved.AddRange(new[] { "RBX", "RBP", "RDI", "RSI", "R12", "R13", "R14", "R15" });
                }

                // Canary: if try/finally or large frame, reserve a canary slot
                bool requiresCanary = (proc.Body ?? Array.Empty<TypedCilStmt>()).Any(s => s is TypedCilTryFinally) || (localsRegionSize + spillRegionSize) > 1024;
                long canarySize = requiresCanary ? 8 : 0;

                // Compute frame layout ordering:
                long offset = 0;
                // shadow space sits at top of caller's frame; keep it accounted separately in FrameSize, not within our offset scheme for locals
                offset += shadowSpace;

                // Reserve space for parameter stack area previously computed
                offset += AlignTo(maxParamStackOffset, 8);

                // Callee-saved area (push area) - conservative size: number of registers * 8
                long calleeSavedArea = calleeSaved.Count * 8;
                offset += calleeSavedArea;

                // Canary
                offset += canarySize;

                // Locals region: place next
                long localsBase = offset;
                offset += AlignTo(localsRegionSize, 8);

                // Spill region
                long spillBase = offset;
                offset += AlignTo(spillRegionSize, 8);

                // Final frame size aligned to 16
                long frameSize = AlignTo(offset, 16);

                // Normalize local offsets to be relative to frame base (positive offsets)
                var normalizedLocals = new List<FrameLocal>();
                foreach (var l in locals)
                {
                    var absOff = localsBase + l.StackOffset;
                    normalizedLocals.Add(new FrameLocal(l.Name, l.Type, l.Size, l.Align, absOff));
                }

                // Build prologue/epilogue emits deterministically (assembly mnemonics)
                var prologue = new List<string>();
                var epilogue = new List<string>();
                // push callee-saved in canonical order
                foreach (var r in calleeSaved)
                {
                    prologue.Add($"push {r}");
                    epilogue.Insert(0, $"pop {r}"); // popped in reverse
                }
                if (requiresCanary)
                {
                    prologue.Add("alloc_canary_slot");
                    epilogue.Insert(0, "check_canary");
                }
                prologue.Add($"sub rsp, {frameSize}  ; allocate frame");
                epilogue.Add($"add rsp, {frameSize}   ; deallocate frame");
                epilogue.Add("ret");

                // Build machine-code prologue/epilogue templates for PE (Windows x64) and ELF (SystemV x64).
                // Templates are conservative minimal prologues; specific immediate stack size adjustments are left for codegen to patch.
                // Prologue: push rbp; mov rbp, rsp
                var prologueBytes = new byte[] { 0x55, 0x48, 0x89, 0xE5 }; // push rbp; mov rbp,rsp
                // Epilogue: leave; ret
                var epilogueBytes = new byte[] { 0xC9, 0xC3 }; // leave; ret

                // For Windows/PE we might add an instruction to reserve space (sub rsp, imm32) - codegen can insert imm32.
                var prologueBytesPE = prologueBytes;
                var epilogueBytesPE = epilogueBytes;

                // For ELF/SystemV x64 the same minimal templates work
                var prologueBytesELF = prologueBytes;
                var epilogueBytesELF = epilogueBytes;

                // Collect notes from ABI or proc annotations
                var notes = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                if (proc.Annotations != null)
                {
                    foreach (var kv in proc.Annotations) notes[kv.Key] = kv.Value;
                }
                if (fnAbi != null && fnAbi.Notes != null && fnAbi.Notes.Count > 0)
                    notes["abi.notes"] = string.Join(", ", fnAbi.Notes);

                // Add audit
                audits.Add(AuditHelpers.MakeAudit("CIAM.FramePlanner.FunctionPlanned",
                    $"proc:{fnName}",
                    0, 0,
                    fnName,
                    $"Frame planned: frame={frameSize} locals={localsRegionSize} spills={spillRegionSize} callee_saved={calleeSaved.Count}"));

                // Build FrameFunction record (including machine-code templates)
                frames.Add(new FrameFunction(
                    fnName,
                    frameSize,
                    shadowSpace,
                    localsRegionSize,
                    spillRegionSize,
                    paramsList,
                    normalizedLocals,
                    calleeSaved,
                    prologue,
                    epilogue,
                    notes.Count > 0 ? notes : null,
                    PrologueTemplatePE: prologueBytesPE,
                    EpilogueTemplatePE: epilogueBytesPE,
                    PrologueTemplateELF: prologueBytesELF,
                    EpilogueTemplateELF: epilogueBytesELF));
            }

            var frameModule = new FrameModule(module.ModuleName, frames, GetDeterministicTimestamp());

            // Write artifact JSON
            try
            {
                var outPath = $"{module.ModuleName}.frames.json";
                var opts = new JsonSerializerOptions { WriteIndented = true };
                File.WriteAllText(outPath, JsonSerializer.Serialize(frameModule, opts));
            }
            catch
            {
                // best-effort
            }

            // Write audits
            try
            {
                var auditPath = $"{module.ModuleName}.frames.audits.json";
                var doc = new
                {
                    module = module.ModuleName,
                    generated = GetDeterministicTimestamp().ToString("o"),
                    audits = audits.Select(a => new { rule = a.RuleId, matchedText = a.MatchedText, timestamp = a.Timestamp.ToString("o"), summary = a.Summary }).ToArray()
                };
                var opts = new JsonSerializerOptions { WriteIndented = true };
                File.WriteAllText(auditPath, JsonSerializer.Serialize(doc, opts));
            }
            catch
            {
                // best-effort
            }

            return frameModule;
        }

        // Conservative estimate of spills required: number of call sites + number of expression stmts that are not literals
        private static int EstimateSpillCount(TypedCilProc proc)
        {
            if (proc == null || proc.Body == null) return 0;
            int count = 0;
            foreach (var s in proc.Body)
            {
                if (s is TypedCilCall) count++;
                if (s is TypedCilExprStmt es && !(es.Expr is TypedCilLiteral)) count++;
                if (s is TypedCilReturn r && r.Expr is TypedCilCallExpr) count++;
            }
            // keep deterministic cap to avoid extreme frames
            return Math.Min(count, 256);
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

csharp RANE_Today/tests/FramePlannerTests.cs
using System;
using System.IO;
using System.Linq;
using RANE.CIAM;
using Xunit;

namespace RANE.Tests
{
    public class FramePlannerTests
    {
        [Fact]
        public void FramePlanner_Produces_Frame_With_PrologueTemplates()
        {
            // Create a minimal module with one proc that has a couple of lets and a return
            var body = new TypedCilStmt[]
            {
                new TypedCilLet("x", "i64", new TypedCilLiteral("42")),
                new TypedCilLet("y", "i64", new TypedCilBinary("+", new TypedCilIdentifier("x"), new TypedCilLiteral("1"))),
                new TypedCilReturn(new TypedCilIdentifier("y"))
            };

            var proc = new TypedCilProc("fp_test", "public", "i64", "()", Array.Empty<string>(), body, Annotations: null);

            var module = new TypedCilModule(
                "frameplanner_test",
                Array.Empty<TypedCilImport>(),
                Array.Empty<TypedCilType>(),
                Array.Empty<TypedCilStruct>(),
                Array.Empty<TypedCilEnum>(),
                Array.Empty<TypedCilVariant>(),
                Array.Empty<TypedCilMMIO>(),
                Array.Empty<TypedCilCapability>(),
                new[] { proc },
                Array.Empty<TypedCilNode>());

            // Run ABITruth which will also invoke FramePlanner via the hook
            var abi = ABITruth.Analyze(module);

            // Also call FramePlanner directly and inspect results
            var fm = FramePlanner.BuildAndWrite(module, abi);

            var fn = fm.Functions.FirstOrDefault(f => f.Name == "fp_test");
            Assert.NotNull(fn);
            Assert.True(fn.FrameSize >= 0);
            Assert.NotNull(fn.PrologueTemplatePE);
            Assert.NotNull(fn.EpilogueTemplatePE);
            Assert.True(fn.PrologueTemplatePE.Length > 0);
            Assert.True(fn.EpilogueTemplatePE.Length > 0);

            // Ensure per-function JSON artifact was written
            var expectedJson = Path.Combine(Directory.GetCurrentDirectory(), "frameplanner_test.frames.json");
            // artifact path uses module name; file may be in working dir - check existence
            Assert.True(File.Exists("frameplanner_test.frames.json") || File.Exists(Path.Combine(Directory.GetCurrentDirectory(), "frameplanner_test.frames.json")));
        }
    }
}

csharp RANE_Today/src/CIAM/FramePlanner.cs
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace RANE.CIAM
{
    // Frame Planner
    // - Consumes TypedCilModule + ABITruth outputs and produces deterministic frame plans per function.
    // - Assigns stack offsets for params (stack-passed), locals, spills; estimates prologue/epilogue sizes.
    // - Produces conservative register-save sets and prologue/epilogue notes.
    // - Emits deterministic JSON artifacts and audit records for CI inspection.
    //
    // This implementation intentionally preserves all existing behavior and files; it performs
    // deterministic, conservative planning useful to downstream Codegen.
    public static class FramePlanner
    {
        // Frame model records emitted as JSON
        public sealed record FrameModule(string ModuleName, IReadOnlyList<FrameFunction> Functions, DateTime Generated);
        public sealed record FrameFunction(
            string Name,
            long FrameSize,
            long ShadowSpace,
            long LocalsSize,
            long SpillSize,
            IReadOnlyList<FrameParam> Params,
            IReadOnlyList<FrameLocal> Locals,
            IReadOnlyList<string> CalleeSaved,
            IReadOnlyList<string> PrologueEmits,
            IReadOnlyList<string> EpilogueEmits,
            IReadOnlyDictionary<string, string>? Notes,
            // New: prologue/epilogue machine-code templates (PE and ELF). JSON serializer will base64 encode byte arrays.
            byte[] PrologueTemplatePE,
            byte[] EpilogueTemplatePE,
            byte[] PrologueTemplateELF,
            byte[] EpilogueTemplateELF);

        public sealed record FrameParam(string Name, string Type, string Passing, long? StackOffset, string? Reg);
        public sealed record FrameLocal(string Name, string Type, long Size, long Align, long StackOffset);

        // Main entry: build frames for a module. Optionally accept precomputed ABI module (otherwise we compute).
        public static FrameModule BuildAndWrite(TypedCilModule module, ABITruth.AbiModule? abi = null)
        {
            if (module == null) throw new ArgumentNullException(nameof(module));
            abi ??= ABITruth.Analyze(module);

            var frames = new List<FrameFunction>();
            var audits = new List<AuditRecord>();

            // Build map for quick ABI lookup
            var abiMap = abi.Functions.ToDictionary(f => f.Name, StringComparer.OrdinalIgnoreCase);

            foreach (var proc in module.Procs ?? Array.Empty<TypedCilProc>())
            {
                var fnName = proc.Name ?? "anon";
                abiMap.TryGetValue(fnName, out var fnAbi);

                // Construct params list (preserve order from ABI or fall back to parsed signature)
                var abiParams = fnAbi?.Params?.ToList() ?? new List<ABITruth.AbiParam>();
                var paramsList = new List<FrameParam>();
                long maxParamStackOffset = 0;
                foreach (var p in abiParams)
                {
                    // Map single assigned register deterministically if present
                    string? reg = null;
                    if (p.Passing == "Reg" && fnAbi != null && fnAbi.AssignedArgRegisters != null && fnAbi.AssignedArgRegisters.Count > 0)
                        reg = fnAbi.AssignedArgRegisters.First();
                    paramsList.Add(new FrameParam(p.Name, p.Type, p.Passing, p.Size > 0 ? (long?)p.Size : p.Size == 0 ? 0 : null, reg));
                    // Use ABI's reported stack offsets if present (we can't rely on them here), keep max param sensed size for layout
                    // Keep determinism: compute a conservative param area
                    // (fnAbi earlier reported StackBytes rather than offsets; we bound by sum)
                    if (p.Passing != "Reg")
                    {
                        maxParamStackOffset = Math.Max(maxParamStackOffset, p.Size);
                    }
                }

                // Identify locals: conservative extraction of let names in proc body
                var lets = (proc.Body ?? Array.Empty<TypedCilStmt>()).OfType<TypedCilLet>().ToList();
                var locals = new List<FrameLocal>();
                long localsRegionSize = 0;
                // allocate locals from high addresses downward within locals region
                foreach (var lt in lets)
                {
                    var lsize = ABITruth.ResolveTypeSize(module, lt.Type);
                    var lalign = ABITruth.ResolveTypeAlign(module, lt.Type);
                    localsRegionSize = AlignTo(localsRegionSize, lalign);
                    var offset = localsRegionSize;
                    locals.Add(new FrameLocal(lt.Name, lt.Type, lsize, lalign, offset));
                    localsRegionSize += AlignTo(lsize, lalign);
                }

                // Conservative spills estimate: number of expr statements that are call results or temporaries
                var spillCount = EstimateSpillCount(proc);
                long spillSlotSize = 8;
                long spillRegionSize = spillCount * spillSlotSize;

                // Shadow space from ABI (Windows x64 default 32). Use ABI value if present.
                long shadowSpace = fnAbi != null ? fnAbi.ShadowSpaceBytes : 32;

                // Callee-saved from ABI or conservative default
                var calleeSaved = fnAbi?.CalleeSavedUsed?.ToList() ?? new List<string>();
                if (!calleeSaved.Any())
                {
                    // Conservative fallback: mark standard callee-saved if function has calls or locals
                    if (lets.Count > 0 || (proc.Body ?? Array.Empty<TypedCilStmt>()).Any(s => s is TypedCilCall || s is TypedCilExprStmt es && es.Expr is TypedCilCallExpr))
                        calleeSaved.AddRange(new[] { "RBX", "RBP", "RDI", "RSI", "R12", "R13", "R14", "R15" });
                }

                // Canary: if try/finally or large frame, reserve a canary slot
                bool requiresCanary = (proc.Body ?? Array.Empty<TypedCilStmt>()).Any(s => s is TypedCilTryFinally) || (localsRegionSize + spillRegionSize) > 1024;
                long canarySize = requiresCanary ? 8 : 0;

                // Compute frame layout ordering:
                long offset = 0;
                // shadow space sits at top of caller's frame; keep it accounted separately in FrameSize, not within our offset scheme for locals
                offset += shadowSpace;

                // Reserve space for parameter stack area previously computed
                offset += AlignTo(maxParamStackOffset, 8);

                // Callee-saved area (push area) - conservative size: number of registers * 8
                long calleeSavedArea = calleeSaved.Count * 8;
                offset += calleeSavedArea;

                // Canary
                offset += canarySize;

                // Locals region: place next
                long localsBase = offset;
                offset += AlignTo(localsRegionSize, 8);

                // Spill region
                long spillBase = offset;
                offset += AlignTo(spillRegionSize, 8);

                // Final frame size aligned to 16
                long frameSize = AlignTo(offset, 16);

                // Normalize local offsets to be relative to frame base (positive offsets)
                var normalizedLocals = new List<FrameLocal>();
                foreach (var l in locals)
                {
                    var absOff = localsBase + l.StackOffset;
                    normalizedLocals.Add(new FrameLocal(l.Name, l.Type, l.Size, l.Align, absOff));
                }

                // Build prologue/epilogue emits deterministically (assembly mnemonics)
                var prologue = new List<string>();
                var epilogue = new List<string>();
                // push callee-saved in canonical order
                foreach (var r in calleeSaved)
                {
                    prologue.Add($"push {r}");
                    epilogue.Insert(0, $"pop {r}"); // popped in reverse
                }
                if (requiresCanary)
                {
                    prologue.Add("alloc_canary_slot");
                    epilogue.Insert(0, "check_canary");
                }
                prologue.Add($"sub rsp, {frameSize}  ; allocate frame");
                epilogue.Add($"add rsp, {frameSize}   ; deallocate frame");
                epilogue.Add("ret");

                // Build machine-code prologue/epilogue templates for PE (Windows x64) and ELF (SystemV x64).
                // Use helper to produce final bytes for the concrete frame size so downstream tests/codegen can validate/patch.
                var prologueBytesPE = MakePrologueBytesPE(frameSize);
                var epilogueBytesPE = MakeEpilogueBytesPE(frameSize);

                var prologueBytesELF = MakePrologueBytesELF(frameSize);
                var epilogueBytesELF = MakeEpilogueBytesELF(frameSize);

                // Collect notes from ABI or proc annotations
                var notes = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                if (proc.Annotations != null)
                {
                    foreach (var kv in proc.Annotations) notes[kv.Key] = kv.Value;
                }
                if (fnAbi != null && fnAbi.Notes != null && fnAbi.Notes.Count > 0)
                    notes["abi.notes"] = string.Join(", ", fnAbi.Notes);

                // Add audit
                audits.Add(AuditHelpers.MakeAudit("CIAM.FramePlanner.FunctionPlanned",
                    $"proc:{fnName}",
                    0, 0,
                    fnName,
                    $"Frame planned: frame={frameSize} locals={localsRegionSize} spills={spillRegionSize} callee_saved={calleeSaved.Count}"));

                // Build FrameFunction record (including machine-code templates)
                frames.Add(new FrameFunction(
                    fnName,
                    frameSize,
                    shadowSpace,
                    localsRegionSize,
                    spillRegionSize,
                    paramsList,
                    normalizedLocals,
                    calleeSaved,
                    prologue,
                    epilogue,
                    notes.Count > 0 ? notes : null,
                    PrologueTemplatePE: prologueBytesPE,
                    EpilogueTemplatePE: epilogueBytesPE,
                    PrologueTemplateELF: prologueBytesELF,
                    EpilogueTemplateELF: epilogueBytesELF));
            }

            var frameModule = new FrameModule(module.ModuleName, frames, GetDeterministicTimestamp());

            // Write artifact JSON
            try
            {
                var outPath = $"{module.ModuleName}.frames.json";
                var opts = new JsonSerializerOptions { WriteIndented = true };
                File.WriteAllText(outPath, JsonSerializer.Serialize(frameModule, opts));
            }
            catch
            {
                // best-effort
            }

            // Write audits
            try
            {
                var auditPath = $"{module.ModuleName}.frames.audits.json";
                var doc = new
                {
                    module = module.ModuleName,
                    generated = GetDeterministicTimestamp().ToString("o"),
                    audits = audits.Select(a => new { rule = a.RuleId, matchedText = a.MatchedText, timestamp = a.Timestamp.ToString("o"), summary = a.Summary }).ToArray()
                };
                var opts = new JsonSerializerOptions { WriteIndented = true };
                File.WriteAllText(auditPath, JsonSerializer.Serialize(doc, opts));
            }
            catch
            {
                // best-effort
            }

            return frameModule;
        }

        // Conservative estimate of spills required: number of call sites + number of expression stmts that are not literals
        private static int EstimateSpillCount(TypedCilProc proc)
        {
            if (proc == null || proc.Body == null) return 0;
            int count = 0;
            foreach (var s in proc.Body)
            {
                if (s is TypedCilCall) count++;
                if (s is TypedCilExprStmt es && !(es.Expr is TypedCilLiteral)) count++;
                if (s is TypedCilReturn r && r.Expr is TypedCilCallExpr) count++;
            }
            // keep deterministic cap to avoid extreme frames
            return Math.Min(count, 256);
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

        // -----------------------
        // Prologue/Epilogue template helpers
        // -----------------------
        // Create a concrete prologue byte sequence for Windows x64 PE codegen:
        // push rbp; mov rbp,rsp; sub rsp, imm32
        public static byte[] MakePrologueBytesPE(long frameSize)
        {
            // push rbp (0x55)
            // mov rbp,rsp (0x48 0x89 0xE5)
            // sub rsp, imm32 (0x48 0x81 0xEC <imm32 little-endian>)
            var imm = (uint)frameSize;
            var bytes = new List<byte> { 0x55, 0x48, 0x89, 0xE5, 0x48, 0x81, 0xEC };
            bytes.AddRange(BitConverter.GetBytes(imm));
            return bytes.ToArray();
        }

        // Create corresponding epilogue: add rsp, imm32; pop rbp; ret
        public static byte[] MakeEpilogueBytesPE(long frameSize)
        {
            // add rsp, imm32 (0x48 0x81 0xC4 <imm32>)
            // pop rbp (0x5D)
            // ret (0xC3)
            var imm = (uint)frameSize;
            var bytes = new List<byte> { 0x48, 0x81, 0xC4 };
            bytes.AddRange(BitConverter.GetBytes(imm));
            bytes.AddRange(new byte[] { 0x5D, 0xC3 });
            return bytes.ToArray();
        }

        // For SystemV/ELF x64 we produce the same logical template (push rbp; mov rbp,rsp; sub rsp, imm32)
        public static byte[] MakePrologueBytesELF(long frameSize) => MakePrologueBytesPE(frameSize);
        public static byte[] MakeEpilogueBytesELF(long frameSize) => MakeEpilogueBytesPE(frameSize);
    }
}

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace RANE.CIAM
{
    // Frame Planner
    // - Consumes TypedCilModule + ABITruth outputs and produces deterministic frame plans per function.
    // - Assigns stack offsets for params (stack-passed), locals, spills; estimates prologue/epilogue sizes.
    // - Produces conservative register-save sets and prologue/epilogue notes.
    // - Emits deterministic JSON artifacts and audit records for CI inspection.
    //
    // This implementation intentionally preserves all existing behavior and files; it performs
    // deterministic, conservative planning useful to downstream Codegen.
    public static class FramePlanner
    {
        // Frame model records emitted as JSON
        public sealed record FrameModule(string ModuleName, IReadOnlyList<FrameFunction> Functions, DateTime Generated);
        public sealed record FrameFunction(
            string Name,
            long FrameSize,
            long ShadowSpace,
            long LocalsSize,
            long SpillSize,
            IReadOnlyList<FrameParam> Params,
            IReadOnlyList<FrameLocal> Locals,
            IReadOnlyList<string> CalleeSaved,
            IReadOnlyList<string> PrologueEmits,
            IReadOnlyList<string> EpilogueEmits,
            IReadOnlyDictionary<string, string>? Notes,
            // New: prologue/epilogue machine-code templates (PE and ELF). JSON serializer will base64 encode byte arrays.
            byte[] PrologueTemplatePE,
            byte[] EpilogueTemplatePE,
            byte[] PrologueTemplateELF,
            byte[] EpilogueTemplateELF);

        public sealed record FrameParam(string Name, string Type, string Passing, long? StackOffset, string? Reg);
        public sealed record FrameLocal(string Name, string Type, long Size, long Align, long StackOffset);

        // Main entry: build frames for a module. Optionally accept precomputed ABI module (otherwise we compute).
        public static FrameModule BuildAndWrite(TypedCilModule module, ABITruth.AbiModule? abi = null)
        {
            if (module == null) throw new ArgumentNullException(nameof(module));
            abi ??= ABITruth.Analyze(module);

            var frames = new List<FrameFunction>();
            var audits = new List<AuditRecord>();

            // Build map for quick ABI lookup
            var abiMap = abi.Functions.ToDictionary(f => f.Name, StringComparer.OrdinalIgnoreCase);

            foreach (var proc in module.Procs ?? Array.Empty<TypedCilProc>())
            {
                var fnName = proc.Name ?? "anon";
                abiMap.TryGetValue(fnName, out var fnAbi);

                // Construct params list (preserve order from ABI or fall back to parsed signature)
                var abiParams = fnAbi?.Params?.ToList() ?? new List<ABITruth.AbiParam>();
                var paramsList = new List<FrameParam>();
                long maxParamStackOffset = 0;
                foreach (var p in abiParams)
                {
                    // Map single assigned register deterministically if present
                    string? reg = null;
                    if (p.Passing == "Reg" && fnAbi != null && fnAbi.AssignedArgRegisters != null && fnAbi.AssignedArgRegisters.Count > 0)
                        reg = fnAbi.AssignedArgRegisters.First();
                    paramsList.Add(new FrameParam(p.Name, p.Type, p.Passing, p.Size > 0 ? (long?)p.Size : p.Size == 0 ? 0 : null, reg));
                    // Use ABI's reported stack offsets if present (we can't rely on them here), keep max param sensed size for layout
                    // Keep determinism: compute a conservative param area
                    // (fnAbi earlier reported StackBytes rather than offsets; we bound by sum)
                    if (p.Passing != "Reg")
                    {
                        maxParamStackOffset = Math.Max(maxParamStackOffset, p.Size);
                    }
                }

                // Identify locals: conservative extraction of let names in proc body
                var lets = (proc.Body ?? Array.Empty<TypedCilStmt>()).OfType<TypedCilLet>().ToList();
                var locals = new List<FrameLocal>();
                long localsRegionSize = 0;
                // allocate locals from high addresses downward within locals region
                foreach (var lt in lets)
                {
                    var lsize = ABITruth.ResolveTypeSize(module, lt.Type);
                    var lalign = ABITruth.ResolveTypeAlign(module, lt.Type);
                    localsRegionSize = AlignTo(localsRegionSize, lalign);
                    var offset = localsRegionSize;
                    locals.Add(new FrameLocal(lt.Name, lt.Type, lsize, lalign, offset));
                    localsRegionSize += AlignTo(lsize, lalign);
                }

                // Conservative spills estimate: number of expr statements that are call results or temporaries
                var spillCount = EstimateSpillCount(proc);
                long spillSlotSize = 8;
                long spillRegionSize = spillCount * spillSlotSize;

                // Shadow space from ABI (Windows x64 default 32). Use ABI value if present.
                long shadowSpace = fnAbi != null ? fnAbi.ShadowSpaceBytes : 32;

                // Callee-saved from ABI or conservative default
                var calleeSaved = fnAbi?.CalleeSavedUsed?.ToList() ?? new List<string>();
                if (!calleeSaved.Any())
                {
                    // Conservative fallback: mark standard callee-saved if function has calls or locals
                    if (lets.Count > 0 || (proc.Body ?? Array.Empty<TypedCilStmt>()).Any(s => s is TypedCilCall || s is TypedCilExprStmt es && es.Expr is TypedCilCallExpr))
                        calleeSaved.AddRange(new[] { "RBX", "RBP", "RDI", "RSI", "R12", "R13", "R14", "R15" });
                }

                // Canary: if try/finally or large frame, reserve a canary slot
                bool requiresCanary = (proc.Body ?? Array.Empty<TypedCilStmt>()).Any(s => s is TypedCilTryFinally) || (localsRegionSize + spillRegionSize) > 1024;
                long canarySize = requiresCanary ? 8 : 0;

                // Compute frame layout ordering:
                long offset = 0;
                // shadow space sits at top of caller's frame; keep it accounted separately in FrameSize, not within our offset scheme for locals
                offset += shadowSpace;

                // Reserve space for parameter stack area previously computed
                offset += AlignTo(maxParamStackOffset, 8);

                // Callee-saved area (push area) - conservative size: number of registers * 8
                long calleeSavedArea = calleeSaved.Count * 8;
                offset += calleeSavedArea;

                // Canary
                offset += canarySize;

                // Locals region: place next
                long localsBase = offset;
                offset += AlignTo(localsRegionSize, 8);

                // Spill region
                long spillBase = offset;
                offset += AlignTo(spillRegionSize, 8);

                // Final frame size aligned to 16
                long frameSize = AlignTo(offset, 16);

                // Normalize local offsets to be relative to frame base (positive offsets)
                var normalizedLocals = new List<FrameLocal>();
                foreach (var l in locals)
                {
                    var absOff = localsBase + l.StackOffset;
                    normalizedLocals.Add(new FrameLocal(l.Name, l.Type, l.Size, l.Align, absOff));
                }

                // Build prologue/epilogue emits deterministically (assembly mnemonics)
                var prologue = new List<string>();
                var epilogue = new List<string>();
                // push callee-saved in canonical order
                foreach (var r in calleeSaved)
                {
                    prologue.Add($"push {r}");
                    epilogue.Insert(0, $"pop {r}"); // popped in reverse
                }
                if (requiresCanary)
                {
                    prologue.Add("alloc_canary_slot");
                    epilogue.Insert(0, "check_canary");
                }
                prologue.Add($"sub rsp, {frameSize}  ; allocate frame");
                epilogue.Add($"add rsp, {frameSize}   ; deallocate frame");
                epilogue.Add("ret");

                // Build machine-code prologue/epilogue templates for PE (Windows x64) and ELF (SystemV x64).
                // Use helper to produce final bytes for the concrete frame size so downstream tests/codegen can validate/patch.
                var prologueBytesPE = MakePrologueBytesPE(frameSize);
                var epilogueBytesPE = MakeEpilogueBytesPE(frameSize);

                var prologueBytesELF = MakePrologueBytesELF(frameSize);
                var epilogueBytesELF = MakeEpilogueBytesELF(frameSize);

                // Collect notes from ABI or proc annotations
                var notes = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                if (proc.Annotations != null)
                {
                    foreach (var kv in proc.Annotations) notes[kv.Key] = kv.Value;
                }
                if (fnAbi != null && fnAbi.Notes != null && fnAbi.Notes.Count > 0)
                    notes["abi.notes"] = string.Join(", ", fnAbi.Notes);

                // Add audit
                audits.Add(AuditHelpers.MakeAudit("CIAM.FramePlanner.FunctionPlanned",
                    $"proc:{fnName}",
                    0, 0,
                    fnName,
                    $"Frame planned: frame={frameSize} locals={localsRegionSize} spills={spillRegionSize} callee_saved={calleeSaved.Count}"));

                // Build FrameFunction record (including machine-code templates)
                frames.Add(new FrameFunction(
                    fnName,
                    frameSize,
                    shadowSpace,
                    localsRegionSize,
                    spillRegionSize,
                    paramsList,
                    normalizedLocals,
                    calleeSaved,
                    prologue,
                    epilogue,
                    notes.Count > 0 ? notes : null,
                    PrologueTemplatePE: prologueBytesPE,
                    EpilogueTemplatePE: epilogueBytesPE,
                    PrologueTemplateELF: prologueBytesELF,
                    EpilogueTemplateELF: epilogueBytesELF));
            }

            var frameModule = new FrameModule(module.ModuleName, frames, GetDeterministicTimestamp());

            // Write artifact JSON
            try
            {
                var outPath = $"{module.ModuleName}.frames.json";
                var opts = new JsonSerializerOptions { WriteIndented = true };
                File.WriteAllText(outPath, JsonSerializer.Serialize(frameModule, opts));
            }
            catch
            {
                // best-effort
            }

            // Write audits
            try
            {
                var auditPath = $"{module.ModuleName}.frames.audits.json";
                var doc = new
                {
                    module = module.ModuleName,
                    generated = GetDeterministicTimestamp().ToString("o"),
                    audits = audits.Select(a => new { rule = a.RuleId, matchedText = a.MatchedText, timestamp = a.Timestamp.ToString("o"), summary = a.Summary }).ToArray()
                };
                var opts = new JsonSerializerOptions { WriteIndented = true };
                File.WriteAllText(auditPath, JsonSerializer.Serialize(doc, opts));
            }
            catch
            {
                // best-effort
            }

            return frameModule;
        }

        // Conservative estimate of spills required: number of call sites + number of expression stmts that are not literals
        private static int EstimateSpillCount(TypedCilProc proc)
        {
            if (proc == null || proc.Body == null) return 0;
            int count = 0;
            foreach (var s in proc.Body)
            {
                if (s is TypedCilCall) count++;
                if (s is TypedCilExprStmt es && !(es.Expr is TypedCilLiteral)) count++;
                if (s is TypedCilReturn r && r.Expr is TypedCilCallExpr) count++;
            }
            // keep deterministic cap to avoid extreme frames
            return Math.Min(count, 256);
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

        // -----------------------
        // Prologue/Epilogue template helpers
        // -----------------------
        // Create a concrete prologue byte sequence for Windows x64 PE codegen:
        // push rbp; mov rbp,rsp; sub rsp, imm32 (or imm8 short form when possible)
        public static byte[] MakePrologueBytesPE(long frameSize)
        {
            // push rbp (0x55)
            // mov rbp,rsp (0x48 0x89 0xE5)
            var bytes = new List<byte> { 0x55, 0x48, 0x89, 0xE5 };

            if (frameSize > 0 && frameSize <= 127)
            {
                // short encoding: 48 83 EC imm8
                bytes.AddRange(new byte[] { 0x48, 0x83, 0xEC, (byte)frameSize });
            }
            else if (frameSize > 0)
            {
                // full encoding: 48 81 EC imm32 (little-endian)
                bytes.AddRange(new byte[] { 0x48, 0x81, 0xEC });
                bytes.AddRange(BitConverter.GetBytes((uint)frameSize));
            }
            // if frameSize == 0 we omit sub instruction

            return bytes.ToArray();
        }

        // Create corresponding epilogue: add rsp, imm32 (or imm8) ; pop rbp; ret
        public static byte[] MakeEpilogueBytesPE(long frameSize)
        {
            var bytes = new List<byte>();

            if (frameSize > 0 && frameSize <= 127)
            {
                // short encoding: 48 83 C4 imm8
                bytes.AddRange(new byte[] { 0x48, 0x83, 0xC4, (byte)frameSize });
            }
            else if (frameSize > 0)
            {
                // full encoding: 48 81 C4 imm32
                bytes.AddRange(new byte[] { 0x48, 0x81, 0xC4 });
                bytes.AddRange(BitConverter.GetBytes((uint)frameSize));
            }
            // pop rbp (0x5D)
            bytes.Add(0x5D);
            // ret (0xC3)
            bytes.Add(0xC3);

            return bytes.ToArray();
        }

        // For SystemV/ELF x64 we produce the same logical template (use short imm8 when possible)
        public static byte[] MakePrologueBytesELF(long frameSize) => MakePrologueBytesPE(frameSize);
        public static byte[] MakeEpilogueBytesELF(long frameSize) => MakeEpilogueBytesPE(frameSize);
    }
}

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace RANE.CIAM
{
    // Frame Planner
    // - Consumes TypedCilModule + ABITruth outputs and produces deterministic frame plans per function.
    // - Assigns stack offsets for params (stack-passed), locals, spills; estimates prologue/epilogue sizes.
    // - Produces conservative register-save sets and prologue/epilogue notes.
    // - Emits deterministic JSON artifacts and audit records for CI inspection.
    //
    // This implementation intentionally preserves all existing behavior and files; it performs
    // deterministic, conservative planning useful to downstream Codegen.
    public static class FramePlanner
    {
        // Frame model records emitted as JSON
        public sealed record FrameModule(string ModuleName, IReadOnlyList<FrameFunction> Functions, DateTime Generated);
        public sealed record FrameFunction(
            string Name,
            long FrameSize,
            long ShadowSpace,
            long LocalsSize,
            long SpillSize,
            IReadOnlyList<FrameParam> Params,
            IReadOnlyList<FrameLocal> Locals,
            IReadOnlyList<string> CalleeSaved,
            IReadOnlyList<string> PrologueEmits,
            IReadOnlyList<string> EpilogueEmits,
            IReadOnlyDictionary<string, string>? Notes,
            // New: prologue/epilogue machine-code templates (PE and ELF). JSON serializer will base64 encode byte arrays.
            byte[] PrologueTemplatePE,
            byte[] EpilogueTemplatePE,
            byte[] PrologueTemplateELF,
            byte[] EpilogueTemplateELF);

        public sealed record FrameParam(string Name, string Type, string Passing, long? StackOffset, string? Reg);
        public sealed record FrameLocal(string Name, string Type, long Size, long Align, long StackOffset);

        // Main entry: build frames for a module. Optionally accept precomputed ABI module (otherwise we compute).
        public static FrameModule BuildAndWrite(TypedCilModule module, ABITruth.AbiModule? abi = null)
        {
            if (module == null) throw new ArgumentNullException(nameof(module));
            abi ??= ABITruth.Analyze(module);

            var frames = new List<FrameFunction>();
            var audits = new List<AuditRecord>();

            // Build map for quick ABI lookup
            var abiMap = abi.Functions.ToDictionary(f => f.Name, StringComparer.OrdinalIgnoreCase);

            foreach (var proc in module.Procs ?? Array.Empty<TypedCilProc>())
            {
                var fnName = proc.Name ?? "anon";
                abiMap.TryGetValue(fnName, out var fnAbi);

                // Construct params list (preserve order from ABI or fall back to parsed signature)
                var abiParams = fnAbi?.Params?.ToList() ?? new List<ABITruth.AbiParam>();
                var paramsList = new List<FrameParam>();
                long maxParamStackOffset = 0;
                foreach (var p in abiParams)
                {
                    // Map single assigned register deterministically if present
                    string? reg = null;
                    if (p.Passing == "Reg" && fnAbi != null && fnAbi.AssignedArgRegisters != null && fnAbi.AssignedArgRegisters.Count > 0)
                        reg = fnAbi.AssignedArgRegisters.First();
                    paramsList.Add(new FrameParam(p.Name, p.Type, p.Passing, p.Size > 0 ? (long?)p.Size : p.Size == 0 ? 0 : null, reg));
                    // Use ABI's reported stack offsets if present (we can't rely on them here), keep max param sensed size for layout
                    // Keep determinism: compute a conservative param area
                    // (fnAbi earlier reported StackBytes rather than offsets; we bound by sum)
                    if (p.Passing != "Reg")
                    {
                        maxParamStackOffset = Math.Max(maxParamStackOffset, p.Size);
                    }
                }

                // Identify locals: conservative extraction of let names in proc body
                var lets = (proc.Body ?? Array.Empty<TypedCilStmt>()).OfType<TypedCilLet>().ToList();
                var locals = new List<FrameLocal>();
                long localsRegionSize = 0;
                // allocate locals from high addresses downward within locals region
                foreach (var lt in lets)
                {
                    var lsize = ABITruth.ResolveTypeSize(module, lt.Type);
                    var lalign = ABITruth.ResolveTypeAlign(module, lt.Type);
                    localsRegionSize = AlignTo(localsRegionSize, lalign);
                    var offset = localsRegionSize;
                    locals.Add(new FrameLocal(lt.Name, lt.Type, lsize, lalign, offset));
                    localsRegionSize += AlignTo(lsize, lalign);
                }

                // Conservative spills estimate: number of expr statements that are call results or temporaries
                var spillCount = EstimateSpillCount(proc);
                long spillSlotSize = 8;
                long spillRegionSize = spillCount * spillSlotSize;

                // Shadow space from ABI (Windows x64 default 32). Use ABI value if present.
                long shadowSpace = fnAbi != null ? fnAbi.ShadowSpaceBytes : 32;

                // Callee-saved from ABI or conservative default
                var calleeSaved = fnAbi?.CalleeSavedUsed?.ToList() ?? new List<string>();
                if (!calleeSaved.Any())
                {
                    // Conservative fallback: mark standard callee-saved if function has calls or locals
                    if (lets.Count > 0 || (proc.Body ?? Array.Empty<TypedCilStmt>()).Any(s => s is TypedCilCall || s is TypedCilExprStmt es && es.Expr is TypedCilCallExpr))
                        calleeSaved.AddRange(new[] { "RBX", "RBP", "RDI", "RSI", "R12", "R13", "R14", "R15" });
                }

                // Canary: if try/finally or large frame, reserve a canary slot
                bool requiresCanary = (proc.Body ?? Array.Empty<TypedCilStmt>()).Any(s => s is TypedCilTryFinally) || (localsRegionSize + spillRegionSize) > 1024;
                long canarySize = requiresCanary ? 8 : 0;

                // Compute frame layout ordering:
                long offset = 0;
                // shadow space sits at top of caller's frame; keep it accounted separately in FrameSize, not within our offset scheme for locals
                offset += shadowSpace;

                // Reserve space for parameter stack area previously computed
                offset += AlignTo(maxParamStackOffset, 8);

                // Callee-saved area (push area) - conservative size: number of registers * 8
                long calleeSavedArea = calleeSaved.Count * 8;
                offset += calleeSavedArea;

                // Canary
                offset += canarySize;

                // Locals region: place next
                long localsBase = offset;
                offset += AlignTo(localsRegionSize, 8);

                // Spill region
                long spillBase = offset;
                offset += AlignTo(spillRegionSize, 8);

                // Final frame size aligned to 16
                long frameSize = AlignTo(offset, 16);

                // Normalize local offsets to be relative to frame base (positive offsets)
                var normalizedLocals = new List<FrameLocal>();
                foreach (var l in locals)
                {
                    var absOff = localsBase + l.StackOffset;
                    normalizedLocals.Add(new FrameLocal(l.Name, l.Type, l.Size, l.Align, absOff));
                }

                // Build prologue/epilogue emits deterministically (assembly mnemonics)
                var prologue = new List<string>();
                var epilogue = new List<string>();
                // push callee-saved in canonical order
                foreach (var r in calleeSaved)
                {
                    prologue.Add($"push {r}");
                    epilogue.Insert(0, $"pop {r}"); // popped in reverse
                }
                if (requiresCanary)
                {
                    prologue.Add("alloc_canary_slot");
                    epilogue.Insert(0, "check_canary");
                }
                prologue.Add($"sub rsp, {frameSize}  ; allocate frame");
                epilogue.Add($"add rsp, {frameSize}   ; deallocate frame");
                epilogue.Add("ret");

                // Build machine-code prologue/epilogue templates for PE (Windows x64) and ELF (SystemV x64).
                // Use helper to produce final bytes for the concrete frame size so downstream tests/codegen can validate/patch.
                var prologueBytesPE = MakePrologueBytesPE(frameSize);
                var epilogueBytesPE = MakeEpilogueBytesPE(frameSize);

                var prologueBytesELF = MakePrologueBytesELF(frameSize);
                var epilogueBytesELF = MakeEpilogueBytesELF(frameSize);

                // Collect notes from ABI or proc annotations
                var notes = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                if (proc.Annotations != null)
                {
                    foreach (var kv in proc.Annotations) notes[kv.Key] = kv.Value;
                }
                if (fnAbi != null && fnAbi.Notes != null && fnAbi.Notes.Count > 0)
                    notes["abi.notes"] = string.Join(", ", fnAbi.Notes);

                // Add audit
                audits.Add(AuditHelpers.MakeAudit("CIAM.FramePlanner.FunctionPlanned",
                    $"proc:{fnName}",
                    0, 0,
                    fnName,
                    $"Frame planned: frame={frameSize} locals={localsRegionSize} spills={spillRegionSize} callee_saved={calleeSaved.Count}"));

                // Build FrameFunction record (including machine-code templates)
                frames.Add(new FrameFunction(
                    fnName,
                    frameSize,
                    shadowSpace,
                    localsRegionSize,
                    spillRegionSize,
                    paramsList,
                    normalizedLocals,
                    calleeSaved,
                    prologue,
                    epilogue,
                    notes.Count > 0 ? notes : null,
                    PrologueTemplatePE: prologueBytesPE,
                    EpilogueTemplatePE: epilogueBytesPE,
                    PrologueTemplateELF: prologueBytesELF,
                    EpilogueTemplateELF: epilogueBytesELF));
            }

            var frameModule = new FrameModule(module.ModuleName, frames, GetDeterministicTimestamp());

            // Write artifact JSON
            try
            {
                var outPath = $"{module.ModuleName}.frames.json";
                var opts = new JsonSerializerOptions { WriteIndented = true };
                File.WriteAllText(outPath, JsonSerializer.Serialize(frameModule, opts));
            }
            catch
            {
                // best-effort
            }

            // Write audits
            try
            {
                var auditPath = $"{module.ModuleName}.frames.audits.json";
                var doc = new
                {
                    module = module.ModuleName,
                    generated = GetDeterministicTimestamp().ToString("o"),
                    audits = audits.Select(a => new { rule = a.RuleId, matchedText = a.MatchedText, timestamp = a.Timestamp.ToString("o"), summary = a.Summary }).ToArray()
                };
                var opts = new JsonSerializerOptions { WriteIndented = true };
                File.WriteAllText(auditPath, JsonSerializer.Serialize(doc, opts));
            }
            catch
            {
                // best-effort
            }

            return frameModule;
        }

        // Conservative estimate of spills required: number of call sites + number of expression stmts that are not literals
        private static int EstimateSpillCount(TypedCilProc proc)
        {
            if (proc == null || proc.Body == null) return 0;
            int count = 0;
            foreach (var s in proc.Body)
            {
                if (s is TypedCilCall) count++;
                if (s is TypedCilExprStmt es && !(es.Expr is TypedCilLiteral)) count++;
                if (s is TypedCilReturn r && r.Expr is TypedCilCallExpr) count++;
            }
            // keep deterministic cap to avoid extreme frames
            return Math.Min(count, 256);
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

        // -----------------------
        // Prologue/Epilogue template helpers
        // -----------------------
        // Create a concrete prologue byte sequence for Windows x64 PE codegen:
        // push rbp; mov rbp,rsp; sub rsp, imm32 (or imm8 short form when possible)
        public static byte[] MakePrologueBytesPE(long frameSize)
        {
            // push rbp (0x55)
            // mov rbp,rsp (0x48 0x89 0xE5)
            var bytes = new List<byte> { 0x55, 0x48, 0x89, 0xE5 };

            if (frameSize > 0 && frameSize <= 127)
            {
                // short encoding: 48 83 EC imm8
                bytes.AddRange(new byte[] { 0x48, 0x83, 0xEC, (byte)frameSize });
            }
            else if (frameSize > 0)
            {
                // full encoding: 48 81 EC imm32 (little-endian)
                bytes.AddRange(new byte[] { 0x48, 0x81, 0xEC });
                bytes.AddRange(BitConverter.GetBytes((uint)frameSize));
            }
            // if frameSize == 0 we omit sub instruction

            return bytes.ToArray();
        }

        // Create corresponding epilogue: add rsp, imm32 (or imm8) ; pop rbp; ret
        public static byte[] MakeEpilogueBytesPE(long frameSize)
        {
            var bytes = new List<byte>();

            if (frameSize > 0 && frameSize <= 127)
            {
                // short encoding: 48 83 C4 imm8
                bytes.AddRange(new byte[] { 0x48, 0x83, 0xC4, (byte)frameSize });
            }
            else if (frameSize > 0)
            {
                // full encoding: 48 81 C4 imm32
                bytes.AddRange(new byte[] { 0x48, 0x81, 0xC4 });
                bytes.AddRange(BitConverter.GetBytes((uint)frameSize));
            }
            // pop rbp (0x5D)
            bytes.Add(0x5D);
            // ret (0xC3)
            bytes.Add(0xC3);

            return bytes.ToArray();
        }

        // For SystemV/ELF x64 we produce the same logical template (use short imm8 when possible)
        public static byte[] MakePrologueBytesELF(long frameSize) => MakePrologueBytesPE(frameSize);
        public static byte[] MakeEpilogueBytesELF(long frameSize) => MakeEpilogueBytesPE(frameSize);

        // -----------------------
        // Template patching helpers
        // -----------------------
        // Patch a prologue template (PE) to use a new frame size; detects short (imm8) vs full (imm32) encodings and adjusts output bytes.
        // Returns a new byte[] with the relocated immediate encoded and instruction encoding chosen deterministically (short when newFrameSize <= 127).
        public static byte[] PatchPrologueTemplatePE(byte[] template, long newFrameSize)
        {
            if (template == null) return MakePrologueBytesPE(newFrameSize);
            // pattern for sub rsp: 48 83 EC imm8  OR 48 81 EC imm32
            int idx = IndexOfSequence(template, new byte[] { 0x48, 0x83, 0xEC });
            bool shortForm = true;
            int origImmLen = 0;
            if (idx >= 0)
            {
                shortForm = true;
                origImmLen = 1;
            }
            else
            {
                idx = IndexOfSequence(template, new byte[] { 0x48, 0x81, 0xEC });
                if (idx >= 0)
                {
                    shortForm = false;
                    origImmLen = 4;
                }
            }

            if (idx < 0)
            {
                // no sub instruction found — just return freshly generated prologue for new size
                return MakePrologueBytesPE(newFrameSize);
            }

            var prefix = template.Take(idx).ToArray();
            var suffix = template.Skip(idx + 3 + origImmLen).ToArray();

            byte[] instr;
            if (newFrameSize == 0)
            {
                // omit instruction entirely
                instr = Array.Empty<byte>();
            }
            else if (newFrameSize <= 127)
            {
                instr = new byte[] { 0x48, 0x83, 0xEC, (byte)newFrameSize };
            }
            else
            {
                var imm = BitConverter.GetBytes((uint)newFrameSize);
                instr = new byte[3 + 4];
                instr[0] = 0x48; instr[1] = 0x81; instr[2] = 0xEC;
                Array.Copy(imm, 0, instr, 3, 4);
            }

            var outBytes = new List<byte>();
            outBytes.AddRange(prefix);
            outBytes.AddRange(instr);
            outBytes.AddRange(suffix);
            return outBytes.ToArray();
        }

        public static byte[] PatchEpilogueTemplatePE(byte[] template, long newFrameSize)
        {
            if (template == null) return MakeEpilogueBytesPE(newFrameSize);
            // pattern for add rsp: 48 83 C4 imm8  OR 48 81 C4 imm32
            int idx = IndexOfSequence(template, new byte[] { 0x48, 0x83, 0xC4 });
            int origImmLen = 0;
            if (idx >= 0) origImmLen = 1;
            else
            {
                idx = IndexOfSequence(template, new byte[] { 0x48, 0x81, 0xC4 });
                if (idx >= 0) origImmLen = 4;
            }

            if (idx < 0)
            {
                // no add instruction found — return fresh epilogue
                return MakeEpilogueBytesPE(newFrameSize);
            }

            var prefix = template.Take(idx).ToArray();
            var suffix = template.Skip(idx + 3 + origImmLen).ToArray();

            byte[] instr;
            if (newFrameSize == 0)
            {
                instr = Array.Empty<byte>();
            }
            else if (newFrameSize <= 127)
            {
                instr = new byte[] { 0x48, 0x83, 0xC4, (byte)newFrameSize };
            }
            else
            {
                var imm = BitConverter.GetBytes((uint)newFrameSize);
                instr = new byte[3 + 4];
                instr[0] = 0x48; instr[1] = 0x81; instr[2] = 0xC4;
                Array.Copy(imm, 0, instr, 3, 4);
            }

            var outBytes = new List<byte>();
            outBytes.AddRange(prefix);
            outBytes.AddRange(instr);
            outBytes.AddRange(suffix);
            return outBytes.ToArray();
        }

        // Generic sequence finder
        private static int IndexOfSequence(byte[] haystack, byte[] needle)
        {
            if (haystack == null || needle == null || haystack.Length == 0 || needle.Length == 0 || needle.Length > haystack.Length) return -1;
            for (int i = 0; i <= haystack.Length - needle.Length; i++)
            {
                bool ok = true;
                for (int j = 0; j < needle.Length; j++)
                {
                    if (haystack[i + j] != needle[j]) { ok = false; break; }
                }
                if (ok) return i;
            }
            return -1;
        }

        // Helpers for ELF templates (alias to PE for now)
        public static byte[] PatchPrologueTemplateELF(byte[] template, long newFrameSize) => PatchPrologueTemplatePE(template, newFrameSize);
        public static byte[] PatchEpilogueTemplateELF(byte[] template, long newFrameSize) => PatchEpilogueTemplatePE(template, newFrameSize);
    }
}

csharp RANE_Today/src/CIAM/FramePlanner.cs
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace RANE.CIAM
{
    // Conservative FramePlanner for TypedCilModule:
    // - Computes per-function frame size from locals/spills and shadow space.
    // - Marks procs that use heap allocations (TypedCilAlloc/Maloc) to avoid reserving them on stack.
    // - Emits deterministic JSON artifact "{module}.frames.json" describing frame decisions.
    public static class FramePlanner
    {
        public sealed record FrameModule(string ModuleName, IReadOnlyList<FrameFunction> Functions, DateTime Generated);
        public sealed record FrameFunction(
            string Name,
            long FrameSize,
            long LocalsSize,
            long SpillSize,
            bool UsesHeapAllocs,
            IReadOnlyDictionary<string, string>? Notes,
            byte[] PrologueTemplatePE,
            byte[] EpilogueTemplatePE);

        // Main entry
        public static FrameModule BuildAndWrite(TypedCilModule module)
        {
            if (module == null) throw new ArgumentNullException(nameof(module));

            var frames = new List<FrameFunction>();
            foreach (var proc in module.Procs ?? Array.Empty<TypedCilProc>())
            {
                long localsRegion = 0;
                int spillCount = 0;
                bool usesHeap = false;
                var notes = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

                if (proc.Body != null)
                {
                    foreach (var s in proc.Body)
                    {
                        if (s is TypedCilLet lt)
                        {
                            // estimate size from declared type
                            var sz = ResolveTypeSize(lt.Type);
                            var align = 8L;
                            localsRegion = AlignTo(localsRegion, align);
                            localsRegion += AlignTo(sz, align);
                        }
                        else if (s is TypedCilAlloc || s is TypedCilMaloc)
                        {
                            usesHeap = true;
                        }
                        else if (s is TypedCilCall || s is TypedCilSpawn || s is TypedCilAsync || s is TypedCilAwait)
                        {
                            spillCount += 1;
                        }
                        else if (s is TypedCilLoop)
                        {
                            // loops may need spills
                            spillCount += 1;
                        }
                    }
                }

                long spillRegion = spillCount * 8;
                long shadowSpace = 32;
                long calleeSavedArea = 8 * 2; // conservative: push rbp + one reg
                long offset = shadowSpace + calleeSavedArea + AlignTo(localsRegion, 8) + AlignTo(spillRegion, 8);
                long frameSize = AlignTo(offset, 16);

                if (usesHeap)
                {
                    notes["frame.allocs"] = "heap";
                }

                notes["locals.bytes"] = localsRegion.ToString();
                notes["spills.count"] = spillCount.ToString();

                var pro = MakePrologueBytesPE(frameSize);
                var epi = MakeEpilogueBytesPE(frameSize);

                frames.Add(new FrameFunction(proc.Name ?? "anon", frameSize, localsRegion, spillRegion, usesHeap, notes, pro, epi));
            }

            var fm = new FrameModule(module.ModuleName, frames, GetDeterministicTimestamp());

            try
            {
                var outPath = $"{module.ModuleName}.frames.json";
                var opts = new JsonSerializerOptions { WriteIndented = true };
                File.WriteAllText(outPath, JsonSerializer.Serialize(fm, opts));
            }
            catch { /* best-effort */ }

            return fm;
        }

        private static long AlignTo(long v, long align)
        {
            if (align <= 0) return v;
            var rem = v % align;
            return rem == 0 ? v : v + (align - rem);
        }

        private static long ResolveTypeSize(string t)
        {
            if (string.IsNullOrEmpty(t)) return 8;
            t = t.Trim().ToLowerInvariant();
            if (t.StartsWith("i") && long.TryParse(t.Substring(1), out _)) return 8;
            if (t == "f64" || t == "double") return 8;
            if (t.EndsWith("*")) return 8;
            if (t == "i32" || t == "int32") return 4;
            return 8;
        }

        private static DateTime GetDeterministicTimestamp()
        {
            try
            {
                var s = Environment.GetEnvironmentVariable("SOURCE_DATE_EPOCH");
                if (!string.IsNullOrEmpty(s) && long.TryParse(s, out var epoch)) return DateTimeOffset.FromUnixTimeSeconds(epoch).UtcDateTime;
            }
            catch { }
            return DateTimeOffset.FromUnixTimeSeconds(0).UtcDateTime;
        }

        // Prologue/Epilogue helpers (PE / Windows x64), with short imm8 when possible (<=127)
        public static byte[] MakePrologueBytesPE(long frameSize)
        {
            var bytes = new List<byte> { 0x55, 0x48, 0x89, 0xE5 }; // push rbp; mov rbp,rsp
            if (frameSize > 0 && frameSize <= 127)
            {
                bytes.AddRange(new byte[] { 0x48, 0x83, 0xEC, (byte)frameSize }); // sub rsp, imm8
            }
            else if (frameSize > 0)
            {
                bytes.AddRange(new byte[] { 0x48, 0x81, 0xEC });
                bytes.AddRange(BitConverter.GetBytes((uint)frameSize));
            }
            return bytes.ToArray();
        }

        public static byte[] MakeEpilogueBytesPE(long frameSize)
        {
            var bytes = new List<byte>();
            if (frameSize > 0 && frameSize <= 127)
            {
                bytes.AddRange(new byte[] { 0x48, 0x83, 0xC4, (byte)frameSize }); // add rsp, imm8
            }
            else if (frameSize > 0)
            {
                bytes.AddRange(new byte[] { 0x48, 0x81, 0xC4 });
                bytes.AddRange(BitConverter.GetBytes((uint)frameSize));
            }
            bytes.Add(0x5D); // pop rbp
            bytes.Add(0xC3); // ret
            return bytes.ToArray();
        }
    }
}

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace RANE.CIAM
{
    // Conservative FramePlanner for TypedCilModule:
    // - Computes per-function frame size from locals/spills and shadow space.
    // - Marks procs that use heap allocations (TypedCilAlloc/Maloc) to avoid reserving them on stack.
    // - Annotates per-variable placement (notes keyed as "local.<name>" -> "heap" | "stack:<size>")
    // - Emits deterministic JSON artifact "{module}.frames.json" describing frame decisions.
    public static class FramePlanner
    {
        public sealed record FrameModule(string ModuleName, IReadOnlyList<FrameFunction> Functions, DateTime Generated);
        public sealed record FrameFunction(
            string Name,
            long FrameSize,
            long LocalsSize,
            long SpillSize,
            bool UsesHeapAllocs,
            IReadOnlyDictionary<string, string>? Notes,
            byte[] PrologueTemplatePE,
            byte[] EpilogueTemplatePE);

        // Main entry
        public static FrameModule BuildAndWrite(TypedCilModule module)
        {
            if (module == null) throw new ArgumentNullException(nameof(module));

            var frames = new List<FrameFunction>();
            foreach (var proc in module.Procs ?? Array.Empty<TypedCilProc>())
            {
                long localsRegion = 0;
                int spillCount = 0;
                bool usesHeap = false;
                var notes = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

                // per-variable placements
                var localPlacements = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

                if (proc.Body != null)
                {
                    foreach (var s in proc.Body)
                    {
                        if (s is TypedCilLet lt)
                        {
                            // estimate size from declared type (fallback) or initializer literal length
                            var sz = ResolveTypeSize(lt.Type);
                            if (lt.Expr is TypedCilLiteral lit)
                            {
                                // if string literal, size = length+1
                                var v = lit.Value ?? "";
                                if ((v.StartsWith("\"") && v.EndsWith("\"")) || (v.StartsWith("'") && v.EndsWith("'")))
                                {
                                    var inner = v[1..^1];
                                    sz = Math.Max(sz, inner.Length + 1);
                                }
                            }

                            // Heuristic: If a variable is pointer-like or large (>128) prefer heap placement
                            var preferHeap = sz > 128 || (lt.Type != null && (lt.Type.EndsWith("*") || lt.Type.IndexOf("array", StringComparison.OrdinalIgnoreCase) >= 0));
                            if (preferHeap)
                            {
                                localPlacements[lt.Name ?? $"_anon{localPlacements.Count}"] = "heap";
                                usesHeap = true;
                            }
                            else
                            {
                                // place on stack, track byte size and alignment
                                localPlacements[lt.Name ?? $"_anon{localPlacements.Count}"] = $"stack:{sz}";
                                var align = 8L;
                                localsRegion = AlignTo(localsRegion, align);
                                localsRegion += AlignTo(sz, align);
                            }
                        }
                        else if (s is TypedCilAlloc || s is TypedCilMaloc)
                        {
                            usesHeap = true;
                        }
                        else if (s is TypedCilCall || s is TypedCilSpawn || s is TypedCilAsync || s is TypedCilAwait)
                        {
                            spillCount += 1;
                        }
                        else if (s is TypedCilLoop)
                        {
                            // loops may need spills
                            spillCount += 1;
                        }
                    }
                }

                long spillRegion = spillCount * 8;
                long shadowSpace = 32;
                long calleeSavedArea = 8 * 2; // conservative: push rbp + one reg
                long offset = shadowSpace + calleeSavedArea + AlignTo(localsRegion, 8) + AlignTo(spillRegion, 8);
                long frameSize = AlignTo(offset, 16);

                if (usesHeap)
                {
                    notes["frame.allocs"] = "heap";
                }

                notes["locals.bytes"] = localsRegion.ToString();
                notes["spills.count"] = spillCount.ToString();

                // embed per-variable decisions into Notes with deterministic keys
                foreach (var kv in localPlacements.OrderBy(kv => kv.Key, StringComparer.Ordinal))
                    notes[$"local.{kv.Key}"] = kv.Value;

                var pro = MakePrologueBytesPE(frameSize);
                var epi = MakeEpilogueBytesPE(frameSize);

                frames.Add(new FrameFunction(proc.Name ?? "anon", frameSize, localsRegion, spillRegion, usesHeap, notes, pro, epi));
            }

            var fm = new FrameModule(module.ModuleName, frames, GetDeterministicTimestamp());

            try
            {
                var outPath = $"{module.ModuleName}.frames.json";
                var opts = new JsonSerializerOptions { WriteIndented = true };
                File.WriteAllText(outPath, JsonSerializer.Serialize(fm, opts));
            }
            catch { /* best-effort */ }

            return fm;
        }

        private static long AlignTo(long v, long align)
        {
            if (align <= 0) return v;
            var rem = v % align;
            return rem == 0 ? v : v + (align - rem);
        }

        private static long ResolveTypeSize(string t)
        {
            if (string.IsNullOrEmpty(t)) return 8;
            t = t.Trim().ToLowerInvariant();
            if (t.StartsWith("i") && long.TryParse(t.Substring(1), out _)) return 8;
            if (t == "f64" || t == "double") return 8;
            if (t.EndsWith("*")) return 8;
            if (t == "i32" || t == "int32") return 4;
            // simple array notation like "u8[128]" or "u8[]" fallback
            var idx = t.IndexOf('[');
            if (idx > 0)
            {
                var inner = t[(idx + 1)..^1];
                if (int.TryParse(inner, out var n)) return n;
            }
            return 8;
        }

        private static DateTime GetDeterministicTimestamp()
        {
            try
            {
                var s = Environment.GetEnvironmentVariable("SOURCE_DATE_EPOCH");
                if (!string.IsNullOrEmpty(s) && long.TryParse(s, out var epoch)) return DateTimeOffset.FromUnixTimeSeconds(epoch).UtcDateTime;
            }
            catch { }
            return DateTimeOffset.FromUnixTimeSeconds(0).UtcDateTime;
        }

        // Prologue/Epilogue helpers (PE / Windows x64), with short imm8 when possible (<=127)
        public static byte[] MakePrologueBytesPE(long frameSize)
        {
            var bytes = new List<byte> { 0x55, 0x48, 0x89, 0xE5 }; // push rbp; mov rbp,rsp
            if (frameSize > 0 && frameSize <= 127)
            {
                bytes.AddRange(new byte[] { 0x48, 0x83, 0xEC, (byte)frameSize }); // sub rsp, imm8
            }
            else if (frameSize > 0)
            {
                bytes.AddRange(new byte[] { 0x48, 0x81, 0xEC });
                bytes.AddRange(BitConverter.GetBytes((uint)frameSize));
            }
            return bytes.ToArray();
        }

        public static byte[] MakeEpilogueBytesPE(long frameSize)
        {
            var bytes = new List<byte>();
            if (frameSize > 0 && frameSize <= 127)
            {
                bytes.AddRange(new byte[] { 0x48, 0x83, 0xC4, (byte)frameSize }); // add rsp, imm8
            }
            else if (frameSize > 0)
            {
                bytes.AddRange(new byte[] { 0x48, 0x81, 0xC4 });
                bytes.AddRange(BitConverter.GetBytes((uint)frameSize));
            }
            bytes.Add(0x5D); // pop rbp
            bytes.Add(0xC3); // ret
            return bytes.ToArray();
        }
    }
}

csharp RANE_Today/src/CIAM/FramePlanner.cs
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace RANE.CIAM
{
    // Conservative FramePlanner for TypedCilModule:
    // - Computes per-function frame size from locals/spills and shadow space.
    // - Marks procs that use heap allocations (TypedCilAlloc/Maloc) to avoid reserving them on stack.
    // - Annotates per-variable placement (notes keyed as "local.<name>" -> "heap" | "stack:<size>")
    // - Emits deterministic JSON artifact "{module}.frames.json" describing frame decisions.
    public static class FramePlanner
    {
        public sealed record FrameModule(string ModuleName, IReadOnlyList<FrameFunction> Functions, DateTime Generated);
        public sealed record FrameFunction(
            string Name,
            long FrameSize,
            long LocalsSize,
            long SpillSize,
            bool UsesHeapAllocs,
            IReadOnlyDictionary<string, string>? Notes,
            byte[] PrologueTemplatePE,
            byte[] EpilogueTemplatePE);

        // Main entry
        public static FrameModule BuildAndWrite(TypedCilModule module)
        {
            if (module == null) throw new ArgumentNullException(nameof(module));

            var frames = new List<FrameFunction>();
            foreach (var proc in module.Procs ?? Array.Empty<TypedCilProc>())
            {
                long localsRegion = 0;
                int spillCount = 0;
                bool usesHeap = false;
                var notes = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

                // per-variable placements
                var localPlacements = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

                // consult resolver-provided per-local hints if present (resolver attaches local.<name> entries)
                var procAnn = proc.Annotations;

                if (proc.Body != null)
                {
                    foreach (var s in proc.Body)
                    {
                        if (s is TypedCilLet lt)
                        {
                            // If resolver already specified placement for this let, honor it
                            var key = $"local.{lt.Name}";
                            if (procAnn != null && procAnn.TryGetValue(key, out var provided))
                            {
                                localPlacements[lt.Name ?? $"_anon{localPlacements.Count}"] = provided;
                                if (string.Equals(provided, "heap", StringComparison.OrdinalIgnoreCase)) usesHeap = true;
                                else if (provided.StartsWith("stack:", StringComparison.OrdinalIgnoreCase))
                                {
                                    var szStr = provided.Split(':', 2)[1];
                                    if (long.TryParse(szStr, out var sz))
                                    {
                                        var align = 8L;
                                        localsRegion = AlignTo(localsRegion, align);
                                        localsRegion += AlignTo(sz, align);
                                    }
                                }
                                continue;
                            }

                            // estimate size from declared type (fallback) or initializer literal length
                            var sz = ResolveTypeSize(lt.Type);
                            if (lt.Expr is TypedCilLiteral lit)
                            {
                                // if string literal, size = length+1
                                var v = lit.Value ?? "";
                                if ((v.StartsWith("\"") && v.EndsWith("\"")) || (v.StartsWith("'") && v.EndsWith("'")))
                                {
                                    var inner = v[1..^1];
                                    sz = Math.Max(sz, inner.Length + 1);
                                }
                            }

                            // Heuristic: If a variable is pointer-like or large (>128) prefer heap placement
                            var preferHeap = sz > 128 || (lt.Type != null && (lt.Type.EndsWith("*") || lt.Type.IndexOf("array", StringComparison.OrdinalIgnoreCase) >= 0));
                            if (preferHeap)
                            {
                                localPlacements[lt.Name ?? $"_anon{localPlacements.Count}"] = "heap";
                                usesHeap = true;
                            }
                            else
                            {
                                // place on stack, track byte size and alignment
                                localPlacements[lt.Name ?? $"_anon{localPlacements.Count}"] = $"stack:{sz}";
                                var align = 8L;
                                localsRegion = AlignTo(localsRegion, align);
                                localsRegion += AlignTo(sz, align);
                            }
                        }
                        else if (s is TypedCilAlloc || s is TypedCilMaloc)
                        {
                            usesHeap = true;
                        }
                        else if (s is TypedCilCall || s is TypedCilSpawn || s is TypedCilAsync || s is TypedCilAwait)
                        {
                            spillCount += 1;
                        }
                        else if (s is TypedCilLoop)
                        {
                            // loops may need spills
                            spillCount += 1;
                        }
                    }
                }

                long spillRegion = spillCount * 8;
                long shadowSpace = 32;
                long calleeSavedArea = 8 * 2; // conservative: push rbp + one reg
                long offset = shadowSpace + calleeSavedArea + AlignTo(localsRegion, 8) + AlignTo(spillRegion, 8);
                long frameSize = AlignTo(offset, 16);

                if (usesHeap)
                {
                    notes["frame.allocs"] = "heap";
                }

                notes["locals.bytes"] = localsRegion.ToString();
                notes["spills.count"] = spillCount.ToString();

                // embed per-variable decisions into Notes with deterministic keys
                foreach (var kv in localPlacements.OrderBy(kv => kv.Key, StringComparer.Ordinal))
                    notes[$"local.{kv.Key}"] = kv.Value;

                var pro = MakePrologueBytesPE(frameSize);
                var epi = MakeEpilogueBytesPE(frameSize);

                frames.Add(new FrameFunction(proc.Name ?? "anon", frameSize, localsRegion, spillRegion, usesHeap, notes, pro, epi));
            }

            var fm = new FrameModule(module.ModuleName, frames, GetDeterministicTimestamp());

            try
            {
                var outPath = $"{module.ModuleName}.frames.json";
                var opts = new JsonSerializerOptions { WriteIndented = true };
                File.WriteAllText(outPath, JsonSerializer.Serialize(fm, opts));
            }
            catch { /* best-effort */ }

            return fm;
        }

        private static long AlignTo(long v, long align)
        {
            if (align <= 0) return v;
            var rem = v % align;
            return rem == 0 ? v : v + (align - rem);
        }

        private static long ResolveTypeSize(string t)
        {
            if (string.IsNullOrEmpty(t)) return 8;
            t = t.Trim().ToLowerInvariant();
            if (t.StartsWith("i") && long.TryParse(t.Substring(1), out _)) return 8;
            if (t == "f64" || t == "double") return 8;
            if (t.EndsWith("*")) return 8;
            if (t == "i32" || t == "int32") return 4;
            // simple array notation like "u8[128]" or "u8[]" fallback
            var idx = t.IndexOf('[');
            if (idx > 0)
            {
                var inner = t[(idx + 1)..^1];
                if (int.TryParse(inner, out var n)) return n;
            }
            return 8;
        }

        private static DateTime GetDeterministicTimestamp()
        {
            try
            {
                var s = Environment.GetEnvironmentVariable("SOURCE_DATE_EPOCH");
                if (!string.IsNullOrEmpty(s) && long.TryParse(s, out var epoch)) return DateTimeOffset.FromUnixTimeSeconds(epoch).UtcDateTime;
            }
            catch { }
            return DateTimeOffset.FromUnixTimeSeconds(0).UtcDateTime;
        }

        // Prologue/Epilogue helpers (PE / Windows x64), with short imm8 when possible (<=127)
        public static byte[] MakePrologueBytesPE(long frameSize)
        {
            var bytes = new List<byte> { 0x55, 0x48, 0x89, 0xE5 }; // push rbp; mov rbp,rsp
            if (frameSize > 0 && frameSize <= 127)
            {
                bytes.AddRange(new byte[] { 0x48, 0x83, 0xEC, (byte)frameSize }); // sub rsp, imm8
            }
            else if (frameSize > 0)
            {
                bytes.AddRange(new byte[] { 0x48, 0x81, 0xEC });
                bytes.AddRange(BitConverter.GetBytes((uint)frameSize));
            }
            return bytes.ToArray();
        }

        public static byte[] MakeEpilogueBytesPE(long frameSize)
        {
            var bytes = new List<byte>();
            if (frameSize > 0 && frameSize <= 127)
            {
                bytes.AddRange(new byte[] { 0x48, 0x83, 0xC4, (byte)frameSize }); // add rsp, imm8
            }
            else if (frameSize > 0)
            {
                bytes.AddRange(new byte[] { 0x48, 0x81, 0xC4 });
                bytes.AddRange(BitConverter.GetBytes((uint)frameSize));
            }
            bytes.Add(0x5D); // pop rbp
            bytes.Add(0xC3); // ret
            return bytes.ToArray();
        }
    }
}

