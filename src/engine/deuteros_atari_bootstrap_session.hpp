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
    [[nodiscard]] const std::string& first_stage_sha256() const { return first_stage_sha256_; }
    [[nodiscard]] const std::string& second_stage_sha256() const { return second_stage_sha256_; }

private:
    DeuterosAtariBootProfile boot_;
    DeuterosAtariFirstStageProfile first_stage_;
    DeuterosAtariSecondStageProfile second_stage_;
    DeuterosAtariDispatchProfile dispatch_;
    DeuterosAtariRawLoadPlan state0_raw_load_plan_;
    DeuterosAtariRawRangeLoadPlan state1_raw_load_plan_;
    DeuterosAtariState5RawLoadPlan state5_raw_load_plan_;
    DeuterosAtariState5ReturnProfile state5_return_;
    DeuterosAtariSupervisorCallbackProfile supervisor_callback_;
    DeuterosAtariSupervisorCallbackContinuation supervisor_callback_continuation_;
    DeuterosAtariPostCallbackCalleeProfiles post_callback_callees_;
    std::string first_stage_sha256_;
    std::string second_stage_sha256_;
};

} // namespace eon
