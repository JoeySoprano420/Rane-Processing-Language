RANE_Today/tools/DiagChecker/Program.cs
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

internal static class Program
{
    // Scans repository source files (pipeline and .rane/.txt) through Tokenizer->Shaper->Parser->Resolver,
    // collects diagnostics and fails with non-zero exit if any Error severity diagnostics are found.
    private static int Main(string[] args)
    {
        try
        {
            var root = Directory.GetCurrentDirectory();
            // candidate extensions to check
            var exts = new[] { ".rane", ".type", ".txt" };

            var files = Directory.EnumerateFiles(root, "*.*", SearchOption.AllDirectories)
                .Where(f => exts.Contains(Path.GetExtension(f), StringComparer.OrdinalIgnoreCase))
                .ToList();

            var foundErrors = new List<string>();

            foreach (var f in files)
            {
                string src;
                try
                {
                    src = File.ReadAllText(f);
                }
                catch
                {
                    // unable to read file — skip
                    continue;
                }

                var tokens = Tokenizer.Tokenize(src);
                var (shaped, _) = ContextualShaper.Shape(tokens, src);
                var parsed = Parser.Parse(shaped);
                var resolved = Resolver.Resolve(parsed, src);

                // gather syntax diagnostics attached by parser
                foreach (var n in parsed)
                {
                    if (n.Annotations != null && n.Annotations.TryGetValue("syntax.diagnostics", out var sdiag))
                    {
                        if (sdiag.Contains("|Error|")) foundErrors.Add($"{f}:syntax => {sdiag}");
                    }
                }

                // gather resolver diagnostics
                foreach (var n in Flatten(resolved))
                {
                    if (n.Annotations != null && n.Annotations.TryGetValue("resolved.diagnostics", out var rdiag))
                    {
                        if (rdiag.Contains("|Error|")) foundErrors.Add($"{f}:resolved => {rdiag}");
                    }
                }
            }

            // Additionally, detect any module diagnostics JSON files produced by SemanticMaterialization,
            // parse them and fail CI if any diagnostic entry contains an Error severity marker "|Error|".
            var diagFiles = Directory.EnumerateFiles(root, "*.diagnostics.json", SearchOption.AllDirectories).ToList();
            foreach (var df in diagFiles)
            {
                try
                {
                    var text = File.ReadAllText(df);
                    using var doc = JsonDocument.Parse(text);
                    if (doc.RootElement.TryGetProperty("diagnostics", out var diags))
                    {
                        foreach (var item in diags.EnumerateArray())
                        {
                            if (item.ValueKind != JsonValueKind.Object) continue;
                            if (item.TryGetProperty("diagnostics", out var dstr) && dstr.ValueKind == JsonValueKind.String)
                            {
                                var s = dstr.GetString() ?? "";
                                if (s.Contains("|Error|")) foundErrors.Add($"{df}:module => {s}");
                            }
                            // Also consider any summary or audit entries that may embed severity tokens
                            if (item.TryGetProperty("summary", out var sum) && sum.ValueKind == JsonValueKind.String)
                            {
                                var ssum = sum.GetString() ?? "";
                                if (ssum.Contains("|Error|")) foundErrors.Add($"{df}:audit => {ssum}");
                            }
                        }
                    }
                }
                catch
                {
                    // best-effort; skip parse errors
                }
            }

            if (foundErrors.Count > 0)
            {
                Console.Error.WriteLine("Error diagnostics detected by DiagChecker:");
                foreach (var e in foundErrors) Console.Error.WriteLine(e);
                return 2;
            }

            Console.WriteLine("DiagChecker: no Error diagnostics found.");
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine("DiagChecker failed: " + ex.Message);
            return 3;
        }
    }

    private static IEnumerable<AstNode> Flatten(AstNode n)
    {
        if (n == null) yield break;
        yield return n;
        foreach (var c in n.Children ?? Array.Empty<AstNode>())
        {
            foreach (var cc in Flatten(c)) yield return cc;
        }
    }
}
