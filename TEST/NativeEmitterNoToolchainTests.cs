csharp RANE_Today/tests/NativeEmitterNoToolchainTests.cs
using System;
using System.IO;
using System.Linq;
using RANE.CIAM;
using RANE.CIAM.Backend;
using Xunit;

namespace RANE.Tests
{
    public class NativeEmitterNoToolchainTests
    {
        [Fact]
        public void EmitNativeExe_SkipToolchain_WritesCAndMetaAndPlaceholderExe()
        {
            var payloadFields = new[] {
                new TypedCilField("code", "i32"),
                new TypedCilField("msg", "string")
            };
            var payloadStruct = new TypedCilStruct("Payload", payloadFields, "Eq, Ord, Debug");

            var packetCases = new[] {
                new TypedCilVariantCase("Ping", Array.Empty<(string,string)>()),
                new TypedCilVariantCase("Pong", Array.Empty<(string,string)>()),
                new TypedCilVariantCase("Data", new[] { ("value", "Payload") })
            };
            var packetVariant = new TypedCilVariant("Packet", packetCases);

            var proc = new TypedCilProc("variant_test", "public", "int", "()", Array.Empty<string>(), Array.Empty<TypedCilStmt>());

            var moduleName = $"no_toolchain_{Guid.NewGuid():N}";
            var workDir = Path.Combine(Path.GetTempPath(), moduleName);
            Directory.CreateDirectory(workDir);

            var module = new TypedCilModule(moduleName,
                new[] { new TypedCilImport("rane_rt_print") },
                Array.Empty<TypedCilType>(),
                new[] { payloadStruct },
                Array.Empty<TypedCilEnum>(),
                new[] { packetVariant },
                Array.Empty<TypedCilMMIO>(),
                Array.Empty<TypedCilCapability>(),
                new[] { proc },
                Array.Empty<TypedCilNode>());

            var outputExe = Path.Combine(workDir, "out_placeholder.exe");
            var typedCilPbPath = Path.Combine(workDir, "out.typedcil.pb");

            // ensure no existing files
            if (File.Exists(outputExe)) File.Delete(outputExe);
            if (File.Exists(typedCilPbPath)) File.Delete(typedCilPbPath);

            // run emitter skipping toolchain
            NativeEmitter.EmitNativeExe(module, typedCilPbPath, outputExe, workDir, skipToolchain: true);

            // Assertions: c source and meta & runtime stub and placeholder exe were written
            var cPath = Path.Combine(workDir, "module.c");
            var metaPath = Path.Combine(workDir, "rane_meta.json");
            var rtPath = Path.Combine(workDir, "rane_rt_stubs.c");

            Assert.True(File.Exists(cPath), "Expected module.c emitted when skipping toolchain.");
            Assert.True(File.Exists(metaPath), "Expected rane_meta.json emitted when skipping toolchain.");
            Assert.True(File.Exists(rtPath), "Expected runtime stub emitted when skipping toolchain.");
            Assert.True(File.Exists(outputExe), "Expected placeholder output exe file when skipping toolchain.");

            // quick content checks
            var cText = File.ReadAllText(cPath);
            Assert.Contains("typedef struct Packet", cText, StringComparison.OrdinalIgnoreCase);
            Assert.Contains("typedef struct Payload", cText, StringComparison.OrdinalIgnoreCase);

            // cleanup
            try { Directory.Delete(workDir, recursive: true); } catch { }
        }
    }
}
