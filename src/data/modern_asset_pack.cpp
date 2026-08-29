#include "data/modern_asset_pack.hpp"

#include "data/release_manifest.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <map>
#include <set>
#include <string_view>
#include <system_error>

namespace eon {
namespace {

constexpr std::uintmax_t maximum_manifest_size = 1024U * 1024U;
constexpr std::uintmax_t maximum_asset_size = 256U * 1024U * 1024U;
constexpr std::size_t maximum_assets = 4096;
constexpr std::size_t maximum_line_size = 4096;
constexpr std::string_view manifest_name = "pack.eonmodern";
constexpr std::string_view schema = "project-eon.modern-asset-pack/v1";

bool printable(const std::string_view value) {
    return !value.empty() && value.find_first_of("\r\n\t") == std::string_view::npos
        && std::all_of(value.begin(), value.end(), [](const unsigned char character) {
            return character >= 0x20U && character <= 0x7eU;
        });
}

bool lower_sha256(const std::string_view value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](const unsigned char character) {
        return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
    });
}

bool pack_identifier(const std::string_view value) {
    return value.size() <= 96 && printable(value) && std::all_of(value.begin(), value.end(),
        [](const unsigned char character) {
            return (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9')
                || character == '.' || character == '-' || character == '_';
        });
}

bool decimal_size(const std::string_view value, std::uintmax_t& size) {
    if (value.empty()) return false;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), size);
    return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
}

bool regular_file(const std::filesystem::path& path, const std::uintmax_t maximum,
                  std::uintmax_t& size, std::string& error) {
    std::error_code filesystem_error;
    const auto status = std::filesystem::symlink_status(path, filesystem_error);
    if (filesystem_error || std::filesystem::is_symlink(status)
        || !std::filesystem::is_regular_file(status)) {
        error = "Modern asset-pack file must be a non-symlink regular file: " + path.string();
        return false;
    }
    size = std::filesystem::file_size(path, filesystem_error);
    if (filesystem_error || size > maximum) {
        error = "Modern asset-pack file exceeds its size limit: " + path.string();
        return false;
    }
    return true;
}

bool safe_relative_path(const std::string_view value) {
    if (!printable(value) || value.find('\\') != std::string_view::npos) return false;
    const std::filesystem::path path(value);
    if (path.empty() || path.is_absolute() || path.has_root_path() || path.lexically_normal() != path) return false;
    for (const auto& part : path) if (part == ".." || part == ".") return false;
    return true;
}

bool parse_game(const std::string_view value, Game& game) {
    if (value == "millennium") { game = Game::millennium; return true; }
    if (value == "deuteros") { game = Game::deuteros; return true; }
    return false;
}

bool parse_platform(const std::string_view value, Platform& platform) {
    if (value == "dos") { platform = Platform::dos; return true; }
    if (value == "amiga") { platform = Platform::amiga; return true; }
    if (value == "atari-st") { platform = Platform::atari_st; return true; }
    return false;
}

bool known_release(const ModernAssetPack& pack) {
    return std::any_of(release_manifest().begin(), release_manifest().end(), [&pack](const auto& release) {
        return release.sha256 == pack.source_release_sha256 && release.game == pack.game
            && release.platform == pack.platform;
    });
}

ModernAssetPackValidation rejected(const std::filesystem::path& path, std::string error) {
    ModernAssetPackValidation result;
    result.manifest_path = path;
    result.error = std::move(error);
    return result;
}

} // namespace

