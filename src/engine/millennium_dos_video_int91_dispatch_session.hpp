#pragma once

#include "engine/millennium_dos_title_initialization_session.hpp"
#include "engine/millennium_dos_video_function_zero_session.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace eon {

// The exact first TITLES.EXE function-$00 call may be routed only after the
// observed INT 91h vector identifies the loaded driver and owned DOS memory
// still contains its complete hash-admitted image. This records the dispatch
// and stops at the driver's first INT 10h boundary. Later BIOS boundaries
// accept only explicit, matched result observations; DOS and INT 91h returns
// are not emulated.
struct MillenniumDosVideoInt91FunctionZeroDispatch {
    std::uint16_t title_call_address = 0;
    std::uint16_t wrapper_address = 0;
    std::uint16_t int91_address = 0;
    std::uint16_t record_segment = 0;
    std::uint16_t record_offset = 0;
    std::uint16_t loaded_driver_segment = 0;
    std::uint16_t ivt_offset = 0;
    std::uint16_t dispatch_table_address = 0;
    std::uint16_t function_zero_target = 0;
    std::uint8_t initial_cached_mode = 0;
    MillenniumDosVideoFunctionZeroBoundary first_bios_boundary{};
    std::uint16_t initial_argument_byte = 0;
    bool initial_cx_from_title_argument = false;
    constexpr bool operator==(const MillenniumDosVideoInt91FunctionZeroDispatch&) const = default;
};

class MillenniumDosVideoInt91DispatchSession {
public:
    MillenniumDosVideoInt91DispatchSession(
        const MillenniumDosTitleInitializationCheckpoint& title,
        std::span<const std::uint8_t> exact_driver_image,
        std::span<const std::uint8_t> owned_loaded_driver_image,
        MillenniumDosVideoDriverKind kind,
        std::uint16_t loaded_driver_segment,
        std::uint8_t ivt_interrupt,
        std::uint16_t ivt_offset,
        std::uint16_t ivt_segment,
        std::uint64_t first_bios_sequence,
        std::optional<std::uint8_t> title_argument_byte = std::nullopt);

    [[nodiscard]] const MillenniumDosVideoInt91FunctionZeroDispatch& dispatch() const {
        return dispatch_;
    }
    [[nodiscard]] const MillenniumDosVideoFunctionZeroSession& function_zero() const {
        return function_zero_;
    }
    [[nodiscard]] MillenniumDosVideoFunctionZeroState state() const {
        return function_zero_.state();
    }
    [[nodiscard]] std::optional<MillenniumDosVideoFunctionZeroBoundary> boundary() const {
        return function_zero_.boundary();
    }
    [[nodiscard]] const std::optional<MillenniumDosVideoFunctionZeroLocalWrite>& cache_write() const {
        return function_zero_.cache_write();
    }
    [[nodiscard]] std::optional<MillenniumDosVideoFunctionZeroOutcome> outcome() const {
        return function_zero_.outcome();
    }
    [[nodiscard]] std::optional<MillenniumDosVideoFunctionZeroPostludeOutcome>
    postlude_outcome() const { return function_zero_.postlude_prefix_outcome(); }

    void observe_int10_result(const MillenniumDosVideoFunctionZeroBiosResult& result);
    [[nodiscard]] bool service_eon_mcga_function_zero_boundary();
    [[nodiscard]] bool eon_mcga_mode_set_completed() const {
        return eon_mcga_mode_set_completed_;
    }
    void advance_success_postlude_prefix();
    void advance_mcga_single_count_store_prefix();

private:
    MillenniumDosVideoInt91FunctionZeroDispatch dispatch_;
    MillenniumDosVideoFunctionZeroSession function_zero_;
    bool eon_mcga_mode_set_completed_ = false;
};

} // namespace eon
