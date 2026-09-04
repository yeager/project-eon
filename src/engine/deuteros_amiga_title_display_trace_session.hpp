#pragma once

#include "data/reference_trace.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace eon {

struct DeuterosAmigaTitleDisplayTraceAdmission;

struct DeuterosAmigaTitleDisplayArtifactCheckpoint {
    std::string role;
    std::uint64_t size = 0;
    std::string sha256;
    std::string format;
    constexpr bool operator==(const DeuterosAmigaTitleDisplayArtifactCheckpoint&) const = default;
};

// Immutable provenance after a second, consumption-time validation. It has
// deliberately no pixels, samples, input events, source paths or guest state.
struct DeuterosAmigaTitleDisplayTraceCheckpoint {
    std::string adapter;
    std::string format;
    std::string release_sha256;
    std::string source_media_sha256;
    std::string source_stage_sha256;
    std::string event_sha256;
    std::size_t event_count = 0;
    std::size_t bridge_event_count = 0;
    std::size_t display_layout_count = 0;
    std::size_t bitplane_layout_count = 0;
    std::size_t palette_checkpoint_count = 0;
    std::size_t input_checkpoint_count = 0;
    std::size_t frame_checkpoint_count = 0;
    std::size_t audio_checkpoint_count = 0;
    std::vector<DeuterosAmigaTitleDisplayArtifactCheckpoint> artifacts;
};

class DeuterosAmigaTitleDisplayTraceSession {
public:
    [[nodiscard]] const DeuterosAmigaTitleDisplayTraceCheckpoint& checkpoint() const noexcept {
        return checkpoint_;
    }

private:
    explicit DeuterosAmigaTitleDisplayTraceSession(
        DeuterosAmigaTitleDisplayTraceCheckpoint checkpoint)
        : checkpoint_(std::move(checkpoint)) {}

    DeuterosAmigaTitleDisplayTraceCheckpoint checkpoint_;
    friend struct DeuterosAmigaTitleDisplayTraceAdmission;
    friend DeuterosAmigaTitleDisplayTraceAdmission
        admit_deuteros_amiga_title_display_trace(const ReferenceTrace& trace);
};

struct DeuterosAmigaTitleDisplayTraceAdmission {
    std::optional<DeuterosAmigaTitleDisplayTraceSession> session;
    std::string error;
};

// Reopens and revalidates an already provenance-validated v4/v5 trace. The
// resulting session owns values only, so changing/deleting external evidence
// cannot create dangling borrows or silently change a live checkpoint.
[[nodiscard]] DeuterosAmigaTitleDisplayTraceAdmission
admit_deuteros_amiga_title_display_trace(const ReferenceTrace& trace);

} // namespace eon
