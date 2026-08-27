#include "platform/game_data.hpp"
#include "launcher.hpp"
#include "engine/deuteros_amiga_opening.hpp"
#include "data/zip_archive.hpp"
#include "data/amiga_adf.hpp"
#include "data/atari_st_prg.hpp"
#include "data/amiga_ofs.hpp"
#include "data/creative_voice.hpp"
#include "data/deuteros_amiga_bundle.hpp"
#include "data/deuteros_amiga_audio.hpp"
#include "engine/deuteros_amiga_paula.hpp"
#include "data/deuteros_amiga_channel_vm.hpp"
#include "data/deuteros_amiga_frame.hpp"
#include "data/deuteros_amiga_loader.hpp"
#include "data/deuteros_amiga_title_stage.hpp"
#include "data/deuteros_atari_boot.hpp"
#include "data/fat12.hpp"
#include "data/millennium_dos_bitmap.hpp"
#include "data/millennium_dos_game_data.hpp"
#include "data/millennium_dos_game_flow.hpp"
#include "data/millennium_dos_gameplay_screen.hpp"
#include "data/millennium_dos_last_screen.hpp"
#include "data/millennium_amiga_loader.hpp"
#include "data/millennium_dos_lib.hpp"
#include "data/millennium_dos_title_flow.hpp"
#include "engine/millennium_dos_title_session.hpp"
#include "engine/millennium_dos_game_session.hpp"
#include "engine/millennium_dos_save_session.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
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
    bool rejected_patched_loader = false;
    try {
        static_cast<void>(eon::parse_millennium_amiga_load_plan(razor_disk));
    } catch (const std::runtime_error&) {
        rejected_patched_loader = true;
    }
    assert(rejected_patched_loader);
    const auto defjam_adf = eon::extract_asset_by_sha256(amiga_millennium->path,
        "8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c");
    assert(defjam_adf && defjam_adf->size() == eon::AmigaAdf::standard_size);
    // Millennium's supplied images keep game media in raw ranges. The plan is
    // recovered from Defjam's original first-stage 68000 loader without
    // unpacking the ADF.
    const eon::AmigaAdf defjam_loader_disk(*defjam_adf);
    const auto defjam_plan = eon::parse_millennium_amiga_load_plan(defjam_loader_disk);
    assert(defjam_plan.bootstrap_loader.disk_offset == 0x400);
    assert(defjam_plan.bootstrap_loader.length == 0x400);
    assert(defjam_plan.bootstrap_loader.destination == 0x70000);
    assert(defjam_plan.first_stage.disk_offset == 0x24200);
    assert(defjam_plan.first_stage.length == 0x6e000);
    assert(defjam_plan.first_stage.destination == 0x41000);
    assert(defjam_plan.resident_stage.disk_offset == 0x16400);
    assert(defjam_plan.resident_stage.length == 0x2c000);
    assert(defjam_plan.resident_stage.destination == 0x68000);
    assert(defjam_plan.resident_entry == 0x68000);
    assert(defjam_plan.loader_magic == 0xa8d398fb);
    // These source-range fingerprints make the raw loader trace reproducible
    // without treating either range as an extracted game file.
    assert(defjam_plan.bootstrap_loader.raw_sha256
        == "c31e59f83d6825a2da7a6fd5e3297a322993b0483105794fca449d97d3861e06");
    assert(defjam_plan.first_stage.raw_sha256
        == "5ed30d5fe99c0dfc905bbe639d626be558f022514c83bc5ff287ad91014ccf7a");
    assert(defjam_plan.resident_stage.raw_sha256
        == "d144abc05f891710dc99b30d87f020bd6e2ff7796ef86a847f07b8d97d55d18e");
    const auto defjam_resident = eon::parse_millennium_amiga_resident_entry(
        defjam_loader_disk, defjam_plan);
    assert(defjam_resident.entry_address == 0x68000);
    assert(defjam_resident.initializer_address == 0x787d4);
    assert(defjam_resident.result_word_address == 0x7b75a);
    assert(defjam_resident.d3_nonzero_or_mask == 0x0100);
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
    eon::MillenniumDosTitleSession title_session(title_flow);
    assert(!title_session.handed_off());
    assert(!title_session.poll_console(false));
    assert(!title_session.handed_off());
    assert(title_session.poll_console(true));
    assert(title_session.handed_off());
    assert(!title_session.poll_console(true));
    const auto game_executable = eon::extract_asset_by_sha256(english_dos->path,
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57");
    assert(game_executable && game_executable->size() == 54'391);
    const auto game_flow = eon::parse_millennium_dos_game_flow(*game_executable);
    assert(game_flow.entry_address == 0xd2b0);
    assert(game_flow.main_loop_address == 0xd3d2);
    assert(game_flow.action_poll_address == 0x10f05);
    assert(game_flow.special_action_0 == 0x0b && game_flow.special_action_1 == 0x0c);
    assert(game_flow.function_key_first_action == 0x3b && game_flow.function_key_count == 10);
    assert(game_flow.function_key_table_address == 0x2fbf);
    assert(game_flow.function_key_table_stride == 8);
    assert(game_flow.function_key_dispatch_address == 0x76f0);
    assert(game_flow.first_function_key.handler_address == 0x6f9a);
    assert(game_flow.first_function_key.selector_address == 0xda1f);
    assert(game_flow.first_function_key.selector_value == 0);
    assert(game_flow.first_function_key.record_pointer_table_address == 0x27c4);
    assert(game_flow.first_function_key.selected_record_address == 0x12cc);
    assert(game_flow.first_function_key.screen_descriptor_address == 0x300f);
    assert(game_flow.first_function_key.screen_descriptor_mode == 7);
    assert(game_flow.first_function_key.selected_record_byte_2 == 0x11);
    assert(game_flow.first_function_key.selected_record_byte_36 == 0);
    assert(game_flow.second_function_key.handler_address == 0x71ca);
    assert(game_flow.second_function_key.availability_address == 0xda26);
    assert(game_flow.second_function_key.minimum_availability == 2);
    assert(game_flow.second_function_key.wait_call_address == 0x9fa);
    assert(game_flow.second_function_key.callback_slot_address == 0x6f98);
    assert(game_flow.second_function_key.callback_address == 0x7221);
    assert(game_flow.second_function_key.first_record_address == 0x1384);
    assert(game_flow.second_function_key.record_stride == 0x00c0);
    assert(game_flow.second_function_key.record_list_address == 0x6e99);
    assert(game_flow.second_function_key.list_mode_address == 0x6e98);
    assert(game_flow.second_function_key.list_mode_value == 1);
    assert(game_flow.third_function_key.handler_address == 0x6faa);
    assert(game_flow.third_function_key.initialization_guard_address == 0xa19e);
    assert(game_flow.third_function_key.availability_address == 0xda27);
    assert(game_flow.third_function_key.wait_call_address == 0x09fa);
    assert(game_flow.third_function_key.callback_slot_address == 0x6f98);
    assert(game_flow.third_function_key.callback_address == 0x712a);
    assert(game_flow.third_function_key.list_mode_address == 0x6e98);
    assert(game_flow.third_function_key.list_mode_value == 0);
    assert(game_flow.third_function_key.source_far_pointer_address == 0x0112);
    assert(game_flow.third_function_key.list_address == 0x6e99);
    assert(game_flow.fourth_function_key.handler_address == 0x72f9);
    assert(game_flow.fourth_function_key.initialization_guard_address == 0xa19e);
    assert(game_flow.fourth_function_key.transfer_al_value == 2);
    assert(game_flow.fourth_function_key.common_routine_address == 0xba5e);
    assert(game_flow.fourth_function_key.first_call_address == 0x4d2c);
    assert(game_flow.fourth_function_key.first_runtime_byte_address == 0xda13);
    assert(game_flow.fourth_function_key.first_runtime_byte_value == 7);
    assert(game_flow.fourth_function_key.second_call_address == 0x9dd5);
    assert(game_flow.fourth_function_key.second_runtime_byte_address == 0xda1e);
    assert(game_flow.fourth_function_key.second_runtime_byte_value == 9);
    assert(game_flow.fourth_function_key.third_runtime_byte_address == 0x75a9);
    assert(game_flow.fourth_function_key.third_runtime_byte_value == 0);
    assert(game_flow.fifth_function_key.handler_address == 0x7597);
    assert(game_flow.fifth_function_key.transfer_al_value == 2);
    assert(game_flow.fifth_function_key.first_call_address == 0xbe28);
    assert(game_flow.fifth_function_key.second_call_address == 0x10b9d);
    assert(game_flow.fifth_function_key.third_call_address == 0x14bf7);
    assert(game_flow.fifth_function_key.fourth_call_address == 0x10b76);
    assert(game_flow.sixth_function_key.handler_address == 0x7415);
    assert(game_flow.sixth_function_key.initialization_guard_address == 0xa19e);
    assert(game_flow.sixth_function_key.display_selector_call_address == 0xd0c9);
    assert(game_flow.sixth_function_key.command_value == 0x0022);
    assert(game_flow.sixth_function_key.first_call_address == 0x4d2c);
    assert(game_flow.sixth_function_key.second_call_address == 0xc980);
    assert(game_flow.sixth_function_key.saved_first_byte_address == 0x7412);
    assert(game_flow.sixth_function_key.first_byte_address == 0x75a8);
    assert(game_flow.sixth_function_key.saved_second_byte_address == 0x740f);
    assert(game_flow.sixth_function_key.second_byte_address == 0x75ae);
    assert(game_flow.sixth_function_key.saved_word_address == 0x7410);
    assert(game_flow.sixth_function_key.word_address == 0x75ac);
    assert(game_flow.sixth_function_key.first_byte_value == 0x0c);
    assert(game_flow.sixth_function_key.second_byte_value == 0);
    assert(game_flow.sixth_function_key.callback_word_value == 0x3207);
    assert(game_flow.sixth_function_key.callback_word_address == 0x75a6);
    assert(game_flow.sixth_function_key.wait_call_address == 0x09fa);
    auto altered_f2_gate = *game_executable;
    altered_f2_gate[0x71ca - 0x100] ^= 0x01;
    bool rejected_altered_f2_gate = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_game_flow(altered_f2_gate));
    } catch (const std::runtime_error&) {
        rejected_altered_f2_gate = true;
    }
    assert(rejected_altered_f2_gate);
    auto altered_f3_gate = *game_executable;
    altered_f3_gate[0x6faa - 0x100] ^= 0x01;
    bool rejected_altered_f3_gate = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_game_flow(altered_f3_gate));
    } catch (const std::runtime_error&) {
        rejected_altered_f3_gate = true;
    }
    assert(rejected_altered_f3_gate);
    auto altered_f4_common = *game_executable;
    altered_f4_common[0xba5e - 0x100] ^= 0x01;
    bool rejected_altered_f4_common = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_game_flow(altered_f4_common));
    } catch (const std::runtime_error&) {
        rejected_altered_f4_common = true;
    }
    assert(rejected_altered_f4_common);
    auto altered_f5_handler = *game_executable;
    altered_f5_handler[0x7597 - 0x100] ^= 0x01;
    bool rejected_altered_f5_handler = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_game_flow(altered_f5_handler));
    } catch (const std::runtime_error&) {
        rejected_altered_f5_handler = true;
    }
    assert(rejected_altered_f5_handler);
    auto altered_f6_handler = *game_executable;
    altered_f6_handler[0x7415 - 0x100] ^= 0x01;
    bool rejected_altered_f6_handler = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_game_flow(altered_f6_handler));
    } catch (const std::runtime_error&) {
        rejected_altered_f6_handler = true;
    }
    assert(rejected_altered_f6_handler);
    auto altered_f7_handler = *game_executable;
    altered_f7_handler[0x7521 - 0x100] ^= 0x01;
    bool rejected_altered_f7_handler = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_game_flow(altered_f7_handler));
    } catch (const std::runtime_error&) {
        rejected_altered_f7_handler = true;
    }
    assert(rejected_altered_f7_handler);
    auto altered_f8_handler = *game_executable;
    altered_f8_handler[0x7306 - 0x100] ^= 0x01;
    bool rejected_altered_f8_handler = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_game_flow(altered_f8_handler));
    } catch (const std::runtime_error&) {
        rejected_altered_f8_handler = true;
    }
    assert(rejected_altered_f8_handler);
    eon::MillenniumDosGameSession game_session(game_flow);
    assert(!game_session.observe_action(0));
    assert(!game_session.last_function_key_index());
    assert(game_session.observe_action(0x3b) == std::optional<std::size_t>{0});
    assert(game_session.last_first_function_key_trace());
    assert(game_session.last_first_function_key_trace()->selected_record_address == 0x12cc);
    assert(game_session.observe_action(0x3c) == std::optional<std::size_t>{1});
    assert(game_session.last_second_function_key_trace());
    assert(game_session.last_second_function_key_trace()->first_record_address == 0x1384);
    assert(game_session.last_second_function_key_trace()->record_stride == 0x00c0);
    assert(!game_session.last_first_function_key_trace());
    assert(game_session.observe_action(0x3d) == std::optional<std::size_t>{2});
    assert(game_session.last_third_function_key_trace());
    assert(game_session.last_third_function_key_trace()->callback_address == 0x712a);
    assert(!game_session.last_second_function_key_trace());
    assert(game_session.observe_action(0x3e) == std::optional<std::size_t>{3});
    assert(game_session.last_fourth_function_key_trace());
    assert(game_session.last_fourth_function_key_trace()->common_routine_address == 0xba5e);
    assert(game_session.last_fourth_function_key_trace()->second_runtime_byte_value == 9);
    assert(!game_session.last_third_function_key_trace());
    assert(game_session.observe_action(0x3f) == std::optional<std::size_t>{4});
    assert(game_session.last_fifth_function_key_trace());
    assert(game_session.last_fifth_function_key_trace()->third_call_address == 0x14bf7);
    assert(!game_session.last_fourth_function_key_trace());
    assert(game_session.observe_action(0x40) == std::optional<std::size_t>{5});
    assert(game_session.last_sixth_function_key_trace());
    assert(game_session.last_sixth_function_key_trace()->callback_word_value == 0x3207);
    assert(!game_session.last_fifth_function_key_trace());
    assert(game_session.observe_action(0x41) == std::optional<std::size_t>{6});
    assert(game_session.last_seventh_function_key_trace());
    assert(game_session.last_seventh_function_key_trace()->handler_address == 0x7521);
    assert(game_session.last_seventh_function_key_trace()->second_command_call_address == 0x0666);
    assert(game_session.last_seventh_function_key_trace()->sixth_runtime_word_address == 0xda37);
    assert(game_session.last_seventh_function_key_trace()->terminal_call_address == 0x4bf7);
    assert(!game_session.last_sixth_function_key_trace());
    assert(game_flow.eighth_function_key.handler_address == 0x7306);
    assert(game_flow.eighth_function_key.reset_runtime_byte_address == 0xda30);
    assert(game_flow.eighth_function_key.reset_runtime_byte_value == 0);
    assert(game_flow.eighth_function_key.initial_al_value == 2);
    assert(game_flow.eighth_function_key.local_preflight_address == 0x731a);
    assert(game_flow.eighth_function_key.preflight_runtime_byte_address == 0xda39);
    assert(game_flow.eighth_function_key.preflight_enabled_call_address == 0x7b47);
    assert(game_flow.eighth_function_key.decrement_runtime_byte_address == 0xda0a);
    assert(game_flow.eighth_function_key.depleted_jump_address == 0x7948);
    assert(game_flow.eighth_function_key.repeated_call_address == 0xcafa);
    assert(game_flow.eighth_function_key.repeat_shift_register == 3);
    assert(game_session.observe_action(0x42) == std::optional<std::size_t>{7});
    assert(game_session.last_eighth_function_key_trace());
    assert(game_session.last_eighth_function_key_trace()->handler_address == 0x7306);
    assert(game_session.last_eighth_function_key_trace()->repeated_call_address == 0xcafa);
    assert(!game_session.last_seventh_function_key_trace());
    assert(game_session.observe_action(0x44) == std::optional<std::size_t>{9});
    assert(!game_session.last_first_function_key_trace());
    assert(!game_session.observe_action(0x45));
    assert(!game_session.observe_action(0x0b));
    assert(game_session.last_special_action() == std::optional<std::uint8_t>{0x0b});
    const auto static_data = eon::extract_asset_by_sha256(english_dos->path,
        "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d");
    assert(static_data && static_data->size() == 12'494);
    const auto game_data = eon::parse_millennium_dos_game_data(*static_data);
    assert(game_data.celestial_table_offset == 0x3d2);
    assert(game_data.celestial_labels.size() == 41);
    assert(game_data.celestial_labels.front().source_offset == 0x3d2);
    assert(game_data.celestial_labels.front().text == "Inner System");
    assert(game_data.celestial_labels[4].text == "Earth ");
    assert(game_data.celestial_labels.back().source_offset == 0x513);
    assert(game_data.celestial_labels.back().text == "Asteroids ");
    const auto text_catalog = eon::parse_millennium_dos_static_text_catalog(*static_data);
    assert(text_catalog.pointers.size() == eon::MillenniumDosStaticTextCatalog::pointer_count);
    assert(text_catalog.records.size() == 434);
    assert(text_catalog.pointers[2].target_offset == 0x36a);
    assert(text_catalog.pointers[6].target_offset == 0x3d2);
    assert(text_catalog.pointers[251].target_offset == 0x0ff1);
    assert(text_catalog.pointers[252].target_offset == 0x0ff1);
    assert(text_catalog.pointers[401].target_offset == 0x2c1f);
    assert(text_catalog.pointers[402].target_offset == 0x2c0c);
    assert(text_catalog.records.front().source_offset == 0x366);
    assert(text_catalog.records.front().bytes == std::vector<std::uint8_t>({0x20, 0x00}));
    const auto initial_save = eon::extract_asset_by_sha256(english_dos->path,
        "a9b3d77534d3d575012f9553bfed9520edf92a83af408c977e7f0fd226a470e7");
    assert(initial_save && initial_save->size() == eon::MillenniumDosSaveLayout::serialized_size);
    const auto save_layout = eon::parse_millennium_dos_save_layout(*initial_save);
    assert(save_layout.version == eon::MillenniumDosSaveLayout::expected_version);
    assert(save_layout.state_table.size() == 38);
    assert(save_layout.state_table.front().runtime_offset_0 == 0x8100);
    assert(save_layout.state_table.front().runtime_offset_4 == 0);
    assert(save_layout.state_table.front().runtime_offset_6 == 0);
    assert(save_layout.state_table.front().runtime_offset_8 == 0x2292);
    assert(save_layout.state_table.back().runtime_offset_0 == 0x8600);
    const eon::MillenniumDosSaveSession save_session(*initial_save);
    assert(save_session.serialized_bytes().size() == initial_save->size());
    assert(save_session.sha256() == "a9b3d77534d3d575012f9553bfed9520edf92a83af408c977e7f0fd226a470e7");
    assert(save_session.state_record(0).runtime_offset_0 == 0x8100);
    assert(save_session.state_record(37).runtime_offset_0 == 0x8600);
    const auto first_opaque_range = save_session.opaque_bytes(0x0002, 4);
    assert(first_opaque_range.size() == 4);
    assert(first_opaque_range[0] == (*initial_save)[0x0002]);
    // The presentation session owns a byte-for-byte in-memory copy.  It must
    // not become a mutable view of an archive extraction buffer.
    const auto original_byte = save_session.serialized_bytes()[0x0002];
    auto independently_mutated_extraction = *initial_save;
    independently_mutated_extraction[0x0002] ^= 0xff;
    assert(save_session.serialized_bytes()[0x0002] == original_byte);
    assert(save_session.sha256() == "a9b3d77534d3d575012f9553bfed9520edf92a83af408c977e7f0fd226a470e7");
    bool rejected_bad_save_record = false;
    try {
        static_cast<void>(save_session.state_record(38));
    } catch (const std::out_of_range&) {
        rejected_bad_save_record = true;
    }
    assert(rejected_bad_save_record);
    bool rejected_bad_save_range = false;
    try {
        static_cast<void>(save_session.opaque_bytes(initial_save->size(), 1));
    } catch (const std::out_of_range&) {
        rejected_bad_save_range = true;
    }
    assert(rejected_bad_save_range);
    auto truncated_save = *initial_save;
    truncated_save.pop_back();
    bool rejected_truncated_save = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_save_layout(truncated_save));
    } catch (const std::runtime_error&) {
        rejected_truncated_save = true;
    }
    assert(rejected_truncated_save);
    assert(gx_lib.directory_offset() == 0x4bd3c);
    assert(gx_lib.entries().size() == 180);
    assert(gx_lib.entries().front().name == "IMG00");
    assert(gx_lib.entries().front().offset == 6);
    assert(gx_lib.entries().front().size == 3'461);
    const auto gameplay_canvas = eon::parse_millennium_dos_gameplay_screen(*gx_bytes);
    assert(gameplay_canvas.canvas.width == 320 && gameplay_canvas.canvas.height == 167);
    assert(gameplay_canvas.canvas_logical_to_dac.size() == 68);
    assert(eon::to_hex(eon::sha256(gameplay_canvas.canvas.pixels))
        == "1ea177a0fe10a1cae9201e6d31bc91f78a943af5fae8ab36a4c882ea32b6f5a8");
    assert(eon::to_hex(eon::sha256(gameplay_canvas.rgba))
        == "b433c77e91dc66e98c2d76a90d63eaabccf706d537ff1258c8af7fbab93efe98");
    const auto* gx_img01 = gx_lib.find("IMG01");
    const auto* gx_imgb3 = gx_lib.find("IMGB3");
    assert(gx_img01 && gx_img01->offset == 0x0d8b && gx_img01->size == 14'079);
    assert(gx_imgb3 && gx_imgb3->offset == 0x4bb83 && gx_imgb3->size == 441);
    assert(eon::to_hex(eon::sha256(gx_lib.read(*gx_imgb3)))
        == "333c18a883b85c9cefe1072cd44b0a6bc51375ec5f63b87f42106e25dfb6f907");
    const auto last_bytes = eon::extract_asset_by_sha256(english_dos->path,
        "a3f5c0b447795881dd4cd5316a091ecc218b1bf563f567b6fe3f093f89781510");
    assert(last_bytes && last_bytes->size() == 18'117);
    const auto last_screen = eon::parse_millennium_dos_last_screen(*last_bytes);
    assert(last_screen.bitmap.width == 318 && last_screen.bitmap.height == 197);
    assert(last_screen.bitmap.max_palette_index == 15);
    assert(last_screen.palette.logical_to_dac.size() == 16);
    assert(last_screen.rgba.size() == 318U * 197U * 4U);
    assert(eon::to_hex(eon::sha256(last_screen.bitmap.pixels))
        == "b13d52cab4ee715be28bca56997157fa102eaf86f53b0771c6b072dc0b701136");
    assert(eon::to_hex(eon::sha256(last_screen.rgba))
        == "1e3183b45e50f2c186ab7cf6a7f820f0481c8103150777973d107375b50b0e99");

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
    const auto* spanish_title = disk.find("TITLE.LIB");
    const auto* spanish_static_data = disk.find("2200AD4.BIN");
    assert(executable && executable->size == 54'566);
    assert(graphics && graphics->size == 311'420);
    assert(spanish_title && spanish_title->size == 18'998);
    assert(spanish_static_data && spanish_static_data->size == 13'254);
    assert(eon::to_hex(eon::sha256(disk.read(*executable)))
        == "9f7d6f28f71eb7f2f6bb48cb3977efbf45049fc74083f8cbc865ec25396330c6");
    assert(eon::to_hex(eon::sha256(disk.read(*graphics)))
        == "e27d1c697da677994e2f864a776f4fc900c7feb4ec4b85500b2bfea3bc834767");
    const eon::MillenniumDosLib spanish_title_lib(disk.read(*spanish_title));
    assert(spanish_title_lib.directory_offset() == 0x486e);
    assert(spanish_title_lib.entries().size() == 38);
    const auto* spanish_p00 = spanish_title_lib.find("P00");
    assert(spanish_p00 && spanish_p00->offset == 6 && spanish_p00->size == 10'555);
    const auto spanish_bitmap = eon::decode_millennium_dos_bitmap(
        spanish_title_lib.read(*spanish_p00));
    const auto spanish_palette = eon::decode_millennium_dos_palette(
        spanish_title_lib.read(*spanish_p00), spanish_bitmap);
    const auto spanish_rgba = eon::colorize_millennium_dos_bitmap(spanish_bitmap, spanish_palette);
    assert(spanish_bitmap.width == 320 && spanish_bitmap.height == 200);
    assert(eon::to_hex(eon::sha256(spanish_bitmap.pixels))
        == "85ec11c9f943672df2ba2a4e2837ce1f3158d61648ec07bcdc84b381bd24f4ee");
    assert(eon::to_hex(eon::sha256(spanish_rgba))
        == "667e297e1cd2860fa5dd6b10749d3af7859dad0844408a32a4d04a682153bc92");
    const auto spanish_game_data = eon::parse_millennium_dos_game_data(
        disk.read(*spanish_static_data));
    assert(spanish_game_data.celestial_table_offset == 0x3db);
    assert(spanish_game_data.celestial_labels.size() == 41);
    assert(spanish_game_data.celestial_labels.front().text == "Sistema inter.");
    assert(spanish_game_data.celestial_labels[4].text == "Tierra ");
    assert(spanish_game_data.celestial_labels.back().text == "Asteroides ");
    const auto spanish_text_catalog = eon::parse_millennium_dos_static_text_catalog(
        disk.read(*spanish_static_data));
    assert(spanish_text_catalog.pointers.size() == eon::MillenniumDosStaticTextCatalog::pointer_count);
    assert(spanish_text_catalog.records.size() == 434);
    assert(spanish_text_catalog.pointers[2].target_offset == 0x36a);

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
    const auto* atari_executable = atari_disk.find("MILENIUM.TOS");
    assert(atari_data && atari_data->size == 932);
    assert(atari_executable && atari_executable->size == 49'269);
    assert(eon::to_hex(eon::sha256(atari_disk.read(*atari_data)))
        == "6f1e8ab7720c530f8cf5bfc07497824ff731ce977a15d941dad5acd999c6eeda");
    const auto atari_prg = eon::parse_atari_st_prg(atari_disk.read(*atari_executable));
    assert(atari_prg.text_bytes == 4'446);
    assert(atari_prg.data_bytes == 44'564);
    assert(atari_prg.bss_bytes == 81'382);
    assert(atari_prg.symbol_bytes == 0);
    assert(atari_prg.flags == 0);
    assert(atari_prg.absolute_flag == 0);
    assert(atari_prg.relocation_count == 227);
    assert(atari_prg.first_relocation_offset == 0x6);
    assert(atari_prg.last_relocation_offset == 0x1150);
    assert(atari_prg.relocations.size() == atari_prg.relocation_count);
    assert(atari_prg.relocations.front().offset == atari_prg.first_relocation_offset);
    assert(atari_prg.relocations.front().original_value == 0x115e);
    assert(atari_prg.relocations.back().offset == atari_prg.last_relocation_offset);
    assert(atari_prg.relocations.back().original_value == 0x139c8);

    const auto deuteros_atari = std::find_if(releases.begin(), releases.end(), [](const auto& release) {
        return release.game == eon::Game::deuteros && release.platform == eon::Platform::atari_st;
    });
    assert(deuteros_atari != releases.end());
    const auto deuteros_st_disk1 = eon::extract_asset_by_sha256(deuteros_atari->path,
        "aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee");
    const auto deuteros_st_disk2 = eon::extract_asset_by_sha256(deuteros_atari->path,
        "5501ce3fd79c9b37cf695692a8012267db23dacd8a2cc64c0c7b7e4305971193");
    assert(deuteros_st_disk1 && deuteros_st_disk2);
    const eon::DeuterosAtariDisk deuteros_disk1(*deuteros_st_disk1);
    const eon::DeuterosAtariDisk deuteros_disk2(*deuteros_st_disk2);
    const auto& deuteros_st_boot = deuteros_disk1.boot_profile();
    assert(deuteros_st_boot.bytes_per_sector == 512);
    assert(deuteros_st_boot.sectors_per_cluster == 2);
    assert(deuteros_st_boot.total_sectors == 1440);
    assert(deuteros_st_boot.sectors_per_track == 9);
    assert(deuteros_st_boot.heads == 2);
    assert(deuteros_st_boot.boot_checksum == 0x1234);
    assert(deuteros_st_boot.boot_branch_target == 0x1e);
    assert(deuteros_st_boot.has_recovered_first_stage);
    assert(deuteros_st_boot.first_stage_track == 70);
    assert(deuteros_st_boot.first_stage_length == 0x1200);
    const auto deuteros_first_stage = deuteros_disk1.read_sectors(70, 0, 1, 9);
    assert(eon::to_hex(eon::sha256(deuteros_first_stage))
        == "dad3594c53375bd8285ef33e2d685bd38a5b38d930f2ea1305d117d63667f168");
    const auto deuteros_first_stage_profile = eon::parse_deuteros_atari_first_stage(deuteros_first_stage);
    assert(deuteros_first_stage_profile.entry_offset == 0x9c4);
    assert(deuteros_first_stage_profile.checksum_start_offset == 6);
    assert(deuteros_first_stage_profile.checksum_byte_count == 0x43c);
    assert(deuteros_first_stage_profile.checksum_seed == 0x22225555);
    assert(deuteros_first_stage_profile.checksum_expected == 0x7ae26af7);
    assert(deuteros_first_stage_profile.next_track == 2);
    assert(deuteros_first_stage_profile.next_side == 0);
    assert(deuteros_first_stage_profile.next_sector == 1);
    assert(deuteros_first_stage_profile.next_sector_count == 9);
    assert(deuteros_first_stage_profile.next_destination == 0x70000);
    assert(deuteros_first_stage_profile.copy_destination == 0x1e00);
    assert(deuteros_first_stage_profile.copy_byte_count == 0x1200);
    const auto deuteros_second_stage = deuteros_disk1.read_sectors(2, 0, 1, 9);
    assert(eon::to_hex(eon::sha256(deuteros_second_stage))
        == "2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7");
    const auto deuteros_second_stage_profile = eon::parse_deuteros_atari_second_stage(
        deuteros_second_stage);
    assert(deuteros_second_stage_profile.supervisor_stack == 0x7b000);
    assert(deuteros_second_stage_profile.application_stack == 0x2478);
    assert(deuteros_second_stage_profile.direct_entry == 0x1ec4);
    assert(deuteros_second_stage_profile.raw_read_routine_offset == 0x60);
    assert(deuteros_second_stage_profile.raw_read_max_sector_count == 9);
    assert(deuteros_second_stage_profile.side_switch_track == 0x50);
    assert(deuteros_disk2.boot_profile().boot_checksum == 0x1234);
    assert(deuteros_disk2.boot_profile().boot_branch_target == 0x22);
    assert(deuteros_disk2.boot_profile().killer_boot_signature);
    assert(deuteros_disk2.boot_profile().has_killer_boot_vector_setup);
    assert(deuteros_disk2.boot_profile().killer_boot_entry_offset == 0x30);
    assert(deuteros_disk2.boot_profile().killer_boot_vector_source_offset == 0xee);
    assert(deuteros_disk2.boot_profile().killer_boot_vector_destination == 0x8);
    assert(deuteros_disk2.boot_profile().killer_boot_vector_longword_count == 10);
    assert(deuteros_disk2.boot_profile().killer_boot_continuation == 0x12);

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
    const auto& main_entry = load_plan.main_stage_entry;
    assert(main_entry.entry_address == 0x21734);
    assert(main_entry.incoming_controller_cell == 0x20976);
    assert(main_entry.incoming_mode_cell == 0x21704);
    assert(main_entry.stack_address == 0x22296);
    assert(main_entry.memory_ceiling == 0x7fff0);
    assert((main_entry.initialization_calls
        == std::array<std::uint32_t, 2>{0x20068, 0x2013a}));
    assert(main_entry.loop_address == 0x217f6);
    assert(main_entry.loop_first_service_address == 0x22a5a);
    assert(main_entry.loop_scheduler_address == 0x21380);
    assert(main_entry.first_state_word_address == 0x21720);
    assert(main_entry.second_state_word_address == 0x2171e);
    assert(main_entry.scheduler_enable_word_address == 0x210f2);
    assert(main_entry.scheduler_enable_word_value == 1);
    assert(main_entry.first_input_address == 0xdff016);
    assert(main_entry.first_input_bit == 10);
    assert(main_entry.second_input_address == 0xbfe001);
    assert(main_entry.second_input_bit == 6);
    assert(main_entry.input_dispatch_address == 0x21982);
    assert(main_entry.input_dispatch_state_address == 0x21704);
    assert(main_entry.input_dispatch_compare_value == 2);
    assert(main_entry.input_dispatch_clamped_value == 1);
    assert(main_entry.input_dispatch_service_address == 0x218cc);
    assert(main_entry.input_dispatch_continue_address == 0x2181c);
    assert(main_entry.dispatch_service_state_address == 0x21704);
    assert(main_entry.dispatch_service_first_exit_value == 2);
    assert(main_entry.dispatch_service_first_exit_address == 0x21a4c);
    assert(main_entry.dispatch_service_second_exit_value == 3);
    assert(main_entry.dispatch_service_second_exit_address == 0x219f8);
    assert(main_entry.first_exit_profile_cell_address == 0x219f4);
    assert(main_entry.first_exit_profile_value == 1);
    assert(main_entry.bootstrap_controller_return_cell == 0x12ff8);
    assert(main_entry.bootstrap_profile_return_cell == 0x12ffc);
    assert(main_entry.second_exit_profile_cell_address == 0x219f4);
    assert(main_entry.second_exit_initial_profile_value == 5);
    assert(main_entry.second_exit_service_address == 0x20b42);
    assert(main_entry.second_exit_service_match_value == 0x4452f018);
    assert(main_entry.second_exit_matched_return_address == 0x21a56);
    // $21982 writes profile one before returning to the bootstrap. Its table
    // routine supplies these exact raw-track load constants.
    assert(load_plan.title_handoff_profile.disk_offset == 0x6e000);
    assert(load_plan.title_handoff_profile.length == 0x6ca00);
    assert(load_plan.title_handoff_profile.destination == 0x13000);
    // Profile one is a raw title/game stage, not an archive to unpack.  Its
    // on-disk JMP vector enters the loaded interval at this exact address.
    assert(load_plan.title_stage.disk_offset == 0x6e000);
    assert(load_plan.title_stage.length == 0x6ca00);
    assert(load_plan.title_stage.destination == 0x13000);
    assert(load_plan.title_stage.entry_address == 0x40426);
    const auto title_stage = eon::parse_deuteros_amiga_title_stage(system_disk, load_plan);
    assert(title_stage.entry_address == 0x40426);
    assert(title_stage.incoming_mode_address == 0x4040e);
    assert(title_stage.special_mode == 5);
    assert(title_stage.special_mode_byte_address == 0x3717e);
    assert(title_stage.special_mode_configuration_address == 0x38092);
    assert(title_stage.special_mode_configuration_value == 0x101);
    assert(title_stage.normal_mode_byte_address == 0x19d52);
    assert(title_stage.normal_mode_byte_value == 1);
    assert(title_stage.main_loop_address == 0x40574);
    assert(title_stage.loop_service_address == 0x222c0);
    assert(title_stage.loop_input_service_address == 0x23e4e);
    assert(title_stage.timer_counter_address == 0x40410);
    assert(title_stage.timer_threshold == 0xea60);
    assert(title_stage.timer_dispatch_address == 0x4069a);
    assert(title_stage.timer_dispatch_inhibit_address == 0x22d34);
    assert(title_stage.timer_dispatch_inhibit_value == 0x11);
    assert(title_stage.timer_counter_reset_address == 0x40410);
    assert(title_stage.transition_active_flag_address == 0x202c6);
    assert(title_stage.transition_saved_display_word_address == 0x202b8);
    assert(title_stage.transition_source_palette_address == 0x1ed24);
    assert(title_stage.transition_work_palette_address == 0x40678);
    assert(title_stage.transition_palette_word_count == 16);
    assert(title_stage.transition_palette_mask == 0x0eee);
    assert(title_stage.transition_graphics_library_base_address == 0x12fec);
    assert(title_stage.transition_first_library_vector == -0xc0);
    assert(title_stage.transition_second_library_vector == -0x1a4);
    assert(title_stage.transition_second_phase_source_address == 0x12e12);
    assert(title_stage.transition_second_phase_first_work_address == 0x1ffda);
    assert(title_stage.transition_second_phase_second_work_address == 0x1ffe6);
    assert(title_stage.transition_second_phase_work_pointer_address == 0x2008e);
    assert(title_stage.transition_first_compare_address == 0x1ffc8);
    assert(title_stage.transition_second_compare_address == 0x1ffce);
    assert(title_stage.transition_third_compare_address == 0x1ffd4);
    assert(title_stage.transition_return_address == 0x4077c);
    assert(title_stage.post_transition_control_address == 0x407e6);
    assert(title_stage.post_transition_control_reset_value == 0);
    assert(title_stage.post_transition_first_helper_address == 0x3f7a8);
    assert(title_stage.post_transition_second_helper_address == 0x1f9a4);
    assert(title_stage.post_transition_third_helper_address == 0x1fe7a);
    assert(title_stage.post_transition_response_helper_address == 0x1f238);
    assert(title_stage.post_transition_response_code == 0x1b);
    assert(title_stage.post_transition_first_compare_value == 0x20);
    assert(title_stage.post_transition_second_compare_value == 0x2e);
    assert(title_stage.post_transition_third_compare_value == 0x2c);
    assert(title_stage.post_transition_return_address == 0x407e4);
    assert(title_stage.post_transition_selector_address == 0x1fe7a);
    assert(title_stage.post_transition_selector_input_mask == 0x0000ffff);
    assert(title_stage.post_transition_selector_first_divisor == 0x64);
    assert(title_stage.post_transition_selector_second_divisor == 0x0a);
    assert(title_stage.post_transition_selector_addend == 0x30);
    assert(title_stage.post_transition_selector_flag_address == 0x1fe54);
    assert(title_stage.post_transition_selector_dispatch_address == 0x1fbe6);
    assert(title_stage.post_transition_dispatch_state_address == 0x1f98c);
    assert(title_stage.post_transition_dispatch_zero_branch_address == 0x1fc22);
    assert(title_stage.post_transition_dispatch_positive_branch_address == 0x1fc9c);
    assert(title_stage.post_transition_dispatch_zero_variant_state_address == 0x1f98e);
    assert(title_stage.post_transition_dispatch_zero_clear_variant_address == 0x1fc2c);
    assert(title_stage.post_transition_dispatch_zero_set_variant_address == 0x1fd0a);
    assert(title_stage.post_transition_dispatch_zero_pattern_table_address == 0x1f99c);
    assert(title_stage.post_transition_dispatch_zero_destination_pointer_address == 0x1f974);
    assert(title_stage.post_transition_dispatch_zero_clear_source_pointer_address == 0x1f970);
    assert(title_stage.post_transition_dispatch_zero_set_source_pointer_address == 0x1f96c);
    assert(title_stage.post_transition_dispatch_zero_pointer_advance_address == 0x1f9a0);
    assert(title_stage.post_transition_dispatch_zero_row_advance == 0x28);
    assert(title_stage.post_transition_dispatch_zero_plane_advance == 0x1f40);
    assert(title_stage.post_transition_dispatch_zero_row_count == 8);
    assert(title_stage.post_transition_dispatch_zero_plane_count == 4);
    assert(title_stage.post_transition_dispatch_negative_service_address == 0x3fbf8);
    assert(title_stage.post_transition_dispatch_negative_service_d0 == 0x13);
    assert(title_stage.post_transition_dispatch_negative_service_d1 == 0x0c);
    assert(title_stage.post_transition_dispatch_negative_suppress_value == 0x20);
    assert(title_stage.post_transition_dispatch_negative_delay == 0x4e20);
    assert(title_stage.post_transition_dispatch_negative_service_preserves_a0_a1);
    assert(title_stage.post_transition_dispatch_negative_restores_d5_then_d0);
    assert(title_stage.post_transition_dispatch_negative_return_address == 0x1fc20);
    assert(title_stage.title_exit_first_address == 0x37f56);
    assert(title_stage.title_exit_first_profile == 2);
    assert(title_stage.title_exit_second_address == 0x38038);
    assert(title_stage.title_exit_second_profile == 4);
    assert(title_stage.title_exit_third_address == 0x38068);
    assert(title_stage.title_exit_third_profile == 3);
    assert(title_stage.title_exit_controller_address == 0x12800);
    assert(title_stage.title_exit_profile_slot_address == 0x12ffc);
    assert(title_stage.title_exit_profile_table_address == 0x12a36);
    assert(title_stage.title_exit_resolved_profile == 0);
    assert(title_stage.title_exit_main_stage_entry_address == 0x21734);
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
    const auto sound_bank = eon::parse_deuteros_amiga_sound_bank(system_disk, first_bundle);
    assert(sound_bank.sounds.size() == 21);
    assert((sound_bank.trailing_bytes == std::vector<std::uint8_t>{0x00, 0x01, 0xce, 0x8e}));
    assert(sound_bank.sounds[1].sample_relative_offset == 0x2a8b);
    assert(sound_bank.sounds[1].length_words == 0x40bc);
    assert(sound_bank.sounds[1].period == 0x1c0 && sound_bank.sounds[2].period == 0x1c2);
    assert(sound_bank.sounds[1].volume == 0x3f);
    assert(sound_bank.sounds[1].pcm == sound_bank.sounds[2].pcm);
    assert(eon::to_hex(eon::sha256(sound_bank.sounds[1].pcm))
        == "f23fcd05f543be31726271b08ebfe7d907acfe31d1780aaf286fd2db701ae5d5");
    // $0b writes the selected original descriptor to AUDx. The first two
    // genuine opening events target AUD0 then AUD1, whose physical stereo
    // routing is left then right. The integer phase model keeps the original
    // AUDxPER cadence at a host-independent clock boundary.
    eon::DeuterosAmigaPaulaMixer paula(sound_bank);
    assert(paula.submit({1, 1}));
    assert(paula.submit({2, 2}));
    const auto opening_audio = paula.render(6);
    assert(opening_audio.size() == 12);
    const auto encoded_first_pcm = sound_bank.sounds[1].pcm[0];
    const auto signed_first_pcm = encoded_first_pcm < 0x80U
        ? static_cast<std::int16_t>(encoded_first_pcm)
        : static_cast<std::int16_t>(encoded_first_pcm) - 256;
    const auto first_pcm = static_cast<float>(signed_first_pcm)
        / 128.0F * static_cast<float>(sound_bank.sounds[1].volume) / 64.0F;
    assert(std::fabs(opening_audio[0] - first_pcm) < 0.000001F);
    assert(std::fabs(opening_audio[1] - first_pcm) < 0.000001F);
    assert(paula.channels()[0].sample_index == 0);
    assert(paula.channels()[1].sample_index == 0);
    static_cast<void>(paula.render(1));
    assert(paula.channels()[0].sample_index == 1);
    assert(paula.channels()[1].sample_index == 1);
    // Sound zero uses the private $22aaa descriptor rather than bundle PCM;
    // an out-of-range or empty mask must fail closed.
    assert(!paula.submit({0, 1}));
    assert(!paula.submit({1, 0}));
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
    // Advance actual bundle-zero programs well beyond the first animation
    // transition. This catches any stateful selector reached by real control
    // flow, rather than relying only on a hand-built channel state below.
    for (std::size_t tick = 0; tick < 512; ++tick) {
        static_cast<void>(live_opening.tick());
        assert(live_opening.frame_composed_on_last_tick());
    }
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
    // $20c9a branches to $21092 on bit 13 and reads the one $23024 scratch
    // buffer. A restoration before its matching save must fail closed rather
    // than receiving made-up background pixels.
    {
        eon::DeuterosAmigaCompositor compositor;
        std::vector<eon::DeuterosAmigaChannelState> restore_only(1);
        restore_only[0].bitmap_selector = 0x2001;
        restore_only[0].y = 10;
        bool rejected = false;
        try {
            static_cast<void>(compositor.compose(system_disk, first_bundle,
                first_indexed_blob, restore_only));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    // The genuine record-one pixels exercise the exact Cxxx path: $20cb0
    // decodes it opaquely, $21034 captures full 320-pixel scanlines, writes
    // $ffff into the channel, and $21092 restores those lines at the next
    // channel Y coordinate.
    {
        eon::DeuterosAmigaCompositor compositor;
        std::vector<eon::DeuterosAmigaChannelState> state(1);
        state[0].bitmap_selector = 0xc001;
        state[0].x = 8;
        state[0].y = 180;
        const auto& saved_frame = compositor.compose(system_disk, first_bundle,
            first_indexed_blob, state);
        assert(compositor.has_saved_scanlines());
        assert(state[0].bitmap_selector == 0xffff);
        std::vector<std::uint8_t> saved_rows(saved_frame.color_indices.begin()
                + static_cast<std::size_t>(180 * 320),
            saved_frame.color_indices.begin() + static_cast<std::size_t>(197 * 320));
        state[0].y = 160;
        const auto& restored_frame = compositor.compose(system_disk, first_bundle,
            first_indexed_blob, state);
        assert(std::equal(saved_rows.begin(), saved_rows.end(),
            restored_frame.color_indices.begin() + static_cast<std::size_t>(160 * 320)));
    }
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
