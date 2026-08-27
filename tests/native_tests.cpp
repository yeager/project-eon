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
#include "data/deuteros_amiga_alternate_renderer.hpp"
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
#include <cstdio>
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
    const auto defjam_splitter = eon::parse_millennium_amiga_resident_word_splitter(
        defjam_loader_disk, defjam_plan);
    assert(defjam_splitter.entry_address == 0x68016);
    assert(defjam_splitter.source_a1_offset == 0x36);
    assert((defjam_splitter.magnitude_word_addresses
        == std::array<std::uint32_t, 3>{{0x7b764, 0x7b766, 0x7b768}}));
    assert((defjam_splitter.sign_byte_addresses
        == std::array<std::uint32_t, 3>{{0x7b776, 0x7b777, 0x7b778}}));
    assert(defjam_splitter.helper_address == 0x7ba12);
    assert(defjam_splitter.signed_word_address == 0x7b768);
    assert(defjam_splitter.signed_sign_address == 0x7b778);
    const auto defjam_helper_boundary = eon::parse_millennium_amiga_resident_helper_raw_boundary(
        defjam_loader_disk, defjam_plan, defjam_splitter);
    assert(defjam_helper_boundary.helper_address == 0x7ba12);
    assert(defjam_helper_boundary.raw_disk_offset == 0x29e12);
    assert((defjam_helper_boundary.raw_prefix == std::array<std::uint8_t, 32>{{
        0x00, 0x01, 0x20, 0x00, 0x80, 0xac, 0x00, 0x00,
        0x01, 0x00, 0x08, 0x80, 0x42, 0x00, 0x00, 0x01,
        0x01, 0x00, 0x80, 0xac, 0x00, 0x00, 0x01, 0x00,
        0x20, 0x80, 0x42, 0x00, 0x00, 0x01, 0x00, 0x10,
    }}));
    assert(defjam_helper_boundary.raw_prefix_sha256
        == "eb11f5c5dfda4234b0214599bffec09402deff2435c58d57db1f7ab84c07c434");
    const auto defjam_setup_helper_boundary =
        eon::parse_millennium_amiga_resident_setup_helper_raw_boundary(
            defjam_loader_disk, defjam_plan);
    assert(defjam_setup_helper_boundary.helper_address == 0x7b77e);
    assert(defjam_setup_helper_boundary.raw_disk_offset == 0x29b7e);
    assert((defjam_setup_helper_boundary.raw_prefix == std::array<std::uint8_t, 32>{{
        0x04, 0x00, 0x6e, 0x00, 0xc2, 0x00, 0x04, 0x4a,
        0x00, 0xc2, 0x40, 0x00, 0x7a, 0x00, 0xc2, 0x00,
        0x10, 0x52, 0x00, 0xc2, 0x01, 0x00, 0x52, 0x00,
        0xc2, 0x00, 0x01, 0x4a, 0x00, 0xc2, 0x08, 0x00,
    }}));
    assert(defjam_setup_helper_boundary.raw_prefix_sha256
        == "a695fd5ead90e07075256b1347220afde1a4439dd804cf1a9d445da4411cb52a");
    const auto defjam_staging_callsites = eon::parse_millennium_amiga_resident_helper_staging_callsites(
        defjam_loader_disk, defjam_plan, defjam_splitter);
    assert(defjam_staging_callsites[0].entry_address == 0x69624);
    assert(defjam_staging_callsites[0].source_address == 0x7cc3c);
    assert(defjam_staging_callsites[1].entry_address == 0x69b88);
    assert(defjam_staging_callsites[1].source_address == 0x7cc68);
    for (const auto& callsite : defjam_staging_callsites) {
        assert(callsite.magnitude_destination == 0x7b764);
        assert(callsite.sign_destination == 0x7b776);
        assert(callsite.setup_helper_address == 0x7b77e);
        assert(callsite.clear_byte_address == 0x7b14e);
        assert(callsite.helper_address == 0x7ba12);
        assert(callsite.post_helper_magnitude_address == 0x7b764);
    }
    assert(defjam_staging_callsites[0].post_helper_return_address == 0x69656);
    assert(defjam_staging_callsites[0].post_helper_source_address == 0x7cc46);
    assert(defjam_staging_callsites[1].post_helper_return_address == 0x69bba);
    assert(defjam_staging_callsites[1].post_helper_source_address == 0x7cc72);
    const auto defjam_first_post_helper_chain =
        eon::parse_millennium_amiga_resident_first_post_helper_static_chain(
            defjam_loader_disk, defjam_plan, defjam_staging_callsites[0]);
    assert(defjam_first_post_helper_chain.staging_entry_address == 0x69624);
    assert(defjam_first_post_helper_chain.static_start_address == 0x69656);
    assert(defjam_first_post_helper_chain.raw_disk_offset == 0x17a56);
    assert(defjam_first_post_helper_chain.byte_count == 86);
    assert(defjam_first_post_helper_chain.sha256
        == "5f42f9d3078d374f8b4a70fcc59c618abb9381d6b33ef25b3f2967876f0afe7b");
    assert(defjam_first_post_helper_chain.next_setup_call_address == 0x696a0);
    assert(defjam_first_post_helper_chain.next_setup_target == 0x7b77e);
    assert(defjam_first_post_helper_chain.following_call_address == 0x696a6);
    assert(defjam_first_post_helper_chain.following_target == 0x7c802);
    const auto defjam_second_post_helper_chain =
        eon::parse_millennium_amiga_resident_second_post_helper_static_chain(
            defjam_loader_disk, defjam_plan, defjam_staging_callsites[1]);
    assert(defjam_second_post_helper_chain.staging_entry_address == 0x69b88);
    assert(defjam_second_post_helper_chain.static_start_address == 0x69bba);
    assert(defjam_second_post_helper_chain.raw_disk_offset == 0x17fba);
    assert(defjam_second_post_helper_chain.byte_count == 44);
    assert(defjam_second_post_helper_chain.sha256
        == "5616f19900cb96ebc81edf90d0d17a9cde1644be07657801e243514b05e6ee23");
    assert(defjam_second_post_helper_chain.static_call_address == 0x69be0);
    assert(defjam_second_post_helper_chain.static_call_target == 0x68d50);
    const auto defjam_staging_reachability =
        eon::parse_millennium_amiga_resident_staging_direct_reachability_boundary(
            defjam_loader_disk, defjam_plan, defjam_staging_callsites);
    assert((defjam_staging_reachability.staging_entry_addresses
        == std::array<std::uint32_t, 2>{{0x69624, 0x69b88}}));
    assert((defjam_staging_reachability.absolute_jsr_counts == std::array<std::uint32_t, 2>{}));
    assert((defjam_staging_reachability.absolute_jmp_counts == std::array<std::uint32_t, 2>{}));
    assert((defjam_staging_reachability.pc_relative_bsr_word_counts == std::array<std::uint32_t, 2>{}));
    assert((defjam_staging_reachability.local_immediate_register_jsr_counts
        == std::array<std::uint32_t, 2>{}));
    assert((defjam_staging_reachability.local_immediate_register_jmp_counts
        == std::array<std::uint32_t, 2>{}));
    assert(defjam_staging_reachability.scanned_raw_disk_offset == 0x16400);
    assert(defjam_staging_reachability.scanned_byte_count == 0x2c000);
    const auto staged_pre_setup = eon::stage_millennium_amiga_resident_helper_pre_setup(
        {{0x1020, 0x3040, 0x5060}}, {{0x01, 0x00, 0xff}});
    assert((staged_pre_setup.magnitude_words
        == std::array<std::uint16_t, 3>{{0x1020, 0x3040, 0x5060}}));
    assert((staged_pre_setup.sign_bytes == std::array<std::uint8_t, 3>{{0x01, 0x00, 0xff}}));
    // These are real, consecutive words from the supplied raw resident range.
    // They exercise the exact pre-helper LSL/ROXL/LSR data movement without
    // claiming that this disk position was an original A1 caller.
    const auto splitter_source = defjam_loader_disk.bytes(
        defjam_plan.resident_stage.disk_offset + 0x100, 6);
    const std::array<std::uint16_t, 3> splitter_words{{
        static_cast<std::uint16_t>((splitter_source[0] << 8U) | splitter_source[1]),
        static_cast<std::uint16_t>((splitter_source[2] << 8U) | splitter_source[3]),
        static_cast<std::uint16_t>((splitter_source[4] << 8U) | splitter_source[5]),
    }};
    assert((splitter_words == std::array<std::uint16_t, 3>{{0x3039, 0x0007, 0xb76a}}));
    const auto splitter_pre_helper = eon::split_millennium_amiga_resident_words_pre_helper(
        splitter_words);
    assert((splitter_pre_helper.magnitude_words
        == std::array<std::uint16_t, 3>{{0x3039, 0x0007, 0x376a}}));
    assert((splitter_pre_helper.sign_bytes == std::array<std::uint8_t, 3>{{0, 0, 1}}));
    auto invalid_first_post_helper_chain_disk_bytes = *defjam_adf;
    invalid_first_post_helper_chain_disk_bytes[0x17aa0] ^= 0x01;
    bool invalid_first_post_helper_chain_rejected = false;
    try {
        const eon::AmigaAdf invalid_first_post_helper_chain_disk(
            std::move(invalid_first_post_helper_chain_disk_bytes));
        static_cast<void>(eon::parse_millennium_amiga_resident_first_post_helper_static_chain(
            invalid_first_post_helper_chain_disk, defjam_plan, defjam_staging_callsites[0]));
    } catch (const std::runtime_error&) {
        invalid_first_post_helper_chain_rejected = true;
    }
    assert(invalid_first_post_helper_chain_rejected);
    auto invalid_second_post_helper_chain_disk_bytes = *defjam_adf;
    invalid_second_post_helper_chain_disk_bytes[0x17fe0] ^= 0x01;
    bool invalid_second_post_helper_chain_rejected = false;
    try {
        const eon::AmigaAdf invalid_second_post_helper_chain_disk(
            std::move(invalid_second_post_helper_chain_disk_bytes));
        static_cast<void>(eon::parse_millennium_amiga_resident_second_post_helper_static_chain(
            invalid_second_post_helper_chain_disk, defjam_plan, defjam_staging_callsites[1]));
    } catch (const std::runtime_error&) {
        invalid_second_post_helper_chain_rejected = true;
    }
    assert(invalid_second_post_helper_chain_rejected);
    auto invalid_staging_reachability_disk_bytes = *defjam_adf;
    const std::array<std::uint8_t, 6> injected_direct_staging_jsr{{
        0x4e, 0xb9, 0x00, 0x06, 0x96, 0x24,
    }};
    std::copy(injected_direct_staging_jsr.begin(), injected_direct_staging_jsr.end(),
        invalid_staging_reachability_disk_bytes.begin() + 0x16400);
    bool invalid_staging_reachability_rejected = false;
    try {
        const eon::AmigaAdf invalid_staging_reachability_disk(
            std::move(invalid_staging_reachability_disk_bytes));
        static_cast<void>(eon::parse_millennium_amiga_resident_staging_direct_reachability_boundary(
            invalid_staging_reachability_disk, defjam_plan, defjam_staging_callsites));
    } catch (const std::runtime_error&) {
        invalid_staging_reachability_rejected = true;
    }
    assert(invalid_staging_reachability_rejected);
    auto invalid_staging_bsr_reachability_disk_bytes = *defjam_adf;
    const std::array<std::uint8_t, 4> injected_direct_staging_bsr{{
        0x61, 0x00, 0x16, 0x22,
    }};
    std::copy(injected_direct_staging_bsr.begin(), injected_direct_staging_bsr.end(),
        invalid_staging_bsr_reachability_disk_bytes.begin() + 0x16400);
    bool invalid_staging_bsr_reachability_rejected = false;
    try {
        const eon::AmigaAdf invalid_staging_bsr_reachability_disk(
            std::move(invalid_staging_bsr_reachability_disk_bytes));
        static_cast<void>(eon::parse_millennium_amiga_resident_staging_direct_reachability_boundary(
            invalid_staging_bsr_reachability_disk, defjam_plan, defjam_staging_callsites));
    } catch (const std::runtime_error&) {
        invalid_staging_bsr_reachability_rejected = true;
    }
    assert(invalid_staging_bsr_reachability_rejected);
    auto invalid_staging_register_reachability_disk_bytes = *defjam_adf;
    const std::array<std::uint8_t, 8> injected_direct_staging_register_jsr{{
        0x20, 0x7c, 0x00, 0x06, 0x96, 0x24, 0x4e, 0x90,
    }};
    std::copy(injected_direct_staging_register_jsr.begin(), injected_direct_staging_register_jsr.end(),
        invalid_staging_register_reachability_disk_bytes.begin() + 0x16400);
    bool invalid_staging_register_reachability_rejected = false;
    try {
        const eon::AmigaAdf invalid_staging_register_reachability_disk(
            std::move(invalid_staging_register_reachability_disk_bytes));
        static_cast<void>(eon::parse_millennium_amiga_resident_staging_direct_reachability_boundary(
            invalid_staging_register_reachability_disk, defjam_plan, defjam_staging_callsites));
    } catch (const std::runtime_error&) {
        invalid_staging_register_reachability_rejected = true;
    }
    assert(invalid_staging_register_reachability_rejected);
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
    assert(title_flow.launcher_title_program_address == 0x68f);
    assert(title_flow.launcher_game_program_address == 0x69a);
    assert(title_flow.launcher_title_call_address == 0x240);
    assert(title_flow.launcher_game_call_address == 0x24c);
    assert(title_flow.launcher_common_call_target == 0x31c);
    assert(title_flow.launcher_common_branch_address == 0x345);
    assert(title_flow.launcher_common_branch_target == 0x34c);
    assert(title_flow.launcher_common_fallthrough_return == 0x34b);
    assert(title_flow.launcher_common_branch_target_static_boundary == 0x35a);
    assert(title_flow.launcher_pre_title_gate_address == 0x215);
    assert(title_flow.launcher_pre_title_gate_target == 0x21a);
    assert(title_flow.launcher_pre_title_call_address == 0x231);
    assert(title_flow.launcher_pre_title_call_target == 0x2cf);
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
    assert(game_flow.first_function_key.display_selector_call_address == 0xd0c9);
    assert(game_flow.first_function_key.setup_entry_address == 0x771d);
    assert(game_flow.first_function_key.selector_address == 0xda1f);
    assert(game_flow.first_function_key.selector_value == 0);
    assert(game_flow.first_function_key.record_pointer_table_address == 0x27c4);
    assert(game_flow.first_function_key.selected_record_address == 0x12cc);
    assert(game_flow.first_function_key.selected_record_storage_address == 0xda20);
    assert(game_flow.first_function_key.screen_descriptor_address == 0x300f);
    assert(game_flow.first_function_key.screen_descriptor_mode == 7);
    assert(game_flow.first_function_key.screen_selector_storage_address == 0x75a8);
    assert(game_flow.first_function_key.screen_descriptor_storage_address == 0x75a6);
    assert(game_flow.first_function_key.setup_first_call_address == 0x5b1f);
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
    assert(game_flow.fourth_function_key.initialization_guard_clear_address == 0xa557);
    assert(game_flow.fourth_function_key.transfer_al_value == 2);
    assert(game_flow.fourth_function_key.common_routine_address == 0xba5e);
    assert(game_flow.fourth_function_key.first_call_address == 0x4d2c);
    assert(game_flow.fourth_function_key.first_write_instruction_address == 0xba64);
    assert(game_flow.fourth_function_key.first_runtime_byte_address == 0xda13);
    assert(game_flow.fourth_function_key.first_runtime_byte_value == 7);
    assert(game_flow.fourth_function_key.second_call_address == 0x9dd5);
    assert(game_flow.fourth_function_key.second_write_instruction_address == 0xba6c);
    assert(game_flow.fourth_function_key.second_runtime_byte_address == 0xda1e);
    assert(game_flow.fourth_function_key.second_runtime_byte_value == 9);
    assert(game_flow.fourth_function_key.third_runtime_byte_address == 0x75a9);
    assert(game_flow.fourth_function_key.third_runtime_byte_value == 0);
    assert(game_flow.fourth_function_key.common_return_instruction_address == 0xba76);
    assert(game_flow.fifth_function_key.handler_address == 0x7597);
    assert(game_flow.fifth_function_key.transfer_al_value == 2);
    assert(game_flow.fifth_function_key.first_call_address == 0xbe28);
    assert(game_flow.fifth_function_key.first_call_initial_nested_call_address == 0x52f9);
    assert(game_flow.fifth_function_key.second_call_address == 0x0b9d);
    assert(game_flow.fifth_function_key.second_call_mode_address == 0x07f9);
    assert(game_flow.fifth_function_key.second_call_mode_value == 1);
    assert(game_flow.fifth_function_key.third_call_address == 0x4bf7);
    assert(game_flow.fifth_function_key.third_call_initial_nested_call_address == 0x0bd7);
    assert(game_flow.fifth_function_key.fourth_call_address == 0x0b76);
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
    assert(game_flow.sixth_function_key.restoration_handler_address == 0x7455);
    assert(game_flow.sixth_function_key.restoration_first_source_address == 0x740f);
    assert(game_flow.sixth_function_key.restoration_first_destination_address == 0x75ae);
    assert(game_flow.sixth_function_key.restoration_word_source_address == 0x7410);
    assert(game_flow.sixth_function_key.restoration_word_destination_address == 0x75ac);
    assert(game_flow.sixth_function_key.restoration_second_source_address == 0x7412);
    assert(game_flow.sixth_function_key.restoration_second_destination_address == 0x75a8);
    assert(game_flow.sixth_function_key.restoration_first_call_address == 0x0b0c);
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
    auto altered_f4_guard_clear = *game_executable;
    altered_f4_guard_clear[0xa557 - 0x100] ^= 0x01;
    bool rejected_altered_f4_guard_clear = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_game_flow(altered_f4_guard_clear));
    } catch (const std::runtime_error&) {
        rejected_altered_f4_guard_clear = true;
    }
    assert(rejected_altered_f4_guard_clear);
    auto altered_f5_handler = *game_executable;
    altered_f5_handler[0x7597 - 0x100] ^= 0x01;
    bool rejected_altered_f5_handler = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_game_flow(altered_f5_handler));
    } catch (const std::runtime_error&) {
        rejected_altered_f5_handler = true;
    }
    assert(rejected_altered_f5_handler);
    auto altered_f5_first_boundary = *game_executable;
    altered_f5_first_boundary[0xbe28 - 0x100] ^= 0x01;
    bool rejected_altered_f5_first_boundary = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_game_flow(altered_f5_first_boundary));
    } catch (const std::runtime_error&) {
        rejected_altered_f5_first_boundary = true;
    }
    assert(rejected_altered_f5_first_boundary);
    auto altered_f6_handler = *game_executable;
    altered_f6_handler[0x7415 - 0x100] ^= 0x01;
    bool rejected_altered_f6_handler = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_game_flow(altered_f6_handler));
    } catch (const std::runtime_error&) {
        rejected_altered_f6_handler = true;
    }
    assert(rejected_altered_f6_handler);
    auto altered_f6_restoration = *game_executable;
    altered_f6_restoration[0x7455 - 0x100] ^= 0x01;
    bool rejected_altered_f6_restoration = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_game_flow(altered_f6_restoration));
    } catch (const std::runtime_error&) {
        rejected_altered_f6_restoration = true;
    }
    assert(rejected_altered_f6_restoration);
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
    auto altered_f9_handler = *game_executable;
    altered_f9_handler[0x7339 - 0x100] ^= 0x01;
    bool rejected_altered_f9_handler = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_game_flow(altered_f9_handler));
    } catch (const std::runtime_error&) {
        rejected_altered_f9_handler = true;
    }
    assert(rejected_altered_f9_handler);
    auto altered_f10_handler = *game_executable;
    altered_f10_handler[0x7384 - 0x100] ^= 0x01;
    bool rejected_altered_f10_handler = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_game_flow(altered_f10_handler));
    } catch (const std::runtime_error&) {
        rejected_altered_f10_handler = true;
    }
    assert(rejected_altered_f10_handler);
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
    // Unlike F8, F4 has no pre-call write. Its first call must return before
    // the first literal store and its second call must return before the two
    // trailing stores. The private overlay must therefore stay untouched.
    assert(!game_session.last_runtime_byte_effect());
    assert(!game_session.reconstructed_runtime_byte(0xda13));
    assert(!game_session.reconstructed_runtime_byte(0xda1e));
    assert(!game_session.reconstructed_runtime_byte(0x75a9));
    assert(!game_session.last_third_function_key_trace());
    assert(game_session.observe_action(0x3f) == std::optional<std::size_t>{4});
    assert(game_session.last_fifth_function_key_trace());
    assert(game_session.last_fifth_function_key_trace()->third_call_address == 0x14bf7);
    assert(!game_session.last_fourth_function_key_trace());
    assert(game_session.observe_action(0x40) == std::optional<std::size_t>{5});
    assert(game_session.last_sixth_function_key_trace());
    assert(game_session.last_sixth_function_key_trace()->callback_word_value == 0x3207);
    // All F6 stores follow two native calls and the observed restoration path
    // itself ends at a native call. Unlike F8, no private runtime overlay is
    // justified from this trace.
    assert(!game_session.last_runtime_byte_effect());
    assert(!game_session.reconstructed_runtime_byte(0x75a8));
    assert(!game_session.reconstructed_runtime_byte(0x75ae));
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
    // The F8 prefix has no runtime admission branch: it deterministically
    // clears $da30 before its unsupported calls. The unknown initial byte is
    // not manufactured; only the post-write value is reconstructed.
    assert(game_session.last_runtime_byte_effect());
    assert(game_session.last_runtime_byte_effect()->address == 0xda30);
    assert(!game_session.last_runtime_byte_effect()->previous);
    assert(game_session.last_runtime_byte_effect()->value == 0);
    assert(game_session.reconstructed_runtime_byte(0xda30) == std::optional<std::uint8_t>{0});
    assert(!game_session.reconstructed_runtime_byte(0xda31));
    assert(game_session.observe_action(0x42) == std::optional<std::size_t>{7});
    assert(game_session.last_runtime_byte_effect());
    assert(game_session.last_runtime_byte_effect()->previous == std::optional<std::uint8_t>{0});
    assert(game_session.last_runtime_byte_effect()->value == 0);
    assert(!game_session.last_seventh_function_key_trace());
    assert(game_flow.ninth_function_key.handler_address == 0x7339);
    assert(game_flow.ninth_function_key.initialization_guard_address == 0xa19e);
    assert(game_flow.ninth_function_key.display_selector_call_address == 0xd0c9);
    assert(game_flow.ninth_function_key.first_reset_runtime_byte_address == 0xda30);
    assert(game_flow.ninth_function_key.first_reset_runtime_byte_value == 0);
    assert(game_flow.ninth_function_key.initial_al_value == 2);
    assert(game_flow.ninth_function_key.local_mode_address == 0x6e2f);
    assert(game_flow.ninth_function_key.local_mode_value == 1);
    assert(game_flow.ninth_function_key.second_reset_runtime_byte_address == 0xdad7);
    assert(game_flow.ninth_function_key.second_reset_runtime_byte_value == 0);
    assert(game_flow.ninth_function_key.enabled_runtime_byte_address == 0xda39);
    assert(game_flow.ninth_function_key.enabled_call_address == 0x7b47);
    assert(game_flow.ninth_function_key.limit_runtime_byte_address == 0xda06);
    assert(game_flow.ninth_function_key.limit_value == 9);
    assert(game_flow.ninth_function_key.local_preflight_address == 0x731a);
    assert(game_flow.ninth_function_key.terminal_call_address == 0x14124);
    assert(game_flow.tenth_function_key.handler_address == 0x7384);
    assert(game_flow.tenth_function_key.initialization_guard_address == 0xa19e);
    assert(game_flow.tenth_function_key.display_selector_call_address == 0xd0c9);
    assert(game_flow.tenth_function_key.first_reset_runtime_byte_address == 0xda30);
    assert(game_flow.tenth_function_key.first_reset_runtime_byte_value == 0);
    assert(game_flow.tenth_function_key.initial_al_value == 2);
    assert(game_flow.tenth_function_key.second_reset_runtime_byte_address == 0xdad7);
    assert(game_flow.tenth_function_key.second_reset_runtime_byte_value == 0);
    assert(game_flow.tenth_function_key.local_mode_address == 0x6e2f);
    assert(game_flow.tenth_function_key.local_mode_value == 1);
    assert(game_flow.tenth_function_key.enabled_runtime_byte_address == 0xda39);
    assert(game_flow.tenth_function_key.enabled_call_address == 0x7b47);
    assert(game_flow.tenth_function_key.limit_runtime_byte_address == 0xda06);
    assert(game_flow.tenth_function_key.limit_value == 2);
    assert(game_flow.tenth_function_key.local_preflight_address == 0x731a);
    assert(game_flow.tenth_function_key.local_mode_reset_value == 0);
    assert(game_flow.tenth_function_key.conditional_runtime_byte_address == 0xda09);
    assert(game_flow.tenth_function_key.conditional_call_address == 0x7a9d);
    assert(game_flow.tenth_function_key.first_terminal_call_address == 0x4140);
    assert(game_flow.tenth_function_key.second_terminal_call_address == 0x7bcb);
    assert(game_flow.tenth_function_key.third_terminal_call_address == 0xa2a0);
    assert(game_flow.tenth_function_key.wait_runtime_byte_address == 0xda41);
    assert(game_flow.tenth_function_key.wait_call_address == 0x09fa);
    assert(game_flow.tenth_function_key.repeat_shift_register == 3);
    assert(game_flow.tenth_function_key.final_call_address == 0x4111);
    assert(game_session.observe_action(0x43) == std::optional<std::size_t>{8});
    assert(game_session.last_ninth_function_key_trace());
    assert(game_session.last_ninth_function_key_trace()->handler_address == 0x7339);
    assert(game_session.last_ninth_function_key_trace()->limit_value == 9);
    assert(!game_session.last_eighth_function_key_trace());
    assert(!game_session.last_runtime_byte_effect());
    assert(game_session.reconstructed_runtime_byte(0xda30) == std::optional<std::uint8_t>{0});
    assert(game_session.observe_action(0x44) == std::optional<std::size_t>{9});
    assert(game_session.last_tenth_function_key_trace());
    assert(game_session.last_tenth_function_key_trace()->handler_address == 0x7384);
    assert(game_session.last_tenth_function_key_trace()->limit_value == 2);
    assert(!game_session.last_first_function_key_trace());
    assert(!game_session.last_ninth_function_key_trace());
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
    const auto atari_bootstrap = eon::parse_millennium_atari_bootstrap(
        atari_disk.read(*atari_executable), atari_prg);
    assert(atari_bootstrap.entry_offset == 0);
    assert(atari_bootstrap.branch_target_offset == 0x24);
    assert(atari_bootstrap.stage_source_offset == 0x115e);
    assert(atari_bootstrap.stage_last_longword_offset == 0x1232);
    assert(atari_bootstrap.stage_destination_offset == 0x1d636);
    assert(atari_bootstrap.stage_bytes == 0xd8);
    const auto atari_bss_entry = eon::parse_millennium_atari_bss_entry(
        atari_disk.read(*atari_executable), atari_prg, atari_bootstrap);
    assert(atari_bss_entry.entry_offset == 0x1d636);
    assert(atari_bss_entry.copy_source_address == 0x1d652);
    assert(atari_bss_entry.copy_destination_address == 0x77000);
    assert(atari_bss_entry.initial_d0 == 0x100);
    assert(atari_bss_entry.copied_words == 0x101);
    assert(atari_bss_entry.jump_address == 0x77000);
    const auto atari_bss_source = eon::materialize_millennium_atari_bss_source(
        atari_disk.read(*atari_executable), atari_prg, atari_bootstrap, atari_bss_entry);
    assert(atari_bss_source.load_base == 0x116c4);
    assert(atari_bss_source.bss_start_address == 0x1d636);
    assert(atari_bss_source.source_address == 0x1d652);
    assert(atari_bss_source.source_data_offset == 0x117a);
    assert(atari_bss_source.original_data_bytes == 0xbc);
    assert(atari_bss_source.bss_zero_bytes == 0x146);
    assert(atari_bss_source.bytes.size() == 0x202);
    const auto atari_target = eon::materialize_millennium_atari_target(
        atari_bss_source, atari_bss_entry);
    assert(atari_target.source_address == 0x1d652);
    assert(atari_target.target_address == 0x77000);
    assert(atari_target.first_opcode == 0x3f3c);
    assert(atari_target.first_immediate_word == 0x0002);
    assert(atari_target.first_immediate_longword == 0x1d6e4);
    assert(atari_target.bytes == atari_bss_source.bytes);
    const auto atari_trap = eon::parse_millennium_atari_trap_entry(atari_bss_source, atari_target);
    assert(atari_trap.target_address == 0x77000);
    assert(atari_trap.fopen_filename_address == 0x1d6e4);
    assert(atari_trap.fopen_filename == "MILL22A.inf");
    assert(atari_trap.fopen_access_mode == 2);
    assert(atari_trap.fopen_function == 0x3d);
    assert(atari_trap.fopen_trap_offset == 14);
    assert(atari_trap.following_fclose_function == 0x3e);
    assert(atari_trap.following_fclose_selector_offset == 18);
    assert(atari_trap.fopen_result_test_offset == 22);
    assert(atari_trap.fopen_result_negative_branch_offset == 24);
    assert(atari_trap.fopen_result_negative_branch_target_offset == 24);
    const auto atari_config = eon::probe_millennium_atari_config(atari_disk);
    assert(atari_config.requested_filename == "MILL22A.inf");
    assert(atari_config.root_entry_count == 13);
    assert(atari_config.present);
    assert(atari_config.first_cluster == 3);
    assert(atari_config.size == 7'506);
    assert(atari_config.first_word == 0x4ef9);
    assert(atari_config.first_longword_operand == 0x2aa88);
    assert(atari_config.sha256 == "74d7d630779fd811aedcdbe31b14e54198eb9ffd673df512dd70b6165c4a37b6");
    const auto atari_config_payload = atari_disk.read(*atari_disk.find("MILL22A.inf"));
    const auto atari_config_entry = eon::parse_millennium_atari_config_entry(atari_config_payload);
    assert(atari_config_entry.proven_load_base == 0x2a4de);
    assert(atari_config_entry.entry_address == 0x2aa88);
    assert(atari_config_entry.entry_file_offset == 0x5aa);
    assert(atari_config_entry.initial_trap_selector == 0x15);
    assert(atari_config_entry.initial_trap_longword_argument == 0);
    assert(atari_config_entry.palette_trap_selector == 0x06);
    assert(atari_config_entry.palette_trap_longword_argument == 0x2a612);
    assert((atari_config_entry.jsr_targets == std::vector<std::uint32_t>{
        0x2b55a, 0x2aa68, 0x2aa0c, 0x2b2be, 0x2b448, 0x2aa0c}));
    assert(atari_config_entry.final_pea_address == 0x2ab0a);
    assert(atari_config_entry.final_trap_selector == 0x26);
    assert(atari_config_entry.return_offset == 0x628);
    const auto atari_first_jsr = eon::parse_millennium_atari_config_first_jsr(
        atari_config_payload, atari_config_entry);
    assert(atari_first_jsr.proven_load_base == 0x2a4de);
    assert(atari_first_jsr.target_address == 0x2b55a);
    assert(atari_first_jsr.target_file_offset == 0x107c);
    assert(atari_first_jsr.leading_opcode == 0x035a);
    assert(atari_first_jsr.movem_opcode == 0x4cdf);
    assert(atari_first_jsr.movem_register_mask == 0x7fff);
    assert(atari_first_jsr.return_opcode == 0x4e75);
    const auto atari_second_jsr = eon::parse_millennium_atari_config_second_jsr(
        atari_config_payload, atari_config_entry);
    assert(atari_second_jsr.proven_load_base == 0x2a4de);
    assert(atari_second_jsr.target_address == 0x2aa68);
    assert(atari_second_jsr.target_file_offset == 0x58a);
    assert(atari_second_jsr.initial_opcode == 0x0880);
    assert(atari_second_jsr.immediate_bit_number == 0x000d);
    assert(atari_second_jsr.conditional_branch_opcode == 0x6714);
    assert(atari_second_jsr.conditional_branch_target_address == 0x2aa82);
    assert(atari_second_jsr.join_jsr_address == 0x2aa82);
    assert(atari_second_jsr.join_jsr_target == 0x2a51c);
    assert(atari_second_jsr.following_jsr_target == 0x2b55a);
    const auto atari_join_jsr = eon::parse_millennium_atari_config_join_jsr(
        atari_config_payload, atari_second_jsr);
    assert(atari_join_jsr.proven_load_base == 0x2a4de);
    assert(atari_join_jsr.target_address == 0x2a51c);
    assert(atari_join_jsr.target_file_offset == 0x3e);
    assert(atari_join_jsr.initial_opcode == 0x548f);
    assert(atari_join_jsr.d0_word_store_opcode == 0x33c0);
    assert(atari_join_jsr.d0_word_store_address == 0x2a512);
    assert(atari_join_jsr.line_a_opcode == 0xa000);
    assert(atari_join_jsr.first_longword_store_address == 0x2a514);
    assert(atari_join_jsr.second_longword_store_address == 0x2a518);
    assert(atari_join_jsr.return_opcode == 0x4e75);
    const auto atari_forwarded_jsr = eon::parse_millennium_atari_config_forwarded_jsr(
        atari_config_payload, atari_config_entry);
    assert(atari_forwarded_jsr.proven_load_base == 0x2a4de);
    assert(atari_forwarded_jsr.entry_address == 0x2aa0c);
    assert(atari_forwarded_jsr.entry_file_offset == 0x52e);
    assert(atari_forwarded_jsr.jump_opcode == 0x4ef9);
    assert(atari_forwarded_jsr.forwarded_address == 0x2a5dc);
    assert(atari_forwarded_jsr.forwarded_file_offset == 0xfe);
    assert(atari_forwarded_jsr.initial_opcode == 0x3f01);
    assert(atari_forwarded_jsr.trap_selector == 0x0019);
    assert(atari_forwarded_jsr.trap_opcode == 0x4e4e);
    assert(atari_forwarded_jsr.stack_cleanup_opcode == 0x504f);
    assert(atari_forwarded_jsr.return_opcode == 0x4e75);
    const auto atari_third_jsr = eon::parse_millennium_atari_config_third_jsr(
        atari_config_payload, atari_config_entry);
    assert(atari_third_jsr.proven_load_base == 0x2a4de);
    assert(atari_third_jsr.target_address == 0x2b2be);
    assert(atari_third_jsr.target_file_offset == 0xde0);
    assert(atari_third_jsr.initial_opcode == 0x1400);
    assert(atari_third_jsr.gate_opcode == 0x0200);
    assert(atari_third_jsr.gate_immediate == 0x00c0);
    assert(atari_third_jsr.branch_opcode == 0x6600);
    assert(atari_third_jsr.branch_displacement == 0x003a);
    assert(atari_third_jsr.branch_target_address == 0x2b300);
    assert(atari_third_jsr.branch_target_opcode == 0x0802);
    assert(atari_third_jsr.branch_target_immediate == 0x0006);
    assert(atari_third_jsr.branch_target_branch_opcode == 0x6700);
    const auto atari_fourth_jsr = eon::parse_millennium_atari_config_fourth_jsr(
        atari_config_payload, atari_config_entry);
    assert(atari_fourth_jsr.proven_load_base == 0x2a4de);
    assert(atari_fourth_jsr.target_address == 0x2b448);
    assert(atari_fourth_jsr.target_file_offset == 0xf6a);
    assert(atari_fourth_jsr.d7_setup_opcode == 0x3e3c);
    assert(atari_fourth_jsr.d7_initial_value == 0x0006);
    assert(atari_fourth_jsr.a5_initial_address == 0x2b428);
    assert(atari_fourth_jsr.a4_initial_address == 0x2b3c8);
    assert(atari_fourth_jsr.d6_initial_value == 0x000f);
    assert(atari_fourth_jsr.d5_initial_value == 0x0002);
    assert(atari_fourth_jsr.d4_initial_value == 0x0100);
    const auto atari_fourth_loop = eon::parse_millennium_atari_config_fourth_loop(
        atari_config_payload, atari_fourth_jsr);
    assert(atari_fourth_loop.target_address == 0x2b448);
    assert(atari_fourth_loop.body_address == 0x2b464);
    assert(atari_fourth_loop.body_file_offset == 0xf86);
    assert(atari_fourth_loop.body_bytes == 22);
    assert(atari_fourth_loop.backedge_opcode == 0x51cd);
    assert(atari_fourth_loop.backedge_displacement == -20);
    assert(atari_fourth_loop.backedge_target_address == 0x2b464);
    assert(atari_fourth_loop.setup_d5_value == 0x0002);
    const auto atari_fourth_post_loop = eon::parse_millennium_atari_config_fourth_post_loop(
        atari_config_payload, atari_fourth_loop);
    assert(atari_fourth_post_loop.post_loop_address == 0x2b47a);
    assert(atari_fourth_post_loop.post_loop_file_offset == 0xf9c);
    assert(atari_fourth_post_loop.a5_advance_opcode == 0x548d);
    assert(atari_fourth_post_loop.outer_backedge_opcode == 0x51ce);
    assert(atari_fourth_post_loop.outer_backedge_displacement == -34);
    assert(atari_fourth_post_loop.outer_backedge_target_address == 0x2b45c);
    assert(atari_fourth_post_loop.target_setup_opcode == 0x3a3c);
    assert(atari_fourth_post_loop.target_setup_immediate == 0x0002);
    const auto atari_fourth_outer_setup = eon::parse_millennium_atari_config_fourth_outer_setup(
        atari_config_payload, atari_fourth_post_loop);
    assert(atari_fourth_outer_setup.setup_address == 0x2b45c);
    assert(atari_fourth_outer_setup.setup_file_offset == 0xf7e);
    assert(atari_fourth_outer_setup.d5_setup_opcode == 0x3a3c);
    assert(atari_fourth_outer_setup.d5_initial_value == 0x0002);
    assert(atari_fourth_outer_setup.d4_setup_opcode == 0x383c);
    assert(atari_fourth_outer_setup.d4_initial_value == 0x0100);
    assert(atari_fourth_outer_setup.continuation_address == 0x2b464);
    const auto atari_fourth_post_outer = eon::parse_millennium_atari_config_fourth_post_outer_boundary(
        atari_config_payload, atari_fourth_post_loop);
    assert(atari_fourth_post_outer.boundary_address == 0x2b480);
    assert(atari_fourth_post_outer.boundary_file_offset == 0xfa2);
    assert(atari_fourth_post_outer.longword_push_opcode == 0x2f3c);
    assert(atari_fourth_post_outer.longword_argument == 0x2b428);
    assert(atari_fourth_post_outer.selector_push_opcode == 0x3f3c);
    assert(atari_fourth_post_outer.trap_selector == 0x0006);
    assert(atari_fourth_post_outer.trap_opcode == 0x4e4e);
    const auto atari_jsr_inventory = eon::inventory_millennium_atari_config_absolute_jsrs(atari_config_payload);
    assert(atari_jsr_inventory.encodings.size() == 19);
    assert((atari_jsr_inventory.encodings.front() == std::pair<std::uint32_t, std::uint32_t>{0x50c, 0x2a5aa}));
    assert((atari_jsr_inventory.encodings[9] == std::pair<std::uint32_t, std::uint32_t>{0x60a, 0x2aa0c}));
    assert((atari_jsr_inventory.encodings.back() == std::pair<std::uint32_t, std::uint32_t>{0xdb2, 0x2aa78}));
    auto invalid_atari_config_payload = atari_config_payload;
    invalid_atari_config_payload[0x5b9] ^= 0x01;
    bool invalid_atari_config_rejected = false;
    try {
        static_cast<void>(eon::parse_millennium_atari_config_entry(invalid_atari_config_payload));
    } catch (const std::runtime_error&) {
        invalid_atari_config_rejected = true;
    }
    assert(invalid_atari_config_rejected);
    auto invalid_atari_first_jsr_payload = atari_config_payload;
    invalid_atari_first_jsr_payload[0x107e] ^= 0x01;
    bool invalid_atari_first_jsr_rejected = false;
    try {
        static_cast<void>(eon::parse_millennium_atari_config_first_jsr(
            invalid_atari_first_jsr_payload, atari_config_entry));
    } catch (const std::runtime_error&) {
        invalid_atari_first_jsr_rejected = true;
    }
    assert(invalid_atari_first_jsr_rejected);
    auto invalid_atari_second_jsr_payload = atari_config_payload;
    invalid_atari_second_jsr_payload[0x58e] ^= 0x01;
    bool invalid_atari_second_jsr_rejected = false;
    try {
        static_cast<void>(eon::parse_millennium_atari_config_second_jsr(
            invalid_atari_second_jsr_payload, atari_config_entry));
    } catch (const std::runtime_error&) {
        invalid_atari_second_jsr_rejected = true;
    }
    assert(invalid_atari_second_jsr_rejected);
    auto invalid_atari_join_jsr_payload = atari_config_payload;
    invalid_atari_join_jsr_payload[0x46] ^= 0x01;
    bool invalid_atari_join_jsr_rejected = false;
    try {
        static_cast<void>(eon::parse_millennium_atari_config_join_jsr(
            invalid_atari_join_jsr_payload, atari_second_jsr));
    } catch (const std::runtime_error&) {
        invalid_atari_join_jsr_rejected = true;
    }
    assert(invalid_atari_join_jsr_rejected);
    auto invalid_atari_forwarded_jsr_payload = atari_config_payload;
    invalid_atari_forwarded_jsr_payload[0xfe] ^= 0x01;
    bool invalid_atari_forwarded_jsr_rejected = false;
    try {
        static_cast<void>(eon::parse_millennium_atari_config_forwarded_jsr(
            invalid_atari_forwarded_jsr_payload, atari_config_entry));
    } catch (const std::runtime_error&) {
        invalid_atari_forwarded_jsr_rejected = true;
    }
    assert(invalid_atari_forwarded_jsr_rejected);
    auto invalid_atari_third_jsr_payload = atari_config_payload;
    invalid_atari_third_jsr_payload[0xe22] ^= 0x01;
    bool invalid_atari_third_jsr_rejected = false;
    try {
        static_cast<void>(eon::parse_millennium_atari_config_third_jsr(
            invalid_atari_third_jsr_payload, atari_config_entry));
    } catch (const std::runtime_error&) {
        invalid_atari_third_jsr_rejected = true;
    }
    assert(invalid_atari_third_jsr_rejected);
    auto invalid_atari_fourth_jsr_payload = atari_config_payload;
    invalid_atari_fourth_jsr_payload[0xf74] ^= 0x01;
    bool invalid_atari_fourth_jsr_rejected = false;
    try {
        static_cast<void>(eon::parse_millennium_atari_config_fourth_jsr(
            invalid_atari_fourth_jsr_payload, atari_config_entry));
    } catch (const std::runtime_error&) {
        invalid_atari_fourth_jsr_rejected = true;
    }
    assert(invalid_atari_fourth_jsr_rejected);
    auto invalid_atari_fourth_loop_payload = atari_config_payload;
    invalid_atari_fourth_loop_payload[0xf98] ^= 0x01;
    bool invalid_atari_fourth_loop_rejected = false;
    try {
        static_cast<void>(eon::parse_millennium_atari_config_fourth_loop(
            invalid_atari_fourth_loop_payload, atari_fourth_jsr));
    } catch (const std::runtime_error&) {
        invalid_atari_fourth_loop_rejected = true;
    }
    assert(invalid_atari_fourth_loop_rejected);
    auto invalid_atari_fourth_post_loop_payload = atari_config_payload;
    invalid_atari_fourth_post_loop_payload[0xf9e] ^= 0x01;
    bool invalid_atari_fourth_post_loop_rejected = false;
    try {
        static_cast<void>(eon::parse_millennium_atari_config_fourth_post_loop(
            invalid_atari_fourth_post_loop_payload, atari_fourth_loop));
    } catch (const std::runtime_error&) {
        invalid_atari_fourth_post_loop_rejected = true;
    }
    assert(invalid_atari_fourth_post_loop_rejected);
    auto invalid_atari_fourth_outer_setup_payload = atari_config_payload;
    invalid_atari_fourth_outer_setup_payload[0xf82] ^= 0x01;
    bool invalid_atari_fourth_outer_setup_rejected = false;
    try {
        static_cast<void>(eon::parse_millennium_atari_config_fourth_outer_setup(
            invalid_atari_fourth_outer_setup_payload, atari_fourth_post_loop));
    } catch (const std::runtime_error&) {
        invalid_atari_fourth_outer_setup_rejected = true;
    }
    assert(invalid_atari_fourth_outer_setup_rejected);
    auto invalid_atari_fourth_post_outer_payload = atari_config_payload;
    invalid_atari_fourth_post_outer_payload[0xfac] ^= 0x01;
    bool invalid_atari_fourth_post_outer_rejected = false;
    try {
        static_cast<void>(eon::parse_millennium_atari_config_fourth_post_outer_boundary(
            invalid_atari_fourth_post_outer_payload, atari_fourth_post_loop));
    } catch (const std::runtime_error&) {
        invalid_atari_fourth_post_outer_rejected = true;
    }
    assert(invalid_atari_fourth_post_outer_rejected);
    auto invalid_atari_jsr_inventory_payload = atari_config_payload;
    invalid_atari_jsr_inventory_payload[0xd54] ^= 0x01;
    bool invalid_atari_jsr_inventory_rejected = false;
    try {
        static_cast<void>(eon::inventory_millennium_atari_config_absolute_jsrs(
            invalid_atari_jsr_inventory_payload));
    } catch (const std::runtime_error&) {
        invalid_atari_jsr_inventory_rejected = true;
    }
    assert(invalid_atari_jsr_inventory_rejected);
    std::size_t millennium_st_images = 0;
    std::size_t millennium_fat12_images = 0;
    std::size_t millennium_config_files = 0;
    for (const auto& asset : eon::inventory_zip(atari_release->path)) {
        if (asset.kind != eon::AssetKind::atari_st_disk) continue;
        ++millennium_st_images;
        const auto candidate = eon::extract_asset_by_sha256(atari_release->path, asset.sha256);
        assert(candidate);
        try {
            const eon::Fat12Disk candidate_disk(*candidate);
            ++millennium_fat12_images;
            if (eon::probe_millennium_atari_config(candidate_disk).present) {
                ++millennium_config_files;
            }
        } catch (const std::runtime_error&) {
            // Protected/raw supplied ST media have no FAT12 file namespace.
        }
    }
    assert(millennium_st_images == 7);
    assert(millennium_fat12_images == 5);
    assert(millennium_config_files == 4);
    auto invalid_atari_target_source = atari_bss_source;
    invalid_atari_target_source.bytes.front() ^= 0x01;
    bool invalid_atari_target_rejected = false;
    try {
        static_cast<void>(eon::materialize_millennium_atari_target(
            invalid_atari_target_source, atari_bss_entry));
    } catch (const std::runtime_error&) {
        invalid_atari_target_rejected = true;
    }
    assert(invalid_atari_target_rejected);
    auto invalid_atari_trap_target = atari_target;
    invalid_atari_trap_target.bytes[24] ^= 0x01;
    bool invalid_atari_trap_rejected = false;
    try {
        static_cast<void>(eon::parse_millennium_atari_trap_entry(
            atari_bss_source, invalid_atari_trap_target));
    } catch (const std::runtime_error&) {
        invalid_atari_trap_rejected = true;
    }
    assert(invalid_atari_trap_rejected);
    const auto atari_executable_bytes = atari_disk.read(*atari_executable);
    assert(std::equal(atari_bss_source.bytes.begin(), atari_bss_source.bytes.begin() + 0xbc,
        atari_executable_bytes.begin() + 28 + 0x117a));
    assert(std::all_of(atari_bss_source.bytes.begin() + 0xbc, atari_bss_source.bytes.end(),
        [](std::uint8_t value) { return value == 0; }));

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
    assert(main_entry.scheduler_state_base_address == 0x210f8);
    assert(main_entry.scheduler_channel_count_address == 0x21248);
    assert(main_entry.scheduler_channel_stride == 0x18);
    assert(main_entry.scheduler_active_program_offset == 0x10);
    assert(main_entry.scheduler_wait_selector_offset == 0x06);
    assert(main_entry.scheduler_wait_value_offset == 0x08);
    assert((main_entry.scheduler_wait_selectors
        == std::array<std::uint8_t, 4>{3, 5, 6, 0x14}));
    assert(main_entry.scheduler_tail_probe_address == 0xdff01f);
    assert(main_entry.scheduler_tail_probe_bit == 5);
    assert(main_entry.scheduler_tail_service_address == 0x21698);
    assert(main_entry.input_dispatch_address == 0x21982);
    assert(main_entry.input_dispatch_state_address == 0x21704);
    assert(main_entry.input_dispatch_compare_value == 2);
    assert(main_entry.input_dispatch_clamped_value == 1);
    assert(main_entry.resource_loader_address == 0x21932);
    assert(main_entry.resource_table_address == 0x21708);
    assert(main_entry.resource_index_scale_shift == 2);
    assert(main_entry.resource_probe_address == 0x2ad24);
    assert(main_entry.resource_payload_address == 0x32a24);
    assert(main_entry.resource_transfer_address == 0x20a90);
    assert(main_entry.resource_transfer_chunk_length == 0x1600);
    assert(main_entry.resource_retry_probe_address == 0xdff016);
    assert(main_entry.resource_retry_probe_bit == 10);
    assert(main_entry.resource_retry_address == 0x2196e);
    assert(main_entry.resource_consumer_address == 0x2016a);
    assert(main_entry.resource_consumer_base_address == 0x32a24);
    assert(main_entry.resource_consumer_base_address == main_entry.resource_payload_address);
    assert(main_entry.resource_consumer_seed_address == 0x20168);
    assert(main_entry.resource_consumer_counter_address == 0x2079e);
    assert(main_entry.resource_consumer_index_mask == 0x3ffe);
    assert(main_entry.resource_consumer_word_addend == 14);
    assert((main_entry.resource_consumer_command_words
        == std::array<std::uint16_t, 2>{0x000a, 0x0011}));
    assert((main_entry.resource_consumer_call_sites
        == std::array<std::uint32_t, 2>{0x2159c, 0x2163a}));
    assert(main_entry.renderer_pass_address == 0x21448);
    assert(main_entry.alternate_renderer_selector == 0x00fe);
    assert(main_entry.alternate_renderer_state_data_offset == 0x000c);
    assert(main_entry.alternate_renderer_address == 0x20580);
    assert(main_entry.regular_renderer_address == 0x20c8c);
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

    // The main loader's probe and body pass start at the same physical ADF
    // offset. These are in-memory copies of the genuine first two complete
    // resources, not extracted files or inferred data formats.
    const auto transferred_bundle0 = eon::read_deuteros_amiga_main_resource(
        system_disk, load_plan, 0);
    assert(transferred_bundle0);
    assert(transferred_bundle0->source_disk_offset == 0x1b800);
    assert(transferred_bundle0->probe_destination_address == 0x2ad24);
    assert(transferred_bundle0->payload_destination_address == 0x32a24);
    assert(transferred_bundle0->payload_length == 0x2f3f4);
    assert(transferred_bundle0->payload == std::vector<std::uint8_t>(
        system_disk.bytes(0x1b800, 0x2f3f4).begin(),
        system_disk.bytes(0x1b800, 0x2f3f4).end()));
    // $2016a reads the transfer as an even byte-indexed word source, not as
    // an extracted resource. Zero state observes the genuine high length word.
    const auto resource_sample0 = eon::sample_deuteros_amiga_main_resource_consumer(
        *transferred_bundle0, main_entry, 0, 0);
    assert(resource_sample0.table_offset == 0);
    assert(resource_sample0.sampled_word == 0x0002);
    assert(resource_sample0.addend_result == 0x0010);
    assert(resource_sample0.seed_after == 0x0010);
    const auto resource_sample1 = eon::sample_deuteros_amiga_main_resource_consumer(
        *transferred_bundle0, main_entry, resource_sample0.seed_after, 4);
    assert(resource_sample1.table_offset == 0x0014);
    assert(resource_sample1.sampled_word == 0x0a78);
    assert(resource_sample1.addend_result == 0x0a86);
    assert(resource_sample1.seed_after == 0x0a96);
    const auto transferred_bundle1 = eon::read_deuteros_amiga_main_resource(
        system_disk, load_plan, 1);
    assert(transferred_bundle1);
    assert(transferred_bundle1->source_disk_offset == 0x4ba00);
    assert(transferred_bundle1->payload_length == 0x215f0);
    assert(transferred_bundle1->payload == std::vector<std::uint8_t>(
        system_disk.bytes(0x4ba00, 0x215f0).begin(),
        system_disk.bytes(0x4ba00, 0x215f0).end()));
    const auto resource_sample_bundle1 = eon::sample_deuteros_amiga_main_resource_consumer(
        *transferred_bundle1, main_entry, 0, 0);
    assert(resource_sample_bundle1.sampled_word == 0x0002);
    assert(resource_sample_bundle1.seed_after == 0x0010);
    assert(main_entry.channel_request_cell_address == 0x210f4);
    assert(main_entry.channel_request_value == 0xffff);
    assert(main_entry.channel_request_loop_test_address == 0x21856);
    assert(main_entry.channel_request_loop_branch_address == 0x2185c);
    assert(main_entry.channel_request_continuation_address == 0x21892);

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
    // $21610 adds the verified main-resource base $32a24 before it stores
    // the $0f operand at state +12. Keep both the raw stream displacement
    // and the original in-memory pointer distinct.
    assert(input_vm.channels()[3].alternate_resource == 0x0b38);
    assert(input_vm.channels()[3].mode_data == 0x3355c);
    // The genuine stream after the $0f alternate renderer is
    // `$05,$0008,$0044,$00`. $213be resumes it only when the low word of
    // `$22a20 - 1` is eight and parameter `$44` is strictly below `$22a16`.
    // Opcode zero clears selector +6, while its program pointer survives to
    // the next scheduler pass.
    // $05 has already yielded in the same scheduler call as $0f.
    assert(input_vm.channels()[3].stream_offset == 0x0a96);
    eon::DeuterosAmigaVmInputs post_input_audio_inputs;
    post_input_audio_inputs.audio_position = 8;
    post_input_audio_inputs.audio_limit = 0x45;
    post_input_audio_inputs.random_word = [&input_random] { return input_random.next(); };
    const auto premature_audio_wait = input_vm.tick(post_input_audio_inputs);
    assert(premature_audio_wait.sounds.empty());
    assert(input_vm.channels()[3].active);
    assert(input_vm.channels()[3].wait_mode == 5);
    post_input_audio_inputs.audio_position = 9;
    const auto resolved_audio_wait = input_vm.tick(post_input_audio_inputs);
    assert(resolved_audio_wait.sounds.empty());
    assert(input_vm.channels()[3].active);
    assert(input_vm.channels()[3].wait_mode == 0);
    assert(input_vm.channels()[3].stream_offset == 0x0a98);
    static_cast<void>(input_vm.tick(post_input_audio_inputs));
    assert(!input_vm.channels()[3].active);

    // The remaining bundle-zero channels continue to execute independently
    // after the first input channel has ended.  Walk their genuine streams
    // for a bounded, deterministic interval: the supplied opening state does
    // *not* reach the VM transition request from these channels.  This is a
    // preservation boundary, not a claim that the later opcode is absent
    // from all original game states.
    eon::DeuterosAmigaChannelVm transition_vm(system_disk, first_bundle);
    eon::DeuterosAmigaRandom transition_random(system_disk, first_bundle);
    eon::DeuterosAmigaVmInputs transition_inputs;
    transition_inputs.random_word = [&transition_random] { return transition_random.next(); };
    bool saw_original_transition = false;
    for (std::size_t tick = 0; tick < 10'000; ++tick) {
        const auto transition_events = transition_vm.tick(transition_inputs);
        transition_random.advance_vblank();
        if (transition_events.transition_requested) {
            saw_original_transition = true;
            break;
        }
    }
    if (saw_original_transition) {
        throw std::runtime_error(
            "Unexpected Deuteros transition from the verified opening channel state");
    }
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
    // The first real $fe render pass receives the exact $32a24+$0b38 stream.
    // $20580's observed opening path sets the original position/table globals
    // and requests eleven glyph writes before its zero-byte return. It is traced
    // in memory only: global video/font setup has not been replaced with a
    // fabricated bitmap renderer.
    const auto& alternate_trace = live_input_opening.alternate_renderer_trace();
    assert(alternate_trace);
    assert(alternate_trace->stream_address == 0x3355c);
    assert(alternate_trace->stream_offset == 0x0b38);
    assert(alternate_trace->position_column == 0x0f);
    assert(alternate_trace->position_row == 0x30);
    assert(alternate_trace->primary_video_offset == 0x1e0f);
    assert(alternate_trace->primary_table_selector == 1);
    assert(alternate_trace->secondary_table_selector == 0);
    const std::vector<std::uint8_t> expected_alternate_glyphs{
        'p', 'l', 'e', 'a', 's', 'e', ' ', 'w', 'a', 'i', 't'};
    assert(alternate_trace->glyph_codes == expected_alternate_glyphs);
    // $206e6 reads the genuine eight-byte `p` glyph at $201b0 + ($70-$20)*8
    // and combines it with selectors one/zero.  In the recovered four-plane
    // write formula that makes set bits palette index two and clear bits
    // index four.  The in-memory frame update must retain that exact
    // bitplane result, rather than rasterising a replacement host font.
    eon::DeuterosAmigaFrame alternate_frame;
    alternate_frame.color_indices.assign(
        static_cast<std::size_t>(eon::DeuterosAmigaFrame::width)
            * eon::DeuterosAmigaFrame::height, 0);
    eon::apply_deuteros_amiga_alternate_renderer(
        alternate_frame, system_disk, load_plan, *alternate_trace);
    const std::array<std::uint8_t, 8> expected_p_row{4, 2, 2, 2, 2, 2, 2, 4};
    const auto p_row = alternate_frame.color_indices.begin()
        + static_cast<std::size_t>(194) * eon::DeuterosAmigaFrame::width + 120;
    assert(std::equal(expected_p_row.begin(), expected_p_row.end(), p_row));
    eon::DeuterosAmigaRandom opening_random(system_disk, first_bundle, 0, 0x240);
    assert(opening_random.next() == 0x11);
    assert(opening_random.seed() == 0x11);
    opening_random.advance_vblank();
    assert(opening_random.vblank_counter() == 0x244);
    // The live opening must take its $2016a words from the completed $21932
    // transfer at $32a24, not through a second host-side interpretation of
    // the raw ADF. Both original-backed forms observe the same word+14 path.
    eon::DeuterosAmigaRandom transferred_opening_random(
        *transferred_bundle0, main_entry, 0, 0x240);
    assert(transferred_opening_random.next() == 0x11);
    assert(transferred_opening_random.seed() == 0x11);
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
