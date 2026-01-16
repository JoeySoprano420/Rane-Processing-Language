using System;

csharp RANE_Today/tests/FramePlannerEmitterTests.cs
using System;
using System.IO;
using System.Linq;
using RANE.CIAM;
using RANE.CIAM.Backend;
using Xunit;

namespace RANE.Tests
{
    public class FramePlannerEmitterTests
    {
        [Fact]
        public void When_FrameAllocHeap_Emitter_Uses_Heap_Not_Stack()
        {
            var body = new TypedCilStmt[]
            {
                // This let looks like a buffer and should be placed on heap by planner
                new TypedCilLet("buf", "u8[256]", new TypedCilLiteral("\"\"")),
                new TypedCilReturn(new TypedCilLiteral("0"))
            };
            var proc = new TypedCilProc("heap_proc", "public", "i64", "()", Array.Empty<string>(), body, Annotations: null);
            var module = new TypedCilModule("heap_mod", Array.Empty<TypedCilImport>(), Array.Empty<TypedCilType>(), Array.Empty<TypedCilStruct>(), Array.Empty<TypedCilEnum>(), Array.Empty<TypedCilVariant>(), Array.Empty<TypedCilMMIO>(), Array.Empty<TypedCilCapability>(), new[] { proc }, Array.Empty<TypedCilNode>());

            // run resolver and emission preparation to get frame annotations
            var resolved = TypedCilResolver.AnalyzeAndAnnotate(module);
            var prepared = EmissionLaw.PrepareModuleForEmission(resolved);

            var c = NativeEmitter.EmitCSource(prepared);

            // ensure emitter used runtime alloc instead of stack array declaration for 'buf'
            Assert.Contains("rane_rt_alloc", c);
            Assert.DoesNotContain("char buf[", c);
            Assert.DoesNotContain("u8 buf[", c);
        }
    }
}

csharp RANE_Today/tests/FramePlannerEmitterTests.cs
using System;
using System.IO;
using System.Linq;
using RANE.CIAM;
using RANE.CIAM.Backend;
using Xunit;

namespace RANE.Tests
{
    public class FramePlannerEmitterTests
    {
        [Fact]
        public void When_FrameAllocHeap_Emitter_Uses_Heap_Not_Stack()
        {
            var body = new TypedCilStmt[]
            {
                // This let looks like a buffer and should be placed on heap by planner
                new TypedCilLet("buf", "u8[256]", new TypedCilLiteral("\"\"")),
                new TypedCilReturn(new TypedCilLiteral("0"))
            };
            var proc = new TypedCilProc("heap_proc", "public", "i64", "()", Array.Empty<string>(), body, Annotations: null);
            var module = new TypedCilModule("heap_mod", Array.Empty<TypedCilImport>(), Array.Empty<TypedCilType>(), Array.Empty<TypedCilStruct>(), Array.Empty<TypedCilEnum>(), Array.Empty<TypedCilVariant>(), Array.Empty<TypedCilMMIO>(), Array.Empty<TypedCilCapability>(), new[] { proc }, Array.Empty<TypedCilNode>());

            // run resolver and emission preparation to get frame annotations
            var resolved = TypedCilResolver.AnalyzeAndAnnotate(module);
            var prepared = EmissionLaw.PrepareModuleForEmission(resolved);

            var c = NativeEmitter.EmitCSource(prepared);

            // ensure emitter used runtime alloc instead of stack array declaration for 'buf'
            Assert.Contains("rane_rt_alloc", c);
            Assert.DoesNotContain("char buf[", c);
            Assert.DoesNotContain("u8 buf[", c);
        }

        [Fact]
        public void When_LocalAnnotatedStack_Emitter_Emits_StackArray()
        {
            var body = new TypedCilStmt[]
            {
                // resolver/frameplanner should see explicit per-local annotation "local.buf" = "stack:256" and emitter should honor it.
                new TypedCilLet("buf", "u8[256]", new TypedCilLiteral("\"\"")),
                new TypedCilReturn(new TypedCilLiteral("0"))
            };
            var ann = new System.Collections.Generic.Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
            {
                // pre-annotate proc to simulate resolver output: force stack placement for 'buf'
                ["local.buf"] = "stack:256"
            };
            var proc = new TypedCilProc("stack_proc", "public", "i64", "()", Array.Empty<string>(), body, ann);
            var module = new TypedCilModule("stack_mod", Array.Empty<TypedCilImport>(), Array.Empty<TypedCilType>(), Array.Empty<TypedCilStruct>(), Array.Empty<TypedCilEnum>(), Array.Empty<TypedCilVariant>(), Array.Empty<TypedCilMMIO>(), Array.Empty<TypedCilCapability>(), new[] { proc }, Array.Empty<TypedCilNode>());

            // run resolver and emission preparation (EmissionLaw will preserve the local.* annotation)
            var resolved = TypedCilResolver.AnalyzeAndAnnotate(module);
            var prepared = EmissionLaw.PrepareModuleForEmission(resolved);

            var c = NativeEmitter.EmitCSource(prepared);

            // emitter should emit concrete stack array for 'buf'
            Assert.Contains("char buf[256];", c);
            // should not use rane_rt_alloc for this variable
            Assert.DoesNotContain("rane_rt_alloc", c);
        }
    }
}

