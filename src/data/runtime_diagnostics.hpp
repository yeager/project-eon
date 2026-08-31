#pragma once

#include "platform/game_data.hpp"
#include "platform/platform_coverage.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace eon {

// A renderer- and CLI-neutral view of preservation facts that are safe to
// show for one already verified release. It contains no source paths, media
// bytes, emulator state, hook data, or instructions for executing guest code.
struct RuntimeDiagnosticStartupBoundary {
    std::string parser_profile_id;
    std::string source_address;
    std::string unresolved;
};

struct RuntimeDiagnosticRecoveryBoundary {
    std::string id;
    std::string parser_profile_id;
    std::string cpu;
    std::string source_address;
    std::string evidence_level;
    std::string runtime_status;
    std::string documentation_anchor;
};

struct RuntimeDiagnosticFunction {
    std::string id;
    std::string parser_profile_id;
    std::string cpu;
    std::string source_asset_sha256;
    std::string source_offset;
    std::string runtime_address;
    std::string evidence_level;
    std::string uncertainty;
    std::string runtime_status;
    std::string documentation_anchor;
    std::string address_space;
};

struct RuntimeDiagnosticsReport {
    Game game = Game::millennium;
    Platform platform = Platform::dos;
    std::string language;
    std::string release_sha256;
    PlatformCoverage coverage = PlatformCoverage::bootstrap_only;
    std::optional<RuntimeDiagnosticStartupBoundary> startup_boundary;
    std::vector<RuntimeDiagnosticRecoveryBoundary> recovery_boundaries;
    std::vector<RuntimeDiagnosticFunction> functions;
    // A release report never admits a reference trace. Trace validation has a
    // separate terminating CLI route and cannot be implied by menu selection.
    std::string trace_admission = "not-loaded";
};

// Compose this only from hash-bound compiled declarations. Inconsistent rows
// fail closed rather than displaying a map for another release or language.
[[nodiscard]] RuntimeDiagnosticsReport runtime_diagnostics_for_release(
    const ReleaseArchive& release);

} // namespace eon
