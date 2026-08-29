#pragma once

#include "data/millennium_amiga_loader.hpp"

#include <vector>

namespace eon {

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
        return resident_entry_;
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
        return post_negative_d3_terminal_;
    }
    [[nodiscard]] const MillenniumAmigaResidentPostNegativeD3ContinuationBoundary&
    post_negative_d3_continuation() const {
        return post_negative_d3_continuation_;
    }

private:
    MillenniumAmigaLoadPlan plan_;
    MillenniumAmigaSharedResidentLayout shared_resident_;
    MillenniumAmigaResidentEntry resident_entry_;
    MillenniumAmigaBootstrapOpaqueInvocationBoundary opaque_invocation_boundary_;
    MillenniumAmigaBootstrapRelocationBoundary relocation_boundary_;
    MillenniumAmigaFirstStageSourceAnchorBoundary first_stage_source_anchors_;
    MillenniumAmigaResidentPostNegativeD3Terminal post_negative_d3_terminal_;
    MillenniumAmigaResidentPostNegativeD3ContinuationBoundary post_negative_d3_continuation_;
};

} // namespace eon
