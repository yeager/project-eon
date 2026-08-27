#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace eon {

// The launcher intentionally consumes portable PO source files directly.
// That keeps translated UI text reviewable in-tree and avoids a system
// gettext runtime dependency in release packages.
class Translator {
public:
    [[nodiscard]] static Translator from_po_file(const std::filesystem::path& path);
    [[nodiscard]] static Translator from_language(
        std::string_view language, const std::filesystem::path& executable_path = {});

    [[nodiscard]] std::string_view translate(std::string_view message) const;
    [[nodiscard]] bool empty() const noexcept { return messages_.empty(); }

private:
    std::unordered_map<std::string, std::string> messages_;
};

// Turns BCP-47/POSIX spellings such as sv_SE.UTF-8 into the PO filename stem
// "sv". An unsupported or empty locale falls back to English source text.
[[nodiscard]] std::string normalize_language(std::string_view language);
[[nodiscard]] std::string language_from_environment();

} // namespace eon
