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
    [[nodiscard]] bool has_translation(std::string_view message) const;
    [[nodiscard]] bool empty() const noexcept { return messages_.empty(); }

private:
    std::unordered_map<std::string, std::string> messages_;
};

// Turns BCP-47/POSIX spellings such as en-GB.UTF-8 into the PO filename stem
// "en_GB", retaining a supplied regional catalog before generic fallback.
[[nodiscard]] std::string normalize_language(std::string_view language);

// Shipped launcher chrome locales, in a stable UI-only order. This list is
// intentionally unrelated to an original release's immutable language code.
[[nodiscard]] const std::vector<std::string_view>& supported_launcher_languages();
// Resolves accepted CLI aliases to one shipped launcher catalog. This is UI
// locale selection only and must never be used for original release identity.
[[nodiscard]] std::string canonical_launcher_language(std::string_view language);
// A stable native-language name for the launcher locale selector. Autonyms
// avoid displaying implementation-oriented catalog stems such as `sv` or
// `zh_CN`, while remaining independent from the selected original release.
[[nodiscard]] std::string_view launcher_language_autonym(std::string_view language);

} // namespace eon
