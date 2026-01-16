using System;
using System.Collections.Generic;

csharp RANE_Today/src/CIAM/TypedCilResolver.cs
using System;
using System.Collections.Generic;
using System.Linq;

namespace RANE.CIAM
{
    // TypedCilResolver: lightweight type-checking and heuristic annotation pass over TypedCilModule.
    // - Performs conservative type inference for Let/Return expressions.
    // - Emits diagnostics in proc Annotations when obvious mismatches occur.
    // - Adds opt.hints (e.g., vectorize) and simple frame hints (frame.allocs.heap) when patterns are detected.
    //
    // This is intentionally conservative and auditable; it does not change semantics.
    public static class TypedCilResolver
    {
        public static TypedCilModule AnalyzeAndAnnotate(TypedCilModule module)
        {
            if (module == null) throw new ArgumentNullException(nameof(module));

            var newProcs = new List<TypedCilProc>();
            foreach (var proc in module.Procs ?? Array.Empty<TypedCilProc>())
            {
                var ann = proc.Annotations != null
                    ? new Dictionary<string, string>(proc.Annotations, StringComparer.OrdinalIgnoreCase)
                    : new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

                // Build parameter type map from proc.Params string if present: "(a:i64,b:i64)"
                var paramTypes = ParseParamsSignature(proc.Params);

                // locals type map
                var locals = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

                // diagnostics collection
                var diags = new List<string>();

                // heuristic flags
                bool hasLoop = false;
                bool hasAlloc = false;
                bool vectorizeCandidate = false;

                // scan body in order to infer types & collect flags
                if (proc.Body != null)
                {
                    foreach (var stmt in proc.Body)
                    {
                        switch (stmt)
                        {
                            case TypedCilLet lt:
                                {
                                    var inferred = InferExprType(lt.Expr, paramTypes, locals);
                                    if (!string.IsNullOrEmpty(inferred))
                                    {
                                        locals[lt.Name] = inferred;
                                    }

                                    // check declared type if available
                                    if (!string.IsNullOrEmpty(lt.Type) && !string.IsNullOrEmpty(inferred))
                                    {
                                        if (!TypeCompatible(lt.Type, inferred))
                                        {
                                            diags.Add($"Let '{lt.Name}' declared {lt.Type} but inferred {inferred}");
                                        }
                                    }

                                    // detect vector pattern by naming convention (_v4d or _v4d_out)
                                    if ((lt.Name ?? "").EndsWith("_v4d_out", StringComparison.OrdinalIgnoreCase) &&
                                        lt.Expr is TypedCilBinary bin &&
                                        bin.Left is TypedCilIdentifier lid && bin.Right is TypedCilIdentifier rid &&
                                        (lid.Name ?? "").EndsWith("_v4d", StringComparison.OrdinalIgnoreCase) &&
                                        (rid.Name ?? "").EndsWith("_v4d", StringComparison.OrdinalIgnoreCase))
                                    {
                                        vectorizeCandidate = true;
                                    }
                                    break;
                                }
                            case TypedCilLoop loop:
                                {
                                    hasLoop = true;
                                    // examine loop body for vectorizable let
                                    if (loop.Body != null)
                                    {
                                        foreach (var lstmt in loop.Body.OfType<TypedCilLet>())
                                        {
                                            if ((lstmt.Name ?? "").EndsWith("_v4d_out", StringComparison.OrdinalIgnoreCase) &&
                                                lstmt.Expr is TypedCilBinary b2 &&
                                                b2.Left is TypedCilIdentifier lid2 && b2.Right is TypedCilIdentifier rid2 &&
                                                (lid2.Name ?? "").EndsWith("_v4d", StringComparison.OrdinalIgnoreCase) &&
                                                (rid2.Name ?? "").EndsWith("_v4d", StringComparison.OrdinalIgnoreCase))
                                            {
                                                vectorizeCandidate = true;
                                            }
                                        }
                                    }
                                    break;
                                }
                            case TypedCilAlloc _:
                                hasAlloc = true;
                                break;
                            case TypedCilMaloc _:
                                hasAlloc = true;
                                break;
                            case TypedCilSpawn _:
                            case TypedCilAsync _:
                                // spawn/async mark function as having concurrency capability
                                ann["resolved.requires"] = CombineRequirements(ann.TryGetValue("resolved.requires", out var r) ? r : null, "threads");
                                break;
                            case TypedCilChecksum _:
                            case TypedCilCipher _:
                                // crypto capability
                                ann["resolved.requires"] = CombineRequirements(ann.TryGetValue("resolved.requires", out var r2) ? r2 : null, "crypto");
                                break;
                            default:
                                break;
                        }
                    }
                }

                // attach annotations
                if (diags.Count > 0)
                    ann["resolved.diagnostics"] = string.Join(" || ", diags);

                if (hasAlloc)
                    ann["frame.allocs.heap"] = "true";

                // compact opt.hints annotation: prefer existing resolver hints, otherwise add vectorize candidate
                var existingHints = ann.TryGetValue("opt.hints", out var eh) ? eh : null;
                var hints = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                if (!string.IsNullOrEmpty(existingHints))
                {
                    foreach (var part in existingHints.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
                        hints.Add(part);
                }
                if (vectorizeCandidate || hasLoop)
                    hints.Add("vectorize");
                if (proc.Body != null && proc.Body.Count >= 12)
                    hints.Add("pgo_candidate");

                if (hints.Count > 0)
                    ann["opt.hints"] = string.Join(", ", hints.OrderBy(x => x, StringComparer.OrdinalIgnoreCase));

                // return new proc with augmented annotations
                newProcs.Add(new TypedCilProc(proc.Name, proc.Visibility, proc.RetType, proc.Params, proc.Requires, proc.Body, ann));
            }

            // produce new module with same other collections
            return new TypedCilModule(
                module.ModuleName,
                module.Imports,
                module.Types,
                module.Structs,
                module.Enums,
                module.Variants,
                module.Mmios,
                module.Capabilities,
                newProcs,
                module.Nodes);
        }

        // Conservative expression type inference
        private static string InferExprType(TypedCilExpr expr, Dictionary<string, string> paramsMap, Dictionary<string, string> locals)
        {
            if (expr == null) return "unknown";
            switch (expr)
            {
                case TypedCilLiteral l:
                    return InferLiteralType(l.Value);
                case TypedCilIdentifier id:
                    {
                        if (locals.TryGetValue(id.Name ?? "", out var lt)) return lt;
                        if (paramsMap.TryGetValue(id.Name ?? "", out var pt)) return pt;
                        return "unknown";
                    }
                case TypedCilUnary u:
                    return InferExprType(u.Operand, paramsMap, locals);
                case TypedCilBinary b:
                    {
                        var lt = InferExprType(b.Left, paramsMap, locals);
                        var rt = InferExprType(b.Right, paramsMap, locals);
                        // numeric promotion heuristic
                        if (lt == "f64" || rt == "f64") return "f64";
                        if (lt.StartsWith("i") || rt.StartsWith("i")) return "i64";
                        return lt != "unknown" ? lt : rt;
                    }
                case TypedCilCallExpr ce:
                    // unknown without symbol table; assume i64 for arithmetic calls, string for print-like
                    if (!string.IsNullOrEmpty(ce.Callee) && ce.Callee.IndexOf("print", StringComparison.OrdinalIgnoreCase) >= 0) return "int";
                    return "i64";
                case TypedCilVariantConstruct vc:
                    return vc.Variant;
                case TypedCilCast c:
                    return c.TargetType;
                case TypedCilTupleExpr te:
                    return "tuple";
                case TypedCilIndex ix:
                    return "i64";
                case TypedCilChecksum _:
                    return "u64";
                case TypedCilCipher _:
                    return "string";
                default:
                    return "unknown";
            }
        }

        private static string InferLiteralType(string lit)
        {
            if (string.IsNullOrEmpty(lit)) return "int";
            var t = lit.Trim();
            if ((t.StartsWith("\"") && t.EndsWith("\"")) || (t.StartsWith("'") && t.EndsWith("'"))) return "string";
            if (t.Contains(".")) return "f64";
            if (long.TryParse(t, out _)) return "i64";
            return "int";
        }

        private static bool TypeCompatible(string declared, string inferred)
        {
            if (string.IsNullOrEmpty(declared) || string.IsNullOrEmpty(inferred)) return true;
            var d = declared.Trim().ToLowerInvariant();
            var i = inferred.Trim().ToLowerInvariant();
            if (d == i) return true;
            if (d.StartsWith("i") && i.StartsWith("i")) return true;
            if (d == "string" && i == "string") return true;
            if ((d == "i64" || d == "int64") && i.StartsWith("i")) return true;
            if ((d == "f64" || d == "double") && i == "f64") return true;
            // allow pointer/void* compatibility
            if (d == "void*" || d.EndsWith("*")) return true;
            return false;
        }

        private static Dictionary<string, string> ParseParamsSignature(string ps)
        {
            var map = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            if (string.IsNullOrEmpty(ps)) return map;
            var s = ps.Trim();
            if (s.StartsWith("(") && s.EndsWith(")")) s = s[1..^1];
            if (string.IsNullOrWhiteSpace(s)) return map;
            foreach (var part in s.Split(',', StringSplitOptions.RemoveEmptyEntries))
            {
                var kv = part.Split(':', 2);
                if (kv.Length == 2)
                    map[kv[0].Trim()] = kv[1].Trim();
            }
            return map;
        }

        private static string CombineRequirements(string? existing, string add)
        {
            if (string.IsNullOrEmpty(existing)) return add;
            var parts = existing.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries).ToList();
            if (!parts.Contains(add, StringComparer.OrdinalIgnoreCase)) parts.Add(add);
            return string.Join(", ", parts);
        }
    }
}

