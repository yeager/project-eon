#include "data/amiga_ofs.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace eon {
namespace {

constexpr std::size_t block_bytes = AmigaAdf::sector_size;
constexpr std::size_t hash_slots = 72;
constexpr std::size_t hash_table = 24;
constexpr std::size_t first_data = 16;
constexpr std::size_t file_size = 324;
constexpr std::size_t name = 432;
constexpr std::size_t hash_chain = 496;
constexpr std::size_t extension = 504;
constexpr std::size_t secondary_type = 508;
constexpr std::uint32_t type_header = 2;
constexpr std::int32_t st_root = 1;
constexpr std::int32_t st_userdir = 2;
constexpr std::int32_t st_file = -3;
constexpr std::uint32_t type_ofs_data = 8;

std::uint32_t be32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) throw std::runtime_error("Truncated AmigaDOS block");
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U)
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U)
        | bytes[offset + 3];
}

std::int32_t be32s(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::int32_t>(be32(bytes, offset));
}

std::string block_name(std::span<const std::uint8_t> bytes) {
    const auto length = bytes[name];
    if (length > 30 || name + 1U + length > bytes.size()) throw std::runtime_error("Invalid AmigaDOS name");
    return std::string(reinterpret_cast<const char*>(bytes.data() + name + 1), length);
}

void append_checked(std::vector<std::uint8_t>& output, std::span<const std::uint8_t> data,
    std::size_t wanted) {
    const auto count = std::min(wanted - output.size(), data.size());
    output.insert(output.end(), data.begin(), data.begin() + static_cast<std::ptrdiff_t>(count));
}

} // namespace

AmigaOfs::AmigaOfs(const AmigaAdf& disk) : disk_(disk), root_block_(disk.root_block()) {
    if (disk.kind() != AmigaDiskKind::dos) throw std::runtime_error("ADF is not an AmigaDOS disk");
    const auto root = block(root_block_);
    if (be32(root, 0) != type_header || be32s(root, secondary_type) != st_root) {
        throw std::runtime_error("ADF root block is not a standard AmigaDOS root directory");
    }
    volume_name_ = block_name(root);
    std::vector<std::uint32_t> ancestry{root_block_};
    scan_directory(root_block_, "", ancestry);
    std::sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) { return a.path < b.path; });
}

std::span<const std::uint8_t, AmigaAdf::sector_size> AmigaOfs::block(std::uint32_t number) const {
    constexpr auto blocks = AmigaAdf::standard_size / AmigaAdf::sector_size;
    if (number >= blocks) throw std::runtime_error("AmigaDOS block reference outside ADF");
    return std::span<const std::uint8_t, AmigaAdf::sector_size>(
        disk_.bytes(static_cast<std::size_t>(number) * block_bytes, block_bytes).data(), block_bytes);
}

void AmigaOfs::scan_directory(std::uint32_t directory_block, const std::string& prefix,
    std::vector<std::uint32_t>& ancestry) {
    const auto directory = block(directory_block);
    if (be32(directory, 0) != type_header) throw std::runtime_error("Invalid AmigaDOS directory block");
    const auto type = be32s(directory, secondary_type);
    if (type != st_root && type != st_userdir) throw std::runtime_error("Non-directory in AmigaDOS directory chain");

    for (std::size_t slot = 0; slot < hash_slots; ++slot) {
        std::uint32_t current = be32(directory, hash_table + slot * 4);
        std::vector<std::uint32_t> chain;
        while (current != 0) {
            if (std::find(chain.begin(), chain.end(), current) != chain.end()) {
                throw std::runtime_error("Cycle in AmigaDOS hash chain");
            }
            chain.push_back(current);
            const auto child = block(current);
            if (be32(child, 0) != type_header) throw std::runtime_error("Invalid AmigaDOS entry block");
            const auto child_type = be32s(child, secondary_type);
            const auto child_name = block_name(child);
            if (child_name.empty()) throw std::runtime_error("Unnamed AmigaDOS entry");
            const auto path = prefix.empty() ? child_name : prefix + "/" + child_name;
            if (child_type == st_file) {
                entries_.push_back({path, current, be32(child, file_size), false});
            } else if (child_type == st_userdir) {
                entries_.push_back({path, current, 0, true});
                if (std::find(ancestry.begin(), ancestry.end(), current) != ancestry.end()) {
                    throw std::runtime_error("Cycle in AmigaDOS directory tree");
                }
                ancestry.push_back(current);
                scan_directory(current, path, ancestry);
                ancestry.pop_back();
            } else {
                throw std::runtime_error("Unsupported AmigaDOS entry type");
            }
            current = be32(child, hash_chain);
        }
    }
}

const AmigaOfs::Entry* AmigaOfs::find(std::string_view path) const {
    const auto it = std::find_if(entries_.begin(), entries_.end(), [path](const Entry& entry) {
        return !entry.directory && entry.path == path;
    });
    return it == entries_.end() ? nullptr : &*it;
}

std::vector<std::uint8_t> AmigaOfs::read_file(std::string_view path) const {
    const auto* entry = find(path);
    if (!entry) throw std::runtime_error("AmigaDOS file not found");
    const auto header = block(entry->header_block);
    std::vector<std::uint8_t> output;
    output.reserve(entry->byte_size);

    // OFS data blocks carry their own next pointer and byte count.  FFS stores
    // raw 512-byte data blocks in the header pointer table, highest index first.
    const auto first = be32(header, first_data);
    if (first != 0 && be32(block(first), 0) == type_ofs_data) {
        std::vector<std::uint32_t> seen;
        std::uint32_t current = first;
        while (current != 0 && output.size() < entry->byte_size) {
            if (std::find(seen.begin(), seen.end(), current) != seen.end()) throw std::runtime_error("Cycle in OFS data chain");
            seen.push_back(current);
            const auto data = block(current);
            if (be32(data, 0) != type_ofs_data) throw std::runtime_error("Invalid OFS data block");
            const auto count = be32(data, 12);
            if (count > block_bytes - 24) throw std::runtime_error("Invalid OFS data length");
            append_checked(output, data.subspan(24, count), entry->byte_size);
            current = be32(data, first_data);
        }
    } else {
        // FFS extends a large file with more file-header blocks. The pointer
        // table in each header is populated from the high end down.
        std::vector<std::uint32_t> headers;
        std::uint32_t current_header = entry->header_block;
        while (current_header != 0 && output.size() < entry->byte_size) {
            if (std::find(headers.begin(), headers.end(), current_header) != headers.end()) {
                throw std::runtime_error("Cycle in FFS file-header extension chain");
            }
            headers.push_back(current_header);
            const auto current = block(current_header);
            if (be32(current, 0) != type_header || be32s(current, secondary_type) != st_file) {
                throw std::runtime_error("Invalid FFS file-header extension");
            }
            for (std::size_t index = hash_slots; index-- > 0 && output.size() < entry->byte_size;) {
                const auto data_block = be32(current, hash_table + index * 4);
                if (data_block == 0) continue;
                append_checked(output, block(data_block), entry->byte_size);
            }
            current_header = be32(current, extension);
        }
    }
    if (output.size() != entry->byte_size) throw std::runtime_error("Truncated AmigaDOS file");
    return output;
}

} // namespace eon
