#include "data/modern_asset_pack.hpp"

#include "data/release_manifest.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <iterator>
#include <map>
#include <limits>
#include <set>
#include <string_view>
#include <system_error>

#include <zlib.h>

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

bool non_symlink_relative_path(const std::filesystem::path& pack_root,
                               const std::filesystem::path& relative,
                               std::string& error) {
    std::error_code filesystem_error;
    auto current = pack_root;
    const auto root_status = std::filesystem::symlink_status(current, filesystem_error);
    if (filesystem_error || std::filesystem::is_symlink(root_status)
        || !std::filesystem::is_directory(root_status)) {
        error = "Modern asset-pack directory must be a non-symlink directory: " + current.string();
        return false;
    }
    for (const auto& component : relative) {
        current /= component;
        const auto status = std::filesystem::symlink_status(current, filesystem_error);
        if (filesystem_error || std::filesystem::is_symlink(status)) {
            error = "Modern asset-pack asset path contains a symlink: " + current.string();
            return false;
        }
    }
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

std::uint32_t big32(const std::vector<std::uint8_t>& bytes, const std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4U) throw std::runtime_error("Truncated Modern PNG field");
    return static_cast<std::uint32_t>(bytes[offset]) << 24U
        | static_cast<std::uint32_t>(bytes[offset + 1]) << 16U
        | static_cast<std::uint32_t>(bytes[offset + 2]) << 8U | bytes[offset + 3];
}

struct MillenniumTitlePngTarget {
    std::string_view id;
    std::uint32_t width;
    std::uint32_t height;
};

// Ordered largest first. Selection is deliberately a finite renderer map,
// not a filename convention that grants arbitrary pack assets display access.
constexpr std::array<MillenniumTitlePngTarget, 2> millennium_title_png_targets{{
    {"millennium.dos.title.png-1280x800", 1280U, 800U},
    {"millennium.dos.title.png-640x400", 640U, 400U},
}};

bool png_chunk_type(const std::vector<std::uint8_t>& bytes, const std::size_t offset,
                    const char a, const char b, const char c, const char d) {
    return bytes[offset] == static_cast<std::uint8_t>(a)
        && bytes[offset + 1] == static_cast<std::uint8_t>(b)
        && bytes[offset + 2] == static_cast<std::uint8_t>(c)
        && bytes[offset + 3] == static_cast<std::uint8_t>(d);
}

bool png_critical_chunk(const std::vector<std::uint8_t>& bytes, const std::size_t offset) {
    return bytes[offset] >= 'A' && bytes[offset] <= 'Z';
}

// SDL_image is deliberately not the first parser of an externally supplied
// Modern title.  Verify every PNG chunk checksum before handing the bounded
// bytes to its decoder.  Feed zlib in portable chunks because crc32 accepts a
// uInt length even though our file-size boundary is expressed as size_t.
bool png_chunk_crc_matches(const std::vector<std::uint8_t>& bytes,
                           const std::size_t type, const std::size_t data,
                           const std::size_t length) {
    uLong crc = crc32(0L, Z_NULL, 0);
    // The type is always four bytes.  Keep it as a separate feed so the
    // data segment has a straightforward bounds-proven offset.
    crc = crc32(crc, bytes.data() + type, 4U);
    std::size_t remaining = length;
    std::size_t offset = data;
    while (remaining != 0U) {
        const auto count = std::min(remaining,
            static_cast<std::size_t>(std::numeric_limits<uInt>::max()));
        crc = crc32(crc, bytes.data() + offset, static_cast<uInt>(count));
        offset += count;
        remaining -= count;
    }
    return static_cast<std::uint32_t>(crc) == big32(bytes, data + length);
}