csharp RANE_Today/src/CIAM/TypedCilResolver.cs
using System;
using System.Collections.Generic;
using System.Linq;

namespace RANE.CIAM
{
    // TypedCilResolver: lightweight type-checking and heuristic annotation pass over TypedCilModule.
    // - Performs conservative type inference for Let/Return expressions.
    // - Emits diagnostics in proc Annotations when obvious mismatches occur.
    // - Adds opt.hints (e.g., vectorize) and simple frame hints (frame.allocs.heap) when patterns are detected.
    //
    // This is intentionally conservative and auditable; it does not change semantics.
    public static class TypedCilResolver
    {
        public static TypedCilModule AnalyzeAndAnnotate(TypedCilModule module)
        {
            if (module == null) throw new ArgumentNullException(nameof(module));

            var newProcs = new List<TypedCilProc>();
            foreach (var proc in module.Procs ?? Array.Empty<TypedCilProc>())
            {
                var ann = proc.Annotations != null
                    ? new Dictionary<string, string>(proc.Annotations, StringComparer.OrdinalIgnoreCase)
                    : new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

                // Build parameter type map from proc.Params string if present: "(a:i64,b:i64)"
                var paramTypes = ParseParamsSignature(proc.Params);

                // locals type map
                var locals = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

                // diagnostics collection
                var diags = new List<string>();

                // heuristic flags
                bool hasLoop = false;
                bool hasAlloc = false;
                bool vectorizeCandidate = false;

                // per-let explicit size/placement (resolver will emit these so FramePlanner/Emitter can use exact sizes)
                var perLocalPlacement = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

                // scan body in order to infer types & collect flags
                if (proc.Body != null)
                {
                    foreach (var stmt in proc.Body)
                    {
                        switch (stmt)
                        {
                            case TypedCilLet lt:
                                {
                                    var inferred = InferExprType(lt.Expr, paramTypes, locals);
                                    if (!string.IsNullOrEmpty(inferred))
                                    {
                                        locals[lt.Name] = inferred;
                                    }

                                    // check declared type if available
                                    if (!string.IsNullOrEmpty(lt.Type) && !string.IsNullOrEmpty(inferred))
                                    {
                                        if (!TypeCompatible(lt.Type, inferred))
                                        {
                                            diags.Add($"Let '{lt.Name}' declared {lt.Type} but inferred {inferred}");
                                        }
                                    }

                                    // detect vector pattern by naming convention (_v4d or _v4d_out)
                                    if ((lt.Name ?? "").EndsWith("_v4d_out", StringComparison.OrdinalIgnoreCase) &&
                                        lt.Expr is TypedCilBinary bin &&
                                        bin.Left is TypedCilIdentifier lid && bin.Right is TypedCilIdentifier rid &&
                                        (lid.Name ?? "").EndsWith("_v4d", StringComparison.OrdinalIgnoreCase) &&
                                        (rid.Name ?? "").EndsWith("_v4d", StringComparison.OrdinalIgnoreCase))
                                    {
                                        vectorizeCandidate = true;
                                    }

                                    // resolver computes exact per-let size when possible (string literal or explicit array type)
                                    var letSize = ResolveLetSize(lt);
                                    var preferHeap = lt.Type != null && (lt.Type.EndsWith("*") || lt.Type.IndexOf("array", StringComparison.OrdinalIgnoreCase) >= 0);
                                    if (letSize > 128 || preferHeap)
                                    {
                                        perLocalPlacement[lt.Name] = "heap";
                                        hasAlloc = true;
                                    }
                                    else if (letSize > 0)
                                    {
                                        perLocalPlacement[lt.Name] = $"stack:{letSize}";
                                    }

                                    break;
                                }
                            case TypedCilLoop loop:
                                {
                                    hasLoop = true;
                                    // examine loop body for vectorizable let
                                    if (loop.Body != null)
                                    {
                                        foreach (var lstmt in loop.Body.OfType<TypedCilLet>())
                                        {
                                            if ((lstmt.Name ?? "").EndsWith("_v4d_out", StringComparison.OrdinalIgnoreCase) &&
                                                lstmt.Expr is TypedCilBinary b2 &&
                                                b2.Left is TypedCilIdentifier lid2 && b2.Right is TypedCilIdentifier rid2 &&
                                                (lid2.Name ?? "").EndsWith("_v4d", StringComparison.OrdinalIgnoreCase) &&
                                                (rid2.Name ?? "").EndsWith("_v4d", StringComparison.OrdinalIgnoreCase))
                                            {
                                                vectorizeCandidate = true;
                                            }
                                        }
                                    }
                                    break;
                                }
                            case TypedCilAlloc _:
                                hasAlloc = true;
                                break;
                            case TypedCilMaloc _:
                                hasAlloc = true;
                                break;
                            case TypedCilSpawn _:
                            case TypedCilAsync _:
                                // spawn/async mark function as having concurrency capability
                                ann["resolved.requires"] = CombineRequirements(ann.TryGetValue("resolved.requires", out var r) ? r : null, "threads");
                                break;
                            case TypedCilChecksum _:
                            case TypedCilCipher _:
                                // crypto capability
                                ann["resolved.requires"] = CombineRequirements(ann.TryGetValue("resolved.requires", out var r2) ? r2 : null, "crypto");
                                break;
                            default:
                                break;
                        }
                    }
                }

                // attach per-local placement annotations into proc annotations so FramePlanner and Emitter can trust exact sizes
                foreach (var kv in perLocalPlacement.OrderBy(kv => kv.Key, StringComparer.Ordinal))
                {
                    var key = $"local.{kv.Key}";
                    if (!ann.ContainsKey(key)) ann[key] = kv.Value;
                }

                // attach diagnostics
                if (diags.Count > 0)
                    ann["resolved.diagnostics"] = string.Join(" || ", diags);

                if (hasAlloc)
                    ann["frame.allocs.heap"] = "true";

                // compact opt.hints annotation: prefer existing resolver hints, otherwise add vectorize candidate
                var existingHints = ann.TryGetValue("opt.hints", out var eh) ? eh : null;
                var hints = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                if (!string.IsNullOrEmpty(existingHints))
                {
                    foreach (var part in existingHints.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
                        hints.Add(part);
                }
                if (vectorizeCandidate || hasLoop)
                    hints.Add("vectorize");
                if (proc.Body != null && proc.Body.Count >= 12)
                    hints.Add("pgo_candidate");

                if (hints.Count > 0)
                    ann["opt.hints"] = string.Join(", ", hints.OrderBy(x => x, StringComparer.OrdinalIgnoreCase));

                // return new proc with augmented annotations
                newProcs.Add(new TypedCilProc(proc.Name, proc.Visibility, proc.RetType, proc.Params, proc.Requires, proc.Body, ann));
            }

            // produce new module with same other collections
            return new TypedCilModule(
                module.ModuleName,
                module.Imports,
                module.Types,
                module.Structs,
                module.Enums,
                module.Variants,
                module.Mmios,
                module.Capabilities,
                newProcs,
                module.Nodes);
        }

        // Conservative per-let size resolver (when possible)
        private static int ResolveLetSize(TypedCilLet lt)
        {
            if (lt == null) return 0;
            // explicit array sizing like "u8[256]"
            if (!string.IsNullOrEmpty(lt.Type))
            {
                var t = lt.Type.Trim();
                var idx = t.IndexOf('[');
                if (idx > 0 && t.EndsWith("]"))
                {
                    var inner = t[(idx + 1)..^1];
                    if (int.TryParse(inner, out var n)) return n;
                }
            }

            if (lt.Expr is TypedCilLiteral lit && lit.Value != null)
            {
                var s = lit.Value.Trim();
                if ((s.StartsWith("\"") && s.EndsWith("\"")) || (s.StartsWith("'") && s.EndsWith("'")))
                {
                    var inner = s.Substring(1, s.Length - 2);
                    return inner.Length + 1;
                }
                if (int.TryParse(s, out var v))
                    return sizeof(long); // treat numeric init as scalar
            }

            return 0;
        }

        // Conservative expression type inference
        private static string InferExprType(TypedCilExpr expr, Dictionary<string, string> paramsMap, Dictionary<string, string> locals)
        {
            if (expr == null) return "unknown";
            switch (expr)
            {
                case TypedCilLiteral l:
                    return InferLiteralType(l.Value);
                case TypedCilIdentifier id:
                    {
                        if (locals.TryGetValue(id.Name ?? "", out var lt)) return lt;
                        if (paramsMap.TryGetValue(id.Name ?? "", out var pt)) return pt;
                        return "unknown";
                    }
                case TypedCilUnary u:
                    return InferExprType(u.Operand, paramsMap, locals);
                case TypedCilBinary b:
                    {
                        var lt = InferExprType(b.Left, paramsMap, locals);
                        var rt = InferExprType(b.Right, paramsMap, locals);
                        // numeric promotion heuristic
                        if (lt == "f64" || rt == "f64") return "f64";
                        if (lt.StartsWith("i") || rt.StartsWith("i")) return "i64";
                        return lt != "unknown" ? lt : rt;
                    }
                case TypedCilCallExpr ce:
                    // unknown without symbol table; assume i64 for arithmetic calls, string for print-like
                    if (!string.IsNullOrEmpty(ce.Callee) && ce.Callee.IndexOf("print", StringComparison.OrdinalIgnoreCase) >= 0) return "int";
                    return "i64";
                case TypedCilVariantConstruct vc:
                    return vc.Variant;
                case TypedCilCast c:
                    return c.TargetType;
                case TypedCilTupleExpr te:
                    return "tuple";
                case TypedCilIndex ix:
                    return "i64";
                case TypedCilChecksum _:
                    return "u64";
                case TypedCilCipher _:
                    return "string";
                default:
                    return "unknown";
            }
        }

        private static string InferLiteralType(string lit)
        {
            if (string.IsNullOrEmpty(lit)) return "int";
            var t = lit.Trim();
            if ((t.StartsWith("\"") && t.EndsWith("\"")) || (t.StartsWith("'") && t.EndsWith("'"))) return "string";
            if (t.Contains(".")) return "f64";
            if (long.TryParse(t, out _)) return "i64";
            return "int";
        }

        private static bool TypeCompatible(string declared, string inferred)
        {
            if (string.IsNullOrEmpty(declared) || string.IsNullOrEmpty(inferred)) return true;
            var d = declared.Trim().ToLowerInvariant();
            var i = inferred.Trim().ToLowerInvariant();
            if (d == i) return true;
            if (d.StartsWith("i") && i.StartsWith("i")) return true;
            if (d == "string" && i == "string") return true;
            if ((d == "i64" || d == "int64") && i.StartsWith("i")) return true;
            if ((d == "f64" || d == "double") && i == "f64") return true;
            // allow pointer/void* compatibility
            if (d == "void*" || d.EndsWith("*")) return true;
            return false;
        }

        private static Dictionary<string, string> ParseParamsSignature(string ps)
        {
            var map = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            if (string.IsNullOrEmpty(ps)) return map;
            var s = ps.Trim();
            if (s.StartsWith("(") && s.EndsWith(")")) s = s[1..^1];
            if (string.IsNullOrWhiteSpace(s)) return map;
            foreach (var part in s.Split(',', StringSplitOptions.RemoveEmptyEntries))
            {
                var kv = part.Split(':', 2);
                if (kv.Length == 2)
                    map[kv[0].Trim()] = kv[1].Trim();
            }
            return map;
        }

        private static string CombineRequirements(string? existing, string add)
        {
            if (string.IsNullOrEmpty(existing)) return add;
            var parts = existing.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries).ToList();
            if (!parts.Contains(add, StringComparer.OrdinalIgnoreCase)) parts.Add(add);
            return string.Join(", ", parts);
        }
    }
}

