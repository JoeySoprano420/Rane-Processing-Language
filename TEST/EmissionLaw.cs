using System;
using System.Collections.Generic;
using System.IO;

csharp RANE_Today/src/CIAM/EmissionLaw.cs
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace RANE.CIAM
{
    // Emission Law: final deterministic decisions that bridge FramePlanner/OSW/ABI -> Codegen
    // - Produces per-function emission plans (target selection, concrete prologue/epilogue bytes,
    //   codegen hints and notes) and writes a deterministic "{module}.emission.json" artifact.
    // - Conservative, audit-friendly: does not lower semantics, only records deterministic choices.
    public static class EmissionLaw
    {
        public sealed record EmissionModule(string ModuleName, IReadOnlyList<EmissionFunction> Functions, DateTime Generated);
        public sealed record EmissionFunction(
            string Name,
            string Target,                    // "PE" or "ELF"
            byte[] PrologueBytes,
            byte[] EpilogueBytes,
            IReadOnlyList<string> Hints,      // merged optimization hints (resolver + OSW)
            IReadOnlyDictionary<string,string>? Notes);

        // Plan emission for the given module and write deterministic artifacts.
        // targetPlatform: "PE" (Windows) or "ELF" (SystemV). Default "PE".
        public static EmissionModule PlanAndWrite(TypedCilModule module, string targetPlatform = "PE")
        {
            if (module == null) throw new ArgumentNullException(nameof(module));
            targetPlatform = (targetPlatform ?? "PE").Trim().ToUpperInvariant();
            var audits = new List<AuditRecord>();
            // Ensure ABI/Frame/OSW are available & deterministic
            var abi = ABITruth.Analyze(module);                 // ABITruth hooks FramePlanner via its Analyze
            var frames = FramePlanner.BuildAndWrite(module, abi);
            var osw = OptimizedStructureWeb.BuildAndWrite(module);

            var funcs = new List<EmissionFunction>();

            foreach (var proc in module.Procs ?? Array.Empty<TypedCilProc>())
            {
                var name = proc.Name ?? "anon";

                // Find frame info for proc
                var frameFn = frames.Functions.FirstOrDefault(f => string.Equals(f.Name, name, StringComparison.OrdinalIgnoreCase));
                // Select prologue/epilogue for target
                byte[] prologue = Array.Empty<byte>();
                byte[] epilogue = Array.Empty<byte>();
                if (frameFn != null)
                {
                    prologue = targetPlatform == "ELF" ? frameFn.PrologueTemplateELF : frameFn.PrologueTemplatePE;
                    epilogue = targetPlatform == "ELF" ? frameFn.EpilogueTemplateELF : frameFn.EpilogueTemplatePE;
                    // Defensive: null -> empty
                    prologue ??= Array.Empty<byte>();
                    epilogue ??= Array.Empty<byte>();
                }

                // Collect hints: prefer resolver opt.hints (proc annotations), then OSW/FramePlanner notes
                var hintList = new List<string>();
                if (proc is not null && TryGetAnnotations(proc, out var pann) && pann != null && pann.TryGetValue("opt.hints", out var ph) && !string.IsNullOrEmpty(ph))
                {
                    hintList.AddRange(ph.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries));
                }

                // OSW hints: inspect osw.Functions annotations if present (keeps deterministic ordering)
                var oswFn = osw.Functions.FirstOrDefault(f => string.Equals(f.Name, name, StringComparison.OrdinalIgnoreCase));
                if (oswFn != null && oswFn.Annotations != null && oswFn.Annotations.TryGetValue("opt.hints", out var oh) && !string.IsNullOrEmpty(oh))
                {
                    hintList.AddRange(oh.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries));
                }

                // FramePlanner notes
                var notes = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                if (frameFn != null && frameFn.Notes != null)
                {
                    foreach (var kv in frameFn.Notes) notes[kv.Key] = kv.Value;
                }

                // Merge unique deterministic hints (preserve first-seen order)
                var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                var mergedHints = new List<string>();
                foreach (var h in hintList)
                {
                    var key = h.Trim();
                    if (key.Length == 0) continue;
                    if (!seen.Contains(key)) { seen.Add(key); mergedHints.Add(key); }
                }

                // Audit per-function emission decision
                audits.Add(AuditHelpers.MakeAudit(
                    "CIAM.EmissionLaw.FunctionPlanned",
                    $"proc:{name}",
                    0, 0,
                    name,
                    $"Emission planned for '{name}' target={targetPlatform} hints=[{string.Join(", ", mergedHints)}]"));

                funcs.Add(new EmissionFunction(name, targetPlatform, prologue, epilogue, mergedHints, notes.Count > 0 ? notes : null));
            }

            var em = new EmissionModule(module.ModuleName, funcs, GetDeterministicTimestamp());

            // Write deterministic JSON artifact for CI inspection (best-effort)
            try
            {
                var outPath = $"{module.ModuleName}.emission.json";
                var opts = new JsonSerializerOptions { WriteIndented = true };
                File.WriteAllText(outPath, JsonSerializer.Serialize(em, opts));
            }
            catch { /* best-effort */ }

            // Write audits
            try
            {
                var auditPath = $"{module.ModuleName}.emission.audits.json";
                var doc = new
                {
                    module = module.ModuleName,
                    generated = GetDeterministicTimestamp().ToString("o"),
                    audits = audits.Select(a => new { rule = a.RuleId, matchedText = a.MatchedText, timestamp = a.Timestamp.ToString("o"), summary = a.Summary }).ToArray()
                };
                var opts = new JsonSerializerOptions { WriteIndented = true };
                File.WriteAllText(auditPath, JsonSerializer.Serialize(doc, opts));
            }
            catch { }

            return em;
        }

        // Helper to extract annotations from TypedCilProc if present (materializer preserved them)
        private static bool TryGetAnnotations(TypedCilProc p, out IReadOnlyDictionary<string,string>? ann)
        {
            ann = null;
            try
            {
                var prop = p.GetType().GetProperty("Annotations");
                if (prop != null)
                {
                    var val = prop.GetValue(p) as IReadOnlyDictionary<string,string>;
                    ann = val;
                    return ann != null;
                }
            }
            catch { }
            return false;
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

csharp RANE_Today/src/CIAM/EmissionLaw.cs
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace RANE.CIAM
{
    public static class EmissionLaw
    {
        public sealed record EmissionModule(string ModuleName, IReadOnlyList<EmissionFunction> Functions, DateTime Generated);
        public sealed record EmissionFunction(
            string Name,
            string Target,
            byte[] PrologueBytes,
            byte[] EpilogueBytes,
            IReadOnlyList<string> Hints,
            IReadOnlyDictionary<string, string>? Notes);

        // Plan emission (deterministic) and optionally invoke native emitter for PE/ELF.
        public static EmissionModule PlanAndWrite(
            TypedCilModule module,
            string targetPlatform = "PE",
            bool emitNative = false,
            string? nativeOutputPath = null,
            string? workDir = null,
            string clangPath = "clang",
            bool nativeEnableLto = true,
            bool nativeEnablePgo = false,
            string? nativeProfileRunCmd = null,
            string[]? nativeExtraFlags = null)
        {
            if (module == null) throw new ArgumentNullException(nameof(module));
            targetPlatform = (targetPlatform ?? "PE").Trim().ToUpperInvariant();
            var audits = new List<AuditRecord>();

            var abi = ABITruth.Analyze(module);
            var frames = FramePlanner.BuildAndWrite(module, abi);
            var osw = OptimizedStructureWeb.BuildAndWrite(module);

            var funcs = new List<EmissionFunction>();
            foreach (var proc in module.Procs ?? Array.Empty<TypedCilProc>())
            {
                var name = proc.Name ?? "anon";
                var frameFn = frames.Functions.FirstOrDefault(f => string.Equals(f.Name, name, StringComparison.OrdinalIgnoreCase));
                byte[] prologue = Array.Empty<byte>();
                byte[] epilogue = Array.Empty<byte>();
                if (frameFn != null)
                {
                    prologue = targetPlatform == "ELF" ? frameFn.PrologueTemplateELF : frameFn.PrologueTemplatePE;
                    epilogue = targetPlatform == "ELF" ? frameFn.EpilogueTemplateELF : frameFn.EpilogueTemplatePE;
                    prologue ??= Array.Empty<byte>();
                    epilogue ??= Array.Empty<byte>();
                }

                var hintList = new List<string>();
                if (proc is not null && TryGetAnnotations(proc, out var pann) && pann != null && pann.TryGetValue("opt.hints", out var ph) && !string.IsNullOrEmpty(ph))
                    hintList.AddRange(ph.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries));
                var oswFn = osw.Functions.FirstOrDefault(f => string.Equals(f.Name, name, StringComparison.OrdinalIgnoreCase));
                if (oswFn != null && oswFn.Annotations != null && oswFn.Annotations.TryGetValue("opt.hints", out var oh) && !string.IsNullOrEmpty(oh))
                    hintList.AddRange(oh.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries));

                var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                var mergedHints = new List<string>();
                foreach (var h in hintList)
                {
                    var key = h.Trim();
                    if (key.Length == 0) continue;
                    if (!seen.Contains(key)) { seen.Add(key); mergedHints.Add(key); }
                }

                audits.Add(AuditHelpers.MakeAudit(
                    "CIAM.EmissionLaw.FunctionPlanned",
                    $"proc:{name}",
                    0, 0,
                    name,
                    $"Emission planned for '{name}' target={targetPlatform} hints=[{string.Join(", ", mergedHints)}]"));

                funcs.Add(new EmissionFunction(name, targetPlatform, prologue, epilogue, mergedHints, null));
            }

            var em = new EmissionModule(module.ModuleName, funcs, GetDeterministicTimestamp());
            try
            {
                var outPath = $"{module.ModuleName}.emission.json";
                var opts = new JsonSerializerOptions { WriteIndented = true };
                File.WriteAllText(outPath, JsonSerializer.Serialize(em, opts));
            }
            catch { }

            try
            {
                var auditPath = $"{module.ModuleName}.emission.audits.json";
                var doc = new
                {
                    module = module.ModuleName,
                    generated = GetDeterministicTimestamp().ToString("o"),
                    audits = audits.Select(a => new { rule = a.RuleId, matchedText = a.MatchedText, timestamp = a.Timestamp.ToString("o"), summary = a.Summary }).ToArray()
                };
                var opts = new JsonSerializerOptions { WriteIndented = true };
                File.WriteAllText(auditPath, JsonSerializer.Serialize(doc, opts));
            }
            catch { }

            // Optionally invoke native emitter with provided options (exposed for orchestration & tests).
            if (emitNative)
            {
                try
                {
                    if (string.IsNullOrEmpty(nativeOutputPath)) throw new ArgumentException("nativeOutputPath required when emitNative=true");
                    var wd = string.IsNullOrEmpty(workDir) ? Path.GetTempPath() : workDir;
                    Backend.NativeEmitter.EmitNativeExe(
                        module,
                        typedCilPbPath: $"{module.ModuleName}.typedcil.json",
                        outputExePath: nativeOutputPath,
                        workDir: wd,
                        clangPath: clangPath,
                        enableLto: nativeEnableLto,
                        enablePgo: nativeEnablePgo,
                        profileRunCommand: nativeProfileRunCmd,
                        extraCompilerFlags: nativeExtraFlags);
                }
                catch (Exception ex)
                {
                    // do not fail the planning phase if native emission fails - record audit and continue.
                    audits.Add(AuditHelpers.MakeAudit("CIAM.EmissionLaw.NativeEmitFailed", $"module:{module.ModuleName}", 0, 0, module.ModuleName, ex.Message));
                }
            }

            return em;
        }

        private static bool TryGetAnnotations(TypedCilProc p, out IReadOnlyDictionary<string, string>? ann)
        {
            ann = null;
            try
            {
                var prop = p.GetType().GetProperty("Annotations");
                if (prop != null)
                {
                    var val = prop.GetValue(p) as IReadOnlyDictionary<string, string>;
                    ann = val;
                    return ann != null;
                }
            }
            catch { }
            return false;
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

