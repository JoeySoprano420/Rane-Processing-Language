csharp RANE_Today/tests/StructLayoutTests.cs
using System;
using System.Collections.Generic;
using RANE.CIAM;
using Xunit;

namespace RANE.Tests
{
    public class StructLayoutTests
    {
        [Fact]
        public void NestedStructs_Produce_Correct_Stack_Size()
        {
            // Inner: { a: u8, b: u32 } -> size = 8 (1 + 3 pad + 4) align=4
            var inner = new TypedCilStruct("Inner", new[] { new TypedCilField("a", "u8"), new TypedCilField("b", "u32") }, Derive: null);
            // Outer: { i: Inner, c: u16 } -> inner (8) + u16 (2) = 10 -> padded to struct align (4) -> 12
            var outer = new TypedCilStruct("Outer", new[] { new TypedCilField("i", "Inner"), new TypedCilField("c", "u16") }, Derive: null);

            var body = new TypedCilStmt[]
            {
                new TypedCilLet("buf", "Outer", new TypedCilLiteral("0")),
                new TypedCilReturn(new TypedCilLiteral("0"))
            };
            var proc = new TypedCilProc("test_proc", "public", "i64", "()", Array.Empty<string>(), body, Annotations: null);
            var module = new TypedCilModule("layout_mod",
                Array.Empty<TypedCilImport>(),
                Array.Empty<TypedCilType>(),
                new[] { inner, outer },
                Array.Empty<TypedCilEnum>(),
                Array.Empty<TypedCilVariant>(),
                Array.Empty<TypedCilMMIO>(),
                Array.Empty<TypedCilCapability>(),
                new[] { proc },
                Array.Empty<TypedCilNode>());

            var resolved = TypedCilResolver.AnalyzeAndAnnotate(module);
            var p = resolved.Procs[0];
            Assert.True(p.Annotations != null && p.Annotations.ContainsKey("local.buf"));
            Assert.Equal("stack:12", p.Annotations["local.buf"], ignoreCase: true);
        }

        [Fact]
        public void ArrayField_Sizes_Are_Computed_Correctly()
        {
            var holder = new TypedCilStruct("ArrHolder", new[] { new TypedCilField("arr", "u8[10]") }, Derive: null);
            var body = new TypedCilStmt[]
            {
                new TypedCilLet("h", "ArrHolder", new TypedCilLiteral("0")),
                new TypedCilReturn(new TypedCilLiteral("0"))
            };
            var proc = new TypedCilProc("arr_proc", "public", "i64", "()", Array.Empty<string>(), body, Annotations: null);
            var module = new TypedCilModule("arr_mod",
                Array.Empty<TypedCilImport>(),
                Array.Empty<TypedCilType>(),
                new[] { holder },
                Array.Empty<TypedCilEnum>(),
                Array.Empty<TypedCilVariant>(),
                Array.Empty<TypedCilMMIO>(),
                Array.Empty<TypedCilCapability>(),
                new[] { proc },
                Array.Empty<TypedCilNode>());

            var resolved = TypedCilResolver.AnalyzeAndAnnotate(module);
            var p = resolved.Procs[0];
            Assert.True(p.Annotations != null && p.Annotations.ContainsKey("local.h"));
            Assert.Equal("stack:10", p.Annotations["local.h"], ignoreCase: true);
        }

        [Fact]
        public void Struct_Align_Override_Produces_Padded_Size()
        {
            var s = new TypedCilStruct("Aligned", new[] { new TypedCilField("a", "u8"), new TypedCilField("b", "u32") }, Derive: "align=16");
            var body = new TypedCilStmt[]
            {
                new TypedCilLet("x", "Aligned", new TypedCilLiteral("0")),
                new TypedCilReturn(new TypedCilLiteral("0"))
            };
            var proc = new TypedCilProc("align_proc", "public", "i64", "()", Array.Empty<string>(), body, Annotations: null);
            var module = new TypedCilModule("align_mod",
                Array.Empty<TypedCilImport>(),
                Array.Empty<TypedCilType>(),
                new[] { s },
                Array.Empty<TypedCilEnum>(),
                Array.Empty<TypedCilVariant>(),
                Array.Empty<TypedCilMMIO>(),
                Array.Empty<TypedCilCapability>(),
                new[] { proc },
                Array.Empty<TypedCilNode>());

            var resolved = TypedCilResolver.AnalyzeAndAnnotate(module);
            var p = resolved.Procs[0];
            Assert.True(p.Annotations != null && p.Annotations.ContainsKey("local.x"));
            // inner natural size would be 8, but align override to 16 pads to 16
            Assert.Equal("stack:16", p.Annotations["local.x"], ignoreCase: true);
        }
    }
}