csharp RANE_Today/src/CIAM/TypedCilResolver.cs
using System;
using System.Collections.Generic;
using System.Linq;

namespace RANE.CIAM
{
    // TypedCilResolver: lightweight type-checking and heuristic annotation pass over TypedCilModule.
    // - Performs conservative type inference for Let/Return expressions.
    // - Emits diagnostics in proc Annotations when obvious mismatches occur.
    // - Adds opt.hints (e.g., vectorize) and simple frame hints (frame.allocs.heap) when patterns are detected.
    //
    // This is intentionally conservative and auditable; it does not change semantics.
    public static class TypedCilResolver
    {
        public static TypedCilModule AnalyzeAndAnnotate(TypedCilModule module)
        {
            if (module == null) throw new ArgumentNullException(nameof(module));

            var newProcs = new List<TypedCilProc>();
            foreach (var proc in module.Procs ?? Array.Empty<TypedCilProc>())
            {
                var ann = proc.Annotations != null
                    ? new Dictionary<string, string>(proc.Annotations, StringComparer.OrdinalIgnoreCase)
                    : new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

                // Build parameter type map from proc.Params string if present: "(a:i64,b:i64)"
                var paramTypes = ParseParamsSignature(proc.Params);

                // locals type map
                var locals = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

                // diagnostics collection
                var diags = new List<string>();

                // heuristic flags
                bool hasLoop = false;
                bool hasAlloc = false;
                bool vectorizeCandidate = false;

                // per-let explicit size/placement (resolver will emit these so FramePlanner/Emitter can use exact sizes)
                var perLocalPlacement = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

                // scan body in order to infer types & collect flags
                if (proc.Body != null)
                {
                    foreach (var stmt in proc.Body)
                    {
                        switch (stmt)
                        {
                            case TypedCilLet lt:
                                {
                                    var inferred = InferExprType(lt.Expr, paramTypes, locals);
                                    if (!string.IsNullOrEmpty(inferred))
                                    {
                                        locals[lt.Name] = inferred;
                                    }

                                    // check declared type if available
                                    if (!string.IsNullOrEmpty(lt.Type) && !string.IsNullOrEmpty(inferred))
                                    {
                                        if (!TypeCompatible(lt.Type, inferred))
                                        {
                                            diags.Add($"Let '{lt.Name}' declared {lt.Type} but inferred {inferred}");
                                        }
                                    }

                                    // detect vector pattern by naming convention (_v4d or _v4d_out)
                                    if ((lt.Name ?? "").EndsWith("_v4d_out", StringComparison.OrdinalIgnoreCase) &&
                                        lt.Expr is TypedCilBinary bin &&
                                        bin.Left is TypedCilIdentifier lid && bin.Right is TypedCilIdentifier rid &&
                                        (lid.Name ?? "").EndsWith("_v4d", StringComparison.OrdinalIgnoreCase) &&
                                        (rid.Name ?? "").EndsWith("_v4d", StringComparison.OrdinalIgnoreCase))
                                    {
                                        vectorizeCandidate = true;
                                    }

                                    // resolver computes exact per-let size when possible (string literal or explicit array type or struct)
                                    var letSize = ResolveLetSize(lt, module);
                                    var preferHeap = lt.Type != null && (lt.Type.EndsWith("*") || lt.Type.IndexOf("array", StringComparison.OrdinalIgnoreCase) >= 0);
                                    if (letSize > 128 || preferHeap)
                                    {
                                        perLocalPlacement[lt.Name] = "heap";
                                        hasAlloc = true;
                                    }
                                    else if (letSize > 0)
                                    {
                                        perLocalPlacement[lt.Name] = $"stack:{letSize}";
                                    }

                                    break;
                                }
                            case TypedCilLoop loop:
                                {
                                    hasLoop = true;
                                    // examine loop body for vectorizable let
                                    if (loop.Body != null)
                                    {
                                        foreach (var lstmt in loop.Body.OfType<TypedCilLet>())
                                        {
                                            if ((lstmt.Name ?? "").EndsWith("_v4d_out", StringComparison.OrdinalIgnoreCase) &&
                                                lstmt.Expr is TypedCilBinary b2 &&
                                                b2.Left is TypedCilIdentifier lid2 && b2.Right is TypedCilIdentifier rid2 &&
                                                (lid2.Name ?? "").EndsWith("_v4d", StringComparison.OrdinalIgnoreCase) &&
                                                (rid2.Name ?? "").EndsWith("_v4d", StringComparison.OrdinalIgnoreCase))
                                            {
                                                vectorizeCandidate = true;
                                            }
                                        }
                                    }
                                    break;
                                }
                            case TypedCilAlloc _:
                                hasAlloc = true;
                                break;
                            case TypedCilMaloc _:
                                hasAlloc = true;
                                break;
                            case TypedCilSpawn _:
                            case TypedCilAsync _:
                                // spawn/async mark function as having concurrency capability
                                ann["resolved.requires"] = CombineRequirements(ann.TryGetValue("resolved.requires", out var r) ? r : null, "threads");
                                break;
                            case TypedCilChecksum _:
                            case TypedCilCipher _:
                                // crypto capability
                                ann["resolved.requires"] = CombineRequirements(ann.TryGetValue("resolved.requires", out var r2) ? r2 : null, "crypto");
                                break;
                            default:
                                break;
                        }
                    }
                }

                // attach per-local placement annotations into proc annotations so FramePlanner and Emitter can trust exact sizes
                foreach (var kv in perLocalPlacement.OrderBy(kv => kv.Key, StringComparer.Ordinal))
                {
                    var key = $"local.{kv.Key}";
                    if (!ann.ContainsKey(key)) ann[key] = kv.Value;
                }

                // attach diagnostics
                if (diags.Count > 0)
                    ann["resolved.diagnostics"] = string.Join(" || ", diags);

                if (hasAlloc)
                    ann["frame.allocs.heap"] = "true";

                // compact opt.hints annotation: prefer existing resolver hints, otherwise add vectorize candidate
                var existingHints = ann.TryGetValue("opt.hints", out var eh) ? eh : null;
                var hints = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                if (!string.IsNullOrEmpty(existingHints))
                {
                    foreach (var part in existingHints.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
                        hints.Add(part);
                }
                if (vectorizeCandidate || hasLoop)
                    hints.Add("vectorize");
                if (proc.Body != null && proc.Body.Count >= 12)
                    hints.Add("pgo_candidate");

                if (hints.Count > 0)
                    ann["opt.hints"] = string.Join(", ", hints.OrderBy(x => x, StringComparer.OrdinalIgnoreCase));

                // return new proc with augmented annotations
                newProcs.Add(new TypedCilProc(proc.Name, proc.Visibility, proc.RetType, proc.Params, proc.Requires, proc.Body, ann));
            }

            // produce new module with same other collections
            return new TypedCilModule(
                module.ModuleName,
                module.Imports,
                module.Types,
                module.Structs,
                module.Enums,
                module.Variants,
                module.Mmios,
                module.Capabilities,
                newProcs,
                module.Nodes);
        }

        // Conservative per-let size resolver (when possible). Now consults module struct definitions and array syntax.
        private static int ResolveLetSize(TypedCilLet lt, TypedCilModule module)
        {
            if (lt == null) return 0;

            // explicit array sizing like "u8[256]"
            if (!string.IsNullOrEmpty(lt.Type))
            {
                var t = lt.Type.Trim();
                // explicit bracket form
                var idx = t.IndexOf('[');
                if (idx > 0 && t.EndsWith("]"))
                {
                    var inner = t[(idx + 1)..^1];
                    if (int.TryParse(inner, out var n)) return n;
                }

                // pointer -> pointer-sized
                if (t.EndsWith("*")) return 8;

                // if type matches a struct name in module, compute approximate struct size
                if (module != null && module.Structs != null)
                {
                    var sdef = module.Structs.FirstOrDefault(s => string.Equals(s.Name, t, StringComparison.OrdinalIgnoreCase));
                    if (sdef != null)
                    {
                        long accum = 0;
                        foreach (var f in sdef.Fields)
                        {
                            accum += ResolveTypeSizeSimple(f.Type);
                        }
                        // clamp to int safely
                        return accum > int.MaxValue ? int.MaxValue : (int)accum;
                    }
                }
            }

            if (lt.Expr is TypedCilLiteral lit && lit.Value != null)
            {
                var s = lit.Value.Trim();
                if ((s.StartsWith("\"") && s.EndsWith("\"")) || (s.StartsWith("'") && s.EndsWith("'")))
                {
                    var inner = s.Substring(1, s.Length - 2);
                    return inner.Length + 1;
                }
                if (int.TryParse(s, out var v))
                    return sizeof(long); // treat numeric init as scalar
            }

            return 0;
        }

        // small helper used by ResolveLetSize to size primitive names
        private static int ResolveTypeSizeSimple(string t)
        {
            if (string.IsNullOrEmpty(t)) return 8;
            t = t.Trim().ToLowerInvariant();
            if (t.StartsWith("i") && int.TryParse(t.Substring(1), out _)) return 8;
            if (t == "f64" || t == "double") return 8;
            if (t.EndsWith("*")) return 8;
            if (t == "i32" || t == "int32") return 4;
            // array form in field type
            var idx = t.IndexOf('[');
            if (idx > 0 && t.EndsWith("]"))
            {
                var inner = t[(idx + 1)..^1];
                if (int.TryParse(inner, out var n)) return n;
            }
            return 8;
        }

        // Conservative expression type inference
        private static string InferExprType(TypedCilExpr expr, Dictionary<string, string> paramsMap, Dictionary<string, string> locals)
        {
            if (expr == null) return "unknown";
            switch (expr)
            {
                case TypedCilLiteral l:
                    return InferLiteralType(l.Value);
                case TypedCilIdentifier id:
                    {
                        if (locals.TryGetValue(id.Name ?? "", out var lt)) return lt;
                        if (paramsMap.TryGetValue(id.Name ?? "", out var pt)) return pt;
                        return "unknown";
                    }
                case TypedCilUnary u:
                    return InferExprType(u.Operand, paramsMap, locals);
                case TypedCilBinary b:
                    {
                        var lt = InferExprType(b.Left, paramsMap, locals);
                        var rt = InferExprType(b.Right, paramsMap, locals);
                        // numeric promotion heuristic
                        if (lt == "f64" || rt == "f64") return "f64";
                        if (lt.StartsWith("i") || rt.StartsWith("i")) return "i64";
                        return lt != "unknown" ? lt : rt;
                    }
                case TypedCilCallExpr ce:
                    // unknown without symbol table; assume i64 for arithmetic calls, string for print-like
                    if (!string.IsNullOrEmpty(ce.Callee) && ce.Callee.IndexOf("print", StringComparison.OrdinalIgnoreCase) >= 0) return "int";
                    return "i64";
                case TypedCilVariantConstruct vc:
                    return vc.Variant;
                case TypedCilCast c:
                    return c.TargetType;
                case TypedCilTupleExpr te:
                    return "tuple";
                case TypedCilIndex ix:
                    return "i64";
                case TypedCilChecksum _:
                    return "u64";
                case TypedCilCipher _:
                    return "string";
                default:
                    return "unknown";
            }
        }

        private static string InferLiteralType(string lit)
        {
            if (string.IsNullOrEmpty(lit)) return "int";
            var t = lit.Trim();
            if ((t.StartsWith("\"") && t.EndsWith("\"")) || (t.StartsWith("'") && t.EndsWith("'"))) return "string";
            if (t.Contains(".")) return "f64";
            if (long.TryParse(t, out _)) return "i64";
            return "int";
        }

        private static bool TypeCompatible(string declared, string inferred)
        {
            if (string.IsNullOrEmpty(declared) || string.IsNullOrEmpty(inferred)) return true;
            var d = declared.Trim().ToLowerInvariant();
            var i = inferred.Trim().ToLowerInvariant();
            if (d == i) return true;
            if (d.StartsWith("i") && i.StartsWith("i")) return true;
            if (d == "string" && i == "string") return true;
            if ((d == "i64" || d == "int64") && i.StartsWith("i")) return true;
            if ((d == "f64" || d == "double") && i == "f64") return true;
            // allow pointer/void* compatibility
            if (d == "void*" || d.EndsWith("*")) return true;
            return false;
        }

        private static Dictionary<string, string> ParseParamsSignature(string ps)
        {
            var map = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            if (string.IsNullOrEmpty(ps)) return map;
            var s = ps.Trim();
            if (s.StartsWith("(") && s.EndsWith(")")) s = s[1..^1];
            if (string.IsNullOrWhiteSpace(s)) return map;
            foreach (var part in s.Split(',', StringSplitOptions.RemoveEmptyEntries))
            {
                var kv = part.Split(':', 2);
                if (kv.Length == 2)
                    map[kv[0].Trim()] = kv[1].Trim();
            }
            return map;
        }

        private static string CombineRequirements(string? existing, string add)
        {
            if (string.IsNullOrEmpty(existing)) return add;
            var parts = existing.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries).ToList();
            if (!parts.Contains(add, StringComparer.OrdinalIgnoreCase)) parts.Add(add);
            return string.Join(", ", parts);
        }
    }
}

