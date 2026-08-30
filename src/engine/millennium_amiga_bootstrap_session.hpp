#pragma once

#include "data/millennium_amiga_loader.hpp"

#include <array>
#include <vector>

namespace eon {

// All records here are independently hash-checked static raw-resident facts
// from the same Defjam image as the load plan. They are a diagnostics snapshot
// only: none represents a call return, runtime register state, input, or a
// runnable Amiga execution path.
struct MillenniumAmigaResidentEvidenceSnapshot {
    MillenniumAmigaResidentEntry entry;
    MillenniumAmigaResidentWordSplitter splitter;
    MillenniumAmigaResidentHelperRawBoundary helper_boundary;
    MillenniumAmigaResidentSetupHelperRawBoundary setup_helper_boundary;
    std::array<MillenniumAmigaResidentHelperStagingCallsite, 2> staging_callsites{};
    MillenniumAmigaResidentFirstPostHelperStaticChain first_post_helper_chain;
    MillenniumAmigaResidentSecondPostHelperStaticChain second_post_helper_chain;
    MillenniumAmigaResidentStagingDirectReachabilityBoundary staging_reachability;
    MillenniumAmigaResidentSeparateEntryGate separate_entry;
    MillenniumAmigaResidentSeparateBranchBoundary separate_branch;
    MillenniumAmigaResidentSeparatePostCallBoundary separate_post_call;
    MillenniumAmigaResidentSeparatePostCallTailBoundary separate_post_call_tail;
    MillenniumAmigaResidentSeparatePostCallTailBranchBoundary separate_post_call_tail_branch;
    MillenniumAmigaResidentSeparateComparisonBoundary separate_comparison;
    MillenniumAmigaResidentSeparateByteGateBoundary separate_byte_gate;
    MillenniumAmigaResidentSeparateByteGateTargetBoundary separate_byte_gate_target;
    MillenniumAmigaResidentSeparateByteGateConvergenceBoundary separate_byte_gate_convergence;
    MillenniumAmigaResidentSeparateByteGateTakenBranchBoundary separate_byte_gate_taken_branch;
    MillenniumAmigaResidentSeparateByteGateFallthroughBoundary separate_byte_gate_fallthrough;
    MillenniumAmigaResidentSeparatePostExternalCallBoundary separate_post_external_call;
    MillenniumAmigaResidentSeparateTerminalJumpRawTargetBoundary separate_terminal_jump_target;
    MillenniumAmigaResidentIndependentEntryGate independent_entry;
    MillenniumAmigaResidentNegativeD3Continuation negative_d3;
    MillenniumAmigaResidentNegativeD3Terminal negative_d3_terminal;
    MillenniumAmigaResidentPostNegativeD3Terminal post_negative_d3_terminal;
    MillenniumAmigaResidentPostNegativeD3ContinuationBoundary post_negative_d3_continuation;
};

// Validates Defjam's original raw Millennium Amiga load plan in memory. It
// records only the source ranges and resident entry, stopping before the
// first transformed stage and before any reconstructed Amiga OS behavior.
class MillenniumAmigaBootstrapSession {
public:
    explicit MillenniumAmigaBootstrapSession(std::vector<std::uint8_t> disk_image);

    [[nodiscard]] const MillenniumAmigaLoadPlan& plan() const { return plan_; }
    [[nodiscard]] const MillenniumAmigaSharedResidentLayout& shared_resident() const {
        return shared_resident_;
    }
    [[nodiscard]] const MillenniumAmigaResidentEntry& resident_entry() const {
        return resident_evidence_.entry;
    }
    [[nodiscard]] const MillenniumAmigaResidentEvidenceSnapshot& resident_evidence() const {
        return resident_evidence_;
    }
    // This is the checked caller-side continuation from the boot loader.  It
    // records that the original first stage is invoked through A3 and that
    // the resident entry is the terminal A3 jump; it never invokes either
    // raw stage or assumes that the first one returns.
    [[nodiscard]] const MillenniumAmigaBootstrapOpaqueInvocationBoundary&
    opaque_invocation_boundary() const {
        return opaque_invocation_boundary_;
    }
    // The relocator is retained as a stop boundary. Its final source byte is
    // outside the verified boot I/O request, so this session never exposes a
    // relocated executable image or treats either reported PC as runnable.
    [[nodiscard]] const MillenniumAmigaBootstrapRelocationBoundary&
    relocation_boundary() const {
        return relocation_boundary_;
    }
    // These anchors belong to the exact bytes requested by the opaque first
    // stage. They are source provenance only and are not a decoded first
    // stage, API model, input map, or renderer input.
    [[nodiscard]] const MillenniumAmigaFirstStageSourceAnchorBoundary&
    first_stage_source_anchors() const {
        return first_stage_source_anchors_;
    }
    // This is a separately bounded resident-local chain. It is not reached by
    // the opaque bootstrap path; retaining it in the same media-bound session
    // prevents a caller from joining arithmetic evidence from another image.
    [[nodiscard]] const MillenniumAmigaResidentPostNegativeD3Terminal&
    post_negative_d3_terminal() const {
        return resident_evidence_.post_negative_d3_terminal;
    }
    [[nodiscard]] const MillenniumAmigaResidentPostNegativeD3ContinuationBoundary&
    post_negative_d3_continuation() const {
        return resident_evidence_.post_negative_d3_continuation;
    }

private:
    MillenniumAmigaLoadPlan plan_;
    MillenniumAmigaSharedResidentLayout shared_resident_;
    MillenniumAmigaResidentEvidenceSnapshot resident_evidence_;
    MillenniumAmigaBootstrapOpaqueInvocationBoundary opaque_invocation_boundary_;
    MillenniumAmigaBootstrapRelocationBoundary relocation_boundary_;
    MillenniumAmigaFirstStageSourceAnchorBoundary first_stage_source_anchors_;
};

} // namespace eon
