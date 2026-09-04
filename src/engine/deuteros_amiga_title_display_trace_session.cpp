#include "engine/deuteros_amiga_title_display_trace_session.hpp"

#include "data/deuteros_amiga_title_display_reference_trace.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>

namespace eon {
namespace {

constexpr std::string_view release_sha256 =
    "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04";
constexpr std::string_view media_sha256 =
    "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38";
constexpr std::string_view stage_sha256 =
    "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03";
constexpr std::string_view v4_adapter = "deuteros-amiga-en-title-display-v4";
constexpr std::string_view v5_adapter = "deuteros-amiga-en-title-display-artifacts-v5";

std::optional<std::vector<std::uint8_t>> read_exact_regular_file(
    const std::filesystem::path& path, const std::uint64_t expected_size) {
    if (expected_size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return std::nullopt;
    }
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error || std::filesystem::is_symlink(status) || !std::filesystem::is_regular_file(status)
        || std::filesystem::file_size(path, error) != expected_size || error) {
        return std::nullopt;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return std::nullopt;
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(expected_size));
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream || static_cast<std::size_t>(stream.gcount()) != bytes.size()) return std::nullopt;
    // Reject a regular file replaced or resized while it was being consumed.
    const auto final_status = std::filesystem::symlink_status(path, error);
    if (error || std::filesystem::is_symlink(final_status)
        || !std::filesystem::is_regular_file(final_status)
        || std::filesystem::file_size(path, error) != expected_size || error) {
        return std::nullopt;
    }
    return bytes;
}

bool counts_match(const ReferenceTrace& trace,
    const DeuterosAmigaTitleDisplayReferenceTraceDiagnostics& diagnostics) {
    return trace.event_count == diagnostics.event_count
        && trace.adapter_display_layout_count == diagnostics.display_layout_count
        && trace.adapter_bitplane_layout_count == diagnostics.bitplane_layout_count
        && trace.adapter_palette_checkpoint_count == diagnostics.palette_checkpoint_count
        && trace.adapter_input_checkpoint_count == diagnostics.input_checkpoint_count
        && trace.adapter_frame_checkpoint_count == diagnostics.frame_checkpoint_count
        && trace.adapter_audio_checkpoint_count == diagnostics.audio_checkpoint_count;
}

} // namespace

DeuterosAmigaTitleDisplayTraceAdmission
admit_deuteros_amiga_title_display_trace(const ReferenceTrace& trace) {
    DeuterosAmigaTitleDisplayTraceAdmission rejected;
    const bool v4 = trace.adapter == v4_adapter
        && trace.format == "project-eon-reference-trace-v4";
    const bool v5 = trace.adapter == v5_adapter
        && trace.format == "project-eon-reference-trace-v5";
    if ((!v4 && !v5) || trace.source_release.game != Game::deuteros
        || trace.source_release.platform != Platform::amiga
        || trace.source_release.language != "en"
        || trace.source_release.sha256 != release_sha256) {
        rejected.error = "Trace does not name the exact Deuteros Amiga title-display release";
        return rejected;
    }
    if (trace.source_media_sha256 != media_sha256 || trace.source_stage_sha256 != stage_sha256) {
        rejected.error = "Trace does not name the exact Deuteros Amiga title-display media";
        return rejected;
    }
    const auto events = read_exact_regular_file(trace.events_path, trace.event_size);
    if (!events || to_hex(sha256(*events)) != trace.event_sha256) {
        rejected.error = "Title-display events changed after validation";
        return rejected;
    }
    DeuterosAmigaTitleDisplayReferenceTraceDiagnostics diagnostics;
    std::string validation_error;
    const std::string_view event_text(reinterpret_cast<const char*>(events->data()), events->size());
    if (!validate_deuteros_amiga_title_display_reference_events(
            event_text, diagnostics, validation_error, trace.input_timeline_sha256)
        || !counts_match(trace, diagnostics)) {
        rejected.error = "Title-display event contract changed after validation";
        return rejected;
    }
    if ((v4 && !trace.artifacts.empty()) || (v5 && trace.artifacts.size() != 7)) {
        rejected.error = "Title-display artifact contract changed after validation";
        return rejected;
    }
    std::vector<DeuterosAmigaTitleDisplayArtifactCheckpoint> artifacts;
    artifacts.reserve(trace.artifacts.size());
    for (const auto& artifact : trace.artifacts) {
        const auto bytes = read_exact_regular_file(artifact.path, artifact.size);
        if (!bytes || to_hex(sha256(*bytes)) != artifact.sha256) {
            rejected.error = "Title-display artifact changed after validation";
            return rejected;
        }
        artifacts.push_back({artifact.role, artifact.size, artifact.sha256, artifact.format});
    }
    DeuterosAmigaTitleDisplayTraceCheckpoint checkpoint{
        trace.adapter, trace.format, trace.source_release.sha256, trace.source_media_sha256,
        trace.source_stage_sha256, trace.event_sha256, diagnostics.event_count,
        diagnostics.bridge_event_count, diagnostics.display_layout_count,
        diagnostics.bitplane_layout_count, diagnostics.palette_checkpoint_count,
        diagnostics.input_checkpoint_count, diagnostics.frame_checkpoint_count,
        diagnostics.audio_checkpoint_count, std::move(artifacts)};
    return {DeuterosAmigaTitleDisplayTraceSession(std::move(checkpoint)), {}};
}

} // namespace eon
