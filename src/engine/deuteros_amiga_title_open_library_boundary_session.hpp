#pragma once

#include "engine/deuteros_amiga_title_exec_boundary_session.hpp"

#include <cstdint>
#include <optional>
#include <stdexcept>

namespace eon {

enum class DeuterosAmigaTitleOpenLibraryBoundaryState {
    awaiting_open_library_return,
    zero_result_original_loop,
    before_graphics_dependent_setup,
    awaiting_external_display_base_read,
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
        const DeuterosAmigaTitleStageProfile& stage) {
        if (exec.state != DeuterosAmigaTitleExecBoundaryState::before_open_library_boundary
            || exec.stop_before_address != 0x4046c || !exec.observed_returns[0]
            || !exec.observed_returns[1]
            || exec.observed_returns[1]->trace_sequence
                <= exec.observed_returns[0]->trace_sequence
            || setup.entry_address != 0x1ed80 || setup.library_name_address != 0x1ed02
            || setup.library_name != "graphics.library" || setup.exec_base_address != 4
            || setup.exec_vector != -0x228 || setup.zero_result_loop_address != 0x1edf6
            || setup.nonzero_result_store_address != 0x1ed96
            || setup.nonzero_result_destination_address != 0x12fec
            || setup.first_return_address != 0x1eda2
            || setup.following_entry_address != 0x1f172
            || setup.palette_copy_entry_address != 0x1eda6
            || setup.external_display_base_source_address != 0x12ff4
            || setup.following_callee_sha256
                != "d6b37bc6431a1fe9145ae9403a5165028ccfd856a6529d1752f824b166807223"
            || stage.initialization_internal_calls[0] != 0x1ed80
            || stage.initialization_internal_calls[1] != 0x1f172
            || setup.first_callee_sha256
                != "42c96aa502e36711ed274b9ddf4d2d1de53abfebb4ebdf88fa99346d2b03e30b") {
            throw std::runtime_error("Invalid Deuteros OpenLibrary boundary provenance");
        }
        preceding_trace_sequence_ = exec.observed_returns[1]->trace_sequence;
        checkpoint_.stop_before_address = 0x1ed8c;
        checkpoint_.caller_return_address = 0x40472;
        checkpoint_.result_store_address = setup.nonzero_result_destination_address;
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

    // Executes only the statically proven nonzero local route: retain the
    // observed D0 store, describe (but do not fabricate the result of) the
    // original word increment, return to the caller, and follow both direct
    // JSR operands. It stops before $1eda6 reads unresolved cell $12ff4.
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostOpenLibraryLocalAdvance>
    advance_nonzero_local_path() {
        if (checkpoint_.state
            != DeuterosAmigaTitleOpenLibraryBoundaryState::before_graphics_dependent_setup
            || !checkpoint_.observed_return || checkpoint_.observed_return->result_d0 == 0) {
            return std::nullopt;
        }
        checkpoint_.state =
            DeuterosAmigaTitleOpenLibraryBoundaryState::awaiting_external_display_base_read;
        checkpoint_.stop_before_address = 0x1eda6;
        return DeuterosAmigaTitlePostOpenLibraryLocalAdvance{
            0x12fec, checkpoint_.observed_return->result_d0,
            0x1ed70, 1, 0x40472, 0x1f172, 0x1f172, 0x1eda6, 0x1eda6};
    }

    [[nodiscard]] const DeuterosAmigaTitleOpenLibraryBoundaryCheckpoint& checkpoint() const noexcept {
        return checkpoint_;
    }

private:
    std::uint64_t preceding_trace_sequence_ = 0;
    DeuterosAmigaTitleOpenLibraryBoundaryCheckpoint checkpoint_;
};

} // namespace eon
