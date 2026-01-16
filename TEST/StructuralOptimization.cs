csharp RANE_Today/src/CIAM/StructuralOptimization.cs
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;

namespace RANE.CIAM
{
    // Structural optimization CIAM stage (conservative, deterministic, safe).
    // - Constant-folds simple integer unary/binary operations where both operands are integer literals.
    // - Removes pure Literal expression statements (no side-effects).
    // - Applies transformations in a functional/immutable style returning a new TypedCilModule.
    //
    // This pass is intentionally small and safe: it only folds integer arithmetic and removes
    // degenerate literal-expression statements. Expand as needed (dead-code elimination,
    // common subexpression elimination, inlining) once verification hooks exist.
    public static class StructuralOptimization
    {
        public static TypedCilModule Optimize(TypedCilModule module)
        {
            if (module is null) throw new ArgumentNullException(nameof(module));

            var newProcs = new List<TypedCilProc>();
            foreach (var p in module.Procs ?? Array.Empty<TypedCilProc>())
            {
                var newBody = new List<TypedCilStmt>();
                foreach (var s in p.Body ?? Array.Empty<TypedCilStmt>())
                {
                    var maybe = FoldStmt(s);
                    if (maybe != null) newBody.Add(maybe);
                }

                newProcs.Add(new TypedCilProc(
                    p.Name,
                    p.Visibility,
                    p.RetType,
                    p.Params,
                    p.Requires,
                    newBody));
            }

            // Keep other top-level pieces unchanged (structs, variants, etc.)
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

        private static TypedCilStmt? FoldStmt(TypedCilStmt s)
        {
            return s switch
            {
                TypedCilExprStmt es =>
                    // drop pure literal-only expr statements
                    FoldExpr(es.Expr) is TypedCilLiteral lit ? null : new TypedCilExprStmt(FoldExpr(es.Expr)),
                TypedCilLet lt =>
                    new TypedCilLet(lt.Name, lt.Type, FoldExpr(lt.Expr)),
                TypedCilReturn rt =>
                    new TypedCilReturn(FoldExpr(rt.Expr)),
                TypedCilCall c =>
                    // fold arguments inside call expression
                    new TypedCilCall(c.Lhs, new TypedCilCallExpr(c.Call.Callee, c.Call.Args.Select(a => FoldExpr(a)).ToArray())),
                TypedCilTryFinally tf =>
                {
                    var tryBody = tf.TryBody?.Select(t => FoldStmt(t)).Where(t => t != null).ToArray() ?? Array.Empty<TypedCilStmt>();
                    var finallyBody = tf.FinallyBody?.Select(t => FoldStmt(t)).Where(t => t != null).ToArray() ?? Array.Empty<TypedCilStmt>();
                    return new TypedCilTryFinally(tryBody!, finallyBody!);
                },
                TypedCilPatternMatch pm =>
                {
                    // fold subject and recursively fold case bodies and guards
                    var subj = FoldExpr(pm.Subject);
                    var cases = pm.Cases?.Select(c =>
                        new TypedCilPatternCase(
                            c.Pattern,
                            c.Guard != null ? FoldExpr(c.Guard) : null,
                            c.Body?.Select(b => FoldStmt(b)).Where(b => b != null).ToArray() ?? Array.Empty<TypedCilStmt>()
                        )).ToArray() ?? Array.Empty<TypedCilPatternCase>();
                    return new TypedCilPatternMatch(subj, cases);
                },
                _ => s
            };
        }

        private static TypedCilExpr FoldExpr(TypedCilExpr e)
        {
            if (e == null) return new TypedCilLiteral("0");

            switch (e)
            {
                case TypedCilLiteral l:
                    return l;

                case TypedCilIdentifier id:
                    return id;

                case TypedCilUnary u:
                    {
                        var child = FoldExpr(u.Operand);
                        if (child is TypedCilLiteral clit && TryParseInteger(clit.Value, out var v))
                        {
                            try
                            {
                                long res = u.Op switch
                                {
                                    "-" => -v,
                                    "+" => +v,
                                    "~" => ~v,
                                    _ => v
                                };
                                return new TypedCilLiteral(res.ToString(CultureInfo.InvariantCulture));
                            }
                            catch { /* overflow -> leave as-is */ }
                        }
                        return new TypedCilUnary(u.Op, child);
                    }

                case TypedCilBinary b:
                    {
                        var left = FoldExpr(b.Left);
                        var right = FoldExpr(b.Right);

                        if (left is TypedCilLiteral ll && right is TypedCilLiteral rl &&
                            TryParseInteger(ll.Value, out var lv) && TryParseInteger(rl.Value, out var rv))
                        {
                            try
                            {
                                long result = b.Op switch
                                {
                                    "+" => lv + rv,
                                    "-" => lv - rv,
                                    "*" => lv * rv,
                                    "/" => rv != 0 ? lv / rv : lv,
                                    "%" => rv != 0 ? lv % rv : lv,
                                    "<<" => lv << (int)rv,
                                    ">>" => lv >> (int)rv,
                                    _ => long.MinValue
                                };
                                if (result != long.MinValue)
                                    return new TypedCilLiteral(result.ToString(CultureInfo.InvariantCulture));
                            }
                            catch { /* overflow -> leave as-is */ }
                        }

                        return new TypedCilBinary(b.Op, left, right);
                    }

                case TypedCilCallExpr ce:
                    {
                        var args = ce.Args?.Select(a => FoldExpr(a)).ToArray() ?? Array.Empty<TypedCilExpr>();
                        return new TypedCilCallExpr(ce.Callee, args);
                    }

                case TypedCilVariantConstruct vc:
                    {
                        var payload = vc.Payload?.Select(p => FoldExpr(p)).ToArray() ?? Array.Empty<TypedCilExpr>();
                        return new TypedCilVariantConstruct(vc.Variant, vc.CaseName, payload);
                    }

                case TypedCilCast c:
                    return new TypedCilCast(FoldExpr(c.Expr), c.TargetType);

                case TypedCilTupleExpr te:
                    {
                        var elems = te.Elements?.Select(ei => FoldExpr(ei)).ToArray() ?? Array.Empty<TypedCilExpr>();
                        return new TypedCilTupleExpr(elems);
                    }

                default:
                    return e;
            }
        }

        private static bool TryParseInteger(string s, out long value)
        {
            value = 0;
            if (string.IsNullOrEmpty(s)) return false;
            var t = s.Trim().Replace("_", "");
            if (t.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
            {
                return long.TryParse(t.Substring(2), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out value);
            }
            if (t.StartsWith("0b", StringComparison.OrdinalIgnoreCase))
            {
                try
                {
                    var bits = t.Substring(2);
                    long acc = 0;
                    foreach (var ch in bits)
                    {
                        acc = (acc << 1) + (ch == '1' ? 1 : 0);
                    }
                    value = acc;
                    return true;
                }
                catch { return false; }
            }
            return long.TryParse(t, NumberStyles.Integer, CultureInfo.InvariantCulture, out value);
        }
    }
}
