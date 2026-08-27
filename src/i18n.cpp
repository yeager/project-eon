#include "i18n.hpp"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace eon {
namespace {

std::string unquote_po(std::string_view text) {
    const auto first = text.find('"');
    const auto last = text.rfind('"');
    if (first == std::string_view::npos || first == last) return {};
    std::string value;
    for (std::size_t index = first + 1; index < last; ++index) {
        if (text[index] != '\\' || index + 1 == last) {
            value.push_back(text[index]);
            continue;
        }
        switch (text[++index]) {
        case 'n': value.push_back('\n'); break;
        case 'r': value.push_back('\r'); break;
        case 't': value.push_back('\t'); break;
        case '\\': value.push_back('\\'); break;
        case '"': value.push_back('"'); break;
        default: value.push_back(text[index]); break;
        }
    }
    return value;
}

enum class Field { none, id, translation };

void store_entry(std::unordered_map<std::string, std::string>& messages,
    const std::string& id, const std::string& translation, bool fuzzy) {
    if (!id.empty() && !translation.empty() && !fuzzy) messages.insert_or_assign(id, translation);
}

} // namespace

std::string normalize_language(std::string_view language) {
    const auto end = language.find_first_of(".@");
    language = language.substr(0, end);
    const auto separator = language.find_first_of("_-");
    language = language.substr(0, separator);
    std::string normalized;
    for (const unsigned char character : language) {
        if (!std::isalpha(character)) return {};
        normalized.push_back(static_cast<char>(std::tolower(character)));
    }
    return normalized;
}

std::string language_from_environment() {
    for (const char* variable : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        if (const char* value = std::getenv(variable); value && *value) {
            const auto language = normalize_language(value);
            if (!language.empty() && language != "c" && language != "posix") return language;
        }
    }
    return "en";
}

Translator Translator::from_po_file(const std::filesystem::path& path) {
    Translator translator;
    std::ifstream file(path, std::ios::binary);
    if (!file) return translator;

    std::string id;
    std::string translation;
    Field field = Field::none;
    bool fuzzy = false;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) {
            store_entry(translator.messages_, id, translation, fuzzy);
            id.clear();
            translation.clear();
            field = Field::none;
            fuzzy = false;
        } else if (line.starts_with("#, fuzzy")) {
            fuzzy = true;
        } else if (line.starts_with("msgid ")) {
            store_entry(translator.messages_, id, translation, fuzzy);
            id = unquote_po(line);
            translation.clear();
            field = Field::id;
            fuzzy = false;
        } else if (line.starts_with("msgstr ")) {
            translation = unquote_po(line);
            field = Field::translation;
        } else if (line.starts_with("msgstr[")) {
            // Singular UI labels do not need plural selection. Preserve the
            // first translation so a future caller still gets useful text.
            if (line.starts_with("msgstr[0]")) {
                translation = unquote_po(line);
                field = Field::translation;
            } else {
                field = Field::none;
            }
        } else if (line.starts_with('"')) {
            if (field == Field::id) id += unquote_po(line);
            else if (field == Field::translation) translation += unquote_po(line);
        }
    }
    store_entry(translator.messages_, id, translation, fuzzy);
    return translator;
}

Translator Translator::from_language(
    const std::string_view language, const std::filesystem::path& executable_path) {
    const auto normalized = normalize_language(language);
    if (normalized.empty() || normalized == "en") return {};

    std::vector<std::filesystem::path> roots;
#ifdef EON_LOCALE_DIR
    roots.emplace_back(EON_LOCALE_DIR);
#endif
    if (!executable_path.empty()) {
        const auto executable_directory = executable_path.parent_path();
        roots.push_back(executable_directory / "po");
        roots.push_back(executable_directory / ".." / "share" / "project-eon" / "po");
        roots.push_back(executable_directory / ".." / "Resources" / "po");
    }
    // The catalog set intentionally contains region-specific Portuguese and
    // Chinese translations. The launcher accepts generic POSIX/BCP-47 input
    // too, so resolve those language families to their supplied catalog.
    const auto catalog_name = normalized == "pt" ? "pt_BR"
        : normalized == "zh" ? "zh_CN" : normalized;
    for (const auto& root : roots) {
        auto translator = from_po_file(root / (catalog_name + ".po"));
        if (!translator.empty()) return translator;
    }
    return {};
}

std::string_view Translator::translate(const std::string_view message) const {
    const auto found = messages_.find(std::string(message));
    return found == messages_.end() ? message : std::string_view(found->second);
}

} // namespace eon
