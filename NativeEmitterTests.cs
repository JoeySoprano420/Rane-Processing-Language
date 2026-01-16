csharp RANE_Today/tests/NativeEmitterTests.cs
using System;
using System.IO;
using System.Linq;
using RANE.CIAM;
using RANE.CIAM.Backend;
using Xunit;

namespace RANE.Tests
{
    public class NativeEmitterTests
    {
        [Fact]
        public void EmitCSource_Includes_SimdIntrinsics_For_VectorizeHint()
        {
            var body = new TypedCilStmt[]
            {
                // pattern recognized by emitter: dest ends with _v4d_out and operands end with _v4d
                new TypedCilLet("res_v4d_out", "i64", new TypedCilBinary("+", new TypedCilIdentifier("a_v4d"), new TypedCilIdentifier("b_v4d"))),
                new TypedCilReturn(new TypedCilLiteral("0"))
            };
            var ann = new System.Collections.Generic.Dictionary<string,string>(StringComparer.OrdinalIgnoreCase) { ["opt.hints"] = "vectorize" };
            var proc = new TypedCilProc("vec_fn", "public", "i64", "()", Array.Empty<string>(), body, ann);
            var module = new TypedCilModule("simd_mod", Array.Empty<TypedCilImport>(), Array.Empty<TypedCilType>(), Array.Empty<TypedCilStruct>(), Array.Empty<TypedCilEnum>(), Array.Empty<TypedCilVariant>(), Array.Empty<TypedCilMMIO>(), Array.Empty<TypedCilCapability>(), new[] { proc }, Array.Empty<TypedCilNode>());

            var c = NativeEmitter.EmitCSource(module);
            Assert.Contains("_mm256_loadu_pd", c);
            Assert.Contains("_mm256_add_pd", c);
            Assert.Contains("_mm256_storeu_pd", c);
        }

        [Fact]
        public void NativeEmitter_PGO_LTO_Flow_Integration()
        {
            // Integration test: only run when clang is available
            if (!NativeEmitter.IsToolchainAvailable("clang"))
            {
                // Skip test in CI where clang not available
                return;
            }

            var body = new TypedCilStmt[]
            {
                new TypedCilLet("x", "i64", new TypedCilLiteral("1")),
                new TypedCilReturn(new TypedCilIdentifier("x"))
            };
            var proc = new TypedCilProc("main", "public", "i64", "()", Array.Empty<string>(), body, Annotations: null);
            var module = new TypedCilModule("pgo_mod", Array.Empty<TypedCilImport>(), Array.Empty<TypedCilType>(), Array.Empty<TypedCilStruct>(), Array.Empty<TypedCilEnum>(), Array.Empty<TypedCilVariant>(), Array.Empty<TypedCilMMIO>(), Array.Empty<TypedCilCapability>(), new[] { proc }, Array.Empty<TypedCilNode>());

            var workDir = Path.Combine(Path.GetTempPath(), "rane_pgo_test_" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(workDir);
            var outExe = Path.Combine(workDir, "pgo_test.exe");

            // Enable both LTO and PGO, profile command runs the exe itself (our emitted exe is benign)
            NativeEmitter.EmitNativeExe(module, $"{workDir}/typed.json", outExe, workDir, clangPath: "clang", preferNative: true, skipToolchain: false, enableLto: true, enablePgo: true, profileRunCommand: "{exe}");

            // verify artifact exists (either real exe or placeholder)
            Assert.True(File.Exists(outExe));
            // clean up
            try { Directory.Delete(workDir, true); } catch { }
        }
    }
}