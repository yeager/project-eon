#include "engine/millennium_dos_video_int91_dispatch_session.hpp"

#include "data/millennium_dos_video_driver.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace eon {

MillenniumDosVideoInt91DispatchSession::MillenniumDosVideoInt91DispatchSession(
    const MillenniumDosTitleInitializationCheckpoint& title,
    const std::span<const std::uint8_t> exact_driver_image,
    const std::span<const std::uint8_t> owned_loaded_driver_image,
    const MillenniumDosVideoDriverKind kind,
    const std::uint16_t loaded_driver_segment,
    const std::uint8_t ivt_interrupt,
    const std::uint16_t ivt_offset,
    const std::uint16_t ivt_segment,
    const std::uint64_t first_bios_sequence,
    const std::optional<std::uint8_t> title_argument_byte)
    : function_zero_(exact_driver_image, kind,
          [&] {
              const auto profile = parse_millennium_dos_video_driver(exact_driver_image, kind);
              if (profile.function_zero_cached_mode_address >= owned_loaded_driver_image.size())
                  throw std::runtime_error("Millennium DOS video cache is outside loaded driver");
              return owned_loaded_driver_image[profile.function_zero_cached_mode_address];
          }(), first_bios_sequence,
          kind == MillenniumDosVideoDriverKind::mcga && title_argument_byte
              ? std::optional<std::uint16_t>{*title_argument_byte}
              : std::nullopt) {
    if (title.state != MillenniumDosTitleInitializationState::private_interrupt_result_boundary
        || title.boundary.result_observed
        || title.boundary.call_address != 0x1b95
        || title.boundary.wrapper_address != 0x0122
        || title.boundary.interrupt_address != 0x0127
        || title.boundary.interrupt != 0x91
        || title.boundary.function != 0
        || title.boundary.record_segment != title.child_code_segment
        || title.boundary.record_offset != 0x1ac4
        || (title_argument_byte && kind != MillenniumDosVideoDriverKind::mcga)
        || title.last_sequence == std::numeric_limits<std::uint64_t>::max()
        || first_bios_sequence != title.last_sequence + 1) {
        throw std::runtime_error("TITLES.EXE boundary is not its first function-zero INT 91h call");
    }
    if (kind == MillenniumDosVideoDriverKind::mcga && title_argument_byte
        && *title_argument_byte != 0x01) {
        throw std::runtime_error("TITLES.EXE MCGA count byte is not the admitted single-count value");
    }
    const auto profile = parse_millennium_dos_video_driver(exact_driver_image, kind);
    if (loaded_driver_segment == 0 || ivt_interrupt != 0x91 || ivt_offset != 0
        || ivt_segment != loaded_driver_segment
        || owned_loaded_driver_image.size() != profile.byte_size
        || !std::equal(exact_driver_image.begin(), exact_driver_image.end(),
            owned_loaded_driver_image.begin())) {
        throw std::runtime_error("Millennium DOS INT 91h vector or loaded driver image does not match");
    }
    const auto first_bios = function_zero_.boundary();
    if (!first_bios) {
        throw std::runtime_error("Millennium DOS function-zero dispatch has no BIOS boundary");
    }
    dispatch_ = {
        title.boundary.call_address,
        title.boundary.wrapper_address,
        title.boundary.interrupt_address,
        title.boundary.record_segment,
        title.boundary.record_offset,
        loaded_driver_segment,
        ivt_offset,
        profile.dispatch_table_address,
        profile.function_zero_address,
        owned_loaded_driver_image[profile.function_zero_cached_mode_address],
        *first_bios};
    dispatch_.initial_argument_byte = title_argument_byte.value_or(0);
    dispatch_.initial_cx_from_title_argument = title_argument_byte.has_value();
}

void MillenniumDosVideoInt91DispatchSession::observe_int10_result(
    const MillenniumDosVideoFunctionZeroBiosResult& result) {
    // The nested session checks the exact next sequence, INT 10h number, and
    // instruction address before changing state. In particular, a result for
    // a later BIOS call cannot skip the function-zero prefix.
    const auto pending = function_zero_.boundary();
    function_zero_.observe_bios_result(result);
    if (pending
        && pending->kind == MillenniumDosVideoFunctionZeroBoundaryKind::set_mode
        && result.origin == MillenniumDosVideoBiosResultOrigin::eon_native_video_policy) {
        eon_mcga_mode_set_completed_ = true;
    }
}

bool MillenniumDosVideoInt91DispatchSession::service_eon_mcga_function_zero_boundary() {
    const auto pending = function_zero_.boundary();
    const auto& profile = function_zero_.driver();
    if (!pending || profile.kind != MillenniumDosVideoDriverKind::mcga
        || profile.function_zero_video_mode != 0x13
        || pending->interrupt_number != 0x10) {
        return false;
    }

    MillenniumDosVideoFunctionZeroBiosResult result;
    result.sequence = pending->sequence;
    result.instruction_address = pending->instruction_address;
    result.interrupt_number = pending->interrupt_number;
    result.origin = MillenniumDosVideoBiosResultOrigin::eon_native_video_policy;
    result.ax_known_mask = 0x00ff;
    result.bx_known_mask = 0;
    result.cx_known_mask = 0;
    result.dx_known_mask = 0;
    result.flags_known_mask = 0;
    // The game loaded CX from its exact TITLES argument before entering this
    // BIOS sequence. Eon's INT 10h service preserves CX as its input register;
    // this is policy output, never an original BIOS observation.
    if (pending->cx_known_mask == 0xffff) {
        result.cx = pending->cx;
        result.cx_known_mask = 0xffff;
    }

    switch (pending->kind) {
    case MillenniumDosVideoFunctionZeroBoundaryKind::cached_mode_query:
        if (pending->instruction_address != profile.function_zero_cached_mode_query_interrupt_site)
            return false;
        result.ax = 0x0013;
        break;
    case MillenniumDosVideoFunctionZeroBoundaryKind::set_mode:
        if (pending->instruction_address != profile.function_zero_set_mode_interrupt_site
            || pending->ax != 0x0013 || pending->ax_known_mask != 0xffff) {
            return false;
        }
        // Eon binds this exact request to its logical 320x200 MCGA surface.
        // The DOS window remains governed by SDL presentation preferences.
        result.ax = 0x0000;
        break;
    case MillenniumDosVideoFunctionZeroBoundaryKind::verify_mode_query:
        if (!eon_mcga_mode_set_completed_
            || pending->instruction_address != profile.function_zero_verify_mode_interrupt_site) {
            return false;
        }
        result.ax = 0x0013;
        break;
    }

    observe_int10_result(result);
    return true;
}

void MillenniumDosVideoInt91DispatchSession::advance_success_postlude_prefix() {
    function_zero_.advance_success_postlude_prefix();
}

void MillenniumDosVideoInt91DispatchSession::advance_mcga_single_count_store_prefix() {
    function_zero_.advance_mcga_single_count_store_prefix();
}

} // namespace eon
