#include "data/zip_archive.hpp"

#include "data/sha256.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <zlib.h>

namespace eon {
namespace {

constexpr std::uint32_t local_signature = 0x04034b50;
constexpr std::uint32_t central_signature = 0x02014b50;
constexpr std::uint32_t end_signature = 0x06054b50;
constexpr std::uint32_t data_descriptor_signature = 0x08074b50;
constexpr std::uint32_t maximum_entry_size = 256U * 1024U * 1024U;

std::uint16_t little16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) throw std::runtime_error("Truncated ZIP field");
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

std::uint32_t little32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) throw std::runtime_error("Truncated ZIP field");
    return static_cast<std::uint32_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
}

bool ends_with_zip(std::string_view name) {
    if (name.size() < 4) return false;
    const auto suffix = name.substr(name.size() - 4);
    return suffix == ".zip" || suffix == ".ZIP" || suffix == ".Zip";
}

bool is_lower_hex_sha256(std::string_view value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](unsigned char byte) {
        return (byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'f');
    });
}

bool is_safe_entry_name(std::string_view name) {
    // Project Eon never writes archive paths to disk, but names are retained
    // in preservation inventories. Reject ambiguous or host-dependent names
    // at the archive boundary so an inventory cannot be mistaken for a safe
    // extraction plan by a later consumer.
    if (name.empty() || name.front() == '/' || name.find('\\') != std::string_view::npos
        || name.find('\0') != std::string_view::npos) return false;
    std::size_t component_start = 0;
    while (component_start < name.size()) {
        const auto separator = name.find('/', component_start);
        const auto component = name.substr(component_start,
            separator == std::string_view::npos ? name.size() - component_start
                                                : separator - component_start);
        if (component.empty() || component == "." || component == "..") return false;
        if (separator == std::string_view::npos) return true;
        component_start = separator + 1;
    }
    // A trailing slash is the sole permitted empty final component; it marks
    // a directory and is handled by the caller.
    return name.back() == '/';
}

void validate_local_entry(const std::vector<std::uint8_t>& bytes, const ZipEntry& entry,
                          std::size_t central_offset) {
    const auto offset = static_cast<std::size_t>(entry.local_offset);
    if (offset > central_offset || central_offset - offset < 30
        || little32(bytes, offset) != local_signature) {
        throw std::runtime_error("ZIP local entry outside local-data region");
    }
    if (little16(bytes, offset + 6) != entry.flags || little16(bytes, offset + 8) != entry.method) {
        throw std::runtime_error("ZIP local entry disagrees with central directory");
    }
    const auto name_length = little16(bytes, offset + 26);
    const auto extra_length = little16(bytes, offset + 28);
    if (name_length > central_offset - offset - 30U
        || extra_length > central_offset - offset - 30U - name_length) {
        throw std::runtime_error("ZIP local entry metadata outside local-data region");
    }
    const auto local_name = std::string_view(
        reinterpret_cast<const char*>(bytes.data() + offset + 30U), name_length);
    if (local_name != entry.name) {
        throw std::runtime_error("ZIP local filename mismatch: " + entry.name);
    }
    const auto data_offset = offset + 30U + name_length + extra_length;
    if (entry.compressed_size > central_offset - data_offset) {
        throw std::runtime_error("ZIP payload overlaps central directory");
    }
    const auto payload_end = data_offset + entry.compressed_size;
    const bool has_data_descriptor = (entry.flags & 0x0008U) != 0;
    if (!has_data_descriptor) {
        if (little32(bytes, offset + 14) != entry.crc32
            || little32(bytes, offset + 18) != entry.compressed_size
            || little32(bytes, offset + 22) != entry.uncompressed_size) {
            throw std::runtime_error("ZIP local size or CRC mismatch");
        }
        return;
    }
    // Classic ZIP data descriptors are 12 bytes, or 16 when preceded by the
    // optional signature. ZIP64 is rejected above, so both stored sizes are
    // necessarily 32-bit.
    const auto descriptor_size = payload_end <= central_offset - 4U
            && little32(bytes, payload_end) == data_descriptor_signature ? 16U : 12U;
    if (descriptor_size > central_offset - payload_end) {
        throw std::runtime_error("ZIP data descriptor outside local-data region");
    }
    const auto descriptor = payload_end + (descriptor_size == 16U ? 4U : 0U);
    if (little32(bytes, descriptor) != entry.crc32
        || little32(bytes, descriptor + 4) != entry.compressed_size
        || little32(bytes, descriptor + 8) != entry.uncompressed_size) {
        throw std::runtime_error("ZIP data descriptor disagrees with central directory");
    }
}

