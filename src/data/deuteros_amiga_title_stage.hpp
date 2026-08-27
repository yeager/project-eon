#pragma once

#include "data/deuteros_amiga_loader.hpp"

#include <cstdint>

namespace eon {

// Facts recovered from the entry point of profile one.  This is intentionally
// a control-flow profile, not a claim that the original stage has been fully
// interpreted as game state.
struct DeuterosAmigaTitleStageProfile {
    std::uint32_t entry_address = 0;
    std::uint32_t incoming_mode_address = 0;
    std::uint16_t special_mode = 0;
    std::uint32_t special_mode_byte_address = 0;
    std::uint32_t special_mode_configuration_address = 0;
    std::uint16_t special_mode_configuration_value = 0;
    std::uint32_t normal_mode_byte_address = 0;
    std::uint8_t normal_mode_byte_value = 0;
    std::uint32_t main_loop_address = 0;
    std::uint32_t loop_service_address = 0;
    std::uint32_t loop_input_service_address = 0;
    std::uint32_t timer_counter_address = 0;
    std::uint32_t timer_threshold = 0;
    std::uint32_t timer_dispatch_address = 0;
};

// Reads profile-one instructions directly from the original ADF and validates
// the known mode branch and the recurring main-loop cadence.
[[nodiscard]] DeuterosAmigaTitleStageProfile parse_deuteros_amiga_title_stage(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

} // namespace eon