ModernAssetPackValidation validate_modern_asset_pack(const std::filesystem::path& manifest_path) {
    std::uintmax_t manifest_size = 0;
    std::string error;
    if (manifest_path.filename() != manifest_name
        || !regular_file(manifest_path, maximum_manifest_size, manifest_size, error)) {
        return rejected(manifest_path, error.empty() ? "Modern asset-pack manifest must be named pack.eonmodern" : error);
    }
    std::ifstream stream(manifest_path, std::ios::binary);
    if (!stream) return rejected(manifest_path, "Unable to read Modern asset-pack manifest");
    stream.seekg(-1, std::ios::end);
    char final_character = '\0';
    stream.get(final_character);
    if (final_character != '\n') {
        return rejected(manifest_path, "Modern asset-pack manifest must use LF-terminated records");
    }
    stream.clear();
    stream.seekg(0);

    std::map<std::string, std::string> fields;
    std::vector<ModernAssetPackAsset> assets;
    std::set<std::string> asset_ids;
    std::set<std::filesystem::path> asset_paths;
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty() || line.size() > maximum_line_size || line.back() == '\r') {
            return rejected(manifest_path, "Modern asset-pack manifest has an invalid line");
        }
        const auto tab = line.find('\t');
        if (tab == std::string::npos || tab == 0) {
            return rejected(manifest_path, "Modern asset-pack manifest line is not key<TAB>value");
        }
        const std::string key = line.substr(0, tab);
        const std::string value = line.substr(tab + 1);
        if (!printable(key) || !printable(value)) {
            return rejected(manifest_path, "Modern asset-pack manifest contains non-printable text");
        }
        if (key != "asset") {
            if (!fields.emplace(key, value).second) {
                return rejected(manifest_path, "Modern asset-pack manifest has a duplicate field");
            }
            continue;
        }
        if (assets.size() == maximum_assets) return rejected(manifest_path, "Modern asset pack has too many assets");
        const auto first = value.find(' ');
        const auto second = first == std::string::npos ? std::string::npos : value.find(' ', first + 1);
        const auto third = second == std::string::npos ? std::string::npos : value.find(' ', second + 1);
        if (first == std::string::npos || second == std::string::npos || third == std::string::npos
            || value.find(' ', third + 1) != std::string::npos) {
            return rejected(manifest_path, "Modern asset-pack asset must be id path size sha256");
        }
        ModernAssetPackAsset asset;
        asset.id = value.substr(0, first);
        const auto relative = value.substr(first + 1, second - first - 1);
        const auto size = std::string_view(value).substr(second + 1, third - second - 1);
        asset.sha256 = value.substr(third + 1);
        if (!pack_identifier(asset.id) || !safe_relative_path(relative) || !decimal_size(size, asset.size)
            || asset.size > maximum_asset_size || !lower_sha256(asset.sha256)) {
            return rejected(manifest_path, "Modern asset-pack asset has invalid identity, path, size, or SHA-256");
        }
        asset.path = manifest_path.parent_path() / relative;
        if (!asset_ids.insert(asset.id).second || !asset_paths.insert(asset.path.lexically_normal()).second) {
            return rejected(manifest_path, "Modern asset pack duplicates an asset id or path");
        }
        assets.push_back(std::move(asset));
    }
    if (!stream.eof()) return rejected(manifest_path, "Unable to read Modern asset-pack manifest");
    const std::set<std::string> required{"schema", "id", "version", "license", "provenance", "game", "platform", "source_release_sha256"};
    if (fields.size() != required.size() || !std::all_of(required.begin(), required.end(), [&fields](const auto& key) {
            return fields.contains(key);
        }) || fields.at("schema") != schema || !pack_identifier(fields.at("id"))
        || !printable(fields.at("version")) || !printable(fields.at("license"))
        || (fields.at("provenance") != "independently-created"
            && fields.at("provenance") != "licensed-derivative") || !lower_sha256(fields.at("source_release_sha256"))
        || assets.empty()) {
        return rejected(manifest_path, "Modern asset-pack manifest schema or required fields are invalid");
    }

    ModernAssetPack pack;
    pack.manifest_path = manifest_path;
    pack.id = fields.at("id");
    pack.version = fields.at("version");
    pack.license = fields.at("license");
    pack.provenance = fields.at("provenance");
    pack.source_release_sha256 = fields.at("source_release_sha256");
    pack.assets = std::move(assets);
    if (!parse_game(fields.at("game"), pack.game) || !parse_platform(fields.at("platform"), pack.platform)
        || !known_release(pack)) {
        return rejected(manifest_path, "Modern asset pack is not bound to one recognised release identity");
    }
    for (const auto& asset : pack.assets) {
        std::uintmax_t observed_size = 0;
        if (!regular_file(asset.path, maximum_asset_size, observed_size, error) || observed_size != asset.size) {
            return rejected(manifest_path, error.empty() ? "Modern asset-pack asset size does not match manifest" : error);
        }
        try {
            if (to_hex(sha256_file(asset.path)) != asset.sha256) {
                return rejected(manifest_path, "Modern asset-pack asset SHA-256 does not match manifest");
            }
        } catch (const std::exception&) {
            return rejected(manifest_path, "Unable to hash Modern asset-pack asset");
        }
    }
    ModernAssetPackValidation result;
    result.manifest_path = manifest_path;
    result.pack = std::move(pack);
    return result;
}

std::vector<ModernAssetPackValidation> discover_modern_asset_packs(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> manifests;
    std::error_code error;
    const auto status = std::filesystem::symlink_status(root, error);
    if (error || !std::filesystem::is_directory(status) || std::filesystem::is_symlink(status)) return {};
    for (std::filesystem::directory_iterator iterator(root, std::filesystem::directory_options::skip_permission_denied, error), end;
         !error && iterator != end; iterator.increment(error)) {
        std::error_code entry_error;
        const auto entry_status = iterator->symlink_status(entry_error);
        if (entry_error || std::filesystem::is_symlink(entry_status)
            || !std::filesystem::is_directory(entry_status)) continue;
        const auto candidate = iterator->path() / manifest_name;
        if (std::filesystem::exists(candidate, entry_error) && !entry_error) manifests.push_back(candidate);
    }
    std::sort(manifests.begin(), manifests.end());
    std::vector<ModernAssetPackValidation> results;
    results.reserve(manifests.size());
    for (const auto& manifest : manifests) results.push_back(validate_modern_asset_pack(manifest));
    return results;
}

} // namespace eon