std::vector<std::uint8_t> read_zip_bytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) throw std::runtime_error("Unable to open ZIP " + path.string());
    const auto length = stream.tellg();
    if (length < 0 || static_cast<std::uint64_t>(length) > maximum_entry_size) {
        throw std::runtime_error("Unsafe ZIP archive size");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    stream.seekg(0);
    if (!bytes.empty()) stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(length));
    if (!stream && !bytes.empty()) throw std::runtime_error("Unable to read ZIP " + path.string());
    return bytes;
}

std::string extension_of(std::string_view name) {
    const auto separator = name.find_last_of("/\\");
    const auto dot = name.find_last_of('.');
    if (dot == std::string_view::npos || (separator != std::string_view::npos && dot < separator)) return {};
    std::string extension(name.substr(dot));
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return extension;
}

AssetKind classify(std::string_view path, std::span<const std::uint8_t> bytes) {
    const auto extension = extension_of(path);
    if (bytes.size() >= 2 && bytes[0] == 'M' && bytes[1] == 'Z') return AssetKind::dos_mz_executable;
    if (extension == ".exe") return AssetKind::dos_flat_executable;
    if (extension == ".com") return AssetKind::dos_com_program;
    if (extension == ".adf") return AssetKind::amiga_adf;
    if (extension == ".st" || extension == ".msa" || extension == ".stx") return AssetKind::atari_st_disk;
    if (extension == ".img" && (bytes.size() == 360U * 1024U || bytes.size() == 720U * 1024U
            || bytes.size() == 1'200U * 1024U || bytes.size() == 1'440U * 1024U)) {
        return AssetKind::dos_floppy_image;
    }
    if (extension == ".voc" || extension == ".wav") return AssetKind::audio;
    if (extension == ".bin" || extension == ".lib" || extension == ".drv") return AssetKind::game_resource;
    return AssetKind::unknown;
}

void recurse_inventory(
    const ZipArchive& archive,
    const std::string& prefix,
    unsigned depth,
    unsigned maximum_nesting,
    std::vector<ArchiveAsset>& output) {
    for (const auto& entry : archive.entries()) {
        if (entry.directory) continue;
        auto bytes = archive.extract(entry);
        const auto virtual_path = prefix.empty() ? entry.name : prefix + "!" + entry.name;
        if (ends_with_zip(entry.name) && depth < maximum_nesting) {
            recurse_inventory(ZipArchive(std::move(bytes)), virtual_path, depth + 1,
                maximum_nesting, output);
        } else {
            output.push_back({virtual_path, bytes.size(), to_hex(sha256(bytes)), classify(virtual_path, bytes)});
        }
    }
}

std::optional<std::vector<std::uint8_t>> recurse_extract(
    const ZipArchive& archive,
    std::string_view expected_sha256,
    unsigned depth,
    unsigned maximum_nesting) {
    for (const auto& entry : archive.entries()) {
        if (entry.directory) continue;
        auto bytes = archive.extract(entry);
        if (ends_with_zip(entry.name) && depth < maximum_nesting) {
            if (auto result = recurse_extract(ZipArchive(std::move(bytes)), expected_sha256,
                    depth + 1, maximum_nesting)) return result;
        } else if (to_hex(sha256(bytes)) == expected_sha256) {
            return bytes;
        }
    }
    return std::nullopt;
}

} // namespace

