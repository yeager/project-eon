#include "data/runtime_diagnostics.hpp"

#include "data/function_map.hpp"
#include "data/recovery_map.hpp"
#include "data/startup_boundary.hpp"

#include <stdexcept>

namespace eon {
namespace {

void require_release_identity(const ReleaseArchive& release, const Game game,
    const Platform platform, const std::string_view language,
    const std::string_view release_sha256, const std::string_view record_kind,
    const std::string_view record_id) {
    if (game != release.game || platform != release.platform || language != release.language
        || release_sha256 != release.sha256) {
        throw std::runtime_error("Runtime diagnostics " + std::string(record_kind)
            + " identity mismatch for " + std::string(record_id));
    }
}

} // namespace

RuntimeDiagnosticsReport runtime_diagnostics_for_release(const ReleaseArchive& release) {
    // Diagnostics are declarative and do not reopen user media, but they are
    // still release-bound: a forged DTO must never borrow the named map rows
    // belonging to a different game, platform, or language.
    if (!is_recognised_release_identity(release)) {
        throw std::runtime_error("Runtime diagnostics release is not an exact recognised manifest identity");
    }
    RuntimeDiagnosticsReport report;
    report.game = release.game;
    report.platform = release.platform;
    report.language = release.language;
    report.release_sha256 = release.sha256;
    report.coverage = platform_coverage(release);

    if (const auto startup = startup_boundary_for_release(release.sha256)) {
        report.startup_boundary = RuntimeDiagnosticStartupBoundary{
            std::string(startup->parser_profile_id), std::string(startup->source_address),
            std::string(startup->unresolved)};
    }
    for (const auto& boundary : recovery_map_for_release(release.sha256)) {
        require_release_identity(release, boundary.game, boundary.platform, boundary.language,
            boundary.release_sha256, "recovery boundary", boundary.id);
        if (!release_has_recovery_map_entry(release.sha256, boundary.id)) {
            throw std::runtime_error("Runtime diagnostics recovery boundary lost parser-profile binding: "
                + std::string(boundary.id));
        }
        report.recovery_boundaries.push_back({
            std::string(boundary.id), std::string(boundary.parser_profile_id), std::string(boundary.cpu),
            std::string(boundary.source_address), std::string(boundary.evidence_level),
            std::string(boundary.runtime_status), std::string(boundary.documentation_anchor)});
    }
    for (const auto& function : function_map_for_release(release.sha256)) {
        require_release_identity(release, function.game, function.platform, function.language,
            function.release_sha256, "function", function.id);
        if (!function_map_entry_is_well_formed(function)) {
            throw std::runtime_error("Runtime diagnostics function has malformed declarative provenance: "
                + std::string(function.id));
        }
        if (!release_has_function_map_entry(release.sha256, function.id)) {
            throw std::runtime_error("Runtime diagnostics function lost parser-profile binding: "
                + std::string(function.id));
        }
        report.functions.push_back({
            std::string(function.id), std::string(function.parser_profile_id), std::string(function.cpu),
            std::string(function.source_asset_sha256), std::string(function.source_offset),
            std::string(function.runtime_address), std::string(function.evidence_level),
            std::string(function.uncertainty), std::string(function.runtime_status),
            std::string(function.documentation_anchor), std::string(function.address_space)});
    }
    return report;
}

} // namespace eon
