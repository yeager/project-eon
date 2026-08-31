#pragma once

#include "platform/game_data.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace eon {

// A Modern asset pack is deliberately an external, separately installed
// presentation layer.  This declaration is admission metadata only: it does
// not decode the asset, alter a supplied archive, or grant a renderer any
// authority to change game logic or save bytes.
struct ModernAssetPackAsset {
    std::string id;
    std::filesystem::path path;
    std::uintmax_t size = 0;
    std::string sha256;
};

struct ModernAssetPack {
    std::filesystem::path manifest_path;
    std::string id;
    std::string version;
    std::string license;
    std::string provenance;
    Game game = Game::millennium;
    Platform platform = Platform::dos;
    std::string source_release_sha256;
    std::vector<ModernAssetPackAsset> assets;
};

struct ModernAssetPackValidation {
    std::filesystem::path manifest_path;
    ModernAssetPack pack;
    std::string error;

    [[nodiscard]] bool accepted() const { return error.empty(); }
};

struct ModernAssetPackRendererTargets {
    bool millennium_dos_title_640x400 = false;
    bool millennium_dos_title_1280x800 = false;
    std::size_t deuteros_amiga_opening_640x400_frames = 0;
    std::size_t deuteros_amiga_opening_1280x800_frames = 0;
};

[[nodiscard]] ModernAssetPackRendererTargets modern_asset_pack_renderer_targets(
    const ModernAssetPack& pack);

struct ModernAssetPackPreflight {
    bool accepted = false;
    std::string pack_id;
    std::string provenance;
    ModernAssetPackRendererTargets targets;
    std::string error;
};

// Validate one explicitly chosen manifest against an already hash-verified
// release identity. This is read-only and does not decode, select, or render
// assets; callers must revalidate again at actual texture use.
[[nodiscard]] ModernAssetPackPreflight preflight_modern_asset_pack(
    const std::filesystem::path& manifest_path, Game game, Platform platform,
    std::string_view source_release_sha256);

// The first renderable v1 target is intentionally narrow: a 640x400 RGBA PNG
// replacement for the recovered Millennium DOS English P00 title. This is a
// renderer input only, not an original resource decoder.
struct ModernAssetPackPngSurface {
    std::string pack_id;
    std::string provenance;
    // The exact, documented renderer target selected from the manifest. It
    // exists for visible Modern provenance, never as an original resource
    // name or instruction to alter game state.
    std::string asset_id;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> png;
};

// The sole finite Deuteros Amiga route currently eligible for regenerated
// opening art.  Frame zero does not exist: each source frame is composed
// after one verified 50 Hz VM tick.  A held original input reaches the
// recovered terminal handoff on tick 82.
inline constexpr std::size_t deuteros_amiga_held_opening_frame_count = 82;

// Admission metadata for one complete, same-resolution external sequence.
// It owns no decoded pixels and does not alter the original opening VM.  Each
// selected frame is rechecked immediately before its renderer upload.
struct ModernAssetPackDeuterosAmigaOpeningSequence {
    std::filesystem::path pack_root;
    std::string pack_id;
    std::string provenance;
    std::string source_release_sha256;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::array<ModernAssetPackAsset, deuteros_amiga_held_opening_frame_count> frames;
};

// Validate one exact pack.eonmodern manifest and its declared asset bytes.
// The function is read-only and does not create a default directory, cache,
// extraction directory, texture, or save file.  A successful result merely
// makes a pack eligible for a future explicit Modern renderer selection.
[[nodiscard]] ModernAssetPackValidation validate_modern_asset_pack(
    const std::filesystem::path& manifest_path);

// Discover direct child packs below a user-selected Modern-pack root.  Each
// candidate is <root>/<pack-id>/pack.eonmodern; symlinked candidates are
// rejected.  Discovery is non-recursive so a pack cannot make Eon traverse
// arbitrary data trees, and it has no game-data fallback or write side effect.
[[nodiscard]] std::vector<ModernAssetPackValidation> discover_modern_asset_packs(
    const std::filesystem::path& root);

// Read the one currently-supported renderer target from a manifest selected
// explicitly by the user. The supplied release hash is reverified original
// launch identity. No directories, caches, archives, game-state, or original
// bytes are written.
[[nodiscard]] ModernAssetPackPngSurface load_millennium_dos_title_modern_surface(
    const std::filesystem::path& manifest_path, std::string_view source_release_sha256);

// Admit exactly all 82 frames for the currently recovered Deuteros Amiga
// held-input opening route. A pack must provide every frame in at least one
// tier; an optional second tier must also be complete. Gaps fail closed.
[[nodiscard]] ModernAssetPackDeuterosAmigaOpeningSequence
load_deuteros_amiga_held_opening_modern_sequence(
    const std::filesystem::path& manifest_path, std::string_view source_release_sha256);

// Load one already-admitted sequence frame for a source VM tick in [1, 82].
// The file is rechecked without following symlinks before its bytes are
// returned, and no cache, extraction directory, or original-media write is
// created.
[[nodiscard]] ModernAssetPackPngSurface load_deuteros_amiga_held_opening_modern_frame(
    const ModernAssetPackDeuterosAmigaOpeningSequence& sequence,
    std::uint64_t source_tick);

} // namespace eon
