#include "launcher.hpp"
#include "launcher_text.hpp"
#include "presentation_preferences.hpp"
#include "presentation/modern_presentation_pipeline.hpp"
#include "i18n.hpp"
#include "engine/deuteros_amiga_opening.hpp"
#include "engine/release_runtime.hpp"
#include "engine/deuteros_amiga_paula.hpp"
#include "engine/deuteros_atari_bootstrap_session.hpp"
#include "engine/millennium_dos_game_session.hpp"
#include "engine/menu_runtime_launch.hpp"
#include "engine/native_session_controller.hpp"
#include "engine/runtime_host.hpp"
#include "engine/millennium_dos_sound_selection_session.hpp"
#include "engine/millennium_dos_title_session.hpp"
#include "engine/millennium_dos_save_session.hpp"
#include "engine/millennium_atari_bootstrap_session.hpp"
#include "engine/millennium_amiga_bootstrap_session.hpp"
#include "data/amiga_adf.hpp"
#include "data/atari_st_prg.hpp"
#include "data/atari_st_stx.hpp"
#include "data/creative_voice.hpp"
#include "data/deuteros_amiga_bundle.hpp"
#include "data/deuteros_amiga_data_disk.hpp"
#include "data/deuteros_amiga_audio.hpp"
#include "data/deuteros_amiga_channel_vm.hpp"
#include "data/deuteros_amiga_frame.hpp"
#include "data/deuteros_amiga_loader.hpp"
#include "data/deuteros_amiga_title_stage.hpp"
#include "data/deuteros_atari_boot.hpp"
#include "data/fat12.hpp"
#include "data/millennium_control_text.hpp"
#include "data/millennium_dos_bitmap.hpp"
#include "data/millennium_dos_game_data.hpp"
#include "data/millennium_dos_game_flow.hpp"
#include "data/millennium_dos_gameplay_screen.hpp"
#include "data/millennium_dos_gx_catalog.hpp"
#include "data/millennium_dos_last_screen.hpp"
#include "data/millennium_amiga_loader.hpp"
#include "data/millennium_dos_lib.hpp"
#include "data/millennium_dos_title_flow.hpp"
#include "data/millennium_dos_title_exit.hpp"
#include "data/millennium_dos_title_transition.hpp"
#include "data/millennium_dos_title_presentation.hpp"
#include "data/millennium_dos_video_driver.hpp"
#include "data/millennium_dos_sound_driver.hpp"
#include "data/millennium_save_comparison.hpp"
#include "data/modern_asset_pack.hpp"
#include "data/modern_pixel_reconstruction.hpp"
#include "data/sha256.hpp"
#include "data/reference_trace.hpp"
#include "data/reference_trace_registry.hpp"
#include "data/function_map.hpp"
#include "data/recovery_map.hpp"
#include "data/runtime_diagnostics.hpp"
#include "data/startup_boundary.hpp"
#include "data/static_control_flow.hpp"
#include "data/zip_archive.hpp"
#include "platform/game_data.hpp"
#include "platform/platform_coverage.hpp"
#include "display_geometry.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

// A Modern pack never becomes an active renderer input merely because a path
// was picked.  This session-local state records the result of the strict,
// hash-bound preflight; the loader rehashes again immediately before decoding
// any PNG bytes.
enum class ModernPackAdmission {
    unselected,
    ready,
    rejected,
};

// SDL's native file-dialog callback may run after the initiating UI event and
// on another thread. Keep its mailbox alive for the process lifetime: the
// callback does no renderer, media, or filesystem work, and a late OS
// callback cannot dereference a destroyed launcher object during shutdown.
struct ModernPackDialogMailbox {
    std::mutex mutex;
    std::optional<std::filesystem::path> pending_selection;
    // SDL retains the filter pointers until the asynchronous native dialog
    // completes. Keep both the translated label and its descriptor in the
    // process-lifetime mailbox rather than handing SDL a temporary string.
    std::string filter_label;
    SDL_DialogFileFilter filter{};
    bool dialog_open = false;
};

ModernPackDialogMailbox& modern_pack_dialog_mailbox() {
    static auto* mailbox = new ModernPackDialogMailbox;
    return *mailbox;
}

void SDLCALL receive_modern_pack_dialog_selection(void* userdata,
    const char* const* filelist, int) {
    auto& mailbox = *static_cast<ModernPackDialogMailbox*>(userdata);
    std::lock_guard lock(mailbox.mutex);
    // One explicit manifest is the whole UI contract. A filter is only a
    // convenience supplied to the native dialog; its result remains untrusted
    // until the existing strict manifest/release/PNG validators consume it.
    if (filelist && filelist[0] && !filelist[1]) {
        mailbox.pending_selection = std::filesystem::path(filelist[0]);
    }
    mailbox.dialog_open = false;
}

// Original-data selection is a launcher-only, read-only operation.  Both
// supported source shapes get an explicit native route because SDL's folder
// and file dialogs deliberately have distinct contracts.  Keep the mailbox
// alive for the same reason as the Modern-pack dialog: native SDL dialogs are
// asynchronous and may complete after the UI event that opened them.  The
// callback merely transfers one untrusted path and its requested shape to the
// main thread.
enum class OriginalDataSourceDialogKind {
    directory,
    archive,
};

struct OriginalDataSourceSelection {
    std::filesystem::path path;
    OriginalDataSourceDialogKind kind;
};

struct OriginalDataSourceDialogMailbox {
    std::mutex mutex;
    std::optional<OriginalDataSourceSelection> pending_selection;
    OriginalDataSourceDialogKind requested_kind = OriginalDataSourceDialogKind::directory;
    bool dialog_open = false;
};

OriginalDataSourceDialogMailbox& original_data_source_dialog_mailbox() {
    static auto* mailbox = new OriginalDataSourceDialogMailbox;
    return *mailbox;
}

void SDLCALL receive_original_data_source_dialog_selection(void* userdata,
    const char* const* filelist, int) {
    auto& mailbox = *static_cast<OriginalDataSourceDialogMailbox*>(userdata);
    std::lock_guard lock(mailbox.mutex);
    if (filelist && filelist[0] && !filelist[1]) {
        mailbox.pending_selection = OriginalDataSourceSelection{
            std::filesystem::path(filelist[0]), mailbox.requested_kind};
    }
    mailbox.dialog_open = false;
}

[[nodiscard]] std::vector<std::uint8_t> read_save_for_inspection(
    const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || error) {
        throw std::runtime_error("Save inspection input is not a regular file");
    }
    const auto size = std::filesystem::file_size(path, error);
    if (error || size != eon::MillenniumDosSaveLayout::serialized_size) {
        throw std::runtime_error("Unsupported Millennium DOS save size");
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Unable to read save inspection input");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (stream.gcount() != static_cast<std::streamsize>(bytes.size())) {
        throw std::runtime_error("Truncated save inspection input");
    }
    return bytes;
}

[[nodiscard]] bool path_is_within(const std::filesystem::path& child,
    const std::filesystem::path& parent) {
    const auto relative = child.lexically_relative(parent);
    if (relative.empty()) return child == parent;
    for (const auto& component : relative) {
        if (component == "..") return false;
    }
    return !relative.is_absolute();
}

// Do not query the host system temporary directory merely to reject it: that
// would make the preservation-sidecar path itself depend on /tmp. Check both
// the operator spelling and the canonical result, because macOS canonically
// represents /tmp under /private/tmp.
[[nodiscard]] bool path_is_system_temporary(const std::filesystem::path& path) {
    const auto normalized = path.generic_string();
    return normalized == "/tmp" || normalized.starts_with("/tmp/")
        || normalized == "/private/tmp" || normalized.starts_with("/private/tmp/");
}

// The sidecar is deliberately not a game-data source. This bounded reader
// accepts an explicitly named, external evidence file after resolving its
// physical target, and returns only parser aggregate metadata. In particular,
// its bytes do not enter a release scanner, runtime session, or renderer.
[[nodiscard]] eon::StaticControlFlowSummary read_static_control_flow_sidecar(
    const std::filesystem::path& supplied_path) {
    if (!supplied_path.is_absolute()) {
        throw std::runtime_error("Static control-flow sidecar must be an absolute path");
    }
    if (path_is_system_temporary(supplied_path)) {
        throw std::runtime_error("Static control-flow sidecar must remain outside the repository and /tmp");
    }
    std::error_code error;
    const auto supplied_status = std::filesystem::symlink_status(supplied_path, error);
    if (error || std::filesystem::is_symlink(supplied_status)
        || !std::filesystem::is_regular_file(supplied_status)) {
        throw std::runtime_error("Static control-flow sidecar must be an existing non-symlink regular file");
    }
    const auto resolved_path = std::filesystem::canonical(supplied_path, error);
    if (error || !resolved_path.is_absolute()) {
        throw std::runtime_error("Unable to resolve static control-flow sidecar path");
    }
    const auto source_root = std::filesystem::canonical(EON_SOURCE_DIR, error);
    if (error) throw std::runtime_error("Unable to resolve Project Eon source root");
    if (path_is_within(resolved_path, source_root)) {
        throw std::runtime_error("Static control-flow sidecar must remain outside the repository and /tmp");
    }
    if (path_is_system_temporary(resolved_path)) {
        throw std::runtime_error("Static control-flow sidecar must remain outside the repository and /tmp");
    }
    const auto size = std::filesystem::file_size(resolved_path, error);
    constexpr std::uintmax_t maximum_bytes = 32U * 1024U * 1024U;
    if (error || size == 0U || size > maximum_bytes) {
        throw std::runtime_error("Static control-flow sidecar exceeds bounded parser limit");
    }
    std::ifstream stream(resolved_path, std::ios::binary);
    if (!stream) throw std::runtime_error("Unable to open static control-flow sidecar");
    std::string json(static_cast<std::size_t>(size), '\0');
    stream.read(json.data(), static_cast<std::streamsize>(json.size()));
    if (stream.gcount() != static_cast<std::streamsize>(json.size())) {
        throw std::runtime_error("Static control-flow sidecar changed during bounded read");
    }
    const auto final_status = std::filesystem::symlink_status(resolved_path, error);
    if (error || std::filesystem::is_symlink(final_status)
        || !std::filesystem::is_regular_file(final_status)
        || std::filesystem::file_size(resolved_path, error) != size || error) {
        throw std::runtime_error("Static control-flow sidecar changed during bounded read");
    }
    return eon::parse_static_control_flow_sidecar(json);
}

struct StaticControlFlowInspection {
    eon::StaticControlFlowSummary summary;
    eon::FunctionMapSidecarCoverage function_map_coverage;
    std::vector<std::pair<const eon::ReleaseArchive*, std::size_t>> release_bindings;
};

[[nodiscard]] StaticControlFlowInspection bind_static_control_flow_sidecar(
    eon::StaticControlFlowSummary summary, const std::vector<eon::ReleaseArchive>& releases) {
    std::map<std::string, const eon::ReleaseArchive*, std::less<>> release_by_hash;
    for (const auto& release : releases) release_by_hash.emplace(release.sha256, &release);
    StaticControlFlowInspection inspection{std::move(summary), {}, {}};
    for (const auto& [archive_sha256, document_count] : inspection.summary.release_document_counts) {
        if (!release_by_hash.contains(archive_sha256)) {
            throw std::runtime_error("Static control-flow sidecar contains a document not bound to a reverified inspected release");
        }
        inspection.release_bindings.emplace_back(release_by_hash.at(archive_sha256), document_count);
    }
    for (const auto& document : inspection.summary.documents) {
        if (!document.direct_media_set_sha256) continue;
        const auto direct_source = std::find_if(releases.begin(), releases.end(), [&](const auto& release) {
            const auto set = eon::direct_media_set_sha256(release);
            return release.sha256 == document.release_sha256 && set
                && *set == *document.direct_media_set_sha256;
        });
        if (direct_source == releases.end()) {
            throw std::runtime_error("Direct static control-flow document is not bound to a reverified inspected direct-media set");
        }
        // The scanner may have completed before the operator supplies this
        // external sidecar. Reopen every declared direct member now, so a
        // changed directory cannot retain a prior scan's set identity merely
        // by reusing the same logical release hash in sidecar metadata.
        try {
            eon::verify_release_archive(*direct_source);
        } catch (const std::exception&) {
            throw std::runtime_error("Direct static control-flow document source changed before inspection");
        }
    }
    inspection.function_map_coverage = eon::function_map_sidecar_coverage(inspection.summary);
    return inspection;
}

int report_millennium_dos_save_inspection(
    const std::span<const std::uint8_t> bytes, const std::string_view source_description) {
    const eon::MillenniumDosSaveSession save(bytes);
    std::cout << "SAVE INSPECTION  read-only; source stays in place\n"
        << "          Millennium 2.2 / DOS structure: version 0x" << std::hex
        << save.layout().version << std::dec << ", " << save.serialized_bytes().size()
        << " bytes, " << save.layout().state_table.size()
        << " recovered positional records\n          source " << save.sha256() << '\n'
        << "          source path: " << source_description << '\n';
    try {
        static_cast<void>(eon::authenticate_millennium_save(
            eon::MillenniumSavePlatform::dos, "2200SAVE.I", save.serialized_bytes()));
        std::cout << "          reference identity: supplied English DOS 2200SAVE.I\n";
    } catch (const std::runtime_error&) {
        std::cout << "          reference identity: not present in supplied media; "
                     "structure-only observation, never imported into runtime\n";
    }
    // The executable reconstructs these four columns into 38 records.
    // Print the verified positional fields rather than assigning game
    // meanings, so a user can compare a supplied original save without
    // Eon copying, modifying, or attempting to load it into a runtime.
    for (std::size_t index = 0; index < save.layout().state_table.size(); ++index) {
        const auto& record = save.state_record(index);
        std::cout << "          [" << index << "] +00=0x" << std::hex
            << record.runtime_offset_0 << " +04=0x" << record.runtime_offset_4
            << " +06=0x" << record.runtime_offset_6 << " +08=0x"
            << record.runtime_offset_8 << std::dec << '\n';
    }
    return 0;
}

int inspect_millennium_dos_save(const std::filesystem::path& path) {
    try {
        std::error_code error;
        if (!std::filesystem::is_regular_file(path, error) || error) {
            throw std::runtime_error("Save inspection input is not a regular file");
        }
        const auto size = std::filesystem::file_size(path, error);
        if (error) throw std::runtime_error("Unable to determine save inspection input size");
        if (size == eon::MillenniumDosSaveLayout::serialized_size) {
            const auto bytes = read_save_for_inspection(path);
            return report_millennium_dos_save_inspection(bytes, path.string());
        }
        // An archive is admitted only after its full hash selects the English
        // DOS release. The save is then read directly in memory: no unpack,
        // copy, or filename-based archive lookup is permitted.
        eon::ReleaseScanner scanner(path);
        while (!scanner.advance(64)) {
        }
        const auto& releases = scanner.releases();
        std::vector<const eon::ReleaseArchive*> english_dos_releases;
        for (const auto& candidate : releases) {
            if (candidate.game == eon::Game::millennium
                && candidate.platform == eon::Platform::dos && candidate.language == "en") {
                english_dos_releases.push_back(&candidate);
            }
        }
        if (english_dos_releases.size() != 1) {
            throw std::runtime_error("Save inspection requires a 9,538-byte DOS save or exactly one verified English Millennium DOS archive");
        }
        const auto* release = english_dos_releases.front();
        eon::verify_release_archive(*release);
        constexpr std::string_view save_sha256 =
            "a9b3d77534d3d575012f9553bfed9520edf92a83af408c977e7f0fd226a470e7";
        const auto bytes = eon::extract_verified_release_asset(*release, save_sha256);
        if (!bytes) throw std::runtime_error("Verified English DOS archive has no expected 2200SAVE.I");
        return report_millennium_dos_save_inspection(*bytes,
            "verified English Millennium DOS archive " + release->sha256);
    } catch (const std::exception& error) {
        std::cerr << "Save inspection rejected: " << error.what() << '\n';
        return 6;
    }
}

enum class Screen { menu, launching };
using LauncherPage = eon::LauncherPage;

const eon::Translator* active_translator = nullptr;
std::unique_ptr<eon::UnicodeTextRenderer> active_text_renderer;

struct Card {
    eon::Game game;
    const char* title;
    const char* subtitle;
    const char* filename;
    SDL_FRect bounds;
    SDL_Texture* texture = nullptr;
};

struct PlatformCard {
    eon::Platform platform;
    const char* title;
    const char* filename;
    SDL_FRect bounds;
    SDL_Texture* texture = nullptr;
};

struct ReleaseLanguageCard {
    // This remains the index in the complete, SHA-sorted identity list even
    // when the renderer is showing one page of release cards.  Pointer and
    // touch activation must pass this value to the SDL-free controller rather
    // than treating a page-local position as a different original release.
    std::size_t identity_index = 0;
    std::string language;
    std::string sha256;
    SDL_FRect bounds;
    // Borrowed from the selected generated platform card. Release identity
    // has no original-art dependency and must not own another SDL texture.
    SDL_Texture* texture = nullptr;
};

enum class ProfileChoice { original, modern, custom };

struct ProfileCard {
    ProfileChoice choice;
    const char* title;
    const char* subtitle;
    const char* filename;
    SDL_FRect bounds;
    SDL_Texture* texture = nullptr;
};

// Modern options are renderer state only. They are deliberately independent
// from original input, media, simulation state and save bytes.
enum class ModernGraphicsPreset { clean, crt, cinematic, high_contrast, custom };
using PixelReconstruction = eon::ModernPixelReconstruction;
// Presentation scheduling is deliberately separate from the recovered clock.
// It controls when SDL presents already-rendered pixels; it never supplies a
// tick, input, or elapsed-time value to an original game path.
enum class RenderPacing { vsync, capped_120, uncapped };

struct ModernGraphicsSettings {
    bool smooth_scaling = true;
    // Reconstruct an edge-aware 2x renderer texture from a decoded original
    // surface. The source vector is retained unchanged and the result exists
    // only in process memory; Original never takes this path.
    PixelReconstruction pixel_reconstruction = PixelReconstruction::scale2x;
    bool scanlines = false;
    bool frame = true;
    bool reduced_motion = false;
    // Presets only select combinations of the renderer controls below.  They
    // are never serialized into a supplied save or sent to a recovered VM.
    ModernGraphicsPreset preset = ModernGraphicsPreset::clean;
    // The selected output mode controls only the SDL window.  Original frame
    // dimensions, indexed pixels and simulation state remain unchanged.
    std::size_t output_resolution_index = 0;
    std::size_t aspect_ratio_index = 0;
    RenderPacing render_pacing = RenderPacing::vsync;
    int focused_option = 0;
};

struct OutputResolution {
    int width = 1280;
    int height = 720;
};

constexpr std::array<OutputResolution, 3> output_resolutions{{
    {1280, 720}, {1600, 900}, {1920, 1080},
}};

// Every option has an explicit display ratio.  The renderer derives both
// viewport dimensions from the same ratio so it never independently stretches
// width and height into an accidental, visually odd shape.
constexpr std::array<float, 3> display_aspect_ratios{{4.0F / 3.0F, 8.0F / 5.0F, 16.0F / 9.0F}};
constexpr std::array<const char*, 3> display_aspect_names{{
    "ORIGINAL 4:3", "SQUARE PIXELS 8:5", "WIDESCREEN 16:9",
}};
// Stable machine-readable counterparts of the translated UI labels. These
// describe renderer geometry only and are safe to emit in launch diagnostics.
constexpr std::array<const char*, 3> display_aspect_identifiers{{
    "original", "square-pixels", "widescreen",
}};

constexpr std::array<const char*, 5> modern_graphics_preset_names{{
    "CLEAN", "CRT", "CINEMATIC", "HIGH CONTRAST", "CUSTOM",
}};
constexpr std::array<const char*, 3> render_pacing_names{{
    "VSYNC (DISPLAY)", "120 FPS (RENDER ONLY)", "UNCAPPED (RENDER ONLY)",
}};
constexpr int original_display_option_count = 2;
constexpr int modern_graphics_option_count = 11;

// These renderer-space bounds are shared by drawing and touch handling.  A
// Custom profile must remain usable on an iPad even when no hardware F10 key
// is attached; they are Eon's own chrome, never a game input surface.
constexpr SDL_FRect modern_graphics_popup_bounds{356, 32, 568, 680};
constexpr float modern_graphics_option_first_baseline = 272.0F;
constexpr float modern_graphics_option_stride = 38.0F;

// This is intentionally a presentation-only preset table.  It has no access
// to game state, media bytes, input state or saves.  Changing a single
// renderer option marks the selection Custom; selecting a named profile
// restores its complete, explicit renderer combination.
void apply_modern_graphics_preset(ModernGraphicsSettings& settings,
    const ModernGraphicsPreset preset) {
    settings.preset = preset;
    switch (preset) {
    case ModernGraphicsPreset::clean:
        settings.pixel_reconstruction = PixelReconstruction::scale2x;
        settings.smooth_scaling = true;
        settings.scanlines = false;
        settings.frame = true;
        break;
    case ModernGraphicsPreset::crt:
        settings.pixel_reconstruction = PixelReconstruction::off;
        settings.smooth_scaling = false;
        settings.scanlines = true;
        settings.frame = true;
        break;
    case ModernGraphicsPreset::cinematic:
        settings.pixel_reconstruction = PixelReconstruction::scale2x;
        settings.smooth_scaling = true;
        settings.scanlines = false;
        settings.frame = false;
        break;
    case ModernGraphicsPreset::high_contrast:
        settings.pixel_reconstruction = PixelReconstruction::scale2x;
        settings.smooth_scaling = false;
        settings.scanlines = false;
        settings.frame = true;
        break;
    case ModernGraphicsPreset::custom:
        break;
    }
}

void mark_modern_graphics_custom(ModernGraphicsSettings& settings) {
    settings.preset = ModernGraphicsPreset::custom;
}

void cycle_modern_graphics_preset(ModernGraphicsSettings& settings, const int direction) {
    constexpr auto count = static_cast<int>(ModernGraphicsPreset::custom) + 1;
    const auto current = static_cast<int>(settings.preset);
    const auto next = direction < 0 ? (current + count - 1) % count : (current + 1) % count;
    apply_modern_graphics_preset(settings, static_cast<ModernGraphicsPreset>(next));
}

// This readout is deliberately derived from already admitted launcher state.
// It is neither a guest debugger nor a source of data for a recovered game
// session.  In particular, a reference trace is only "admitted" after the
// separate CLI validator has checked its complete external manifest; the UI
// does not open, retain, replay, or infer a trace behind an active game.
struct ModernRuntimeDiagnostics {
    // The SDL/F10 layer receives this aggregate only from an already admitted
    // launcher-owned diagnostics snapshot. It must never open a sidecar (or
    // retain its path), and absence is distinct from a zero-candidate result.
    struct StaticControlFlow {
        std::size_t document_count = 0;
        std::size_t range_count = 0;
        std::size_t candidate_count = 0;
    };
    // This is a named, declarative function-map view over the same
    // hash-checked recovery boundaries that the CLI reports.  It is copied
    // from the compiled map solely for presentation: none of these labels or
    // addresses is an executable guest dispatch target.
    struct RecoveryFunction {
        std::string id;
        std::string profile;
        std::string cpu;
        std::string source_asset_sha256;
        std::string source_span_sha256;
        std::string source_offset;
        std::string runtime_address;
        std::string address_space;
        std::string evidence_level;
        std::string uncertainty;
        std::string runtime_status;
    };
    std::string release_identity;
    std::string runtime_admission = "NOT SELECTED";
    std::string runtime_rejection = "NONE";
    // Controller-owned lifecycle state, deliberately separate from the
    // coordinator snapshot so diagnostics expose revocation in progress.
    std::string lifecycle_state = "MENU";
    // These originate exclusively from ReleaseRuntimeCoordinator's admitted
    // session snapshot. They never reconstruct session state from launcher
    // focus or add an SDL-side media/input path.
    std::string session_adapter = "NOT SELECTED";
    std::string session_boundary = "—";
    std::string session_capabilities = "—";
    std::string recovery_coverage = "—";
    // The first hash-checked address is a preservation navigation marker,
    // not a request to execute, emulate, or hook original machine code.
    std::string startup_boundary = "—";
    std::size_t recovery_boundary_count = 0;
    std::vector<RecoveryFunction> recovery_functions;
    std::string trace_admission = "NOT LOADED";
    std::optional<StaticControlFlow> static_control_flow;
    // The live runtime provides only this static, hash-bound table summary.
    // It is deliberately absent after title handoff/revocation and cannot be
    // interpreted as an active input mapping or executed game path.
    std::string millennium_dos_static_dispatch;
    std::string millennium_dos_owned_function;
    std::string deuteros_amiga_title_dependency_chain;
    std::string native_code_images;
    // This comes only from the launcher preflight object. It does not expose
    // a local path, decode an external asset, or imply the renderer loaded it.
    std::string modern_pack = "NOT SELECTED";
    std::string modern_pack_targets = "—";
};

[[nodiscard]] std::string static_control_flow_diagnostics_summary(
    const std::optional<ModernRuntimeDiagnostics::StaticControlFlow>& static_control_flow,
    const eon::Translator& translator) {
    if (!static_control_flow) {
        return std::string(translator.translate("UNAVAILABLE (CLI INSPECTION ONLY)"));
    }
    // These counts came from metadata already admitted by the launcher. They
    // are never decoded instructions, addresses, sidecar names, paths, or
    // original-media bytes.
    return "DOCUMENTS=" + std::to_string(static_control_flow->document_count)
        + " / RANGES=" + std::to_string(static_control_flow->range_count)
        + " / CANDIDATES=" + std::to_string(static_control_flow->candidate_count);
}

[[nodiscard]] std::string modern_pack_renderer_targets_summary(
    const eon::ModernAssetPackRendererTargets& targets) {
    std::vector<std::string> entries;
    if (targets.millennium_dos_title_640x400 || targets.millennium_dos_title_1280x800) {
        entries.push_back("TITLE 640/1280="
            + std::string(targets.millennium_dos_title_640x400 ? "Y" : "N")
            + "/" + std::string(targets.millennium_dos_title_1280x800 ? "Y" : "N"));
    }
    if (targets.deuteros_amiga_opening_640x400_frames
        || targets.deuteros_amiga_opening_1280x800_frames) {
        entries.push_back("OPENING 640/1280="
            + std::to_string(targets.deuteros_amiga_opening_640x400_frames) + "/"
            + std::to_string(eon::deuteros_amiga_held_opening_frame_count) + ","
            + std::to_string(targets.deuteros_amiga_opening_1280x800_frames) + "/"
            + std::to_string(eon::deuteros_amiga_held_opening_frame_count));
    }
    if (entries.empty()) return "—";
    std::string summary = entries.front();
    for (std::size_t index = 1; index < entries.size(); ++index) summary += "; " + entries[index];
    return summary;
}

[[nodiscard]] std::string truncated_diagnostic_value(const std::string_view value,
    const std::size_t maximum_characters = 22U) {
    if (value.size() <= maximum_characters) return std::string(value);
    return std::string(value.substr(0, maximum_characters - 1U)) + "…";
}

[[nodiscard]] std::string truncated_identity_hash(const std::string_view hash) {
    // The launcher needs enough identity to correlate an on-screen readout
    // with a preservation report, but never needs to turn the whole digest
    // into a UI label. Keep both ends to make accidental prefix collisions
    // visibly distinguishable without crowding a 1280x720 modal.
    if (hash.size() <= 16) return std::string(hash);
    return std::string(hash.substr(0, 12)) + "…" + std::string(hash.substr(hash.size() - 4));
}

std::size_t output_resolution_index_for(const eon::DisplayPreferences& display) {
    for (std::size_t index = 0; index < output_resolutions.size(); ++index) {
        if (output_resolutions[index].width == display.width
            && output_resolutions[index].height == display.height) return index;
    }
    throw std::runtime_error("Unsupported validated display resolution");
}

void draw_text(SDL_Renderer* renderer, float x, float y, const std::string& text) {
    const auto translated = active_translator ? active_translator->translate(text) : std::string_view(text);
    const std::string localized(translated);
    if (active_text_renderer && active_text_renderer->draw(x, y, localized)) return;
    // SDL_RenderDebugText is an emergency diagnostic only. SDL3 documents it
    // as ASCII-only; a renderer setup failure must not replace localized UTF-8
    // with transliterations or synthetic text.
    SDL_RenderDebugText(renderer, x, y, localized.c_str());
}

void draw_original_text(SDL_Renderer* renderer, const float x, const float y,
    const std::string_view text) {
    // Unlike launcher chrome, this is verified source text from the user's
    // supplied media. Localisation must never translate or replace it.
    const std::string original(text);
    if (active_text_renderer && active_text_renderer->draw(x, y, original)) return;
    SDL_RenderDebugText(renderer, x, y, original.c_str());
}

void draw_original_multiline_text(SDL_Renderer* renderer, const float x, float y,
    const std::string_view text, const float line_stride = 22.0F) {
    std::size_t line_start = 0;
    while (line_start <= text.size()) {
        const auto line_end = text.find('\n', line_start);
        const auto count = (line_end == std::string_view::npos ? text.size() : line_end) - line_start;
        auto line = text.substr(line_start, count);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        draw_original_text(renderer, x, y, line);
        y += line_stride;
        if (line_end == std::string_view::npos) return;
        line_start = line_end + 1;
    }
}

// These are launcher labels, rather than names read from original media. Keep
// the CLI/provenance names in game_data untouched: only rendered launcher UI
// passes through the catalog.
std::string_view launcher_game_label(const eon::Game game) {
    return game == eon::Game::millennium ? "MILLENNIUM 2.2" : "DEUTEROS";
}

std::string_view launcher_platform_label(const eon::Platform platform) {
    switch (platform) {
    case eon::Platform::dos: return "DOS";
    case eon::Platform::amiga: return "AMIGA";
    case eon::Platform::atari_st: return "ATARI ST";
    }
    return "UNKNOWN PLATFORM";
}

// Modern presentation is deliberately renderer-only. It frames original
// decoded surfaces and may derive a transient, opt-in in-memory Scale2x
// texture from them; it neither changes an input, simulation, save byte, nor
// writes or substitutes supplied media.
void draw_modern_chrome(SDL_Renderer* renderer) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 9, 28, 48, 235);
    SDL_FRect header{32, 24, 1216, 112};
    SDL_RenderFillRect(renderer, &header);
    SDL_SetRenderDrawColor(renderer, 39, 202, 213, 210);
    SDL_RenderRect(renderer, &header);
    SDL_SetRenderDrawColor(renderer, 19, 86, 116, 110);
    for (int x = 48; x < 1248; x += 48) {
        SDL_RenderLine(renderer, static_cast<float>(x), 144, static_cast<float>(x), 696);
    }
    for (int y = 168; y < 696; y += 48) {
        SDL_RenderLine(renderer, 32, static_cast<float>(y), 1248, static_cast<float>(y));
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void draw_modern_surface_frame(SDL_Renderer* renderer, const SDL_FRect& bounds) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 4, 13, 25, 210);
    SDL_FRect shadow{bounds.x - 12, bounds.y - 12, bounds.w + 24, bounds.h + 24};
    SDL_RenderFillRect(renderer, &shadow);
    SDL_SetRenderDrawColor(renderer, 39, 202, 213, 235);
    SDL_RenderRect(renderer, &shadow);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void draw_scanlines(SDL_Renderer* renderer, const SDL_FRect& bounds) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 56);
    for (float y = bounds.y + 1; y < bounds.y + bounds.h; y += 2) {
        SDL_RenderLine(renderer, bounds.x, y, bounds.x + bounds.w, y);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

// These two looks deliberately operate after the original or separately
// admitted Modern texture has been rendered.  They are transient SDL draw
// operations, not colour changes to a decoded original surface or a pack.
// The named profiles are presentation looks rather than accessibility claims:
// users retain Custom controls and Original never calls this function.
void draw_modern_preset_overlay(SDL_Renderer* renderer, const SDL_FRect& bounds,
    const ModernGraphicsPreset preset, const bool reduced_motion) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    if (preset == ModernGraphicsPreset::cinematic && !reduced_motion) {
        // A restrained warm wash plus a vignette makes this profile visibly
        // distinct from Clean without touching any original texture bytes.
        SDL_SetRenderDrawColor(renderer, 106, 62, 18, 24);
        SDL_RenderFillRect(renderer, &bounds);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 44);
        constexpr float edge = 18.0F;
        const SDL_FRect top{bounds.x, bounds.y, bounds.w, edge};
        const SDL_FRect bottom{bounds.x, bounds.y + bounds.h - edge, bounds.w, edge};
        const SDL_FRect left{bounds.x, bounds.y, edge, bounds.h};
        const SDL_FRect right{bounds.x + bounds.w - edge, bounds.y, edge, bounds.h};
        SDL_RenderFillRect(renderer, &top);
        SDL_RenderFillRect(renderer, &bottom);
        SDL_RenderFillRect(renderer, &left);
        SDL_RenderFillRect(renderer, &right);
    } else if (preset == ModernGraphicsPreset::high_contrast) {
        // A crisp black surround and bright inner keyline improve separation
        // against the Modern chrome while leaving all source pixels alone.
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 92);
        SDL_FRect surround{bounds.x - 5, bounds.y - 5, bounds.w + 10, bounds.h + 10};
        SDL_RenderRect(renderer, &surround);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 140);
        SDL_RenderRect(renderer, &bounds);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

// This report is intentionally shared by every platform inspector. The map
// has already been bounded by the rehashed outer archive and parser-profile
// manifest; printing it neither extracts more media nor dispatches any guest
// code. It is a diagnostic equivalent of a symbol map, not a hook table.
void report_recovery_map(const eon::ReleaseArchive& release) {
    const auto diagnostics = eon::runtime_diagnostics_for_release(release);
    const auto& entries = diagnostics.recovery_boundaries;
    if (entries.empty()) return;
    std::cout << "          RECOVERY MAP  " << entries.size()
        << " hash-bound static path" << (entries.size() == 1 ? "" : "s") << '\n';
    for (const auto& entry : entries) {
        std::cout << "            " << entry.id << ": profile " << entry.parser_profile_id
            << ", " << entry.cpu << " " << entry.source_address << ", "
            << entry.evidence_level << "; " << entry.runtime_status << '\n';
    }
}

// The startup marker gives preservation work a stable first address to cite
// after an archive has been rehashed. It deliberately retains the next hard
// boundary: `--inspect` must not make a static observation appear executable.
void report_startup_boundary(const eon::ReleaseArchive& release) {
    const auto boundary = eon::runtime_diagnostics_for_release(release).startup_boundary;
    if (!boundary) return;
    std::cout << "          STARTUP BOUNDARY  " << boundary->parser_profile_id
        << " at " << boundary->source_address << "; stops before "
        << boundary->unresolved << '\n';
}

void write_json_string(std::ostream& output, const std::string_view value) {
    output << '"';
    for (const unsigned char character : value) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (character < 0x20U) {
                constexpr char hexadecimal[] = "0123456789abcdef";
                output << "\\u00" << hexadecimal[character >> 4U]
                    << hexadecimal[character & 0x0fU];
            } else {
                output << static_cast<char>(character);
            }
        }
    }
    output << '"';
}

// This is intentionally a compact release-level API, not an inventory
// export. It serializes only facts already admitted by the scanner and never
// includes a path, filename, archive member, asset byte, or user-supplied
// directory. Consumers can therefore preserve/review identity and boundaries
// without turning a report into a media catalogue.
void report_inspection_json(const std::vector<eon::ReleaseArchive>& releases,
    const eon::ReleaseScanSnapshot& scan_snapshot,
    const std::optional<StaticControlFlowInspection>& static_control_flow = std::nullopt) {
    const auto& scan = scan_snapshot.report;
    std::cout << "{\"schema\":\"project-eon.inspect/v1\",\"releases\":[";
    for (std::size_t index = 0; index < releases.size(); ++index) {
        if (index != 0) std::cout << ',';
        const auto& release = releases[index];
        const auto diagnostics = eon::runtime_diagnostics_for_release(release);
        std::cout << "{\"game\":"; write_json_string(std::cout, eon::name(release.game));
        std::cout << ",\"platform\":"; write_json_string(std::cout, eon::name(release.platform));
        std::cout << ",\"language\":"; write_json_string(std::cout, release.language);
        std::cout << ",\"sha256\":"; write_json_string(std::cout, release.sha256);
        std::cout << ",\"media_layout\":";
        switch (release.layout) {
        case eon::ReleaseMediaLayout::zip_archive:
            write_json_string(std::cout, "zip-archive"); break;
        case eon::ReleaseMediaLayout::verified_directory:
            write_json_string(std::cout, "verified-directory"); break;
        case eon::ReleaseMediaLayout::verified_container_set:
            write_json_string(std::cout, "verified-container-set"); break;
        }
        std::cout << ",\"direct_set_sha256\":";
        if (const auto direct_set = eon::direct_media_set_sha256(release)) {
            write_json_string(std::cout, *direct_set);
        } else {
            std::cout << "null";
        }
        std::cout << ",\"coverage\":";
        write_json_string(std::cout, eon::name(diagnostics.coverage));
        std::cout << ",\"startup_boundary\":";
        if (const auto& startup = diagnostics.startup_boundary) {
            std::cout << "{\"profile\":"; write_json_string(std::cout, startup->parser_profile_id);
            std::cout << ",\"source_address\":"; write_json_string(std::cout, startup->source_address);
            std::cout << ",\"unresolved\":"; write_json_string(std::cout, startup->unresolved);
            std::cout << '}';
        } else {
            std::cout << "null";
        }
        std::cout << ",\"recovery_boundaries\":[";
        const auto& boundaries = diagnostics.recovery_boundaries;
        for (std::size_t boundary_index = 0; boundary_index < boundaries.size(); ++boundary_index) {
            if (boundary_index != 0) std::cout << ',';
            const auto& boundary = boundaries[boundary_index];
            std::cout << "{\"id\":"; write_json_string(std::cout, boundary.id);
            std::cout << ",\"profile\":"; write_json_string(std::cout, boundary.parser_profile_id);
            std::cout << ",\"cpu\":"; write_json_string(std::cout, boundary.cpu);
            std::cout << ",\"source_address\":"; write_json_string(std::cout, boundary.source_address);
            std::cout << ",\"evidence_level\":"; write_json_string(std::cout, boundary.evidence_level);
            std::cout << ",\"runtime_status\":"; write_json_string(std::cout, boundary.runtime_status);
            std::cout << ",\"documentation_anchor\":";
            write_json_string(std::cout, boundary.documentation_anchor);
            std::cout << '}';
        }
        std::cout << "],\"function_map\":[";
        const auto& functions = diagnostics.functions;
        bool first_function = true;
        for (const auto& function : functions) {
            if (!first_function) std::cout << ',';
            first_function = false;
            std::cout << "{\"id\":"; write_json_string(std::cout, function.id);
            std::cout << ",\"profile\":"; write_json_string(std::cout, function.parser_profile_id);
            std::cout << ",\"cpu\":"; write_json_string(std::cout, function.cpu);
            std::cout << ",\"source_asset_sha256\":";
            write_json_string(std::cout, function.source_asset_sha256);
            std::cout << ",\"source_span_sha256\":";
            write_json_string(std::cout, function.source_span_sha256.empty()
                ? function.source_asset_sha256 : function.source_span_sha256);
            std::cout << ",\"source_offset\":"; write_json_string(std::cout, function.source_offset);
            std::cout << ",\"runtime_address\":"; write_json_string(std::cout, function.runtime_address);
            std::cout << ",\"address_space\":"; write_json_string(std::cout, function.address_space);
            std::cout << ",\"evidence_level\":"; write_json_string(std::cout, function.evidence_level);
            std::cout << ",\"uncertainty\":"; write_json_string(std::cout, function.uncertainty);
            std::cout << ",\"runtime_status\":"; write_json_string(std::cout, function.runtime_status);
            std::cout << ",\"documentation_anchor\":";
            write_json_string(std::cout, function.documentation_anchor);
            std::cout << '}';
        }
        std::cout << "]}";
    }
    std::cout << "]";
    if (static_control_flow) {
        const auto& summary = static_control_flow->summary;
        std::cout << ",\"static_control_flow\":{\"classification\":";
        write_json_string(std::cout, "static-candidate-unclassified");
        std::cout << ",\"documents\":" << summary.document_count
            << ",\"ranges\":" << summary.range_count
            << ",\"candidates\":" << summary.edge_count
            << ",\"declared_bytes\":" << summary.declared_byte_count
            << ",\"function_map_entries_for_bound_releases\":"
            << static_control_flow->function_map_coverage.function_entry_count
            << ",\"function_map_direct_range_bindings\":"
            << static_control_flow->function_map_coverage.direct_range_binding_count
            << ",\"function_map_not_declared_by_sidecar\":"
            << static_control_flow->function_map_coverage.not_declared_by_sidecar_count
            << ",\"release_bindings\":[";
        for (std::size_t index = 0; index < static_control_flow->release_bindings.size(); ++index) {
            if (index != 0) std::cout << ',';
            const auto& [release, document_count] = static_control_flow->release_bindings[index];
            std::cout << "{\"game\":"; write_json_string(std::cout, eon::name(release->game));
            std::cout << ",\"platform\":"; write_json_string(std::cout, eon::name(release->platform));
            std::cout << ",\"language\":"; write_json_string(std::cout, release->language);
            std::cout << ",\"sha256\":"; write_json_string(std::cout, release->sha256);
            std::cout << ",\"documents\":" << document_count << '}';
        }
        std::cout << "]}";
    }
    std::cout << ",\"scan\":{\"source_kind\":";
    write_json_string(std::cout, eon::name(scan_snapshot.source_kind));
    std::cout << ",\"discovering\":" << (scan_snapshot.discovering ? "true" : "false")
        << ",\"complete\":" << (scan_snapshot.complete ? "true" : "false")
        << ",\"scanned_candidates\":" << scan_snapshot.scanned_count
        << ",\"unique_releases\":" << scan_snapshot.unique_release_count
        << ",\"unique_unbound_direct_media\":"
        << scan_snapshot.unique_unbound_direct_media_count;
    std::cout << ",\"candidates\":" << scan_snapshot.candidate_count
        << ",\"size_rejected_candidates\":" << scan.size_rejected_candidates
        << ",\"manifest_size_matches\":" << scan.size_candidates
        << ",\"direct_media_size_matches\":" << scan.direct_media_size_candidates
        << ",\"hashed\":" << scan.hashed_candidates
        << ",\"direct_media_hashed\":" << scan.direct_media_hashed_candidates
        << ",\"hash_rejected_candidates\":" << scan.hash_rejected_candidates
        << ",\"verified_occurrences\":" << scan.verified_occurrences
        << ",\"duplicate_occurrences\":" << scan.duplicate_occurrences
        << ",\"verified_direct_set_occurrences\":" << scan.verified_direct_set_occurrences
        << ",\"duplicate_direct_set_occurrences\":" << scan.duplicate_direct_set_occurrences
        << ",\"verified_container_set_occurrences\":" << scan.verified_container_set_occurrences
        << ",\"verified_unbound_direct_media_occurrences\":"
        << scan.verified_direct_media_occurrences
        << ",\"duplicate_unbound_direct_media_occurrences\":"
        << scan.duplicate_direct_media_occurrences
        << ",\"unreadable_candidates\":" << scan.unreadable_candidates
        << ",\"symlink_rejected_entries\":" << scan.symlink_rejected_entries
        << "}}\n";
}

// This is the active-session companion to --inspect-json. It is deliberately
// created only after the native controller has rehashed one exact release and
// constructed its native adapter. The report contains declarative
// recovery facts and the adapter's bounded capabilities, but no local path,
// archive member, original byte, SDL object, emulator state, or guest action.
void report_runtime_diagnostics_json(const eon::ResolvedLaunchRequest& launch,
    const eon::ReleaseRuntimeAdmission admission, const eon::ReleaseRuntimeRejection rejection,
    const std::optional<eon::RuntimeSessionSnapshot>& session,
    const std::optional<eon::MillenniumDosStaticDispatchDiagnostics>& millennium_dispatch,
    const std::optional<eon::MillenniumDosOwnedFunctionDiagnostics>& owned_function,
    const std::optional<eon::MillenniumDosNativeProcessCheckpoint>& millennium_process,
    const std::optional<eon::DeuterosAtariBootstrapCheckpoint>& atari_checkpoint,
    const std::optional<eon::DeuterosAmigaTitleDependencyChainCheckpoint>& deuteros_title_chain,
    const eon::NativeCodeImageRegistryDiagnostics& code_images,
    const eon::Presentation presentation,
    const eon::DisplayPreferences& display, const std::string_view aspect_identifier) {
    const auto diagnostics = eon::runtime_diagnostics_for_release(launch.release);
    std::cout << "{\"schema\":\"project-eon.runtime-diagnostics/v1\",\"release\":{\"game\":";
    write_json_string(std::cout, eon::name(launch.release.game));
    std::cout << ",\"platform\":"; write_json_string(std::cout, eon::name(launch.release.platform));
    std::cout << ",\"language\":"; write_json_string(std::cout, launch.release.language);
    std::cout << ",\"sha256\":"; write_json_string(std::cout, launch.release.sha256);
    std::cout << "},\"presentation\":";
    write_json_string(std::cout, presentation == eon::Presentation::original ? "original" : "modern");
    std::cout << ",\"display\":{\"resolution\":";
    write_json_string(std::cout, std::to_string(display.width) + "x" + std::to_string(display.height));
    std::cout << ",\"aspect\":"; write_json_string(std::cout, aspect_identifier);
    std::cout << "},\"runtime_admission\":";
    write_json_string(std::cout, eon::release_runtime_admission_label(admission));
    std::cout << ",\"runtime_rejection\":";
    write_json_string(std::cout, eon::release_runtime_rejection_label(rejection));
    std::cout << ",\"runtime_session\":";
    if (session) {
        std::cout << "{\"kind\":"; write_json_string(std::cout, eon::runtime_session_kind_label(session->kind));
        std::cout << ",\"boundary\":";
        write_json_string(std::cout, eon::runtime_session_boundary_label(session->boundary));
        std::cout << ",\"input_contract\":";
        write_json_string(std::cout,
            eon::runtime_input_contract_identifier(session->input_contract));
        std::cout << ",\"capabilities\":{\"decoded_presentation\":"
            << (session->capabilities.decoded_presentation ? "true" : "false")
            << ",\"audio_observations\":"
            << (session->capabilities.audio_observations ? "true" : "false")
            << ",\"admitted_input\":"
            << (session->capabilities.admitted_input ? "true" : "false") << "}}";
    } else {
        std::cout << "null";
    }
    // Prepared from the active coordinator's independently rehashed English
    // DOS leaf. This is a static recovery entry, not a live-PC or reachability
    // claim, and the host has no operation that can advance it.
    std::cout << ",\"millennium_dos_native_process\":";
    if (millennium_process) {
        const auto hex = [](const auto value) {
            std::ostringstream output;
            output << '$' << std::hex << value;
            return output.str();
        };
        std::cout << "{\"static_recovery_entry\":true,\"recovery_entry\":\"startup\""
            << ",\"state\":\"startup-first-private-interrupt\",\"boundary\":{\"kind\":\"private-interrupt\",\"address\":";
        write_json_string(std::cout, hex(millennium_process->boundary.address));
        std::cout << ",\"interrupt\":"
            << (millennium_process->boundary.interrupt
                    ? std::to_string(*millennium_process->boundary.interrupt) : "null")
            << "},\"release_sha256\":";
        write_json_string(std::cout, millennium_process->release_sha256);
        std::cout << ",\"game_executable_sha256\":";
        write_json_string(std::cout, millennium_process->game_executable_sha256);
        std::cout << '}';
    } else {
        std::cout << "null";
    }
    // This is static code provenance, never a live control binding. In
    // particular, the action values are not accepted as SDL input and no
    // handler execution is reported by this no-SDL diagnostics invocation.
    std::cout << ",\"millennium_dos_static_dispatch\":";
    if (millennium_dispatch) {
        const auto hex = [](const auto value) {
            std::ostringstream output;
            output << '$' << std::hex << value;
            return output.str();
        };
        std::cout << "{\"static_only\":true,\"action_poll_address\":";
        write_json_string(std::cout, hex(millennium_dispatch->action_poll_address));
        std::cout << ",\"first_action\":"
            << static_cast<unsigned>(millennium_dispatch->first_action)
            << ",\"action_count\":" << millennium_dispatch->action_count
            << ",\"table_address\":";
        write_json_string(std::cout, hex(millennium_dispatch->table_address));
        std::cout << ",\"table_stride\":" << millennium_dispatch->table_stride
            << ",\"dispatch_address\":";
        write_json_string(std::cout, hex(millennium_dispatch->dispatch_address));
        std::cout << ",\"handler_addresses\":[";
        for (std::size_t index = 0; index < millennium_dispatch->handler_addresses.size(); ++index) {
            if (index != 0) std::cout << ',';
            write_json_string(std::cout, hex(millennium_dispatch->handler_addresses[index]));
        }
        std::cout << "],\"handlers\":[";
        for (std::size_t index = 0; index < millennium_dispatch->handlers.size(); ++index) {
            if (index != 0) std::cout << ',';
            const auto& handler = millennium_dispatch->handlers[index];
            std::cout << "{\"function_id\":";
            write_json_string(std::cout, handler.function_id);
            std::cout << ",\"action\":" << static_cast<unsigned>(handler.action)
                << ",\"handler_address\":";
            write_json_string(std::cout, hex(handler.handler_address));
            std::cout << '}';
        }
        std::cout << "]}";
    } else {
        std::cout << "null";
    }
    std::cout << ",\"millennium_dos_owned_function\":";
    if (owned_function) {
        const auto hex=[](const auto value){ std::ostringstream out; out << '$' << std::hex << value; return out.str(); };
        std::cout << "{\"release_sha256\":"; write_json_string(std::cout,owned_function->release_sha256);
        std::cout << ",\"game_executable_sha256\":"; write_json_string(std::cout,owned_function->game_executable_sha256);
        std::cout << ",\"session\":"; write_json_string(std::cout,eon::runtime_session_kind_label(owned_function->session_kind));
        std::cout << ",\"mode\":"; write_json_string(std::cout,owned_function->mode);
        std::cout << ",\"function_id\":"; write_json_string(std::cout,owned_function->function_id);
        std::cout << ",\"index\":" << owned_function->function_key_index << ",\"handler\":";
        write_json_string(std::cout,hex(owned_function->handler_address));
        std::cout << ",\"boundary\":{\"instruction\":"; write_json_string(std::cout,hex(owned_function->boundary.instruction_address));
        std::cout << ",\"runtime\":";
        if(owned_function->boundary.runtime_address) write_json_string(std::cout,hex(*owned_function->boundary.runtime_address)); else std::cout << "null";
        std::cout << ",\"call_target\":";
        if(owned_function->boundary.call_target) write_json_string(std::cout,hex(*owned_function->boundary.call_target)); else std::cout << "null";
        std::cout << "}}";
    } else std::cout << "null";
    std::cout << ",\"native_code_images\":{\"mapped_descriptors\":"
        << code_images.mapped_descriptor_count << ",\"excluded_images\":"
        << code_images.excluded_image_count << ",\"active\":";
    if (code_images.active) {
        std::cout << "{\"image_id\":";
        write_json_string(std::cout, code_images.active->image_id);
        std::cout << ",\"range_id\":";
        write_json_string(std::cout, code_images.active->range_id);
        std::cout << ",\"address_basis\":";
        write_json_string(std::cout,
            eon::native_code_address_basis_label(code_images.active->address_basis));
        std::cout << ",\"load_status\":";
        write_json_string(std::cout,
            eon::native_code_load_status_label(code_images.active->load_status));
        std::cout << '}';
    } else {
        std::cout << "null";
    }
    std::cout << '}';
    std::cout << ",\"deuteros_amiga_title_dependency_chain\":";
    if (deuteros_title_chain) {
        const auto& chain = *deuteros_title_chain;
        const auto address=[](const auto value){ std::ostringstream out; out << '$' << std::hex << value; return out.str(); };
        std::cout << "{\"custom_chip_writes\":" << chain.observed_custom_chip_write_count
            << ",\"custom_chip_complete\":" << (chain.custom_chip_complete ? "true" : "false")
            << ",\"callback_return\":" << (chain.callback_exec_return_observed ? "true" : "false")
            << ",\"service_setup_armed\":" << (chain.service_setup_boundary_armed ? "true" : "false")
            << ",\"first_service_return\":" << (chain.service_setup_local_plan ? "true" : "false")
            << ",\"second_service_return\":" << (chain.second_service_local_plan ? "true" : "false")
            << ",\"third_service_return\":" << (chain.third_service_local_plan ? "true" : "false")
            << ",\"fourth_service_return\":" << (chain.fourth_service_local_plan ? "true" : "false")
            << ",\"fifth_service_return\":" << (chain.fifth_service_local_plan ? "true" : "false")
            << ",\"stop_before\":";
        write_json_string(std::cout, address(chain.stop_before_address));
        std::cout << '}';
    } else std::cout << "null";
    // Atari's bootstrap profile has an additional media-safe checkpoint. It
    // is optional because every other adapter deliberately has no equivalent
    // inferred machine-state record. The controller returns it only while
    // the exact typed session is live; it contains offsets/hashes/opcodes,
    // not raw state-1 bytes, registers, paths, or an XBIOS result.
    std::cout << ",\"atari_bootstrap_checkpoint\":";
    if (atari_checkpoint) {
        const auto* checkpoint = &*atari_checkpoint;
        std::cout << "{\"first_stage_sha256\":";
        write_json_string(std::cout, checkpoint->first_stage_sha256);
        std::cout << ",\"second_stage_sha256\":";
        write_json_string(std::cout, checkpoint->second_stage_sha256);
        std::cout << ",\"relocated_dispatcher_address\":";
        write_json_string(std::cout, "$" + [&] {
            std::ostringstream value;
            value << std::hex << checkpoint->relocated_dispatcher_address;
            return value.str();
        }());
        std::cout << ",\"state1_raw_request_count\":"
            << checkpoint->state1_raw_request_count;
        std::cout << ",\"state5_state1_prefix\":{\"source_offset\":";
        write_json_string(std::cout, "+0x" + [&] {
            std::ostringstream value;
            value << std::hex << checkpoint->state5_state1_prefix_source_offset;
            return value.str();
        }());
        std::cout << ",\"byte_count\":" << checkpoint->state5_state1_prefix_byte_count;
        std::cout << ",\"sha256\":";
        write_json_string(std::cout, checkpoint->state5_state1_prefix_sha256);
        std::cout << "}";
        std::cout << ",\"state0_duplicate_stage\":{\"byte_count\":"
            << checkpoint->state0_duplicate_byte_count;
        std::cout << ",\"direct_entry_offset\":";
        write_json_string(std::cout, "+0x" + [&] {
            std::ostringstream value;
            value << std::hex << checkpoint->state0_duplicate_direct_entry_offset;
            return value.str();
        }());
        std::cout << ",\"dispatcher_offset\":";
        write_json_string(std::cout, "+0x" + [&] {
            std::ostringstream value;
            value << std::hex << checkpoint->state0_duplicate_dispatcher_offset;
            return value.str();
        }());
        std::cout << ",\"sha256\":";
        write_json_string(std::cout, checkpoint->state0_duplicate_sha256);
        std::cout << "}";
        std::cout << ",\"state1_skipped_ascii\":{\"branch_relative_offset\":";
        write_json_string(std::cout, "+0x" + [&] {
            std::ostringstream value;
            value << std::hex << checkpoint->state1_skipped_ascii_branch_relative_offset;
            return value.str();
        }());
        std::cout << ",\"block_relative_offset\":";
        write_json_string(std::cout, "+0x" + [&] {
            std::ostringstream value;
            value << std::hex << checkpoint->state1_skipped_ascii_relative_offset;
            return value.str();
        }());
        std::cout << ",\"byte_count\":" << checkpoint->state1_skipped_ascii_byte_count;
        std::cout << ",\"printable_run_count\":"
            << checkpoint->state1_skipped_ascii_printable_run_count;
        std::cout << ",\"sha256\":";
        write_json_string(std::cout, checkpoint->state1_skipped_ascii_sha256);
        std::cout << "}";
        std::cout << ",\"state1_display_service\":{\"branch_relative_offset\":";
        write_json_string(std::cout, "+0x" + [&] {
            std::ostringstream value;
            value << std::hex << checkpoint->state1_display_branch_relative_offset;
            return value.str();
        }());
        std::cout << ",\"service_relative_offset\":";
        write_json_string(std::cout, "+0x" + [&] {
            std::ostringstream value;
            value << std::hex << checkpoint->state1_display_service_relative_offset;
            return value.str();
        }());
        std::cout << ",\"xbios_selector\":";
        write_json_string(std::cout, "$" + [&] {
            std::ostringstream value;
            value << std::hex << checkpoint->state1_display_xbios_selector;
            return value.str();
        }());
        std::cout << "}}";
    } else {
        std::cout << "null";
    }
    std::cout << ",\"recovery\":{\"coverage\":";
    write_json_string(std::cout, eon::name(diagnostics.coverage));
    std::cout << ",\"trace_admission\":"; write_json_string(std::cout, diagnostics.trace_admission);
    std::cout << ",\"startup_boundary\":";
    if (const auto& startup = diagnostics.startup_boundary) {
        std::cout << "{\"profile\":"; write_json_string(std::cout, startup->parser_profile_id);
        std::cout << ",\"source_address\":"; write_json_string(std::cout, startup->source_address);
        std::cout << ",\"unresolved\":"; write_json_string(std::cout, startup->unresolved);
        std::cout << '}';
    } else {
        std::cout << "null";
    }
    std::cout << ",\"boundaries\":[";
    for (std::size_t index = 0; index < diagnostics.recovery_boundaries.size(); ++index) {
        if (index != 0) std::cout << ',';
        const auto& boundary = diagnostics.recovery_boundaries[index];
        std::cout << "{\"id\":"; write_json_string(std::cout, boundary.id);
        std::cout << ",\"profile\":"; write_json_string(std::cout, boundary.parser_profile_id);
        std::cout << ",\"cpu\":"; write_json_string(std::cout, boundary.cpu);
        std::cout << ",\"source_address\":"; write_json_string(std::cout, boundary.source_address);
        std::cout << ",\"evidence_level\":"; write_json_string(std::cout, boundary.evidence_level);
        std::cout << ",\"runtime_status\":"; write_json_string(std::cout, boundary.runtime_status);
        std::cout << ",\"documentation_anchor\":";
        write_json_string(std::cout, boundary.documentation_anchor);
        std::cout << '}';
    }
    std::cout << "],\"function_map\":[";
    for (std::size_t index = 0; index < diagnostics.functions.size(); ++index) {
        if (index != 0) std::cout << ',';
        const auto& function = diagnostics.functions[index];
        std::cout << "{\"id\":"; write_json_string(std::cout, function.id);
        std::cout << ",\"profile\":"; write_json_string(std::cout, function.parser_profile_id);
        std::cout << ",\"cpu\":"; write_json_string(std::cout, function.cpu);
        std::cout << ",\"source_asset_sha256\":"; write_json_string(std::cout, function.source_asset_sha256);
        std::cout << ",\"source_span_sha256\":";
        write_json_string(std::cout, function.source_span_sha256.empty()
            ? function.source_asset_sha256 : function.source_span_sha256);
        std::cout << ",\"source_offset\":"; write_json_string(std::cout, function.source_offset);
        std::cout << ",\"runtime_address\":"; write_json_string(std::cout, function.runtime_address);
        std::cout << ",\"address_space\":"; write_json_string(std::cout, function.address_space);
        std::cout << ",\"evidence_level\":"; write_json_string(std::cout, function.evidence_level);
        std::cout << ",\"uncertainty\":"; write_json_string(std::cout, function.uncertainty);
        std::cout << ",\"runtime_status\":"; write_json_string(std::cout, function.runtime_status);
        std::cout << ",\"documentation_anchor\":";
        write_json_string(std::cout, function.documentation_anchor);
        std::cout << '}';
    }
    std::cout << "]}}\n";
}

// A validated capture is evidence, not a runtime input. This compact export
// deliberately reports its admitted identity, boundaries and checkpoint
// counts without serializing local paths, event bytes, media bytes, or
// artifact paths. It is suitable for preservation dashboards and CI records.
void report_reference_trace_json(const eon::ReferenceTrace& trace) {
    std::cout << "{\"schema\":\"project-eon.reference-trace/v1\",\"release\":{\"game\":";
    write_json_string(std::cout, eon::name(trace.source_release.game));
    std::cout << ",\"platform\":"; write_json_string(std::cout, eon::name(trace.source_release.platform));
    std::cout << ",\"language\":"; write_json_string(std::cout, trace.source_release.language);
    std::cout << ",\"sha256\":"; write_json_string(std::cout, trace.source_release.sha256);
    std::cout << "},\"capture\":{\"start_utc\":"; write_json_string(std::cout, trace.capture_start_utc);
    std::cout << ",\"end_utc\":"; write_json_string(std::cout, trace.capture_end_utc);
    std::cout << ",\"emulator_name\":"; write_json_string(std::cout, trace.emulator_name);
    std::cout << ",\"emulator_version\":"; write_json_string(std::cout, trace.emulator_version);
    std::cout << ",\"config_sha256\":"; write_json_string(std::cout, trace.config_sha256);
    std::cout << ",\"command_tail_sha256\":"; write_json_string(std::cout, trace.command_tail_sha256);
    std::cout << ",\"input_timeline_sha256\":"; write_json_string(std::cout, trace.input_timeline_sha256);
    std::cout << "},\"adapter\":"; write_json_string(std::cout, trace.adapter);
    std::cout << ",\"runtime_policy\":";
    const auto* descriptor = eon::reference_trace_adapter_descriptor(trace.adapter);
    write_json_string(std::cout, descriptor
        ? eon::reference_trace_runtime_policy_label(descriptor->runtime_policy)
        : "diagnostics-only");
    std::cout << ",\"events\":{\"sha256\":"; write_json_string(std::cout, trace.event_sha256);
    std::cout << ",\"count\":" << trace.event_count << "},\"source\":{\"media_sha256\":";
    write_json_string(std::cout, trace.source_media_sha256);
    std::cout << ",\"stage_sha256\":"; write_json_string(std::cout, trace.source_stage_sha256);
    std::cout << "},\"recovery_boundaries\":[";
    for (std::size_t index = 0; index < trace.recovery_boundaries.size(); ++index) {
        if (index != 0) std::cout << ',';
        const auto& boundary = trace.recovery_boundaries[index];
        std::cout << "{\"id\":"; write_json_string(std::cout, boundary.id);
        std::cout << ",\"source_address\":"; write_json_string(std::cout, boundary.source_address);
        std::cout << ",\"documentation_anchor\":"; write_json_string(std::cout, boundary.documentation_anchor);
        std::cout << '}';
    }
    std::cout << "],\"checkpoints\":{\"interrupts\":" << trace.adapter_interrupt_count
        << ",\"files\":" << trace.adapter_file_count
        << ",\"exec\":" << trace.adapter_exec_count
        << ",\"private_returns\":" << trace.adapter_private_return_count
        << ",\"callbacks\":" << trace.adapter_callback_count
        << ",\"frames\":" << trace.adapter_frame_count
        << ",\"states\":" << trace.adapter_state_count
        << ",\"display_layouts\":" << trace.adapter_display_layout_count
        << ",\"bitplane_layouts\":" << trace.adapter_bitplane_layout_count
        << ",\"palettes\":" << trace.adapter_palette_checkpoint_count
        << ",\"input\":" << trace.adapter_input_checkpoint_count
        << ",\"frame_checkpoints\":" << trace.adapter_frame_checkpoint_count
        << ",\"audio\":" << trace.adapter_audio_checkpoint_count << "},\"artifacts\":[";
    for (std::size_t index = 0; index < trace.artifacts.size(); ++index) {
        if (index != 0) std::cout << ',';
        const auto& artifact = trace.artifacts[index];
        std::cout << "{\"role\":"; write_json_string(std::cout, artifact.role);
        std::cout << ",\"size\":" << artifact.size;
        std::cout << ",\"sha256\":"; write_json_string(std::cout, artifact.sha256);
        std::cout << ",\"format\":"; write_json_string(std::cout, artifact.format);
        std::cout << '}';
    }
    std::cout << "]}\n";
}

// Keep the user-facing Atari card label tied to a concise, release-specific
// provenance statement.  This runs after verify_release_archive() in the
// inspection loop; it is not a claim that either native boundary is emulated.
void report_atari_launch_boundary(const eon::ReleaseArchive& release) {
    if (release.platform != eon::Platform::atari_st) return;
    if (release.game == eon::Game::millennium) {
        std::cout << "          ATARI LAUNCH BOUNDARY  bootstrap only; stops before "
            "GEMDOS TRAP #1/Fopen result, input, and later launcher control flow\n";
        return;
    }
    std::cout << "          ATARI LAUNCH BOUNDARY  protected bootstrap only; stops before "
        "XBIOS/callback behavior, state selection, title, and gameplay\n";
}

// This is the textual counterpart of the platform cards.  It is emitted only
// after every unfiltered release has been rehashed for this inspection, so a
// stale scanner result cannot make an Atari (or sibling) platform appear
// launchable.  It reports admission only; it neither opens a guest image nor
// crosses a GEMDOS, XBIOS, or callback boundary.
void report_platform_admission(const std::vector<eon::ReleaseArchive>& releases) {
    for (const auto game : {eon::Game::millennium, eon::Game::deuteros}) {
        for (const auto platform : {eon::Platform::dos, eon::Platform::amiga,
                 eon::Platform::atari_st}) {
            const auto status = eon::platform_card_status(releases, game, platform);
            if (status == eon::PlatformCardStatus::unavailable) continue;
            const auto languages = eon::available_release_languages(releases, game, platform);
            const char* admission = status == eon::PlatformCardStatus::ready
                ? "READY" : "RELEASE SELECTION REQUIRED";
            // Admission says whether the card can proceed. Coverage is a
            // separate preservation fact: Atari's verified route is a
            // bootstrap, never a synonym for complete native runtime parity.
            const auto coverage = eon::name(eon::platform_coverage(game, platform));
            std::cout << "PLATFORM ADMISSION  " << eon::name(game) << " / "
                << eon::name(platform) << " / " << admission << " / " << coverage << " / "
                << languages.size() << " verified original "
                << (languages.size() == 1 ? "language" : "languages");
            if (languages.size() > 1
                && std::find(languages.begin(), languages.end(), "en") != languages.end()) {
                std::cout << "; English default";
            }
            std::cout << '\n';
        }
    }
}

// The scanner deliberately avoids treating names as release identity.  This
// explicit preservation report is different: after the complete outer archive
// has been rehashed, it exposes a bounded (two nested ZIP levels) leaf manifest
// so archival work can cite the exact original bytes without extracting or
// copying them to the filesystem.
void report_verified_release_inventory(const eon::ReleaseArchive& release) {
    const auto assets = eon::inventory_verified_release(release);
    std::cout << "          ARCHIVE INVENTORY  " << assets.size()
        << " hash-addressed leaf asset" << (assets.size() == 1 ? "" : "s")
        << "; read in place only\n";
    for (const auto& asset : assets) {
        std::cout << "            " << eon::name(asset.kind) << "  " << asset.size
            << " bytes  " << asset.sha256 << "  " << asset.path << '\n';
    }
}

SDL_FRect aspect_viewport(const float x, const float y, const float maximum_width,
    const float maximum_height, const ModernGraphicsSettings& settings) {
    const auto ratio = display_aspect_ratios.at(settings.aspect_ratio_index);
    const auto viewport = eon::fit_display_aspect_viewport(
        x, y, maximum_width, maximum_height, ratio);
    // Center inside the allocated presentation region. This makes a wider or
    // narrower chosen ratio deliberate and legible, never a clipped crop.
    return {viewport.x, viewport.y, viewport.width, viewport.height};
}

// Keep this overlay's translation boundary explicit. draw_text() also has a
// defensive translation lookup for legacy launcher call sites, but Modern's
// settings are a visible opt-in control surface: every label and state name
// must be supplied by the selected PO catalogue before it reaches the renderer.
void draw_modern_graphics_popup(SDL_Renderer* renderer,
    const ModernGraphicsSettings& settings, const bool modern, const ModernPackAdmission modern_pack_admission,
    const eon::Translator& translator) {
    const auto tr = [&translator](const std::string_view message) {
        return std::string(translator.translate(message));
    };
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 3, 10, 20, 240);
    SDL_RenderFillRect(renderer, &modern_graphics_popup_bounds);
    SDL_SetRenderDrawColor(renderer, 39, 202, 213, 255);
    SDL_RenderRect(renderer, &modern_graphics_popup_bounds);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    if (modern) draw_text(renderer, 390, 174, tr("MODERN GRAPHICS SETTINGS"));
    else draw_text(renderer, 390, 174, tr("ORIGINAL DISPLAY SETTINGS"));
    draw_text(renderer, 390, 212, tr("UP/DOWN: SELECT   LEFT/RIGHT: CHANGE   F10: CLOSE"));
    draw_text(renderer, 390, 232, tr("TOUCH: TAP ROW TO CHANGE   TAP OUTSIDE TO CLOSE"));
    constexpr std::array<const char*, modern_graphics_option_count> names{{
        "GRAPHICS PRESET", "OUTPUT RESOLUTION", "ASPECT RATIO", "RENDER PACING", "PIXEL RECONSTRUCTION", "SMOOTH SCALING", "SCANLINES", "MODERN FRAME",
        "MODERN ASSET PACK", "DEVELOPER DIAGNOSTICS", "REDUCED MOTION",
    }};
    constexpr std::array<const char*, original_display_option_count> original_names{{
        "OUTPUT RESOLUTION", "ASPECT RATIO",
    }};
    const auto& resolution = output_resolutions.at(settings.output_resolution_index);
    const std::array<std::string, modern_graphics_option_count> values{{
        tr(modern_graphics_preset_names.at(static_cast<std::size_t>(settings.preset))),
        std::to_string(resolution.width) + "x" + std::to_string(resolution.height),
        tr(display_aspect_names.at(settings.aspect_ratio_index)),
        tr(render_pacing_names.at(static_cast<std::size_t>(settings.render_pacing))),
        tr(settings.pixel_reconstruction == PixelReconstruction::scale2x ? "SCALE2X (MEMORY ONLY)"
            : settings.pixel_reconstruction == PixelReconstruction::scale4x ? "SCALE4X (MEMORY ONLY)"
            : "OFF (ORIGINAL PIXELS)"),
        tr(settings.smooth_scaling ? "ON" : "OFF"),
        tr(settings.scanlines ? "ON" : "OFF"),
        tr(settings.frame ? "ON" : "OFF"),
        tr(modern_pack_admission == ModernPackAdmission::ready ? "READY"
            : modern_pack_admission == ModernPackAdmission::rejected ? "REJECTED" : "CHOOSE…"),
        tr("OPEN"),
        tr(settings.reduced_motion ? "ON" : "OFF"),
    }};
    const std::array<std::string, original_display_option_count> original_values{{
        std::to_string(resolution.width) + "x" + std::to_string(resolution.height),
        tr(display_aspect_names.at(settings.aspect_ratio_index)),
    }};
    // Original permits only output viewport choices. They are host renderer
    // state, so they preserve decoded source pixels, media, input, timing,
    // simulation and save bytes while meeting the same display requirement.
    const auto option_count = modern ? modern_graphics_option_count : original_display_option_count;
    for (int option = 0; option < option_count; ++option) {
        const auto index = static_cast<std::size_t>(option);
        SDL_SetRenderDrawColor(renderer, index == static_cast<std::size_t>(settings.focused_option)
                ? 255 : 205, index == static_cast<std::size_t>(settings.focused_option) ? 195 : 225,
            index == static_cast<std::size_t>(settings.focused_option) ? 80 : 235, 255);
        draw_text(renderer, 390, modern_graphics_option_first_baseline
            + static_cast<float>(index) * modern_graphics_option_stride,
            std::string(index == static_cast<std::size_t>(settings.focused_option) ? "> " : "  ")
                + tr(modern ? names[index] : original_names[index]));
        draw_text(renderer, 690, modern_graphics_option_first_baseline
            + static_cast<float>(index) * modern_graphics_option_stride,
            modern ? values[index] : original_values[index]);
    }
    SDL_SetRenderDrawColor(renderer, 205, 225, 235, 255);
    draw_text(renderer, 390, 692, tr("SETTINGS APPLY TO SDL RENDERING ONLY."));
}

// This is a separate page of the existing F10 modal, rather than an overlay
// on a recovered screen.  The only values it reads are launcher provenance
// and SDL renderer configuration.  It has no route to original media bytes,
// guest input, simulation state, or save serialization.
void draw_modern_runtime_diagnostics_popup(SDL_Renderer* renderer,
    const ModernGraphicsSettings& settings, const ModernRuntimeDiagnostics& diagnostics,
    const eon::Translator& translator) {
    const auto tr = [&translator](const std::string_view message) {
        return std::string(translator.translate(message));
    };
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 3, 10, 20, 240);
    SDL_RenderFillRect(renderer, &modern_graphics_popup_bounds);
    SDL_SetRenderDrawColor(renderer, 39, 202, 213, 255);
    SDL_RenderRect(renderer, &modern_graphics_popup_bounds);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    draw_text(renderer, 390, 174, tr("MODERN RUNTIME DIAGNOSTICS"));
    draw_text(renderer, 390, 212, tr("ENTER: VIEW FUNCTION MAP   F10 / ESC: BACK TO SETTINGS"));
    const auto& resolution = output_resolutions.at(settings.output_resolution_index);
    const std::array<std::pair<const char*, std::string>, 16> rows{{
        {"RELEASE IDENTITY", diagnostics.release_identity},
        {"RUNTIME ADMISSION", tr(diagnostics.runtime_admission) + " / "
            + tr(diagnostics.runtime_rejection)},
        {"LIFECYCLE STATE", tr(diagnostics.lifecycle_state)},
        {"SESSION ADAPTER", tr(diagnostics.session_adapter)},
        {"SESSION BOUNDARY", tr(diagnostics.session_boundary)},
        {"SESSION CAPABILITIES", diagnostics.session_capabilities},
        {"RECOVERY COVERAGE", tr(diagnostics.recovery_coverage)},
        {"STARTUP BOUNDARY", diagnostics.startup_boundary},
        {"RECOVERY MAP BOUNDARIES", std::to_string(diagnostics.recovery_boundary_count)},
        {"TRACE ADMISSION", tr(diagnostics.trace_admission)},
        {"STATIC CONTROL FLOW", static_control_flow_diagnostics_summary(
            diagnostics.static_control_flow, translator)
            + (diagnostics.millennium_dos_static_dispatch.empty() ? ""
                : " / " + diagnostics.millennium_dos_static_dispatch)
            + (diagnostics.millennium_dos_owned_function.empty() ? ""
                : " / " + diagnostics.millennium_dos_owned_function)
            + (diagnostics.deuteros_amiga_title_dependency_chain.empty() ? ""
                : " / " + diagnostics.deuteros_amiga_title_dependency_chain)
            + (diagnostics.native_code_images.empty() ? ""
                : " / " + diagnostics.native_code_images)},
        {"MODERN PACK", diagnostics.modern_pack},
        {"PACK RENDER TARGETS", diagnostics.modern_pack_targets},
        {"GRAPHICS PRESET", tr(modern_graphics_preset_names.at(static_cast<std::size_t>(settings.preset)))},
        {"RENDERER SETTINGS", std::to_string(resolution.width) + "x" + std::to_string(resolution.height)
            + " / " + tr(display_aspect_names.at(settings.aspect_ratio_index))
            + " / " + tr("PIXEL RECONSTRUCTION") + "="
            + tr(settings.pixel_reconstruction == PixelReconstruction::scale2x ? "SCALE2X (MEMORY ONLY)"
                : settings.pixel_reconstruction == PixelReconstruction::scale4x ? "SCALE4X (MEMORY ONLY)"
                : "OFF (ORIGINAL PIXELS)")
            + " / " + tr("SMOOTH SCALING") + "=" + tr(settings.smooth_scaling ? "ON" : "OFF")
            + " / " + tr("SCANLINES") + "=" + tr(settings.scanlines ? "ON" : "OFF")
            + " / " + tr("MODERN FRAME") + "=" + tr(settings.frame ? "ON" : "OFF")},
        {"FRAME PACING", tr(render_pacing_names.at(static_cast<std::size_t>(settings.render_pacing)))},
    }};
    for (std::size_t index = 0; index < rows.size(); ++index) {
        // Sixteen two-line provenance rows must remain above the fixed
        // read-only notice in the 720p logical diagnostics panel.
        const float y = 226.0F + static_cast<float>(index) * 24.0F;
        SDL_SetRenderDrawColor(renderer, 205, 225, 235, 255);
        draw_text(renderer, 390, y, tr(rows[index].first));
        SDL_SetRenderDrawColor(renderer, 39, 202, 213, 255);
        draw_text(renderer, 390, y + 20.0F, rows[index].second);
    }
    SDL_SetRenderDrawColor(renderer, 205, 225, 235, 255);
    draw_text(renderer, 390, 656, tr("DIAGNOSTICS ARE READ-ONLY; ORIGINAL DATA IS NOT MODIFIED."));
}

// The function map is deliberately its own diagnostics subpage.  Unlike a
// hook browser, it exposes only the parser-bound source identity already
// present in RecoveryMapEntry.  Pages avoid truncating a release with more
// than three named boundaries while keeping every row readable at 720p.
void draw_recovery_function_map_popup(SDL_Renderer* renderer,
    const ModernRuntimeDiagnostics& diagnostics, const std::size_t page,
    const eon::Translator& translator) {
    const auto tr = [&translator](const std::string_view message) {
        return std::string(translator.translate(message));
    };
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 3, 10, 20, 240);
    SDL_RenderFillRect(renderer, &modern_graphics_popup_bounds);
    SDL_SetRenderDrawColor(renderer, 39, 202, 213, 255);
    SDL_RenderRect(renderer, &modern_graphics_popup_bounds);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    constexpr std::size_t rows_per_page = 3;
    const auto page_count = std::max<std::size_t>(1,
        (diagnostics.recovery_functions.size() + rows_per_page - 1U) / rows_per_page);
    const auto first = std::min(page * rows_per_page, diagnostics.recovery_functions.size());
    const auto last = std::min(first + rows_per_page, diagnostics.recovery_functions.size());
    draw_text(renderer, 390, 174, tr("RECOVERY FUNCTION MAP"));
    draw_text(renderer, 390, 212, tr("UP/DOWN: PAGE   F10 / ESC: BACK TO DIAGNOSTICS"));
    draw_text(renderer, 390, 240, diagnostics.release_identity);
    draw_text(renderer, 850, 240, tr("PAGE") + " " + std::to_string(page + 1U)
        + "/" + std::to_string(page_count));
    if (first == last) {
        SDL_SetRenderDrawColor(renderer, 205, 225, 235, 255);
        draw_text(renderer, 390, 320, tr("NO HASH-BOUND FUNCTION ENTRIES FOR THIS RELEASE."));
    }
    for (std::size_t index = first; index < last; ++index) {
        const auto& entry = diagnostics.recovery_functions[index];
        const float y = 286.0F + static_cast<float>(index - first) * 112.0F;
        SDL_SetRenderDrawColor(renderer, 39, 202, 213, 255);
        draw_text(renderer, 390, y, entry.id);
        SDL_SetRenderDrawColor(renderer, 205, 225, 235, 255);
        const auto span_identity = entry.source_span_sha256.empty()
            ? entry.source_asset_sha256 : entry.source_span_sha256;
        draw_text(renderer, 390, y + 22.0F, entry.cpu + " / " + entry.source_offset + " -> "
            + entry.runtime_address + (entry.address_space == "runtime" ? "" : " [" + entry.address_space + "]")
            + " / OWNER " + truncated_identity_hash(entry.source_asset_sha256)
            + " / SPAN " + truncated_identity_hash(span_identity));
        draw_text(renderer, 390, y + 44.0F, entry.profile + " / " + entry.evidence_level);
        // Function-map uncertainty is preservation metadata, not original
        // text. Keep it readable within the 720p modal instead of letting a
        // long, honest boundary run off screen and conceal its status. The
        // complete value remains available from --inspect-json; truncation
        // here never changes admission or drops the row from diagnostics.
        draw_text(renderer, 390, y + 66.0F,
            truncated_diagnostic_value(entry.runtime_status + "; " + entry.uncertainty, 92U));
    }
    SDL_SetRenderDrawColor(renderer, 205, 225, 235, 255);
    draw_text(renderer, 390, 630, tr("DECLARATIVE DIAGNOSTICS ONLY; THIS DOES NOT EXECUTE ORIGINAL CODE."));
}

bool inside(const SDL_FRect& rectangle, float x, float y) {
    return x >= rectangle.x && x <= rectangle.x + rectangle.w
        && y >= rectangle.y && y <= rectangle.y + rectangle.h;
}

SDL_Texture* load_launcher_asset(SDL_Renderer* renderer, const char* directory,
    const char* filename) {
    const auto base = std::filesystem::path(SDL_GetBasePath());
    const std::array<std::filesystem::path, 5> candidates{{
        base / "assets" / directory / filename,
        base / "Resources" / "assets" / directory / filename,
        base / ".." / "share" / "project-eon" / "assets" / directory / filename,
        std::filesystem::path(EON_ASSET_DIR) / directory / filename,
        std::filesystem::path("assets") / directory / filename,
    }};
    for (const auto& path : candidates) {
        if (SDL_Texture* texture = IMG_LoadTexture(renderer, path.string().c_str())) return texture;
    }
    std::cerr << "Unable to load launcher asset " << directory << '/' << filename << ": "
              << SDL_GetError() << '\n';
    return nullptr;
}

SDL_Texture* load_card(SDL_Renderer* renderer, const char* filename) {
    return load_launcher_asset(renderer, "cards", filename);
}

SDL_Texture* load_branding_texture(SDL_Renderer* renderer, const char* filename) {
    return load_launcher_asset(renderer, "branding", filename);
}

std::optional<std::filesystem::path> find_font_directory() {
    const auto base = std::filesystem::path(SDL_GetBasePath());
    const std::array<std::filesystem::path, 5> candidates{{
        base / "assets" / "fonts",
        base / "Resources" / "assets" / "fonts",
        base / ".." / "share" / "project-eon" / "assets" / "fonts",
        std::filesystem::path(EON_FONT_DIR),
        std::filesystem::path("assets") / "fonts",
    }};
    for (const auto& candidate : candidates) {
        if (std::filesystem::is_regular_file(candidate / "NotoSans-Regular.ttf")) return candidate;
    }
    return std::nullopt;
}

void report_deuteros_amiga(const eon::ReleaseArchive& release) {
    constexpr auto clean_system_adf =
        "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38";
    constexpr auto clean_data_adf =
        "99909db1e190be02e049084743af44f00e331be6bf2d97b4831ada5fe4c30b4a";
    const auto image = eon::extract_verified_release_asset(release, clean_system_adf);
    if (!image) return;
    const auto data_image = eon::extract_verified_release_asset(release, clean_data_adf);
    if (!data_image) throw std::runtime_error("Verified Deuteros Amiga data ADF is unavailable");
    const eon::AmigaAdf disk{std::span<const std::uint8_t>(*image)};
    const eon::AmigaAdf data_disk{std::span<const std::uint8_t>(*data_image)};
    const auto data_header = eon::inspect_deuteros_amiga_data_disk_header(data_disk);
    const auto plan = eon::parse_deuteros_amiga_load_plan(disk);
    std::cout << "          Paired data ADF: DEU\\0 custom-media header verified; "
        << data_header.sector_count << " sectors, prefix 0x"
        << std::hex << data_header.header_prefix_length << std::dec << " SHA-256 "
        << data_header.header_prefix_sha256 << ", DEUTEROSDATA markers "
        << data_header.data_marker_count << "; leaf SHA-256 " << clean_data_adf << '\n';
    std::cout << "          ADF boot checksum valid; main stage disk 0x" << std::hex
        << plan.main_stage.disk_offset << " -> memory 0x" << plan.main_stage.destination
        << ", entry 0x" << plan.main_stage.entry_address << std::dec << '\n';
    const auto& main_entry = plan.main_stage_entry;
    std::cout << "          Main entry evidence: incoming A1/D0 -> 0x" << std::hex
        << main_entry.incoming_controller_cell << "/0x" << main_entry.incoming_mode_cell
        << ", stack 0x" << main_entry.stack_address << ", ceiling 0x"
        << main_entry.memory_ceiling << ", raw init calls 0x"
        << main_entry.initialization_calls[0] << "/0x" << main_entry.initialization_calls[1]
        << std::dec << '\n';
    std::cout << "          Main loop evidence: 0x" << std::hex << main_entry.loop_address
        << " resets words 0x" << main_entry.first_state_word_address << "/0x"
        << main_entry.second_state_word_address << ", enables 0x"
        << main_entry.scheduler_enable_word_address << "=0x"
        << main_entry.scheduler_enable_word_value << ", services 0x"
        << main_entry.loop_first_service_address << "/0x" << main_entry.loop_scheduler_address
        << ", probes 0x" << main_entry.first_input_address << " bit " << std::dec
        << static_cast<unsigned>(main_entry.first_input_bit) << " and 0x" << std::hex
        << main_entry.second_input_address << " bit " << std::dec
        << static_cast<unsigned>(main_entry.second_input_bit) << '\n';
    std::cout << "          >2 scheduler evidence: state 0x" << std::hex
        << main_entry.scheduler_state_base_address << ", count 0x"
        << main_entry.scheduler_channel_count_address << ", stride 0x"
        << main_entry.scheduler_channel_stride << ", program/selector/value +0x"
        << main_entry.scheduler_active_program_offset << "/+0x"
        << main_entry.scheduler_wait_selector_offset << "/+0x"
        << main_entry.scheduler_wait_value_offset << ", selectors 0x" << std::hex
        << static_cast<unsigned>(main_entry.scheduler_wait_selectors[0]) << "/0x"
        << static_cast<unsigned>(main_entry.scheduler_wait_selectors[1]) << "/0x"
        << static_cast<unsigned>(main_entry.scheduler_wait_selectors[2]) << "/0x"
        << static_cast<unsigned>(main_entry.scheduler_wait_selectors[3])
        << "; tail bit " << std::dec << static_cast<unsigned>(main_entry.scheduler_tail_probe_bit)
        << " at 0x" << std::hex << main_entry.scheduler_tail_probe_address
        << " -> 0x" << main_entry.scheduler_tail_service_address << std::dec << '\n';
    std::cout << "          Input dispatch evidence: 0x" << std::hex
        << main_entry.input_dispatch_address << " compares word 0x"
        << main_entry.input_dispatch_state_address << " with " << std::dec
        << main_entry.input_dispatch_compare_value << ", clamps below to "
        << main_entry.input_dispatch_clamped_value << ", routes <= to 0x"
        << std::hex << main_entry.input_dispatch_service_address << " and > to 0x"
        << main_entry.input_dispatch_continue_address << std::dec << '\n';
    std::cout << "          <= service evidence: increments word 0x" << std::hex
        << main_entry.dispatch_service_state_address << "; result " << std::dec
        << main_entry.dispatch_service_first_exit_value << " -> 0x" << std::hex
        << main_entry.dispatch_service_first_exit_address << " (profile cell 0x"
        << main_entry.first_exit_profile_cell_address << " = " << std::dec
        << main_entry.first_exit_profile_value << ", return slots 0x" << std::hex
        << main_entry.bootstrap_controller_return_cell << "/0x"
        << main_entry.bootstrap_profile_return_cell << "); result " << std::dec
        << main_entry.dispatch_service_second_exit_value << " -> 0x" << std::hex
        << main_entry.dispatch_service_second_exit_address << " (cell 0x"
        << main_entry.second_exit_profile_cell_address << " = " << std::dec
        << main_entry.second_exit_initial_profile_value << ", service 0x" << std::hex
        << main_entry.second_exit_service_address << ", match 0x"
        << main_entry.second_exit_service_match_value << " -> 0x"
        << main_entry.second_exit_matched_return_address << ")" << std::dec << '\n';
    const auto main_resource_catalog = eon::inspect_deuteros_amiga_main_resource_catalog(disk, plan);
    std::cout << "          Main-resource catalogue: " << main_resource_catalog.entries.size()
        << " caller-proved table entries, " << main_resource_catalog.total_source_bytes
        << " bounded source bytes (read-only)\n";
    for (const auto& resource : main_resource_catalog.entries) {
        std::cout << "            [" << resource.resource_index << "] ADF 0x" << std::hex
            << resource.source_disk_offset << ", 0x" << resource.source_length << std::dec
            << " bytes";
        if (resource.source_range_available) {
            std::cout << "; SHA-256 " << resource.source_sha256 << '\n';
        } else {
            std::cout << " (" << resource.preservation_boundary << ")\n";
        }
    }
    for (std::size_t index = 0; index < 2; ++index) {
        const auto bundle = eon::parse_deuteros_amiga_bundle(
            disk, plan.resource_disk_offsets[index]);
        const auto bitmap_blob = eon::parse_deuteros_amiga_indexed_blob(disk, bundle);
        const auto bitmap_catalog = eon::inspect_deuteros_amiga_bitmap_catalog(
            disk, bundle, bitmap_blob);
        std::cout << "          Resource bundle " << index << ": disk 0x" << std::hex
            << bundle.disk_offset << ", 0x" << bundle.length << std::dec
            << " bytes, " << bundle.object_count << " objects, mode "
            << bundle.mode_flag << '\n';
        std::cout << "            Bitmap catalogue: " << bitmap_catalog.record_count
            << " hash-verified decoded records, " << bitmap_catalog.decoded_pixel_count
            << " indexed pixels; bundle SHA-256 " << bitmap_catalog.bundle_sha256 << '\n';
    }
    for (std::uint16_t index = 0; index < 2; ++index) {
        const auto transfer = eon::read_deuteros_amiga_main_resource(disk, plan, index);
        if (transfer) {
            std::cout << "          Resource transfer " << index << ": ADF 0x" << std::hex
                << transfer->source_disk_offset << " -> RAM 0x"
                << transfer->payload_destination_address << ", 0x"
                << transfer->payload_length << std::dec << " original bytes (in memory only)\n";
            const auto observation = eon::sample_deuteros_amiga_main_resource_consumer(
                *transfer, main_entry, 0, 0);
            std::cout << "            $2016a raw observation (seed/counter 0): offset 0x"
                << std::hex << observation.table_offset << ", word 0x"
                << observation.sampled_word << ", result 0x" << observation.addend_result
                << ", seed 0x" << observation.seed_after << std::dec << '\n';
        }
    }
    std::cout << "          Resource consumer: 0x" << std::hex
        << main_entry.resource_consumer_address << " loads 0x"
        << main_entry.resource_consumer_base_address << ", seed/counter 0x"
        << main_entry.resource_consumer_seed_address << "/0x"
        << main_entry.resource_consumer_counter_address << ", mask 0x"
        << main_entry.resource_consumer_index_mask << ", +0x"
        << main_entry.resource_consumer_word_addend << "; command arms 0x"
        << main_entry.resource_consumer_command_words[0] << "@0x"
        << main_entry.resource_consumer_call_sites[0] << "/0x"
        << main_entry.resource_consumer_command_words[1] << "@0x"
        << main_entry.resource_consumer_call_sites[1] << std::dec << '\n';
    std::cout << "          Channel command 0x10: writes 0x" << std::hex
        << main_entry.channel_request_value << " to 0x"
        << main_entry.channel_request_cell_address << "; loop tests at 0x"
        << main_entry.channel_request_loop_test_address << " and nonzero branch 0x"
        << main_entry.channel_request_loop_branch_address << " -> 0x"
        << main_entry.channel_request_continuation_address << std::dec << '\n';
    const auto channel_request_continuation =
        eon::parse_deuteros_amiga_channel_request_continuation(disk, plan);
    std::cout << "          Channel-request static continuation: ADF 0x" << std::hex
        << plan.main_stage.disk_offset + channel_request_continuation.entry_address
            - plan.main_stage.destination
        << ", entry 0x" << channel_request_continuation.entry_address
        << "; BSR 0x" << channel_request_continuation.local_call_addresses[0] << " -> 0x"
        << channel_request_continuation.local_call_targets[0] << ", 0x"
        << channel_request_continuation.local_call_addresses[1] << " -> 0x"
        << channel_request_continuation.local_call_targets[1] << "; bit " << std::dec
        << static_cast<unsigned>(channel_request_continuation.input_test_bit) << " loop 0x"
        << std::hex << channel_request_continuation.input_zero_branch_address << " -> 0x"
        << channel_request_continuation.input_zero_branch_target << "; final 0x"
        << channel_request_continuation.final_branch_address << " -> 0x"
        << channel_request_continuation.final_branch_target << std::dec << "; SHA-256 "
        << channel_request_continuation.raw_sha256
        << " (static only; no condition, call, or input-port execution)\n";
    const auto channel_request_first_callee =
        eon::parse_deuteros_amiga_channel_request_first_callee(
            disk, plan, channel_request_continuation);
    std::cout << "          Channel-request first callee: ADF 0x" << std::hex
        << plan.main_stage.disk_offset + channel_request_first_callee.entry_address
            - plan.main_stage.destination
        << ", entry 0x" << channel_request_first_callee.entry_address
        << "; bit " << std::dec << static_cast<unsigned>(channel_request_first_callee.input_test_bit)
        << " branch 0x" << std::hex << channel_request_first_callee.input_zero_branch_address
        << " -> 0x" << channel_request_first_callee.input_zero_branch_target
        << "; DBRA 0x" << channel_request_first_callee.loop_branch_address << " -> 0x"
        << channel_request_first_callee.loop_branch_target << "; vectors 0x"
        << channel_request_first_callee.vector_call_addresses[0] << "/0x"
        << channel_request_first_callee.vector_call_addresses[1] << "; final services 0x"
        << channel_request_first_callee.final_service_call_addresses[0] << "/0x"
        << channel_request_first_callee.final_service_call_addresses[1] << " -> 0x"
        << channel_request_first_callee.final_service_target << "; SHA-256 "
        << channel_request_first_callee.raw_sha256 << std::dec
        << " (static only; no polls, writes, vectors, calls, or returns executed)\n";
    const auto channel_request_second_callee =
        eon::parse_deuteros_amiga_channel_request_second_callee(
            disk, plan, channel_request_continuation);
    std::cout << "          Channel-request second callee: ADF 0x" << std::hex
        << plan.main_stage.disk_offset + channel_request_second_callee.entry_address
            - plan.main_stage.destination
        << ", entry 0x" << channel_request_second_callee.entry_address
        << "; longword 0x" << channel_request_second_callee.copied_longword_source_address
        << " -> 0x" << channel_request_second_callee.copied_longword_destination_address
        << "; clears 0x" << channel_request_second_callee.cleared_word_addresses[0]
        << "/0x" << channel_request_second_callee.cleared_word_addresses[1] << "/0x"
        << channel_request_second_callee.cleared_word_addresses[2] << "/0x"
        << channel_request_second_callee.cleared_word_addresses[3] << "; 0x"
        << channel_request_second_callee.final_word_value << " -> 0x"
        << channel_request_second_callee.final_word_address << "; RTS 0x"
        << channel_request_second_callee.return_address << "; SHA-256 "
        << channel_request_second_callee.raw_sha256 << std::dec
        << " (static only; no low-memory/custom-register effects executed)\n";
    const auto channel_request_following_service =
        eon::parse_deuteros_amiga_channel_request_following_service(
            disk, plan, channel_request_continuation);
    std::cout << "          Channel-request following service: ADF 0x" << std::hex
        << plan.main_stage.disk_offset + channel_request_following_service.entry_address
            - plan.main_stage.destination
        << ", entry 0x" << channel_request_following_service.entry_address
        << "; execution 0x" << channel_request_following_service.execution_entry_address
        << ", embedded table 0x" << channel_request_following_service.embedded_table_address
        << ", descriptors 0x" << channel_request_following_service.descriptor_base_address
        << " stride 0x" << channel_request_following_service.descriptor_stride
        << "; RTS 0x" << channel_request_following_service.return_address
        << "; SHA-256 " << channel_request_following_service.raw_sha256 << std::dec
        << " (static only; no descriptor, flag, or runtime-cell effects executed)\n";
    const auto channel_request_adjacent_entry =
        eon::parse_deuteros_amiga_channel_request_adjacent_entry(
            disk, plan, channel_request_following_service);
    std::cout << "          Channel-request adjacent entry: ADF 0x" << std::hex
        << plan.main_stage.disk_offset + channel_request_adjacent_entry.entry_address
            - plan.main_stage.destination
        << ", entry 0x" << channel_request_adjacent_entry.entry_address
        << "; test 0x" << channel_request_adjacent_entry.tested_byte_address
        << ", zero 0x" << channel_request_adjacent_entry.zero_branch_address
        << " -> 0x" << channel_request_adjacent_entry.zero_branch_target
        << ", early RTS 0x" << channel_request_adjacent_entry.early_return_address
        << "; descriptors 0x" << channel_request_adjacent_entry.descriptor_base_address
        << " stride 0x" << channel_request_adjacent_entry.descriptor_stride
        << "; final RTS 0x" << channel_request_adjacent_entry.final_return_address
        << "; SHA-256 " << channel_request_adjacent_entry.raw_sha256 << std::dec
        << " (static only; no caller state, pointers, branches, or copies executed)\n";
    const auto opening_bundle = eon::parse_deuteros_amiga_bundle(
        disk, plan.resource_disk_offsets[0]);
    const auto sound_bank = eon::parse_deuteros_amiga_sound_bank(disk, opening_bundle);
    std::cout << "          Opening Paula table: " << sound_bank.sounds.size()
        << " original DMA records, ADF 0x" << std::hex
        << opening_bundle.disk_offset + sound_bank.table_relative_offset << "+0x"
        << sound_bank.table_length << "; SHA-256 " << sound_bank.table_sha256
        << std::dec << " (descriptor/DMA provenance only)\n";
    const auto& handoff = plan.title_handoff_profile;
    const auto title_stage = eon::parse_deuteros_amiga_title_stage(disk, plan);
    std::cout << "          Title input profile: disk 0x" << std::hex << handoff.disk_offset
        << ", length 0x" << handoff.length << ", memory 0x" << handoff.destination
        << ", entry 0x" << plan.title_stage.entry_address << std::dec << '\n';
    std::cout << "          Title stage: mode " << title_stage.special_mode << " -> byte 0x"
        << std::hex << title_stage.special_mode_byte_address << ", config 0x"
        << title_stage.special_mode_configuration_value << std::dec << "; loop 0x"
        << std::hex << title_stage.main_loop_address << std::dec << '\n';
    std::cout << "          Title initialization requirement: stack 0x" << std::hex
        << title_stage.initialization_stack_address << ", Exec base 0x"
        << title_stage.initialization_exec_base_address << " vectors -0x"
        << static_cast<std::uint16_t>(-title_stage.initialization_exec_vectors[0]) << "/-0x"
        << static_cast<std::uint16_t>(-title_stage.initialization_exec_vectors[1])
        << ", D0 0x" << title_stage.initialization_exec_allocation_size
        << "; custom 0x" << title_stage.initialization_custom_base_address
        << "+40/42/9a/96 (not called or written)" << std::dec << '\n';
    const auto post_exec_pointer_seed =
        eon::parse_deuteros_amiga_title_post_exec_pointer_seed_profile(disk, plan);
    std::cout << "          Conditional post-Exec pointer seed: call 0x" << std::hex
        << post_exec_pointer_seed.call_site_address << " with D1=0x"
        << post_exec_pointer_seed.caller_d1_literal << " -> local 0x"
        << post_exec_pointer_seed.callee_address << ", literal 0x"
        << post_exec_pointer_seed.literal_value << " to cell 0x"
        << post_exec_pointer_seed.destination_address
        << " (prior ABI returns unmodelled)" << std::dec << '\n';
    const auto post_exec_state_init =
        eon::parse_deuteros_amiga_title_post_exec_state_init_profile(disk, plan);
    std::cout << "          Conditional post-Exec local state init: call 0x" << std::hex
        << post_exec_state_init.caller_address << " -> 0x"
        << post_exec_state_init.entry_address << "; words 0x"
        << post_exec_state_init.cleared_word_value << "/0x"
        << post_exec_state_init.initial_word_value << " -> 0x"
        << post_exec_state_init.cleared_word_address << "/0x"
        << post_exec_state_init.initial_word_address << ", long 0x"
        << post_exec_state_init.initial_long_value << " -> 0x"
        << post_exec_state_init.initial_long_address << ", word 0x"
        << post_exec_state_init.copied_word_source_address << " -> 0x"
        << post_exec_state_init.copied_word_destination_address
        << " (prior ABI returns unmodelled)" << std::dec << '\n';
    const auto post_exec_third_service =
        eon::parse_deuteros_amiga_title_post_exec_third_service_profile(disk, plan);
    std::cout << "          Conditional post-Exec third service: call 0x" << std::hex
        << post_exec_third_service.caller_address << " -> 0x"
        << post_exec_third_service.dispatch_entry_address << " -> local 0x"
        << post_exec_third_service.graphics_service_address << "; vectors -0x"
        << static_cast<std::uint16_t>(-post_exec_third_service.graphics_library_vectors[0])
        << "/-0x" << static_cast<std::uint16_t>(-post_exec_third_service.graphics_library_vectors[1])
        << "/-0x" << static_cast<std::uint16_t>(-post_exec_third_service.graphics_library_vectors[2])
        << ", tail 0x" << post_exec_third_service.dispatcher_tail_jump_address
        << " (prior ABI returns unmodelled)" << std::dec << '\n';
    const auto post_exec_tail_dispatch =
        eon::parse_deuteros_amiga_title_post_exec_tail_dispatch_profile(disk, plan);
    std::cout << "          Conditional post-Exec tail dispatch: 0x" << std::hex
        << post_exec_tail_dispatch.caller_address << " -> 0x"
        << post_exec_tail_dispatch.entry_address << "; BSR targets 0x"
        << post_exec_tail_dispatch.local_call_addresses[0] << "/0x"
        << post_exec_tail_dispatch.local_call_addresses[1] << "/0x"
        << post_exec_tail_dispatch.local_call_addresses[2] << "/0x"
        << post_exec_tail_dispatch.local_call_addresses[3] << ", RTS 0x"
        << post_exec_tail_dispatch.return_address
        << " (prior ABI returns unmodelled)" << std::dec << '\n';
    const auto post_exec_tail_first_callee =
        eon::parse_deuteros_amiga_title_post_exec_tail_first_callee_profile(disk, plan);
    std::cout << "          Conditional tail first callee: BSR 0x" << std::hex
        << post_exec_tail_first_callee.caller_address << " -> local 0x"
        << post_exec_tail_first_callee.entry_address << "; A0/A1 0x"
        << post_exec_tail_first_callee.a0_literal << "/0x"
        << post_exec_tail_first_callee.a1_literal << ", vector -0x"
        << static_cast<std::uint16_t>(-post_exec_tail_first_callee.graphics_library_vector)
        << " via cell 0x" << post_exec_tail_first_callee.graphics_library_base_address
        << " (ABI return unmodelled)" << std::dec << '\n';
    const auto post_exec_tail_second_callee =
        eon::parse_deuteros_amiga_title_post_exec_tail_second_callee_profile(disk, plan);
    std::cout << "          Conditional tail second callee: BSR 0x" << std::hex
        << post_exec_tail_second_callee.caller_address << " -> local 0x"
        << post_exec_tail_second_callee.entry_address << "; cells 0x"
        << post_exec_tail_second_callee.selection_cells[0] << "/0x"
        << post_exec_tail_second_callee.selection_cells[3] << ", vector -0x"
        << static_cast<std::uint16_t>(-post_exec_tail_second_callee.graphics_library_vector)
        << " via cell 0x" << post_exec_tail_second_callee.graphics_library_base_address
        << " (ABI return unmodelled)" << std::dec << '\n';
    const auto post_exec_tail_third_callee =
        eon::parse_deuteros_amiga_title_post_exec_tail_third_callee_profile(disk, plan);
    std::cout << "          Conditional tail third callee: BSR 0x" << std::hex
        << post_exec_tail_third_callee.caller_address << " -> re-entry 0x"
        << post_exec_tail_third_callee.entry_address << ", continuation 0x"
        << post_exec_tail_third_callee.caller_continuation_address << ", RTS 0x"
        << post_exec_tail_third_callee.routine_return_address
        << " (ABI return unmodelled)" << std::dec << '\n';
    const auto post_exec_tail_fourth_callee =
        eon::parse_deuteros_amiga_title_post_exec_tail_fourth_callee_profile(disk, plan);
    std::cout << "          Conditional tail fourth callee: BSR 0x" << std::hex
        << post_exec_tail_fourth_callee.caller_address << " -> local 0x"
        << post_exec_tail_fourth_callee.entry_address << "; A0/A1 0x"
        << post_exec_tail_fourth_callee.a0_literal << "/0x"
        << post_exec_tail_fourth_callee.a1_literal << ", vector -0x"
        << static_cast<std::uint16_t>(-post_exec_tail_fourth_callee.graphics_library_vector)
        << " via cell 0x" << post_exec_tail_fourth_callee.graphics_library_base_address
        << " (ABI return unmodelled)" << std::dec << '\n';
    const auto post_exec_tail_return =
        eon::parse_deuteros_amiga_title_post_exec_tail_return_profile(disk, plan);
    std::cout << "          Conditional tail return: 0x" << std::hex
        << post_exec_tail_return.continuation_address << " table 0x"
        << post_exec_tail_return.source_table_address << " -> cells 0x"
        << post_exec_tail_return.destination_addresses[0] << "/0x"
        << post_exec_tail_return.destination_addresses[1] << "; local 0x"
        << post_exec_tail_return.local_service_address << " vector -0x"
        << static_cast<std::uint16_t>(-post_exec_tail_return.exec_vector)
        << " via cell 0x" << post_exec_tail_return.exec_base_address
        << " (ABI return unmodelled)" << std::dec << '\n';
    const auto post_exec_tail_return_continuation =
        eon::parse_deuteros_amiga_title_post_exec_tail_return_continuation_profile(disk, plan);
    std::cout << "          Conditional tail return continuation: 0x" << std::hex
        << post_exec_tail_return_continuation.continuation_address << "; calls 0x"
        << post_exec_tail_return_continuation.direct_call_addresses[0] << "/0x"
        << post_exec_tail_return_continuation.direct_call_addresses[12]
        << ", indirect literal 0x"
        << post_exec_tail_return_continuation.indirect_call_pointer_literal
        << ", stop 0x" << post_exec_tail_return_continuation.stop_before_address
        << " (all ABI returns unmodelled)" << std::dec << '\n';
    const auto post_exec_pointer_route =
        eon::parse_deuteros_amiga_title_post_exec_pointer_route_profile(disk, plan);
    std::cout << "          Conditional pointer route: JSR 0x" << std::hex
        << post_exec_pointer_route.caller_address << " -> 0x"
        << post_exec_pointer_route.entry_address << "; pointer literals 0x"
        << post_exec_pointer_route.selected_pointer_literal << "/0x"
        << post_exec_pointer_route.alternate_pointer_literal << " via cell 0x"
        << post_exec_pointer_route.selected_pointer_cell_address << ", branches 0x"
        << post_exec_pointer_route.selected_branch_target << "/0x"
        << post_exec_pointer_route.alternate_branch_target
        << " (conditions and ABI returns unmodelled)" << std::dec << '\n';
    const auto paired_local_route =
        eon::parse_deuteros_amiga_title_post_exec_paired_local_route_profile(disk, plan);
    std::cout << "          Paired local route: D0 0x" << std::hex
        << paired_local_route.d0_literals[0] << "/0x" << paired_local_route.d0_literals[1]
        << " calls 0x" << paired_local_route.entry_address << "; branches 0x"
        << paired_local_route.clear_bit_branch_target << "/0x"
        << paired_local_route.high_block_entry_address << ", RTS 0x"
        << paired_local_route.high_block_return_address
        << " (cells and writes unmodelled)" << std::dec << '\n';
    const auto service_route =
        eon::parse_deuteros_amiga_title_post_exec_service_route_profile(disk, plan);
    std::cout << "          Post-Exec service route: JSR 0x" << std::hex
        << service_route.caller_address << " -> 0x" << service_route.entry_address
        << "; external calls 0x" << service_route.external_call_targets[0] << "/0x"
        << service_route.external_call_targets[1] << "/0x"
        << service_route.external_call_targets[2] << ", nested 0x"
        << service_route.nested_entry_address << " -> 0x"
        << service_route.nested_branch_target << ", continuation 0x"
        << service_route.continuation_target
        << " (calls, branches, and writes unmodelled)" << std::dec << '\n';
    const auto service_continuation =
        eon::parse_deuteros_amiga_title_post_exec_service_continuation_profile(disk, plan);
    std::cout << "          Service continuation: 0x" << std::hex
        << service_continuation.entry_address << " calls 0x"
        << service_continuation.first_external_call_target << ", local 0x"
        << service_continuation.local_service_call_targets[0] << ", dispatch 0x"
        << service_continuation.graphics_dispatch_target << "; tables 0x"
        << service_continuation.table_addresses[0] << "/0x"
        << service_continuation.table_addresses[1] << ", RTS 0x"
        << service_continuation.return_address
        << " (call results, predicates, and writes unmodelled)" << std::dec << '\n';
    const auto post_exec_tail_flag_gate =
        eon::parse_deuteros_amiga_title_post_exec_tail_flag_gate_profile(disk, plan);
    std::cout << "          Conditional tail flag gate: 0x" << std::hex
        << post_exec_tail_flag_gate.entry_address << " cells 0x"
        << post_exec_tail_flag_gate.source_word_addresses[0] << "/0x"
        << post_exec_tail_flag_gate.source_word_addresses[1] << "; jump 0x"
        << post_exec_tail_flag_gate.absolute_jump_target << ", calls 0x"
        << post_exec_tail_flag_gate.direct_call_targets[0] << "/0x"
        << post_exec_tail_flag_gate.direct_call_targets[2] << ", stop 0x"
        << post_exec_tail_flag_gate.stop_after_address
        << " (all results and writes unmodelled)" << std::dec << '\n';
    const auto post_exec_tail_flag_gate_first_callee =
        eon::parse_deuteros_amiga_title_post_exec_tail_flag_gate_first_callee_profile(disk, plan);
    std::cout << "          Flag-gate first callee: JSR 0x" << std::hex
        << post_exec_tail_flag_gate_first_callee.caller_address << " -> 0x"
        << post_exec_tail_flag_gate_first_callee.entry_address << "; byte 0x"
        << post_exec_tail_flag_gate_first_callee.tested_byte_address << ", word 0x"
        << post_exec_tail_flag_gate_first_callee.first_loop_word_address
        << ", RTS 0x" << post_exec_tail_flag_gate_first_callee.terminal_return_address
        << " (cell values and loop execution unmodelled)" << std::dec << '\n';
    const auto post_exec_tail_flag_gate_copy_callee =
        eon::parse_deuteros_amiga_title_post_exec_tail_flag_gate_copy_callee_profile(disk, plan);
    std::cout << "          Flag-gate copy callee: JSR 0x" << std::hex
        << post_exec_tail_flag_gate_copy_callee.caller_addresses[0] << "/0x"
        << post_exec_tail_flag_gate_copy_callee.caller_addresses[1] << " -> 0x"
        << post_exec_tail_flag_gate_copy_callee.entry_address << "; 0x"
        << post_exec_tail_flag_gate_copy_callee.transferred_byte_count << " byte 0x"
        << post_exec_tail_flag_gate_copy_callee.source_address << " -> 0x"
        << post_exec_tail_flag_gate_copy_callee.destination_address << ", D1 loop 0x"
        << static_cast<unsigned>(post_exec_tail_flag_gate_copy_callee.delay_loop_counter)
        << ", gate 0x"
        << post_exec_tail_flag_gate_copy_callee.gate_word_address << ", RTS 0x"
        << post_exec_tail_flag_gate_copy_callee.terminal_return_address
        << " (cell values, transfer, loop, and increment unmodelled)" << std::dec << '\n';
    const auto title_response_queue =
        eon::parse_deuteros_amiga_title_response_queue_profile(disk, plan);
    std::cout << "          Title response queue boundary: entry 0x" << std::hex
        << title_response_queue.entry_address << "; pending word 0x"
        << title_response_queue.pending_word_address << ", bytes 0x"
        << title_response_queue.byte_region_address << ", shift 0x" << std::dec
        << title_response_queue.shift_byte_count << ", RTS 0x" << std::hex
        << title_response_queue.return_address << "; SHA-256 "
        << title_response_queue.sha256 << std::dec
        << " (runtime queue/callback input unmodelled)\n";
    const auto title_callback =
        eon::parse_deuteros_amiga_title_callback_registration_profile(disk, plan);
    std::cout << "          Title callback boundary: registration 0x" << std::hex
        << title_callback.registration_entry_address << " -> callback 0x"
        << title_callback.callback_address << "; Exec base 0x"
        << title_callback.exec_base_address << " vector -0x"
        << static_cast<std::uint16_t>(-title_callback.exec_vector)
        << "; byte-one table 0x" << title_callback.callback_source_table_address
        << " +0x" << title_callback.callback_source_table_byte_count << " -> queue 0x"
        << title_callback.callback_destination_address << ", pending 0x"
        << title_callback.callback_pending_word_address << "; byte-two gate 0x"
        << title_callback.callback_second_event_gate_address << " -> service 0x"
        << title_callback.callback_second_event_service_address << std::dec
        << " (static provenance only; no Exec/callback/input execution)\n";
    std::cout << "          Timed title transition: 0x" << std::hex
        << title_stage.transition_source_palette_address << " -> 0x"
        << title_stage.transition_work_palette_address << ", " << std::dec
        << title_stage.transition_palette_word_count << " RGB4 words, mask 0x"
        << std::hex << title_stage.transition_palette_mask << std::dec << '\n';
    std::cout << "          Bootstrap profile table: entry 5 0x" << std::hex
        << title_stage.bootstrap_profile_five_address << " -> BSR 0x"
        << title_stage.bootstrap_profile_five_first_call_address << std::dec
        << " (return intentionally unmodelled)\n";
    std::cout << "          Profile 5 helper: controller cell 0x" << std::hex
        << title_stage.bootstrap_profile_five_helper_controller_cell << ", writes +0x"
        << title_stage.bootstrap_profile_five_helper_long_offset << "=0x"
        << title_stage.bootstrap_profile_five_helper_long_value << ", +0x"
        << title_stage.bootstrap_profile_five_helper_word_offset << "=0x"
        << title_stage.bootstrap_profile_five_helper_word_value << std::dec
        << ", +0x" << std::hex
        << title_stage.bootstrap_profile_five_helper_byte_offset << "=0x"
        << static_cast<unsigned>(title_stage.bootstrap_profile_five_helper_byte_value) << std::dec
        << ", then vector -0x1c8 (return unmodelled)\n";
    std::cout << "          Transition gate: counter 0x" << std::hex
        << title_stage.timer_counter_address << " >= 0x" << title_stage.timer_threshold
        << ", skip when word 0x" << title_stage.timer_dispatch_inhibit_address
        << " == 0x" << title_stage.timer_dispatch_inhibit_value
        << "; return clears 0x" << title_stage.timer_counter_reset_address << std::dec << '\n';
    std::cout << "          Transition return: compares 0x" << std::hex
        << title_stage.transition_first_compare_address << ", 0x"
        << title_stage.transition_second_compare_address << ", 0x"
        << title_stage.transition_third_compare_address << "; second phase 0x"
        << title_stage.transition_second_phase_source_address << " -> 0x"
        << title_stage.transition_second_phase_first_work_address << "/0x"
        << title_stage.transition_second_phase_second_work_address << ", slot 0x"
        << title_stage.transition_second_phase_work_pointer_address << ", rts 0x"
        << title_stage.transition_return_address << std::dec << '\n';
    std::cout << "          Post-transition control: word 0x" << std::hex
        << title_stage.post_transition_control_address << " reset to 0; helper chain 0x"
        << title_stage.post_transition_first_helper_address << " -> 0x"
        << title_stage.post_transition_second_helper_address << " -> 0x"
        << title_stage.post_transition_third_helper_address << " -> 0x"
        << title_stage.post_transition_response_helper_address << "; response 0x"
        << title_stage.post_transition_response_code << ", compares 0x"
        << title_stage.post_transition_first_compare_value << "/0x"
        << title_stage.post_transition_second_compare_value << "/0x"
        << title_stage.post_transition_third_compare_value << ", rts 0x"
        << title_stage.post_transition_return_address << std::dec << '\n';
    std::cout << "          Title-stage selector: 0x" << std::hex
        << title_stage.post_transition_selector_address << " masks D0 with 0x"
        << title_stage.post_transition_selector_input_mask << ", divides by 0x"
        << title_stage.post_transition_selector_first_divisor << "/0x"
        << title_stage.post_transition_selector_second_divisor << ", adds 0x"
        << title_stage.post_transition_selector_addend << ", clears 0x"
        << title_stage.post_transition_selector_flag_address << ", then jumps within title stage to 0x"
        << title_stage.post_transition_selector_dispatch_address << std::dec << '\n';
    std::cout << "          Selector dispatch: signed byte 0x" << std::hex
        << title_stage.post_transition_dispatch_state_address << " zero/positive branches 0x"
        << title_stage.post_transition_dispatch_zero_branch_address << "/0x"
        << title_stage.post_transition_dispatch_positive_branch_address
        << " (sibling byte-combine route); zero's second state 0x"
        << title_stage.post_transition_dispatch_zero_variant_state_address
        << " selects 0x" << title_stage.post_transition_dispatch_zero_clear_variant_address
        << "/0x" << title_stage.post_transition_dispatch_zero_set_variant_address
        << "; negative path calls 0x" << title_stage.post_transition_dispatch_negative_service_address
        << " with D0/D1=0x" << title_stage.post_transition_dispatch_negative_service_d0 << "/0x"
        << title_stage.post_transition_dispatch_negative_service_d1 << " unless D0=0x"
        << title_stage.post_transition_dispatch_negative_suppress_value << ", delay 0x"
        << title_stage.post_transition_dispatch_negative_delay << std::dec << '\n';
    std::cout << "          Title exits: 0x" << std::hex
        << title_stage.title_exit_first_address << "/0x"
        << title_stage.title_exit_second_address << "/0x"
        << title_stage.title_exit_third_address << " select bootstrap profiles " << std::dec
        << title_stage.title_exit_first_profile << "/" << title_stage.title_exit_second_profile
        << "/" << title_stage.title_exit_third_profile << "; bootstrap 0x" << std::hex
        << title_stage.title_exit_controller_address << " resolves all to profile " << std::dec
        << title_stage.title_exit_resolved_profile << " main entry 0x" << std::hex
        << title_stage.title_exit_main_stage_entry_address << std::dec << '\n';
}

void report_millennium_dos(const eon::ReleaseArchive& release) {
    if (release.language == "es") {
        // The Spanish release is a genuine FAT12 floppy rather than a loose
        // file archive. Read the verified image in memory; never unpack it
        // into a runtime directory.
        constexpr auto spanish_image_sha256 =
            "1cb7d399ab22110317b1c7486a575c00895f12a17268d0c984ac264a5695961d";
        const auto image = eon::extract_verified_release_asset(release, spanish_image_sha256);
        if (!image) throw std::runtime_error("Verified Spanish Millennium floppy missing");
        const eon::Fat12Disk disk{std::span<const std::uint8_t>(*image)};
        const auto* title_entry = disk.find("TITLE.LIB");
        const auto* static_entry = disk.find("2200AD4.BIN");
        const auto* ibm_entry = disk.find("IBM.COM");
        const auto* manual_entry = disk.find("MILL.BAT");
        const auto* titles_entry = disk.find("TITLES.EXE");
        const auto* game_entry = disk.find("2200AD.EXE");
        if (!title_entry || !static_entry || !ibm_entry || !manual_entry || !titles_entry || !game_entry) {
            throw std::runtime_error("Verified Spanish Millennium media missing title data");
        }
        // MillenniumDosLib is a non-owning view. Keep the FAT12 file bytes
        // alive through resource lookup and decode; the original image stays
        // read-only in memory throughout.
        const auto title_library_bytes = disk.read(*title_entry);
        const eon::MillenniumDosLib title_lib(title_library_bytes);
        const auto* p00 = title_lib.find("P00");
        if (!p00) throw std::runtime_error("Verified Spanish TITLE.LIB has no P00 entry");
        const auto resource = title_lib.read(*p00);
        const auto bitmap = eon::decode_millennium_dos_bitmap(resource);
        const auto palette = eon::decode_millennium_dos_palette(resource, bitmap);
        const auto static_data = disk.read(*static_entry);
        const auto game_data = eon::parse_millennium_dos_game_data(static_data);
        const auto static_evidence = eon::parse_millennium_dos_static_data_evidence(static_data);
        const auto control_text = eon::parse_millennium_dos_control_text_evidence(static_data);
        const auto launch_manual = eon::parse_millennium_dos_spanish_launch_manual(
            disk.read(*manual_entry));
        std::cout << "          Spanish FAT12: " << disk.root_entries().size()
            << " root files; TITLE.LIB P00 " << bitmap.width << 'x' << bitmap.height
            << ", RGB6 DAC entries 256, logical translation "
            << palette.logical_to_dac.size() << '\n';
        std::cout << "          Spanish 2200AD4.BIN: " << game_data.celestial_labels.size()
            << " original celestial labels at 0x" << std::hex
            << static_evidence.celestial_table_offset << std::dec << " ("
            << game_data.celestial_labels[4].text << ")\n";
        std::cout << "          Spanish 2200AD4.BIN static text: " << static_evidence.pointer_count
            << " original pointers to " << static_evidence.raw_record_count
            << " raw records; source SHA-256 " << static_evidence.source_sha256 << " (read-only)\n";
        std::cout << "          Spanish static-text topology anchors:";
        for (const auto& anchor : static_evidence.topology_anchors) {
            std::cout << " " << anchor.table_index << "->0x" << std::hex
                << anchor.target_offset << std::dec;
        }
        std::cout << " (original pointer table only)\n";
        std::cout << "          Spanish control-text provenance: pointers";
        for (const auto& literal : control_text.literals) {
            std::cout << " 0x" << std::hex << literal.record_offset << "/0x"
                << literal.literal_offset << std::dec << " (" << literal.record_sha256 << ")";
        }
        std::cout << " (original text only; no host control binding)\n";
        std::cout << "          Spanish MILL.BAT: " << launch_manual.original_text.size()
            << " original launcher-documentation bytes (SHA-256 " << launch_manual.sha256 << ")\n";
        const auto ibm_handoff = eon::parse_millennium_dos_spanish_ibm_handoff_evidence(
            disk.read(*ibm_entry), disk.read(*titles_entry), disk.read(*game_entry));
        const auto game_startup = eon::parse_millennium_dos_spanish_game_startup_evidence(
            disk.read(*game_entry));
        const auto game_startup_callees = eon::parse_millennium_dos_spanish_game_startup_callees(
            disk.read(*game_entry), game_startup);
        const auto game_startup_followups = eon::parse_millennium_dos_spanish_game_startup_followups(
            disk.read(*game_entry), game_startup_callees);
        std::cout << "          Spanish IBM.COM handoff: caller 0x" << std::hex
            << ibm_handoff.caller_entry_address << " names 0x" << ibm_handoff.title_name_address
            << "/0x" << ibm_handoff.game_name_address << "; calls 0x"
            << ibm_handoff.first_call_address << "/0x" << ibm_handoff.second_call_address
            << " -> 0x" << ibm_handoff.callee_address << "; JNE 0x"
            << ibm_handoff.first_nonzero_branch_address << "/0x"
            << ibm_handoff.second_nonzero_branch_address << "; DOS EXEC AX=0x"
            << ibm_handoff.exec_ax << " INT 0x" << static_cast<unsigned>(ibm_handoff.exec_interrupt)
            << " ES:BX=CS:0x" << ibm_handoff.exec_parameter_block_address
            << "; carry branch 0x" << ibm_handoff.carry_branch_address << " -> 0x"
            << ibm_handoff.carry_branch_target_address << "; SHA-256 "
            << ibm_handoff.ibm_sha256 << std::dec
            << " (static only; no DOS call, result, title, or game ABI executed)\n";
        std::cout << "          Spanish 2200AD startup: COM entry -> 0x" << std::hex
            << game_startup.startup_entry_address << "; SS:SP=CS:0x"
            << game_startup.stack_pointer << "; AX=0x" << game_startup.private_function
            << " ES:BX=CS:0x" << game_startup.private_record_address << "; CALL 0x"
            << game_startup.private_call_address << " -> 0x" << game_startup.private_wrapper_address
            << "; AL==0x" << static_cast<unsigned>(game_startup.compared_al_value)
            << " calls 0x" << game_startup.equal_call_target_address << ", otherwise 0x"
            << game_startup.other_call_target_address << "; SHA-256 " << game_startup.startup_sha256
            << std::dec << " (static only; no private result, branch, or game state is supplied)\n";
        std::cout << "          Spanish startup callees: AL==0x1 path 0x" << std::hex
            << game_startup_callees.equal_entry_address << " -> private 0x"
            << game_startup_callees.equal_private_target_address << ", local 0x"
            << game_startup_callees.equal_followup_target_address << ", then stores 0x1 at 0x"
            << game_startup_callees.equal_result_storage_address << "; other path 0x"
            << game_startup_callees.other_entry_address << " -> private 0x"
            << game_startup_callees.other_private_target_address << ", local 0x"
            << game_startup_callees.other_followup_target_address << ", compares 0x"
            << static_cast<unsigned>(game_startup_callees.other_compare_value) << " at 0x"
            << game_startup_callees.other_result_source_address << "; SHA-256 "
            << game_startup_callees.equal_sha256 << "/" << game_startup_callees.other_sha256
            << std::dec << " (call returns and predicates remain unmodeled)\n";
        std::cout << "          Spanish startup follow-ups: 0x" << std::hex
            << game_startup_followups.equal_entry_address << " stores 0x"
            << static_cast<unsigned>(game_startup_followups.equal_literal_value) << " at 0x"
            << game_startup_followups.equal_storage_address << "; 0x"
            << game_startup_followups.palette_entry_address << " initializes CX=0x" << std::hex
            << game_startup_followups.palette_initial_cx << " before original AX=0x"
            << game_startup_followups.bios_ax << " INT 0x"
            << game_startup_followups.bios_interrupt << " palette table 0x"
            << game_startup_followups.palette_table_address << "; SHA-256 "
            << game_startup_followups.palette_sha256 << "/"
            << game_startup_followups.palette_table_sha256 << std::dec
            << " (static only; no BIOS call, branch, or presentation is executed)\n";
        std::cout << "          Spanish isolation boundary: only this image's IBM.COM, TITLES.EXE, "
            << "and 2200AD.EXE are reported; no English executable, state, or title path is "
            << "substituted. Next trace inputs: hash-locked DOS child return/AL, file operations, "
            << "and CPU/interrupt events for this Spanish image.\n";
        return;
    }
    constexpr auto title_lib_sha256 =
        "6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678";
    const auto title_bytes = eon::extract_verified_release_asset(release, title_lib_sha256);
    if (!title_bytes) return;
    const eon::MillenniumDosLib title_lib(*title_bytes);
    constexpr auto gx_lib_sha256 =
        "4adf9991226deab4749ac07ad637851994f57d11f6dc45f3f5ce862b5bc34c2f";
    const auto gx_bytes = eon::extract_verified_release_asset(release, gx_lib_sha256);
    if (!gx_bytes) throw std::runtime_error("Verified Millennium GX.LIB missing");
    const auto gx_canvas = eon::parse_millennium_dos_gameplay_screen(*gx_bytes);
    const auto gx_catalog = eon::inspect_millennium_dos_gx_bitmap_catalog(*gx_bytes);
    std::cout << "          GX.LIB IMG00 -> IMG01: " << gx_canvas.canvas.width << 'x'
        << gx_canvas.canvas.height << " original indexed canvas\n";
    std::cout << "          GX.LIB bitmap catalogue: " << gx_catalog.resource_count
        << " hash-verified original records; " << gx_catalog.bitmap_decoder_admitted_count
        << " decoder-admitted, " << gx_catalog.bitmap_decoder_boundary_count << " explicit format boundaries, "
        << gx_catalog.decoded_pixel_count
        << " decoded indexed pixels (catalogue only; no screen semantics inferred)\n";
    if (gx_catalog.bitmap_decoder_boundary_count != 0) {
        std::cout << "          GX.LIB decoder boundaries:";
        for (const auto& resource : gx_catalog.resources) {
            if (!resource.bitmap_decoder_admitted) {
                std::cout << ' ' << resource.name << " (" << resource.decoder_boundary << ')';
            }
        }
        std::cout << " (no alternate codec or clipped output is used)\n";
    }
    constexpr auto last_lib_sha256 =
        "a3f5c0b447795881dd4cd5316a091ecc218b1bf563f567b6fe3f093f89781510";
    const auto last_bytes = eon::extract_verified_release_asset(release, last_lib_sha256);
    if (!last_bytes) throw std::runtime_error("Verified Millennium LAST.LIB missing");
    const auto last_screen = eon::parse_millennium_dos_last_screen(*last_bytes);
    std::cout << "          LAST.LIB last: " << last_screen.bitmap.width << 'x'
        << last_screen.bitmap.height << " original indexed screen, RGB6 DAC entries 256\n";
    constexpr auto titles_sha256 =
        "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
    constexpr auto launcher_sha256 =
        "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e";
    const auto titles = eon::extract_verified_release_asset(release, titles_sha256);
    const auto launcher = eon::extract_verified_release_asset(release, launcher_sha256);
    if (!titles || !launcher) throw std::runtime_error("Verified Millennium title flow assets missing");
    const auto flow = eon::parse_millennium_dos_title_flow(*titles, *launcher);
    const auto sound_selection = eon::parse_millennium_dos_sound_selection(*launcher);
    const auto sound_blaster = eon::extract_verified_release_asset(release,
        "be5a00e0b71d893a3aeaaa1127b1e5b870fe734dc876e636c6a933b6444f1b72");
    const auto covox = eon::extract_verified_release_asset(release,
        "99e110b91534206a6b83680a3e11cceadd0e5ddf863560aed53dcbd2c49df7c4");
    if (!sound_blaster || !covox) {
        throw std::runtime_error("Verified Millennium sound-driver leaves missing");
    }
    const auto sound_blaster_leaf = eon::admit_millennium_dos_sound_driver_leaf(*sound_blaster);
    const auto covox_leaf = eon::admit_millennium_dos_sound_driver_leaf(*covox);
    const auto title_exit = eon::parse_millennium_dos_title_exit_closure(*titles);
    const auto presentation = eon::parse_millennium_dos_title_presentation_assets(title_lib, flow);
    const auto& transition = presentation.transition;
    std::cout << "          TITLE.LIB P00: " << presentation.base_bitmap.width << 'x'
        << presentation.base_bitmap.height << ", codec "
        << static_cast<unsigned>(presentation.base_bitmap.codec) << ", indices 0.."
        << static_cast<unsigned>(presentation.base_bitmap.max_palette_index)
        << ", RGB6 DAC entries 256, logical translation "
        << presentation.base_palette.logical_to_dac.size() << ", renderer-ready RGBA "
        << presentation.base_rgba.size() << " bytes (original P00 only)\n";
    std::cout << "          TITLES.EXE: resource " << flow.title_resource_index
        << ", " << flow.intro_transition_steps << " transition steps, key poll INT 0x"
        << std::hex << static_cast<unsigned>(flow.input_interrupt) << "; selection JLE 0x"
        << flow.title_selection_callee_branch_address << " -> 0x"
        << flow.title_selection_callee_branch_target << ", target CALL 0x"
        << flow.title_selection_callee_jle_target_call_address << " -> 0x"
        << flow.title_selection_callee_jle_target_call_target << std::dec
        << "; launcher hand-off " << flow.launcher_title_program << " -> "
        << flow.launcher_game_program << " (DX 0x" << std::hex
        << flow.launcher_title_program_address << " / 0x"
        << flow.launcher_game_program_address << ", near-call target 0x"
        << flow.launcher_common_call_target << ", JC 0x"
        << flow.launcher_common_branch_address << " -> 0x"
        << flow.launcher_common_branch_target << " (static boundary 0x"
        << flow.launcher_common_branch_target_static_boundary << "); pre-title JE 0x"
        << flow.launcher_pre_title_gate_address << " -> 0x"
        << flow.launcher_pre_title_gate_target << ", near-call 0x"
        << flow.launcher_pre_title_call_address << " -> 0x"
        << flow.launcher_pre_title_call_target << " (JNC 0x"
        << flow.launcher_pre_title_callee_branch_address << " -> 0x"
        << flow.launcher_pre_title_callee_branch_target << ", fallthrough JMP 0x"
        << flow.launcher_pre_title_callee_fallthrough_jump_address << " -> 0x"
        << flow.launcher_pre_title_callee_fallthrough_jump_target << "; target JC 0x"
        << flow.launcher_pre_title_callee_jnc_target_branch_address << " -> 0x"
        << flow.launcher_pre_title_callee_jnc_target_branch_target << "; JC target JMP 0x"
        << flow.launcher_pre_title_callee_jc_target_jump_address << " -> 0x"
        << flow.launcher_pre_title_callee_jc_target_jump_target << "; join JE 0x"
        << flow.launcher_pre_title_callee_join_branch_address << " -> 0x"
        << flow.launcher_pre_title_callee_join_branch_target << " (terminal opcode 0x"
        << flow.launcher_pre_title_callee_join_branch_terminal_address << "); private INT 0x"
        << static_cast<unsigned>(flow.launcher_private_interrupt_number) << " loader 0x"
        << flow.launcher_private_interrupt_loader_call_address << " -> 0x"
        << flow.launcher_private_interrupt_loader_call_target << ", setup 0x"
        << flow.launcher_private_interrupt_install_address << " offset 0x"
        << flow.launcher_private_interrupt_handler_offset << std::dec
        << " (loader reads " << flow.launcher_private_interrupt_handler_first_program
        << "/" << flow.launcher_private_interrupt_handler_other_program << " to DS:0x"
        << std::hex << flow.launcher_private_interrupt_handler_destination_offset
        << "; command-tail e/E/m/M scan 0x" << flow.launcher_video_selection_scan_address
        << " maps selectors 1/2, empty tail calls 0x"
        << flow.launcher_video_selection_default_detector_address
        << "; numeric segment unmodelled; raw save 0x"
        << flow.launcher_private_interrupt_saved_offset_cell << "/0x"
        << flow.launcher_private_interrupt_saved_segment_cell << ", restore 0x"
        << flow.launcher_private_interrupt_restore_address << std::dec << "))\n";
    std::cout << "          MILL.COM sound selection: routine 0x" << std::hex
        << sound_selection.selector_entry_address << " accepts 0/1/2 -> table slots "
        << static_cast<unsigned>(sound_selection.ibm_speaker_table_slot) << "/"
        << static_cast<unsigned>(sound_selection.sound_blaster_table_slot) << "/"
        << static_cast<unsigned>(sound_selection.covox_table_slot) << " ("
        << sound_selection.ibm_speaker_filename << "/" << sound_blaster_leaf.original_filename
        << "/" << covox_leaf.original_filename << "); admitted original leaves "
        << std::dec << sound_blaster_leaf.byte_size << "/" << covox_leaf.byte_size
        << " bytes; table slot " << static_cast<unsigned>(sound_selection.missing_srol_table_slot) << " "
        << sound_selection.missing_srol_filename
        << " is absent (no fallback or sound-driver execution)\n" << std::dec;
    std::cout << "          TITLES.EXE local exit: 0x" << std::hex
        << title_exit.nonzero_entry_address << " calls 0x"
        << title_exit.private_driver_target_address << "/0x"
        << title_exit.post_driver_target_address << ", clears 0x"
        << title_exit.status_storage_address << ", then jumps to 0x"
        << title_exit.exit_stub_address << " (pre-exit CALL 0x"
        << title_exit.exit_stub_preceding_call_target_address << ", INT 0x"
        << static_cast<unsigned>(title_exit.exit_interrupt) << " AH=0x"
        << static_cast<unsigned>(title_exit.exit_service)
        << "; static boundary only)\n" << std::dec;
    std::cout << "          TITLE.LIB P01-P25: " << transition.patches.size()
        << " decoded " << transition.patches.front().bitmap.width << 'x'
        << transition.patches.front().bitmap.height
        << " patches; record bank +0x" << std::hex << transition.source_bank_offset
        << "+0x" << transition.source_bank_size << " SHA-256 "
        << transition.source_bank_sha256 << std::dec
        << " (static order/provenance only; no timing, composition, or frame claimed)\n";
    const auto ega640 = eon::extract_verified_release_asset(release,
        "ba003dd155fee868980f6ece933c33f9b22af68ed376cd64f4e027abd65baf6a");
    const auto mcga = eon::extract_verified_release_asset(release,
        "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228");
    if (!ega640 || !mcga) throw std::runtime_error("Verified Millennium video driver missing");
    const auto ega_driver = eon::parse_millennium_dos_video_driver(*ega640,
        eon::MillenniumDosVideoDriverKind::ega640);
    const auto mcga_driver = eon::parse_millennium_dos_video_driver(*mcga,
        eon::MillenniumDosVideoDriverKind::mcga);
    std::cout << "          private INT 91 video ABI: EGA640 AX=0 -> BIOS mode 0x"
        << std::hex << static_cast<unsigned>(ega_driver.function_zero_video_mode)
        << " at 0x" << ega_driver.function_zero_set_mode_interrupt_site
        << "; MCGA AX=0 -> BIOS mode 0x"
        << static_cast<unsigned>(mcga_driver.function_zero_video_mode) << " at 0x"
        << mcga_driver.function_zero_set_mode_interrupt_site
        << "; AX=4 masks ES:BX[0] with 0x"
        << static_cast<unsigned>(ega_driver.function_four_input_mask)
        << "; AX=0x1f returns AH=0x"
        << static_cast<unsigned>(ega_driver.function_thirty_one_return_ah)
        << "/0x" << static_cast<unsigned>(mcga_driver.function_thirty_one_return_ah)
        << "; AX=0x13 polls VGA status 0x" << ega_driver.function_thirteen_status_port
        << " mask 0x" << static_cast<unsigned>(ega_driver.function_thirteen_retrace_mask)
        << " (read-only retrace wait, no host I/O)"
        << " with driver-local AL at 0x" << ega_driver.function_thirty_one_state_address
        << "/0x" << mcga_driver.function_thirty_one_state_address
        << " (validated ABI only; no BIOS/driver execution)\n" << std::dec;
    constexpr auto static_data_sha256 =
        "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d";
    const auto static_data = eon::extract_verified_release_asset(release, static_data_sha256);
    if (!static_data) throw std::runtime_error("Verified Millennium static game data missing");
    const auto game_data = eon::parse_millennium_dos_game_data(*static_data);
    const auto static_evidence = eon::parse_millennium_dos_static_data_evidence(*static_data);
    const auto control_text = eon::parse_millennium_dos_control_text_evidence(*static_data);
    std::cout << "          2200AD4.BIN: " << game_data.celestial_labels.size()
        << " original celestial labels at 0x" << std::hex
        << static_evidence.celestial_table_offset << std::dec << " ("
        << game_data.celestial_labels[4].text << ")\n";
    std::cout << "          2200AD4.BIN static text: " << static_evidence.pointer_count
        << " original pointers to " << static_evidence.raw_record_count
        << " raw records; source SHA-256 " << static_evidence.source_sha256 << " (read-only)\n";
    std::cout << "          Static-text topology anchors:";
    for (const auto& anchor : static_evidence.topology_anchors) {
        std::cout << " " << anchor.table_index << "->0x" << std::hex
            << anchor.target_offset << std::dec;
    }
    std::cout << " (original pointer table only)\n";
    std::cout << "          Control-text provenance: pointers";
    for (const auto& literal : control_text.literals) {
        std::cout << " 0x" << std::hex << literal.record_offset << "/0x"
            << literal.literal_offset << std::dec << " (" << literal.record_sha256 << ")";
    }
    std::cout << " (original text only; no host control binding)\n";
    constexpr auto game_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    const auto game = eon::extract_verified_release_asset(release, game_sha256);
    if (!game) throw std::runtime_error("Verified Millennium DOS executable missing");
    constexpr auto gx_overlay_sha256 =
        "093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb";
    const auto gx_overlay = eon::extract_verified_release_asset(release, gx_overlay_sha256);
    if (!gx_overlay) throw std::runtime_error("Verified Millennium DOS GX overlay missing");
    const auto game_flow = eon::parse_millennium_dos_game_flow(*game);
    const auto sound_effect_names = eon::parse_millennium_dos_sound_effect_name_table_evidence(*game);
    std::cout << "          2200AD.EXE SFX name bank: " << sound_effect_names.filenames.size()
        << " original VOC names at 0x" << std::hex << sound_effect_names.table_address
        << " (SHA-256 " << sound_effect_names.table_sha256
        << "; static only, no event mapping or playback)\n" << std::dec;
    // The executable names an exact original VOC family, but no recovered
    // caller selects an entry or invokes a sound-driver ABI. Decode it here
    // only as a hash-verified inspection diagnostic: SDL must not play a
    // voice merely because its bytes are available.
    const auto inventory = eon::inventory_verified_release(release);
    std::size_t decoded_voice_count = 0;
    std::size_t decoded_pcm_sample_count = 0;
    std::array<std::uint32_t, 2> voice_sample_rates{};
    std::size_t voice_sample_rate_count = 0;
    for (const auto& filename : sound_effect_names.filenames) {
        const auto asset = std::find_if(inventory.begin(), inventory.end(),
            [filename](const eon::ArchiveAsset& candidate) {
                const auto separator = candidate.path.find_last_of('/');
                const auto leaf = separator == std::string::npos
                    ? std::string_view(candidate.path)
                    : std::string_view(candidate.path).substr(separator + 1U);
                return candidate.kind == eon::AssetKind::audio && leaf == filename;
            });
        if (asset == inventory.end()) {
            throw std::runtime_error("Verified Millennium DOS VOC named by executable is missing");
        }
        const auto bytes = eon::extract_verified_release_asset(release, asset->sha256);
        if (!bytes) throw std::runtime_error("Verified Millennium DOS VOC cannot be read");
        const auto voice = eon::decode_creative_voice(*bytes);
        if (voice.unsigned_pcm.size() > std::numeric_limits<std::size_t>::max() - decoded_pcm_sample_count) {
            throw std::runtime_error("Millennium DOS VOC diagnostic sample count overflows");
        }
        decoded_pcm_sample_count += voice.unsigned_pcm.size();
        if (std::find(voice_sample_rates.begin(), voice_sample_rates.begin() + voice_sample_rate_count,
                voice.sample_rate) == voice_sample_rates.begin() + voice_sample_rate_count) {
            if (voice_sample_rate_count == voice_sample_rates.size()) {
                throw std::runtime_error("Unsupported Millennium DOS VOC sample-rate family");
            }
            voice_sample_rates[voice_sample_rate_count++] = voice.sample_rate;
        }
        ++decoded_voice_count;
    }
    std::cout << "          Original VOC bank: " << decoded_voice_count << " hash-verified voices, "
        << decoded_pcm_sample_count << " unsigned PCM samples at ";
    for (std::size_t index = 0; index < voice_sample_rate_count; ++index) {
        if (index != 0) std::cout << '/';
        std::cout << voice_sample_rates[index] << " Hz";
    }
    std::cout << " (inspection only; no event mapping, driver ABI, or playback)\n";
    const auto startup_allocation = eon::parse_millennium_dos_startup_allocation_boundary(*game);
    const auto startup_zero_path = eon::parse_millennium_dos_startup_zero_path_boundary(*game);
    const auto startup_zero_continuation =
        eon::parse_millennium_dos_startup_zero_continuation_boundary(*game);
    const auto startup_post_allocation =
        eon::parse_millennium_dos_startup_post_allocation_boundary(*game);
    const auto startup_post_release =
        eon::parse_millennium_dos_startup_post_release_continuation(*game);
    const auto startup_post_gx_loader =
        eon::parse_millennium_dos_startup_post_gx_loader_boundary(*game);
    const auto private_int91 = eon::parse_millennium_dos_private_int91_wrapper(*game);
    const auto post_int91_selector = eon::parse_millennium_dos_post_int91_caller_selector(*game);
    const auto post_overlay_adapter =
        eon::parse_millennium_dos_post_overlay_adapter_continuation(*game);
    const auto post_overlay_loop =
        eon::parse_millennium_dos_post_overlay_adapter_loop(*game);
    const auto post_overlay_dispatch =
        eon::parse_millennium_dos_post_overlay_dispatch_prefix(*game);
    const auto startup_nonzero_path = eon::parse_millennium_dos_startup_nonzero_path_boundary(*game);
    const auto gx_overlay_load = eon::parse_millennium_dos_gx_overlay_load_evidence(
        *game, *gx_overlay);
    const auto gx_overlay_adapter = eon::parse_millennium_dos_gx_overlay_adapter_evidence(
        *game, gx_overlay_load);
    const auto gx_overlay_dispatcher = eon::parse_millennium_dos_gx_overlay_dispatcher_evidence(
        *gx_overlay, gx_overlay_adapter);
    const auto gx_overlay_selector = eon::parse_millennium_dos_gx_overlay_selector_evidence(
        *game, *gx_overlay, gx_overlay_adapter, gx_overlay_dispatcher);
    const auto gx_overlay_startup_records =
        eon::parse_millennium_dos_gx_overlay_startup_record_evidence(
            *gx_overlay, gx_overlay_selector);
    const auto gx_overlay_dispatch13 = eon::parse_millennium_dos_gx_overlay_dispatch13_evidence(
        *gx_overlay, gx_overlay_dispatcher);
    std::cout << "          2200AD.EXE startup: entry 0x" << std::hex
        << game_flow.entry_address << ", SS=CS, SP=0x" << game_flow.startup_stack_pointer
        << ", first CALL 0x" << game_flow.startup_first_call_address
        << " -> INT 0x" << static_cast<unsigned>(game_flow.startup_first_call_interrupt)
        << ", static RET 0x" << game_flow.startup_first_call_return_address
        << " (return-site 0x" << game_flow.startup_first_call_return_site
        << ", AX -> 0x" << game_flow.startup_result_word_address
        << ", AH -> 0x" << game_flow.startup_result_high_byte_first_address << "/0x"
        << game_flow.startup_result_high_byte_second_address << ", SP -> 0x"
        << game_flow.startup_stack_snapshot_address << ")"
        << "; AL==$" << static_cast<unsigned>(game_flow.startup_mode_equal_value)
        << " -> 0x" << game_flow.startup_equal_call_address << ", otherwise 0x"
        << game_flow.startup_other_call_address << " (both re-enter INT 0x"
        << static_cast<unsigned>(game_flow.startup_first_call_interrupt) << " wrapper at 0x"
        << game_flow.startup_equal_path_private_call_site << "/0x"
        << game_flow.startup_other_path_private_call_site << "; equal follow-up writes $"
        << static_cast<unsigned>(game_flow.startup_equal_followup_write_value) << " -> 0x"
        << game_flow.startup_equal_followup_write_address << ", other first reaches INT 0x"
        << static_cast<unsigned>(game_flow.startup_other_followup_interrupt_number) << " at 0x"
        << game_flow.startup_other_followup_interrupt_site << " (AH=0x"
        << static_cast<unsigned>(game_flow.startup_other_followup_video_function) << ", AL=0x"
        << static_cast<unsigned>(game_flow.startup_other_followup_video_subfunction) << "); DX!=0 -> 0x"
        << game_flow.startup_nonzero_dx_branch_address << std::dec
        << " (validated boundary only; no native calls executed)\n";
    std::cout << "          2200AD startup continuation: 0x" << std::hex
        << startup_allocation.continuation_entry_address << " CALL 0x"
        << startup_allocation.allocator_entry_address << " reaches INT 0x"
        << static_cast<unsigned>(startup_allocation.allocator_first_external_interrupt)
        << " at 0x" << startup_allocation.allocator_first_external_interrupt_site
        << "; post-call DX==0 -> 0x" << startup_allocation.dx_zero_branch_target
        << ", DX!=0 -> 0x" << startup_allocation.dx_nonzero_jump_target << std::dec
        << " (static boundary only; no DOS result or branch chosen)\n";
    std::cout << "          2200AD DX-zero successor: 0x" << std::hex
        << startup_zero_path.zero_path_entry_address << " -> selector 0x"
        << startup_zero_path.selector_entry_address << " reads 0x"
        << startup_zero_path.selector_mode_byte_address << "; local CALL 0x"
        << startup_zero_path.selector_call_address << " -> 0x"
        << startup_zero_path.selector_call_target << " replaces DX with name 0x"
        << startup_zero_path.security_name_address << " before INT 0x"
        << static_cast<unsigned>(startup_zero_path.first_external_interrupt) << " at 0x"
        << startup_zero_path.first_external_interrupt_site << " (AH=0x"
        << static_cast<unsigned>(startup_zero_path.first_external_service) << ", AL=0x"
        << static_cast<unsigned>(startup_zero_path.first_external_access_mode) << std::dec
        << "; static boundary only, no mode or DOS result supplied)\n";
    std::cout << "          2200AD DX-nonzero successor: 0x" << std::hex
        << startup_nonzero_path.nonzero_entry_address << " loads AL=0x"
        << static_cast<unsigned>(startup_nonzero_path.immediate_al_value) << " then jumps to 0x"
        << startup_nonzero_path.continuation_entry_address << "; local CALL 0x"
        << startup_nonzero_path.continuation_first_call_address << " -> 0x"
        << startup_nonzero_path.continuation_first_call_target << " reaches INT 0x"
        << static_cast<unsigned>(startup_nonzero_path.first_external_interrupt) << " at 0x"
        << startup_nonzero_path.first_external_interrupt_site << " (AH=0x"
        << static_cast<unsigned>(startup_nonzero_path.first_external_service) << std::dec
        << "; static boundary only, no mouse state or return supplied)\n";
    std::cout << "          2200AD DX-zero return continuation: 0x" << std::hex
        << startup_zero_continuation.continuation_entry_address << " reads CS:0x"
        << startup_zero_continuation.source_byte_address << " then calls 0x"
        << startup_zero_continuation.first_local_call_target << "; reaches INT 0x"
        << static_cast<unsigned>(startup_zero_continuation.first_external_interrupt) << " at 0x"
        << startup_zero_continuation.first_external_interrupt_site << " (AH=0x"
        << static_cast<unsigned>(startup_zero_continuation.first_external_service)
        << ", BX=0x" << startup_zero_continuation.allocation_request_paragraphs << std::dec
        << "; conditional static boundary only, no DOS result or allocation supplied)\n";
    std::cout << "          2200AD post-allocation successor: 0x" << std::hex
        << startup_post_allocation.entry_address << " stores BX -> CS:0x"
        << startup_post_allocation.cs_override_store_target_address << ", moves AX -> ES at 0x"
        << startup_post_allocation.es_from_ax_address << ", then reaches INT 0x"
        << static_cast<unsigned>(startup_post_allocation.first_external_interrupt) << " at 0x"
        << startup_post_allocation.first_external_interrupt_site << " (AH=0x"
        << static_cast<unsigned>(startup_post_allocation.first_external_service) << std::dec
        << "; conditional static boundary only, no return or segment meaning supplied)\n";
    std::cout << "          2200AD post-release continuation: 0x" << std::hex
        << startup_post_release.entry_address << " restores DX, loads far cell 0x"
        << startup_post_release.far_pointer_address << " into DX/SI, then calls 0x"
        << startup_post_release.first_call_target << ", static-data loader 0x"
        << startup_post_release.static_data_call_target << ", and GX loader 0x"
        << startup_post_release.gx_loader_call_target << std::dec
        << " (conditional static boundary only; no DOS result, pointer, or calls supplied)\n";
    std::cout << "          2200AD post-GX-loader boundary: 0x" << std::hex
        << startup_post_gx_loader.entry_address << " restores ES from CS, loads BX=0x"
        << startup_post_gx_loader.bx_literal << "/AX=0x" << startup_post_gx_loader.ax_literal
        << ", then calls private wrapper 0x" << startup_post_gx_loader.private_call_target
        << " (INT 0x" << static_cast<unsigned>(startup_post_gx_loader.private_interrupt)
        << std::dec << "; conditional static boundary only, no calls or result supplied)\n";
    std::cout << "          2200AD private wrapper: 0x" << std::hex
        << private_int91.entry_address << " is called from 0x"
        << private_int91.caller_call_address << ", preserves raw stack span through INT 0x"
        << static_cast<unsigned>(private_int91.private_interrupt) << " at 0x"
        << private_int91.private_interrupt_site << ", then has RET at 0x"
        << private_int91.return_address << std::dec
        << " (static wrapper only; no private-interrupt ABI, return, or result supplied)\n";
    std::cout << "          2200AD post-wrapper caller selector: return site 0x" << std::hex
        << post_int91_selector.return_site_address << " compares CS:0x"
        << post_int91_selector.source_byte_address << " against 0x"
        << static_cast<unsigned>(post_int91_selector.first_compare_value) << "/0x"
        << static_cast<unsigned>(post_int91_selector.second_compare_value) << "/0x"
        << static_cast<unsigned>(post_int91_selector.third_compare_value)
        << ", writes DX to CS:0x" << post_int91_selector.shared_store_target_address
        << ", then CALLs 0x" << post_int91_selector.first_call_target << std::dec
        << " (conditional static prefix only; no wrapper return, state, or callee effect supplied)\n";
    std::cout << "          2200AD post-overlay-adapter continuation: return site 0x" << std::hex
        << post_overlay_adapter.return_site_address << " has CALL targets 0x"
        << post_overlay_adapter.initial_call_targets[0] << "/0x"
        << post_overlay_adapter.initial_call_targets[1] << "/0x"
        << post_overlay_adapter.initial_call_targets[2] << "/0x"
        << post_overlay_adapter.initial_call_targets[3] << "/0x"
        << post_overlay_adapter.initial_call_targets[4] << "/0x"
        << post_overlay_adapter.initial_call_targets[5] << "; CS:0x"
        << post_overlay_adapter.mode_byte_address << " == 0x"
        << static_cast<unsigned>(post_overlay_adapter.mode_equal_value) << " branches to 0x"
        << post_overlay_adapter.equal_branch_target << ", otherwise CALLs 0x"
        << post_overlay_adapter.other_call_target << ", converging at 0x"
        << post_overlay_adapter.convergence_address << std::dec
        << " (conditional static prefix only; no call return, state, or target effect supplied)\n";
    std::cout << "          2200AD post-overlay-adapter loop: 0x" << std::hex
        << post_overlay_loop.entry_address << " has " << std::dec
        << post_overlay_loop.call_addresses.size() << " hash-bound CALLs, tests AL at 0x"
        << std::hex << post_overlay_loop.first_al_test_address << "/0x"
        << post_overlay_loop.loop_al_test_address << ", and branches to 0x"
        << post_overlay_loop.loop_zero_branch_target << " before dispatcher 0x"
        << post_overlay_loop.following_dispatch_address << std::dec
        << " (conditional static span only; no calls, byte values, branches, or target effects supplied)\n";
    std::cout << "          2200AD post-overlay dispatch: action 0x" << std::hex
        << static_cast<unsigned>(post_overlay_dispatch.first_action_value) << " -> 0x"
        << post_overlay_dispatch.first_action_call_target << ", action 0x"
        << static_cast<unsigned>(post_overlay_dispatch.second_action_value) << " -> 0x"
        << post_overlay_dispatch.second_action_call_target << "; guard 0x"
        << post_overlay_dispatch.guard_byte_address << " and table 0x"
        << post_overlay_dispatch.table_base_address << " remain native; scaled CALL 0x"
        << post_overlay_dispatch.scaled_call_target << std::dec
        << " (validated dispatcher only; no input or call effects)\n";
    std::cout << "          2200GX.EXE overlay evidence: name 0x" << std::hex
        << gx_overlay_load.source_name_address << ", loader 0x"
        << gx_overlay_load.loader_entry_address << " reads segment cell 0x"
        << gx_overlay_load.loader_segment_cell_address << "; calls 0x"
        << gx_overlay_load.first_call_address << " -> 0x" << gx_overlay_load.first_call_target
        << "/0x" << gx_overlay_load.second_call_address << " -> 0x"
        << gx_overlay_load.second_call_target << "/0x" << gx_overlay_load.third_call_address
        << " -> 0x" << gx_overlay_load.third_call_target << "; caller 0x"
        << gx_overlay_load.caller_call_address << " -> 0x" << gx_overlay_load.caller_target
        << "; adapter 0x" << gx_overlay_adapter.entry_address << " RETF 0x"
        << gx_overlay_adapter.far_transfer_address << " to overlay offset 0x"
        << gx_overlay_adapter.overlay_entry_offset << "; SHA-256 "
        << gx_overlay_load.overlay_sha256 << std::dec
        << " (static only; no segment, calls, overlay code, or display executed)\n";
    std::cout << "          2200GX.EXE dispatcher: entry +0x" << std::hex
        << gx_overlay_dispatcher.entry_offset << ", table +0x"
        << gx_overlay_dispatcher.table_offset << "; selector 0xe/0xf/0x12/0x14 -> 0x"
        << gx_overlay_dispatcher.observed_selector_targets[0x0e] << "/0x"
        << gx_overlay_dispatcher.observed_selector_targets[0x0f] << "/0x"
        << gx_overlay_dispatcher.observed_selector_targets[0x12] << "/0x"
        << gx_overlay_dispatcher.observed_selector_targets[0x14] << "; near RET then far RETF +0x"
        << gx_overlay_dispatcher.far_return_offset << "; SHA-256 "
        << gx_overlay_dispatcher.dispatch_sha256 << std::dec
        << " (static only; no selector, handler, return, or display executed)\n";
    std::cout << "          2200GX selector prefix: 2200AD 0x" << std::hex
        << gx_overlay_selector.caller_entry_address << " reads 0x"
        << gx_overlay_selector.selector_source_address << "; 0x3/0x4/0x2/default -> AX 0x"
        << gx_overlay_selector.overlay_targets[0] << "/0x"
        << gx_overlay_selector.overlay_targets[1] << "/0x"
        << gx_overlay_selector.overlay_targets[2] << "/0x"
        << gx_overlay_selector.overlay_targets[3] << ", stores DX at 0x"
        << gx_overlay_selector.dx_storage_address << ", CALL 0x"
        << gx_overlay_selector.adapter_call_address << " -> 0x"
        << gx_overlay_selector.adapter_target << "; SHA-256 "
        << gx_overlay_selector.caller_sha256 << std::dec
        << " (static only; no selector value, records, returns, resources, or display executed)\n";
    std::cout << "          2200GX startup records: entries +0x" << std::hex
        << gx_overlay_startup_records.entry_offsets[0] << "/+0x"
        << gx_overlay_startup_records.entry_offsets[1] << "/+0x"
        << gx_overlay_startup_records.entry_offsets[2] << "/+0x"
        << gx_overlay_startup_records.entry_offsets[3] << " select original records +0x"
        << gx_overlay_startup_records.source_record_offsets[0] << "/+0x"
        << gx_overlay_startup_records.source_record_offsets[1] << "/+0x"
        << gx_overlay_startup_records.source_record_offsets[2] << "/+0x"
        << gx_overlay_startup_records.source_record_offsets[3] << "; shared +0x"
        << gx_overlay_startup_records.shared_copy_entry_offset << " copies " << std::dec
        << static_cast<unsigned>(gx_overlay_startup_records.copy_word_count)
        << " words to +0x" << std::hex << gx_overlay_startup_records.copy_destination_offset
        << "; SHA-256 " << gx_overlay_startup_records.entry_span_sha256 << std::dec
        << " (raw startup provenance only; no record meaning, state, or display inferred)\n";
    std::cout << "          2200GX dispatcher slot 13: entry +0x" << std::hex
        << gx_overlay_dispatch13.entry_offset << " resets words +0x"
        << gx_overlay_dispatch13.zeroed_word_storage_offsets[0] << "/+0x"
        << gx_overlay_dispatch13.zeroed_word_storage_offsets[1] << "/+0x"
        << gx_overlay_dispatch13.zeroed_word_storage_offsets[2] << ", compares AL with 0x"
        << static_cast<unsigned>(gx_overlay_dispatch13.first_result_compare_value)
        << ", and returns/loops via +0x" << gx_overlay_dispatch13.local_back_edge_target_offset
        << "; SHA-256 " << gx_overlay_dispatch13.span_sha256 << std::dec
        << " (conditional static span only; no selector, calls, results, or state supplied)\n";
    constexpr auto initial_save_sha256 =
        "a9b3d77534d3d575012f9553bfed9520edf92a83af408c977e7f0fd226a470e7";
    const auto initial_save = eon::extract_verified_release_asset(release, initial_save_sha256);
    if (!initial_save) throw std::runtime_error("Verified Millennium initial save missing");
    const eon::MillenniumDosSaveSession save_session(*initial_save);
    std::cout << "          2200SAVE.I: SHA-256 " << save_session.sha256()
        << ", version 0x" << std::hex << save_session.layout().version << std::dec
        << ", " << save_session.layout().state_table.size()
        << " recovered state-table records\n";
    // These are the literal positional words restored by the verified load
    // path.  Their gameplay meanings remain deliberately unnamed.
    for (std::size_t index = 0; index < save_session.layout().state_table.size(); ++index) {
        const auto& record = save_session.state_record(index);
        std::cout << "            [" << index << "] +00=0x" << std::hex
            << record.runtime_offset_0 << " +04=0x" << record.runtime_offset_4
            << " +06=0x" << record.runtime_offset_6 << " +08=0x"
            << record.runtime_offset_8 << std::dec << '\n';
    }
}

void report_millennium_amiga(const eon::ReleaseArchive& release) {
    std::size_t shared_resident_images = 0;
    std::optional<eon::MillenniumAmigaSharedResidentLayout> shared_resident;
    for (const auto& asset : eon::inventory_verified_release(release)) {
        if (asset.kind != eon::AssetKind::amiga_adf) continue;
        const auto image = eon::extract_verified_release_asset(release, asset.sha256);
        if (!image) throw std::runtime_error("Verified Millennium Amiga ADF is missing");
        const auto layout = eon::parse_millennium_amiga_shared_resident_layout(*image);
        if (shared_resident && (layout.disk_offset != shared_resident->disk_offset
            || layout.length != shared_resident->length
            || layout.destination != shared_resident->destination
            || layout.raw_sha256 != shared_resident->raw_sha256)) {
            throw std::runtime_error("Millennium Amiga shared resident layout differs between variants");
        }
        shared_resident = layout;
        ++shared_resident_images;
    }
    if (shared_resident) {
        std::cout << "          shared resident evidence: " << shared_resident_images
            << " original images, disk 0x" << std::hex << shared_resident->disk_offset
            << " + 0x" << shared_resident->length << " -> memory 0x"
            << shared_resident->destination << std::dec << "; SHA-256 "
            << shared_resident->raw_sha256 << '\n';
    }
    // Defjam's image contains the recovered raw-sector loader. Other supplied
    // crack variants preserve the shared media ranges but patch this stage.
    constexpr auto loader_adf_sha256 =
        "8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c";
    const auto image = eon::extract_verified_release_asset(release, loader_adf_sha256);
    if (!image) return;
    const eon::MillenniumAmigaBootstrapSession live_bootstrap(*image);
    const auto& plan = live_bootstrap.plan();
    const auto& opaque_invocation = live_bootstrap.opaque_invocation_boundary();
    const auto& first_stage_source_anchors = live_bootstrap.first_stage_source_anchors();
    const auto& resident_evidence = live_bootstrap.resident_evidence();
    std::cout << "          bounded launcher bootstrap: resident entry 0x" << std::hex
        << live_bootstrap.resident_entry().entry_address << ", raw resident SHA-256 "
        << live_bootstrap.shared_resident().raw_sha256 << std::dec
        << " (opaque handoff validated; no raw-stage invocation)\n";
    const auto& resident = resident_evidence.entry;
    const auto& splitter = resident_evidence.splitter;
    const auto& helper_boundary = resident_evidence.helper_boundary;
    const auto& setup_helper_boundary = resident_evidence.setup_helper_boundary;
    const auto& staging_callsites = resident_evidence.staging_callsites;
    const auto& first_post_helper_chain = resident_evidence.first_post_helper_chain;
    const auto& second_post_helper_chain = resident_evidence.second_post_helper_chain;
    const auto& staging_reachability = resident_evidence.staging_reachability;
    const auto& separate_post_call = resident_evidence.separate_post_call;
    const auto& separate_post_call_tail = resident_evidence.separate_post_call_tail;
    const auto& separate_post_call_tail_branch = resident_evidence.separate_post_call_tail_branch;
    const auto& separate_comparison = resident_evidence.separate_comparison;
    const auto& separate_byte_gate = resident_evidence.separate_byte_gate;
    const auto& separate_byte_gate_target = resident_evidence.separate_byte_gate_target;
    const auto& separate_byte_gate_convergence = resident_evidence.separate_byte_gate_convergence;
    const auto& separate_byte_gate_taken_branch = resident_evidence.separate_byte_gate_taken_branch;
    const auto& separate_byte_gate_fallthrough = resident_evidence.separate_byte_gate_fallthrough;
    const auto& independent_entry = resident_evidence.independent_entry;
    const auto& negative_d3 = resident_evidence.negative_d3;
    const auto& negative_d3_terminal = resident_evidence.negative_d3_terminal;
    const auto& post_negative_d3 = resident_evidence.post_negative_d3_terminal;
    const auto& post_negative_d3_continuation = resident_evidence.post_negative_d3_continuation;
    std::cout << "          raw loader: disk 0x" << std::hex
        << plan.first_stage.disk_offset << " + 0x" << plan.first_stage.length
        << " -> memory 0x" << plan.first_stage.destination
        << "; disk 0x" << plan.resident_stage.disk_offset << " + 0x"
        << plan.resident_stage.length << " -> entry 0x" << plan.resident_entry
        << ", marker 0x" << plan.loader_magic << std::dec << '\n'
        << "          raw stage SHA-256: bootstrap " << plan.bootstrap_loader.raw_sha256
        << "; first " << plan.first_stage.raw_sha256
        << "; resident " << plan.resident_stage.raw_sha256 << '\n'
        << "          opaque loader handoff: " << std::dec
        << opaque_invocation.byte_count << " bytes at disk 0x" << std::hex
        << opaque_invocation.raw_disk_offset << " (SHA-256 " << opaque_invocation.sha256
        << "); JSR (A3) at 0x" << opaque_invocation.first_stage_invocation_address
        << " -> 0x" << opaque_invocation.first_stage_target
        << ", terminal JMP (A3) at 0x" << opaque_invocation.resident_stage_jump_address
        << " -> 0x" << opaque_invocation.resident_stage_target << std::dec
        << " (opaque; no invocation or return execution)\n"
        << "          first-stage source anchors: disk 0x" << std::hex
        << first_stage_source_anchors.raw_disk_offset << " + 0x"
        << first_stage_source_anchors.byte_count << " (SHA-256 "
        << first_stage_source_anchors.sha256 << "); offsets +0x"
        << first_stage_source_anchors.anchor_stage_offsets[0] << ", +0x"
        << first_stage_source_anchors.anchor_stage_offsets[1] << ", +0x"
        << first_stage_source_anchors.anchor_stage_offsets[2] << std::dec
        << " and two verified source windows (provenance only; no stage decoding)\n"
        << "          resident gate: entry 0x" << std::hex << resident.entry_address
        << " calls 0x" << resident.initializer_address << "; d3 != 0 ORs 0x"
        << resident.d3_nonzero_or_mask << " into d0, stores word at 0x"
        << resident.result_word_address << '\n'
        << "          resident word splitter: entry 0x" << splitter.entry_address
        << " reads A1+0x" << splitter.source_a1_offset << "; low 15-bit words -> 0x"
        << splitter.magnitude_word_addresses[0] << ", 0x" << splitter.magnitude_word_addresses[1]
        << ", 0x" << splitter.magnitude_word_addresses[2] << "; sign bytes -> 0x"
        << splitter.sign_byte_addresses[0] << ", 0x" << splitter.sign_byte_addresses[1]
        << ", 0x" << splitter.sign_byte_addresses[2] << "; helper 0x"
        << splitter.helper_address << std::dec << '\n'
        << "          splitter pre-helper transform is modeled in memory; no raw-resident "
           "caller or helper return effect is yet proven\n"
        << "          helper raw boundary: target 0x" << std::hex << helper_boundary.helper_address
        << " maps to disk 0x" << helper_boundary.raw_disk_offset << std::dec
        << "; 32-byte SHA-256 " << helper_boundary.raw_prefix_sha256
        << " (not treated as an executable helper)\n"
        << "          setup helper raw boundary: target 0x" << std::hex
        << setup_helper_boundary.helper_address << " maps to disk 0x"
        << setup_helper_boundary.raw_disk_offset << std::dec << "; 32-byte SHA-256 "
        << setup_helper_boundary.raw_prefix_sha256
        << " (not treated as an executable helper)\n";
    for (const auto& callsite : staging_callsites) {
        std::cout << "          staging caller 0x" << std::hex << callsite.entry_address
            << ": source 0x" << callsite.source_address << ", JSR setup 0x"
            << callsite.setup_helper_address << ", JSR helper 0x" << callsite.helper_address
            << "; static post-JSR bytes at 0x" << callsite.post_helper_return_address
            << " load 0x" << callsite.post_helper_source_address << " and 0x"
            << callsite.post_helper_magnitude_address << std::dec
            << " (byte boundary only; no helper-return execution)\n";
    }
    std::cout << "          first post-helper static chain: caller 0x" << std::hex
        << first_post_helper_chain.staging_entry_address << ", bytes at 0x"
        << first_post_helper_chain.static_start_address << " (disk 0x"
        << first_post_helper_chain.raw_disk_offset << ", " << std::dec
        << first_post_helper_chain.byte_count << " bytes, SHA-256 "
        << first_post_helper_chain.sha256 << "); static JSR 0x" << std::hex
        << first_post_helper_chain.next_setup_target << " at 0x"
        << first_post_helper_chain.next_setup_call_address << ", then 0x"
        << first_post_helper_chain.following_target << " at 0x"
        << first_post_helper_chain.following_call_address << std::dec
        << " (byte chain only; no helper-return or call execution)\n";
    std::cout << "          second post-helper static chain: caller 0x" << std::hex
        << second_post_helper_chain.staging_entry_address << ", bytes at 0x"
        << second_post_helper_chain.static_start_address << " (disk 0x"
        << second_post_helper_chain.raw_disk_offset << ", " << std::dec
        << second_post_helper_chain.byte_count << " bytes, SHA-256 "
        << second_post_helper_chain.sha256 << "); static JSR 0x" << std::hex
        << second_post_helper_chain.static_call_target << " at 0x"
        << second_post_helper_chain.static_call_address << std::dec
        << " (byte chain only; no helper-return or call execution)\n";
    std::cout << "          staging reachability boundary: raw disk 0x" << std::hex
        << staging_reachability.scanned_raw_disk_offset << " +0x"
        << staging_reachability.scanned_byte_count << "; direct absolute JSR/JMP counts to 0x"
        << staging_reachability.staging_entry_addresses[0] << " = " << std::dec
        << staging_reachability.absolute_jsr_counts[0] << '/' << staging_reachability.absolute_jmp_counts[0]
        << ", 0x" << std::hex << staging_reachability.staging_entry_addresses[1] << " = " << std::dec
        << staging_reachability.absolute_jsr_counts[1] << '/' << staging_reachability.absolute_jmp_counts[1]
        << "; PC-relative BSR.W counts = " << staging_reachability.pc_relative_bsr_word_counts[0]
        << '/' << staging_reachability.pc_relative_bsr_word_counts[1]
        << "; local MOVEA/JSR(An), JMP(An) counts = "
        << staging_reachability.local_immediate_register_jsr_counts[0] << '/'
        << staging_reachability.local_immediate_register_jsr_counts[1] << ", "
        << staging_reachability.local_immediate_register_jmp_counts[0] << '/'
        << staging_reachability.local_immediate_register_jmp_counts[1]
        << " (only these static encodings; indirect/transformed paths unproven)\n";
    std::cout << "          separate post-call boundary: entry 0x" << std::hex
        << separate_post_call.entry_address << " (disk 0x" << separate_post_call.raw_disk_offset
        << ", SHA-256 " << separate_post_call.sha256 << "); D0 #0x"
        << separate_post_call.d0_immediate << ", A5 source 0x"
        << separate_post_call.a5_source_address << ", store D0 0x"
        << separate_post_call.stored_d0_address << ", JSR 0x"
        << separate_post_call.following_call_target << " at 0x"
        << separate_post_call.following_call_address << " (target disk 0x"
        << separate_post_call.following_target_raw_disk_offset << ", SHA-256 "
        << separate_post_call.following_target_prefix_sha256 << std::dec
        << "; static only, no prior-return/RAM/call execution)\n";
    std::cout << "          separate post-call tail: six JSRs at 0x" << std::hex
        << separate_post_call_tail.entry_address << " (disk 0x"
        << separate_post_call_tail.raw_disk_offset << ", " << std::dec
        << separate_post_call_tail.byte_count << " bytes, SHA-256 "
        << separate_post_call_tail.sha256 << ")";
    for (std::size_t index = 0; index < separate_post_call_tail.call_targets.size(); ++index) {
        std::cout << "; 0x" << std::hex << separate_post_call_tail.call_targets[index]
            << " at 0x" << separate_post_call_tail.call_addresses[index]
            << " -> disk 0x" << separate_post_call_tail.target_raw_disk_offsets[index]
            << " (SHA-256 " << separate_post_call_tail.target_prefix_sha256[index] << ')';
    }
    std::cout << std::dec << " (static only, no prior-return/call execution)\n";
    std::cout << "          separate post-call tail branch: entry 0x" << std::hex
        << separate_post_call_tail_branch.entry_address << " (disk 0x"
        << separate_post_call_tail_branch.raw_disk_offset << ", SHA-256 "
        << separate_post_call_tail_branch.sha256 << "); CMP.B #0x"
        << static_cast<unsigned>(separate_post_call_tail_branch.compare_immediate)
        << ", 0x" << separate_post_call_tail_branch.compared_byte_address
        << "; BCS.W at 0x" << separate_post_call_tail_branch.conditional_branch_address
        << " -> 0x" << separate_post_call_tail_branch.conditional_branch_target
        << " (target disk 0x" << separate_post_call_tail_branch.target_raw_disk_offset
        << ", SHA-256 " << separate_post_call_tail_branch.target_prefix_sha256 << std::dec
        << "; static only, no prior-return/RAM/call execution)\n";
    std::cout << "          separate comparison boundary: BNE.W at 0x" << std::hex
        << separate_comparison.preceding_branch_address << " -> 0x"
        << separate_comparison.preceding_branch_target << "; entry 0x"
        << separate_comparison.entry_address << " (disk 0x"
        << separate_comparison.raw_disk_offset << ", SHA-256 "
        << separate_comparison.sha256 << ')';
    for (std::size_t index = 0;
         index < separate_comparison.conditional_branch_addresses.size(); ++index) {
        std::cout << "; branch 0x" << separate_comparison.conditional_branch_addresses[index]
            << " -> 0x" << separate_comparison.conditional_branch_targets[index];
    }
    std::cout << "; continuation disk 0x"
        << separate_comparison.continuation_raw_disk_offset << " (SHA-256 "
        << separate_comparison.continuation_prefix_sha256 << std::dec
        << "; static only, no register/flag/path execution)\n";
    std::cout << "          separate byte gate: entry 0x" << std::hex
        << separate_byte_gate.entry_address << " (disk 0x" << separate_byte_gate.raw_disk_offset
        << ", SHA-256 " << separate_byte_gate.sha256 << "); CMP.B D0,0x"
        << separate_byte_gate.compared_byte_address << "; BEQ.W at 0x"
        << separate_byte_gate.conditional_branch_address << " -> 0x"
        << separate_byte_gate.conditional_branch_target << " (target disk 0x"
        << separate_byte_gate.target_raw_disk_offset << ", SHA-256 "
        << separate_byte_gate.target_prefix_sha256 << "; fallthrough disk 0x"
        << separate_byte_gate.fallthrough_raw_disk_offset << ", SHA-256 "
        << separate_byte_gate.fallthrough_prefix_sha256 << std::dec
        << "; static only, no register/branch execution)\n";
    std::cout << "          taken byte-gate target: entry 0x" << std::hex
        << separate_byte_gate_target.entry_address << " (disk 0x"
        << separate_byte_gate_target.raw_disk_offset << ", SHA-256 "
        << separate_byte_gate_target.sha256 << ")";
    for (std::size_t index = 0;
         index < separate_byte_gate_target.conditional_branch_addresses.size(); ++index) {
        std::cout << "; BCC.W 0x"
            << separate_byte_gate_target.conditional_branch_addresses[index] << " -> 0x"
            << separate_byte_gate_target.conditional_branch_targets[index];
    }
    std::cout << "; convergence 0x" << separate_byte_gate_target.convergence_address
        << " (disk 0x" << separate_byte_gate_target.convergence_raw_disk_offset
        << ", SHA-256 " << separate_byte_gate_target.convergence_prefix_sha256 << std::dec
        << "; static only, no flag/cell/path execution)\n";
    std::cout << "          byte-gate convergence: entry 0x" << std::hex
        << separate_byte_gate_convergence.entry_address << " (disk 0x"
        << separate_byte_gate_convergence.raw_disk_offset << ", SHA-256 "
        << separate_byte_gate_convergence.sha256 << "); BEQ.W at 0x"
        << separate_byte_gate_convergence.conditional_branch_address << " -> 0x"
        << separate_byte_gate_convergence.conditional_branch_target << " (target disk 0x"
        << separate_byte_gate_convergence.target_raw_disk_offset << ", SHA-256 "
        << separate_byte_gate_convergence.target_prefix_sha256 << "; fallthrough disk 0x"
        << separate_byte_gate_convergence.fallthrough_raw_disk_offset << ", SHA-256 "
        << separate_byte_gate_convergence.fallthrough_prefix_sha256 << std::dec
        << "; static only, no register/branch execution)\n";
    std::cout << "          taken convergence branch: entry 0x" << std::hex
        << separate_byte_gate_taken_branch.entry_address << " (disk 0x"
        << separate_byte_gate_taken_branch.raw_disk_offset << ", SHA-256 "
        << separate_byte_gate_taken_branch.sha256 << ')';
    for (std::size_t index = 0;
         index < separate_byte_gate_taken_branch.conditional_branch_addresses.size(); ++index) {
        std::cout << "; BCC.W 0x"
            << separate_byte_gate_taken_branch.conditional_branch_addresses[index] << " -> 0x"
            << separate_byte_gate_taken_branch.conditional_branch_targets[index];
    }
    std::cout << "; external JSR 0x" << separate_byte_gate_taken_branch.external_call_address
        << " -> 0x" << separate_byte_gate_taken_branch.external_call_target << " (disk 0x"
        << separate_byte_gate_taken_branch.external_prefix_raw_disk_offset << ", SHA-256 "
        << separate_byte_gate_taken_branch.external_prefix_sha256 << std::dec
        << "; static boundary only, no call execution)\n";
    std::cout << "          byte-gate fallthrough: entry 0x" << std::hex
        << separate_byte_gate_fallthrough.entry_address << " (disk 0x"
        << separate_byte_gate_fallthrough.raw_disk_offset << ", SHA-256 "
        << separate_byte_gate_fallthrough.sha256 << "); BEQ.W at 0x"
        << separate_byte_gate_fallthrough.conditional_branch_address << " -> 0x"
        << separate_byte_gate_fallthrough.conditional_branch_target
        << "; alternate prefix 0x" << separate_byte_gate_fallthrough.other_path_entry_address
        << " (SHA-256 " << separate_byte_gate_fallthrough.other_path_sha256 << "); BRA.W at 0x"
        << separate_byte_gate_fallthrough.other_path_branch_address << " -> 0x"
        << separate_byte_gate_fallthrough.other_path_branch_target << std::dec
        << " (static only, no register/branch execution)\n";
    std::cout << "          independent resident gate: entry 0x" << std::hex
        << independent_entry.entry_address << "; negative-D3 branch 0x"
        << independent_entry.negative_d3_branch_address << " -> 0x"
        << independent_entry.negative_d3_target << "; fixed-byte test 0x"
        << independent_entry.flag_test_address << " at 0x" << independent_entry.flag_address
        << ", zero branch 0x" << independent_entry.flag_zero_branch_address << " -> 0x"
        << independent_entry.flag_zero_target << std::dec
        << " (static only, no flag/branch execution)\n"
        << "          negative-D3 continuation: entry 0x" << std::hex
        << negative_d3.entry_address << "; external JMP 0x"
        << negative_d3.external_jump_address << " -> 0x" << negative_d3.external_jump_target
        << "; terminal tail 0x" << negative_d3_terminal.entry_address
        << " has immediates 0x" << negative_d3_terminal.first_add_immediate << ", 0x"
        << negative_d3_terminal.second_add_immediate << " and RTS 0x"
        << negative_d3_terminal.return_address << std::dec
        << " (validated raw bytes only; no predicates, target, or effects executed)\n"
        << "          post-negative-D3 terminal: entry 0x" << std::hex
        << post_negative_d3.entry_address << "; byte stores 0x"
        << post_negative_d3.absolute_byte_store_addresses[0] << "/0x"
        << post_negative_d3.absolute_byte_store_addresses[1] << "; BNE 0x"
        << post_negative_d3.nonzero_branch_address << " -> 0x"
        << post_negative_d3.nonzero_branch_target << ", zero RTS 0x"
        << post_negative_d3.zero_return_address << "; BPL 0x"
        << post_negative_d3.nonnegative_branch_address << " -> boundary 0x"
        << post_negative_d3.nonnegative_branch_target << ", negative RTS 0x"
        << post_negative_d3.negative_return_address << "; SHA-256 "
        << post_negative_d3.raw_sha256 << std::dec
        << " (static only; no registers, stores, predicates, or continuation executed)\n"
        << "          post-negative-D3 continuation: entry 0x" << std::hex
        << post_negative_d3_continuation.entry_address << " (disk 0x"
        << post_negative_d3_continuation.raw_disk_offset << ", " << std::dec
        << post_negative_d3_continuation.byte_count << " bytes); BCC 0x" << std::hex
        << post_negative_d3_continuation.compare_branch_address << " -> 0x"
        << post_negative_d3_continuation.compare_branch_target << ", BCS 0x"
        << post_negative_d3_continuation.low_range_branch_address << " -> 0x"
        << post_negative_d3_continuation.low_range_branch_target << ", BMI 0x"
        << post_negative_d3_continuation.negative_range_branch_address << " -> 0x"
        << post_negative_d3_continuation.negative_range_branch_target << "; terminal JMP 0x"
        << post_negative_d3_continuation.terminal_jump_address << " -> 0x"
        << post_negative_d3_continuation.terminal_jump_target << "; SHA-256 "
        << post_negative_d3_continuation.raw_sha256 << std::dec
        << " (static only; no branch, restore, or jump execution)\n";
}

void report_millennium_atari_root_inventory(const eon::MillenniumAtariRootInventory& inventory) {
    // Keep the complete raw filesystem listing in a small separate function.
    // Besides making the provenance report easier to audit, this avoids
    // stressing compiler optimisation of the already deliberately detailed
    // Atari bootstrap report below.
    if (inventory.files.empty()) throw std::runtime_error("Verified Millennium Atari ST root is empty");
    std::cout << "          Equinox FAT12 root: " << inventory.files.size()
        << " original regular files; first " << inventory.files.front().name
        << " SHA-256 " << inventory.files.front().sha256 << ", final "
        << inventory.files.back().name << " SHA-256 "
        << inventory.files.back().sha256
        << " (complete read-only inventory; no file access or format semantics inferred)\n";
    for (const auto& file : inventory.files) {
        std::cout << "            " << file.name << " cluster " << file.first_cluster
            << ", " << file.size << " bytes, SHA-256 " << file.sha256 << '\n';
    }
}

void report_millennium_atari_st(const eon::ReleaseArchive& release) {
    constexpr auto equinox_disk_sha256 =
        "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7";
    constexpr auto disk1_stx_sha256 =
        "081d8bc102b8c7669c5cb21abace9b08532bc0b34164f11465d0c87b63a422fd";
    const auto image = eon::extract_verified_release_asset(release, equinox_disk_sha256);
    if (!image) return;
    const eon::Fat12Disk disk{std::span<const std::uint8_t>(*image)};
    const auto* executable = disk.find("MILENIUM.TOS");
    if (!executable) throw std::runtime_error("Verified Millennium Atari ST disk has no MILENIUM.TOS");
    const auto executable_bytes = disk.read(*executable);
    const eon::MillenniumAtariBootstrapSession live_bootstrap(disk, executable_bytes);
    const auto prg = eon::parse_atari_st_prg(executable_bytes);
    const auto bootstrap = eon::parse_millennium_atari_bootstrap(executable_bytes, prg);
    const auto bss_entry = eon::parse_millennium_atari_bss_entry(executable_bytes, prg, bootstrap);
    const auto bss_source = eon::materialize_millennium_atari_bss_source(
        executable_bytes, prg, bootstrap, bss_entry);
    const auto target = eon::materialize_millennium_atari_target(bss_source, bss_entry);
    const auto trap_entry = eon::parse_millennium_atari_trap_entry(bss_source, target);
    const auto fopen_fallthrough = eon::parse_millennium_atari_fopen_fallthrough(target, trap_entry);
    const auto fread_frame_prefix = eon::execute_millennium_atari_fread_frame_prefix(
        target, fopen_fallthrough);
    const auto fread_config_transfer = eon::parse_millennium_atari_fread_config_transfer_boundary(
        target, fopen_fallthrough);
    std::cout << "          bounded launcher bootstrap: executed " << std::dec
        << live_bootstrap.execution().first_copy_longwords << " original longword copies and "
        << live_bootstrap.execution().second_copy_words << " original word copies to target 0x"
        << std::hex << live_bootstrap.target().target_address << ", stops before TRAP #1 at 0x"
        << live_bootstrap.execution().stop_before_trap_address << " after " << std::dec
        << live_bootstrap.execution().target_prefix_bytes_executed
        << " original Fopen-prefix bytes / "
        << -live_bootstrap.execution().relative_stack_pointer_delta
        << " relative stack bytes; Fopen boundary "
        << live_bootstrap.fopen_boundary().fopen_filename << std::dec
        << " (no GEMDOS call)\n";
    const auto physical_disk = eon::extract_verified_release_asset(release, disk1_stx_sha256);
    if (physical_disk) {
        const eon::AtariStStxPhysicalDisk stx(*physical_disk);
        const eon::AtariStStxFat12Root stx_root(stx);
        const auto boot = stx.sector(0, 0, 1);
        const auto loader = stx.sector(1, 0, 9);
        const auto boot_location = std::find_if(stx.sectors().begin(), stx.sectors().end(),
            [](const eon::AtariStStxSector& sector) {
                return sector.track == 0 && sector.side == 0 && sector.id == 1;
            });
        const auto loader_location = std::find_if(stx.sectors().begin(), stx.sectors().end(),
            [](const eon::AtariStStxSector& sector) {
                return sector.track == 1 && sector.side == 0 && sector.id == 9;
            });
        if (boot_location == stx.sectors().end() || loader_location == stx.sectors().end()) {
            throw std::runtime_error("Verified Millennium Atari STX has no required physical sector");
        }
        std::cout << "          physical Disk 1 STX: SHA-256 " << disk1_stx_sha256
            << "; " << stx.track_count() << " track records, " << stx.sectors().size()
            << " identified sectors; T0/H0/S1 +0x" << std::hex
            << boot_location->payload_offset << ", " << std::dec << boot.size()
            << " bytes, SHA-256 " << eon::to_hex(eon::sha256(boot))
            << "; T1/H0/S9 +0x" << std::hex << loader_location->payload_offset
            << ", " << std::dec << loader.size() << " bytes, SHA-256 "
            << eon::to_hex(eon::sha256(loader)) << "; literal +0xbe MILL22B.inf"
            << "; BPB-backed root LBA " << std::dec << stx_root.root_start_lba() << ".."
            << (stx_root.root_start_lba() + stx_root.root_sector_count() - 1U) << " has "
            << stx_root.entries().size() << " direct-sector entries"
            << "; mirrored FAT LBA " << stx_root.fat_start_lba() << ".."
            << (stx_root.fat_start_lba() + stx_root.sectors_per_fat() - 1U)
            << (stx_root.fat_mirrors_match() ? " matches" : " differs from")
            << " second copy; SHA-256 " << stx_root.fat_primary_sha256() << "/"
            << stx_root.fat_secondary_sha256()
            << (stx_root.fat_mirrors_match()
                ? " (no flattened image, file extraction, boot semantics, or executable handoff)\n"
                : " (root records only; no FAT chain, flattened image, file extraction, boot semantics, or executable handoff)\n");
    } else {
        // The standalone Equinox outer release is an independently recognised
        // launch source, but does not contain this larger collection's
        // physical dump.  State that scope rather than suggesting its STX
        // evidence was searched elsewhere or silently borrowed.
        std::cout << "          physical Disk 1 STX: absent from this verified outer release "
            "(no physical-media fallback or substitution)\n";
    }
    const auto equinox_config = eon::probe_millennium_atari_config(disk);
    if (!equinox_config.present) throw std::runtime_error("Verified Millennium Atari ST disk has no MILL22A.inf");
    const auto& root_inventory = live_bootstrap.root_inventory();
    const auto auxiliary_resource = eon::probe_millennium_atari_auxiliary_resource_name(disk);
    const auto config_entry = eon::parse_millennium_atari_config_entry(
        disk.read(*disk.find(equinox_config.requested_filename)));
    const auto config_load_address_boundary = eon::parse_millennium_atari_fread_config_load_address_boundary(
        fread_config_transfer, disk.read(*disk.find(equinox_config.requested_filename)), config_entry);
    const auto fread_mapped_config_prelude = eon::parse_millennium_atari_fread_mapped_config_prelude(
        fread_config_transfer, disk.read(*disk.find(equinox_config.requested_filename)), config_entry);
    const auto config_trap_argument_strings = eon::parse_millennium_atari_config_trap_argument_strings(
        disk.read(*disk.find(equinox_config.requested_filename)), config_entry);
    const auto config_first_jsr = eon::parse_millennium_atari_config_first_jsr(
        disk.read(*disk.find(equinox_config.requested_filename)), config_entry);
    const auto config_second_jsr = eon::parse_millennium_atari_config_second_jsr(
        disk.read(*disk.find(equinox_config.requested_filename)), config_entry);
    const auto config_join_jsr = eon::parse_millennium_atari_config_join_jsr(
        disk.read(*disk.find(equinox_config.requested_filename)), config_second_jsr);
    const auto config_forwarded_jsr = eon::parse_millennium_atari_config_forwarded_jsr(
        disk.read(*disk.find(equinox_config.requested_filename)), config_entry);
    const auto config_third_jsr = eon::parse_millennium_atari_config_third_jsr(
        disk.read(*disk.find(equinox_config.requested_filename)), config_entry);
    const auto config_fourth_jsr = eon::parse_millennium_atari_config_fourth_jsr(
        disk.read(*disk.find(equinox_config.requested_filename)), config_entry);
    const auto config_fourth_prelude = eon::parse_millennium_atari_config_fourth_prelude(
        disk.read(*disk.find(equinox_config.requested_filename)), config_fourth_jsr);
    const auto config_fourth_loop = eon::parse_millennium_atari_config_fourth_loop(
        disk.read(*disk.find(equinox_config.requested_filename)), config_fourth_jsr);
    const auto config_fourth_post_loop = eon::parse_millennium_atari_config_fourth_post_loop(
        disk.read(*disk.find(equinox_config.requested_filename)), config_fourth_loop);
    const auto config_fourth_outer_setup = eon::parse_millennium_atari_config_fourth_outer_setup(
        disk.read(*disk.find(equinox_config.requested_filename)), config_fourth_post_loop);
    const auto config_fourth_post_outer = eon::parse_millennium_atari_config_fourth_post_outer_boundary(
        disk.read(*disk.find(equinox_config.requested_filename)), config_fourth_post_loop);
    const auto config_fourth_post_outer_tail = eon::parse_millennium_atari_config_fourth_post_outer_tail(
        disk.read(*disk.find(equinox_config.requested_filename)), config_fourth_post_outer);
    const auto config_fourth_post_outer_recurrence = eon::parse_millennium_atari_config_fourth_post_outer_recurrence(
        disk.read(*disk.find(equinox_config.requested_filename)), config_fourth_post_outer_tail,
        config_fourth_loop);
    const auto config_jsr_inventory = eon::inventory_millennium_atari_config_absolute_jsrs(
        disk.read(*disk.find(equinox_config.requested_filename)));
    const auto config_residual_jsr_body = eon::parse_millennium_atari_config_residual_jsr_body(
        disk.read(*disk.find(equinox_config.requested_filename)), config_jsr_inventory);
    std::cout << "          auxiliary resource-name evidence: "
        << auxiliary_resource.container_filename << " cluster " << auxiliary_resource.first_cluster
        << ", +0x" << std::hex << auxiliary_resource.literal_file_offset << " = "
        << auxiliary_resource.literal_filename << "; SHA-256 " << auxiliary_resource.sha256 << std::dec
        << " (literal only; no open, decoder, or presentation claim)\n";
    std::cout << "          MILENIUM.TOS: text " << prg.text_bytes << ", data "
        << prg.data_bytes << ", BSS " << prg.bss_bytes << ", "
        << prg.relocation_count << " relocations (0x" << std::hex
        << prg.first_relocation_offset << "..0x" << prg.last_relocation_offset
        << std::dec << ")\n"
        << "          relocation values: [0x" << std::hex
        << prg.relocations.front().offset << "]=0x" << prg.relocations.front().original_value
        << ", [0x" << prg.relocations.back().offset << "]=0x"
        << prg.relocations.back().original_value << std::dec
        << " (unrelocated disk words)\n"
        << "          entry: BRA 0x" << std::hex << bootstrap.branch_target_offset
        << "; copies 0x" << bootstrap.stage_bytes << " bytes from 0x"
        << bootstrap.stage_source_offset << "..0x" << bootstrap.stage_last_longword_offset
        << " to BSS 0x" << bootstrap.stage_destination_offset
        << " and JMPs there" << std::dec << " (original bootstrap; not executed)\n"
        << "          BSS entry: copies " << bss_entry.copied_words << " words from 0x"
        << std::hex << bss_entry.copy_source_address << " to 0x"
        << bss_entry.copy_destination_address << "; JMP 0x" << bss_entry.jump_address
        << std::dec << " (original bootstrap; not executed)\n"
        << "          BSS source: PRG load base 0x" << std::hex << bss_source.load_base
        << "; " << std::dec << bss_source.original_data_bytes << " original DATA bytes at 0x"
        << std::hex << bss_source.source_data_offset << " plus " << std::dec
        << bss_source.bss_zero_bytes << " loader-zeroed BSS bytes (in-memory only)\n"
        << "          target 0x" << std::hex << target.target_address << ": opcode 0x"
        << target.first_opcode << " immediate word 0x" << target.first_immediate_word
        << ", immediate longword 0x" << target.first_immediate_longword << std::dec
        << " (validated original target; not executed)\n"
        << "          GEMDOS boundary: Fopen 0x" << std::hex << trap_entry.fopen_filename_address
        << " (\"" << trap_entry.fopen_filename << "\", mode " << std::dec
        << trap_entry.fopen_access_mode << ", function 0x" << std::hex
        << trap_entry.fopen_function << ") via TRAP #1 +0x" << trap_entry.fopen_trap_offset
        << "; next Fclose selector 0x" << trap_entry.following_fclose_function
        << " is prepared at +0x" << trap_entry.following_fclose_selector_offset << std::dec
        << "; Fopen negative D0 loops at +0x" << std::hex
        << trap_entry.fopen_result_negative_branch_target_offset << std::dec
        << " (reported only; no GEMDOS emulation)\n"
        << "          Fopen fall-through: target +0x" << std::hex << fopen_fallthrough.entry_offset
        << "; selector 0x" << fopen_fallthrough.fread_function << " reads D0 handle, 0x"
        << fopen_fallthrough.fread_byte_count << " bytes to 0x"
        << fopen_fallthrough.fread_buffer_address << " via TRAP #1 +0x"
        << fopen_fallthrough.fread_trap_offset << "; stack cleanup 0x"
        << fopen_fallthrough.stack_cleanup_bytes << "; SHA-256 " << fopen_fallthrough.sha256
        << std::dec << " (static call boundary only; no Fopen/Fread result or data is modeled)\n"
        << "          Fread frame prefix: target +0x" << std::hex
        << fread_frame_prefix.entry_offset << "; " << std::dec
        << fread_frame_prefix.byte_count << " original bytes, "
        << fread_frame_prefix.relative_stack_pointer_delta
        << " relative stack delta; D0 handle slot +"
        << fread_frame_prefix.opaque_handle_frame_offset << "..+"
        << (fread_frame_prefix.opaque_handle_frame_offset
            + fread_frame_prefix.opaque_handle_frame_bytes - 1U)
        << " remains opaque; stops before TRAP #1 +0x" << std::hex
        << fread_frame_prefix.stop_before_trap_offset << std::dec
        << " (no Fopen result or GEMDOS call)\n"
        << "          Fread-config transfer: target +0x" << std::hex
        << fread_config_transfer.entry_offset << "; TRAP #1, stack cleanup 0x"
        << fread_config_transfer.stack_cleanup_bytes << ", JSR 0x"
        << fread_config_transfer.config_buffer_address << "; SHA-256 "
        << fread_config_transfer.sha256 << std::dec
        << " (static edge only; no GEMDOS result, buffer fill, or JSR execution)\n"
        << "          requested config " << equinox_config.requested_filename << ": "
        << (equinox_config.present ? "present" : "absent") << " in Equinox FAT12 root ("
        << equinox_config.root_entry_count << " live entries)";
    if (equinox_config.present) {
        std::cout << "; cluster " << equinox_config.first_cluster << ", "
            << equinox_config.size << " bytes, SHA-256 " << equinox_config.sha256
            << ", leading words 0x" << std::hex << equinox_config.first_word << " 0x"
            << equinox_config.first_longword_operand << std::dec;
    }
    std::cout << " (metadata only; never generated or written)\n";
    report_millennium_atari_root_inventory(root_inventory);
    const auto* save_i = disk.find("2200SAVE.I");
    const auto* save_ii = disk.find("2200SAVE.II");
    const auto* save_iii = disk.find("2200SAVE.III");
    const auto* save_iv = disk.find("2200SAVE.IV");
    if (!save_i || !save_ii || !save_iii || !save_iv) {
        throw std::runtime_error("Verified Millennium Atari ST disk has incomplete save inventory");
    }
    const auto authenticated_save_i = eon::authenticate_millennium_save(
        eon::MillenniumSavePlatform::atari_st, "2200SAVE.I", disk.read(*save_i));
    const auto authenticated_save_ii = eon::authenticate_millennium_save(
        eon::MillenniumSavePlatform::atari_st, "2200SAVE.II", disk.read(*save_ii));
    const auto authenticated_save_iii = eon::authenticate_millennium_save(
        eon::MillenniumSavePlatform::atari_st, "2200SAVE.III", disk.read(*save_iii));
    const auto authenticated_save_iv = eon::authenticate_millennium_save(
        eon::MillenniumSavePlatform::atari_st, "2200SAVE.IV", disk.read(*save_iv));
    const auto save_i_ii = eon::compare_millennium_saves(authenticated_save_i,
        authenticated_save_ii);
    const auto save_iii_iv = eon::compare_millennium_saves(authenticated_save_iii,
        authenticated_save_iv);
    std::cout << "          Atari save byte comparison: I/II " << save_i_ii.equal_positions
        << "/" << save_i_ii.shared_bytes << " equal positions (prefix/suffix "
        << save_i_ii.common_prefix_bytes << "/" << save_i_ii.common_suffix_bytes
        << "); III/IV " << save_iii_iv.equal_positions << "/" << save_iii_iv.shared_bytes
        << " (prefix/suffix " << save_iii_iv.common_prefix_bytes << "/"
        << save_iii_iv.common_suffix_bytes
        << "; hash-gated byte facts only, no save-format or compatibility claim)\n";
    std::cout << "          MILL22A.inf candidate entry: JMP 0x" << std::hex << config_entry.entry_address
        << " resolves from independent static base 0x" << config_entry.proven_load_base
        << " to file +0x" << config_entry.entry_file_offset << "; TRAP #14 selectors 0x"
        << config_entry.initial_trap_selector << " (longword 0x"
        << config_entry.initial_trap_longword_argument << ") and 0x"
        << config_entry.second_trap_selector << " (longword 0x"
        << config_entry.second_trap_longword_argument << "); JSRs";
    for (const auto address : config_entry.jsr_targets) std::cout << " 0x" << address;
    std::cout << "; PEA 0x" << config_entry.final_pea_address << ", TRAP #14 selector 0x"
        << config_entry.final_trap_selector << ", RTS +0x" << config_entry.return_offset
        << std::dec << " (validated only; no TOS/XBIOS calls or config execution)\n";
    std::cout << "          Fread/config address boundary: JSR destination 0x" << std::hex
        << config_load_address_boundary.fread_destination_address << "; file JMP 0x"
        << config_load_address_boundary.payload_initial_jump_target_address << " would be +0x"
        << config_load_address_boundary.payload_initial_jump_target_file_offset_from_destination
        << " from that destination, while independent entry evidence is +0x"
        << config_load_address_boundary.independent_entry_file_offset << " (delta +0x"
        << config_load_address_boundary.independent_entry_offset_delta << "; SHA-256 "
        << config_load_address_boundary.payload_initial_jump_sha256 << std::dec
        << "; unresolved native load-address boundary, no mapping or execution inferred)\n";
    std::cout << "          conditional Fread-mapped config prelude: 0x" << std::hex
        << fread_mapped_config_prelude.mapped_entry_address << " = file +0x"
        << fread_mapped_config_prelude.mapped_entry_file_offset << " (" << std::dec
        << fread_mapped_config_prelude.byte_count << " bytes; SHA-256 "
        << fread_mapped_config_prelude.sha256 << "), branch -> 0x" << std::hex
        << fread_mapped_config_prelude.conditional_branch_target_address << ", converged JSR 0x"
        << fread_mapped_config_prelude.converged_jsr_address << " -> 0x"
        << fread_mapped_config_prelude.converged_jsr_target_address << ", fall-through 0x"
        << fread_mapped_config_prelude.continuation_address << std::dec
        << " (conditional bytes only; no GEMDOS return, branch, JSR, or platform state supplied)\n";
    std::cout << "          second literal TRAP argument: 0x" << std::hex
        << config_trap_argument_strings.argument_address << " (file +0x"
        << config_trap_argument_strings.file_offset << ") = "
        << config_trap_argument_strings.strings[0] << ", "
        << config_trap_argument_strings.strings[1] << "; SHA-256 "
        << config_trap_argument_strings.sha256 << std::dec
        << " (bytes only; no service or file access)\n";
    std::cout << "          MILL22A.inf first JSR target: 0x" << std::hex
        << config_first_jsr.target_address << " is file +0x" << config_first_jsr.target_file_offset
        << "; leading opcode 0x" << config_first_jsr.leading_opcode << ", then MOVEM.L opcode 0x"
        << config_first_jsr.movem_opcode << " mask 0x" << config_first_jsr.movem_register_mask
        << ", RTS 0x" << config_first_jsr.return_opcode << std::dec
        << " (validated only; no JSR execution or caller-state inference)\n";
    std::cout << "          MILL22A.inf second JSR target: 0x" << std::hex
        << config_second_jsr.target_address << " is file +0x" << config_second_jsr.target_file_offset
        << "; opcode 0x" << config_second_jsr.initial_opcode << " immediate 0x"
        << config_second_jsr.immediate_bit_number << ", branch 0x"
        << config_second_jsr.conditional_branch_opcode << " -> 0x"
        << config_second_jsr.conditional_branch_target_address << "; joined JSR at 0x"
        << config_second_jsr.join_jsr_address << " -> 0x" << config_second_jsr.join_jsr_target
        << ", following JSR -> 0x" << config_second_jsr.following_jsr_target << std::dec
        << " (validated only; no branch evaluation or calls)\n";
    std::cout << "          MILL22A.inf joined JSR target: 0x" << std::hex
        << config_join_jsr.target_address << " is file +0x" << config_join_jsr.target_file_offset
        << "; opcodes 0x" << config_join_jsr.initial_opcode << " 0x"
        << config_join_jsr.d0_word_store_opcode << " -> 0x"
        << config_join_jsr.d0_word_store_address << ", opaque Line-A 0x"
        << config_join_jsr.line_a_opcode << ", stores 0x"
        << config_join_jsr.first_longword_store_address << " and 0x"
        << config_join_jsr.second_longword_store_address << ", RTS 0x"
        << config_join_jsr.return_opcode << std::dec
        << " (validated only; no Line-A/OS emulation or RAM-state synthesis)\n";
    std::cout << "          MILL22A.inf forwarded JSR target: 0x" << std::hex
        << config_forwarded_jsr.entry_address << " is file +0x"
        << config_forwarded_jsr.entry_file_offset << "; JMP 0x"
        << config_forwarded_jsr.forwarded_address << " (file +0x"
        << config_forwarded_jsr.forwarded_file_offset << "), opcodes 0x"
        << config_forwarded_jsr.initial_opcode << ", TRAP #14 selector 0x"
        << config_forwarded_jsr.trap_selector << " via 0x" << config_forwarded_jsr.trap_opcode
        << ", cleanup 0x" << config_forwarded_jsr.stack_cleanup_opcode << ", RTS 0x"
        << config_forwarded_jsr.return_opcode << std::dec
        << " (validated only; no XBIOS/firmware calls or state synthesis)\n";
    std::cout << "          MILL22A.inf 0x2b2be target: file +0x" << std::hex
        << config_third_jsr.target_file_offset << "; opcodes 0x"
        << config_third_jsr.initial_opcode << " 0x" << config_third_jsr.gate_opcode
        << " immediate 0x" << config_third_jsr.gate_immediate << ", branch 0x"
        << config_third_jsr.branch_opcode << " +0x" << config_third_jsr.branch_displacement
        << " -> 0x" << config_third_jsr.branch_target_address << " (opcode 0x"
        << config_third_jsr.branch_target_opcode << " immediate 0x"
        << config_third_jsr.branch_target_immediate << ", branch 0x"
        << config_third_jsr.branch_target_branch_opcode << ')' << std::dec
        << " (validated only; no D0 branch choice or platform-state inference)\n";
    std::cout << "          MILL22A.inf 0x2b448 setup: file +0x" << std::hex
        << config_fourth_jsr.target_file_offset << "; D7 0x" << config_fourth_jsr.d7_initial_value
        << ", A5 0x" << config_fourth_jsr.a5_initial_address << ", A4 0x"
        << config_fourth_jsr.a4_initial_address << ", D6/D5/D4 0x"
        << config_fourth_jsr.d6_initial_value << "/0x" << config_fourth_jsr.d5_initial_value
        << "/0x" << config_fourth_jsr.d4_initial_value << std::dec
        << " (validated original setup only; no loop, trap, or data interpretation)\n";
    std::cout << "          MILL22A.inf static predecessor: 0x" << std::hex
        << config_fourth_prelude.prelude_address << " file +0x"
        << config_fourth_prelude.prelude_file_offset << " (" << std::dec
        << config_fourth_prelude.byte_count << " bytes; SHA-256 "
        << config_fourth_prelude.sha256 << "); D0/D1 0x" << std::hex
        << config_fourth_prelude.d0_initial_value << "/0x"
        << config_fourth_prelude.d1_initial_value << ", DBF 0x"
        << config_fourth_prelude.first_dbf_opcode << " -> 0x"
        << config_fourth_prelude.first_dbf_target_address << ", second D0 0x"
        << config_fourth_prelude.second_d0_initial_value << ", DBF 0x"
        << config_fourth_prelude.second_dbf_opcode << " -> 0x"
        << config_fourth_prelude.second_dbf_target_address << " -> 0x"
        << config_fourth_prelude.continuation_address << std::dec
        << " (static adjacency only; no callsite, loop execution, or data effect inferred)\n";
    std::cout << "          MILL22A.inf 0x2b448 first loop: 0x" << std::hex
        << config_fourth_loop.body_address << " file +0x" << config_fourth_loop.body_file_offset
        << " (" << std::dec << config_fourth_loop.body_bytes << " bytes), DBF opcode 0x"
        << std::hex << config_fourth_loop.backedge_opcode << " displacement " << std::dec
        << config_fourth_loop.backedge_displacement << " -> 0x" << std::hex
        << config_fourth_loop.backedge_target_address << "; setup D5 0x"
        << config_fourth_loop.setup_d5_value << std::dec
        << " (validated backedge only; no iteration or data-state execution)\n";
    std::cout << "          MILL22A.inf post-inner-loop: 0x" << std::hex
        << config_fourth_post_loop.post_loop_address << " file +0x"
        << config_fourth_post_loop.post_loop_file_offset << "; A5 opcode 0x"
        << config_fourth_post_loop.a5_advance_opcode << ", outer DBF 0x"
        << config_fourth_post_loop.outer_backedge_opcode << " displacement " << std::dec
        << config_fourth_post_loop.outer_backedge_displacement << " -> 0x" << std::hex
        << config_fourth_post_loop.outer_backedge_target_address << " (setup 0x"
        << config_fourth_post_loop.target_setup_opcode << " immediate 0x"
        << config_fourth_post_loop.target_setup_immediate << ')' << std::dec
        << " (validated control flow only; no loops, data, or native calls)\n";
    std::cout << "          MILL22A.inf outer-loop setup: 0x" << std::hex
        << config_fourth_outer_setup.setup_address << " file +0x"
        << config_fourth_outer_setup.setup_file_offset << "; D5 0x"
        << config_fourth_outer_setup.d5_initial_value << ", D4 0x"
        << config_fourth_outer_setup.d4_initial_value << " -> 0x"
        << config_fourth_outer_setup.continuation_address << std::dec
        << " (validated fall-through only; no loop iterations or data access)\n";
    std::cout << "          MILL22A.inf post-outer-loop boundary: 0x" << std::hex
        << config_fourth_post_outer.boundary_address << " file +0x"
        << config_fourth_post_outer.boundary_file_offset << "; pushes 0x"
        << config_fourth_post_outer.longword_argument << " and selector 0x"
        << config_fourth_post_outer.trap_selector << ", TRAP #14 opcode 0x"
        << config_fourth_post_outer.trap_opcode << std::dec
        << " (validated boundary only; no trap/native call or result inference)\n";
    std::cout << "          MILL22A.inf post-trap tail: 0x" << std::hex
        << config_fourth_post_outer_tail.tail_address << " file +0x"
        << config_fourth_post_outer_tail.tail_file_offset << std::dec << "; "
        << config_fourth_post_outer_tail.tail_bytes << " bytes, SHA-256 "
        << config_fourth_post_outer_tail.sha256
        << "; stack cleanup 0x" << std::hex
        << config_fourth_post_outer_tail.initial_stack_cleanup_opcode << ", D0 0x"
        << config_fourth_post_outer_tail.d0_initial_value << " decrement branch " << std::dec
        << static_cast<int>(config_fourth_post_outer_tail.d0_nonzero_branch_displacement)
        << " -> 0x" << std::hex << config_fourth_post_outer_tail.d0_nonzero_branch_target_address
        << "; DBF D7 " << std::dec << config_fourth_post_outer_tail.d7_backedge_displacement
        << " -> 0x" << std::hex << config_fourth_post_outer_tail.d7_backedge_target_address
        << "; selector 0x" << config_fourth_post_outer_tail.selector << ", TRAP #14 0x"
        << config_fourth_post_outer_tail.trap_opcode << ", cleanup/RTS 0x"
        << config_fourth_post_outer_tail.final_stack_cleanup_opcode << "/0x"
        << config_fourth_post_outer_tail.return_opcode << std::dec
        << " (validated only; no post-trap execution or state interpretation)\n";
    std::cout << "          MILL22A.inf post-trap recurrence: 0x" << std::hex
        << config_fourth_post_outer_recurrence.prefix_address << " file +0x"
        << config_fourth_post_outer_recurrence.prefix_file_offset << std::dec << "; "
        << config_fourth_post_outer_recurrence.prefix_bytes << " bytes, SHA-256 "
        << config_fourth_post_outer_recurrence.sha256 << ", falls through to 0x" << std::hex
        << config_fourth_post_outer_recurrence.continuation_address << std::dec
        << " (static linkage only; no trap return or loop execution)\n";
    std::cout << "          MILL22A.inf absolute-JSR encodings: "
        << config_jsr_inventory.encodings.size() << " (file +0x" << std::hex
        << config_jsr_inventory.encodings.front().first << " -> 0x"
        << config_jsr_inventory.encodings.front().second << ", final +0x"
        << config_jsr_inventory.encodings.back().first << " -> 0x"
        << config_jsr_inventory.encodings.back().second << std::dec
        << "; byte inventory only, not reachability claims)\n";
    std::cout << "          MILL22A.inf inventory-only JSR body: +0x" << std::hex
        << config_residual_jsr_body.callsite_file_offset << " -> 0x"
        << config_residual_jsr_body.target_address << " (file +0x"
        << config_residual_jsr_body.target_file_offset << ", " << std::dec
        << config_residual_jsr_body.byte_count << " bytes through RTS 0x" << std::hex
        << config_residual_jsr_body.return_opcode << "; SHA-256 "
        << config_residual_jsr_body.sha256 << std::dec
        << "; static body only, no reachability or execution claim)\n";

    // The outer archive is the supplied-media boundary.  Inspect every ST
    // leaf in memory so absence is not guessed from the one Equinox variant.
    std::size_t supplied_st_images = 0;
    std::size_t readable_fat12_images = 0;
    std::size_t named_config_files = 0;
    std::size_t exact_equinox_config_files = 0;
    std::size_t exact_equinox_program_files = 0;
    constexpr std::string_view equinox_config_sha256 =
        "74d7d630779fd811aedcdbe31b14e54198eb9ffd673df512dd70b6165c4a37b6";
    constexpr std::string_view equinox_program_sha256 =
        "4584ddc459e3bf03e642f3156fbedb74aa33a847db4937beb5635eb492e93686";
    for (const auto& asset : eon::inventory_verified_release(release)) {
        if (asset.kind != eon::AssetKind::atari_st_disk) continue;
        ++supplied_st_images;
        const auto candidate = eon::extract_verified_release_asset(release, asset.sha256);
        if (!candidate) throw std::runtime_error("Verified Atari ST asset disappeared during scan");
        std::optional<eon::Fat12Disk> candidate_disk;
        try {
            candidate_disk.emplace(*candidate);
        } catch (const std::runtime_error&) {
            // The remaining supplied ST images may be raw/protected media.
            // They do not expose a FAT12 filename namespace to this parser.
            continue;
        }
        ++readable_fat12_images;
        const auto candidate_config = eon::probe_millennium_atari_config(*candidate_disk);
        if (candidate_config.present) {
            ++named_config_files;
            if (candidate_config.sha256 == equinox_config_sha256) ++exact_equinox_config_files;
        }
        if (const auto* candidate_program = candidate_disk->find("MILENIUM.TOS")) {
            if (!candidate_program->directory()
                && eon::to_hex(eon::sha256(candidate_disk->read(*candidate_program)))
                    == equinox_program_sha256) {
                ++exact_equinox_program_files;
            }
        }
    }
    std::cout << "          supplied ST config scan: " << supplied_st_images << " images, "
        << readable_fat12_images << " valid FAT12 volumes, " << named_config_files << " files named "
        << equinox_config.requested_filename << "; " << exact_equinox_config_files
        << " exact config hash and " << exact_equinox_program_files << " exact MILENIUM.TOS hash"
        << " (raw/protected images are not reinterpreted as files)\n";
    std::cout << "          Atari ST trace boundary: next evidence must identify GEMDOS Fopen D0/handle "
        << "behaviour, TRAP #14 and Line-A returns, configuration load address, and any codec, "
        << "palette, or planar destination. No alternate ST image or DOS/Amiga asset is substituted.\n";
}

void report_deuteros_atari_st(const eon::ReleaseArchive& release) {
    // The corpus has no pristine ST master: this hash identifies the supplied
    // Replicants Disk 1 whose boot code contains the recovered XBIOS stage.
    constexpr auto replicants_disk1_sha256 =
        "aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee";
    constexpr auto disk2_sha256 =
        "5501ce3fd79c9b37cf695692a8012267db23dacd8a2cc64c0c7b7e4305971193";
    const auto disk1_image = eon::extract_verified_release_asset(release, replicants_disk1_sha256);
    const auto disk2_image = eon::extract_verified_release_asset(release, disk2_sha256);
    if (!disk1_image || !disk2_image) return;
    const eon::DeuterosAtariBootstrapSession live_bootstrap(*disk1_image);
    const eon::DeuterosAtariDisk disk1(*disk1_image);
    const eon::DeuterosAtariDisk disk2(*disk2_image);
    const auto& stage = disk1.boot_profile();
    const auto& continuation = disk2.boot_profile();
    std::cout << "          protected ST media: " << stage.total_sectors << " sectors, "
        << stage.sectors_per_track << " sectors/track, boot checksum 0x" << std::hex
        << stage.boot_checksum << std::dec << "; FAT root intentionally unavailable\n";
    std::size_t atari_leaf_count = 0;
    std::size_t protected_geometry_count = 0;
    std::size_t valid_boot_profile_count = 0;
    std::size_t replicants_boot_count = 0;
    std::size_t killer_boot_count = 0;
    std::size_t nonstandard_leaf_count = 0;
    std::size_t invalid_branch_count = 0;
    std::size_t invalid_bpb_count = 0;
    std::size_t invalid_checksum_count = 0;
    for (const auto& asset : eon::inventory_verified_release(release)) {
        if (asset.kind != eon::AssetKind::atari_st_disk) continue;
        ++atari_leaf_count;
        const auto candidate = eon::extract_verified_release_asset(release, asset.sha256);
        if (!candidate) throw std::runtime_error("Verified Deuteros Atari ST asset disappeared during scan");
        const auto evidence = eon::inspect_deuteros_atari_media(*candidate);
        if (!evidence.standard_protected_geometry) {
            ++nonstandard_leaf_count;
            continue;
        }
        ++protected_geometry_count;
        switch (evidence.boot_envelope_status) {
        case eon::DeuterosAtariMediaEvidence::BootEnvelopeStatus::invalid_branch:
            ++invalid_branch_count;
            break;
        case eon::DeuterosAtariMediaEvidence::BootEnvelopeStatus::invalid_bpb:
            ++invalid_bpb_count;
            break;
        case eon::DeuterosAtariMediaEvidence::BootEnvelopeStatus::invalid_checksum:
            ++invalid_checksum_count;
            break;
        case eon::DeuterosAtariMediaEvidence::BootEnvelopeStatus::nonstandard_geometry:
        case eon::DeuterosAtariMediaEvidence::BootEnvelopeStatus::valid:
            break;
        }
        if (!evidence.valid_boot_profile) continue;
        ++valid_boot_profile_count;
        if (evidence.recovered_replicants_first_stage) ++replicants_boot_count;
        if (evidence.killer_boot_signature) ++killer_boot_count;
    }
    std::cout << "          protected-media variant census: " << atari_leaf_count << " supplied ST leaves, "
        << protected_geometry_count << " 720 KiB candidates, " << valid_boot_profile_count
        << " valid checksum/BPB boot profiles, " << replicants_boot_count
        << " Replicants first-stage shapes, " << killer_boot_count << " KILLER_BOOT markers, "
        << nonstandard_leaf_count << " nonstandard leaves; invalid envelope branch/BPB/checksum "
        << invalid_branch_count << "/" << invalid_bpb_count << "/" << invalid_checksum_count
        << " (read-only classification; no profile substitution, FAT namespace, XBIOS, or execution)\n";
    std::cout << "          bounded launcher bootstrap: first/second raw stages SHA-256 "
        << live_bootstrap.first_stage_sha256() << "/"
        << live_bootstrap.second_stage_sha256()
        << " (no XBIOS, callback, or state selection)\n";
    const auto& entry_execution = live_bootstrap.entry_execution();
    std::cout << "          executed local entry join: stage +0x" << std::hex
        << entry_execution.join_offset << " +0x" << entry_execution.executed_byte_count
        << " sets SP=0x" << entry_execution.application_stack << " then JMP 0x"
        << entry_execution.dispatcher_entry << " (SHA-256 " << entry_execution.sha256
        << "; stops before copied dispatcher +0x"
        << entry_execution.stop_before_dispatcher_source_offset << std::dec << ")\n";
    if (stage.has_recovered_first_stage) {
        const auto first_stage = disk1.read_sectors(stage.first_stage_track, stage.first_stage_side,
            stage.first_stage_sector, stage.first_stage_sector_count);
        const auto profile = eon::parse_deuteros_atari_first_stage(first_stage);
        const auto second_stage = disk1.read_sectors(profile.next_track, profile.next_side,
            profile.next_sector, profile.next_sector_count);
        const auto second_profile = eon::parse_deuteros_atari_second_stage(second_stage);
        const auto dispatch = eon::parse_deuteros_atari_dispatch(second_stage);
        const auto& state0_plan = live_bootstrap.state0_raw_load_plan();
        const auto& state0_duplicate = live_bootstrap.state0_duplicate_stage_prefix();
        const auto& state1_plan = live_bootstrap.state1_raw_load_plan();
        const auto& state1_service = live_bootstrap.state1_service_boundary();
        const auto& state1_skipped_ascii = live_bootstrap.state1_skipped_ascii_block();
        const auto& state1_display_service = live_bootstrap.state1_display_service_boundary();
        const auto& state5_plan = live_bootstrap.state5_raw_load_plan();
        const auto& state5_state1_prefix = live_bootstrap.state5_state1_prefix();
        const auto& state5_return = live_bootstrap.state5_return();
        const auto& supervisor_callback = live_bootstrap.supervisor_callback();
        const auto& second_callee_continuation = live_bootstrap.second_callee_continuation();
        const auto& raw_reader_wrapper = live_bootstrap.raw_reader_wrapper();
        const auto& raw_reader_call = live_bootstrap.raw_reader_call_layout();
        const auto& direct_vector_callees = live_bootstrap.direct_vector_callees();
        const auto& direct_vector_transfer_loop = live_bootstrap.direct_vector_transfer_loop();
        const auto& direct_vector_transfer_tail = live_bootstrap.direct_vector_transfer_tail();
        const auto& state_selection = live_bootstrap.state_selection_layout();
        const auto& state_selection_continuation = live_bootstrap.state_selection_continuation();
        std::cout << "          Disk 1 XBIOS first stage: track " << stage.first_stage_track
            << ", side " << static_cast<unsigned>(stage.first_stage_side) << ", sectors "
            << static_cast<unsigned>(stage.first_stage_sector) << ".."
            << stage.first_stage_sector_count << " (" << first_stage.size()
            << " original bytes)\n";
        std::cout << "          First-stage control flow: entry +0x" << std::hex
            << profile.entry_offset << ", checksum +0x" << profile.checksum_start_offset
            << " +0x" << profile.checksum_byte_count << " (seed 0x"
            << profile.checksum_seed << ", expected 0x" << profile.checksum_expected
            << "); next raw load track " << std::dec << profile.next_track << ", side "
            << static_cast<unsigned>(profile.next_side) << ", sectors "
            << static_cast<unsigned>(profile.next_sector) << ".." << profile.next_sector_count
            << " -> RAM 0x" << std::hex << profile.next_destination << std::dec << '\n';
        std::cout << "          Track-2 loader: supervisor stack 0x" << std::hex
            << second_profile.supervisor_stack << ", application stack 0x"
            << second_profile.application_stack << ", direct entry 0x"
            << second_profile.direct_entry << std::dec << "; raw reader +0x"
            << std::hex << second_profile.raw_read_routine_offset << std::dec
            << " caps at " << second_profile.raw_read_max_sector_count << " sectors\n";
        std::cout << "          Copied handoff provenance: RAM 0x" << std::hex
            << profile.copy_source << " + 0x" << second_profile.direct_entry_source_offset
            << " -> 0x" << second_profile.direct_entry << "; state 0x"
            << second_profile.dispatch_state_address << " indexes table 0x"
            << second_profile.dispatch_table_address << std::dec << '\n';
        std::cout << "          Static dispatch vectors: 0x" << std::hex
            << dispatch.vector_addresses[0] << ", 0x" << dispatch.vector_addresses[1]
            << "; state 0 raw args (RAM 0x" << dispatch.state0_destination << ", 0x"
            << dispatch.state0_byte_count << " bytes, sector 0x"
            << dispatch.state0_linear_sector << ")" << std::dec << '\n';
        std::cout << "          Static aliases: table slots 2/3/4 -> state-0 routine 0x"
            << std::hex << dispatch.vector_addresses[0] << std::dec << '\n';
        std::cout << "          State-1 vector service boundary: stage +0x" << std::hex
            << state1_service.callee_offset << " pushes 0x" << state1_service.longword_argument
            << " and XBIOS selector 0x" << state1_service.xbios_selector << ", TRAP #14; "
            << "ADDQ.L #" << std::dec << static_cast<unsigned>(state1_service.stack_cleanup_bytes)
            << ",A7 then returns raw args (RAM 0x" << std::hex << state1_service.destination
            << ", 0x" << state1_service.byte_count << " bytes, sector 0x"
            << state1_service.linear_sector << "); SHA-256 " << state1_service.callee_sha256
            << std::dec << " (no XBIOS execution, state selection, or media interpretation)\n";
        std::vector<std::uint8_t> state0_bytes;
        for (const auto& request : state0_plan.requests) {
            const auto chunk = disk1.read_sectors(request.track, request.side,
                request.first_sector, request.sector_count);
            state0_bytes.insert(state0_bytes.end(), chunk.begin(), chunk.end());
        }
        std::cout << "          Static state-0 raw-load plan: Disk 1 +0x" << std::hex
            << state0_plan.source_offset << " +0x" << state0_plan.byte_count
            << " -> RAM 0x" << state0_plan.destination << std::dec << " in "
            << state0_plan.requests.size() << " original nine-sector reads; SHA-256 "
            << eon::to_hex(eon::sha256(state0_bytes))
            << " (not selected or interpreted at runtime)\n";
        std::cout << "          State-0 duplicate-stage boundary: prefix +0x0 +0x"
            << std::hex << state0_duplicate.byte_count << " SHA-256 "
            << state0_duplicate.sha256 << "; direct entry +0x"
            << state0_duplicate.direct_entry_offset << ", dispatcher +0x"
            << state0_duplicate.dispatcher_offset << std::dec
            << " (identity only; no state-0 selection or entry inferred)\n";
        std::vector<std::uint8_t> state1_bytes;
        state1_bytes.reserve(state1_plan.byte_count);
        for (const auto& request : state1_plan.requests) {
            const auto chunk = disk1.read_sectors(request.track, request.side,
                request.first_sector, request.sector_count);
            state1_bytes.insert(state1_bytes.end(), chunk.begin(), chunk.end());
        }
        std::cout << "          Static state-1 raw-load plan: Disk 1 +0x" << std::hex
            << state1_plan.source_offset << " +0x" << state1_plan.byte_count
            << " -> RAM 0x" << state1_plan.destination << std::dec << " in "
            << state1_plan.requests.size() << " original reads; SHA-256 "
            << eon::to_hex(eon::sha256(state1_bytes))
            << " (not selected or interpreted at runtime)\n";
        std::cout << "          State-1 skipped ASCII evidence: Disk 1 +0x" << std::hex
            << state1_plan.source_offset + state1_skipped_ascii.branch_relative_offset
            << " BRA.W displacement 0x" << static_cast<std::uint16_t>(
                state1_skipped_ascii.branch_displacement)
            << "; block +0x" << state1_plan.source_offset
                + state1_skipped_ascii.ascii_relative_offset
            << " +0x" << state1_skipped_ascii.ascii_byte_count << std::dec << " ("
            << state1_skipped_ascii.printable_run_count << " printable runs), SHA-256 "
            << state1_skipped_ascii.ascii_sha256
            << "; marker offsets +0x" << std::hex
            << state1_skipped_ascii.presentation_marker_offset << "/+0x"
            << state1_skipped_ascii.game_name_marker_offsets[0] << "/+0x"
            << state1_skipped_ascii.game_name_marker_offsets[1] << std::dec
            << " (preservation metadata only; never rendered, translated, or interpreted)\n";
        std::cout << "          State-1 display-service boundary: Disk 1 +0x" << std::hex
            << state1_plan.source_offset + state1_display_service.branch_relative_offset
            << " BRA.W displacement 0x" << static_cast<std::uint16_t>(
                state1_display_service.branch_displacement)
            << " -> +0x" << state1_plan.source_offset
                + state1_display_service.service_setup_relative_offset
            << "; setup +0x" << state1_display_service.service_setup_byte_count
            << " SHA-256 " << state1_display_service.service_setup_sha256
            << "; XBIOS selector 0x" << state1_display_service.xbios_selector << std::dec
            << " (static branch/setup only; no relocation, service result, display, or execution inferred)\n";
        std::cout << "          State-5/State-1 prefix: Disk 1 +0x" << std::hex
            << state5_state1_prefix.source_offset << " +0x" << state5_state1_prefix.byte_count
            << " SHA-256 " << state5_state1_prefix.sha256 << std::dec
            << " (physical byte identity only; no state-5 selection or raw-reader result inferred)\n";
        const auto materialize_raw_range = [&disk1](const auto& plan) {
            std::vector<std::uint8_t> bytes;
            bytes.reserve(plan.byte_count);
            for (const auto& request : plan.requests) {
                const auto chunk = disk1.read_sectors(request.track, request.side,
                    request.first_sector, request.sector_count);
                bytes.insert(bytes.end(), chunk.begin(), chunk.end());
            }
            return bytes;
        };
        const auto state5_first_bytes = materialize_raw_range(state5_plan.first_read);
        const auto state5_second_bytes = materialize_raw_range(state5_plan.second_read);
        std::cout << "          Static vector-5 raw-load plans: Disk 1 +0x" << std::hex
            << state5_plan.first_read.source_offset << " +0x" << state5_plan.first_read.byte_count
            << " -> RAM 0x" << state5_plan.first_read.destination << std::dec << " in "
            << state5_plan.first_read.requests.size() << " original reads; SHA-256 "
            << eon::to_hex(eon::sha256(state5_first_bytes)) << "; copy RAM 0x" << std::hex
            << state5_plan.copy_source << " +0x" << state5_plan.copy_byte_count << " -> 0x"
            << state5_plan.copy_destination << std::dec << "; Disk 1 +0x" << std::hex
            << state5_plan.second_read.source_offset << " +0x" << state5_plan.second_read.byte_count
            << " -> RAM 0x" << state5_plan.second_read.destination << std::dec << " in "
            << state5_plan.second_read.requests.size() << " original reads; SHA-256 "
            << eon::to_hex(eon::sha256(state5_second_bytes))
            << " (not selected or interpreted at runtime)\n";
        std::cout << "          Static vector 5: raw args (RAM 0x" << std::hex
            << dispatch.state5_first_destination << ", 0x" << dispatch.state5_first_byte_count
            << " bytes, reader 0x" << dispatch.state5_first_reader_argument
            << "), copy 0x" << dispatch.state5_copy_source << " -> 0x"
            << dispatch.state5_copy_destination << " +0x" << dispatch.state5_copy_byte_count
            << ", then reader 0x" << dispatch.state5_second_reader_argument << std::dec << '\n';
        std::cout << "          Vector-5 return: stage +0x" << std::hex
            << state5_return.branch_offset << " BRA.W " << std::dec
            << state5_return.branch_displacement << " -> stage +0x" << std::hex
            << state5_return.branch_target_offset << "; SHA-256 " << state5_return.branch_sha256
            << "; tail MOVE.W $" << state5_return.state_word_address << ",D0 / RTS at +0x"
            << state5_return.dispatcher_tail_offset << "; SHA-256 "
            << state5_return.dispatcher_tail_sha256 << std::dec
            << " (validated only; no state selection, raw read, or XBIOS execution)\n";
        std::cout << "          Supervisor callback boundary: stage +0x" << std::hex
            << supervisor_callback.callsite_offset << " pushes callback 0x"
            << supervisor_callback.callback_address << " and XBIOS selector 0x"
            << supervisor_callback.xbios_selector << ", TRAP #14 0x"
            << supervisor_callback.trap_opcode << "; callsite SHA-256 "
            << supervisor_callback.callsite_sha256 << "; callback +0x"
            << supervisor_callback.callback_offset << " loads (A7),D0, sets A7=0x"
            << supervisor_callback.callback_stack_address << ", pushes D0, RTS; SHA-256 "
            << supervisor_callback.callback_sha256 << std::dec
            << " (ABI boundary only; no XBIOS/callback execution)\n";
        std::cout << "          Static raw-reader wrapper: stage +0x" << std::hex
            << raw_reader_wrapper.wrapper_offset << " +0x" << raw_reader_wrapper.wrapper_byte_count
            << " BSR.W -> +0x" << raw_reader_wrapper.raw_reader_bsr_target_offset
            << "; status-word branch -> +0x" << raw_reader_wrapper.nonzero_branch_target_offset
            << "; loop -> +0x" << raw_reader_wrapper.loop_branch_target_offset
            << "; SHA-256 " << raw_reader_wrapper.wrapper_sha256 << std::dec
            << " (layout only; no status value, XBIOS result, or reachability inferred)\n";
        std::cout << "          Raw-reader call layout: stage +0x" << std::hex
            << raw_reader_call.routine_offset << " +0x" << raw_reader_call.routine_byte_count
            << " count cap 0x" << raw_reader_call.count_compare_immediate
            << "; ABI opcode 0x" << raw_reader_call.abi_call_opcode << " selector 0x"
            << raw_reader_call.abi_selector << "; post-call store $"
            << raw_reader_call.post_call_store_address << ", RTS; SHA-256 "
            << raw_reader_call.routine_sha256 << std::dec
            << " (static bytes only; no ABI result or disk operation is performed)\n";
        std::cout << "          Direct dispatch-table callees: slots "
            << direct_vector_callees.distinct_callees[0].vector_slot << "/"
            << direct_vector_callees.distinct_callees[1].vector_slot << "/"
            << direct_vector_callees.distinct_callees[2].vector_slot << " at stage +0x"
            << std::hex << direct_vector_callees.distinct_callees[0].stage_offset << "/+0x"
            << direct_vector_callees.distinct_callees[1].stage_offset << "/+0x"
            << direct_vector_callees.distinct_callees[2].stage_offset << "; hashes "
            << direct_vector_callees.distinct_callees[0].sha256 << "/"
            << direct_vector_callees.distinct_callees[1].sha256 << "/"
            << direct_vector_callees.distinct_callees[2].sha256 << std::dec
            << "; alias BRA +0x" << std::hex << direct_vector_callees.alias_branch_offset
            << " -> +0x" << direct_vector_callees.alias_branch_target_offset << std::dec
            << " (table/linkage bytes only; no index, call, return, or state interpretation)\n";
        std::cout << "          Direct-vector literal transfer loop: stage +0x" << std::hex
            << direct_vector_transfer_loop.loop_block_offset << " +0x"
            << direct_vector_transfer_loop.loop_block_byte_count << " writes literal A0 0x"
            << direct_vector_transfer_loop.destination_pointer << " and A1 0x"
            << direct_vector_transfer_loop.source_pointer << "; DBF displacement " << std::dec
            << direct_vector_transfer_loop.dbf_displacement << " -> stage +0x" << std::hex
            << direct_vector_transfer_loop.dbf_target_offset << "; SHA-256 "
            << direct_vector_transfer_loop.loop_block_sha256 << std::dec
            << " (byte layout only; no vector selection, transfer, call, return, or media behavior)\n";
        std::cout << "          Direct-vector transfer tail: stage +0x" << std::hex
            << direct_vector_transfer_tail.tail_offset << " +0x"
            << direct_vector_transfer_tail.tail_byte_count << " BSR.W -> +0x"
            << direct_vector_transfer_tail.range_wrapper_bsr_target_offset << "; BRA.W -> +0x"
            << direct_vector_transfer_tail.dispatcher_return_bra_target_offset << "; SHA-256 "
            << direct_vector_transfer_tail.tail_sha256 << std::dec
            << " (static bytes only; no vector selection, call, return, transfer, or media behavior)\n";
        std::cout << "          State-selection layout: stage +0x" << std::hex
            << state_selection.input_capture_offset << " loads RAM $"
            << state_selection.source_longword_address << " then stores its low word at $"
            << state_selection.state_word_address << "; SHA-256 "
            << state_selection.input_capture_sha256 << "; later stage +0x"
            << state_selection.table_lookup_offset << " reads that word, shifts it by two, loads"
            << " table $" << state_selection.table_base_address << ", then JSR (A1); SHA-256 "
            << state_selection.table_lookup_sha256 << std::dec
            << " (static layout only; no input value, service return, bounds check, or vector"
            << " selection inferred)\n";
        std::cout << "          Post-indirect-call layout: stage +0x" << std::hex
            << state_selection_continuation.continuation_offset << " saves D1, advances D4 by 0x"
            << state_selection_continuation.raw_reader_argument_advance_bytes
            << ", transfers D2 to D7, then BSR.W -> +0x"
            << state_selection_continuation.raw_reader_wrapper_target_offset << "; SHA-256 "
            << state_selection_continuation.continuation_sha256 << std::dec
            << " (layout only; no callee return, register meaning, raw read, or reachability inferred)\n";
        std::cout << "          Post-raw-reader continuation: stage +0x" << std::hex
            << second_callee_continuation.continuation_offset << " -> XBIOS selector 0x"
            << second_callee_continuation.trap_selector << " with pointer 0x"
            << second_callee_continuation.trap_argument_address << "; SHA-256 "
            << second_callee_continuation.continuation_sha256 << "; post-service layout copies "
            << std::dec << (static_cast<std::uint32_t>(second_callee_continuation.copy_loop_counter) + 1U)
            << " longwords from RAM 0x" << std::hex << second_callee_continuation.copy_source
            << " through pointer at 0x" << second_callee_continuation.copy_destination_pointer_address
            << " then RTS (not reached or executed without raw-reader/service return evidence)\n";
    }
    std::cout << "          Disk 2 boot continuation: "
        << (continuation.killer_boot_signature ? "KILLER_BOOT signature" : "unclassified")
        << ", branch 0x" << std::hex << continuation.boot_branch_target << std::dec << '\n';
    if (continuation.has_killer_boot_continuation_profile) {
        const auto disk2_boot = disk2.read_sectors(0, 0, 1, 1);
        const auto killer_handoff = eon::parse_deuteros_atari_killer_boot_handoff(
            disk2_boot, continuation);
        const auto killer_execution = eon::execute_deuteros_atari_killer_boot_prefix(
            disk2_boot, continuation);
        std::cout << "          Disk 2 relocated continuation: boot +0x" << std::hex
            << continuation.killer_boot_vector_source_offset << " +0x"
            << continuation.killer_boot_relocated_byte_count << " -> RAM 0x"
            << continuation.killer_boot_vector_destination << "; SHA-256 "
            << continuation.killer_boot_relocated_sha256 << "; clears 0x"
            << continuation.killer_boot_clear_start << " in 0x"
            << continuation.killer_boot_clear_stride << "-byte blocks (not executed)" << std::dec
            << '\n';
        std::cout << "          Disk 2 KILLER_BOOT handoff: setup +0x" << std::hex
            << killer_handoff.setup_offset << " +0x" << killer_handoff.setup_byte_count
            << " copies boot +0x" << killer_handoff.source_offset << " +0x"
            << killer_handoff.byte_count << " -> RAM 0x" << killer_handoff.destination
            << "; JMP 0x" << killer_handoff.continuation_address << " enters relocated +0x"
            << killer_handoff.continuation_relocated_offset << " opcode 0x"
            << killer_handoff.continuation_first_opcode << "; copied vector +0x"
            << killer_handoff.vector_jump_relocated_offset << " is JMP (A0) through RAM $"
            << killer_handoff.vector_jump_pointer_address << "; setup/relocated SHA-256 "
            << killer_handoff.setup_sha256 << "/" << killer_handoff.relocated_sha256 << std::dec
            << " (the indirect vector-cell path does not execute)\n";
        std::cout << "          Disk 2 local execution prefix: copied "
            << killer_execution.relocated_longwords.size() << " original longwords to RAM 0x"
            << std::hex << killer_execution.relocation_destination << ", direct continuation 0x"
            << killer_execution.continuation_address << " clears 8 longwords at 0x"
            << killer_execution.first_clear_address << "..0x"
            << killer_execution.cleared_longword_addresses.back() << ", then branches to 0x"
            << killer_execution.loop_target_address << " with A0=0x"
            << killer_execution.next_clear_address << std::dec
            << " (isolated writes; no vector, ABI, or unbounded loop execution)\n";
    }
    std::cout << "          Atari ST trace boundary: next evidence must identify the XBIOS Floprd result, "
        << "callback entry/return frame, dispatch word at RAM 0x1eaa, and selected vector D1/D2 "
        << "returns. Reported raw-load plans are not performed and no Amiga or synthetic screen is used.\n";
}


// Modern packs are intentionally a report-only preservation boundary.  This
// routine does not retain a pack object beyond inspection, nor does it create
// a default directory, decode an asset, or give a renderer any pack choice.
void report_modern_asset_packs(const std::filesystem::path& root,
                               const std::vector<eon::ReleaseArchive>& inspected_releases) {
    std::cout << "MODERN PACKS  read-only admission report; no pack is selected or rendered\n";
    std::error_code error;
    const auto status = std::filesystem::symlink_status(root, error);
    if (error || std::filesystem::is_symlink(status) || !std::filesystem::is_directory(status)) {
        std::cout << "MODERN PACK ROOT REJECTED  must be an existing non-symlink directory: "
            << root << '\n';
        return;
    }
    const auto validations = eon::discover_modern_asset_packs(root);
    if (validations.empty()) {
        std::cout << "MODERN PACKS  no direct-child pack.eonmodern candidates\n";
        return;
    }
    for (const auto& validation : validations) {
        if (!validation.accepted()) {
            std::cout << "MODERN PACK REJECTED  " << validation.manifest_path << '\n'
                << "          " << validation.error << '\n';
            continue;
        }
        const auto& pack = validation.pack;
        const bool source_is_inspected = std::any_of(inspected_releases.begin(), inspected_releases.end(),
            [&pack](const eon::ReleaseArchive& release) {
                return release.game == pack.game && release.platform == pack.platform
                    && release.sha256 == pack.source_release_sha256;
            });
        if (!source_is_inspected) {
            std::cout << "MODERN PACK REJECTED  " << validation.manifest_path << '\n'
                << "          pack source is not one of this inspection's reverified original releases\n";
            continue;
        }
        std::cout << "MODERN PACK ELIGIBLE  " << pack.id << " " << pack.version << '\n'
            << "          " << eon::name(pack.game) << " / " << eon::name(pack.platform)
            << " / source " << pack.source_release_sha256 << '\n'
            << "          " << pack.provenance << "; " << pack.assets.size()
            << " hash-verified external assets; admission only\n";
        const auto targets = eon::modern_asset_pack_renderer_targets(pack);
        std::cout << "          renderer targets: Millennium title "
            << (targets.millennium_dos_title_1280x800 ? "1280x800"
                : targets.millennium_dos_title_640x400 ? "640x400" : "none")
            << "; Deuteros opening " << targets.deuteros_amiga_opening_640x400_frames
            << "/" << eon::deuteros_amiga_held_opening_frame_count << " at 640x400, "
            << targets.deuteros_amiga_opening_1280x800_frames << "/"
            << eon::deuteros_amiga_held_opening_frame_count << " at 1280x800\n";
    }
}

} // namespace

int main(int argc, char** argv) {
    const auto parsed = eon::parse_command_line(argc, argv);
    if (parsed.help) {
        std::cout << eon::usage();
        return 0;
    }
    if (!parsed.request) {
        std::cerr << parsed.error << "\n\n" << eon::usage();
        return 2;
    }
    auto request = *parsed.request;
    const auto presentation_preferences_path = eon::default_presentation_preferences_path();
    const auto saved_presentation_preferences = eon::load_presentation_preferences(
        presentation_preferences_path);
    // Persisted locale is a card-menu preference, never an ambient host-locale
    // inference and never a diagnostic/CLI output switch. English remains the
    // default when no setting exists; an explicit --language always wins.
    const bool graphical_menu_requested = !request.verify_game && !request.inspect_data && !request.game
        && !request.reference_trace && !request.inspect_save;
    if (graphical_menu_requested && !request.language_explicit && saved_presentation_preferences) {
        request.language = saved_presentation_preferences->launcher_language;
    }
    auto translator = eon::Translator::from_language(request.language,
        argc > 0 ? std::filesystem::path(argv[0]) : std::filesystem::path{});
    active_translator = &translator;
    const auto tr = [&translator](std::string_view message) {
        return std::string(translator.translate(message));
    };
    if (request.inspect_save) return inspect_millennium_dos_save(*request.inspect_save);
    if (saved_presentation_preferences) {
        if (!request.display_resolution_explicit) {
            request.display.width = output_resolutions.at(
                saved_presentation_preferences->output_resolution_index).width;
            request.display.height = output_resolutions.at(
                saved_presentation_preferences->output_resolution_index).height;
        }
        if (!request.display_aspect_explicit) {
            request.display.aspect_ratio_index = saved_presentation_preferences->aspect_ratio_index;
        }
    }
    const bool command_requires_data = request.verify_game || request.inspect_data
        || request.game || request.reference_trace;
    if (command_requires_data && !eon::is_original_data_source(
            eon::classify_original_data_source(request.data_directory))) {
        std::cerr << "Data path does not exist: " << request.data_directory << '\n';
        return 2;
    }
    auto scanner = std::make_unique<eon::ReleaseScanner>(request.data_directory);
    std::vector<eon::ReleaseArchive> releases;
    // Direct launches and command-line verification intentionally wait for a
    // complete answer. The graphical menu instead advances this scanner after
    // its first frame, mirroring OpenCaptive's non-blocking data scanner.
    if (request.verify_game || request.inspect_data || request.game || request.reference_trace) {
        while (!scanner->advance(64)) {
        }
        releases = scanner->releases();
        if (releases.empty()) {
            std::cerr << "No recognised original release archives found.\n";
            return 3;
        }
    }
    if (request.reference_trace) {
        const auto validation = eon::validate_reference_trace(*request.reference_trace, releases,
            *request.game, *request.platform);
        if (!validation.trace) {
            std::cerr << "Reference trace rejected: " << validation.error << '\n';
            return 6;
        }
        const auto& trace = *validation.trace;
        try {
            // A trace is evidence about one immutable release. Re-read and
            // hash the current archive before reporting it so a post-scan
            // replacement cannot inherit the earlier admission result.
            eon::verify_release_archive(trace.source_release);
        } catch (const std::exception& error) {
            std::cerr << "Reference trace rejected: source release no longer verifies: "
                << error.what() << '\n';
            return 6;
        }
        // The registry is the only policy authority. Its sole non-diagnostic
        // policy remains the narrow GX exception: the release-runtime gate
        // reopens and rehashes source/events before a call-free overlay is
        // constructed, and never launches or publishes a session.
        std::optional<eon::MillenniumDosGxStartupTraceAdmission> gx_admission;
        const auto* trace_descriptor = eon::reference_trace_adapter_descriptor(trace.adapter);
        if (!trace.adapter.empty() && trace_descriptor == nullptr) {
            std::cerr << "Reference trace rejected: adapter is absent from the policy registry\n";
            return 6;
        }
        if (trace_descriptor && trace_descriptor->runtime_policy
                == eon::ReferenceTraceRuntimePolicy::transient_call_free_gx_startup) {
            const eon::ReleaseRuntimeCoordinator trace_gate;
            gx_admission.emplace(trace_gate.admit_millennium_dos_gx_startup_reference_trace(trace));
            if (!gx_admission->session) {
                std::cerr << "Reference trace rejected: " << gx_admission->error << '\n';
                return 6;
            }
        }
        if (request.reference_trace_json) {
            report_reference_trace_json(trace);
            return 0;
        }
        std::cout << "REFERENCE TRACE VERIFIED  provenance-only; no replay performed\n"
            << "          " << eon::name(trace.source_release.game) << " / "
            << eon::name(trace.source_release.platform) << " / " << trace.source_release.language << '\n'
            << "          source " << trace.source_release.sha256 << '\n'
            << "          events " << trace.event_sha256 << " (" << trace.event_count << " ordered events)\n"
            << "          capture " << trace.capture_start_utc << " to " << trace.capture_end_utc << '\n'
            << "          emulator " << trace.emulator_name << " " << trace.emulator_version << '\n'
            << "          capture fingerprints config=" << trace.config_sha256
            << " command-tail=" << trace.command_tail_sha256
            << " input-timeline=" << trace.input_timeline_sha256 << '\n';
        for (const auto& artifact : trace.artifacts) {
            std::cout << "          artifact " << artifact.role << " " << artifact.sha256
                << " (" << artifact.size << " bytes; " << artifact.format
                << "; verified diagnostics only)\n";
        }
        if (!trace.adapter.empty()) {
            if (!trace.source_media_sha256.empty()) {
                std::cout << "          source media " << trace.source_media_sha256 << '\n'
                    << "          source stage " << trace.source_stage_sha256 << '\n';
            }
            const auto summary = eon::reference_trace_diagnostic_summary(trace);
            std::cout << "          adapter " << trace.adapter << " (" << summary.observations
                << "; " << summary.disposition << ")\n";
            for (const auto& boundary : trace.recovery_boundaries) {
                std::cout << "          RECOVERY MAP " << boundary.id << " at "
                    << boundary.source_address << " (" << boundary.documentation_anchor << ")\n";
            }
        }
        return 0;
    }
    if (request.verify_game || request.inspect_data) {
        if (request.inspect_data && !request.inspect_json) {
            std::cout << "INSPECTION  read-only provenance scan; original media stays in place\n";
        }
        bool found = false;
        std::vector<eon::ReleaseArchive> inspected_releases;
        for (const auto& release : releases) {
            if (request.verify_game && release.game != *request.verify_game) continue;
            if (request.inspect_data && request.game && release.game != *request.game) continue;
            if (request.inspect_data && request.platform && release.platform != *request.platform) continue;
            // A release language is an immutable original-media identity,
            // not a display locale. Inspection applies the same exact filter
            // as a direct launch, so a Spanish request cannot report English
            // evidence as an invisible edition fallback.
            if (request.inspect_data && request.release_language
                && release.language != *request.release_language) continue;
            if (request.inspect_data && request.release_sha256
                && release.sha256 != *request.release_sha256) continue;
            try {
                eon::verify_release_archive(release);
            } catch (const std::exception& error) {
                std::cerr << "Recognised release changed after scan; refusing to inspect it: "
                    << error.what() << '\n';
                return 6;
            }
            found = true;
            inspected_releases.push_back(release);
            if (request.inspect_json) continue;
            std::cout << "VERIFIED  " << eon::name(release.game) << " / "
                << eon::name(release.platform) << " / " << release.language << '\n'
                << "          " << release.sha256 << '\n'
                << "          " << release.path << '\n';
            if (const auto direct_set = eon::direct_media_set_sha256(release)) {
                std::cout << "          VERIFIED DIRECTORY SET  " << *direct_set << '\n';
            }
            report_recovery_map(release);
            report_startup_boundary(release);
            report_atari_launch_boundary(release);
            if (release.game == eon::Game::deuteros
                && release.platform == eon::Platform::amiga) {
                report_deuteros_amiga(release);
            }
            if (release.game == eon::Game::deuteros
                && release.platform == eon::Platform::atari_st) {
                report_deuteros_atari_st(release);
            }
            if (release.game == eon::Game::millennium
                && release.platform == eon::Platform::amiga
                && release.language == "en") {
                report_millennium_amiga(release);
            }
            if (release.game == eon::Game::millennium
                && release.platform == eon::Platform::dos) {
                report_millennium_dos(release);
            }
            if (release.game == eon::Game::millennium
                && release.platform == eon::Platform::atari_st
                && release.language == "en") {
                report_millennium_atari_st(release);
            }
            if (request.inventory_assets) report_verified_release_inventory(release);
        }
        if (request.inspect_json) {
            if (!found) {
                std::cerr << "No recognised original release matches the requested inspection filters.\n";
                return 5;
            }
            std::optional<StaticControlFlowInspection> static_control_flow;
            if (request.static_control_flow_sidecar) {
                try {
                    static_control_flow.emplace(bind_static_control_flow_sidecar(
                        read_static_control_flow_sidecar(*request.static_control_flow_sidecar),
                        inspected_releases));
                } catch (const std::exception& error) {
                    std::cerr << "Static control-flow sidecar rejected: " << error.what() << '\n';
                    return 6;
                }
            }
            report_inspection_json(inspected_releases, scanner->snapshot(), static_control_flow);
            return 0;
        }
        if (request.modern_pack_root) {
            report_modern_asset_packs(*request.modern_pack_root, inspected_releases);
        }
        if (request.inspect_data) {
            const auto& report = scanner->report();
            // Filtered inspection intentionally reports only the selected
            // original identity, so it must not imply a complete card state.
            // The unfiltered report has reverified every identity and can
            // therefore expose the exact launcher admission table.
            if (!request.game && !request.platform && !request.release_language) {
                report_platform_admission(inspected_releases);
            }
            std::cout << "SCAN SUMMARY  " << eon::name(report.source_kind) << " source; "
                << report.candidates << " candidates; "
                << report.size_rejected_candidates << " size-rejected (not hashed); "
                << report.size_candidates << " manifest-size matches; "
                << report.hashed_candidates << " hashed; "
                << report.hash_rejected_candidates << " hash-rejected; "
                << report.verified_occurrences << " verified occurrences; "
                << report.verified_direct_set_occurrences << " verified direct-set occurrences; "
                << report.verified_container_set_occurrences << " verified container-set occurrences; "
                << releases.size() << " unique releases; "
                << report.verified_direct_media_occurrences << " verified unbound direct-media occurrences; "
                << scanner->unbound_direct_media().size() << " unique unbound direct-media leaves; "
                << report.duplicate_occurrences << " duplicate occurrences; "
                << report.duplicate_direct_set_occurrences << " duplicate direct-set occurrences; "
                << report.duplicate_direct_media_occurrences << " duplicate unbound direct-media occurrences; "
                << report.symlink_rejected_entries << " symlink entries rejected; "
                << report.unreadable_candidates << " unreadable candidates\n";
        }
        if (!found) {
            if (request.inspect_data) {
                std::cerr << "No recognised original release matches the requested inspection filters.\n";
            } else {
                std::cerr << "No recognised original release matches the requested verification game.\n";
            }
        }
        return found ? 0 : 5;
    }
    if (request.game && !eon::release_available(releases, *request.game, request.platform)) {
        std::cerr << "Requested original release is not present for the selected platform. "
                     "Use --inspect to list hash-recognised releases; no platform fallback was selected.\n";
        return 4;
    }
    // A CLI platform request is fixed.  The start menu can otherwise choose
    // among the hash-verified platform releases it has actually discovered.
    eon::LauncherInteractionController launcher_interaction;
    auto& launcher_session = launcher_interaction.session;
    auto& launcher_route = launcher_session.route;
    launcher_route.game = request.game.value_or(eon::Game::millennium);
    launcher_route.platform = request.platform;
    launcher_route.release_language = request.release_language;
    launcher_route.release_sha256 = request.release_sha256;
    auto& active_platform = launcher_route.platform;
    auto& active_release_language = launcher_route.release_language;
    auto& active_release_sha256 = launcher_route.release_sha256;
    eon::RuntimeHost runtime;
    // This status is limited to the same three media-safe admission classes
    // exposed by F10. It gives a rejected profile-card launch an immediate
    // visible result without retaining a path, leaf name, or parser error in
    // the start menu.
    std::string launcher_runtime_admission = std::string(
        eon::release_runtime_admission_label(runtime.admission()));
    std::string launcher_runtime_rejection = std::string(
        eon::release_runtime_rejection_label(runtime.rejection()));
    const auto active_launch = [&]() -> std::optional<eon::ResolvedLaunchRequest> {
        return runtime.active();
    };
    if (request.game && active_platform) {
        auto launch_candidate = request;
        launch_candidate.platform = active_platform;
        launch_candidate.release_sha256 = active_release_sha256;
        launch_candidate.release_language = active_release_language;
        const auto admission = runtime.launch_direct(launch_candidate, releases);
        launcher_runtime_admission = std::string(
            eon::release_runtime_admission_label(admission.admission));
        launcher_runtime_rejection = std::string(
            eon::release_runtime_rejection_label(admission.rejection));
        if (!admission.accepted()) {
            if (admission.rejection == eon::ReleaseRuntimeRejection::launch_identity) {
                std::cerr << "The selected game and platform need one exact verified original release. "
                             "Use --release-sha256 when several outer containers share a language; "
                             "no scan-order fallback was selected.\n";
            } else {
                // The common runtime gate intentionally keeps parser and
                // source details private. Its stable rejection vocabulary is
                // nevertheless useful to a CLI operator: do not misreport a
                // media/adapter boundary as an ambiguous release selection.
                std::cerr << "Native runtime admission rejected the selected verified release: "
                          << eon::release_runtime_rejection_label(admission.rejection) << ".\n";
            }
            return 4;
        }
        active_release_sha256 = active_launch()->request.release_sha256;
        active_release_language = active_launch()->request.release_language;
    }
    const auto resolve_active_release = [&](const eon::Game game) -> std::optional<eon::ReleaseArchive> {
        if (!active_launch() || active_launch()->request.game != game) return std::nullopt;
        return active_launch()->release;
    };
    std::optional<eon::MillenniumDosPresentationSnapshot> millennium_assets =
        runtime.millennium_dos_presentation();
    // The command-line path is an initial explicit selection, not a mutable
    // request object. Custom's native picker may replace this session-local
    // candidate before launch; neither route has a default pack location.
    std::optional<std::filesystem::path> selected_modern_pack_manifest =
        request.modern_pack_manifest;
    std::optional<eon::ModernAssetPackPreflight> selected_modern_pack_preflight;
    ModernPackAdmission modern_pack_admission = ModernPackAdmission::unselected;
    const auto clear_modern_pack_admission = [&] {
        selected_modern_pack_manifest.reset();
        selected_modern_pack_preflight.reset();
        modern_pack_admission = ModernPackAdmission::unselected;
    };
    const auto admit_modern_pack_for_release = [&](const std::filesystem::path& manifest,
                                                   const eon::ReleaseArchive& release) {
        const auto preflight = eon::preflight_modern_asset_pack(manifest, release.game,
            release.platform, release.sha256);
        selected_modern_pack_preflight = preflight;
        if (!preflight.accepted) {
            // Do not retain an inadmissible path for a later loader. This is
            // deliberately distinct from the loader's final byte rehash,
            // which protects against a pack changing after this UI check.
            selected_modern_pack_manifest.reset();
            modern_pack_admission = ModernPackAdmission::rejected;
            std::cerr << "Modern asset pack rejected before launch: " << preflight.error << '\n';
            return false;
        }
        selected_modern_pack_manifest = manifest;
        modern_pack_admission = ModernPackAdmission::ready;
        return true;
    };
    // Original mode never reads an optional Modern pack. A CLI path is kept
    // only for the Modern renderer route; this avoids even a manifest access
    // while the preservation profile is selected.
    if (request.presentation != eon::Presentation::modern) {
        clear_modern_pack_admission();
    } else if (selected_modern_pack_manifest && active_launch()) {
        admit_modern_pack_for_release(*selected_modern_pack_manifest, active_launch()->release);
    }
    // A path supplied on the command line is an explicit renderer request,
    // not an optional picker result. Do not let automation claim a successful
    // Modern launch check after silently discarding the requested pack. The
    // interactive picker remains recoverable: it can reject one selection and
    // let the user choose another without ending the launcher session.
    if (request.presentation == eon::Presentation::modern && request.modern_pack_manifest
        && modern_pack_admission != ModernPackAdmission::ready) {
        std::cerr << "Modern asset pack required by the CLI selection was rejected; launch aborted.\n";
        return 5;
    }

    // This is a bounded startup diagnostic for real-media CI and preservation
    // workstations. It crosses the exact same identity, outer-hash, and typed
    // adapter gate as a CLI/menu launch, then exits before SDL, input, audio,
    // rendering, game timing, or a save can be created or changed.
    if (request.launch_check) {
        if (!active_launch()) {
            std::cerr << "Launch check has no admitted original release.\n";
            return 4;
        }
        if (request.runtime_diagnostics_json) {
            report_runtime_diagnostics_json(*active_launch(), runtime.admission(), runtime.rejection(),
                runtime.session_snapshot(), runtime.millennium_dos_static_dispatch_diagnostics(),
                runtime.millennium_dos_owned_function_diagnostics(),
                runtime.millennium_dos_native_process_checkpoint(),
                runtime.deuteros_atari_bootstrap_checkpoint(),
                runtime.deuteros_amiga_title_dependency_chain_checkpoint(),
                runtime.native_code_image_registry_diagnostics(),
                request.presentation, request.display,
                display_aspect_identifiers.at(request.display.aspect_ratio_index));
            return 0;
        }
        if (request.launch_check_json) {
            std::cout << "{\"schema\":\"project-eon.launch-check/v1\",\"release\":{\"game\":";
            write_json_string(std::cout, eon::name(active_launch()->release.game));
            std::cout << ",\"platform\":";
            write_json_string(std::cout, eon::name(active_launch()->release.platform));
            std::cout << ",\"language\":";
            write_json_string(std::cout, active_launch()->release.language);
            std::cout << ",\"sha256\":";
            write_json_string(std::cout, active_launch()->release.sha256);
            // The exact presentation is part of the normalized launch
            // request. Report it beside the immutable release identity so a
            // preservation workstation can prove an Original check did not
            // silently become Modern (or vice versa), without serializing
            // renderer state, pack paths, original bytes, or save data.
            std::cout << "},\"presentation\":";
            write_json_string(std::cout, request.presentation == eon::Presentation::original
                ? "original" : "modern");
            std::cout << ",\"display\":{\"resolution\":";
            write_json_string(std::cout, std::to_string(request.display.width) + "x"
                + std::to_string(request.display.height));
            std::cout << ",\"aspect\":";
            write_json_string(std::cout,
                display_aspect_identifiers.at(request.display.aspect_ratio_index));
            std::cout << '}';
            std::cout << ",\"coverage\":";
            const auto diagnostics = eon::runtime_diagnostics_for_release(active_launch()->release);
            write_json_string(std::cout, eon::name(diagnostics.coverage));
            std::cout << ",\"runtime_admission\":";
            write_json_string(std::cout,
                eon::release_runtime_admission_label(runtime.admission()));
            std::cout << ",\"runtime_rejection\":";
            write_json_string(std::cout,
                eon::release_runtime_rejection_label(runtime.rejection()));
            // The controller publishes this copy; do not reconstruct a
            // parallel session identity from mutable launcher state. It is
            // intentionally limited to adapter/boundary/capability facts and
            // never serializes source paths, original bytes, SDL state or an
            // inferred game input contract.
            std::cout << ",\"runtime_session\":";
            if (const auto session = runtime.session_snapshot()) {
                std::cout << "{\"kind\":";
                write_json_string(std::cout, eon::runtime_session_kind_label(session->kind));
                std::cout << ",\"boundary\":";
                write_json_string(std::cout,
                    eon::runtime_session_boundary_label(session->boundary));
                std::cout << ",\"input_contract\":";
                write_json_string(std::cout,
                    eon::runtime_input_contract_identifier(session->input_contract));
                std::cout << ",\"capabilities\":{\"decoded_presentation\":"
                    << (session->capabilities.decoded_presentation ? "true" : "false")
                    << ",\"audio_observations\":"
                    << (session->capabilities.audio_observations ? "true" : "false")
                    << ",\"admitted_input\":"
                    << (session->capabilities.admitted_input ? "true" : "false")
                    << "}}";
            } else {
                std::cout << "null";
            }
            std::cout << "}\n";
            return 0;
        }
        std::cout << "LAUNCH CHECK  " << eon::name(active_launch()->release.game) << " / "
                  << eon::name(active_launch()->release.platform) << " / "
                  << active_launch()->release.language << " / "
                  << eon::release_runtime_admission_label(runtime.admission()) << '\n';
        return 0;
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Project Eon", request.display.width, request.display.height,
            SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        std::cerr << "SDL_CreateWindowAndRenderer failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderLogicalPresentation(renderer, 1280, 720, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(renderer, 1);
    if (const auto font_directory = find_font_directory()) {
        active_text_renderer = eon::UnicodeTextRenderer::create(renderer, *font_directory);
    }

    std::array<Card, 2> cards{{
        {eon::Game::millennium, "MILLENNIUM 2.2", "RETURN TO EARTH", "millennium.png", {64, 170, 552, 310}},
        {eon::Game::deuteros, "DEUTEROS", "THE NEXT MILLENNIUM", "deuteros.png", {664, 170, 552, 310}},
    }};
    for (auto& card : cards) card.texture = load_card(renderer, card.filename);
    std::array<PlatformCard, 3> platform_card_templates{{
        {eon::Platform::dos, "DOS", "dos-platform-v1.png", {64, 188, 352, 308}},
        {eon::Platform::amiga, "AMIGA", "amiga-platform-v1.png", {464, 188, 352, 308}},
        {eon::Platform::atari_st, "ATARI ST", "atari-st-platform-v1.png", {864, 188, 352, 308}},
    }};
    for (auto& card : platform_card_templates) card.texture = load_card(renderer, card.filename);
    std::array<ProfileCard, 3> profile_cards{{
        {ProfileChoice::original, "ORIGINAL", "PRESERVATION PROFILE", "original-profile-v1.png", {64, 188, 352, 308}},
        {ProfileChoice::modern, "MODERN", "ENHANCED PROFILE", "modern-profile-v1.png", {464, 188, 352, 308}},
        {ProfileChoice::custom, "CUSTOM", "TUNE MODERN SETTINGS", "custom-profile-v1.png", {864, 188, 352, 308}},
    }};
    for (auto& card : profile_cards) card.texture = load_card(renderer, card.filename);
    // Project Eon branding is original launcher artwork, never recovered game
    // pixels. It remains a renderer-only menu resource in every profile.
    SDL_Texture* project_eon_logo_texture = load_branding_texture(renderer, "project-eon-logo-v1.png");
    SDL_AudioStream* deuteros_audio_stream = nullptr;
    SDL_Texture* preview_texture = nullptr;
    // These transient textures are derived from decoded original pixels only.
    // They have no on-disk representation and are selected exclusively by
    // Modern at render time.
    SDL_Texture* modern_preview_texture = nullptr;
    eon::ModernPresentationPipeline deuteros_modern_pipeline;
    // The opening VM advances at a verified 20 ms cadence. Retain its
    // decoded frame only until that cadence produces a new source frame, so
    // presentation refreshes never repeatedly colorize or reconstruct the
    // same original pixels. This remains renderer-only process memory.
    std::optional<std::vector<std::uint8_t>> deuteros_preview_rgba;
    std::optional<std::uint64_t> deuteros_preview_source_tick;
    // A complete external Modern sequence is an alternative presentation of
    // the finite held-input route only.  It neither provides VM state nor
    // substitutes a single original pixel in Original mode.
    SDL_Texture* deuteros_external_modern_texture = nullptr;
    std::optional<eon::ModernAssetPackPresentationResolver>
        deuteros_external_modern_resolver;
    std::optional<eon::ModernAssetPackPngSurface> deuteros_external_modern_surface;
    std::optional<std::uint64_t> deuteros_external_modern_source_tick;
    bool deuteros_external_modern_attempted = false;
    const auto create_deuteros_opening_texture = [&] {
        if (runtime.state() != eon::NativeSessionState::deuteros_amiga_opening || preview_texture) return;
        preview_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STREAMING, eon::DeuterosAmigaFrame::width,
            eon::DeuterosAmigaFrame::height);
    };
    const auto start_deuteros_audio = [&] {
        if (runtime.state() != eon::NativeSessionState::deuteros_amiga_opening) return;
        if (deuteros_audio_stream) {
            static_cast<void>(SDL_ClearAudioStream(deuteros_audio_stream));
            return;
        }
        const SDL_AudioSpec format{SDL_AUDIO_F32, 2, 48'000};
        deuteros_audio_stream = SDL_OpenAudioDeviceStream(
            SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &format, nullptr, nullptr);
        if (!deuteros_audio_stream) {
            std::cerr << "Unable to open Deuteros audio output: " << SDL_GetError() << '\n';
            return;
        }
        if (!SDL_ResumeAudioStreamDevice(deuteros_audio_stream)) {
            std::cerr << "Unable to start Deuteros audio output: " << SDL_GetError() << '\n';
            SDL_DestroyAudioStream(deuteros_audio_stream);
            deuteros_audio_stream = nullptr;
        }
    };
    const auto discard_deuteros_external_modern_sequence = [&] {
        if (deuteros_external_modern_texture) SDL_DestroyTexture(deuteros_external_modern_texture);
        deuteros_external_modern_texture = nullptr;
        deuteros_external_modern_resolver.reset();
        deuteros_external_modern_surface.reset();
        deuteros_external_modern_source_tick.reset();
        deuteros_external_modern_attempted = false;
    };
    const auto load_deuteros_external_modern_sequence = [&] {
        if (deuteros_external_modern_attempted) return;
        deuteros_external_modern_attempted = true;
        if (runtime.state() != eon::NativeSessionState::deuteros_amiga_opening
            || !selected_modern_pack_manifest
            || request.presentation != eon::Presentation::modern
            || active_platform != eon::Platform::amiga || !active_release_language
            || !active_release_sha256
            || *active_release_language != "en") return;
        const auto release = resolve_active_release(eon::Game::deuteros);
        if (!release) return;
        try {
            deuteros_external_modern_resolver = eon::ModernAssetPackPresentationResolver::create(
                *selected_modern_pack_manifest,
                eon::ModernAssetPackPresentationTarget::deuteros_amiga_held_opening,
                eon::Game::deuteros, eon::Platform::amiga, release->sha256);
        } catch (const std::exception& error) {
            std::cerr << "Modern Deuteros opening pack not used: " << error.what() << '\n';
            deuteros_external_modern_resolver.reset();
        }
    };
    const auto refresh_deuteros_external_modern_texture = [&](
        const std::uint64_t source_tick, const bool title_handed_off) -> SDL_Texture* {
        if (!deuteros_external_modern_resolver || source_tick == 0
            || source_tick > eon::deuteros_amiga_held_opening_frame_count
            // Tick 82 is an external rendering target only when the original
            // VM actually took its held-input handoff. Without it, the opening
            // continues into a route that this finite art sequence does not claim.
            || (source_tick == eon::deuteros_amiga_held_opening_frame_count && !title_handed_off)) {
            return nullptr;
        }
        if (deuteros_external_modern_texture && deuteros_external_modern_source_tick
            && *deuteros_external_modern_source_tick == source_tick) {
            return deuteros_external_modern_texture;
        }
        try {
            auto surface = deuteros_external_modern_resolver->resolve(source_tick, title_handed_off);
            SDL_IOStream* stream = SDL_IOFromConstMem(surface.png.data(), surface.png.size());
            if (!stream) throw std::runtime_error("Unable to open Modern Deuteros PNG bytes: "
                + std::string(SDL_GetError()));
            SDL_Surface* image = IMG_Load_IO(stream, true);
            if (!image) throw std::runtime_error("Unable to decode Modern Deuteros PNG: "
                + std::string(SDL_GetError()));
            if (image->w != static_cast<int>(surface.width) || image->h != static_cast<int>(surface.height)) {
                SDL_DestroySurface(image);
                throw std::runtime_error("Modern Deuteros PNG dimensions changed during decode");
            }
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, image);
            SDL_DestroySurface(image);
            if (!texture) throw std::runtime_error("Unable to upload Modern Deuteros PNG: "
                + std::string(SDL_GetError()));
            if (deuteros_external_modern_texture) SDL_DestroyTexture(deuteros_external_modern_texture);
            deuteros_external_modern_texture = texture;
            deuteros_external_modern_surface = std::move(surface);
            deuteros_external_modern_source_tick = source_tick;
            return texture;
        } catch (const std::exception& error) {
            // A changed late asset revokes the complete sequence for the rest
            // of this session. Do not select a lesser tier or another pack.
            std::cerr << "Modern Deuteros opening pack disabled: " << error.what() << '\n';
            if (deuteros_external_modern_texture) SDL_DestroyTexture(deuteros_external_modern_texture);
            deuteros_external_modern_texture = nullptr;
            deuteros_external_modern_resolver.reset();
            deuteros_external_modern_surface.reset();
            deuteros_external_modern_source_tick.reset();
            return nullptr;
        }
    };
    SDL_Texture* millennium_preview_texture = nullptr;
    SDL_Texture* millennium_modern_preview_texture = nullptr;
    eon::ModernPresentationPipeline millennium_modern_pipeline;
    SDL_Texture* millennium_external_modern_texture = nullptr;
    std::optional<eon::ModernAssetPackPngSurface> millennium_external_modern_surface;
    std::optional<eon::ModernAssetPackPresentationResolver> millennium_external_modern_resolver;
    bool millennium_external_modern_attempted = false;
    SDL_Texture* millennium_gx_canvas_texture = nullptr;
    const auto discard_millennium_assets = [&] {
        if (millennium_preview_texture) SDL_DestroyTexture(millennium_preview_texture);
        if (millennium_modern_preview_texture) SDL_DestroyTexture(millennium_modern_preview_texture);
        if (millennium_external_modern_texture) SDL_DestroyTexture(millennium_external_modern_texture);
        if (millennium_gx_canvas_texture) SDL_DestroyTexture(millennium_gx_canvas_texture);
        millennium_preview_texture = nullptr;
        millennium_modern_preview_texture = nullptr;
        millennium_modern_pipeline.reset();
        millennium_external_modern_texture = nullptr;
        millennium_external_modern_surface.reset();
        millennium_external_modern_resolver.reset();
        millennium_external_modern_attempted = false;
        millennium_gx_canvas_texture = nullptr;
        millennium_assets.reset();
    };
    const auto create_millennium_textures = [&] {
        if (!millennium_assets || millennium_preview_texture) return;
        millennium_preview_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STATIC, millennium_assets->assets.title.width,
            millennium_assets->assets.title.height);
        if (millennium_preview_texture) {
            SDL_UpdateTexture(millennium_preview_texture, nullptr,
                millennium_assets->assets.title.rgba_frames.front().data(),
                millennium_assets->assets.title.width * 4);
        }
        if (millennium_assets->assets.gx_canvas) {
            millennium_gx_canvas_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                SDL_TEXTUREACCESS_STATIC, millennium_assets->assets.gx_canvas->width,
                millennium_assets->assets.gx_canvas->height);
            if (millennium_gx_canvas_texture) {
                SDL_UpdateTexture(millennium_gx_canvas_texture, nullptr,
                    millennium_assets->assets.gx_canvas->rgba_frames.front().data(),
                    millennium_assets->assets.gx_canvas->width * 4);
            }
        }
    };
    const auto millennium_texture_for = [&](const PixelReconstruction reconstruction) {
        if (millennium_external_modern_texture) return millennium_external_modern_texture;
        if (!millennium_assets || !millennium_preview_texture || reconstruction == PixelReconstruction::off) {
            return millennium_preview_texture;
        }
        const auto release = resolve_active_release(eon::Game::millennium);
        if (!release) return millennium_preview_texture;
        const eon::ModernReconstructionCacheKey requested_key{
            release->sha256, "millennium.dos.title", 0, reconstruction};
        if (millennium_modern_preview_texture && !millennium_modern_pipeline.matches(requested_key)) {
            SDL_DestroyTexture(millennium_modern_preview_texture);
            millennium_modern_preview_texture = nullptr;
        }
        const auto& title = millennium_assets->assets.title;
        if (title.rgba_frames.empty()) return millennium_preview_texture;
        const auto* enhanced = millennium_modern_pipeline.resolve(requested_key,
            title.rgba_frames.front(), title.width, title.height);
        if (enhanced && !millennium_modern_preview_texture) {
            millennium_modern_preview_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                SDL_TEXTUREACCESS_STATIC, enhanced->width, enhanced->height);
            if (!millennium_modern_preview_texture || !SDL_UpdateTexture(
                    millennium_modern_preview_texture, nullptr, enhanced->rgba.data(), enhanced->width * 4)) {
                std::cerr << "Unable to upload transient Modern Millennium texture: " << SDL_GetError() << '\n';
                SDL_DestroyTexture(millennium_modern_preview_texture);
                millennium_modern_preview_texture = nullptr;
            }
        }
        if (!enhanced && millennium_modern_pipeline.failure()) {
            std::cerr << "Modern Millennium reconstruction rejected: "
                      << *millennium_modern_pipeline.failure() << '\n';
        }
        return millennium_modern_preview_texture ? millennium_modern_preview_texture
                                                 : millennium_preview_texture;
    };
    create_millennium_textures();
    // Menu scanning is deliberately incremental.  Do not lock Millennium's
    // verified DOS resources to the empty pre-scan release list: load them
    // only when the scanner has actually found the selected original media.
    const auto load_millennium_assets_if_available = [&] {
        if (!millennium_assets) {
            millennium_assets = runtime.millennium_dos_presentation();
            create_millennium_textures();
        }
        if (!millennium_assets || millennium_assets->assets.language != "en"
            || millennium_external_modern_attempted || !selected_modern_pack_manifest
            || request.presentation != eon::Presentation::modern
            || active_platform != eon::Platform::dos) return;
        millennium_external_modern_attempted = true;
        const auto release = resolve_active_release(eon::Game::millennium);
        if (!release) return;
        try {
            millennium_external_modern_resolver = eon::ModernAssetPackPresentationResolver::create(
                *selected_modern_pack_manifest,
                eon::ModernAssetPackPresentationTarget::millennium_dos_title,
                eon::Game::millennium, eon::Platform::dos, release->sha256);
            millennium_external_modern_surface = millennium_external_modern_resolver->resolve(0);
            const auto& surface = *millennium_external_modern_surface;
            SDL_IOStream* stream = SDL_IOFromConstMem(surface.png.data(), surface.png.size());
            if (!stream) throw std::runtime_error("Unable to open Modern title PNG bytes: " + std::string(SDL_GetError()));
            SDL_Surface* image = IMG_Load_IO(stream, true);
            if (!image) throw std::runtime_error("Unable to decode Modern title PNG: " + std::string(SDL_GetError()));
            if (image->w != static_cast<int>(surface.width) || image->h != static_cast<int>(surface.height)) {
                SDL_DestroySurface(image);
                throw std::runtime_error("Modern title PNG dimensions changed during decode");
            }
            millennium_external_modern_texture = SDL_CreateTextureFromSurface(renderer, image);
            SDL_DestroySurface(image);
            if (!millennium_external_modern_texture) {
                throw std::runtime_error("Unable to upload Modern title PNG: " + std::string(SDL_GetError()));
            }
        } catch (const std::exception& error) {
            millennium_external_modern_surface.reset();
            millennium_external_modern_resolver.reset();
            std::cerr << "Modern title pack not used: " << error.what() << '\n';
        }
    };

    Screen screen = request.game ? Screen::launching : Screen::menu;
    auto& launcher_page = launcher_route.page;
    eon::Game selected = request.game.value_or(eon::Game::millennium);
    auto& card_focus = launcher_interaction.focus;
    auto& focused = card_focus.game;
    auto& focused_platform_card = card_focus.platform;
    auto& focused_release_card = card_focus.release;
    auto& focused_profile_card = card_focus.profile;
    auto& custom_profile_ready = launcher_session.custom_profile_ready;
    bool show_scanner = false;
    const auto scanner_source_text = [&](const eon::OriginalDataSourceKind source_kind) {
        switch (source_kind) {
        case eon::OriginalDataSourceKind::directory: return tr("DATA SOURCE: DIRECTORY");
        case eon::OriginalDataSourceKind::archive: return tr("DATA SOURCE: ARCHIVE");
        case eon::OriginalDataSourceKind::missing: return tr("DATA SOURCE: MISSING");
        case eon::OriginalDataSourceKind::unsupported: return tr("DATA SOURCE: UNSUPPORTED");
        }
        return tr("DATA SOURCE: UNSUPPORTED");
    };
    const auto scanner_rejections_text = [&](const eon::ReleaseScanSnapshot& snapshot) {
        auto text = tr("REJECTIONS: SIZE {size}; HASH {hash}; UNREADABLE {unreadable}; LINKS {links}");
        const auto replace = [&](const std::string_view token, const std::size_t value) {
            const auto position = text.find(token);
            if (position != std::string::npos) text.replace(position, token.size(), std::to_string(value));
        };
        replace("{size}", snapshot.report.size_rejected_candidates);
        replace("{hash}", snapshot.report.hash_rejected_candidates);
        replace("{unreadable}", snapshot.report.unreadable_candidates);
        replace("{links}", snapshot.report.symlink_rejected_entries);
        return text;
    };
    const auto scanner_admission_text = [&](const eon::ReleaseScanSnapshot& snapshot) {
        // This is the same aggregate-only recognition result that the CLI
        // inspection route reports. It deliberately exposes neither a
        // candidate filename nor a source path, so a rejected archive never
        // turns into a UI-visible alternate-media catalogue.
        auto text = tr("VERIFIED RELEASES: {unique}; DUPLICATES: {duplicates}");
        const auto replace = [&](const std::string_view token, const std::size_t value) {
            const auto position = text.find(token);
            if (position != std::string::npos) text.replace(position, token.size(), std::to_string(value));
        };
        replace("{unique}", snapshot.unique_release_count);
        replace("{duplicates}", snapshot.report.duplicate_occurrences);
        return text;
    };
    // It intentionally has room for the longest shipped translation, rather
    // than treating English text width as the launcher layout contract.
    const SDL_FRect data_directory_picker_bounds{640.0F, 16.0F, 398.0F, 34.0F};
    const SDL_FRect data_archive_picker_bounds{640.0F, 56.0F, 398.0F, 34.0F};
    const SDL_FRect launcher_language_bounds{230.0F, 16.0F, 170.0F, 34.0F};
    // These are Eon-shell controls, rendered in the same logical coordinate
    // space as cards. They contain no release identity and therefore cannot
    // select or launch original media by themselves.
    const SDL_FRect launcher_back_bounds{64.0F, 16.0F, 152.0F, 34.0F};
    const SDL_FRect release_page_previous_bounds{320.0F, 82.0F, 108.0F, 30.0F};
    const SDL_FRect release_page_next_bounds{438.0F, 82.0F, 108.0F, 30.0F};
    bool show_modern_graphics_settings = false;
    bool show_modern_runtime_diagnostics = false;
    bool show_recovery_function_map = false;
    std::size_t recovery_function_map_page = 0;
    const auto clear_deuteros_opening_input = [&] {
        // The recovered `$14` path receives only a held host signal. A
        // launcher-modal transition is not an original input poll, so it
        // must cancel any prior host hold rather than letting it leak behind
        // the F10 renderer settings dialog or into a fresh opening session.
        runtime.set_input_suppressed(true);
    };
    std::optional<std::uint32_t> deuteros_title_resource;
    std::optional<eon::MillenniumDosStartupInputSnapshot> millennium_startup_input;
    // SDL text input is the host analogue of DOS' character-availability
    // poll. Keep it active only while TITLES.EXE's recovered title boundary
    // is live; raw key presses alone do not prove a nonzero DOS AL result.
    bool millennium_title_text_input_active = false;
    // This evidence-only object is intentionally never constructed by the
    // launcher until a genuine DOS return/startup path is recovered.
    std::unique_ptr<eon::MillenniumDosGameSession> millennium_game_session;
    std::size_t millennium_state_page = 0;
    const auto platform_cards_for_game = [&](const eon::Game game) {
        std::vector<PlatformCard> cards_for_game;
        const auto supported = eon::supported_platforms(game);
        cards_for_game.reserve(supported.size());
        const float width = supported.size() == 2 ? 552.0F : 352.0F;
        const float first_x = 64.0F;
        const float stride = supported.size() == 2 ? 600.0F : 400.0F;
        for (std::size_t index = 0; index < supported.size(); ++index) {
            const auto found = std::find_if(platform_card_templates.begin(),
                platform_card_templates.end(), [&](const PlatformCard& candidate) {
                    return candidate.platform == supported[index];
                });
            if (found == platform_card_templates.end()) continue;
            auto card = *found;
            card.bounds = {first_x + static_cast<float>(index) * stride, 188, width, 308};
            cards_for_game.push_back(card);
        }
        return cards_for_game;
    };
    // Keep keyboard/gamepad focus on the same card as the automatic
    // hash-verified choice. The card collection itself is game-specific, so
    // an unsupported platform is never misrepresented as absent user media.
    const auto focus_active_platform_card = [&] {
        if (!active_platform) return;
        const auto platform_cards = platform_cards_for_game(
            cards[static_cast<std::size_t>(focused)].game);
        const auto card = std::find_if(platform_cards.begin(), platform_cards.end(),
            [&](const PlatformCard& candidate) { return candidate.platform == *active_platform; });
        if (card != platform_cards.end()) {
            card_focus.set(eon::LauncherPage::platforms, platform_cards.size(),
                static_cast<std::size_t>(std::distance(platform_cards.begin(), card)));
        }
    };
    struct ReleaseLanguageCardPage {
        std::vector<ReleaseLanguageCard> cards;
        std::size_t page = 0;
        std::size_t page_count = 0;
    };
    const auto release_language_cards = [&] {
        ReleaseLanguageCardPage result;
        std::vector<ReleaseLanguageCard> cards_for_platform;
        if (!active_platform) return result;
        const auto identities = eon::available_release_identities(releases,
            cards[static_cast<std::size_t>(focused)].game, *active_platform);
        if (identities.empty()) return result;
        // The controller focus is bounded whenever scanner data changes, but
        // keep this renderer-only view defensive so a scanner update between
        // event and draw cannot turn a stale card position into an out of
        // range page calculation.
        const auto page = eon::release_card_page_for_focus(identities.size(), focused_release_card);
        result.page = page.page;
        result.page_count = page.page_count;
        const auto artwork = std::find_if(platform_card_templates.begin(),
            platform_card_templates.end(), [&](const PlatformCard& card) {
                return card.platform == *active_platform;
            });
        for (std::size_t visible_index = 0; visible_index < page.visible_count; ++visible_index) {
            const auto identity_index = page.first_identity + visible_index;
            SDL_FRect bounds;
            if (page.visible_count == 1) {
                bounds = {364.0F, 188.0F, 552.0F, 308.0F};
            } else if (page.visible_count == 2) {
                bounds = {64.0F + static_cast<float>(visible_index) * 600.0F,
                    188.0F, 552.0F, 308.0F};
            } else if (page.visible_count == 3) {
                bounds = {64.0F + static_cast<float>(visible_index) * 400.0F,
                    188.0F, 352.0F, 308.0F};
            } else {
                // Four identities use a two-by-two grid. This leaves the
                // header and back hint visible in the 1280x720 logical
                // presentation, unlike a fourth horizontal card beyond the
                // viewport. More identities page by the focused card.
                bounds = {64.0F + static_cast<float>(visible_index % 2U) * 600.0F,
                    132.0F + static_cast<float>(visible_index / 2U) * 286.0F,
                    552.0F, 254.0F};
            }
            cards_for_platform.push_back({identity_index, identities[identity_index].language,
                identities[identity_index].sha256, bounds,
                artwork == platform_card_templates.end() ? nullptr : artwork->texture});
        }
        result.cards = std::move(cards_for_platform);
        return result;
    };
    const auto focus_menu_card = [&](const std::size_t next_focus) {
        card_focus.set(eon::LauncherPage::games, cards.size(), next_focus);
        if (request.platform) return;
        // A release-bound pack cannot follow a game selection.
        clear_modern_pack_admission();
        const auto previous_platform = active_platform;
        launcher_interaction.synchronize(releases);
        if (active_platform != previous_platform) {
            discard_millennium_assets();
        }
        if (!active_platform) card_focus.reset_after_game_change();
        // This is also needed when the active platform happens not to change:
        // the game card can have been focused while scanning was incomplete.
        focus_active_platform_card();
    };
    const auto stop_millennium_title = [&] {
        // This is host lifecycle cleanup only. It neither sends an additional
        // DOS availability result nor modifies original title/game data.
        if (millennium_title_text_input_active) SDL_StopTextInput(window);
        millennium_title_text_input_active = false;
        millennium_startup_input.reset();
    };
    const auto reset_deuteros_runtime = [&] {
        // A replacement scanner has no authority to retain decoded frames,
        // VM state, queued audio, or an external Modern sequence from the
        // preceding user-supplied release.  Keep the SDL audio device itself
        // open, but flush it before discarding the source-bound mixer.
        clear_deuteros_opening_input();
        if (deuteros_audio_stream) {
            static_cast<void>(SDL_ClearAudioStream(deuteros_audio_stream));
        }
        deuteros_title_resource.reset();
        deuteros_preview_rgba.reset();
        deuteros_preview_source_tick.reset();
        deuteros_modern_pipeline.reset();
        if (preview_texture) SDL_DestroyTexture(preview_texture);
        if (modern_preview_texture) SDL_DestroyTexture(modern_preview_texture);
        preview_texture = nullptr;
        modern_preview_texture = nullptr;
        discard_deuteros_external_modern_sequence();
    };
    const auto reset_active_runtime = [&] {
        // Leaving a launch or changing its source is a hard preservation
        // boundary. Nothing derived from the former exact archive may remain
        // addressable while the launcher is visible or a new bounded scan is
        // incomplete. The release card itself remains a selection only; a
        // later launch must reacquire and rehash it from scratch.
        stop_millennium_title();
        millennium_game_session.reset();
        millennium_state_page = 0;
        discard_millennium_assets();
        reset_deuteros_runtime();
        // SDL-side resources and scheduler state are revoked before the
        // controller discards its coordinator-owned native session.
        runtime.begin_source_revocation();
        runtime.finish_source_revocation();
        launcher_runtime_admission = std::string(
            eon::release_runtime_admission_label(runtime.admission()));
        launcher_runtime_rejection = std::string(
            eon::release_runtime_rejection_label(runtime.rejection()));
    };
    const auto start_millennium_title = [&] {
        // A title session is intentionally one-shot after its observed
        // character-availability hand-off. Returning from the launcher or
        // selecting the title again starts a fresh original boundary, and
        // must not retain an IME/virtual keyboard from the previous visit.
        stop_millennium_title();
        if (!resolve_active_release(eon::Game::millennium)) return;
        if (active_platform == eon::Platform::atari_st || active_platform == eon::Platform::amiga) return;
        load_millennium_assets_if_available();
        // The coordinator constructed these exact, parser-validated input
        // boundaries during rehash admission. SDL receives a value snapshot
        // only; it cannot synthesize a title/input state or retain an adapter
        // borrow after reset.
        millennium_startup_input = runtime.millennium_dos_startup_input();
        if (millennium_startup_input && millennium_startup_input->title_active) millennium_state_page = 0;
        if (millennium_startup_input && (millennium_startup_input->sound_selection_active
                || millennium_startup_input->title_active)
            && !millennium_title_text_input_active) {
            if (!SDL_StartTextInput(window)) {
                std::cerr << "Unable to enable Millennium DOS title text input: "
                          << SDL_GetError() << '\n';
            } else {
                millennium_title_text_input_active = true;
            }
        }
    };
    const auto start_deuteros = [&] {
        stop_millennium_title();
        reset_deuteros_runtime();
        if (!resolve_active_release(eon::Game::deuteros)) return;
        create_deuteros_opening_texture();
        start_deuteros_audio();
        load_deuteros_external_modern_sequence();
        static_cast<void>(runtime.advance(SDL_GetTicks()));
        deuteros_title_resource.reset();
    };
    const auto launch_menu_selection = [&] {
        // The coordinator acquires only a fresh, exact release, but the SDL
        // layer also owns source-derived textures, audio queues and title
        // input state. Revoke all of those before every menu launch, even if
        // the preceding route returned normally, so an attempted re-launch
        // can never present a frame or queued sample from another release.
        reset_active_runtime();
        const auto menu_launch = runtime.launch_menu(launcher_session, request, releases);
        launcher_runtime_admission = std::string(
            eon::release_runtime_admission_label(menu_launch.admission));
        launcher_runtime_rejection = std::string(
            eon::release_runtime_rejection_label(menu_launch.rejection));
        if (!menu_launch.accepted()) return;
        // The chosen release card is part of the Modern-pack identity.  A
        // previously valid candidate becomes rejected rather than silently
        // following a different language/container/platform selection. Read
        // the one identity already resolved and rehashed by the common gate;
        // do not independently resolve card fields a second time here.
        if (menu_launch.active_launch->request.presentation == eon::Presentation::modern
            && selected_modern_pack_manifest) {
            admit_modern_pack_for_release(*selected_modern_pack_manifest, active_launch()->release);
        }
        millennium_assets = runtime.millennium_dos_presentation();
        active_release_sha256 = active_launch()->request.release_sha256;
        active_release_language = active_launch()->request.release_language;
        selected = launcher_route.game;
        screen = Screen::launching;
        if (selected == eon::Game::millennium) start_millennium_title();
        if (selected == eon::Game::deuteros) start_deuteros();
    };
    const auto apply_launcher_navigation = [&](const eon::LauncherSourceIdentity& before) {
        if (!launcher_interaction.source_changed_since(before)) return;
        // A menu route may be driven by keyboard, gamepad, mouse, or touch.
        // All four must revoke the prior admitted adapter before a different
        // game/platform/language/hash can be presented or launched.
        clear_modern_pack_admission();
        // A rejection belongs to one attempted immutable route. Once a card
        // changes that route, do not show its cause for the next game or
        // release—even when there was no active adapter left to revoke.
        launcher_runtime_admission = "NOT SELECTED";
        launcher_runtime_rejection = "NONE";
        if (runtime.requires_revocation_for(launcher_interaction.source_identity())) {
            reset_active_runtime();
        }
    };
    const auto activate_launcher_card = [&](const std::optional<std::size_t> card) {
        const auto before = launcher_interaction.source_identity();
        const auto effect = card
            ? launcher_interaction.activate_card(releases, *card)
            : launcher_interaction.activate(releases);
        request.presentation = launcher_session.presentation;
        apply_launcher_navigation(before);
        if (effect == eon::LauncherInteractionEffect::open_custom_settings) {
            // Custom is a deliberate renderer-only configuration route. It
            // cannot become a third runtime identity or a launch shortcut.
            show_modern_graphics_settings = true;
            runtime.set_input_suppressed(true);
        } else if (effect == eon::LauncherInteractionEffect::launch) {
            launch_menu_selection();
        }
    };
    const auto move_launcher_cards = [&](const int direction) {
        const auto before = launcher_interaction.source_identity();
        launcher_interaction.move(releases, direction);
        apply_launcher_navigation(before);
        if (launcher_page == eon::LauncherPage::games) {
            if (!active_platform) card_focus.reset_after_game_change();
            focus_active_platform_card();
        }
    };
    const auto edge_launcher_cards = [&](const bool first) {
        const auto before = launcher_interaction.source_identity();
        if (first) launcher_interaction.first(releases);
        else launcher_interaction.last(releases);
        apply_launcher_navigation(before);
        if (launcher_page == eon::LauncherPage::games) focus_active_platform_card();
    };
    const auto back_launcher_cards = [&] {
        const auto before = launcher_interaction.source_identity();
        launcher_interaction.back(releases);
        apply_launcher_navigation(before);
        if (launcher_page == eon::LauncherPage::games) focus_active_platform_card();
    };
    const auto page_release_cards = [&](const int direction) {
        const auto before = launcher_interaction.source_identity();
        if (!launcher_interaction.page_releases(releases, direction)) return;
        // Paging is presentation-only, but retain the shared source-change
        // boundary so future controller changes cannot bypass lifecycle
        // revocation by adding identity state to a page action.
        apply_launcher_navigation(before);
    };
    const auto open_original_data_source_dialog = [&](const OriginalDataSourceDialogKind kind) {
        if (screen != Screen::menu) return;
        auto& mailbox = original_data_source_dialog_mailbox();
        {
            std::lock_guard lock(mailbox.mutex);
            if (mailbox.dialog_open) return;
            mailbox.dialog_open = true;
            mailbox.requested_kind = kind;
        }
        // Do not provide the conventional data directory as an initial
        // location: a missing default remains an inert lookup and is never
        // created or persisted merely because the launcher is shown.
        if (kind == OriginalDataSourceDialogKind::directory) {
            SDL_ShowOpenFolderDialog(receive_original_data_source_dialog_selection, &mailbox,
                window, nullptr, false);
        } else {
            SDL_ShowOpenFileDialog(receive_original_data_source_dialog_selection, &mailbox,
                window, nullptr, 0, nullptr, false);
        }
    };
    const auto cycle_launcher_language = [&](const int direction) {
        const auto& languages = eon::supported_launcher_languages();
        const auto current = eon::canonical_launcher_language(request.language);
        const auto found = std::find(languages.begin(), languages.end(), current);
        const auto index = found == languages.end() ? std::size_t{0}
            : static_cast<std::size_t>(std::distance(languages.begin(), found));
        const auto next = direction < 0 ? (index + languages.size() - 1U) % languages.size()
            : (index + 1U) % languages.size();
        request.language = std::string(languages[next]);
        translator = eon::Translator::from_language(request.language,
            argc > 0 ? std::filesystem::path(argv[0]) : std::filesystem::path{});
        // This changes Eon's chrome only. `release_language` remains the
        // selected original archive identity and never follows UI locale.
        active_translator = &translator;
        if (!eon::save_launcher_language_preference(presentation_preferences_path,
                request.language)) {
            std::cerr << "Unable to save launcher language preference: "
                      << presentation_preferences_path << '\n';
        }
    };
    const auto handle_menu_pointer_down = [&](const float x, const float y) {
        // SDL mouse and touch input share one card route. The latter is
        // needed by the iPad build; both still pass through the same
        // hash-verified platform/release admission checks as keyboard focus.
        if (inside(launcher_language_bounds, x, y)) {
            cycle_launcher_language(x < launcher_language_bounds.x
                    + launcher_language_bounds.w / 2.0F
                ? -1
                : 1);
        } else if (launcher_page != LauncherPage::games && inside(launcher_back_bounds, x, y)) {
            back_launcher_cards();
        } else if (launcher_page == LauncherPage::releases
            && inside(release_page_previous_bounds, x, y)) {
            page_release_cards(-1);
        } else if (launcher_page == LauncherPage::releases
            && inside(release_page_next_bounds, x, y)) {
            page_release_cards(1);
        } else if (inside(data_directory_picker_bounds, x, y)) {
            open_original_data_source_dialog(OriginalDataSourceDialogKind::directory);
        } else if (inside(data_archive_picker_bounds, x, y)) {
            open_original_data_source_dialog(OriginalDataSourceDialogKind::archive);
        } else if (launcher_page == LauncherPage::games) {
            for (std::size_t index = 0; index < cards.size(); ++index) {
                if (inside(cards[index].bounds, x, y)) {
                    activate_launcher_card(index);
                }
            }
        } else if (launcher_page == LauncherPage::platforms) {
            const auto platform_cards = platform_cards_for_game(
                cards[static_cast<std::size_t>(focused)].game);
            for (std::size_t index = 0; index < platform_cards.size(); ++index) {
                if (inside(platform_cards[index].bounds, x, y)) {
                    activate_launcher_card(index);
                }
            }
        } else if (launcher_page == LauncherPage::releases) {
            const auto release_page = release_language_cards();
            for (const auto& card : release_page.cards) {
                if (inside(card.bounds, x, y)) activate_launcher_card(card.identity_index);
            }
        } else {
            for (std::size_t index = 0; index < profile_cards.size(); ++index) {
                if (!inside(profile_cards[index].bounds, x, y)) continue;
                activate_launcher_card(index);
            }
        }
    };
    if (screen == Screen::launching && selected == eon::Game::millennium) {
        start_millennium_title();
    }
    if (screen == Screen::launching && selected == eon::Game::deuteros) start_deuteros();
    ModernGraphicsSettings modern_graphics_settings;
    modern_graphics_settings.output_resolution_index = output_resolution_index_for(request.display);
    modern_graphics_settings.aspect_ratio_index = request.display.aspect_ratio_index;
    if (saved_presentation_preferences) {
        const auto& saved = *saved_presentation_preferences;
        if (!request.display_resolution_explicit) {
            modern_graphics_settings.output_resolution_index = saved.output_resolution_index;
        }
        if (!request.display_aspect_explicit) {
            modern_graphics_settings.aspect_ratio_index = saved.aspect_ratio_index;
        }
        apply_modern_graphics_preset(modern_graphics_settings,
            static_cast<ModernGraphicsPreset>(saved.modern_preset_index));
        modern_graphics_settings.render_pacing = static_cast<RenderPacing>(saved.render_pacing_index);
        modern_graphics_settings.pixel_reconstruction = static_cast<PixelReconstruction>(saved.pixel_reconstruction_index);
        modern_graphics_settings.smooth_scaling = saved.smooth_scaling;
        modern_graphics_settings.scanlines = saved.scanlines;
        modern_graphics_settings.frame = saved.frame;
        modern_graphics_settings.reduced_motion = saved.reduced_motion;
    }
    const auto current_modern_runtime_diagnostics = [&] {
        ModernRuntimeDiagnostics diagnostics;
        const auto runtime_view = runtime.snapshot();
        diagnostics.release_identity = tr("NOT SELECTED");
        diagnostics.runtime_admission = std::string(eon::release_runtime_admission_label(
            runtime_view.admission));
        diagnostics.runtime_rejection = std::string(eon::release_runtime_rejection_label(
            runtime_view.rejection));
        diagnostics.lifecycle_state = std::string(eon::native_session_state_label(runtime_view.state))
            + " / GEN=" + std::to_string(runtime_view.generation)
            + " / REVOKING=" + (runtime_view.revoking ? "Y" : "N")
            + " / INPUT=" + (runtime_view.input_suppressed ? "SUPPRESSED" : "ACTIVE");
        // This is a compact, renderer-only diagnostic code. It makes the
        // selected preservation contract visible even before a session is
        // admitted, while the separately reported capabilities remain facts
        // about the recovered session rather than a claim about the mode.
        diagnostics.session_capabilities = std::string("MODE=")
            + (request.presentation == eon::Presentation::original ? "ORIGINAL" : "MODERN");
        if (const auto& session = runtime_view.presentation) {
            diagnostics.session_adapter = std::string(eon::runtime_presentation_kind_label(session->kind));
            diagnostics.session_boundary = std::string(eon::runtime_session_boundary_label(session->boundary));
            // Capability values are compact diagnostic codes, like recovery
            // map addresses. Only their launcher row label is translated.
            diagnostics.session_capabilities += " / DECODED_PRESENTATION="
                + std::string(session->capabilities.decoded_presentation ? "Y" : "N")
                + " / AUDIO=" + (session->capabilities.audio_observations ? "Y" : "N")
                + " / INPUT=" + std::string(
                    eon::runtime_input_contract_identifier(session->input_contract));
        }
        if (const auto dispatch = runtime.millennium_dos_static_dispatch_diagnostics()) {
            std::ostringstream summary;
            summary << "F1-F10 TABLE=$" << std::hex << dispatch->table_address
                    << " STRIDE=" << std::dec << dispatch->table_stride
                    << " DISPATCH=$" << std::hex << dispatch->dispatch_address
                    << " (STATIC ONLY)";
            diagnostics.millennium_dos_static_dispatch = summary.str();
        }
        if (const auto owned = runtime.millennium_dos_owned_function_diagnostics()) {
            std::ostringstream summary;
            summary << owned->function_id << " INDEX=" << owned->function_key_index
                << " HANDLER=$" << std::hex << owned->handler_address
                << " BOUNDARY=$" << owned->boundary.instruction_address
                << " " << owned->mode;
            diagnostics.millennium_dos_owned_function = summary.str();
        }
        if (const auto chain = runtime.deuteros_amiga_title_dependency_chain_checkpoint()) {
            std::ostringstream summary;
            summary << "DEUTEROS TITLE STOP=$" << std::hex << chain->stop_before_address
                << " CUSTOM=" << std::dec << chain->observed_custom_chip_write_count << "/4"
                << " CALLBACK=" << (chain->callback_exec_return_observed ? "Y" : "N")
                << " S1=" << (chain->service_setup_local_plan ? "Y" : "N")
                << " S2=" << (chain->second_service_local_plan ? "Y" : "N")
                << " S3=" << (chain->third_service_local_plan ? "Y" : "N")
                << " S4=" << (chain->fourth_service_local_plan ? "Y" : "N")
                << " S5=" << (chain->fifth_service_local_plan ? "Y" : "N");
            diagnostics.deuteros_amiga_title_dependency_chain = summary.str();
        }
        {
            const auto images = runtime.native_code_image_registry_diagnostics();
            std::ostringstream summary;
            summary << tr("CODE IMAGES") << '=' << images.mapped_descriptor_count
                << " / " << tr("EXCLUDED") << '=' << images.excluded_image_count;
            if (images.active) {
                summary << " / " << tr("ACTIVE") << '=' << images.active->image_id
                    << ':' << images.active->range_id
                    << " / " << eon::native_code_address_basis_label(images.active->address_basis)
                    << " / " << eon::native_code_load_status_label(images.active->load_status);
            } else {
                summary << " / " << tr("ACTIVE") << '=' << tr("NONE");
            }
            diagnostics.native_code_images = summary.str();
        }
        diagnostics.modern_pack = tr(modern_pack_admission == ModernPackAdmission::ready ? "READY"
            : modern_pack_admission == ModernPackAdmission::rejected ? "REJECTED" : "NOT SELECTED");
        if (modern_pack_admission == ModernPackAdmission::ready && selected_modern_pack_preflight
            && selected_modern_pack_preflight->accepted) {
            diagnostics.modern_pack += " / " + truncated_diagnostic_value(selected_modern_pack_preflight->pack_id)
                + " / " + truncated_diagnostic_value(selected_modern_pack_preflight->provenance);
            diagnostics.modern_pack_targets = modern_pack_renderer_targets_summary(
                selected_modern_pack_preflight->targets);
        }
        // In the menu, show the currently focused game/platform/release
        // choice; after launch, show the fixed session selection. This is a
        // live UI readout, not a second admission path.
        const auto game = screen == Screen::launching ? selected
            : cards.at(static_cast<std::size_t>(focused)).game;
        const auto release = resolve_active_release(game);
        if (!release) return diagnostics;
        const auto report = eon::runtime_diagnostics_for_release(*release);
        diagnostics.release_identity = tr(launcher_game_label(release->game)) + " / "
            + tr(launcher_platform_label(release->platform)) + " / " + release->language
            + " / " + truncated_identity_hash(release->sha256);
        diagnostics.recovery_coverage = std::string(eon::name(report.coverage));
        if (const auto& boundary = report.startup_boundary) {
            diagnostics.startup_boundary = boundary->parser_profile_id
                + " / " + std::string(boundary->source_address);
        }
        diagnostics.recovery_boundary_count = report.recovery_boundaries.size();
        diagnostics.recovery_functions.reserve(report.functions.size());
        for (const auto& entry : report.functions) {
            diagnostics.recovery_functions.push_back({
                entry.id, entry.parser_profile_id, entry.cpu, entry.source_asset_sha256,
                entry.source_span_sha256,
                entry.source_offset, entry.runtime_address, entry.address_space, entry.evidence_level,
                entry.uncertainty, entry.runtime_status,
            });
        }
        // GUI launches intentionally do not accept a trace path. The CLI
        // verifier exits after a complete, hash-locked validation, so no
        // unvalidated trace can appear admitted here.
        diagnostics.trace_admission = "NOT LOADED";
        return diagnostics;
    };
    const auto open_modern_pack_dialog = [&] {
        // Changing an art candidate is allowed only before Custom launches a
        // session. Once a recovered VM/title session is active, its external
        // presentation is fixed for that session just like the CLI route.
        if (screen != Screen::menu || launcher_page != LauncherPage::profiles
            || focused_profile_card != 2 || custom_profile_ready) return;
        auto& mailbox = modern_pack_dialog_mailbox();
        {
            std::lock_guard lock(mailbox.mutex);
            if (mailbox.dialog_open) return;
            mailbox.filter_label = tr("MODERN ASSET PACK");
            mailbox.filter = {mailbox.filter_label.c_str(), "eonmodern"};
            mailbox.dialog_open = true;
        }
        // A null initial location deliberately avoids default-directory
        // lookup, creation, persistence, or background pack discovery.
        SDL_ShowOpenFileDialog(receive_modern_pack_dialog_selection, &mailbox, window,
            &mailbox.filter, 1, nullptr, false);
    };
    const auto cycle_output_resolution = [&](const int direction) {
        const auto count = output_resolutions.size();
        const auto current = modern_graphics_settings.output_resolution_index;
        const auto next = direction < 0 ? (current + count - 1U) % count : (current + 1U) % count;
        modern_graphics_settings.output_resolution_index = next;
        const auto& resolution = output_resolutions.at(next);
        SDL_SetWindowSize(window, resolution.width, resolution.height);
    };
    const auto cycle_aspect_ratio = [&](const int direction) {
        const auto count = display_aspect_ratios.size();
        const auto current = modern_graphics_settings.aspect_ratio_index;
        modern_graphics_settings.aspect_ratio_index = direction < 0
            ? (current + count - 1U) % count : (current + 1U) % count;
    };
    const auto cycle_render_pacing = [&](const int direction) {
        constexpr auto count = static_cast<int>(RenderPacing::uncapped) + 1;
        const auto current = static_cast<int>(modern_graphics_settings.render_pacing);
        const auto next = direction < 0 ? (current + count - 1) % count : (current + 1) % count;
        modern_graphics_settings.render_pacing = static_cast<RenderPacing>(next);
        // SDL's swap interval is a presentation setting.  A failed request is
        // reported to stderr, but does not cause a recovery path to change.
        if (!SDL_SetRenderVSync(renderer,
                modern_graphics_settings.render_pacing == RenderPacing::vsync ? 1 : 0)) {
            std::cerr << "Unable to set renderer frame pacing: " << SDL_GetError() << '\n';
        }
    };
    if (!SDL_SetRenderVSync(renderer,
            modern_graphics_settings.render_pacing == RenderPacing::vsync ? 1 : 0)) {
        std::cerr << "Unable to apply saved renderer frame pacing: " << SDL_GetError() << '\n';
    }
    const auto change_modern_graphics_option = [&](const int direction) {
        const bool modern_renderer = request.presentation == eon::Presentation::modern;
        if (!modern_renderer) {
            if (modern_graphics_settings.focused_option == 0) cycle_output_resolution(direction);
            else if (modern_graphics_settings.focused_option == 1) cycle_aspect_ratio(direction);
            return;
        }
        switch (modern_graphics_settings.focused_option) {
        case 0: cycle_modern_graphics_preset(modern_graphics_settings, direction); break;
        case 1:
            cycle_output_resolution(direction);
            mark_modern_graphics_custom(modern_graphics_settings);
            break;
        case 2:
            cycle_aspect_ratio(direction);
            mark_modern_graphics_custom(modern_graphics_settings);
            break;
        case 3: cycle_render_pacing(direction); mark_modern_graphics_custom(modern_graphics_settings); break;
        case 4: {
            const auto next = (static_cast<int>(modern_graphics_settings.pixel_reconstruction) + 3 + direction) % 3;
            modern_graphics_settings.pixel_reconstruction = static_cast<PixelReconstruction>(next);
            if (millennium_modern_preview_texture) SDL_DestroyTexture(millennium_modern_preview_texture);
            millennium_modern_preview_texture = nullptr;
            millennium_modern_pipeline.reset();
            if (modern_preview_texture) SDL_DestroyTexture(modern_preview_texture);
            modern_preview_texture = nullptr;
            deuteros_modern_pipeline.reset();
            mark_modern_graphics_custom(modern_graphics_settings); break;
        }
        case 5: modern_graphics_settings.smooth_scaling = !modern_graphics_settings.smooth_scaling;
            mark_modern_graphics_custom(modern_graphics_settings); break;
        case 6: modern_graphics_settings.scanlines = !modern_graphics_settings.scanlines;
            mark_modern_graphics_custom(modern_graphics_settings); break;
        case 7: modern_graphics_settings.frame = !modern_graphics_settings.frame;
            mark_modern_graphics_custom(modern_graphics_settings); break;
        case 8: open_modern_pack_dialog(); break;
        case 9:
            show_recovery_function_map = false;
            recovery_function_map_page = 0;
            show_modern_runtime_diagnostics = true;
            break;
        case 10: modern_graphics_settings.reduced_motion = !modern_graphics_settings.reduced_motion;
            mark_modern_graphics_custom(modern_graphics_settings); break;
        default: break;
        }
    };
    const auto close_modern_graphics_settings = [&] {
        show_recovery_function_map = false;
        recovery_function_map_page = 0;
        show_modern_runtime_diagnostics = false;
        show_modern_graphics_settings = false;
        runtime.set_input_suppressed(false);
        if (request.presentation == eon::Presentation::modern) {
            const eon::PresentationPreferences preferences{
                modern_graphics_settings.output_resolution_index,
                modern_graphics_settings.aspect_ratio_index,
                static_cast<std::size_t>(modern_graphics_settings.preset),
                static_cast<std::size_t>(modern_graphics_settings.render_pacing),
                static_cast<std::size_t>(modern_graphics_settings.pixel_reconstruction),
                modern_graphics_settings.smooth_scaling,
                modern_graphics_settings.scanlines,
                modern_graphics_settings.frame,
                modern_graphics_settings.reduced_motion,
                request.language,
            };
            if (!eon::save_presentation_preferences(presentation_preferences_path, preferences)) {
                std::cerr << "Unable to save Modern presentation preferences: "
                          << presentation_preferences_path << '\n';
            }
        }
        if (screen == Screen::menu && launcher_page == LauncherPage::profiles
            && focused_profile_card == 2) launcher_session.confirm_custom();
    };
    const auto visible_graphics_option_count = [&] {
        return request.presentation == eon::Presentation::modern
            ? modern_graphics_option_count : original_display_option_count;
    };
    const auto modal_pointer_position = [&](const SDL_Event& event) -> std::optional<SDL_FPoint> {
        // A touchscreen can synthesize a mouse click. Admit exactly one of
        // those representations so one physical tap cannot change a renderer
        // setting twice. Both forms use renderer coordinates and are consumed
        // by the F10 modal before they can reach recovered input handling.
        float window_x = 0.0F;
        float window_y = 0.0F;
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.which != SDL_TOUCH_MOUSEID) {
            window_x = event.button.x;
            window_y = event.button.y;
        } else if (event.type == SDL_EVENT_FINGER_DOWN) {
            int window_width = 0;
            int window_height = 0;
            SDL_GetWindowSize(window, &window_width, &window_height);
            window_x = event.tfinger.x * static_cast<float>(window_width);
            window_y = event.tfinger.y * static_cast<float>(window_height);
        } else {
            return std::nullopt;
        }
        SDL_FPoint position{};
        if (!SDL_RenderCoordinatesFromWindow(renderer, window_x, window_y, &position.x, &position.y)) {
            return std::nullopt;
        }
        return position;
    };
    const auto recovery_function_map_page_count = [&] {
        constexpr std::size_t rows_per_page = 3;
        const auto function_count = current_modern_runtime_diagnostics().recovery_functions.size();
        return std::max<std::size_t>(1, (function_count + rows_per_page - 1U) / rows_per_page);
    };
    const auto handle_modal_pointer_down = [&](const float x, const float y) {
        // F10's pages are launcher-only. This handler deliberately has no
        // route to runtime input, original media, or session state.
        if (!inside(modern_graphics_popup_bounds, x, y)) {
            if (show_modern_runtime_diagnostics) {
                if (show_recovery_function_map) show_recovery_function_map = false;
                else show_modern_runtime_diagnostics = false;
            } else {
                close_modern_graphics_settings();
            }
            return;
        }
        if (show_modern_runtime_diagnostics) {
            if (!show_recovery_function_map) {
                // The visible ENTER prompt occupies this header band; pointer
                // activation is equivalent to Enter, not an alternate source
                // of diagnostic data.
                if (y >= 192.0F && y <= 224.0F) {
                    show_recovery_function_map = true;
                    recovery_function_map_page = 0;
                }
                return;
            }
            // The function-map header has no mutable data. Its left/right
            // halves page the same read-only rows as Up/Down; any body tap
            // returns to the diagnostics page like Escape/F10.
            if (y >= 192.0F && y <= 252.0F) {
                const auto page_count = recovery_function_map_page_count();
                recovery_function_map_page = x < 640.0F
                    ? (recovery_function_map_page + page_count - 1U) % page_count
                    : (recovery_function_map_page + 1U) % page_count;
            } else {
                show_recovery_function_map = false;
            }
            return;
        }
        const auto first_row_top = modern_graphics_option_first_baseline - 22.0F;
        const auto row = static_cast<int>((y - first_row_top) / modern_graphics_option_stride);
        if (row >= 0 && row < visible_graphics_option_count()) {
            modern_graphics_settings.focused_option = row;
            change_modern_graphics_option(1);
        }
    };
    std::optional<std::uint64_t> last_capped_present_ns;
    bool running = true;
    while (running) {
        std::optional<OriginalDataSourceSelection> selected_original_data_source;
        {
            auto& mailbox = original_data_source_dialog_mailbox();
            std::lock_guard lock(mailbox.mutex);
            if (mailbox.pending_selection) {
                selected_original_data_source = std::move(*mailbox.pending_selection);
                mailbox.pending_selection.reset();
            }
        }
        if (selected_original_data_source && screen == Screen::menu) {
            // The path shape chosen in the native dialog is part of the
            // handoff contract. The shared classifier rejects symlinks before
            // ReleaseScanner receives the exact directory or regular archive.
            const auto source_kind = eon::classify_original_data_source(
                selected_original_data_source->path);
            const auto is_directory = selected_original_data_source->kind
                    == OriginalDataSourceDialogKind::directory
                && source_kind == eon::OriginalDataSourceKind::directory;
            const auto is_archive = selected_original_data_source->kind
                    == OriginalDataSourceDialogKind::archive
                && source_kind == eon::OriginalDataSourceKind::archive;
            if (is_directory || is_archive) {
                // A selected source replaces only the scanner instance.  No
                // archive is opened here; ReleaseScanner advances later in
                // bounded steps and all recognition remains hash-addressed.
                request.data_directory = selected_original_data_source->path;
                request.data_directory_is_default = false;
                scanner = std::make_unique<eon::ReleaseScanner>(request.data_directory);
                releases.clear();
                launcher_interaction.reset_for_data(cards[static_cast<std::size_t>(focused)].game);
                request.presentation = launcher_session.presentation;
                clear_modern_pack_admission();
                reset_active_runtime();
                launcher_runtime_admission = "NOT SELECTED";
                show_scanner = true;
                focus_menu_card(focused);
            } else {
                std::cerr << "Selected original data source is not accessible: "
                          << selected_original_data_source->path.string() << '\n';
            }
        }
        // SDL may call the dialog callback on another thread. Consume its
        // one explicit candidate only while the same unlaunched Custom modal
        // remains active; a late callback can never alter an active session.
        {
            auto& mailbox = modern_pack_dialog_mailbox();
            std::lock_guard lock(mailbox.mutex);
            if (mailbox.pending_selection) {
                if (screen == Screen::menu && launcher_page == LauncherPage::profiles
                    && focused_profile_card == 2 && !custom_profile_ready) {
                    // The dialog yields an untrusted path.  Bind it to the
                    // exact release card before F10 may call it ready; no
                    // extension, pack id, language, or scan order is used as
                    // a substitute for this original-media identity.
                    if (const auto resolved = launcher_route.resolve_launch(request, releases)) {
                        admit_modern_pack_for_release(*mailbox.pending_selection, resolved->release);
                    } else {
                        selected_modern_pack_manifest.reset();
                        selected_modern_pack_preflight = eon::ModernAssetPackPreflight{
                            false, {}, {}, {}, "No exact original release is selected"};
                        modern_pack_admission = ModernPackAdmission::rejected;
                        std::cerr << "Modern asset pack rejected before launch: "
                                  << selected_modern_pack_preflight->error << '\n';
                    }
                }
                mailbox.pending_selection.reset();
            }
        }
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F10 && !event.key.repeat) {
                // F10 is consumed by Project Eon's renderer chrome, never by
                // original DOS/Amiga input. Original exposes only its two
                // display-only controls; it never switches into Modern or
                // enables a filter, art pack, pacing change, or diagnostic.
                if (show_modern_runtime_diagnostics) {
                    // F10 follows the displayed back target: the function
                    // map returns to diagnostics, while diagnostics returns
                    // to the renderer settings page.
                    if (show_recovery_function_map) show_recovery_function_map = false;
                    else show_modern_runtime_diagnostics = false;
                    continue;
                }
                if (!show_modern_graphics_settings) {
                    clear_deuteros_opening_input();
                } else {
                    runtime.set_input_suppressed(false);
                }
                show_modern_graphics_settings = !show_modern_graphics_settings;
                modern_graphics_settings.focused_option = std::min(
                    modern_graphics_settings.focused_option, visible_graphics_option_count() - 1);
                if (!show_modern_graphics_settings && screen == Screen::menu
                    && launcher_page == LauncherPage::profiles
                    && focused_profile_card == 2) launcher_session.confirm_custom();
                continue;
            }
            if (show_modern_graphics_settings) {
                // This renderer-only dialog is a real input boundary. In
                // particular, Space, Enter and South/A must not leak into a
                // recovered opening or DOS availability poll behind it.
                if (show_modern_runtime_diagnostics) {
                    // The diagnostics page is strictly read-only. It consumes
                    // launcher navigation before any recovered input route.
                    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat
                        && event.key.key == SDLK_ESCAPE) {
                        if (show_recovery_function_map) show_recovery_function_map = false;
                        else show_modern_runtime_diagnostics = false;
                    } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat
                        && !show_recovery_function_map
                        && (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER)) {
                        show_recovery_function_map = true;
                        recovery_function_map_page = 0;
                    } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat
                        && show_recovery_function_map
                        && (event.key.key == SDLK_UP || event.key.key == SDLK_DOWN)) {
                        const auto page_count = recovery_function_map_page_count();
                        recovery_function_map_page = event.key.key == SDLK_UP
                            ? (recovery_function_map_page + page_count - 1U) % page_count
                            : (recovery_function_map_page + 1U) % page_count;
                    } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN
                        && event.gbutton.button == SDL_GAMEPAD_BUTTON_BACK) {
                        if (show_recovery_function_map) show_recovery_function_map = false;
                        else show_modern_runtime_diagnostics = false;
                    } else if (const auto pointer = modal_pointer_position(event)) {
                        handle_modal_pointer_down(pointer->x, pointer->y);
                    }
                } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                    if (event.key.key == SDLK_ESCAPE) {
                        close_modern_graphics_settings();
                    } else if (event.key.key == SDLK_UP) {
                        modern_graphics_settings.focused_option =
                            (modern_graphics_settings.focused_option + visible_graphics_option_count() - 1)
                                % visible_graphics_option_count();
                    } else if (event.key.key == SDLK_DOWN) {
                        modern_graphics_settings.focused_option =
                            (modern_graphics_settings.focused_option + 1) % visible_graphics_option_count();
                    } else if (event.key.key == SDLK_LEFT || event.key.key == SDLK_RIGHT) {
                        change_modern_graphics_option(event.key.key == SDLK_LEFT ? -1 : 1);
                    }
                } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                    if (event.gbutton.button == SDL_GAMEPAD_BUTTON_BACK) {
                        close_modern_graphics_settings();
                    } else if (event.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_UP) {
                        modern_graphics_settings.focused_option =
                            (modern_graphics_settings.focused_option + visible_graphics_option_count() - 1)
                                % visible_graphics_option_count();
                    } else if (event.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_DOWN) {
                        modern_graphics_settings.focused_option =
                            (modern_graphics_settings.focused_option + 1) % visible_graphics_option_count();
                    } else if (event.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_LEFT
                        || event.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) {
                        change_modern_graphics_option(
                            event.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_LEFT ? -1 : 1);
                    }
                } else if (const auto pointer = modal_pointer_position(event)) {
                    handle_modal_pointer_down(pointer->x, pointer->y);
                }
                continue;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
                if (screen == Screen::launching && !request.game) {
                    reset_active_runtime();
                    screen = Screen::menu;
                }
                else if (screen == Screen::menu && launcher_page != LauncherPage::games) {
                    back_launcher_cards();
                }
                else running = false;
                // Escape is a single navigation action. Do not let this same
                // event reach the menu key handler below and skip two card
                // pages (profiles -> games) in one press.
                continue;
            }
            if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN
                && event.gbutton.button == SDL_GAMEPAD_BUTTON_BACK) {
                if (screen == Screen::launching && !request.game) {
                    reset_active_runtime();
                    screen = Screen::menu;
                }
                else if (screen == Screen::menu && launcher_page != LauncherPage::games) {
                    back_launcher_cards();
                } else running = false;
                // Keep Back equivalent to Escape and consume it before the
                // game/menu controls can reinterpret the input.
                continue;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F1 && !event.key.repeat) {
                // Presentation is chosen by the profile card before launch.
                // Keep this historical shortcut inert so it cannot violate
                // an Original session's immutable renderer contract.
                continue;
            }
            if (event.type == SDL_EVENT_TEXT_INPUT && event.text.text && event.text.text[0] != '\0'
                && screen == Screen::launching && selected == eon::Game::millennium
                && millennium_startup_input
                && (millennium_startup_input->sound_selection_active
                    || millennium_startup_input->title_active)) {
                if (millennium_startup_input->sound_selection_active) {
                    // The source-level menu accepts literal ASCII data bytes,
                    // not an SDL scancode or an inferred device choice. Its
                    // one selection ends at the driver ABI boundary.
                    const auto input_result = runtime.observe_input(
                        eon::RuntimeInputObservation::ascii(event.text.text[0]));
                    millennium_startup_input = runtime.millennium_dos_startup_input();
                    if (input_result == eon::RuntimeInputDisposition::boundary_reached
                        && millennium_title_text_input_active) {
                        SDL_StopTextInput(window);
                        millennium_title_text_input_active = false;
                    }
                    continue;
                }
                // TITLES.EXE's INT 21h/AH=06h poll distinguishes a nonzero
                // console character from a raw physical key. SDL text input
                // supplies only that availability signal; UTF-8 contents are
                // deliberately never decoded as a DOS character or command.
                if (!millennium_startup_input->title_handed_off) {
                    const auto input_result = runtime.observe_input(
                        eon::RuntimeInputObservation::available_character());
                    millennium_startup_input = runtime.millennium_dos_startup_input();
                    if (input_result == eon::RuntimeInputDisposition::boundary_reached
                        && millennium_title_text_input_active) {
                        SDL_StopTextInput(window);
                        millennium_title_text_input_active = false;
                    }
                }
            }
            if ((event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP)
                && screen == Screen::launching && selected == eon::Game::deuteros
                && (event.key.key == SDLK_SPACE || event.key.key == SDLK_RETURN)) {
                // $14 consumes the input word last polled by the original loop.
                // Feed the physical held state, leaving acceptance to the VM's
                // recovered timing and input-gate logic.
                static_cast<void>(runtime.observe_input(
                    eon::RuntimeInputObservation::opening_input_held(
                        event.type == SDL_EVENT_KEY_DOWN)));
            }
            if ((event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN
                    || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP)
                && screen == Screen::launching && selected == eon::Game::deuteros
                && event.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH) {
                // The sole gamepad route into the recovered Amiga opening is
                // the same physical held signal as Space/Enter. It does not
                // manufacture a title or gameplay action.
                static_cast<void>(runtime.observe_input(
                    eon::RuntimeInputObservation::opening_input_held(
                        event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)));
            }
            if (screen == Screen::menu && event.type == SDL_EVENT_KEY_DOWN
                && event.key.key == SDLK_D && !event.key.repeat) show_scanner = !show_scanner;
            if (screen == Screen::menu && event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                if (event.key.key == SDLK_L) {
                    cycle_launcher_language(1);
                    continue;
                }
                if (event.key.key == SDLK_O) {
                    open_original_data_source_dialog(OriginalDataSourceDialogKind::directory);
                    continue;
                }
                if (event.key.key == SDLK_A) {
                    open_original_data_source_dialog(OriginalDataSourceDialogKind::archive);
                    continue;
                }
                const bool previous = event.key.key == SDLK_LEFT || event.key.key == SDLK_UP;
                const bool next = event.key.key == SDLK_RIGHT || event.key.key == SDLK_DOWN;
                if (previous || next) move_launcher_cards(previous ? -1 : 1);
                else if (event.key.key == SDLK_HOME) edge_launcher_cards(true);
                else if (event.key.key == SDLK_END) edge_launcher_cards(false);
                else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                    activate_launcher_card(std::nullopt);
                }
            }
            if (screen == Screen::menu && event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                const auto button = event.gbutton.button;
                if (button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) {
                    cycle_launcher_language(-1);
                } else if (button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) {
                    cycle_launcher_language(1);
                } else if (button == SDL_GAMEPAD_BUTTON_DPAD_LEFT || button == SDL_GAMEPAD_BUTTON_DPAD_UP) {
                    move_launcher_cards(-1);
                } else if (button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT || button == SDL_GAMEPAD_BUTTON_DPAD_DOWN) {
                    move_launcher_cards(1);
                } else if (button == SDL_GAMEPAD_BUTTON_SOUTH || button == SDL_GAMEPAD_BUTTON_START) {
                    activate_launcher_card(std::nullopt);
                }
            }
            if (screen == Screen::menu && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                && event.button.which != SDL_TOUCH_MOUSEID) {
                float x = 0, y = 0;
                SDL_RenderCoordinatesFromWindow(renderer, event.button.x, event.button.y, &x, &y);
                handle_menu_pointer_down(x, y);
            }
            if (screen == Screen::menu && event.type == SDL_EVENT_FINGER_DOWN) {
                int window_width = 0;
                int window_height = 0;
                SDL_GetWindowSize(window, &window_width, &window_height);
                float x = 0, y = 0;
                SDL_RenderCoordinatesFromWindow(renderer,
                    event.tfinger.x * static_cast<float>(window_width),
                    event.tfinger.y * static_cast<float>(window_height), &x, &y);
                handle_menu_pointer_down(x, y);
            }
        }

        // The DOS compatibility service is a bounded native tick. It can
        // consume only deterministic operations against the already admitted
        // immutable driver leaf and becomes a no-op at allocation, vector,
        // stack, and EXEC evidence boundaries.
        if (screen == Screen::launching) {
            static_cast<void>(runtime.tick_millennium_dos_compatibility_runner());
        }

        if (!scanner->done()) {
            const auto source_before_scan = launcher_interaction.source_identity();
            static_cast<void>(scanner->advance(show_scanner ? 32 : 1));
            releases = scanner->releases();
            // A scan can discover a second verified container after an
            // earlier unique release was shown. Reconcile through the common
            // card controller so that no automatic selection can silently
            // become an ambiguous launch, and revoke all source-bound SDL
            // state if that reconciliation changes the identity.
            launcher_interaction.synchronize(releases);
            apply_launcher_navigation(source_before_scan);
            if (screen == Screen::menu && !request.platform) focus_menu_card(focused);
        }
        if (screen == Screen::launching && selected == eon::Game::deuteros
            && runtime.state() == eon::NativeSessionState::deuteros_amiga_opening) {
            const auto advance = runtime.advance(SDL_GetTicks());
            if (advance.opening_started) {
                // A scheduler can only be restarted for this already admitted
                // native opening. Rebuild transient SDL objects from its live
                // presentation rather than retaining a prior release's ones.
            create_deuteros_opening_texture();
            start_deuteros_audio();
            load_deuteros_external_modern_sequence();
            }
            // The runner owns only scheduler arithmetic and delegates every
            // tick through the native lifecycle controller. A title-boundary
            // transition is therefore published before SDL sees its events.
            for (const auto& events : advance.opening.events) {
                if (!events.alternate_resources.empty()) {
                    // Opcode $0f exposes this original bundle-relative target.
                    // It is retained as evidence for the subsequent verified
                    // stage, not given an invented title/menu interpretation.
                    deuteros_title_resource = events.alternate_resources.front().resource_relative_offset;
                }
                if (events.title_handoff) {
                    // The original opening returns to bootstrap here. Drop
                    // any host-side preview PCM rather than letting it play
                    // under the unexecuted title stage.
                    if (deuteros_audio_stream) {
                        static_cast<void>(SDL_ClearAudioStream(deuteros_audio_stream));
                    }
                }
            }
            // VM events are proven at 50 Hz, but the exact relation between
            // that scheduler and host-device latency is not yet recovered.
            // Keep just one VBL of original PCM queued; do not manufacture a
            // silent/fallback waveform to fill the device.
            if (deuteros_audio_stream) {
                constexpr int queued_target_bytes = 960 * 2 * static_cast<int>(sizeof(float));
                constexpr int bytes_per_frame = 2 * static_cast<int>(sizeof(float));
                int queued = SDL_GetAudioStreamQueued(deuteros_audio_stream);
                while (queued >= 0 && queued < queued_target_bytes) {
                    const auto missing_frames = static_cast<std::size_t>(
                        (queued_target_bytes - queued + bytes_per_frame - 1) / bytes_per_frame);
                    const auto samples = runtime.render_deuteros_amiga_opening_audio(missing_frames);
                    if (!samples || samples->empty()) break;
                    if (!SDL_PutAudioStreamData(deuteros_audio_stream, samples->data(),
                        static_cast<int>(samples->size() * sizeof(float)))) {
                        std::cerr << "Unable to queue Deuteros audio: " << SDL_GetError() << '\n';
                        break;
                    }
                    queued = SDL_GetAudioStreamQueued(deuteros_audio_stream);
                }
            }
        }

        const bool modern = request.presentation == eon::Presentation::modern;
        SDL_SetRenderDrawColor(renderer, modern ? 5 : 0, modern ? 15 : 0, modern ? 27 : 0, 255);
        SDL_RenderClear(renderer);
        if (modern) draw_modern_chrome(renderer);
        SDL_SetRenderDrawColor(renderer, 205, 225, 235, 255);

        if (screen == Screen::menu) {
            if (project_eon_logo_texture) {
                const SDL_FRect logo_bounds{1060.0F, 18.0F, 150.0F, 150.0F};
                SDL_RenderTexture(renderer, project_eon_logo_texture, nullptr, &logo_bounds);
            }
            draw_text(renderer, 64, 56, tr("PROJECT EON"));
            SDL_SetRenderDrawColor(renderer, 24, 55, 88, 255);
            SDL_RenderFillRect(renderer, &launcher_language_bounds);
            SDL_SetRenderDrawColor(renderer, 185, 210, 135, 255);
            SDL_RenderRect(renderer, &launcher_language_bounds);
            draw_text(renderer, launcher_language_bounds.x + 10.0F,
                launcher_language_bounds.y + 9.0F,
                "< " + tr("LANGUAGE") + ": "
                    + std::string(eon::launcher_language_autonym(request.language)) + " >");
            SDL_SetRenderDrawColor(renderer, 24, 55, 88, 255);
            SDL_RenderFillRect(renderer, &data_directory_picker_bounds);
            SDL_SetRenderDrawColor(renderer, 185, 210, 135, 255);
            SDL_RenderRect(renderer, &data_directory_picker_bounds);
            draw_text(renderer, data_directory_picker_bounds.x + 10.0F,
                data_directory_picker_bounds.y + 9.0F,
                tr("CHOOSE ORIGINAL DATA FOLDER (O)"));
            SDL_SetRenderDrawColor(renderer, 24, 55, 88, 255);
            SDL_RenderFillRect(renderer, &data_archive_picker_bounds);
            SDL_SetRenderDrawColor(renderer, 185, 210, 135, 255);
            SDL_RenderRect(renderer, &data_archive_picker_bounds);
            draw_text(renderer, data_archive_picker_bounds.x + 10.0F,
                data_archive_picker_bounds.y + 9.0F,
                tr("CHOOSE ORIGINAL ARCHIVE (A)"));
            const auto draw_card_border = [&](const SDL_FRect& bounds, const bool active, const bool enabled) {
                SDL_SetRenderDrawColor(renderer, active ? 255 : enabled ? 185 : 85,
                    active ? 195 : enabled ? 210 : 90, active ? 80 : enabled ? 135 : 90, 255);
                SDL_RenderRect(renderer, &bounds);
            };
            if (launcher_page != LauncherPage::games) {
                SDL_SetRenderDrawColor(renderer, 32, 73, 104, 255);
                SDL_RenderFillRect(renderer, &launcher_back_bounds);
                SDL_SetRenderDrawColor(renderer, 110, 190, 232, 255);
                SDL_RenderRect(renderer, &launcher_back_bounds);
                draw_text(renderer, launcher_back_bounds.x + 12.0F, launcher_back_bounds.y + 9.0F,
                    "<<");
            }
            if (launcher_page == LauncherPage::games) {
                draw_text(renderer, 64, 82, tr("SELECT A GAME"));
                draw_text(renderer, 64, 108, tr("CLICK A GAME CARD OR USE LEFT/RIGHT, THEN ENTER"));
                for (std::size_t index = 0; index < cards.size(); ++index) {
                    auto& card = cards[index];
                    if (card.texture) SDL_RenderTexture(renderer, card.texture, nullptr, &card.bounds);
                    const bool available = eon::release_available(releases, card.game, std::nullopt);
                    draw_card_border(card.bounds, index == static_cast<std::size_t>(focused), available);
                    draw_text(renderer, card.bounds.x + 18, card.bounds.y + card.bounds.h - 45,
                        tr(card.title));
                    draw_text(renderer, card.bounds.x + 18, card.bounds.y + card.bounds.h - 25,
                        tr(card.subtitle));
                    draw_text(renderer, card.bounds.x + 18, card.bounds.y + card.bounds.h + 16,
                        available ? tr("VERIFIED ORIGINAL DATA") : scanner->done()
                        ? tr("ORIGINAL DATA NOT FOUND") : tr("SCANNING ORIGINAL DATA..."));
                }
            } else if (launcher_page == LauncherPage::platforms) {
                const auto game = cards[static_cast<std::size_t>(focused)].game;
                const auto platform_cards = platform_cards_for_game(game);
                draw_text(renderer, 64, 82, tr("SELECT A VERIFIED PLATFORM"));
                draw_text(renderer, 64, 108, tr("UNAVAILABLE PLATFORM CARDS CANNOT START A GAME"));
                for (std::size_t index = 0; index < platform_cards.size(); ++index) {
                    auto& card = platform_cards[index];
                    const auto status = eon::platform_card_status(releases, game, card.platform);
                    const bool selectable = eon::platform_card_selectable(status);
                    if (card.texture) SDL_RenderTexture(renderer, card.texture, nullptr, &card.bounds);
                    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                    if (!selectable) {
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 155);
                        SDL_RenderFillRect(renderer, &card.bounds);
                    }
                    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                    draw_card_border(card.bounds, index == static_cast<std::size_t>(focused_platform_card), selectable);
                    // Coverage is a project capability, while the final row
                    // is this scan's immutable-media admission. Showing both
                    // prevents a recovered path from being mistaken for an
                    // installed release, or a present archive for parity.
                    draw_text(renderer, card.bounds.x + 18, card.bounds.y + card.bounds.h - 68,
                        tr(card.title));
                    draw_text(renderer, card.bounds.x + 18, card.bounds.y + card.bounds.h - 44,
                        tr(eon::name(eon::platform_coverage(game, card.platform))));
                    draw_text(renderer, card.bounds.x + 18, card.bounds.y + card.bounds.h - 20,
                        status == eon::PlatformCardStatus::release_selection_required
                        ? tr("RELEASE SELECTION REQUIRED") : selectable ? tr("VERIFIED ORIGINAL DATA") : scanner->done()
                        ? tr("ORIGINAL DATA NOT FOUND") : tr("SCANNING ORIGINAL DATA..."));
                }
            } else if (launcher_page == LauncherPage::releases) {
                const auto release_page = release_language_cards();
                const auto& language_cards = release_page.cards;
                draw_text(renderer, 64, 82, tr("SELECT AN ORIGINAL RELEASE"));
                draw_text(renderer, 64, 108, tr("RELEASE IDENTITY IS FIXED AT LAUNCH"));
                if (release_page.page_count > 1) {
                    SDL_SetRenderDrawColor(renderer, 32, 73, 104, 255);
                    SDL_RenderFillRect(renderer, &release_page_previous_bounds);
                    SDL_RenderFillRect(renderer, &release_page_next_bounds);
                    SDL_SetRenderDrawColor(renderer, 110, 190, 232, 255);
                    SDL_RenderRect(renderer, &release_page_previous_bounds);
                    SDL_RenderRect(renderer, &release_page_next_bounds);
                    draw_text(renderer, release_page_previous_bounds.x + 12.0F,
                        release_page_previous_bounds.y + 7.0F, "< " + tr("PAGE"));
                    draw_text(renderer, release_page_next_bounds.x + 12.0F,
                        release_page_next_bounds.y + 7.0F, tr("PAGE") + " >");
                    draw_text(renderer, 1040, 108, tr("PAGE") + " "
                        + std::to_string(release_page.page + 1U) + "/"
                        + std::to_string(release_page.page_count));
                }
                for (std::size_t index = 0; index < language_cards.size(); ++index) {
                    const auto& card = language_cards[index];
                    if (card.texture) SDL_RenderTexture(renderer, card.texture, nullptr, &card.bounds);
                    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(renderer, 3, 10, 20, card.texture ? 142 : 255);
                    SDL_RenderFillRect(renderer, &card.bounds);
                    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                    draw_card_border(card.bounds,
                        card.identity_index == static_cast<std::size_t>(focused_release_card), true);
                    // Language is part of the hash-bound original identity.
                    // Only the two currently catalogued labels are localized;
                    // a future recognised language must remain visibly its
                    // exact manifest code rather than being mislabelled as
                    // English by a launcher fallback.
                    const auto label = card.language == "es" ? tr("SPANISH")
                        : card.language == "en" ? tr("ENGLISH") : card.language;
                    draw_text(renderer, card.bounds.x + 24, card.bounds.y + 84,
                        active_platform ? tr(launcher_platform_label(*active_platform)) : tr("UNKNOWN PLATFORM"));
                    draw_text(renderer, card.bounds.x + 24, card.bounds.y + 126, label);
                    draw_text(renderer, card.bounds.x + 24, card.bounds.y + 150,
                        std::string(tr("VERIFIED ORIGINAL DATA")) + " / "
                            + truncated_identity_hash(card.sha256));
                    draw_text(renderer, card.bounds.x + 24, card.bounds.y + 184,
                        active_platform
                        ? tr(eon::name(eon::platform_coverage(eon::ReleaseArchive{
                            launcher_route.game, *active_platform, card.language, card.sha256, {},
                            eon::ReleaseMediaLayout::zip_archive, {}})))
                        : tr("RELEASE IDENTITY IS FIXED AT LAUNCH"));
                }
            } else {
                draw_text(renderer, 64, 82, tr("SELECT A PRESENTATION PROFILE"));
                draw_text(renderer, 64, 108, tr("ORIGINAL AND MODERN START DIRECTLY. CUSTOM TUNES MODERN FIRST."));
                for (std::size_t index = 0; index < profile_cards.size(); ++index) {
                    auto& card = profile_cards[index];
                    if (card.texture) SDL_RenderTexture(renderer, card.texture, nullptr, &card.bounds);
                    draw_card_border(card.bounds, index == static_cast<std::size_t>(focused_profile_card), true);
                    draw_text(renderer, card.bounds.x + 18, card.bounds.y + card.bounds.h - 46,
                        tr(card.title));
                    draw_text(renderer, card.bounds.x + 18, card.bounds.y + card.bounds.h - 22,
                        tr(card.subtitle));
                }
                if (focused_profile_card == 2 && custom_profile_ready) {
                    draw_text(renderer, 64, 530, tr("CUSTOM SETTINGS READY — ENTER / CLICK TO START MODERN"));
                }
                if (launcher_runtime_admission != "NOT SELECTED"
                    && launcher_runtime_admission != "READY") {
                    draw_text(renderer, 64, 530, std::string(tr("RUNTIME ADMISSION")) + ": "
                        + tr(launcher_runtime_admission) + " / "
                        + tr(launcher_runtime_rejection));
                }
            }
            if (show_scanner) {
                const auto snapshot = scanner->snapshot();
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 220);
                SDL_FRect overlay{64, 530, 1152, 178};
                SDL_RenderFillRect(renderer, &overlay);
                draw_text(renderer, 86, 552, tr("DATA SCANNER (content hashes, read-only)"));
                draw_text(renderer, 86, 576, scanner_source_text(snapshot.source_kind));
                const auto scanner_progress = snapshot.discovering
                    ? tr("Discovering files: ") + std::to_string(snapshot.candidate_count)
                    : tr("Files hashed: ") + std::to_string(snapshot.scanned_count)
                        + " / " + std::to_string(snapshot.candidate_count);
                draw_text(renderer, 86, 600, scanner_progress);
                draw_text(renderer, 86, 624, scanner_rejections_text(snapshot));
                draw_text(renderer, 86, 648, scanner_admission_text(snapshot));
                draw_text(renderer, 86, 672, snapshot.complete
                    ? tr("Complete. Only hash-verified original releases are selectable.")
                    : tr("Scanning in progress. Press D to hide this progress panel."));
            }
        } else {
            draw_text(renderer, 64, 56, tr("LAUNCH REQUEST ACCEPTED"));
            draw_text(renderer, 64, 92, tr("Game: ") + tr(launcher_game_label(selected)));
            draw_text(renderer, 64, 116, tr("Platform: ")
                + (active_platform ? tr(launcher_platform_label(*active_platform)) : tr("AUTO")));
            draw_text(renderer, 64, 136, modern ? tr("Presentation: Modern") : tr("Presentation: Original"));
            draw_text(renderer, 64, 156, tr("Original data is present and selected."));
            draw_text(renderer, 64, 180, tr("The simulation is incomplete; no synthetic substitute will run."));
            if (selected == eon::Game::millennium && millennium_assets
                && millennium_startup_input && millennium_startup_input->sound_selection_active
                && millennium_assets->assets.sound_selection_prompt) {
                // These lines are unmodified text bytes from the verified
                // launcher, rendered with Eon's UI font only because the DOS
                // text-mode font/mode has not been recovered. They are not
                // translated launcher text and are not a visual-parity claim.
                draw_original_multiline_text(renderer, 64, 222,
                    *millennium_assets->assets.sound_selection_prompt);
                if (!millennium_startup_input->sound_selection_awaiting_choice
                    && millennium_startup_input->selected_original_filename) {
                    draw_original_text(renderer, 64, 430,
                        *millennium_startup_input->selected_original_filename);
                    draw_text(renderer, 64, 454, millennium_startup_input->selected_driver_is_admitted
                        ? tr("VERIFIED ORIGINAL DATA") : tr("STARTUP BOUNDARY"));
                    draw_text(renderer, 64, 478,
                        tr("The simulation is incomplete; no synthetic substitute will run."));
                }
            } else if (selected == eon::Game::millennium && millennium_preview_texture && millennium_assets) {
                // Input availability proves only TITLES.EXE's local exit path.
                // Neither DOS EXEC return nor 2200AD startup is observed, so
                // a GX canvas must never replace the original title frame.
                constexpr bool millennium_game_execution_observed = false;
                SDL_Texture* texture = millennium_texture_for(
                    modern ? modern_graphics_settings.pixel_reconstruction : PixelReconstruction::off);
                if (modern && millennium_external_modern_texture && millennium_external_modern_surface) {
                    draw_text(renderer, 64, 202, tr("MODERN TITLE PACK: ")
                        + millennium_external_modern_surface->pack_id + " ("
                        + std::to_string(millennium_external_modern_surface->width) + "x"
                        + std::to_string(millennium_external_modern_surface->height) + " RGBA PNG; "
                        + millennium_external_modern_surface->provenance + ")");
                }
                if (millennium_game_execution_observed) {
                    draw_text(renderer, 64, 220,
                        tr("AUTHENTIC DOS HANDOFF - TITLES.EXE -> 2200ad.exe; GX.LIB IMG00 -> IMG01"));
                    draw_text(renderer, 64, 238,
                        tr("ORIGINAL GX CANVAS + READ-ONLY 2200SAVE.I POSITIONAL TABLE"));
                } else {
                    if (millennium_assets->assets.language == "es") {
                        draw_text(renderer, 64, 220,
                            tr("AUTHENTIC SPANISH DOS TITLE - FAT12 TITLE.LIB P00 + VGA RGB6 DAC"));
                        draw_text(renderer, 64, 238, millennium_startup_input
                                && millennium_startup_input->title_handed_off
                            ? tr("The simulation is incomplete; no synthetic substitute will run.")
                            : tr("TYPE A CHARACTER: ORIGINAL INT 21h/AH=06h TITLE HANDOFF"));
                    } else {
                        draw_text(renderer, 64, 220, tr("AUTHENTIC DOS TITLE - P00 INDICES + VGA RGB6 DAC"));
                        draw_text(renderer, 64, 238, millennium_startup_input
                                && millennium_startup_input->title_handed_off
                            ? tr("The simulation is incomplete; no synthetic substitute will run.")
                            : tr("TYPE A CHARACTER: ORIGINAL INT 21h/AH=06h TITLE HANDOFF"));
                    }
                }
                SDL_SetTextureScaleMode(texture,
                    modern && modern_graphics_settings.smooth_scaling
                        ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
                const auto preview_bounds = aspect_viewport(64, 250, 576, 400,
                    modern_graphics_settings);
                if (modern && modern_graphics_settings.frame) draw_modern_surface_frame(renderer, preview_bounds);
                SDL_RenderTexture(renderer, texture, nullptr, &preview_bounds);
                if (modern && modern_graphics_settings.scanlines) draw_scanlines(renderer, preview_bounds);
                if (modern) draw_modern_preset_overlay(renderer, preview_bounds,
                    modern_graphics_settings.preset, modern_graphics_settings.reduced_motion);
                if (millennium_game_execution_observed) {
                    const auto& save = *millennium_assets->assets.initial_save;
                    constexpr std::size_t records_per_page = 8;
                    const auto first_record = millennium_state_page * records_per_page;
                    std::ostringstream heading;
                    // This is a diagnostic view of original bytes, rather
                    // than game prose. Keep its variable part notation-only:
                    // it must not leak untranslated English into the launcher.
                    heading << "2200SAVE.I  SHA-256 " << save.sha256()
                            << "  v0x" << std::hex << save.layout().version << std::dec;
                    draw_text(renderer, 610, 270, heading.str());
                    draw_text(renderer, 610, 290,
                        tr("RECOVERED POSITIONAL WORDS ONLY; NO INFERRED GAME SEMANTICS"));
                    if (millennium_game_session) {
                        std::ostringstream dispatch;
                        dispatch << "F1-F10 -> [";
                        if (const auto index = millennium_game_session->last_function_key_index()) {
                            dispatch << *index << "]  (8 B)";
                        } else {
                            dispatch << "--]";
                        }
                        draw_text(renderer, 610, 310, dispatch.str());
                    }
                    if (millennium_game_session) {
                        if (const auto trace = millennium_game_session->last_first_function_key_trace()) {
                            std::ostringstream f1;
                            f1 << "F1: $" << std::hex << trace->handler_address
                               << " -> $" << trace->display_selector_call_address
                               << " -> $" << trace->setup_entry_address
                               << " (RET); $" << trace->selector_address
                               << "=0 -> $" << trace->selected_record_address << "  (+02=$"
                               << static_cast<unsigned>(trace->selected_record_byte_2) << ')';
                            draw_text(renderer, 610, 326, f1.str());
                        }
                        if (const auto trace = millennium_game_session->last_second_function_key_trace()) {
                            std::ostringstream f2;
                            f2 << "F2: [$" << std::hex
                               << trace->availability_address << "] >= $"
                               << static_cast<unsigned>(trace->minimum_availability)
                               << " -> $" << trace->handler_address
                               << " -> $" << trace->first_record_address
                               << " +$" << trace->record_stride;
                            draw_text(renderer, 610, 342, f2.str());
                        }
                        if (const auto trace = millennium_game_session->last_third_function_key_trace()) {
                            std::ostringstream f3;
                            f3 << "F3: [$" << std::hex
                               << trace->initialization_guard_address << "] == 0; [$"
                               << trace->availability_address << "] != 0 -> $"
                               << trace->handler_address << " -> $"
                               << trace->callback_address;
                            draw_text(renderer, 610, 358, f3.str());
                        }
                        if (const auto trace = millennium_game_session->last_fourth_function_key_trace()) {
                            std::ostringstream f4;
                            f4 << "F4: [$" << std::hex
                               << trace->initialization_guard_address << "] == 0 -> $"
                               << trace->handler_address << " -> $"
                               << trace->common_routine_address << " -> [$"
                               << trace->first_runtime_byte_address << "]=$"
                               << static_cast<unsigned>(trace->first_runtime_byte_value)
                               << " ; $" << trace->first_call_address
                               << " ; [$"
                               << trace->initialization_guard_clear_address << ']';
                            draw_text(renderer, 610, 374, f4.str());
                        }
                        if (const auto trace = millennium_game_session->last_fifth_function_key_trace()) {
                            std::ostringstream f5;
                            f5 << "F5: $" << std::hex << trace->handler_address
                               << " -> AL=$" << static_cast<unsigned>(trace->transfer_al_value)
                               << " -> $" << trace->first_call_address
                               << " -> $" << trace->first_call_initial_nested_call_address
                               << " ; $"
                               << trace->second_call_address << ",$" << trace->third_call_address
                               << ",$" << trace->fourth_call_address;
                            draw_text(renderer, 610, 390, f5.str());
                        }
                        if (const auto trace = millennium_game_session->last_sixth_function_key_trace()) {
                            std::ostringstream f6;
                            f6 << "F6: [$" << std::hex
                               << trace->initialization_guard_address << "] == 0 -> $"
                               << trace->handler_address << " -> [$"
                               << trace->first_byte_address << "]=$"
                               << static_cast<unsigned>(trace->first_byte_value)
                               << "; [$" << trace->second_byte_address << "]=$"
                               << static_cast<unsigned>(trace->second_byte_value)
                               << " -> $" << trace->wait_call_address;
                            draw_text(renderer, 610, 390, f6.str());
                        }
                        if (const auto trace = millennium_game_session->last_seventh_function_key_trace()) {
                            std::ostringstream f7;
                            f7 << "F7: [$" << std::hex
                               << trace->initialization_guard_address << "] == 0 -> $"
                               << trace->handler_address << " -> [$"
                               << trace->first_runtime_word_address << "],[$"
                               << trace->second_runtime_word_address << "],[$"
                               << trace->third_runtime_word_address << "] -> $"
                               << trace->terminal_call_address;
                            draw_text(renderer, 610, 390, f7.str());
                        }
                        if (const auto trace = millennium_game_session->last_eighth_function_key_trace()) {
                            std::ostringstream f8;
                            f8 << "F8: $" << std::hex << trace->handler_address
                               << " -> [$" << trace->reset_runtime_byte_address << "]=$"
                               << static_cast<unsigned>(trace->reset_runtime_byte_value)
                               << " -> $" << trace->local_preflight_address
                               << " -> $" << trace->repeated_call_address
                               << " (SHR BL,1 CARRY)";
                            draw_text(renderer, 610, 390, f8.str());
                            if (const auto effect = millennium_game_session->last_runtime_byte_effect()) {
                                std::ostringstream effect_text;
                                effect_text << "[$" << std::hex << effect->address << "] ";
                                if (effect->previous) {
                                    effect_text << '$' << static_cast<unsigned>(*effect->previous);
                                } else {
                                    effect_text << "?";
                                }
                                effect_text << " -> $" << static_cast<unsigned>(effect->value)
                                            << " ; !W, !C";
                                draw_text(renderer, 610, 406, effect_text.str());
                            }
                        }
                        if (const auto trace = millennium_game_session->last_ninth_function_key_trace()) {
                            std::ostringstream f9;
                            f9 << "F9: [$" << std::hex
                               << trace->initialization_guard_address << "] == 0 -> $"
                               << trace->handler_address << " -> [$"
                               << trace->first_reset_runtime_byte_address << "]=$"
                               << static_cast<unsigned>(trace->first_reset_runtime_byte_value)
                               << " -> [$" << trace->limit_runtime_byte_address << "] < $"
                               << static_cast<unsigned>(trace->limit_value)
                               << " -> $" << trace->local_preflight_address;
                            draw_text(renderer, 610, 390, f9.str());
                        }
                        if (const auto trace = millennium_game_session->last_tenth_function_key_trace()) {
                            std::ostringstream f10;
                            f10 << "F10: [$" << std::hex
                                << trace->initialization_guard_address << "] == 0 -> $"
                                << trace->handler_address << " -> [$"
                                << trace->limit_runtime_byte_address << "] < $"
                                << static_cast<unsigned>(trace->limit_value)
                                << " -> $" << trace->local_preflight_address
                                << " -> $" << trace->wait_call_address;
                            draw_text(renderer, 610, 390, f10.str());
                        }
                    }
                    for (std::size_t row = 0; row < records_per_page; ++row) {
                        const auto record_index = first_record + row;
                        if (record_index >= save.layout().state_table.size()) break;
                        const auto& record = save.state_record(record_index);
                        std::ostringstream line;
                        line << '[' << record_index << "] +00=0x" << std::hex
                             << record.runtime_offset_0 << " +04=0x" << record.runtime_offset_4
                             << " +06=0x" << record.runtime_offset_6 << " +08=0x"
                             << record.runtime_offset_8;
                        draw_text(renderer, 610, 406.0F + static_cast<float>(row) * 17.0F,
                            line.str());
                    }
                    std::ostringstream pager;
                    pager << "P " << (millennium_state_page + 1) << "/"
                          << ((eon::MillenniumDosSaveLayout::state_table_count + records_per_page - 1)
                              / records_per_page)
                          << "  <- / ->";
                    draw_text(renderer, 610, 496, pager.str());
                }
                draw_text(renderer, 64, 680, request.game ? tr("ESC: QUIT") : tr("ESC: BACK TO MENU"));
            } else if (selected == eon::Game::millennium && active_platform
                && *active_platform != eon::Platform::dos) {
                draw_text(renderer, 64, 220,
                    tr("VERIFIED NATIVE MILLENNIUM DATA - NO DOS RESOURCE SUBSTITUTION"));
                draw_text(renderer, 64, 244,
                    tr("INTERACTIVE AMIGA/ATARI ST FLOW IS NOT YET RECOVERED."));
                draw_text(renderer, 64, 268,
                    tr("NO SYNTHETIC SCREEN OR STATE WILL RUN FOR THIS PLATFORM."));
                const auto amiga_bootstrap = runtime.millennium_amiga_bootstrap_presentation();
                if (*active_platform == eon::Platform::amiga && amiga_bootstrap) {
                    // These are source-range and caller-side handoff facts
                    // from the same hash-validated session. They make the
                    // native Amiga boundary inspectable, but do not decode
                    // the opaque first stage or manufacture a screen.
                    const auto& plan = amiga_bootstrap->plan;
                    const auto& handoff = amiga_bootstrap->opaque_invocation_boundary;
                    const auto& resident = amiga_bootstrap->resident_evidence.entry;
                    const auto& evidence = amiga_bootstrap->resident_evidence;
                    std::ostringstream ranges;
                    ranges << "ADF+0x" << std::hex << plan.first_stage.disk_offset
                           << "/0x" << plan.first_stage.length << " -> RAM 0x"
                           << plan.first_stage.destination << "; ADF+0x"
                           << plan.resident_stage.disk_offset << "/0x"
                           << plan.resident_stage.length << " -> RAM 0x"
                           << plan.resident_stage.destination;
                    draw_text(renderer, 64, 292, ranges.str());
                    std::ostringstream entry;
                    entry << "A3: 0x" << handoff.first_stage_invocation_address << " -> 0x"
                          << handoff.first_stage_target << "; 0x"
                          << handoff.resident_stage_jump_address << " -> 0x"
                          << handoff.resident_stage_target << "; entry 0x"
                          << resident.entry_address << " -> 0x" << resident.initializer_address
                          << "; [0x" << resident.result_word_address << "]";
                    draw_text(renderer, 64, 308, entry.str());
                    std::ostringstream boundaries;
                    boundaries << "Static resident paths: gate 0x" << std::hex
                               << evidence.independent_entry.entry_address << "; opaque ABI JMP 0x"
                               << evidence.post_negative_d3_continuation.terminal_jump_address
                               << " -> 0x" << evidence.post_negative_d3_continuation.terminal_jump_target;
                    draw_text(renderer, 64, 324, boundaries.str());
                    draw_text(renderer, 64, 340,
                        tr("HASH-VALIDATED STATIC EVIDENCE ONLY - NO CALL RETURN OR RUNTIME STATE."));
                } else if (*active_platform == eon::Platform::atari_st) {
                    const auto atari_bootstrap = runtime.millennium_atari_bootstrap_presentation();
                    if (atari_bootstrap) {
                        // The Atari session is just as release-bound as the
                        // Amiga one. Show its locally executed loader facts,
                        // not a substituted DOS image or a fabricated GEMDOS
                        // result.
                        const auto& execution = atari_bootstrap->execution;
                        const auto& fopen = atari_bootstrap->fopen_boundary;
                        const auto& fread = atari_bootstrap->fread_frame_prefix;
                        std::ostringstream loader;
                        loader << tr("BOOT COPY") << " 0x" << std::hex << execution.bss_entry_address
                               << " -> 0x" << execution.target_address
                               << " / " << std::dec << execution.first_copy_longwords
                               << "+" << execution.second_copy_words;
                        draw_text(renderer, 64, 292, loader.str());
                        std::ostringstream open;
                        open << "GEMDOS FOPEN " << fopen.fopen_filename << " / mode 0x"
                             << std::hex << fopen.fopen_access_mode << " / TRAP #1 +0x"
                             << fopen.fopen_trap_offset;
                        draw_text(renderer, 64, 308, open.str());
                        std::ostringstream read;
                        read << "FREAD 0x" << std::hex << fread.byte_count_argument
                             << " -> 0x" << fread.buffer_address << " / TRAP #1 +0x"
                             << fread.stop_before_trap_offset;
                        draw_text(renderer, 64, 324, read.str());
                        draw_text(renderer, 64, 340,
                            tr("HASH-VALIDATED STATIC EVIDENCE ONLY - NO CALL RETURN OR RUNTIME STATE."));
                    }
                }
                draw_text(renderer, 64, 680, request.game ? tr("ESC: QUIT") : tr("ESC: BACK TO MENU"));
            } else if (selected == eon::Game::deuteros && active_platform
                && *active_platform == eon::Platform::atari_st) {
                draw_text(renderer, 64, 220,
                    tr("VERIFIED DEUTEROS ATARI ST MEDIA - PROTECTED BOOT CHAIN ONLY"));
                draw_text(renderer, 64, 244,
                    tr("INTERACTIVE ATARI ST PRESENTATION IS NOT YET RECOVERED."));
                draw_text(renderer, 64, 268,
                    tr("NO AMIGA PREVIEW OR SYNTHETIC STATE WILL RUN FOR THIS PLATFORM."));
                const auto atari_bootstrap = runtime.deuteros_atari_bootstrap_presentation();
                if (atari_bootstrap) {
                    // This panel consumes only byte-validated facts retained by
                    // the admitted session.  In particular, it neither selects
                    // a protected boot state nor crosses the XBIOS/raw-read
                    // boundary to fabricate an interactive presentation.
                    const auto& copy = atari_bootstrap->copy_execution;
                    const auto& entry = atari_bootstrap->entry_execution;
                    std::ostringstream stages;
                    stages << tr("BOOT STAGES") << ": SHA-256 "
                           << atari_bootstrap->checkpoint.first_stage_sha256.substr(0, 16)
                           << " / " << atari_bootstrap->checkpoint.second_stage_sha256.substr(0, 16)
                           << "; +0x" << std::hex << atari_bootstrap->first_stage_disk_offset << "/0x"
                           << atari_bootstrap->first_stage_length;
                    draw_text(renderer, 64, 292, stages.str());
                    std::ostringstream relocation;
                    relocation << tr("BOOT COPY") << ": 0x" << std::hex
                               << copy.source_address << " -> 0x" << copy.destination_address
                               << " -> 0x" << copy.relocated_entry_address;
                    draw_text(renderer, 64, 308, relocation.str());
                    std::ostringstream dispatcher;
                    dispatcher << tr("ENTRY PREFIX") << ": +0x" << std::hex << entry.join_offset
                               << " -> 0x" << entry.dispatcher_entry
                               << "; " << tr("STOP BEFORE") << " +0x"
                               << entry.stop_before_dispatcher_source_offset;
                    draw_text(renderer, 64, 324, dispatcher.str());
                    draw_text(renderer, 64, 340,
                        tr("STATIC BOOT EVIDENCE ONLY — NO XBIOS, RAW READ, STATE SELECTION, TITLE, OR GAMEPLAY."));
                }
                draw_text(renderer, 64, 680, request.game ? tr("ESC: QUIT") : tr("ESC: BACK TO MENU"));
            } else if (selected == eon::Game::deuteros && preview_texture) {
                // The active opening and its post-handoff title evidence are
                // copied through the native-session firewall.  In particular,
                // this renderer never inspects the coordinator-owned VM or
                // title-stage adapter after a lifecycle transition.
                const auto opening = runtime.deuteros_amiga_opening_presentation();
                const auto title_stage = runtime.deuteros_amiga_title_stage_boundary();
                if (!opening && !title_stage) {
                    draw_text(renderer, 64, 220, request.game ? tr("ESC: QUIT") : tr("ESC: BACK TO MENU"));
                    continue;
                }
                const auto source_tick = opening ? opening->checkpoint.tick
                    : deuteros_preview_source_tick.value_or(0);
                if (opening && (!deuteros_preview_source_tick
                        || *deuteros_preview_source_tick != source_tick)) {
                    deuteros_preview_rgba = opening->rgba_frame;
                    deuteros_preview_source_tick = source_tick;
                    if (deuteros_preview_rgba) {
                        SDL_UpdateTexture(preview_texture, nullptr, deuteros_preview_rgba->data(),
                            eon::DeuterosAmigaFrame::width * 4);
                    }
                }
                const auto& frame = deuteros_preview_rgba;
                if (opening) {
                    draw_text(renderer, 64, 220, tr("AUTHENTIC AMIGA OPENING - ORIGINAL CHANNEL PROGRAM + PALETTE"));
                    draw_text(renderer, 64, 238, tr("HOLD SPACE / ENTER: ORIGINAL INPUT SIGNAL"));
                    draw_text(renderer, 64, 252, tr("PAULA: ORIGINAL PCM + PERIOD + VOLUME (FIRST DMA PASS)"));
                }
                // Machine-state telemetry is deliberately notation-only: it
                // makes the recovered 50 Hz opening observable without
                // naming a title/menu action or creating a host control.
                std::ostringstream opening_provenance;
                if (opening) {
                    opening_provenance << "T=" << opening->checkpoint.tick
                                      << "; VBL=0x" << std::hex << opening->checkpoint.vblank_counter
                                      << "; PAL=" << std::dec << opening->palette_index
                                      << "; CH=" << opening->active_channel_count
                                      << "; $2171e=" << (opening->checkpoint.input_gate ? 1 : 0);
                    draw_text(renderer, 760, 238, opening_provenance.str());
                }
                if (deuteros_title_resource) {
                    std::ostringstream handoff;
                    handoff << std::hex << *deuteros_title_resource;
                    draw_text(renderer, 64, 268, tr("ORIGINAL TITLE HANDOFF: RESOURCE 0x")
                        + handoff.str()
                        + "; "
                        + tr("TITLE-STAGE EXECUTION IS NOT YET RECOVERED; NO TITLE SCREEN IS FABRICATED"));
                }
                if (title_stage) {
                    std::ostringstream provenance;
                    provenance << tr("AUTHENTIC TITLE STAGE READY") << ": ADF +0x" << std::hex
                               << title_stage->stage.disk_offset << "; 0x"
                               << title_stage->stage.length << " -> RAM 0x"
                               << title_stage->stage.destination << "; 0x"
                               << title_stage->stage.entry_address;
                    draw_text(renderer, 64, 284, provenance.str());
                    draw_text(renderer, 64, 298,
                        tr("TITLE-STAGE EXECUTION IS NOT YET RECOVERED; NO TITLE SCREEN IS FABRICATED"));
                    draw_text(renderer, 64, 312,
                        tr("ORIGINAL TITLE STAGE SHA-256: ") + title_stage->original_sha256);
                    // This compact row is machine-state provenance, not
                    // launcher prose or a simulated title display. These
                    // are the only caller-proven title writes before the
                    // unresolved Exec read, followed by its local A7 setup.
                    const auto& prefix_state = title_stage->entry_prefix_state;
                    const auto& exec_prelude = title_stage->exec_prelude;
                    std::ostringstream prefix_provenance;
                    prefix_provenance << "0x" << std::hex << prefix_state.writes[0].address
                                      << ".w=0x" << prefix_state.writes[0].value
                                      << "; 0x" << prefix_state.writes[1].address
                                      << ".b=0x" << prefix_state.writes[1].value
                                      << "; A7=0x" << exec_prelude.stack_pointer_value;
                    draw_text(renderer, 64, 326, prefix_provenance.str());
                    const auto& palette = title_stage->graphics_setup_palette;
                    for (std::size_t index = 0; index < palette.size(); ++index) {
                        const auto& color = palette[index];
                        SDL_SetRenderDrawColor(renderer, color.red, color.green, color.blue, 255);
                        const SDL_FRect swatch{520.0F + static_cast<float>(index) * 14.0F,
                            326.0F, 14.0F, 14.0F};
                        SDL_RenderFillRect(renderer, &swatch);
                        SDL_SetRenderDrawColor(renderer, 205, 225, 235, 255);
                        SDL_RenderRect(renderer, &swatch);
                    }
                }
                if (title_stage && title_stage->alternate_renderer_trace) {
                    const auto& trace = *title_stage->alternate_renderer_trace;
                    draw_text(renderer, 64, title_stage ? 342 : 284, tr("ORIGINAL $20580 STREAM: +0x")
                        + [&] { std::ostringstream stream; stream << std::hex << trace.stream_offset;
                            return stream.str(); }()
                        + " - " + std::to_string(trace.glyph_codes.size()));
                }
                SDL_Texture* texture = preview_texture;
                if (modern) {
                    if (SDL_Texture* external = refresh_deuteros_external_modern_texture(source_tick,
                            title_stage.has_value())) {
                        texture = external;
                        const auto& surface = *deuteros_external_modern_surface;
                        draw_text(renderer, 64, title_stage ? 342 : 284,
                            tr("MODERN") + " " + surface.pack_id + " ("
                            + std::to_string(surface.width) + "x" + std::to_string(surface.height)
                            + "; " + surface.provenance + "; T=1-82)");
                    }
                }
                if (texture == preview_texture && modern
                    && modern_graphics_settings.pixel_reconstruction != PixelReconstruction::off && frame) {
                    if (const auto release = resolve_active_release(eon::Game::deuteros)) {
                        const eon::ModernReconstructionCacheKey requested_key{release->sha256,
                            "deuteros.amiga.opening", source_tick,
                            modern_graphics_settings.pixel_reconstruction};
                        // A new source tick, release or F10 reconstruction mode
                        // has different dimensions/pixels. The pipeline revokes
                        // its CPU surface first; SDL follows that exact key
                        // boundary rather than treating texture existence as
                        // cache validity.
                        if (modern_preview_texture && !deuteros_modern_pipeline.matches(requested_key)) {
                            SDL_DestroyTexture(modern_preview_texture);
                            modern_preview_texture = nullptr;
                        }
                        const auto* enhanced = deuteros_modern_pipeline.resolve(requested_key, *frame,
                            eon::DeuterosAmigaFrame::width, eon::DeuterosAmigaFrame::height);
                        if (enhanced) {
                            if (!modern_preview_texture) {
                                modern_preview_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                    SDL_TEXTUREACCESS_STREAMING, enhanced->width, enhanced->height);
                            }
                            if (modern_preview_texture && SDL_UpdateTexture(modern_preview_texture, nullptr,
                                    enhanced->rgba.data(), enhanced->width * 4)) {
                                texture = modern_preview_texture;
                            } else if (modern_preview_texture) {
                                std::cerr << "Unable to update transient Modern Deuteros texture: "
                                          << SDL_GetError() << '\n';
                            }
                        } else if (deuteros_modern_pipeline.failure()) {
                            std::cerr << "Modern Deuteros reconstruction rejected: "
                                      << *deuteros_modern_pipeline.failure() << '\n';
                        }
                    }
                }
                SDL_SetTextureScaleMode(texture,
                    modern && modern_graphics_settings.smooth_scaling
                        ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
                // Keep the original pixels intact while allowing the extra
                // provenance boundary to remain visible after title handoff.
                const auto preview_bounds = aspect_viewport(64,
                    title_stage ? 350.0F : deuteros_title_resource ? 306.0F : 274.0F,
                    576, title_stage ? 350.0F : 400.0F, modern_graphics_settings);
                if (modern && modern_graphics_settings.frame) draw_modern_surface_frame(renderer, preview_bounds);
                SDL_RenderTexture(renderer, texture, nullptr, &preview_bounds);
                if (modern && modern_graphics_settings.scanlines) draw_scanlines(renderer, preview_bounds);
                if (modern) draw_modern_preset_overlay(renderer, preview_bounds,
                    modern_graphics_settings.preset, modern_graphics_settings.reduced_motion);
                draw_text(renderer, 64, 580, request.game ? tr("ESC: QUIT") : tr("ESC: BACK TO MENU"));
            } else {
                draw_text(renderer, 64, 220, request.game ? tr("ESC: QUIT") : tr("ESC: BACK TO MENU"));
            }
        }
        if (show_modern_graphics_settings) {
            if (show_modern_runtime_diagnostics) {
                const auto diagnostics = current_modern_runtime_diagnostics();
                if (show_recovery_function_map) {
                    draw_recovery_function_map_popup(renderer, diagnostics,
                        recovery_function_map_page, translator);
                } else {
                    draw_modern_runtime_diagnostics_popup(renderer, modern_graphics_settings,
                        diagnostics, translator);
                }
            } else {
                draw_modern_graphics_popup(renderer, modern_graphics_settings,
                    request.presentation == eon::Presentation::modern,
                    modern_pack_admission, translator);
            }
        }
        // The 120-FPS option limits only host presentation.  Recovered
        // sessions continue to derive their timing from their own real-time
        // schedulers above; no frame is synthesized and no original tick is
        // skipped, inserted, or passed through this renderer-only limiter.
        if (modern_graphics_settings.render_pacing == RenderPacing::capped_120) {
            constexpr std::uint64_t presentation_period_ns = 1'000'000'000ULL / 120ULL;
            const auto now_ns = SDL_GetTicksNS();
            if (last_capped_present_ns && now_ns - *last_capped_present_ns < presentation_period_ns) {
                SDL_DelayPrecise(presentation_period_ns - (now_ns - *last_capped_present_ns));
            }
        }
        SDL_RenderPresent(renderer);
        if (modern_graphics_settings.render_pacing == RenderPacing::capped_120) {
            last_capped_present_ns = SDL_GetTicksNS();
        } else {
            last_capped_present_ns.reset();
        }
    }

    for (auto& card : cards) SDL_DestroyTexture(card.texture);
    SDL_DestroyTexture(project_eon_logo_texture);
    if (millennium_title_text_input_active) SDL_StopTextInput(window);
    SDL_DestroyTexture(millennium_preview_texture);
    SDL_DestroyTexture(millennium_modern_preview_texture);
    SDL_DestroyTexture(millennium_external_modern_texture);
    SDL_DestroyTexture(millennium_gx_canvas_texture);
    SDL_DestroyTexture(preview_texture);
    SDL_DestroyTexture(modern_preview_texture);
    SDL_DestroyTexture(deuteros_external_modern_texture);
    SDL_DestroyAudioStream(deuteros_audio_stream);
    active_text_renderer.reset();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
