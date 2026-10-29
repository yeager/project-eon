#pragma once

#include "engine/deuteros_amiga_title_exec_boundary_session.hpp"
#include "engine/deuteros_amiga_graphics_library_compatibility.hpp"
#include "engine/deuteros_amiga_native_display_surface.hpp"

#include <cstdint>
#include <array>
#include <limits>
#include <optional>
#include <stdexcept>

namespace eon {

enum class DeuterosAmigaTitleOpenLibraryBoundaryState {
    awaiting_open_library_return,
    zero_result_original_loop,
    before_graphics_dependent_setup,
    awaiting_external_display_base_read,
    before_custom_chip_boundary,
};

struct DeuterosAmigaObservedDisplayBaseRead {
    std::uint64_t trace_sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t source_address = 0;
    std::uint32_t observed_value = 0;
};

struct DeuterosAmigaTitleDisplayLocalAdvance {
    std::optional<DeuterosAmigaObservedDisplayBaseRead> observation;
    std::optional<DeuterosAmigaNativeDisplaySurface> native_surface;
    std::uint32_t display_base_value = 0;
    std::uint32_t display_base_source_address = 0;
    std::array<std::uint32_t, 2> base_pointer_destinations{};
    std::uint32_t palette_destination_address = 0;
    std::uint32_t palette_count_address = 0;
    std::uint16_t palette_count = 0;
    std::uint32_t palette_pointer_address = 0;
    std::array<std::uint16_t, 20> palette_words{};
    std::uint32_t derived_pointer_destination_address = 0;
    std::uint32_t derived_pointer_value = 0;
    std::uint32_t cleared_word_address = 0;
    std::array<std::uint32_t, 2> caller_pointer_copy_destinations{};
    std::uint32_t clear_destination = 0;
    std::uint32_t clear_byte_count = 0;
    std::uint8_t clear_write_width = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaTitlePostOpenLibraryLocalAdvance {
    std::uint32_t observed_result_store_address = 0;
    std::uint32_t observed_result_store_value = 0;
    std::uint32_t increment_word_address = 0;
    std::uint16_t increment_delta = 0;
    std::uint32_t first_call_address = 0;
    std::uint32_t first_call_target = 0;
    std::uint32_t nested_call_address = 0;
    std::uint32_t nested_call_target = 0;
    std::uint32_t stop_before_address = 0;
};

// A value-only observation made by a separately admitted genuine trace.  It
// neither asks the host to open an Amiga library nor assigns semantics to D0
// or SR beyond their observed register widths.
struct DeuterosAmigaObservedOpenLibraryReturn {
    std::uint64_t trace_sequence = 0;
    std::uint32_t entry_address = 0;
    std::uint32_t library_name_address = 0;
    std::uint32_t exec_base_source_address = 0;
    std::uint32_t call_address = 0;
    std::int16_t vector = 0;
    std::uint32_t return_address = 0;
    std::uint32_t result_d0 = 0;
    std::uint16_t result_sr = 0;
};

struct DeuterosAmigaTitleOpenLibraryBoundaryCheckpoint {
    DeuterosAmigaTitleOpenLibraryBoundaryState state =
        DeuterosAmigaTitleOpenLibraryBoundaryState::awaiting_open_library_return;
    std::uint32_t stop_before_address = 0;
    std::uint32_t caller_return_address = 0;
    std::uint32_t result_store_address = 0;
    std::optional<DeuterosAmigaObservedOpenLibraryReturn> observed_return;
    std::optional<DeuterosAmigaNativeOpenLibraryServiceReturn> native_service_return;
    std::optional<DeuterosAmigaNativeDisplaySurface> native_display_surface_return;
};

// Continues the hash-validated title chain only far enough to admit the first
// OpenLibrary return.  The nonzero path retains the original sparse store and
// stops back at the caller before the graphics-dependent setup; the zero path
// records the original self-loop without executing it.
class DeuterosAmigaTitleOpenLibraryBoundarySession {
public:
    DeuterosAmigaTitleOpenLibraryBoundarySession(
        const DeuterosAmigaTitleExecBoundaryCheckpoint& exec,
        const DeuterosAmigaTitleGraphicsSetupProfile& setup,
        const DeuterosAmigaTitleStageProfile& stage,
        const DeuterosAmigaTitleDisplayClearProfile& clear) {
        const bool observed_exec_returns = exec.observed_returns[0]
            && exec.observed_returns[1]
            && exec.observed_returns[1]->trace_sequence
                > exec.observed_returns[0]->trace_sequence;
        const bool native_privilege_transition = exec.native_privilege_transition
            && exec.native_privilege_transition->super_state_call_address == 0x4045a
            && exec.native_privilege_transition->super_state_vector == -0x96
            && exec.native_privilege_transition->super_state_return_address == 0x4045e
            && exec.native_privilege_transition->user_state_call_address == 0x40468
            && exec.native_privilege_transition->user_state_vector == -0x9c
            && exec.native_privilege_transition->user_state_return_address == 0x4046c
            && exec.native_privilege_transition->user_state_stack_argument == 0x7fff0
            && exec.native_privilege_transition->host_privilege_transition_elided;
        if (exec.state != DeuterosAmigaTitleExecBoundaryState::before_open_library_boundary
            || exec.stop_before_address != 0x4046c
            || (!observed_exec_returns && !native_privilege_transition)
            || setup.entry_address != 0x1ed80 || setup.library_name_address != 0x1ed02
            || setup.library_name != "graphics.library" || setup.exec_base_address != 4
            || setup.exec_vector != -0x228 || setup.zero_result_loop_address != 0x1edf6
            || setup.nonzero_result_store_address != 0x1ed96
            || setup.nonzero_result_destination_address != 0x12fec
            || setup.first_return_address != 0x1eda2
            || setup.following_entry_address != 0x1f172
            || setup.palette_copy_entry_address != 0x1eda6
            || setup.external_display_base_source_address != 0x12ff4
            || setup.palette_count_address != 0x12ec6
            || setup.palette_count != 0x0014
            || setup.palette_pointer_address != 0x12ec8
            || setup.palette_destination_address != 0x12ecc
            || setup.following_callee_sha256
                != "d6b37bc6431a1fe9145ae9403a5165028ccfd856a6529d1752f824b166807223"
            || stage.initialization_internal_calls[0] != 0x1ed80
            || stage.initialization_internal_calls[1] != 0x1f172
            || stage.initialization_internal_calls[2] != 0x1f182
            || clear.entry_address != 0x1f182
            || clear.destination_pointer_address != 0x1f168
            || clear.iteration_count != 0x1f40 || clear.value != 0
            || clear.write_width_bytes != 4 || clear.return_address != 0x1f194
            || clear.sha256
                != "9b02afb723e201cacb93d18d87613dee0f56369707867989209a41d9430ec5f3"
            || setup.first_callee_sha256
                != "42c96aa502e36711ed274b9ddf4d2d1de53abfebb4ebdf88fa99346d2b03e30b") {
            throw std::runtime_error("Invalid Deuteros OpenLibrary boundary provenance");
        }
        preceding_trace_sequence_ = observed_exec_returns
            ? exec.observed_returns[1]->trace_sequence : 0;
        checkpoint_.stop_before_address = 0x1ed8c;
        checkpoint_.caller_return_address = 0x40472;
        checkpoint_.result_store_address = setup.nonzero_result_destination_address;
        setup_ = setup;
        clear_ = clear;
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleOpenLibraryBoundaryCheckpoint>
    observe_return(const DeuterosAmigaObservedOpenLibraryReturn& observation) {
        if (checkpoint_.state
            != DeuterosAmigaTitleOpenLibraryBoundaryState::awaiting_open_library_return) {
            return std::nullopt;
        }
        if (observation.trace_sequence <= preceding_trace_sequence_
            || observation.entry_address != 0x1ed80
            || observation.library_name_address != 0x1ed02
            || observation.exec_base_source_address != 4
            || observation.call_address != 0x1ed8c || observation.vector != -0x228
            || observation.return_address != 0x1ed90) {
            throw std::runtime_error("Deuteros OpenLibrary return does not match boundary");
        }
        checkpoint_.observed_return = observation;
        if (observation.result_d0 == 0) {
            checkpoint_.state =
                DeuterosAmigaTitleOpenLibraryBoundaryState::zero_result_original_loop;
            checkpoint_.stop_before_address = 0x1edf6;
        } else {
            checkpoint_.state =
                DeuterosAmigaTitleOpenLibraryBoundaryState::before_graphics_dependent_setup;
            checkpoint_.stop_before_address = checkpoint_.caller_return_address;
        }
        return checkpoint_;
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleOpenLibraryBoundaryCheckpoint>
    provide_native_service_return(
        const DeuterosAmigaNativeOpenLibraryServiceReturn& service_return,
        const std::uint64_t runtime_generation) {
        if (checkpoint_.state
                != DeuterosAmigaTitleOpenLibraryBoundaryState::awaiting_open_library_return
            || checkpoint_.observed_return || checkpoint_.native_service_return
            || service_return.runtime_generation == 0
            || service_return.runtime_generation != runtime_generation
            || service_return.service_sequence != 1
            || service_return.entry_address != 0x1ed80
            || service_return.library_name_address != 0x1ed02
            || service_return.exec_base_source_address != 4
            || service_return.call_address != 0x1ed8c || service_return.vector != -0x228
            || service_return.return_address != 0x1ed90
            || service_return.library_name != "graphics.library"
            || service_return.minimum_version != 0
            || service_return.origin != DeuterosAmigaServiceOrigin::native_compatibility
            || (service_return.native_service_token & 0xf0000000U) != 0xe0000000U
            || (service_return.native_service_token & 0x0fffffffU)
                != service_return.runtime_generation) {
            throw std::runtime_error("Deuteros native graphics-library service return is detached");
        }
        checkpoint_.native_service_return = service_return;
        checkpoint_.state =
            DeuterosAmigaTitleOpenLibraryBoundaryState::before_graphics_dependent_setup;
        checkpoint_.stop_before_address = checkpoint_.caller_return_address;
        return checkpoint_;
    }

    // Executes only the statically proven nonzero local route: retain the
    // observed D0 store, describe (but do not fabricate the result of) the
    // original word increment, return to the caller, and follow both direct
    // JSR operands. It stops before $1eda6 reads unresolved cell $12ff4.
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostOpenLibraryLocalAdvance>
    advance_nonzero_local_path() {
        if (checkpoint_.state
            != DeuterosAmigaTitleOpenLibraryBoundaryState::before_graphics_dependent_setup
            || (checkpoint_.observed_return && checkpoint_.native_service_return)
            || (!checkpoint_.observed_return && !checkpoint_.native_service_return)
            || (checkpoint_.observed_return && checkpoint_.observed_return->result_d0 == 0)
            || (checkpoint_.native_service_return
                && checkpoint_.native_service_return->native_service_token == 0)) {
            return std::nullopt;
        }
        checkpoint_.state =
            DeuterosAmigaTitleOpenLibraryBoundaryState::awaiting_external_display_base_read;
        checkpoint_.stop_before_address = 0x1eda6;
        return DeuterosAmigaTitlePostOpenLibraryLocalAdvance{
            0x12fec, checkpoint_.observed_return
                ? checkpoint_.observed_return->result_d0
                : checkpoint_.native_service_return->native_service_token,
            0x1ed70, 1, 0x40472, 0x1f172, 0x1f172, 0x1eda6, 0x1eda6};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleDisplayLocalAdvance>
    observe_display_base_and_advance(const DeuterosAmigaObservedDisplayBaseRead& observation) {
        if (checkpoint_.state
            != DeuterosAmigaTitleOpenLibraryBoundaryState::awaiting_external_display_base_read) {
            return std::nullopt;
        }
        if (!checkpoint_.observed_return
            || observation.trace_sequence <= checkpoint_.observed_return->trace_sequence
            || observation.instruction_address != 0x1eda6
            || observation.source_address != setup_.external_display_base_source_address
            || observation.observed_value
                > std::numeric_limits<std::uint32_t>::max() - setup_.derived_pointer_addend
            || clear_.iteration_count
                > std::numeric_limits<std::uint32_t>::max() / clear_.write_width_bytes
            || observation.observed_value > std::numeric_limits<std::uint32_t>::max()
                - clear_.iteration_count * clear_.write_width_bytes) {
            throw std::runtime_error("Deuteros display-base observation does not match boundary");
        }
        checkpoint_.state =
            DeuterosAmigaTitleOpenLibraryBoundaryState::before_custom_chip_boundary;
        checkpoint_.stop_before_address = 0x40498;
        return DeuterosAmigaTitleDisplayLocalAdvance{observation,
            std::nullopt, observation.observed_value, observation.source_address,
            setup_.external_display_base_destinations, setup_.palette_destination_address,
            setup_.palette_count_address, setup_.palette_count,
            setup_.palette_pointer_address,
            setup_.palette_words, setup_.derived_pointer_destination_address,
            observation.observed_value + setup_.derived_pointer_addend, 0x1f16c,
            {0x1f974, 0x410d8}, observation.observed_value,
            clear_.iteration_count * clear_.write_width_bytes,
            clear_.write_width_bytes, 0x40498};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleDisplayLocalAdvance>
    provide_native_display_surface(
        const DeuterosAmigaNativeDisplaySurface& surface,
        const std::uint64_t runtime_generation) {
        constexpr std::uint32_t arena_begin = 0x00e00000;
        constexpr std::uint32_t arena_end = 0x00f00000;
        if (checkpoint_.state
                != DeuterosAmigaTitleOpenLibraryBoundaryState::awaiting_external_display_base_read
            || !checkpoint_.native_service_return || checkpoint_.observed_return
            || checkpoint_.native_display_surface_return
            || surface.runtime_generation == 0
            || surface.runtime_generation != runtime_generation
            || surface.service_sequence != 2 || surface.allocation_id != 1
            || surface.byte_count != 0x7d00 || surface.alignment != 0x1000
            || surface.base_address < arena_begin
            || surface.base_address > arena_end - surface.byte_count
            || (surface.base_address & (surface.alignment - 1U)) != 0
            || setup_.derived_pointer_addend > surface.byte_count
            || surface.origin != DeuterosAmigaDisplaySurfaceOrigin::native_compatibility
            || checkpoint_.native_service_return->runtime_generation != runtime_generation) {
            throw std::runtime_error("Deuteros native display surface is detached from its graphics service");
        }
        checkpoint_.native_display_surface_return = surface;
        checkpoint_.state =
            DeuterosAmigaTitleOpenLibraryBoundaryState::before_custom_chip_boundary;
        checkpoint_.stop_before_address = 0x40498;
        return DeuterosAmigaTitleDisplayLocalAdvance{std::nullopt, surface,
            surface.base_address, setup_.external_display_base_source_address,
            setup_.external_display_base_destinations,
            setup_.palette_destination_address, setup_.palette_count_address,
            setup_.palette_count, setup_.palette_pointer_address, setup_.palette_words,
            setup_.derived_pointer_destination_address,
            surface.base_address + setup_.derived_pointer_addend, 0x1f16c,
            {0x1f974, 0x410d8}, surface.base_address,
            clear_.iteration_count * clear_.write_width_bytes,
            clear_.write_width_bytes, 0x40498};
    }

    [[nodiscard]] const DeuterosAmigaTitleOpenLibraryBoundaryCheckpoint& checkpoint() const noexcept {
        return checkpoint_;
    }

private:
    std::uint64_t preceding_trace_sequence_ = 0;
    DeuterosAmigaTitleOpenLibraryBoundaryCheckpoint checkpoint_;
    DeuterosAmigaTitleGraphicsSetupProfile setup_;
    DeuterosAmigaTitleDisplayClearProfile clear_;
};

} // namespace eon
