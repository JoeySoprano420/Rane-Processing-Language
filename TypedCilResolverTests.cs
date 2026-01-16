csharp RANE_Today/tests/TypedCilResolverTests.cs
using System;
using System.Collections.Generic;
using RANE.CIAM;
using Xunit;

namespace RANE.Tests
{
    public class TypedCilResolverTests
    {
        [Fact]
        public void TypedCilResolver_Annotates_Vectorize_For_LoopAndVectorPattern()
        {
            var loopBody = new TypedCilStmt[]
            {
                new TypedCilLet("res_v4d_out", "i64", new TypedCilBinary("+", new TypedCilIdentifier("a_v4d"), new TypedCilIdentifier("b_v4d")))
            };
            var loop = new TypedCilLoop(loopBody);
            var proc = new TypedCilProc("vec_proc", "public", "i64", "()", Array.Empty<string>(), new TypedCilStmt[] { loop }, Annotations: null);
            var module = new TypedCilModule("tcil_resolve_mod", Array.Empty<TypedCilImport>(), Array.Empty<TypedCilType>(), Array.Empty<TypedCilStruct>(), Array.Empty<TypedCilEnum>(), Array.Empty<TypedCilVariant>(), Array.Empty<TypedCilMMIO>(), Array.Empty<TypedCilCapability>(), new[] { proc }, Array.Empty<TypedCilNode>());

            var outModule = TypedCilResolver.AnalyzeAndAnnotate(module);
            var outProc = outModule.Procs[0];
            Assert.True(outProc.Annotations != null && outProc.Annotations.ContainsKey("opt.hints"));
            Assert.Contains("vectorize", outProc.Annotations["opt.hints"], StringComparison.OrdinalIgnoreCase);
        }

        [Fact]
        public void TypedCilResolver_Detects_Heap_Allocations()
        {
            var body = new TypedCilStmt[]
            {
                new TypedCilAlloc("buf", "u8*", new TypedCilLiteral("256")),
                new TypedCilLet("x", "i64", new TypedCilLiteral("1"))
            };
            var proc = new TypedCilProc("alloc_proc", "public", "i64", "()", Array.Empty<string>(), body, Annotations: null);
            var module = new TypedCilModule("alloc_mod", Array.Empty<TypedCilImport>(), Array.Empty<TypedCilType>(), Array.Empty<TypedCilStruct>(), Array.Empty<TypedCilEnum>(), Array.Empty<TypedCilVariant>(), Array.Empty<TypedCilMMIO>(), Array.Empty<TypedCilCapability>(), new[] { proc }, Array.Empty<TypedCilNode>());

            var outModule = TypedCilResolver.AnalyzeAndAnnotate(module);
            var outProc = outModule.Procs[0];
            Assert.True(outProc.Annotations != null && outProc.Annotations.ContainsKey("frame.allocs.heap"));
            Assert.Equal("true", outProc.Annotations["frame.allocs.heap"], ignoreCase: true);
        }
    }
}