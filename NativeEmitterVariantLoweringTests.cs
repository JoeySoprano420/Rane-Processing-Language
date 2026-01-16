csharp RANE_Today/tests/NativeEmitterVariantLoweringTests.cs
using System;
using System.IO;
using System.Linq;
using RANE.CIAM;
using Xunit;

namespace RANE.Tests
{
    public class NativeEmitterVariantLoweringTests
    {
        [Fact]
        public void EmitCFromTypedCil_Includes_Variant_Typedefs_And_Helpers()
        {
            // Build a TypedCil module with Payload struct and Packet variant (Ping | Pong | Data(Payload))
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

            // Simple proc that would match on Packet (body can be empty for this test)
            var proc = new TypedCilProc("variant_test", "public", "int", "()", Array.Empty<string>(), Array.Empty<TypedCilStmt>());

            var module = new TypedCilModule("variant_lowering_test",
                new[] { new TypedCilImport("rane_rt_print") },
                Array.Empty<TypedCilType>(),
                new[] { payloadStruct },
                Array.Empty<TypedCilEnum>(),
                new[] { packetVariant },
                Array.Empty<TypedCilMMIO>(),
                Array.Empty<TypedCilCapability>(),
                new[] { proc },
                Array.Empty<TypedCilNode>());

            var c = NativeEmitter.EmitCSource(module);

            // Assert typedef for Packet and helper names exist
            Assert.Contains("typedef struct Packet", c, StringComparison.OrdinalIgnoreCase);
            Assert.Contains("Packet_is_Data", c, StringComparison.OrdinalIgnoreCase);
            Assert.Contains("Packet_payload_ptr_Data", c, StringComparison.OrdinalIgnoreCase);
            // Ensure the Payload struct typedef emitted
            Assert.Contains("typedef struct Payload", c, StringComparison.OrdinalIgnoreCase);
        }
    }
}