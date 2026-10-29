#pragma once

#include "data/sha256.hpp"
#include "engine/native_runtime_memory.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace eon {

struct DeuterosAmigaComputedDispatchObservation {
    std::uint64_t sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t d0_value = 0;
    std::uint32_t a0_value = 0;
    std::uint32_t return_address = 0;
};

struct DeuterosAmigaComputedDispatchCheckpoint {
    std::uint64_t sequence = 0;
    std::uint32_t pointer_cell_address = 0x222a2;
    std::uint32_t pointer_value = 0;
    std::uint32_t selected_target = 0;
    bool used_null_fallback = false;
    bool response_loop_target_candidate = false;
    bool fallback_clear_write = false;
};

struct DeuterosAmigaComputedDispatchResult {
    bool accepted = false;
    std::string error;
    DeuterosAmigaComputedDispatchCheckpoint checkpoint;
};

// Replays only the source-bound target selection at $223c4..$223dc. A dynamic
// nonzero target is recorded as a candidate; this helper never executes it.
class DeuterosAmigaTitleComputedDispatchSession {
public:
    explicit DeuterosAmigaTitleComputedDispatchSession(
        std::span<const std::uint8_t> original_title_stage)
        : original_title_stage_(original_title_stage.begin(), original_title_stage.end()) {
        constexpr std::string_view stage_hash =
            "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03";
        constexpr std::string_view dispatch_hash =
            "62d436102c1adab08f4598c6f9a213d1c50ed81d1be135238208c54eb6a9845d";
        constexpr std::size_t dispatch_offset = 0x223c4U - 0x13000U;
        constexpr std::size_t dispatch_size = 0x1a;
        if (to_hex(sha256(original_title_stage_)) != stage_hash
            || to_hex(sha256(std::span<const std::uint8_t>(original_title_stage_)
                    .subspan(dispatch_offset, dispatch_size))) != dispatch_hash) {
            throw std::runtime_error("Unsupported Deuteros computed-dispatch source");
        }
    }

    [[nodiscard]] DeuterosAmigaComputedDispatchResult observe_call(
        const DeuterosAmigaComputedDispatchObservation& observation,
        const NativeRuntimeMemory& owned_memory) {
        DeuterosAmigaComputedDispatchResult result;
        constexpr std::uint32_t stage_begin = 0x13000;
        constexpr std::uint32_t stage_end = 0x7fa00;
        constexpr std::uint32_t call_instruction = 0x223dc;
        constexpr std::uint32_t return_address = 0x223de;
        constexpr std::uint32_t pointer_cell = 0x222a2;
        constexpr std::uint32_t fallback = 0x36a8c;
        if (observation.sequence == 0 || observation.sequence <= last_sequence_
            || observation.instruction_address != call_instruction
            || observation.return_address != return_address) {
            result.error = "Deuteros computed dispatch observation is out of sequence or at the wrong call";
            return result;
        }
        const auto pointer_bytes = owned_memory.read_linear_range(pointer_cell, 4);
        if (!pointer_bytes) {
            result.error = "Deuteros callback pointer cell is not fully owned";
            return result;
        }
        const auto pointer_value = (static_cast<std::uint32_t>((*pointer_bytes)[0]) << 24U)
            | (static_cast<std::uint32_t>((*pointer_bytes)[1]) << 16U)
            | (static_cast<std::uint32_t>((*pointer_bytes)[2]) << 8U)
            | static_cast<std::uint32_t>((*pointer_bytes)[3]);
        const bool null_fallback = pointer_value == 0;
        const auto selected_target = null_fallback ? fallback : pointer_value;
        if (observation.d0_value != selected_target || observation.a0_value != selected_target) {
            result.error = "Deuteros computed dispatch registers disagree with the owned pointer";
            return result;
        }
        if (selected_target < stage_begin || selected_target >= stage_end
            || !owned_memory.read_linear_range(selected_target, 2)) {
            result.error = "Deuteros computed dispatch target is outside owned title-stage memory";
            return result;
        }
        const auto target_offset = static_cast<std::size_t>(selected_target - stage_begin);
        const auto target_size = selected_target == 0x4077e ? 0x36U : 2U;
        if (target_offset > original_title_stage_.size()
            || target_size > original_title_stage_.size() - target_offset) {
            result.error = "Deuteros computed dispatch target span is outside the admitted source";
            return result;
        }
        const auto target_source = std::span<const std::uint8_t>(original_title_stage_)
            .subspan(target_offset, target_size);
        const auto target_owned = owned_memory.read_linear_range(selected_target, target_size);
        if (!target_owned || !std::equal(target_source.begin(), target_source.end(), target_owned->begin())) {
            result.error = "Deuteros computed dispatch target bytes differ from the admitted source";
            return result;
        }
        if (selected_target == 0x4077e
            && to_hex(sha256(target_source))
                != "90648a2b59070bdb564c87d2471b6f3d42f73b957d0cd3c24ab3503c97a551ad") {
            result.error = "Deuteros response-loop target span hash mismatch";
            return result;
        }
        result.checkpoint = {observation.sequence, pointer_cell, pointer_value,
            selected_target, null_fallback, selected_target == 0x4077e, null_fallback};
        result.accepted = true;
        last_sequence_ = observation.sequence;
        return result;
    }

private:
    std::vector<std::uint8_t> original_title_stage_;
    std::uint64_t last_sequence_ = 0;
};

} // namespace eon