ZipArchive::ZipArchive(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {
    if (bytes_.size() < 22) throw std::runtime_error("ZIP is shorter than EOCD");
    const auto search_start = bytes_.size() > 65'557 ? bytes_.size() - 65'557 : 0;
    std::size_t end_offset = std::numeric_limits<std::size_t>::max();
    for (std::size_t offset = bytes_.size() - 22;; --offset) {
        // The EOCD is allowed to have a comment, which may itself contain the
        // four-byte EOCD signature.  Only accept a candidate whose declared
        // comment reaches the physical end of this supplied byte stream.
        // Without this check, a signature in a comment can be mistaken for an
        // empty archive and silently hide the real central directory.
        if (little32(bytes_, offset) == end_signature
            && little16(bytes_, offset + 20) == bytes_.size() - offset - 22U) {
            end_offset = offset;
            break;
        }
        if (offset == search_start) break;
    }
    if (end_offset == std::numeric_limits<std::size_t>::max()) throw std::runtime_error("ZIP EOCD not found");
    if (little16(bytes_, end_offset + 4) != 0 || little16(bytes_, end_offset + 6) != 0
        || little16(bytes_, end_offset + 8) != little16(bytes_, end_offset + 10)) {
        throw std::runtime_error("Multi-disk ZIP archives are unsupported");
    }
    const auto entry_count = little16(bytes_, end_offset + 10);
    const auto central_size = little32(bytes_, end_offset + 12);
    auto offset = static_cast<std::size_t>(little32(bytes_, end_offset + 16));
    if (entry_count == 0xffffU || central_size == 0xffffffffU
        || offset == 0xffffffffU) {
        throw std::runtime_error("ZIP64 archives are unsupported");
    }
    if (offset > end_offset || central_size > end_offset - offset) {
        throw std::runtime_error("ZIP central directory outside archive");
    }
    const auto central_end = offset + central_size;
    // ZIP64 and archive-extra-data records are deliberately unsupported.  In
    // the supported classic layout, the central directory terminates at the
    // EOCD, so it cannot overlap an EOCD comment or leave unauthenticated
    // bytes between directory metadata and the end record.
    if (central_end != end_offset) {
        throw std::runtime_error("ZIP central directory does not end at EOCD");
    }
    entries_.reserve(entry_count);
    std::unordered_set<std::string> entry_names;
    for (std::uint16_t index = 0; index < entry_count; ++index) {
        if (offset > bytes_.size() || bytes_.size() - offset < 46
            || little32(bytes_, offset) != central_signature) {
            throw std::runtime_error("Invalid ZIP central entry");
        }
        const auto name_length = little16(bytes_, offset + 28);
        const auto extra_length = little16(bytes_, offset + 30);
        const auto comment_length = little16(bytes_, offset + 32);
        const auto record_size = 46U + name_length + extra_length + comment_length;
        if (record_size > bytes_.size() - offset || name_length == 0 || name_length > 4096) {
            throw std::runtime_error("Unsafe ZIP entry metadata");
        }
        const auto flags = little16(bytes_, offset + 8);
        if ((flags & 0x0001U) != 0) throw std::runtime_error("Encrypted ZIP entries are unsupported");
        const auto name_begin = bytes_.begin() + static_cast<std::ptrdiff_t>(offset + 46);
        std::string name(name_begin, name_begin + name_length);
        if (!is_safe_entry_name(name)) throw std::runtime_error("Unsafe ZIP entry name");
        if (!entry_names.insert(name).second) throw std::runtime_error("Duplicate ZIP entry name");
        ZipEntry entry{
            std::move(name),
            flags,
            little16(bytes_, offset + 10),
            little32(bytes_, offset + 16),
            little32(bytes_, offset + 20),
            little32(bytes_, offset + 24),
            little32(bytes_, offset + 42),
            false,
        };
        entry.directory = !entry.name.empty() && (entry.name.back() == '/' || entry.name.back() == '\\');
        if (entry.compressed_size > maximum_entry_size || entry.uncompressed_size > maximum_entry_size) {
            throw std::runtime_error("ZIP entry exceeds safety limit");
        }
        validate_local_entry(bytes_, entry,
            static_cast<std::size_t>(little32(bytes_, end_offset + 16)));
        entries_.push_back(std::move(entry));
        offset += record_size;
    }
    if (offset != central_end) throw std::runtime_error("ZIP central directory size mismatch");
}

ZipArchive ZipArchive::open(const std::filesystem::path& path) {
    return ZipArchive(read_zip_bytes(path));
}

ZipArchive ZipArchive::open_verified(const std::filesystem::path& path,
                                     std::string_view expected_sha256) {
    if (!is_lower_hex_sha256(expected_sha256)) {
        throw std::runtime_error("Expected outer ZIP SHA-256 must be 64 lower-case hex characters");
    }
    auto bytes = read_zip_bytes(path);
    if (to_hex(sha256(bytes)) != expected_sha256) {
        throw std::runtime_error("Supplied ZIP no longer matches its verified release identity");
    }
    return ZipArchive(std::move(bytes));
}

std::vector<std::uint8_t> ZipArchive::extract(const ZipEntry& entry) const {
    const auto offset = static_cast<std::size_t>(entry.local_offset);
    if (offset > bytes_.size() || bytes_.size() - offset < 30
        || little32(bytes_, offset) != local_signature) {
        throw std::runtime_error("Invalid ZIP local entry");
    }
    if (little16(bytes_, offset + 6) != entry.flags || little16(bytes_, offset + 8) != entry.method) {
        throw std::runtime_error("ZIP local entry disagrees with central directory");
    }
    const auto name_length = little16(bytes_, offset + 26);
    const auto extra_length = little16(bytes_, offset + 28);
    if (name_length > bytes_.size() - offset - 30U
        || extra_length > bytes_.size() - offset - 30U - name_length) {
        throw std::runtime_error("ZIP local entry metadata outside archive");
    }
    const auto local_name = std::string_view(
        reinterpret_cast<const char*>(bytes_.data() + offset + 30U), name_length);
    if (local_name != entry.name) throw std::runtime_error("ZIP local filename mismatch");
    const auto local_crc32 = little32(bytes_, offset + 14);
    const auto local_compressed_size = little32(bytes_, offset + 18);
    const auto local_uncompressed_size = little32(bytes_, offset + 22);
    const bool has_data_descriptor = (entry.flags & 0x0008U) != 0;
    // Bit 3 delegates these values to a trailing data descriptor. Some
    // original archives retain provisional local values, so only the central
    // directory is authoritative in that case; extraction still verifies its
    // central-directory CRC against the decoded payload below.
    if (!has_data_descriptor) {
        if (local_crc32 != entry.crc32 || local_compressed_size != entry.compressed_size
            || local_uncompressed_size != entry.uncompressed_size) {
            throw std::runtime_error("ZIP local size or CRC mismatch");
        }
    }
    const auto data_offset = offset + 30U + name_length + extra_length;
    if (data_offset > bytes_.size() || entry.compressed_size > bytes_.size() - data_offset) {
        throw std::runtime_error("ZIP payload outside archive");
    }
    std::vector<std::uint8_t> output(entry.uncompressed_size);
    if (entry.method == 0) {
        if (entry.compressed_size != entry.uncompressed_size) throw std::runtime_error("Invalid stored ZIP size");
        std::copy_n(bytes_.begin() + static_cast<std::ptrdiff_t>(data_offset), entry.uncompressed_size, output.begin());
    } else if (entry.method == 8) {
        z_stream stream{};
        std::uint8_t empty_sink = 0;
        stream.next_in = const_cast<Bytef*>(bytes_.data() + data_offset);
        stream.avail_in = entry.compressed_size;
        stream.next_out = output.empty() ? &empty_sink : output.data();
        stream.avail_out = output.empty() ? 1U : entry.uncompressed_size;
        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) throw std::runtime_error("Unable to initialise deflate");
        const auto result = inflate(&stream, Z_FINISH);
        const bool valid = result == Z_STREAM_END && stream.total_out == entry.uncompressed_size;
        inflateEnd(&stream);
        if (!valid) throw std::runtime_error("Invalid deflate stream");
    } else {
        throw std::runtime_error("Unsupported ZIP compression method");
    }
    if (::crc32(0, output.data(), static_cast<uInt>(output.size())) != entry.crc32) {
        throw std::runtime_error("ZIP CRC mismatch");
    }
    return output;
}

