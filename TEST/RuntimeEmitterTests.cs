using System;

csharp RANE_Today/tests/RuntimeEmitterTests.cs
using System;
using System.IO;
using System.Linq;
using RANE.CIAM.Backend;
using Xunit;

namespace RANE.Tests
{
    public class RuntimeEmitterTests
    {
        [Fact]
        public void EmitCSource_Contains_RuntimeAlloc_Mutex_Spawn_Checksum_Cipher()
        {
            var body = new TypedCilStmt[]
            {
                new TypedCilAlloc("buf", "u8*", new TypedCilLiteral("128")),
                new TypedCilMutex("m"),
                new TypedCilLock(new TypedCilIdentifier("m")),
                new TypedCilUnlock(new TypedCilIdentifier("m")),
                new TypedCilSpawn(new TypedCilIdentifier("worker"), Array.Empty<TypedCilExpr>()),
                new TypedCilChecksum(new TypedCilLiteral("\"hello\""), "fnv1a"),
                new TypedCilRender(new TypedCilLiteral("\"done\""), "text"),
                new TypedCilDealloc(new TypedCilIdentifier("buf"))
            };

            var proc = new TypedCilProc("rt_test", "public", "i64", "()", Array.Empty<string>(), body, Annotations: null);
            var module = new TypedCilModule(
                "rt_mod",
                Array.Empty<TypedCilImport>(),
                Array.Empty<TypedCilType>(),
                Array.Empty<TypedCilStruct>(),
                Array.Empty<TypedCilEnum>(),
                Array.Empty<TypedCilVariant>(),
                Array.Empty<TypedCilMMIO>(),
                Array.Empty<TypedCilCapability>(),
                new[] { proc },
                Array.Empty<TypedCilNode>());

            var c = NativeEmitter.EmitCSource(module);

            Assert.Contains("rane_rt_alloc", c);
            Assert.Contains("rane_rt_mutex_init", c);
            Assert.Contains("rane_rt_mutex_lock", c);
            Assert.Contains("rane_rt_mutex_unlock", c);
            Assert.Contains("rane_rt_spawn", c);
            Assert.Contains("rane_rt_await", c); // may appear if async variants emitted
            Assert.Contains("rane_rt_checksum", c);
            Assert.Contains("rane_rt_cipher_xor", c); // present by signature in emitter header
        }

        [Fact]
        public void EmitCSource_VectorizedLoop_Emits_LoopIntrinsics_When_Annotated()
        {
            // Emit a proc with a loop and an annotation for vectorize_loop_count
            var loopBody = new TypedCilStmt[]
            {
                new TypedCilLet("res_v4d_out", "i64", new TypedCilBinary("+", new TypedCilIdentifier("a_v4d"), new TypedCilIdentifier("b_v4d")))
            };
            var loop = new TypedCilLoop(loopBody);
            var ann = new System.Collections.Generic.Dictionary<string,string>(StringComparer.OrdinalIgnoreCase) { ["opt.hints"] = "vectorize", ["vectorize_loop_count"] = "8" };
            var proc = new TypedCilProc("vec_loop", "public", "i64", "()", Array.Empty<string>(), new TypedCilStmt[] { loop }, ann);
            var module = new TypedCilModule("vec_mod", Array.Empty<TypedCilImport>(), Array.Empty<TypedCilType>(), Array.Empty<TypedCilStruct>(), Array.Empty<TypedCilEnum>(), Array.Empty<TypedCilVariant>(), Array.Empty<TypedCilMMIO>(), Array.Empty<TypedCilCapability>(), new[] { proc }, Array.Empty<TypedCilNode>());

            var c = NativeEmitter.EmitCSource(module);
            Assert.Contains("_mm256_loadu_pd", c);
            Assert.Contains("_mm256_add_pd", c);
            Assert.Contains("_mm256_storeu_pd", c);
            Assert.Contains("for (int i_", c); // loop emitted
        }
    }
}

csharp RANE_Today/tests/RuntimeEmitterTests.cs
using System;
using System.IO;
using System.Linq;
using RANE.CIAM;
using RANE.CIAM.Backend;
using Xunit;

