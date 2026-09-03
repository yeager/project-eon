#pragma once

#include "data/deuteros_atari_boot.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace eon {

// A media-safe summary of the exact static bootstrap evidence admitted for
// the Replicants Disk 1 profile. This is not an emulator snapshot: it retains
// no RAM, registers, source paths, disk bytes, or selected dispatcher vector.
struct DeuterosAtariBootstrapCheckpoint {
    std::string first_stage_sha256;
    std::string second_stage_sha256;
    std::size_t first_stage_entry_offset = 0;
    std::size_t relocated_dispatcher_address = 0;
    std::uint16_t state1_xbios_selector = 0;
    std::size_t state0_raw_request_count = 0;
    std::size_t state1_raw_request_count = 0;
    std::size_t state1_skipped_ascii_branch_relative_offset = 0;
    std::size_t state1_skipped_ascii_relative_offset = 0;
    std::size_t state1_skipped_ascii_byte_count = 0;
    std::size_t state1_skipped_ascii_printable_run_count = 0;
    std::string state1_skipped_ascii_sha256;
    std::size_t state5_first_source_offset = 0;
    std::size_t state5_second_source_offset = 0;
    std::size_t state1_display_branch_relative_offset = 0;
    std::size_t state1_display_service_relative_offset = 0;
    std::uint16_t state1_display_xbios_selector = 0;
};

// Materializes only the two raw stages explicitly requested by the supplied
// Replicants Disk 1 boot code. It validates their original bytes and stops
// before any XBIOS call, state selection, or title/game interpretation.
class DeuterosAtariBootstrapSession {
public:
    explicit DeuterosAtariBootstrapSession(std::vector<std::uint8_t> disk_image);

