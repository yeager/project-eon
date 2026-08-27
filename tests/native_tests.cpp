#include "platform/game_data.hpp"
#include "launcher.hpp"
#include "engine/deuteros_amiga_opening.hpp"
#include "data/zip_archive.hpp"
#include "data/amiga_adf.hpp"
#include "data/amiga_ofs.hpp"
#include "data/creative_voice.hpp"
#include "data/deuteros_amiga_bundle.hpp"
#include "data/deuteros_amiga_channel_vm.hpp"
#include "data/deuteros_amiga_frame.hpp"
#include "data/deuteros_amiga_loader.hpp"
#include "data/fat12.hpp"
#include "data/millennium_dos_bitmap.hpp"
#include "data/millennium_dos_lib.hpp"
#include "data/millennium_dos_title_flow.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <map>
#include <stdexcept>
#include <set>
#include <span>

int main() {
    {
        char program[] = "project-eon";
        char* args[] = {program};
        const auto defaults = eon::parse_command_line(1, args);
        assert(defaults.request && defaults.request->data_directory_is_default);
        assert(!defaults.request->data_directory.empty());
        char data_option[] = "--data";
        char custom_path[] = "original-media";
        char* explicit_args[] = {program, data_option, custom_path};
        const auto explicit_data = eon::parse_command_line(3, explicit_args);
        assert(explicit_data.request && !explicit_data.request->data_directory_is_default);
        assert(explicit_data.request->data_directory == "original-media");
    }
    const std::filesystem::path data_directory = EON_REAL_DATA_DIR;
    if (data_directory.empty() || !std::filesystem::is_directory(data_directory)) {
        std::cout << "SKIP: configure -DEON_REAL_DATA_DIR=<original archive directory>\n";
        return 0;
    }
    eon::ReleaseScanner incremental_scanner(data_directory);
    assert(incremental_scanner.candidate_count() >= 6);
    assert(!incremental_scanner.done());
    const auto scanned_before = incremental_scanner.scanned_count();
    static_cast<void>(incremental_scanner.advance(1));
    assert(incremental_scanner.scanned_count() == scanned_before + 1);
    while (!incremental_scanner.advance(2)) {
    }
    assert(incremental_scanner.done());
    assert(incremental_scanner.releases().size() == 6);
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

    const auto amiga_millennium = std::find_if(releases.begin(), releases.end(), [](const auto& release) {
        return release.game == eon::Game::millennium && release.platform == eon::Platform::amiga;
    });
    assert(amiga_millennium != releases.end());
    // Razor is a genuine supplied 880 KiB DOS\0 image whose standard root
    // block is intact. Other supplied crack variants replace this block with
    // game code, so only this one is evidence for the filesystem reader.
    const auto razor_adf = eon::extract_asset_by_sha256(amiga_millennium->path,
        "fe83c10119ef9bf2953b6fcd9a13d07f2c276215aaa64e2e541402a527a616f2");
    assert(razor_adf && razor_adf->size() == eon::AmigaAdf::standard_size);
    assert(eon::to_hex(eon::sha256(*razor_adf))
        == "fe83c10119ef9bf2953b6fcd9a13d07f2c276215aaa64e2e541402a527a616f2");
    const eon::AmigaAdf razor_disk(*razor_adf);
    const eon::AmigaOfs razor_filesystem(razor_disk);
    assert(razor_filesystem.root_block() == 880);
    assert(razor_filesystem.volume_name() == "Millennium (Crack Razor)");
    assert(razor_filesystem.entries().empty());
    const auto defjam_adf = eon::extract_asset_by_sha256(amiga_millennium->path,
        "8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c");
    assert(defjam_adf && defjam_adf->size() == eon::AmigaAdf::standard_size);
    bool rejected_non_filesystem = false;
    try {
        const eon::AmigaAdf defjam_disk(*defjam_adf);
        static_cast<void>(eon::AmigaOfs(defjam_disk));
    } catch (const std::runtime_error&) {
        rejected_non_filesystem = true;
    }
    assert(rejected_non_filesystem);

    const auto english_dos = std::find_if(releases.begin(), releases.end(), [](const auto& release) {
        return release.game == eon::Game::millennium
            && release.platform == eon::Platform::dos && release.language == "en";
    });
    assert(english_dos != releases.end());
    eon::ReleaseScanner direct_archive_scanner(english_dos->path);
    assert(direct_archive_scanner.candidate_count() == 1);
    assert(direct_archive_scanner.advance());
    assert(direct_archive_scanner.releases().size() == 1);
    assert(direct_archive_scanner.releases().front().sha256 == english_dos->sha256);
    const auto sfx1_bytes = eon::extract_asset_by_sha256(english_dos->path,
        "5f796a7fe8bcf5113a65087f76853061f8d96065f9a3cbe66b6c61303b677a88");
    assert(sfx1_bytes);
    const auto sfx1 = eon::decode_creative_voice(*sfx1_bytes);
    assert(sfx1.sample_rate == 10'000);
    assert(sfx1.unsigned_pcm.size() == 738);
    assert(eon::to_hex(eon::sha256(sfx1.unsigned_pcm))
        == "811de4108fe6551e09da1865f3ff2e18a8313aad30a6916210c4d5d49b1e1c06");
    const auto title_bytes = eon::extract_asset_by_sha256(english_dos->path,
        "6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678");
    const auto gx_bytes = eon::extract_asset_by_sha256(english_dos->path,
        "4adf9991226deab4749ac07ad637851994f57d11f6dc45f3f5ce862b5bc34c2f");
    assert(title_bytes && gx_bytes);
    assert(title_bytes->size() == 18'907);
    assert(gx_bytes->size() == 312'748);
    const eon::MillenniumDosLib title_lib(*title_bytes);
    const eon::MillenniumDosLib gx_lib(*gx_bytes);
    assert(title_lib.directory_offset() == 0x4813);
    assert(title_lib.entries().size() == 38);
    assert(title_lib.entries().front().name == "P00");
    assert(title_lib.entries().front().offset == 6);
    assert(title_lib.entries().front().size == 10'555);
    const auto* title_p01 = title_lib.find("p01");
    const auto* title_p25 = title_lib.find("P25");
    assert(title_p01 && title_p01->offset == 0x2941 && title_p01->size == 213);
    assert(title_p25 && title_p25->offset == 0x473e && title_p25->size == 213);
    assert(eon::to_hex(eon::sha256(title_lib.read(title_lib.entries().front())))
        == "14ca6d3c86eba5e9e2afaed21fca9fc6dd1da9e357e305859a495b6dfd69919d");
    const auto title_bitmap = eon::decode_millennium_dos_bitmap(
        title_lib.read(title_lib.entries().front()));
    assert(title_bitmap.flags == 7);
    assert(title_bitmap.max_palette_index == 35);
    assert(title_bitmap.codec == 2);
    assert(title_bitmap.deduction == 0);
    assert(title_bitmap.width == 320);
    assert(title_bitmap.height == 200);
    assert(title_bitmap.encoded_span == 9'687);
    assert(title_bitmap.pixels.size() == 64'000);
    assert(*std::max_element(title_bitmap.pixels.begin(), title_bitmap.pixels.end()) == 35);
    assert(std::count_if(title_bitmap.pixels.begin(), title_bitmap.pixels.end(),
        [](std::uint8_t value) { return value != 0; }) == 7'386);
    assert(eon::to_hex(eon::sha256(title_bitmap.pixels))
        == "85ec11c9f943672df2ba2a4e2837ce1f3158d61648ec07bcdc84b381bd24f4ee");
    const auto title_palette = eon::decode_millennium_dos_palette(
        title_lib.read(title_lib.entries().front()), title_bitmap);
    assert(title_palette.logical_to_dac.size() == 36);
    assert(title_palette.auxiliary_translation.size() == 36);
    assert(eon::to_hex(eon::sha256(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(title_palette.dac_rgb6.data()), 768)))
        == "b6dd34314102e429fdd98390b1fda27d3ea94d16bfcefa2983e3e319a2a20eae");
    assert(eon::to_hex(eon::sha256(title_palette.logical_to_dac))
        == "cd7a7f81dd75249a8669e0f4c1792d99b37f3ea28c54319a3f2e84b4a86ff3e2");
    assert(eon::to_hex(eon::sha256(title_palette.auxiliary_translation))
        == "652ea21cfa18c27470daaee4521d863a3d377f803a5f80ba0132af49b24083d4");
    assert(title_palette.logical_to_dac[0] == 0x00);
    assert(title_palette.logical_to_dac[5] == 0xff);
    assert(title_palette.logical_to_dac[35] == 0x11);
    assert((title_palette.dac_rgb6[0xff] == std::array<std::uint8_t, 3>{0x3f, 0x3f, 0x3f}));
    assert((title_palette.dac_rgb6[0xee] == std::array<std::uint8_t, 3>{0x00, 0x28, 0x1c}));
    const auto title_rgba = eon::colorize_millennium_dos_bitmap(title_bitmap, title_palette);
    assert(title_rgba.size() == 320U * 200U * 4U);
    assert(eon::to_hex(eon::sha256(title_rgba))
        == "500a1451ab435a9c8ffaf1dbfaacee52cca0e32b375c883a45dd8f879a952888");
    const auto titles_bytes = eon::extract_asset_by_sha256(english_dos->path,
        "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6");
    const auto mill_bytes = eon::extract_asset_by_sha256(english_dos->path,
        "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e");
    assert(titles_bytes && mill_bytes);
    const auto title_flow = eon::parse_millennium_dos_title_flow(*titles_bytes, *mill_bytes);
    assert(title_flow.title_entry_address == 0x1b80);
    assert(title_flow.title_resource_index == 0);
    assert(title_flow.intro_transition_steps == 37);
    assert(title_flow.intro_step_stride == 0x170);
    assert(title_flow.input_interrupt == 0x21);
    assert(title_flow.input_service == 0x06);
    assert(title_flow.input_parameter == 0xff);
    assert(title_flow.exit_code == 0);
    assert(title_flow.launcher_title_offset == 0x58f);
    assert(title_flow.launcher_game_offset == 0x59a);
    assert(title_flow.launcher_title_program == "TITLES.EXE");
    assert(title_flow.launcher_game_program == "2200ad.exe");
    assert(gx_lib.directory_offset() == 0x4bd3c);
    assert(gx_lib.entries().size() == 180);
    assert(gx_lib.entries().front().name == "IMG00");
    assert(gx_lib.entries().front().offset == 6);
    assert(gx_lib.entries().front().size == 3'461);
    const auto* gx_img01 = gx_lib.find("IMG01");
    const auto* gx_imgb3 = gx_lib.find("IMGB3");
    assert(gx_img01 && gx_img01->offset == 0x0d8b && gx_img01->size == 14'079);
    assert(gx_imgb3 && gx_imgb3->offset == 0x4bb83 && gx_imgb3->size == 441);
    assert(eon::to_hex(eon::sha256(gx_lib.read(*gx_imgb3)))
        == "333c18a883b85c9cefe1072cd44b0a6bc51375ec5f63b87f42106e25dfb6f907");

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
    // $21982 writes profile one before returning to the bootstrap. Its table
    // routine supplies these exact raw-track load constants.
    assert(load_plan.title_handoff_profile.disk_offset == 0x6e000);
    assert(load_plan.title_handoff_profile.length == 0x6ca00);
    assert(load_plan.title_handoff_profile.destination == 0x13000);
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
    const auto first_bitmap = eon::decode_deuteros_amiga_bitmap(
        system_disk, first_bundle, first_indexed_blob, 1);
    assert(first_bitmap.width == 48);
    assert(first_bitmap.height == 17);
    assert(first_bitmap.color_indices.size() == 816);
    assert(std::count_if(first_bitmap.color_indices.begin(), first_bitmap.color_indices.end(),
        [](auto color) { return color != 0; }) == 311);
    assert(eon::to_hex(eon::sha256(first_bitmap.color_indices))
        == "fca175276cfe376b85e936f455aa9e89d1a0d4c89a61d2b6ce317fa6aa58a6a3");
    const auto first_special_bitmap = eon::decode_deuteros_amiga_bitmap(
        system_disk, first_bundle, first_indexed_blob, 0);
    assert(first_special_bitmap.width == 112);
    assert(first_special_bitmap.height == 77);
    assert(std::count_if(first_special_bitmap.color_indices.begin(),
        first_special_bitmap.color_indices.end(), [](auto color) { return color != 0; }) == 2'712);
    assert(eon::to_hex(eon::sha256(first_special_bitmap.color_indices))
        == "a7abcb6a308f7016e28611862a14de3adfa12881efcce458e5888b07e2d0c1cb");
    eon::DeuterosAmigaOpening live_opening(*amiga_disk1);
    static_cast<void>(live_opening.tick());
    const auto live_tick2 = live_opening.tick();
    assert(live_tick2.palette == 1);
    static_cast<void>(live_opening.tick());
    const auto live_frame = live_opening.rgba_frame();
    assert(live_frame && live_frame->size() == 320U * 200U * 4U);
    assert(live_opening.frame_composed_on_last_tick());
    assert(live_opening.ticks() == 3);
    assert(live_opening.title_handoff_profile().disk_offset == 0x6e000);
    eon::DeuterosAmigaChannelVm opening_vm(system_disk, first_bundle);
    assert(opening_vm.channels().size() == 4);
    const auto tick1 = opening_vm.tick();
    assert(!tick1.palette && tick1.sounds.empty());
    const auto tick2 = opening_vm.tick();
    assert(tick2.palette == 1);
    assert(tick2.sounds.size() == 2);
    assert(tick2.sounds[0].sound == 1 && tick2.sounds[0].channels == 1);
    assert(tick2.sounds[1].sound == 2 && tick2.sounds[1].channels == 2);
    assert(opening_vm.input_gate());
    const auto tick3 = opening_vm.tick();
    assert(!tick3.palette && tick3.sounds.empty());
    assert(opening_vm.channels()[0].bitmap_selector == 1);
    assert(opening_vm.channels()[0].x == 8);
    assert(opening_vm.channels()[0].y == 183);
    assert(opening_vm.channels()[0].wait_mode == 3);
    assert(opening_vm.channels()[0].timer == 0);
    const auto tick3_frame = eon::compose_deuteros_amiga_frame(
        system_disk, first_bundle, first_indexed_blob, opening_vm.channels());
    assert(tick3_frame.color_indices.size() == 320 * 200);
    assert(std::count_if(tick3_frame.color_indices.begin(), tick3_frame.color_indices.end(),
        [](auto color) { return color != 0; }) == 311);
    assert(eon::to_hex(eon::sha256(tick3_frame.color_indices))
        == "d841fd0e6e01c09f7dc8ce6cd2bda1828a0eb62c5f198750403aa996cd7d48d4");
    const auto tick3_rgba = eon::colorize_deuteros_amiga_frame(
        tick3_frame, eon::decode_deuteros_amiga_palette(system_disk, first_bundle, 1));
    assert(tick3_rgba.size() == 320 * 200 * 4);
    static_cast<void>(opening_vm.tick());
    assert(opening_vm.channels()[0].y == 181);
    assert(opening_vm.channels()[0].wait_mode == 6);
    assert(opening_vm.channels()[0].timer == 38);
    // Channel 3 is the first opening input waiter: after its verified 0x50
    // tick delay, $14 accepts the already-polled button and immediately
    // installs the real bundle-relative alternate pointer through $0f.
    eon::DeuterosAmigaChannelVm input_vm(system_disk, first_bundle);
    eon::DeuterosAmigaRandom input_random(system_disk, first_bundle);
    eon::DeuterosAmigaVmInputs input_vm_inputs;
    input_vm_inputs.input_pressed = true;
    input_vm_inputs.random_word = [&input_random] { return input_random.next(); };
    std::optional<std::uint32_t> first_input_alternate;
    for (std::size_t tick = 1; tick <= 96; ++tick) {
        const auto events = input_vm.tick(input_vm_inputs);
        input_random.advance_vblank();
        if (!events.alternate_resources.empty()) {
            assert(events.alternate_resources.size() == 1);
            first_input_alternate = events.alternate_resources.front();
            assert(tick == 82);
            break;
        }
    }
    assert(first_input_alternate == 0x0b38);
    // The SDL session uses the same input contract, rather than a separately
    // scripted preview path. Holding the recovered input signal reaches the
    // same verified handoff tick and raw resource pointer.
    eon::DeuterosAmigaOpening live_input_opening(*amiga_disk1);
    std::optional<std::uint32_t> live_input_alternate;
    for (std::size_t tick = 1; tick <= 96; ++tick) {
        const auto events = live_input_opening.tick(true);
        if (!events.alternate_resources.empty()) {
            assert(events.alternate_resources.size() == 1);
            live_input_alternate = events.alternate_resources.front();
            assert(tick == 82);
            break;
        }
    }
    assert(live_input_alternate == 0x0b38);
    eon::DeuterosAmigaRandom opening_random(system_disk, first_bundle, 0, 0x240);
    assert(opening_random.next() == 0x11);
    assert(opening_random.seed() == 0x11);
    opening_random.advance_vblank();
    assert(opening_random.vblank_counter() == 0x244);
    for (std::size_t index = 0; index + 1 < first_indexed_blob.record_offsets.size(); ++index) {
        const auto bitmap = eon::decode_deuteros_amiga_bitmap(
            system_disk, first_bundle, first_indexed_blob, index);
        assert(bitmap.width > 0 && bitmap.height > 0);
        assert(bitmap.color_indices.size()
            == static_cast<std::size_t>(bitmap.width) * bitmap.height);
        assert(std::all_of(bitmap.color_indices.begin(), bitmap.color_indices.end(),
            [](auto color) { return color < 16; }));
    }

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
    for (std::size_t index = 0; index + 1 < second_indexed_blob.record_offsets.size(); ++index) {
        const auto bitmap = eon::decode_deuteros_amiga_bitmap(
            system_disk, second_bundle, second_indexed_blob, index);
        assert(bitmap.width > 0 && bitmap.height > 0);
        assert(bitmap.color_indices.size()
            == static_cast<std::size_t>(bitmap.width) * bitmap.height);
        assert(std::all_of(bitmap.color_indices.begin(), bitmap.color_indices.end(),
            [](auto color) { return color < 16; }));
    }
    return 0;
}
