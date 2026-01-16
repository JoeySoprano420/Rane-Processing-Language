using System;
using System.IO;

csharp RANE_Today/tests/FramePlannerTests.cs
using System;
using System.IO;
using System.Linq;
using RANE.CIAM;
using Xunit;

namespace RANE.Tests
{
    public class FramePlannerTests
    {
        [Fact]
        public void FramePlanner_Produces_Frame_With_PrologueTemplates()
        {
            // Create a minimal module with one proc that has a couple of lets and a return
            var body = new TypedCilStmt[]
            {
                new TypedCilLet("x", "i64", new TypedCilLiteral("42")),
                new TypedCilLet("y", "i64", new TypedCilBinary("+", new TypedCilIdentifier("x"), new TypedCilLiteral("1"))),
                new TypedCilReturn(new TypedCilIdentifier("y"))
            };

            var proc = new TypedCilProc("fp_test", "public", "i64", "()", Array.Empty<string>(), body, Annotations: null);

            var module = new TypedCilModule(
                "frameplanner_test",
                Array.Empty<TypedCilImport>(),
                Array.Empty<TypedCilType>(),
                Array.Empty<TypedCilStruct>(),
                Array.Empty<TypedCilEnum>(),
                Array.Empty<TypedCilVariant>(),
                Array.Empty<TypedCilMMIO>(),
                Array.Empty<TypedCilCapability>(),
                new[] { proc },
                Array.Empty<TypedCilNode>());

            // Run ABITruth which will also invoke FramePlanner via the hook
            var abi = ABITruth.Analyze(module);

            // Also call FramePlanner directly and inspect results
            var fm = FramePlanner.BuildAndWrite(module, abi);

            var fn = fm.Functions.FirstOrDefault(f => f.Name == "fp_test");
            Assert.NotNull(fn);
            Assert.True(fn.FrameSize >= 0);
            Assert.NotNull(fn.PrologueTemplatePE);
            Assert.NotNull(fn.EpilogueTemplatePE);
            Assert.True(fn.PrologueTemplatePE.Length > 0);
            Assert.True(fn.EpilogueTemplatePE.Length > 0);

            // Validate that the stored prologue/epilogue match the helper-generated concrete bytes
            var expectedPro = FramePlanner.MakePrologueBytesPE(fn.FrameSize);
            var expectedEpi = FramePlanner.MakeEpilogueBytesPE(fn.FrameSize);
            Assert.Equal(expectedPro, fn.PrologueTemplatePE);
            Assert.Equal(expectedEpi, fn.EpilogueTemplatePE);

            // Ensure per-function JSON artifact was written
            Assert.True(File.Exists("frameplanner_test.frames.json") || File.Exists(Path.Combine(Directory.GetCurrentDirectory(), "frameplanner_test.frames.json")));
        }
    }
}

csharp RANE_Today/tests/FramePlannerTests.cs
using System;
using System.IO;
using System.Linq;
using RANE.CIAM;
using Xunit;

namespace RANE.Tests
{
    public class FramePlannerTests
    {
        [Fact]
        public void FramePlanner_Produces_Frame_With_PrologueTemplates()
        {
            // Create a minimal module with one proc that has a couple of lets and a return
            var body = new TypedCilStmt[]
            {
                new TypedCilLet("x", "i64", new TypedCilLiteral("42")),
                new TypedCilLet("y", "i64", new TypedCilBinary("+", new TypedCilIdentifier("x"), new TypedCilLiteral("1"))),
                new TypedCilReturn(new TypedCilIdentifier("y"))
            };

            var proc = new TypedCilProc("fp_test", "public", "i64", "()", Array.Empty<string>(), body, Annotations: null);

            var module = new TypedCilModule(
                "frameplanner_test",
                Array.Empty<TypedCilImport>(),
                Array.Empty<TypedCilType>(),
                Array.Empty<TypedCilStruct>(),
                Array.Empty<TypedCilEnum>(),
                Array.Empty<TypedCilVariant>(),
                Array.Empty<TypedCilMMIO>(),
                Array.Empty<TypedCilCapability>(),
                new[] { proc },
                Array.Empty<TypedCilNode>());

            // Run ABITruth which will also invoke FramePlanner via the hook
            var abi = ABITruth.Analyze(module);

            // Also call FramePlanner directly and inspect results
            var fm = FramePlanner.BuildAndWrite(module, abi);

            var fn = fm.Functions.FirstOrDefault(f => f.Name == "fp_test");
            Assert.NotNull(fn);
            Assert.True(fn.FrameSize >= 0);
            Assert.NotNull(fn.PrologueTemplatePE);
            Assert.NotNull(fn.EpilogueTemplatePE);
            Assert.True(fn.PrologueTemplatePE.Length > 0);
            Assert.True(fn.EpilogueTemplatePE.Length > 0);

            // Validate that the stored prologue/epilogue match the helper-generated concrete bytes
            var expectedPro = FramePlanner.MakePrologueBytesPE(fn.FrameSize);
            var expectedEpi = FramePlanner.MakeEpilogueBytesPE(fn.FrameSize);
            Assert.Equal(expectedPro, fn.PrologueTemplatePE);
            Assert.Equal(expectedEpi, fn.EpilogueTemplatePE);

            // Ensure per-function JSON artifact was written
            Assert.True(File.Exists("frameplanner_test.frames.json") || File.Exists(Path.Combine(Directory.GetCurrentDirectory(), "frameplanner_test.frames.json")));
        }

        [Fact]
        public void PatchPrologueTemplatePE_ConvertsBetweenImm32AndImm8()
        {
            // Start with a full-imm32 prologue (large frame)
            long largeFrame = 2048;
            var fullTemplate = FramePlanner.MakePrologueBytesPE(largeFrame);

            // Patch it down to a small frame that fits in signed int8 (<=127)
            long smallFrame = 64;
            var patchedToSmall = FramePlanner.PatchPrologueTemplatePE(fullTemplate, smallFrame);
            var expectedSmall = FramePlanner.MakePrologueBytesPE(smallFrame);
            Assert.Equal(expectedSmall, patchedToSmall);

            // Now start with a short-imm8 template and grow it to a large frame (imm32)
            var shortTemplate = FramePlanner.MakePrologueBytesPE(smallFrame);
            var patchedToLarge = FramePlanner.PatchPrologueTemplatePE(shortTemplate, largeFrame);
            var expectedLarge = FramePlanner.MakePrologueBytesPE(largeFrame);
            Assert.Equal(expectedLarge, patchedToLarge);

            // Also verify that patching to zero removes the sub/add instruction
            var removed = FramePlanner.PatchPrologueTemplatePE(fullTemplate, 0);
            var expectedNoSub = FramePlanner.MakePrologueBytesPE(0);
            Assert.Equal(expectedNoSub, removed);
        }
    }
}