std::optional<std::vector<std::uint8_t>> ZipArchive::extract_asset_by_sha256(
    std::string_view expected_sha256, const unsigned maximum_nesting) const {
    if (!is_lower_hex_sha256(expected_sha256)) {
        throw std::runtime_error("Expected asset SHA-256 must be 64 lower-case hex characters");
    }
    return recurse_extract(*this, expected_sha256, 0, maximum_nesting);
}

std::vector<ArchiveAsset> inventory_zip(const std::filesystem::path& path, unsigned maximum_nesting) {
    std::vector<ArchiveAsset> assets;
    recurse_inventory(ZipArchive::open(path), path.filename().string(), 0, maximum_nesting, assets);
    return assets;
}

std::optional<std::vector<std::uint8_t>> extract_asset_by_sha256(
    const std::filesystem::path& path,
    std::string_view expected_sha256,
    unsigned maximum_nesting) {
    return ZipArchive::open(path).extract_asset_by_sha256(expected_sha256, maximum_nesting);
}

std::vector<ArchiveAsset> inventory_verified_zip(
    const std::filesystem::path& path,
    std::string_view expected_archive_sha256,
    unsigned maximum_nesting) {
    std::vector<ArchiveAsset> assets;
    recurse_inventory(ZipArchive::open_verified(path, expected_archive_sha256),
        path.filename().string(), 0, maximum_nesting, assets);
    return assets;
}

std::optional<std::vector<std::uint8_t>> extract_verified_asset_by_sha256(
    const std::filesystem::path& path,
    std::string_view expected_archive_sha256,
    std::string_view expected_asset_sha256,
    unsigned maximum_nesting) {
    return ZipArchive::open_verified(path, expected_archive_sha256).extract_asset_by_sha256(
        expected_asset_sha256, maximum_nesting);
}

std::string name(AssetKind kind) {
    switch (kind) {
    case AssetKind::amiga_adf: return "Amiga ADF";
    case AssetKind::atari_st_disk: return "Atari ST disk";
    case AssetKind::dos_floppy_image: return "DOS floppy image";
    case AssetKind::dos_flat_executable: return "DOS flat executable";
    case AssetKind::dos_mz_executable: return "DOS MZ executable";
    case AssetKind::dos_com_program: return "DOS COM program";
    case AssetKind::audio: return "audio";
    case AssetKind::game_resource: return "game resource";
    case AssetKind::unknown: return "unknown";
    }
    return "unknown";
}

AssetKind classify_asset(const std::string_view path, const std::span<const std::uint8_t> bytes) {
    return classify(path, bytes);
}

} // namespace eon
