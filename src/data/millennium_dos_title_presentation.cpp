#include "data/millennium_dos_title_presentation.hpp"

#include "data/sha256.hpp"

#include <stdexcept>
#include <string_view>

namespace eon {
namespace {

constexpr std::string_view english_title_library_sha256 =
    "6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678";

} // namespace

MillenniumDosTitlePresentationAssets parse_millennium_dos_title_presentation_assets(
    const MillenniumDosLib& title_library, const MillenniumDosTitleFlow& flow) {
    // Do not make structural TITLE.LIB compatibility an implicit cross-release
    // policy. The base-frame resource and the transition instruction profile
    // are evidence for this exact English leaf only.
    if (title_library.source_sha256() != english_title_library_sha256) {
        throw std::runtime_error("Unsupported Millennium English DOS title library");
    }
    if (flow.title_resource_index != 0 || flow.intro_transition_steps != 0x25
        || flow.intro_step_stride != 0x170) {
        throw std::runtime_error("Unsupported Millennium English DOS title presentation profile");
    }
    if (title_library.entries().size() != 38) {
        throw std::runtime_error("Unexpected Millennium English DOS title resource count");
    }
    const auto* base_entry = title_library.find("P00");
    if (!base_entry || base_entry != &title_library.entries().front()
        || base_entry->offset != 0x000006 || base_entry->size != 10'555) {
        throw std::runtime_error("Invalid Millennium English DOS P00 resource range");
    }

    const auto base_record = title_library.read(*base_entry);
    MillenniumDosTitlePresentationAssets result;
    result.title_library_sha256 = title_library.source_sha256();
    result.base_resource_name = base_entry->name;
    result.base_resource_offset = base_entry->offset;
    result.base_resource_size = base_entry->size;
    result.base_resource_sha256 = to_hex(sha256(base_record));
    result.base_bitmap = decode_millennium_dos_bitmap(base_record);
    if (result.base_bitmap.flags != 0x07 || result.base_bitmap.codec != 2
        || result.base_bitmap.max_palette_index != 35 || result.base_bitmap.width != 320
        || result.base_bitmap.height != 200 || result.base_bitmap.encoded_span != 9'687
        || result.base_bitmap.pixels.size() != 64'000) {
        throw std::runtime_error("Unsupported Millennium English DOS P00 bitmap profile");
    }
    result.base_palette = decode_millennium_dos_palette(base_record, result.base_bitmap);
    if (result.base_palette.logical_to_dac.size() != 36
        || result.base_palette.auxiliary_translation.length != 36) {
        throw std::runtime_error("Unsupported Millennium English DOS P00 palette profile");
    }
    result.base_rgba = colorize_millennium_dos_bitmap(result.base_bitmap, result.base_palette);
    if (result.base_rgba.size() != 320U * 200U * 4U) {
        throw std::runtime_error("Invalid Millennium English DOS P00 RGBA extent");
    }
    result.transition = parse_millennium_dos_title_transition(title_library, flow);
    if (result.transition.patches.size() != 37) {
        throw std::runtime_error("Incomplete Millennium English DOS title patch bank");
    }
    return result;
}

} // namespace eon
