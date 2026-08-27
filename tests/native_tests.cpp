#include "platform/game_data.hpp"
#include "launcher.hpp"
#include "data/zip_archive.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>

int main() {
    const std::filesystem::path data_directory = EON_REAL_DATA_DIR;
    if (data_directory.empty() || !std::filesystem::is_directory(data_directory)) {
        std::cout << "SKIP: configure -DEON_REAL_DATA_DIR=<original archive directory>\n";
        return 0;
    }
    const auto releases = eon::find_release_archives(data_directory);
    // Six genuine outer archives: five platform/game pairs plus Spanish DOS.
    assert(releases.size() == 6);
    std::size_t millennium = 0;
    std::size_t deuteros = 0;
    for (const auto& release : releases) {
        release.game == eon::Game::millennium ? ++millennium : ++deuteros;
    }
    assert(millennium == 4);
    assert(deuteros == 2);
    assert(eon::release_available(releases, eon::Game::millennium, eon::Platform::dos));
    assert(eon::release_available(releases, eon::Game::deuteros, eon::Platform::amiga));
    assert(!eon::release_available(releases, eon::Game::deuteros, eon::Platform::dos));

    std::size_t asset_count = 0;
    std::set<std::string> asset_hashes;
    std::map<eon::AssetKind, std::size_t> kind_counts;
    for (const auto& release : releases) {
        const auto assets = eon::inventory_zip(release.path);
        asset_count += assets.size();
        for (const auto& asset : assets) {
            asset_hashes.insert(asset.sha256);
            ++kind_counts[asset.kind];
        }
    }
    assert(asset_count == 67);
    // Genuine clean Deuteros Amiga disk 1 and Spanish Millennium DOS image.
    assert(asset_hashes.contains("6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38"));
    assert(asset_hashes.contains("1cb7d399ab22110317b1c7486a575c00895f12a17268d0c984ac264a5695961d"));
    assert(kind_counts[eon::AssetKind::amiga_adf] == 17);
    assert(kind_counts[eon::AssetKind::atari_st_disk] == 18);
    assert(kind_counts[eon::AssetKind::dos_floppy_image] == 1);
    assert(kind_counts[eon::AssetKind::dos_flat_executable] == 3);
    assert(kind_counts[eon::AssetKind::dos_com_program] == 1);
    assert(kind_counts[eon::AssetKind::audio] == 14);
    assert(kind_counts[eon::AssetKind::game_resource] == 12);
    assert(kind_counts[eon::AssetKind::unknown] == 1);
    return 0;
}
