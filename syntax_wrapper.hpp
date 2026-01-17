#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace rane {

class SyntaxSheet {
public:
  // Return the full embedded syntax document (owned string reference).
  static const std::string& text() noexcept;

  // A canonical list of reserved/keyword tokens (deduplicated subset).
  static const std::vector<std::string>& reserved_keywords() noexcept;

  // Return all lines that contain 'needle' (case-sensitive).
  static std::vector<std::string> find_lines_matching(std::string_view needle);

  // Fast containment check for a standalone keyword in the document.
  // Matches only word boundaries (non-alnum/_ before and after).
  static bool contains_keyword(std::string_view keyword) noexcept;

  // Write the embedded syntax text to disk. Throws std::ios_base::failure on error.
  static void write_to_file(const std::string& path);

private:
  // private ctor to prevent instantiation
  SyntaxSheet() = delete;
};

} // namespace rane
