#include "platform/game_data.hpp"
#include "launcher.hpp"
#include "data/zip_archive.hpp"
#include "data/amiga_adf.hpp"
#include "data/deuteros_amiga_bundle.hpp"
#include "data/deuteros_amiga_loader.hpp"
#include "data/fat12.hpp"
#include "data/sha256.hpp"

#include <algorithm>
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

    const auto spanish = std::find_if(releases.begin(), releases.end(), [](const auto& release) {
        return release.game == eon::Game::millennium
            && release.platform == eon::Platform::dos && release.language == "es";
    });
    assert(spanish != releases.end());
    const auto spanish_zip = eon::ZipArchive::open(spanish->path);
    const auto image_entry = std::find_if(spanish_zip.entries().begin(), spanish_zip.entries().end(),
        [](const auto& entry) { return entry.name == "MRTE.IMG"; });
    assert(image_entry != spanish_zip.entries().end());
    const eon::Fat12Disk disk(spanish_zip.extract(*image_entry));
    assert(disk.bytes_per_sector() == 512);
    assert(disk.sectors_per_cluster() == 2);
    assert(disk.root_entries().size() == 39);
    const auto* executable = disk.find("2200AD.EXE");
    const auto* graphics = disk.find("GX.LIB");
    assert(executable && executable->size == 54'566);
    assert(graphics && graphics->size == 311'420);
    assert(eon::to_hex(eon::sha256(disk.read(*executable)))
        == "9f7d6f28f71eb7f2f6bb48cb3977efbf45049fc74083f8cbc865ec25396330c6");
    assert(eon::to_hex(eon::sha256(disk.read(*graphics)))
        == "e27d1c697da677994e2f864a776f4fc900c7feb4ec4b85500b2bfea3bc834767");

    const auto atari_release = std::find_if(releases.begin(), releases.end(), [](const auto& release) {
        return release.game == eon::Game::millennium && release.platform == eon::Platform::atari_st;
    });
    assert(atari_release != releases.end());
    const auto atari_image = eon::extract_asset_by_sha256(atari_release->path,
        "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7");
    assert(atari_image);
    const eon::Fat12Disk atari_disk(*atari_image);
    assert(atari_disk.root_entries().size() == 13);
    const auto* atari_data = atari_disk.find("DATA12.BIN");
    assert(atari_data && atari_data->size == 932);
    assert(eon::to_hex(eon::sha256(atari_disk.read(*atari_data)))
        == "6f1e8ab7720c530f8cf5bfc07497824ff731ce977a15d941dad5acd999c6eeda");

    const auto deuteros_amiga = std::find_if(releases.begin(), releases.end(), [](const auto& release) {
        return release.game == eon::Game::deuteros && release.platform == eon::Platform::amiga;
    });
    assert(deuteros_amiga != releases.end());
    const auto amiga_disk1 = eon::extract_asset_by_sha256(deuteros_amiga->path,
        "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38");
    const auto amiga_disk2 = eon::extract_asset_by_sha256(deuteros_amiga->path,
        "99909db1e190be02e049084743af44f00e331be6bf2d97b4831ada5fe4c30b4a");
    assert(amiga_disk1 && amiga_disk2);
    const eon::AmigaAdf system_disk(*amiga_disk1);
    const eon::AmigaAdf data_disk(*amiga_disk2);
    assert(system_disk.kind() == eon::AmigaDiskKind::dos);
    assert(data_disk.kind() == eon::AmigaDiskKind::deuteros_data);
    assert(system_disk.identifier() == std::string("DOS\0", 4));
    assert(data_disk.identifier() == std::string("DEU\0", 4));
    assert(system_disk.boot_checksum_valid());
    assert(data_disk.boot_checksum_valid());
    assert(system_disk.root_block() == 880);
    assert(data_disk.root_block() == 880);
    const auto system_root = system_disk.sector(40, 0, 0);
    const auto data_root = data_disk.sector(40, 0, 0);
    assert(system_root[0] == 0x4e && system_root[1] == 0xf9); // JMP $00040426
    assert(data_root[0] == 0x00 && data_root[1] == 0x04
        && data_root[2] == 0xbb && data_root[3] == 0x1a);
    const auto load_plan = eon::parse_deuteros_amiga_load_plan(system_disk);
    assert(load_plan.bootstrap_loader.disk_offset == 0x2c00);
    assert(load_plan.bootstrap_loader.length == 0x1600);
    assert(load_plan.bootstrap_loader.destination == 0x12800);
    assert(load_plan.bootstrap_loader.entry_address == 0x12a4e);
    assert(load_plan.main_stage.disk_offset == 0x5800);
    assert(load_plan.main_stage.length == 0x4200);
    assert(load_plan.main_stage.destination == 0x20000);
    assert(load_plan.main_stage.entry_address == 0x21734);
    assert((load_plan.resource_disk_offsets == std::array<std::uint32_t, 5>{
        0x1b800, 0x4ba00, 0x37000, 0x59600, 0x6e000}));

    const auto first_bundle = eon::parse_deuteros_amiga_bundle(
        system_disk, load_plan.resource_disk_offsets[0]);
    assert(first_bundle.length == 0x2f3f4);
    assert(first_bundle.object_count == 4);
    assert((first_bundle.channel_offsets == std::array<std::uint32_t, 7>{
        0x382, 0x3c, 0x92c, 0xa78, 0, 0, 0}));
    assert((first_bundle.auxiliary_offsets == std::array<std::uint32_t, 6>{
        0xa98, 0, 0xb4b, 0x121b4, 0x122de, 0x1255e}));
    assert(first_bundle.mode_flag == 0);
    const auto first_palette = eon::decode_deuteros_amiga_palette(system_disk, first_bundle, 1);
    assert((first_palette[0] == eon::RgbColor{0x00, 0x00, 0x00}));
    assert((first_palette[1] == eon::RgbColor{0x88, 0x88, 0x66}));
    assert((first_palette[5] == eon::RgbColor{0xaa, 0x66, 0x00}));
    assert((first_palette[9] == eon::RgbColor{0xff, 0xff, 0x00}));
    assert((first_palette[14] == eon::RgbColor{0xff, 0xff, 0xff}));
    assert((first_palette[15] == eon::RgbColor{0xee, 0x44, 0x00}));
    const auto first_channels = eon::parse_deuteros_amiga_channels(system_disk, first_bundle);
    assert(first_channels.size() == 4);
    for (const auto& channel : first_channels) {
        assert(channel.initial_state_0 == 0x00ff0000);
        assert(channel.initial_state_4 == 3);
        assert(channel.initial_state_8 == 1);
    }
    const auto opening_command = eon::decode_deuteros_amiga_channel_command(
        system_disk, first_bundle, first_channels[0].stream_relative_offset);
    assert(opening_command.opcode == 0x13);
    assert(opening_command.operand_count == 0);
    assert(opening_command.encoded_size == 2);
    const auto palette_command = eon::decode_deuteros_amiga_channel_command(
        system_disk, first_bundle, first_channels[1].stream_relative_offset);
    assert(palette_command.opcode == 4);
    assert(palette_command.operand_count == 1);
    assert(palette_command.operands[0] == 1);
    assert(palette_command.encoded_size == 4);
    const auto first_indexed_blob = eon::parse_deuteros_amiga_indexed_blob(system_disk, first_bundle);
    assert(first_indexed_blob.table_relative_offset == 0x122de);
    assert(first_indexed_blob.data_relative_offset == 0x1255e);
    assert(first_indexed_blob.data_size == 0x1ce96);
    assert(first_indexed_blob.record_offsets.size() == 143);
    assert(first_indexed_blob.record_offsets.front() == 0);
    assert(first_indexed_blob.record_offsets[1] == 0x994);
    assert(first_indexed_blob.record_offsets.back() == 0x1ce8e);

    const auto second_bundle = eon::parse_deuteros_amiga_bundle(
        system_disk, load_plan.resource_disk_offsets[1]);
    assert(second_bundle.length == 0x215f0);
    assert(second_bundle.object_count == 6);
    assert((second_bundle.channel_offsets == std::array<std::uint32_t, 7>{
        0x1efa, 0x3c, 0x6a0, 0xd78, 0x153a, 0x19e4, 0}));
    assert((second_bundle.auxiliary_offsets == std::array<std::uint32_t, 6>{
        0x1f4c, 0x22ac, 0, 0, 0x15a92, 0x15c92}));
    assert(second_bundle.mode_flag == 1);
    const auto second_channels = eon::parse_deuteros_amiga_channels(system_disk, second_bundle);
    assert(second_channels.size() == 6);
    for (const auto& channel : second_channels) {
        assert(channel.initial_state_0 == 0x00ff0009);
        assert(channel.initial_state_4 == 0x00c60003);
        assert(channel.initial_state_8 == 1);
    }
    const auto second_command = eon::decode_deuteros_amiga_channel_command(
        system_disk, second_bundle, second_channels[0].stream_relative_offset);
    assert(second_command.opcode == 4);
    assert(second_command.operands[0] == 0x10);
    const auto second_indexed_blob = eon::parse_deuteros_amiga_indexed_blob(system_disk, second_bundle);
    assert(second_indexed_blob.table_relative_offset == 0x15a92);
    assert(second_indexed_blob.data_relative_offset == 0x15c92);
    assert(second_indexed_blob.data_size == 0xb95e);
    assert(second_indexed_blob.record_offsets.size() == 75);
    assert(second_indexed_blob.record_offsets.front() == 0);
    assert(second_indexed_blob.record_offsets[1] == 0xf2e);
    assert(second_indexed_blob.record_offsets.back() == 0xb956);
    return 0;
}
