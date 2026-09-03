#include "data/millennium_dos_lib.hpp"

#include "data/sha256.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>

namespace eon {
namespace {

constexpr std::size_t header_size = 6;
constexpr std::size_t directory_entry_size = 12;

std::uint16_t little16(const std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("Truncated Millennium DOS LIB field");
    }
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

std::uint32_t banked_offset(
    const std::span<const std::uint8_t> bytes, std::size_t offset, std::size_t bank_offset) {
    return static_cast<std::uint32_t>(little16(bytes, offset))
        | (static_cast<std::uint32_t>(bytes[bank_offset]) << 16U);
}

std::string decode_name(const std::span<const std::uint8_t> bytes, std::size_t offset) {
    std::string name;
    bool padding = false;
    for (std::size_t index = 0; index < 8; ++index) {
        const auto value = bytes[offset + index];
        if (value == 0) {
            padding = true;
            continue;
        }
        if (padding || value < 0x21 || value > 0x7e) {
            throw std::runtime_error("Invalid Millennium DOS LIB entry name");
        }
        name.push_back(static_cast<char>(value));
    }
    if (name.empty()) throw std::runtime_error("Empty Millennium DOS LIB entry name");
    return name;
}

std::string upper(std::string_view text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char value) {
        return static_cast<char>(std::toupper(value));
    });
    return result;
}

} // namespace

MillenniumDosLib::MillenniumDosLib(const std::span<const std::uint8_t> bytes)
    : bytes_(bytes) {
    if (bytes_.size() < header_size) throw std::runtime_error("Millennium DOS LIB is too short");
    source_sha256_ = to_hex(sha256(bytes_));
    const auto count = little16(bytes_, 0);
    if (count == 0) throw std::runtime_error("Empty Millennium DOS LIB directory");
    if (bytes_[5] != 0) throw std::runtime_error("Invalid Millennium DOS LIB header flags");
    directory_offset_ = banked_offset(bytes_, 2, 4);

    const auto directory_size = static_cast<std::size_t>(count) * directory_entry_size;
    if (directory_offset_ < header_size || directory_offset_ > bytes_.size()
        || directory_size > bytes_.size() - directory_offset_
        || directory_offset_ + directory_size != bytes_.size()) {
        throw std::runtime_error("Invalid Millennium DOS LIB directory range");
    }

    entries_.reserve(count);
    std::set<std::string> names;
    for (std::size_t index = 0; index < count; ++index) {
        const auto directory_entry = static_cast<std::size_t>(directory_offset_)
            + index * directory_entry_size;
        if (bytes_[directory_entry + 3] != 0) {
            throw std::runtime_error("Invalid Millennium DOS LIB entry flags");
        }
        const auto offset = banked_offset(bytes_, directory_entry, directory_entry + 2);
        const auto name = decode_name(bytes_, directory_entry + 4);
        if (!names.insert(upper(name)).second) {
            throw std::runtime_error("Duplicate Millennium DOS LIB entry name");
        }
        if (offset < header_size || offset >= directory_offset_
            || (!entries_.empty() && offset <= entries_.back().offset)) {
            throw std::runtime_error("Invalid Millennium DOS LIB entry offset");
        }
        if (!entries_.empty()) entries_.back().size = offset - entries_.back().offset;
        entries_.push_back({name, offset, 0});
    }
    entries_.back().size = directory_offset_ - entries_.back().offset;
}

const MillenniumDosLibEntry* MillenniumDosLib::find(std::string_view name) const {
    const auto wanted = upper(name);
    const auto found = std::find_if(entries_.begin(), entries_.end(), [&wanted](const auto& entry) {
        return upper(entry.name) == wanted;
    });
    return found == entries_.end() ? nullptr : &*found;
}

std::span<const std::uint8_t> MillenniumDosLib::read(const MillenniumDosLibEntry& entry) const {
    const auto found = std::find_if(entries_.begin(), entries_.end(), [&entry](const auto& candidate) {
        return candidate.name == entry.name && candidate.offset == entry.offset
            && candidate.size == entry.size;
    });
    if (found == entries_.end() || entry.offset > bytes_.size()
        || entry.size > bytes_.size() - entry.offset) {
        throw std::runtime_error("Millennium DOS LIB entry does not belong to library");
    }
    return bytes_.subspan(entry.offset, entry.size);
}

} // namespace eon