    [[nodiscard]] const DeuterosAtariBootProfile& boot() const { return boot_; }
    [[nodiscard]] const DeuterosAtariFirstStageProfile& first_stage() const { return first_stage_; }
    [[nodiscard]] const DeuterosAtariSecondStageProfile& second_stage() const { return second_stage_; }
    [[nodiscard]] const DeuterosAtariFirstStageCopyExecutionPrefix& first_stage_copy_execution() const {
        return first_stage_copy_execution_;
    }
    [[nodiscard]] const DeuterosAtariSecondStageEntryExecutionPrefix& entry_execution() const {
        return entry_execution_;
    }
    [[nodiscard]] const DeuterosAtariDispatchProfile& dispatch() const { return dispatch_; }
    [[nodiscard]] const DeuterosAtariState1ServiceBoundary& state1_service_boundary() const {
        return state1_service_boundary_;
    }
    [[nodiscard]] const DeuterosAtariRawLoadPlan& state0_raw_load_plan() const {
        return state0_raw_load_plan_;
    }
    [[nodiscard]] const DeuterosAtariRawRangeLoadPlan& state1_raw_load_plan() const {
        return state1_raw_load_plan_;
    }
    [[nodiscard]] const DeuterosAtariState1SkippedAsciiBlock& state1_skipped_ascii_block() const {
        return state1_skipped_ascii_block_;
    }
    [[nodiscard]] const DeuterosAtariState5RawLoadPlan& state5_raw_load_plan() const {
        return state5_raw_load_plan_;
    }
    [[nodiscard]] const DeuterosAtariState1DisplayServiceBoundary&
    state1_display_service_boundary() const { return state1_display_service_boundary_; }
    [[nodiscard]] const DeuterosAtariState5ReturnProfile& state5_return() const {
        return state5_return_;
    }
    [[nodiscard]] const DeuterosAtariSupervisorCallbackProfile& supervisor_callback() const {
        return supervisor_callback_;
    }
    [[nodiscard]] const DeuterosAtariSupervisorCallbackContinuation& supervisor_callback_continuation() const {
        return supervisor_callback_continuation_;
    }
    [[nodiscard]] const DeuterosAtariPostCallbackCalleeProfiles& post_callback_callees() const {
        return post_callback_callees_;
    }
    [[nodiscard]] const DeuterosAtariFirstCalleeContinuation& first_callee_continuation() const {
        return first_callee_continuation_;
    }
    [[nodiscard]] const DeuterosAtariSecondCalleeContinuation& second_callee_continuation() const {
        return second_callee_continuation_;
    }
    [[nodiscard]] const DeuterosAtariRawReaderWrapperProfile& raw_reader_wrapper() const {
        return raw_reader_wrapper_;
    }
    [[nodiscard]] const DeuterosAtariRawReaderCallLayout& raw_reader_call_layout() const {
        return raw_reader_call_layout_;
    }
    [[nodiscard]] const DeuterosAtariDirectVectorCalleeProfiles& direct_vector_callees() const {
        return direct_vector_callees_;
    }
    [[nodiscard]] const DeuterosAtariDirectVectorTransferLoopProfile& direct_vector_transfer_loop() const {
        return direct_vector_transfer_loop_;
    }
    [[nodiscard]] const DeuterosAtariDirectVectorTransferTailProfile& direct_vector_transfer_tail() const {
        return direct_vector_transfer_tail_;
    }
    [[nodiscard]] const DeuterosAtariStateSelectionLayout& state_selection_layout() const {
        return state_selection_layout_;
    }
    [[nodiscard]] const DeuterosAtariStateSelectionContinuation& state_selection_continuation() const {
        return state_selection_continuation_;
    }
    [[nodiscard]] const std::string& first_stage_sha256() const { return first_stage_sha256_; }
    [[nodiscard]] const std::string& second_stage_sha256() const { return second_stage_sha256_; }
    [[nodiscard]] DeuterosAtariBootstrapCheckpoint checkpoint() const;

private:
    DeuterosAtariBootProfile boot_;
    DeuterosAtariFirstStageProfile first_stage_;
    DeuterosAtariSecondStageProfile second_stage_;
    DeuterosAtariFirstStageCopyExecutionPrefix first_stage_copy_execution_;
    DeuterosAtariSecondStageEntryExecutionPrefix entry_execution_;
    DeuterosAtariDispatchProfile dispatch_;
    DeuterosAtariState1ServiceBoundary state1_service_boundary_;
    DeuterosAtariRawLoadPlan state0_raw_load_plan_;
    DeuterosAtariRawRangeLoadPlan state1_raw_load_plan_;
    DeuterosAtariState1SkippedAsciiBlock state1_skipped_ascii_block_;
    DeuterosAtariState5RawLoadPlan state5_raw_load_plan_;
    DeuterosAtariState1DisplayServiceBoundary state1_display_service_boundary_;
    DeuterosAtariState5ReturnProfile state5_return_;
    DeuterosAtariSupervisorCallbackProfile supervisor_callback_;
    DeuterosAtariSupervisorCallbackContinuation supervisor_callback_continuation_;
    DeuterosAtariPostCallbackCalleeProfiles post_callback_callees_;
    DeuterosAtariFirstCalleeContinuation first_callee_continuation_;
    DeuterosAtariSecondCalleeContinuation second_callee_continuation_;
    DeuterosAtariRawReaderWrapperProfile raw_reader_wrapper_;
    DeuterosAtariRawReaderCallLayout raw_reader_call_layout_;
    DeuterosAtariDirectVectorCalleeProfiles direct_vector_callees_;
    DeuterosAtariDirectVectorTransferLoopProfile direct_vector_transfer_loop_;
    DeuterosAtariDirectVectorTransferTailProfile direct_vector_transfer_tail_;
    DeuterosAtariStateSelectionLayout state_selection_layout_;
    DeuterosAtariStateSelectionContinuation state_selection_continuation_;
    std::string first_stage_sha256_;
    std::string second_stage_sha256_;
};

} // namespace eon
