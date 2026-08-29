#pragma once

#include "data/deuteros_atari_boot.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace eon {

// Materializes only the two raw stages explicitly requested by the supplied
// Replicants Disk 1 boot code. It validates their original bytes and stops
// before any XBIOS call, state selection, or title/game interpretation.
class DeuterosAtariBootstrapSession {
public:
    explicit DeuterosAtariBootstrapSession(std::vector<std::uint8_t> disk_image);

    [[nodiscard]] const DeuterosAtariBootProfile& boot() const { return boot_; }
    [[nodiscard]] const DeuterosAtariFirstStageProfile& first_stage() const { return first_stage_; }
    [[nodiscard]] const DeuterosAtariSecondStageProfile& second_stage() const { return second_stage_; }
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
    [[nodiscard]] const DeuterosAtariState5RawLoadPlan& state5_raw_load_plan() const {
        return state5_raw_load_plan_;
    }
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

private:
    DeuterosAtariBootProfile boot_;
    DeuterosAtariFirstStageProfile first_stage_;
    DeuterosAtariSecondStageProfile second_stage_;
    DeuterosAtariDispatchProfile dispatch_;
    DeuterosAtariState1ServiceBoundary state1_service_boundary_;
    DeuterosAtariRawLoadPlan state0_raw_load_plan_;
    DeuterosAtariRawRangeLoadPlan state1_raw_load_plan_;
    DeuterosAtariState5RawLoadPlan state5_raw_load_plan_;
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
