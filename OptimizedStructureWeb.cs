csharp RANE_Today/src/CIAM/OptimizedStructureWeb.cs
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace RANE.CIAM
{
    // Optimized Structure Web (OSW) builder
    // - Conservative, deterministic transformation from TypedCilModule -> OSW representation.
    // - Creates per-proc nodes (one per statement) and linear/control edges.
    // - Emits a small deterministic JSON artifact "{module}.osw.json" for CI inspection.
    //
    // This pass is intentionally simple and safe: it does not attempt aggressive IR transforms
    // (those belong in StructuralOptimization or downstream passes). The OSW is a stable,
    // auditable intermediate that downstream frame planner / codegen can consume.
    public static class OptimizedStructureWeb
    {
        // Lightweight OSW model records
        public sealed record OswModule(string ModuleName, IReadOnlyList<OswFunction> Functions, DateTime Generated);
        public sealed record OswFunction(string Name, IReadOnlyList<OswNode> Nodes, IReadOnlyList<OswEdge> Edges, IReadOnlyDictionary<string,string>? Annotations);
        public sealed record OswNode(string Id, string Kind, string? Text, IReadOnlyDictionary<string,string>? Annotations);
        public sealed record OswEdge(string From, string To, string Label);

        // Build OSW from TypedCilModule and write a deterministic JSON alongside the current working dir.
        public static OswModule BuildAndWrite(TypedCilModule module)
        {
            if (module == null) throw new ArgumentNullException(nameof(module));

            var functions = new List<OswFunction>();
            var audits = new List<AuditRecord>();

            foreach (var proc in module.Procs ?? Array.Empty<TypedCilProc>())
            {
                var name = proc.Name ?? "anon";
                var nodes = new List<OswNode>();
                var edges = new List<OswEdge>();

                var stmts = proc.Body ?? Array.Empty<TypedCilStmt>();
                for (int i = 0; i < stmts.Count; i++)
                {
                    var s = stmts[i];
                    var nid = $"{SanitizeId(name)}_n{i}";
                    var kind = s?.GetType().Name ?? "Empty";
                    var text = SummarizeStmt(s);
                    var ann = new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);

                    // preserve any proc-level annotations (resolver hints) on first node for visibility
                    if (i == 0 && proc is not null && TryGetAnnotations(proc, out var pAnn) && pAnn != null)
                    {
                        foreach (var kv in pAnn) ann[kv.Key] = kv.Value;
                    }

                    nodes.Add(new OswNode(nid, kind, text, ann.Count > 0 ? ann : null));
                    // linear edge to next unless stmt is a return
                    if (i + 1 < stmts.Count && !(s is TypedCilReturn))
                        edges.Add(new OswEdge(nid, $"{SanitizeId(name)}_n{i+1}", "fallthrough"));

                    // basic branching support for PatternMatch: add edges to case-first nodes
                    if (s is TypedCilPatternMatch pm)
                    {
                        // each case body will immediately map to subsequent nodes: connect from match node to its first body stmt if present
                        for (int cIdx = 0; cIdx < pm.Cases.Count; cIdx++)
                        {
                            var caseLabel = $"case{cIdx}";
                            // body may be empty - then edge to fallthrough next statement
                            if (pm.Cases[cIdx].Body != null && pm.Cases[cIdx].Body.Count > 0)
                            {
                                // find index of the first statement of the case within the original proc body:
                                // conservative: we don't splice case bodies into top-level sequence here, so use a labelled virtual target node.
                                var tgt = $"{nid}_ {caseLabel}";
                                // create a virtual node placeholder to indicate branch target (no body statements inline)
                                if (!nodes.Any(n => n.Id == tgt))
                                    nodes.Add(new OswNode(tgt, "MatchCaseTarget", $"case:{cIdx}", null));
                                edges.Add(new OswEdge(nid, tgt, $"match:{cIdx}"));
                            }
                            else
                            {
                                // empty case -> fallthrough to next statement
                                if (i + 1 < stmts.Count) edges.Add(new OswEdge(nid, $"{SanitizeId(name)}_n{i+1}", $"match:{cIdx}"));
                            }
                        }
                    }

                    // TryFinally: add edge from try->finally marker (virtual node)
                    if (s is TypedCilTryFinally tf)
                    {
                        var finalTgt = $"{nid}_finally";
                        if (!nodes.Any(n => n.Id == finalTgt))
                            nodes.Add(new OswNode(finalTgt, "FinallyTarget", "finally", null));
                        edges.Add(new OswEdge(nid, finalTgt, "finally"));
                    }
                }

                // Emit per-function audit
                audits.Add(AuditHelpers.MakeAudit("CIAM.OSW.FunctionEmitted", $"proc:{name}", 0, 0, name, $"OSW function '{name}' nodes={nodes.Count} edges={edges.Count}"));

                functions.Add(new OswFunction(name, nodes, edges, proc is not null && TryGetAnnotations(proc, out var annx) ? annx : null));
            }

            var osw = new OswModule(module.ModuleName, functions, GetDeterministicTimestamp());

            // write deterministic JSON artifact for CI review
            try
            {
                var outPath = $"{module.ModuleName}.osw.json";
                var opts = new JsonSerializerOptions { WriteIndented = true };
                var json = JsonSerializer.Serialize(osw, opts);
                File.WriteAllText(outPath, json);
            }
            catch
            {
                // best-effort: do not fail on IO write errors
            }

            // Also write audit file (deterministic)
            try
            {
                var auditPath = $"{module.ModuleName}.osw.audits.json";
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
                // ignore
            }

            return osw;
        }

        // Minimal helper to extract annotations from TypedCilProc if available (SemanticMaterialization propagated them).
        private static bool TryGetAnnotations(TypedCilProc p, out IReadOnlyDictionary<string,string>? ann)
        {
            ann = null;
            try
            {
                // TypedCilProc may or may not carry Annotations (preserved by materializer if present).
                // Use reflection-safe check (avoid compile-time coupling if typed differently).
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

        // Summarize a TypedCilStmt into a compact, deterministic text for OSW nodes.
        private static string SummarizeStmt(TypedCilStmt s)
        {
            if (s == null) return string.Empty;
            return s switch
            {
                TypedCilLet lt => $"let {lt.Name} : {lt.Type}",
                TypedCilReturn r => $"return {ShortExpr(r.Expr)}",
                TypedCilExprStmt es => $"expr {ShortExpr(es.Expr)}",
                TypedCilCall c => $"call {c.Call.Callee} -> {c.Lhs ?? "_"}",
                TypedCilTryFinally _ => "try/finally",
                TypedCilPatternMatch _ => "match",
                TypedCilLoop _ => "loop",
                _ => s.GetType().Name
            };
        }

        private static string ShortExpr(TypedCilExpr e)
        {
            if (e == null) return "";
            return e switch
            {
                TypedCilLiteral l => l.Value,
                TypedCilIdentifier id => id.Name,
                TypedCilUnary u => $"{u.Op}{ShortExpr(u.Operand)}",
                TypedCilBinary b => $"{ShortExpr(b.Left)} {b.Op} {ShortExpr(b.Right)}",
                TypedCilCallExpr ce => $"{ce.Callee}(...{(ce.Args?.Count ?? 0)})",
                TypedCilVariantConstruct vc => $"{vc.CaseName}(...{(vc.Payload?.Count ?? 0)})",
                TypedCilTupleExpr te => $"tuple({(te.Elements?.Count ?? 0)})",
                _ => e.GetType().Name
            };
        }

        private static string SanitizeId(string s) => (s ?? "anon").Replace(' ', '_').Replace('-', '_').Replace(':', '_');

        // Deterministic timestamp helper (shared with AuditHelpers)
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