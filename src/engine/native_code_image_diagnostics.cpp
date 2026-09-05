#include "engine/native_code_image_diagnostics.hpp"

#include <algorithm>

namespace eon {
namespace {

constexpr std::size_t excluded_image_count = 7;

struct SessionImageBinding {
    std::string_view release_sha256;
    RuntimeSessionKind session_kind;
    std::string_view image_id;
    std::string_view range_id;
};

constexpr std::string_view millennium_dos_release =
    "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123";
constexpr std::string_view millennium_amiga_release =
    "2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400";
constexpr std::string_view millennium_amiga_direct_release =
    "ec0424445d494809d2661492e289af71b056a429dde13b053a472ccc8347d4dd";
constexpr std::string_view deuteros_amiga_release =
    "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04";
constexpr std::string_view deuteros_atari_release =
    "c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653";

constexpr SessionImageBinding bindings[] = {
    {millennium_dos_release, RuntimeSessionKind::millennium_dos_title,
        "millennium-dos-titles-exe-linear", "millennium-dos-title-flow"},
    {millennium_dos_release, RuntimeSessionKind::millennium_dos_sound_driver_boundary,
        "millennium-dos-mill-com-linear", "millennium-dos-launcher"},
    {millennium_dos_release, RuntimeSessionKind::millennium_dos_title_handoff_boundary,
        "millennium-dos-titles-exe-linear", "millennium-dos-title-flow"},
    {millennium_dos_release, RuntimeSessionKind::millennium_dos_gx_startup_boundary,
        "millennium-dos-2200gx-exe-linear", "millennium-dos-gx-overlay"},
    {millennium_dos_release, RuntimeSessionKind::millennium_dos_post_overlay_loop,
        "millennium-dos-2200ad-exe-linear", "millennium-dos-game-flow"},
    {millennium_dos_release, RuntimeSessionKind::millennium_dos_first_function,
        "millennium-dos-2200ad-exe-linear", "millennium-dos-game-flow"},
    {millennium_dos_release, RuntimeSessionKind::millennium_dos_second_function,
        "millennium-dos-2200ad-exe-linear", "millennium-dos-game-flow"},
    {millennium_dos_release, RuntimeSessionKind::millennium_dos_second_function_callback,
        "millennium-dos-2200ad-exe-linear", "millennium-dos-game-flow"},
    {millennium_dos_release, RuntimeSessionKind::millennium_dos_third_function,
        "millennium-dos-2200ad-exe-linear", "millennium-dos-game-flow"},
    {millennium_dos_release, RuntimeSessionKind::millennium_dos_fourth_function,
        "millennium-dos-2200ad-exe-linear", "millennium-dos-game-flow"},
    {millennium_dos_release, RuntimeSessionKind::millennium_dos_fifth_function,
        "millennium-dos-2200ad-exe-linear", "millennium-dos-game-flow"},
    {millennium_dos_release, RuntimeSessionKind::millennium_dos_sixth_function,
        "millennium-dos-2200ad-exe-linear", "millennium-dos-game-flow"},
    {millennium_dos_release, RuntimeSessionKind::millennium_dos_seventh_function,
        "millennium-dos-2200ad-exe-linear", "millennium-dos-game-flow"},
    {millennium_dos_release, RuntimeSessionKind::millennium_dos_eighth_function,
        "millennium-dos-2200ad-exe-linear", "millennium-dos-game-flow"},
    {millennium_dos_release, RuntimeSessionKind::millennium_dos_ninth_function,
        "millennium-dos-2200ad-exe-linear", "millennium-dos-game-flow"},
    {millennium_dos_release, RuntimeSessionKind::millennium_dos_ninth_function_handoff,
        "millennium-dos-2200ad-exe-linear", "millennium-dos-game-flow"},
    {millennium_dos_release, RuntimeSessionKind::millennium_dos_tenth_function,
        "millennium-dos-2200ad-exe-linear", "millennium-dos-game-flow"},
    {millennium_amiga_release, RuntimeSessionKind::millennium_amiga_bootstrap,
        "millennium-amiga-defjam-first-stage-entry-linear",
        "millennium-amiga-defjam-first-stage-entry"},
    {millennium_amiga_direct_release, RuntimeSessionKind::millennium_amiga_bootstrap,
        "millennium-amiga-defjam-direct-first-stage-entry-linear",
        "millennium-amiga-defjam-direct-first-stage-entry"},
    {deuteros_amiga_release, RuntimeSessionKind::deuteros_amiga_opening,
        "deuteros-amiga-clean-loaded-spans", "deuteros-amiga-clean-main-stage"},
    {deuteros_amiga_release, RuntimeSessionKind::deuteros_amiga_title_stage,
        "deuteros-amiga-clean-loaded-spans", "deuteros-amiga-clean-title-handoff"},
    {deuteros_amiga_release, RuntimeSessionKind::deuteros_amiga_title_display_trace_boundary,
        "deuteros-amiga-clean-loaded-spans", "deuteros-amiga-clean-title-handoff"},
    {deuteros_atari_release, RuntimeSessionKind::deuteros_atari_bootstrap,
        "deuteros-atari-replicants-first-stage-linear",
        "deuteros-atari-replicants-first-stage"},
};

} // namespace

std::string_view native_code_address_basis_label(const NativeCodeAddressBasis basis) {
    switch (basis) {
    case NativeCodeAddressBasis::dos_com_linear_0x100: return "dos-com-linear-0x100";
    case NativeCodeAddressBasis::runtime_absolute: return "runtime-absolute";
    case NativeCodeAddressBasis::image_relative_unrelocated: return "image-relative-unrelocated";
    case NativeCodeAddressBasis::disk_relative: return "disk-relative";
    }
    return "unknown";
}

std::string_view native_code_load_status_label(const NativeCodeLoadStatus status) {
    switch (status) {
    case NativeCodeLoadStatus::address_basis_declared: return "address-basis-declared";
    case NativeCodeLoadStatus::unproven: return "unproven";
    }
    return "unknown";
}

NativeCodeImageRegistryDiagnostics native_code_image_registry_diagnostics(
    const std::optional<RuntimeSessionSnapshot>& session) {
    NativeCodeImageRegistryDiagnostics result{
        native_code_image_manifest().size(), excluded_image_count, std::nullopt};
    if (!session) return result;

    const auto binding = std::find_if(std::begin(bindings), std::end(bindings), [&](const auto& row) {
        return row.release_sha256 == session->release_sha256 && row.session_kind == session->kind;
    });
    if (binding == std::end(bindings)) return result;
    const auto manifest = native_code_image_manifest();
    const auto descriptor = std::find_if(manifest.begin(), manifest.end(), [&](const auto& row) {
        return row.release_sha256 == binding->release_sha256
            && row.image_id == binding->image_id && row.range_id == binding->range_id;
    });
    if (descriptor == manifest.end()) return result;
    result.active = ActiveNativeCodeImageDiagnostics{std::string(descriptor->image_id),
        std::string(descriptor->range_id), descriptor->address_basis, descriptor->load_status};
    return result;
}

} // namespace eon
