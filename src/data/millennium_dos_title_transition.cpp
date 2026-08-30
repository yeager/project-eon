#include "data/millennium_dos_title_transition.hpp"
#include "data/sha256.hpp"

#include <cstddef>
#include <stdexcept>
#include <string_view>

namespace eon {
namespace {
std::string name_for(const std::uint16_t index) {
    constexpr char hex[] = "0123456789ABCDEF";
    if (index == 0 || index > 0xffU) throw std::runtime_error("Invalid Millennium title patch index");
    return {"P" + std::string(1, hex[(index >> 4U) & 0xfU]) + hex[index & 0xfU]};
}
} // namespace

MillenniumDosTitleTransitionSequence parse_millennium_dos_title_transition(
    const MillenniumDosLib& title_library, const MillenniumDosTitleFlow& flow) {
    constexpr std::string_view english_title_library_sha256 =
        "6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678";
    // The transition addresses belong to the exact English title bank
    // established by the hash-locked title-flow profile. Do not let a
    // structurally valid or cross-edition LIB contribute its P01..P25 bytes.
    if (title_library.source_sha256() != english_title_library_sha256) {
        throw std::runtime_error("Unsupported Millennium English DOS title library");
    }
    if (flow.intro_transition_steps == 0 || flow.intro_step_stride == 0
        || flow.intro_transition_steps >= title_library.entries().size()) {
        throw std::runtime_error("Invalid Millennium DOS title transition bounds");
    }
    MillenniumDosTitleTransitionSequence result;
    result.original_step_stride = flow.intro_step_stride;
    result.patches.reserve(flow.intro_transition_steps);
    std::vector<std::uint8_t> source_bank;
    for (std::uint16_t index = 1; index <= flow.intro_transition_steps; ++index) {
        const auto name = name_for(index);
        const auto* entry = title_library.find(name);
        if (!entry) throw std::runtime_error("Missing Millennium DOS title patch " + name);
        const auto record = title_library.read(*entry);
        if (result.patches.empty()) {
            result.source_bank_offset = entry->offset;
        } else if (entry->offset != result.source_bank_offset + result.source_bank_size) {
            throw std::runtime_error("Non-contiguous Millennium DOS title patch bank");
        }
        result.source_bank_size += entry->size;
        source_bank.insert(source_bank.end(), record.begin(), record.end());
        const auto bitmap = decode_millennium_dos_bitmap(record);
        if (bitmap.pixels.size() != flow.intro_step_stride) {
            throw std::runtime_error("Millennium DOS title patch differs from verified stride");
        }
        // Static mode-2 code at $163b skips stream, optional RGB6 DAC, and
        // auxiliary indices before selecting this XLAT range. This retains
        // bytes only; it does not claim a selected runtime display mode.
        constexpr std::size_t header_size = 0x1c;
        std::size_t offset = header_size + bitmap.encoded_span;
        if ((bitmap.flags & 1U) != 0U) offset += 0x300;
        const auto count = static_cast<std::size_t>(bitmap.max_palette_index) + 1U;
        if (offset > record.size() || count > record.size() - offset) {
            throw std::runtime_error("Truncated Millennium DOS title patch auxiliary table");
        }
        offset += count;
        if (offset > record.size() || count > record.size() - offset) {
            throw std::runtime_error("Truncated Millennium DOS title patch XLAT table");
        }
        result.patches.push_back({index, name, entry->offset, entry->size, to_hex(sha256(record)), bitmap,
            {record.begin() + static_cast<std::ptrdiff_t>(offset),
             record.begin() + static_cast<std::ptrdiff_t>(offset + count)}});
    }
    result.source_bank_sha256 = to_hex(sha256(source_bank));
    return result;
}

} // namespace eon