namespace RANE.Tests
{
    public class RuntimeEmitterTests
    {
        [Fact]
        public void EmitCSource_Contains_RuntimeAlloc_Mutex_Spawn_Checksum_Cipher()
        {
            var body = new TypedCilStmt[]
            {
                new TypedCilAlloc("buf", "u8*", new TypedCilLiteral("128")),
                new TypedCilMutex("m"),
                new TypedCilLock(new TypedCilIdentifier("m")),
                new TypedCilUnlock(new TypedCilIdentifier("m")),
                new TypedCilSpawn(new TypedCilIdentifier("worker"), Array.Empty<TypedCilExpr>()),
                new TypedCilChecksum(new TypedCilLiteral("\"hello\""), "fnv1a"),
                new TypedCilRender(new TypedCilLiteral("\"done\""), "text"),
                new TypedCilDealloc(new TypedCilIdentifier("buf"))
            };

            var proc = new TypedCilProc("rt_test", "public", "i64", "()", Array.Empty<string>(), body, Annotations: null);
            var module = new TypedCilModule(
                "rt_mod",
                Array.Empty<TypedCilImport>(),
                Array.Empty<TypedCilType>(),
                Array.Empty<TypedCilStruct>(),
                Array.Empty<TypedCilEnum>(),
                Array.Empty<TypedCilVariant>(),
                Array.Empty<TypedCilMMIO>(),
                Array.Empty<TypedCilCapability>(),
                new[] { proc },
                Array.Empty<TypedCilNode>());

            // Run resolver to attach conservative annotations, then prepare via EmissionLaw so FramePlanner annotations are merged.
            var resolved = TypedCilResolver.AnalyzeAndAnnotate(module);
            var prepared = EmissionLaw.PrepareModuleForEmission(resolved);

            var c = NativeEmitter.EmitCSource(prepared);

            Assert.Contains("rane_rt_alloc", c);
            Assert.Contains("rane_rt_mutex_init", c);
            Assert.Contains("rane_rt_mutex_lock", c);
            Assert.Contains("rane_rt_mutex_unlock", c);
            Assert.Contains("rane_rt_spawn", c);
            Assert.Contains("rane_rt_await", c); // may appear if async variants emitted
            Assert.Contains("rane_rt_checksum", c);
            Assert.Contains("rane_rt_cipher_xor", c); // present by signature in emitter header
        }

        [Fact]
        public void EmitCSource_VectorizedLoop_Emits_LoopIntrinsics_When_Annotated()
        {
            // Emit a proc with a loop and an annotation for vectorize_loop_count
            var loopBody = new TypedCilStmt[]
            {
                new TypedCilLet("res_v4d_out", "i64", new TypedCilBinary("+", new TypedCilIdentifier("a_v4d"), new TypedCilIdentifier("b_v4d")))
            };
            var loop = new TypedCilLoop(loopBody);
            var ann = new System.Collections.Generic.Dictionary<string, string>(StringComparer.OrdinalIgnoreCase) { ["opt.hints"] = "vectorize", ["vectorize_loop_count"] = "8" };
            var proc = new TypedCilProc("vec_loop", "public", "i64", "()", Array.Empty<string>(), new TypedCilStmt[] { loop }, ann);
            var module = new TypedCilModule("vec_mod", Array.Empty<TypedCilImport>(), Array.Empty<TypedCilType>(), Array.Empty<TypedCilStruct>(), Array.Empty<TypedCilEnum>(), Array.Empty<TypedCilVariant>(), Array.Empty<TypedCilMMIO>(), Array.Empty<TypedCilCapability>(), new[] { proc }, Array.Empty<TypedCilNode>());

            var resolved = TypedCilResolver.AnalyzeAndAnnotate(module);
            var prepared = EmissionLaw.PrepareModuleForEmission(resolved);

            var c = NativeEmitter.EmitCSource(prepared);
            Assert.Contains("_mm256_loadu_pd", c);
            Assert.Contains("_mm256_add_pd", c);
            Assert.Contains("_mm256_storeu_pd", c);
            Assert.Contains("for (int i_", c); // loop emitted
        }
    }
}

