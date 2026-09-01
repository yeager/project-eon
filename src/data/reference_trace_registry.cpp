#include "data/reference_trace_registry.hpp"

#include <algorithm>
#include <array>

namespace eon {
namespace {

constexpr std::array<ReferenceTraceAdapterDescriptor, 10> registry{{
    {"millennium-dos-en-startup-v1", "project-eon-reference-trace-v2", Game::millennium, Platform::dos, "en", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "", "", ReferenceTraceRuntimePolicy::diagnostics_only, {"millennium-dos-launcher", "millennium-dos-title-flow", "millennium-dos-game-flow"}, 3},
    {"millennium-dos-en-gx-startup-v2", "project-eon-reference-trace-v2", Game::millennium, Platform::dos, "en", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "", "", ReferenceTraceRuntimePolicy::transient_call_free_gx_startup, {"millennium-dos-game-flow", "millennium-dos-gx-overlay", ""}, 2},
    {"millennium-dos-en-title-init-v2", "project-eon-reference-trace-v2", Game::millennium, Platform::dos, "en", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "", "", ReferenceTraceRuntimePolicy::diagnostics_only, {"millennium-dos-launcher", "millennium-dos-title-flow", ""}, 2},
    {"deuteros-atari-st-boot-v1", "project-eon-reference-trace-v2", Game::deuteros, Platform::atari_st, "en", "c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653", "aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee", "2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7", ReferenceTraceRuntimePolicy::diagnostics_only, {"deuteros-atari-protected-boot", "deuteros-atari-first-stage", ""}, 2},
    {"millennium-amiga-en-defjam-bootstrap-v1", "project-eon-reference-trace-v2", Game::millennium, Platform::amiga, "en", "2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400", "", "", ReferenceTraceRuntimePolicy::diagnostics_only, {"millennium-amiga-defjam-bootstrap", "millennium-amiga-shared-resident", ""}, 2},
    {"deuteros-amiga-en-title-stage-v1", "project-eon-reference-trace-v2", Game::deuteros, Platform::amiga, "en", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04", "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38", "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03", ReferenceTraceRuntimePolicy::diagnostics_only, {"deuteros-amiga-main-stage", "deuteros-amiga-title-handoff", ""}, 2},
    {"deuteros-amiga-en-main-copy-loop-v3", "project-eon-reference-trace-v3", Game::deuteros, Platform::amiga, "en", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04", "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38", "a82c0d6a12e156e0832d632a6c40dd58713a00b611dbcba7289aa16b0969a0a6", ReferenceTraceRuntimePolicy::diagnostics_only, {"deuteros-amiga-main-stage", "", ""}, 1},
    {"deuteros-amiga-en-title-bridge-v3", "project-eon-reference-trace-v3", Game::deuteros, Platform::amiga, "en", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04", "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38", "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03", ReferenceTraceRuntimePolicy::diagnostics_only, {"deuteros-amiga-main-stage", "deuteros-amiga-title-handoff", ""}, 2},
    {"deuteros-amiga-en-title-display-v4", "project-eon-reference-trace-v4", Game::deuteros, Platform::amiga, "en", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04", "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38", "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03", ReferenceTraceRuntimePolicy::diagnostics_only, {"deuteros-amiga-main-stage", "deuteros-amiga-title-handoff", ""}, 2},
    {"deuteros-amiga-en-title-display-artifacts-v5", "project-eon-reference-trace-v5", Game::deuteros, Platform::amiga, "en", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04", "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38", "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03", ReferenceTraceRuntimePolicy::diagnostics_only, {"deuteros-amiga-main-stage", "deuteros-amiga-title-handoff", ""}, 2},
}};

} // namespace

std::string_view reference_trace_runtime_policy_label(const ReferenceTraceRuntimePolicy policy) {
    switch (policy) {
    case ReferenceTraceRuntimePolicy::diagnostics_only: return "diagnostics-only";
    case ReferenceTraceRuntimePolicy::transient_call_free_gx_startup:
        return "transient-call-free-gx-startup";
    }
    return "diagnostics-only";
}

ReferenceTraceDiagnosticReport reference_trace_diagnostic_report(const std::string_view wire_id) {
    if (wire_id == "millennium-dos-en-gx-startup-v2") {
        return ReferenceTraceDiagnosticReport::millennium_dos_gx_startup;
    }
    if (wire_id == "millennium-dos-en-title-init-v2") {
        return ReferenceTraceDiagnosticReport::millennium_dos_title_init;
    }
    if (wire_id == "deuteros-atari-st-boot-v1") {
        return ReferenceTraceDiagnosticReport::deuteros_atari_boot;
    }
    if (wire_id == "millennium-amiga-en-defjam-bootstrap-v1") {
        return ReferenceTraceDiagnosticReport::millennium_amiga_bootstrap;
    }
    if (wire_id == "deuteros-amiga-en-title-stage-v1") {
        return ReferenceTraceDiagnosticReport::deuteros_amiga_title_stage;
    }
    if (wire_id == "deuteros-amiga-en-title-bridge-v3") {
        return ReferenceTraceDiagnosticReport::deuteros_amiga_title_bridge;
    }
    if (wire_id == "deuteros-amiga-en-title-display-v4"
        || wire_id == "deuteros-amiga-en-title-display-artifacts-v5") {
        return ReferenceTraceDiagnosticReport::deuteros_amiga_title_display;
    }
    return ReferenceTraceDiagnosticReport::generic;
}

std::span<const ReferenceTraceAdapterDescriptor> reference_trace_adapter_registry() { return registry; }

const ReferenceTraceAdapterDescriptor* reference_trace_adapter_descriptor(const std::string_view wire_id) {
    const auto found = std::find_if(registry.begin(), registry.end(), [wire_id](const auto& descriptor) {
        return descriptor.wire_id == wire_id;
    });
    return found == registry.end() ? nullptr : &*found;
}

} // namespace eon
