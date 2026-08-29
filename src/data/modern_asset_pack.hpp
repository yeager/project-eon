#pragma once

#include "platform/game_data.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
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

} // namespace eon