csharp RANE_Today/src/CIAM/TypedCilResolver.cs
using System;
using System.Collections.Generic;
using System.Linq;

namespace RANE.CIAM
{
    // TypedCilResolver: lightweight type-checking and heuristic annotation pass over TypedCilModule.
    // - Performs conservative type inference for Let/Return expressions.
    // - Emits diagnostics in proc Annotations when obvious mismatches occur.
    // - Adds opt.hints (e.g., vectorize) and simple frame hints (frame.allocs.heap) when patterns are detected.
    //
    // This is intentionally conservative and auditable; it does not change semantics.
    public static class TypedCilResolver
    {
        public static TypedCilModule AnalyzeAndAnnotate(TypedCilModule module)
        {
            if (module == null) throw new ArgumentNullException(nameof(module));

            var newProcs = new List<TypedCilProc>();
            foreach (var proc in module.Procs ?? Array.Empty<TypedCilProc>())
            {
                var ann = proc.Annotations != null
                    ? new Dictionary<string, string>(proc.Annotations, StringComparer.OrdinalIgnoreCase)
                    : new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

                // Build parameter type map from proc.Params string if present: "(a:i64,b:i64)"
                var paramTypes = ParseParamsSignature(proc.Params);

                // locals type map
                var locals = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

                // diagnostics collection
                var diags = new List<string>();

                // heuristic flags
                bool hasLoop = false;
                bool hasAlloc = false;
                bool vectorizeCandidate = false;

                // per-let explicit size/placement (resolver will emit these so FramePlanner/Emitter can use exact sizes)
                var perLocalPlacement = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

                // scan body in order to infer types & collect flags
                if (proc.Body != null)
                {
                    foreach (var stmt in proc.Body)
                    {
                        switch (stmt)
                        {
                            case TypedCilLet lt:
                                {
                                    var inferred = InferExprType(lt.Expr, paramTypes, locals);
                                    if (!string.IsNullOrEmpty(inferred))
                                    {
                                        locals[lt.Name] = inferred;
                                    }

                                    // check declared type if available
                                    if (!string.IsNullOrEmpty(lt.Type) && !string.IsNullOrEmpty(inferred))
                                    {
                                        if (!TypeCompatible(lt.Type, inferred))
                                        {
                                            diags.Add($"Let '{lt.Name}' declared {lt.Type} but inferred {inferred}");
                                        }
                                    }

                                    // detect vector pattern by naming convention (_v4d or _v4d_out)
                                    if ((lt.Name ?? "").EndsWith("_v4d_out", StringComparison.OrdinalIgnoreCase) &&
                                        lt.Expr is TypedCilBinary bin &&
                                        bin.Left is TypedCilIdentifier lid && bin.Right is TypedCilIdentifier rid &&
                                        (lid.Name ?? "").EndsWith("_v4d", StringComparison.OrdinalIgnoreCase) &&
                                        (rid.Name ?? "").EndsWith("_v4d", StringComparison.OrdinalIgnoreCase))
                                    {
                                        vectorizeCandidate = true;
                                    }

                                    // resolver computes exact per-let size when possible (string literal or explicit array type or struct)
                                    var letSize = ResolveLetSize(lt, module);
                                    var preferHeap = lt.Type != null && (lt.Type.EndsWith("*") || lt.Type.IndexOf("array", StringComparison.OrdinalIgnoreCase) >= 0);
                                    if (letSize > 128 || preferHeap)
                                    {
                                        perLocalPlacement[lt.Name] = "heap";
                                        hasAlloc = true;
                                    }
                                    else if (letSize > 0)
                                    {
                                        perLocalPlacement[lt.Name] = $"stack:{letSize}";
                                    }

                                    break;
                                }
                            case TypedCilLoop loop:
                                {
                                    hasLoop = true;
                                    // examine loop body for vectorizable let
                                    if (loop.Body != null)
                                    {
                                        foreach (var lstmt in loop.Body.OfType<TypedCilLet>())
                                        {
                                            if ((lstmt.Name ?? "").EndsWith("_v4d_out", StringComparison.OrdinalIgnoreCase) &&
                                                lstmt.Expr is TypedCilBinary b2 &&
                                                b2.Left is TypedCilIdentifier lid2 && b2.Right is TypedCilIdentifier rid2 &&
                                                (lid2.Name ?? "").EndsWith("_v4d", StringComparison.OrdinalIgnoreCase) &&
                                                (rid2.Name ?? "").EndsWith("_v4d", StringComparison.OrdinalIgnoreCase))
                                            {
                                                vectorizeCandidate = true;
                                            }
                                        }
                                    }
                                    break;
                                }
                            case TypedCilAlloc _:
                                hasAlloc = true;
                                break;
                            case TypedCilMaloc _:
                                hasAlloc = true;
                                break;
                            case TypedCilSpawn _:
                            case TypedCilAsync _:
                                // spawn/async mark function as having concurrency capability
                                ann["resolved.requires"] = CombineRequirements(ann.TryGetValue("resolved.requires", out var r) ? r : null, "threads");
                                break;
                            case TypedCilChecksum _:
                            case TypedCilCipher _:
                                // crypto capability
                                ann["resolved.requires"] = CombineRequirements(ann.TryGetValue("resolved.requires", out var r2) ? r2 : null, "crypto");
                                break;
                            default:
                                break;
                        }
                    }
                }

                // attach per-local placement annotations into proc annotations so FramePlanner and Emitter can trust exact sizes
                foreach (var kv in perLocalPlacement.OrderBy(kv => kv.Key, StringComparer.Ordinal))
                {
                    var key = $"local.{kv.Key}";
                    if (!ann.ContainsKey(key)) ann[key] = kv.Value;
                }

                // attach diagnostics
                if (diags.Count > 0)
                    ann["resolved.diagnostics"] = string.Join(" || ", diags);

                if (hasAlloc)
                    ann["frame.allocs.heap"] = "true";

                // compact opt.hints annotation: prefer existing resolver hints, otherwise add vectorize candidate
                var existingHints = ann.TryGetValue("opt.hints", out var eh) ? eh : null;
                var hints = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                if (!string.IsNullOrEmpty(existingHints))
                {
                    foreach (var part in existingHints.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
                        hints.Add(part);
                }
                if (vectorizeCandidate || hasLoop)
                    hints.Add("vectorize");
                if (proc.Body != null && proc.Body.Count >= 12)
                    hints.Add("pgo_candidate");

                if (hints.Count > 0)
                    ann["opt.hints"] = string.Join(", ", hints.OrderBy(x => x, StringComparer.OrdinalIgnoreCase));

                // return new proc with augmented annotations
                newProcs.Add(new TypedCilProc(proc.Name, proc.Visibility, proc.RetType, proc.Params, proc.Requires, proc.Body, ann));
            }

            // produce new module with same other collections
            return new TypedCilModule(
                module.ModuleName,
                module.Imports,
                module.Types,
                module.Structs,
                module.Enums,
                module.Variants,
                module.Mmios,
                module.Capabilities,
                newProcs,
                module.Nodes);
        }

        // Conservative per-let size resolver (when possible). Now consults module struct definitions and array syntax.
        private static int ResolveLetSize(TypedCilLet lt, TypedCilModule module)
        {
            if (lt == null) return 0;

            // explicit array sizing like "u8[256]" or nested arrays
            if (!string.IsNullOrEmpty(lt.Type))
            {
                var t = lt.Type.Trim();

                // bracketed array form: base[n], possibly nested like "u8[4][8]" we'll treat top-level only for local sizing.
                var firstBracket = t.IndexOf('[');
                if (firstBracket > 0 && t.EndsWith("]"))
                {
                    var baseType = t.Substring(0, firstBracket);
                    var inner = t[(firstBracket + 1)..^1];
                    if (int.TryParse(inner, out var n))
                    {
                        // element size
                        var (elemSize, _) = GetTypeSizeAndAlign(baseType, module);
                        long total = (long)elemSize * n;
                        return total > int.MaxValue ? int.MaxValue : (int)total;
                    }
                }

                // pointer -> pointer-sized
                if (t.EndsWith("*")) return 8;

                // if type matches a struct name in module, compute full struct layout size
                var sdef = module?.Structs?.FirstOrDefault(s => string.Equals(s.Name, t, StringComparison.OrdinalIgnoreCase));
                if (sdef != null)
                {
                    var (size, _) = GetTypeSizeAndAlign(t, module);
                    return size;
                }
            }

            if (lt.Expr is TypedCilLiteral lit && lit.Value != null)
            {
                var s = lit.Value.Trim();
                if ((s.StartsWith("\"") && s.EndsWith("\"")) || (s.StartsWith("'") && s.EndsWith("'")))
                {
                    var inner = s.Substring(1, s.Length - 2);
                    return inner.Length + 1;
                }
                if (int.TryParse(s, out var v))
                    return sizeof(long); // treat numeric init as scalar
            }

            return 0;
        }

        // Compute size and alignment for a type name, handling primitives, arrays, pointers and structs.
        // Returns (size, alignment).
        private static (int size, int align) GetTypeSizeAndAlign(string typeName, TypedCilModule module)
        {
            if (string.IsNullOrEmpty(typeName)) return (8, 8);
            var t = typeName.Trim();

            // pointer types
            if (t.EndsWith("*")) return (8, 8);

            // array syntax like "u8[128]" or nested "T[4][8]" we handle only single bracket here
            var firstBracket = t.IndexOf('[');
            if (firstBracket > 0 && t.EndsWith("]"))
            {
                var baseType = t.Substring(0, firstBracket);
                var inner = t[(firstBracket + 1)..^1];
                if (int.TryParse(inner, out var count))
                {
                    var (elemSize, elemAlign) = GetTypeSizeAndAlign(baseType, module);
                    long total = (long)elemSize * count;
                    int align = elemAlign;
                    return (total > int.MaxValue ? int.MaxValue : (int)total, align);
                }
            }

            // primitives
            var low = t.ToLowerInvariant();
            if (low == "u8" || low == "i8" || low == "byte" || low == "char") return (1, 1);
            if (low == "i16" || low == "u16" || low == "short") return (2, 2);
            if (low == "i32" || low == "u32" || low == "int" || low == "int32") return (4, 4);
            if (low == "i64" || low == "u64" || low == "long" || low == "int64") return (8, 8);
            if (low == "f32" || low == "float") return (4, 4);
            if (low == "f64" || low == "double") return (8, 8);
            if (low == "string") return (8, 8); // pointer-sized
            if (low == "bool") return (1, 1);

            // struct layout: find definition in module
            if (module != null && module.Structs != null)
            {
                var sdef = module.Structs.FirstOrDefault(s => string.Equals(s.Name, t, StringComparison.OrdinalIgnoreCase));
                if (sdef != null)
                {
                    long offset = 0;
                    int structAlign = 1;
                    foreach (var f in sdef.Fields)
                    {
                        var (fsize, falign) = GetTypeSizeAndAlign(f.Type, module);
                        structAlign = Math.Max(structAlign, falign);
                        // align current offset to field alignment
                        if (falign > 0)
                        {
                            var rem = offset % falign;
                            if (rem != 0) offset += (falign - rem);
                        }
                        offset += fsize;
                    }
                    // final padding to struct alignment
                    if (structAlign > 0)
                    {
                        var rem = offset % structAlign;
                        if (rem != 0) offset += (structAlign - rem);
                    }
                    var finalSize = offset > int.MaxValue ? int.MaxValue : (int)offset;
                    return (finalSize, structAlign);
                }
            }

            // fallback default to 8/8
            return (8, 8);
        }

        // small wrapper kept for compatibility with older codepaths (returns size only).
        private static int ResolveTypeSizeSimple(string t, TypedCilModule module)
        {
            var (sz, _) = GetTypeSizeAndAlign(t, module);
            return sz;
        }

        // Conservative expression type inference
        private static string InferExprType(TypedCilExpr expr, Dictionary<string, string> paramsMap, Dictionary<string, string> locals)
        {
            if (expr == null) return "unknown";
            switch (expr)
            {
                case TypedCilLiteral l:
                    return InferLiteralType(l.Value);
                case TypedCilIdentifier id:
                    {
                        if (locals.TryGetValue(id.Name ?? "", out var lt)) return lt;
                        if (paramsMap.TryGetValue(id.Name ?? "", out var pt)) return pt;
                        return "unknown";
                    }
                case TypedCilUnary u:
                    return InferExprType(u.Operand, paramsMap, locals);
                case TypedCilBinary b:
                    {
                        var lt = InferExprType(b.Left, paramsMap, locals);
                        var rt = InferExprType(b.Right, paramsMap, locals);
                        // numeric promotion heuristic
                        if (lt == "f64" || rt == "f64") return "f64";
                        if (lt.StartsWith("i") || rt.StartsWith("i")) return "i64";
                        return lt != "unknown" ? lt : rt;
                    }
                case TypedCilCallExpr ce:
                    // unknown without symbol table; assume i64 for arithmetic calls, string for print-like
                    if (!string.IsNullOrEmpty(ce.Callee) && ce.Callee.IndexOf("print", StringComparison.OrdinalIgnoreCase) >= 0) return "int";
                    return "i64";
                case TypedCilVariantConstruct vc:
                    return vc.Variant;
                case TypedCilCast c:
                    return c.TargetType;
                case TypedCilTupleExpr te:
                    return "tuple";
                case TypedCilIndex ix:
                    return "i64";
                case TypedCilChecksum _:
                    return "u64";
                case TypedCilCipher _:
                    return "string";
                default:
                    return "unknown";
            }
        }

        private static string InferLiteralType(string lit)
        {
            if (string.IsNullOrEmpty(lit)) return "int";
            var t = lit.Trim();
            if ((t.StartsWith("\"") && t.EndsWith("\"")) || (t.StartsWith("'") && t.EndsWith("'"))) return "string";
            if (t.Contains(".")) return "f64";
            if (long.TryParse(t, out _)) return "i64";
            return "int";
        }

        private static bool TypeCompatible(string declared, string inferred)
        {
            if (string.IsNullOrEmpty(declared) || string.IsNullOrEmpty(inferred)) return true;
            var d = declared.Trim().ToLowerInvariant();
            var i = inferred.Trim().ToLowerInvariant();
            if (d == i) return true;
            if (d.StartsWith("i") && i.StartsWith("i")) return true;
            if (d == "string" && i == "string") return true;
            if ((d == "i64" || d == "int64") && i.StartsWith("i")) return true;
            if ((d == "f64" || d == "double") && i == "f64") return true;
            // allow pointer/void* compatibility
            if (d == "void*" || d.EndsWith("*")) return true;
            return false;
        }

        private static Dictionary<string, string> ParseParamsSignature(string ps)
        {
            var map = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            if (string.IsNullOrEmpty(ps)) return map;
            var s = ps.Trim();
            if (s.StartsWith("(") && s.EndsWith(")")) s = s[1..^1];
            if (string.IsNullOrWhiteSpace(s)) return map;
            foreach (var part in s.Split(',', StringSplitOptions.RemoveEmptyEntries))
            {
                var kv = part.Split(':', 2);
                if (kv.Length == 2)
                    map[kv[0].Trim()] = kv[1].Trim();
            }
            return map;
        }

        private static string CombineRequirements(string? existing, string add)
        {
            if (string.IsNullOrEmpty(existing)) return add;
            var parts = existing.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries).ToList();
            if (!parts.Contains(add, StringComparer.OrdinalIgnoreCase)) parts.Add(add);
            return string.Join(", ", parts);
        }
    }
}