bool millennium_title_png_layout(const std::vector<std::uint8_t>& bytes,
                                 const MillenniumTitlePngTarget& target) {
    constexpr std::array<std::uint8_t, 8> signature{{0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a}};
    if (bytes.size() < 57U || !std::equal(signature.begin(), signature.end(), bytes.begin())) return false;
    std::size_t offset = 8;
    bool first_chunk = true;
    bool saw_idat = false;
    bool closed_idat = false;
    while (offset <= bytes.size() && bytes.size() - offset >= 12U) {
        const auto length = big32(bytes, offset);
        const auto type = offset + 4U;
        const auto data = offset + 8U;
        if (length > bytes.size() - data || bytes.size() - data - length < 4U) return false;
        const auto next = data + static_cast<std::size_t>(length) + 4U;
        if (!png_chunk_crc_matches(bytes, type, data, length)) return false;
        if (first_chunk) {
            if (length != 13U || !png_chunk_type(bytes, type, 'I', 'H', 'D', 'R')
                || big32(bytes, data) != target.width || big32(bytes, data + 4U) != target.height
                || bytes[data + 8U] != 8U || bytes[data + 9U] != 6U
                || bytes[data + 10U] != 0U || bytes[data + 11U] != 0U || bytes[data + 12U] != 0U) {
                return false;
            }
            first_chunk = false;
        } else if (png_chunk_type(bytes, type, 'I', 'D', 'A', 'T')) {
            if (closed_idat) return false; // PNG IDAT chunks must be consecutive.
            saw_idat = true;
        } else if (png_chunk_type(bytes, type, 'I', 'E', 'N', 'D')) {
            // IEND is terminal and cannot be followed by a hidden payload.
            return saw_idat && length == 0U && next == bytes.size();
        } else {
            if (saw_idat) closed_idat = true;
            // Unknown critical chunks can change decode semantics. Future
            // mappings may admit one only with a documented decoder contract.
            if (png_critical_chunk(bytes, type)) return false;
        }
        offset = next;
    }
    return false;
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
        const auto relative_path = std::filesystem::path(relative);
        if (!non_symlink_relative_path(manifest_path.parent_path(), relative_path, error)) {
            return rejected(manifest_path, error);
        }
        asset.path = manifest_path.parent_path() / relative_path;
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

ModernAssetPackPngSurface load_millennium_dos_title_modern_surface(
    const std::filesystem::path& manifest_path, const std::string_view source_release_sha256) {
    constexpr std::uintmax_t maximum_png_size = 8U * 1024U * 1024U;
    const auto validation = validate_modern_asset_pack(manifest_path);
    if (!validation.accepted()) throw std::runtime_error("Modern title pack rejected: " + validation.error);
    const auto& pack = validation.pack;
    if (pack.game != Game::millennium || pack.platform != Platform::dos
        || pack.source_release_sha256 != source_release_sha256) {
        throw std::runtime_error("Modern title pack does not match selected Millennium DOS release");
    }
    const MillenniumTitlePngTarget* target = nullptr;
    const ModernAssetPackAsset* asset = nullptr;
    for (const auto& candidate_target : millennium_title_png_targets) {
        const auto found = std::find_if(pack.assets.begin(), pack.assets.end(), [&candidate_target](const auto& candidate) {
            return candidate.id == candidate_target.id;
        });
        if (found != pack.assets.end()) {
            target = &candidate_target;
            asset = &*found;
            break;
        }
    }
    if (!asset || !target) {
        throw std::runtime_error("Modern title pack has no supported 640x400 or 1280x800 RGBA PNG title asset");
    }
    if (asset->size == 0 || asset->size > maximum_png_size) {
        throw std::runtime_error("Modern title PNG asset is empty or exceeds its 8 MiB renderer limit");
    }
    // Rehash exactly the bytes that will be uploaded as a transient texture.
    // This closes the normal admission-to-render change window without a cache
    // or any write to supplied game media.
    std::ifstream stream(asset->path, std::ios::binary);
    if (!stream) throw std::runtime_error("Unable to read Modern title RGBA asset");
    std::vector<std::uint8_t> png((std::istreambuf_iterator<char>(stream)), {});
    // A byte-count and SHA-256 comparison bind the actual buffer. `eofbit` is
    // not a reliable completeness signal for every istreambuf_iterator
    // implementation, whereas `badbit` records a genuine I/O failure.
    if (stream.bad()) throw std::runtime_error("Unable to read Modern title PNG asset");
    if (png.size() != asset->size) throw std::runtime_error("Modern title PNG asset size changed after validation");
    if (to_hex(sha256(png)) != asset->sha256) {
        throw std::runtime_error("Modern title PNG asset hash changed after validation");
    }
    if (!millennium_title_png_layout(png, *target)) {
        throw std::runtime_error("Modern title asset is not a structurally valid mapped RGBA PNG");
    }
    return {pack.id, pack.provenance, std::string(target->id), target->width, target->height, std::move(png)};
}

} // namespace eon
