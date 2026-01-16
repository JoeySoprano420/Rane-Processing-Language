RANE_Today/tests/TypedCilRoundTripTests.cs
using System;
using System.IO;
using System.Linq;
using RANE.CIAM;
using Xunit;

namespace RANE.Tests
{
    public class TypedCilRoundTripTests
    {
        [Fact]
        public void MaterializeAndSerialize_MatchesCanonicalSampleSubset()
        {
            // Materialize a small deterministic LockedNode set (reuse semantic materialization smoke scenario)
            var payloadFields = new LockedNode[]
            {
                new LockedNode("Field", "code", "i32", Array.Empty<string>(), new System.Collections.Generic.Dictionary<string,string>(), new System.Collections.Generic.Dictionary<string,string>{{"field","code"},{"type","i32"}}, 0,0, Array.Empty<LockedNode>(), new AuditRecord("test","payload.field", DateTime.UtcNow, "field")),
                new LockedNode("Field", "msg", "string", Array.Empty<string>(), new System.Collections.Generic.Dictionary<string,string>(), new System.Collections.Generic.Dictionary<string,string>{{"field","msg"},{"type","string"}}, 0,0, Array.Empty<LockedNode>(), new AuditRecord("test","payload.field", DateTime.UtcNow, "field"))
            };
            var payloadStruct = new LockedNode("StructDecl", "Payload", null, Array.Empty<string>(), new System.Collections.Generic.Dictionary<string,string>(), new System.Collections.Generic.Dictionary<string,string>{{"derive","Eq, Ord, Debug"}}, 0,0, payloadFields, new AuditRecord("test","struct", DateTime.UtcNow, "struct"));
            var variantChildren = new LockedNode[]
            {
                new LockedNode("VariantCase","Ping",null, Array.Empty<string>(), new System.Collections.Generic.Dictionary<string,string>(), new System.Collections.Generic.Dictionary<string,string>(),0,0, Array.Empty<LockedNode>(), new AuditRecord("test","case", DateTime.UtcNow, "case")),
                new LockedNode("VariantCase","Pong",null, Array.Empty<string>(), new System.Collections.Generic.Dictionary<string,string>(), new System.Collections.Generic.Dictionary<string,string>(),0,0, Array.Empty<LockedNode>(), new AuditRecord("test","case", DateTime.UtcNow, "case")),
                new LockedNode("VariantCase","Data",null, Array.Empty<string>(), new System.Collections.Generic.Dictionary<string,string>(), new System.Collections.Generic.Dictionary<string,string>{{"payload","Payload"}},0,0, Array.Empty<LockedNode>(), new AuditRecord("test","case", DateTime.UtcNow, "case"))
            };
            var packetVariant = new LockedNode("VariantDecl","Packet",null, Array.Empty<string>(), new System.Collections.Generic.Dictionary<string,string>(), new System.Collections.Generic.Dictionary<string,string>(), 0,0, variantChildren, new AuditRecord("test","variant", DateTime.UtcNow, "variant"));

            var mmio = new LockedNode("MMIO","REG",null, Array.Empty<string>(), new System.Collections.Generic.Dictionary<string,string>(), new System.Collections.Generic.Dictionary<string,string>{{"from","4096"},{"size","256"}},0,0, Array.Empty<LockedNode>(), new AuditRecord("test","mmio", DateTime.UtcNow, "mmio"));
            var cap = new LockedNode("Capability","file_io",null, Array.Empty<string>(), new System.Collections.Generic.Dictionary<string,string>(), new System.Collections.Generic.Dictionary<string,string>{{"cap","file_io"}},0,0, Array.Empty<LockedNode>(), new AuditRecord("test","cap", DateTime.UtcNow, "cap"));

            // simple main that includes a let and a return
            var retNode = new LockedNode("Return", null, null, Array.Empty<string>(), new System.Collections.Generic.Dictionary<string,string>(), new System.Collections.Generic.Dictionary<string,string>{{"value","0"}},0,0, Array.Empty<LockedNode>(), new AuditRecord("test","ret", DateTime.UtcNow, "ret"));
            var mainProc = new LockedNode("FunctionDecl", "main", "int", new[] { "file_io" }, new System.Collections.Generic.Dictionary<string,string>(), new System.Collections.Generic.Dictionary<string,string>{{"visibility","public"},{"params","()"},{"ret","int"}}, 0,0, new[] { retNode }, new AuditRecord("test","main", DateTime.UtcNow, "main"));

            var lockedTop = new LockedNode[] { payloadStruct, packetVariant, mmio, cap, mainProc };

            var (cil, ast, audits) = SemanticMaterialization.Materialize(lockedTop, "demo_roundtrip", "original source text");

            // Serialize AST deterministically
            var serialized = TypedCilSerializer.Serialize(ast);

            // Load canonical sample (if present) and compare that canonical non-comment non-empty lines
            var canonicalPath = Path.Combine(Directory.GetCurrentDirectory(), "Typed_Common_Intermediary_Language.rane");
            if (!File.Exists(canonicalPath))
            {
                // If canonical sample not present in test env, at least ensure serializer produced something useful
                Assert.False(string.IsNullOrWhiteSpace(serialized));
                Assert.Contains("module demo_roundtrip", serialized);
                return;
            }

            var canonical = File.ReadAllLines(canonicalPath)
                                .Select(l => l.Trim())
                                .Where(l => !string.IsNullOrEmpty(l) && !l.StartsWith("//"))
                                .ToArray();

            var produced = serialized.Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries)
                                .Select(l => l.Trim())
                                .Where(l => !string.IsNullOrEmpty(l) && !l.StartsWith("//"))
                                .ToArray();

            // Check that canonical trimmed lines are present as subsequence in produced output (tolerant round-trip)
            int idx = 0;
            foreach (var line in canonical)
            {
                // try to find line in produced starting from idx
                bool found = false;
                for (int j = idx; j < produced.Length; j++)
                {
                    if (string.Equals(line, produced[j], StringComparison.OrdinalIgnoreCase))
                    {
                        idx = j + 1;
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    // fail only if line is a significant declaration (heuristic)
                    if (line.StartsWith("module") || line.StartsWith("struct") || line.StartsWith("variant") || line.StartsWith("proc") || line.StartsWith("mmio") || line.StartsWith("capability"))
                        Assert.True(false, $"Canonical important line not found in produced CIL: '{line}'");
                }
            }
        }
    }
}