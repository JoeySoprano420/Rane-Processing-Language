csharp RANE_Today/tests/StructuralOptimizationTests.cs
using System;
using System.Linq;
using RANE.CIAM;
using Xunit;

namespace RANE.Tests
{
    public class StructuralOptimizationTests
    {
        [Fact]
        public void ConstantFoldingAndLiteralExprRemoval_Works()
        {
            // Build a simple module with a proc containing:
            //   42;          -- literal expr (should be removed)
            //   return 2 + 3 -- should be folded to return 5
            var body = new TypedCilStmt[]
            {
                new TypedCilExprStmt(new TypedCilLiteral("42")),
                new TypedCilReturn(new TypedCilBinary("+", new TypedCilLiteral("2"), new TypedCilLiteral("3")))
            };

            var proc = new TypedCilProc("opt_test", "public", "i64", "()", Array.Empty<string>(), body);

            var module = new TypedCilModule(
                "opt_mod",
                Array.Empty<TypedCilImport>(),
                Array.Empty<TypedCilType>(),
                Array.Empty<TypedCilStruct>(),
                Array.Empty<TypedCilEnum>(),
                Array.Empty<TypedCilVariant>(),
                Array.Empty<TypedCilMMIO>(),
                Array.Empty<TypedCilCapability>(),
                new[] { proc },
                Array.Empty<TypedCilNode>());

            var opt = StructuralOptimization.Optimize(module);

            var outProc = opt.Procs.First(p => p.Name == "opt_test");
            // literal expr statement removed -> only Return remains
            Assert.Single(outProc.Body);
            var ret = Assert.IsType<TypedCilReturn>(outProc.Body[0]);
            var lit = Assert.IsType<TypedCilLiteral>(ret.Expr);
            Assert.Equal("5", lit.Value);
        }
    }
}