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
    // This is the checked caller-side continuation from the boot loader.  It
    // records that the original first stage is invoked through A3 and that
    // the resident entry is the terminal A3 jump; it never invokes either
    // raw stage or assumes that the first one returns.
    [[nodiscard]] const MillenniumAmigaBootstrapOpaqueInvocationBoundary&
    opaque_invocation_boundary() const {
        return opaque_invocation_boundary_;
    }
    [[nodiscard]] const MillenniumAmigaFirstStageEntryBoundary&
    first_stage_entry_boundary() const { return first_stage_entry_boundary_; }
    // Hash-bound setup prefix reached after the resident first-stage view
    // initialization. This is static source evidence only; it does not claim
    // an OpenDevice result or that input.device has been called.
    [[nodiscard]] const MillenniumAmigaInputDeviceOpenBoundary&
    input_device_open_boundary() const { return input_device_open_boundary_; }
    // The relocator is retained as a stop boundary. Its final source byte is
    // outside the verified boot I/O request, so this session never exposes a
    // relocated executable image or treats either reported PC as runnable.
    [[nodiscard]] const MillenniumAmigaBootstrapRelocationBoundary&
    relocation_boundary() const {
        return relocation_boundary_;
    }
private:
    MillenniumAmigaLoadPlan plan_;
    MillenniumAmigaBootstrapOpaqueInvocationBoundary opaque_invocation_boundary_;
    MillenniumAmigaFirstStageEntryBoundary first_stage_entry_boundary_;
    MillenniumAmigaInputDeviceOpenBoundary input_device_open_boundary_;
    MillenniumAmigaBootstrapRelocationBoundary relocation_boundary_;
};

} // namespace eon
