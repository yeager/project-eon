#include "platform/game_data.hpp"
#include "launcher.hpp"
#include "i18n.hpp"
#include "launcher_text.hpp"
#include "engine/deuteros_amiga_opening.hpp"
#include "engine/deuteros_atari_bootstrap_session.hpp"
#include "engine/millennium_amiga_bootstrap_session.hpp"
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
#include "data/millennium_control_text.hpp"
#include "data/millennium_dos_game_data.hpp"
#include "data/millennium_dos_game_flow.hpp"
#include "data/millennium_dos_gameplay_screen.hpp"
#include "data/millennium_dos_last_screen.hpp"
#include "data/millennium_save_comparison.hpp"
#include "data/millennium_amiga_loader.hpp"
#include "data/millennium_dos_lib.hpp"
#include "data/millennium_dos_title_flow.hpp"
#include "data/millennium_dos_title_exit.hpp"
#include "data/millennium_dos_title_transition.hpp"
#include "data/millennium_dos_video_driver.hpp"
#include "data/millennium_dos_sound_driver.hpp"
#include "data/modern_pixel_reconstruction.hpp"
#include "engine/millennium_dos_title_session.hpp"
#include "engine/millennium_dos_game_session.hpp"
#include "engine/millennium_dos_save_session.hpp"
#include "engine/millennium_atari_bootstrap_session.hpp"
#include "data/sha256.hpp"
#include "data/recovery_map.hpp"
#include "data/release_manifest.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <set>
#include <span>
#include <vector>

namespace {

void assert_deuteros_atari_post_callback_callees(const std::vector<std::uint8_t>& second_stage,
    const eon::DeuterosAtariSecondStageProfile& stage,
    const eon::DeuterosAtariSupervisorCallbackContinuation& continuation) {
    const auto callees = eon::parse_deuteros_atari_post_callback_callee_profiles(
        second_stage, stage, continuation);
    assert(callees.first_callee_offset == 0x800);
    assert(callees.first_callee_byte_count == 48);
    assert(callees.first_callee_sha256
        == "bb662ff9f02861d2bc40c9d3d2ca97a662abc494ec20a4037807a81b22ca95a6");
    assert(callees.first_callee_literal == 0x71100);
    assert(callees.first_callee_ram_address == 0x25f4);
    assert(callees.first_callee_trap_selector == 5);
    assert(callees.first_callee_trap_offset == 36);
    assert(callees.first_callee_trap_opcode == 0x4e4e);
    assert(callees.first_callee_stack_cleanup_opcode == 0xdffc);
    assert(callees.first_callee_stack_cleanup_bytes == 12);
    assert(callees.first_callee_post_trap_branch_offset == 44);
    assert(callees.first_callee_post_trap_branch_displacement == 0x08e8);
    assert(callees.first_callee_post_trap_branch_target_offset == 0x1116);
    assert(callees.second_callee_offset == 0x1122);
    assert(callees.second_callee_prefix_byte_count == 22);
    assert(callees.second_callee_prefix_sha256
        == "c74fb6b1e03cf6a123698e0356f3c9dbc45e637d9ce2a9479fef37eec6cbfd8c");
    assert(callees.second_callee_byte_count == 0x7e00);
    assert(callees.second_callee_destination == 0x20000);
    assert(callees.second_callee_raw_reader_argument == 0x9000);
    assert(callees.second_callee_bsr_opcode == 0x6100);
    assert(callees.second_callee_bsr_displacement == -0x1106);
    assert(callees.second_callee_bsr_target_offset == 0x30);

    auto altered_second_stage = second_stage;
    altered_second_stage[0x800] ^= 0x01;
    bool rejected = false;
    try {
        static_cast<void>(eon::parse_deuteros_atari_post_callback_callee_profiles(
            altered_second_stage, stage, continuation));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
}

void assert_deuteros_atari_second_callee_continuation(const std::vector<std::uint8_t>& second_stage,
    const eon::DeuterosAtariSecondStageProfile& stage,
    const eon::DeuterosAtariPostCallbackCalleeProfiles& callees) {
    const auto continuation = eon::parse_deuteros_atari_second_callee_continuation(
        second_stage, stage, callees);
    assert(continuation.continuation_offset == 0x1138);
    assert(continuation.continuation_byte_count == 38);
    assert(continuation.continuation_sha256
        == "5b1480495df8defe3e1264dd083ec1c91134c01e56d3d94e060c583ee9b54a89");
    assert(continuation.trap_argument_address == 0x20000);
    assert(continuation.trap_selector == 6);
    assert(continuation.trap_offset == 0x1144);
    assert(continuation.trap_opcode == 0x4e4e);
    assert(continuation.stack_cleanup_opcode == 0x5c8f);
    assert(continuation.stack_cleanup_bytes == 6);
    assert(continuation.copy_source == 0x20020);
    assert(continuation.copy_destination_pointer_address == 0x25f4);
    assert(continuation.copy_loop_counter == 0x1f3f);
    assert(continuation.copy_move_opcode == 0x22d8);
    assert(continuation.copy_dbf_opcode == 0x51cf);
    assert(continuation.copy_dbf_displacement == -4);
    assert(continuation.copy_loop_target_offset == 0x1156);
    assert(continuation.return_opcode == 0x4e75);

    auto altered_second_stage = second_stage;
    altered_second_stage[0x1138] ^= 0x01;
    bool rejected = false;
    try {
        static_cast<void>(eon::parse_deuteros_atari_second_callee_continuation(
            altered_second_stage, stage, callees));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
}

void assert_deuteros_atari_first_callee_continuation(const std::vector<std::uint8_t>& second_stage,
    const eon::DeuterosAtariSecondStageProfile& stage,
    const eon::DeuterosAtariPostCallbackCalleeProfiles& callees) {
    const auto continuation = eon::parse_deuteros_atari_first_callee_continuation(
        second_stage, stage, callees);
    assert(continuation.continuation_offset == 0x1116);
    assert(continuation.continuation_byte_count == 12);
    assert(continuation.continuation_sha256
        == "8778c08ae16a5f66009dda8d60a0dacba267cca4d29211a11fd2e30c40a7796b");
    assert(continuation.immediate_load_opcode == 0x203c);
    assert(continuation.immediate_value == 0xb000);
    assert(continuation.absolute_store_opcode == 0x21c0);
    assert(continuation.absolute_store_address == 0x25f0);
    assert(continuation.return_opcode == 0x4e75);

    auto altered_second_stage = second_stage;
    altered_second_stage[0x1116] ^= 0x01;
    bool rejected = false;
    try {
        static_cast<void>(eon::parse_deuteros_atari_first_callee_continuation(
            altered_second_stage, stage, callees));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
}

void assert_deuteros_atari_raw_reader_wrapper(const std::vector<std::uint8_t>& second_stage,
    const eon::DeuterosAtariSecondStageProfile& stage,
    const eon::DeuterosAtariPostCallbackCalleeProfiles& callees) {
    const auto wrapper = eon::parse_deuteros_atari_raw_reader_wrapper(second_stage, stage, callees);
    assert(wrapper.wrapper_offset == 0x30);
    assert(wrapper.wrapper_byte_count == 48);
    assert(wrapper.wrapper_sha256
        == "132ce2473e3764453bba01308e1f5044dc748bbea8b01975b67a259aa57cea7e");
    assert(wrapper.divisor_opcode == 0x8efc);
    assert(wrapper.divisor == 0x1200);
    assert(wrapper.raw_reader_bsr_target_offset == 0x60);
    assert(wrapper.status_word_address == 0x1e28);
    assert(wrapper.nonzero_branch_target_offset == 0x2a);
    assert(wrapper.first_terminal_branch_target_offset == 0x5e);
    assert(wrapper.second_terminal_branch_target_offset == 0x5e);
    assert(wrapper.destination_advance_bytes == 0x1200);
    assert(wrapper.loop_branch_target_offset == 0x34);
    assert(wrapper.return_helper_target_offset == 0x2a);

    auto altered_second_stage = second_stage;
    altered_second_stage[0x30] ^= 0x01;
    bool rejected = false;
    try {
        static_cast<void>(eon::parse_deuteros_atari_raw_reader_wrapper(
            altered_second_stage, stage, callees));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
}

void assert_deuteros_atari_raw_reader_call_layout(const std::vector<std::uint8_t>& second_stage,
    const eon::DeuterosAtariSecondStageProfile& stage,
    const eon::DeuterosAtariRawReaderWrapperProfile& wrapper) {
    const auto layout = eon::parse_deuteros_atari_raw_reader_call_layout(second_stage, stage, wrapper);
    assert(layout.routine_offset == 0x60);
    assert(layout.routine_byte_count == 74);
    assert(layout.routine_sha256
        == "a5bec9d04daa8ce600add594f6325030acd2ad8535910dee62497da90d572c90");
    assert(layout.initial_count_opcode == 0x7409);
    assert(layout.initial_count_immediate == 9);
    assert(layout.count_compare_opcode == 0xb0bc);
    assert(layout.count_compare_immediate == 0x1200);
    assert(layout.count_branch_target_offset == 0x72);
    assert(layout.side_compare_immediate == 0x50);
    assert(layout.side_branch_target_offset == 0x82);
    assert(layout.abi_call_offset == 0x9c);
    assert(layout.abi_selector == 8);
    assert(layout.abi_call_opcode == 0x4e4e);
    assert(layout.stack_cleanup_bytes == 0x14);
    assert(layout.post_call_store_address == 0x1e28);
    assert(layout.return_opcode == 0x4e75);

    auto altered_second_stage = second_stage;
    altered_second_stage[0x60] ^= 0x01;
    bool rejected = false;
    try {
        static_cast<void>(eon::parse_deuteros_atari_raw_reader_call_layout(
            altered_second_stage, stage, wrapper));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
}

void assert_deuteros_atari_direct_vector_callees(const std::vector<std::uint8_t>& second_stage,
    const eon::DeuterosAtariSecondStageProfile& stage,
    const eon::DeuterosAtariDispatchProfile& dispatch) {
    const auto profiles = eon::parse_deuteros_atari_direct_vector_callees(
        second_stage, stage, dispatch);
    assert(profiles.vector_table_offset == 0xac);
    assert(profiles.distinct_callees[0].vector_slot == 0);
    assert(profiles.distinct_callees[0].runtime_address == 0x1f1a);
    assert(profiles.distinct_callees[0].stage_offset == 0x11a);
    assert(profiles.distinct_callees[0].byte_count == 20);
    assert(profiles.distinct_callees[0].sha256
        == "04c8eba86a6259f8d0b175fa18792cc64263863db51e76f9de839eec5c79ce0f");
    assert(profiles.distinct_callees[1].vector_slot == 1);
    assert(profiles.distinct_callees[1].runtime_address == 0x1f2e);
    assert(profiles.distinct_callees[1].stage_offset == 0x12e);
    assert(profiles.distinct_callees[1].byte_count == 34);
    assert(profiles.distinct_callees[1].sha256
        == "0bc76b22089d008e4ce90d63216c75acbe0786b0a06127fbd66ef0dc252949ac");
    assert(profiles.distinct_callees[2].vector_slot == 5);
    assert(profiles.distinct_callees[2].runtime_address == 0x1f52);
    assert(profiles.distinct_callees[2].stage_offset == 0x152);
    assert(profiles.distinct_callees[2].byte_count == 84);
    assert(profiles.distinct_callees[2].sha256
        == "eaee587850078d67a72dcf0da4b45e672c89a1352b040db580bedc0ba3b20e97");
    assert(profiles.alias_branch_offset == 0x150);
    assert(profiles.alias_branch_opcode == 0x60c8);
    assert(profiles.alias_branch_displacement == -56);
    assert(profiles.alias_branch_target_offset == 0x11a);

    auto altered_second_stage = second_stage;
    altered_second_stage[0x152] ^= 0x01;
    bool rejected = false;
    try {
        static_cast<void>(eon::parse_deuteros_atari_direct_vector_callees(
            altered_second_stage, stage, dispatch));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
}

void assert_deuteros_atari_direct_vector_transfer_loop(
    const std::vector<std::uint8_t>& second_stage,
    const eon::DeuterosAtariSecondStageProfile& stage,
    const eon::DeuterosAtariDispatchProfile& dispatch) {
    const auto callees = eon::parse_deuteros_atari_direct_vector_callees(second_stage, stage, dispatch);
    const auto loop = eon::parse_deuteros_atari_direct_vector_transfer_loop(
        second_stage, stage, callees);
    assert(loop.loop_block_offset == 0x170);
    assert(loop.loop_block_byte_count == 28);
    assert(loop.loop_block_sha256
        == "92cb6cf8a41c55df8459a9608c9626ff7cc831cceb69dd2b5531ac766b111552");
    assert(loop.destination_pointer_load_opcode == 0x41f9);
    assert(loop.destination_pointer == 0x57a00);
    assert(loop.source_pointer_load_opcode == 0x227c);
    assert(loop.source_pointer == 0xb006);
    assert(loop.counter_load_opcode == 0x303c);
    assert(loop.counter_initial_value == 0x9392);
    assert(loop.transfer_opcode == 0x12d8);
    assert(loop.dbf_opcode == 0x51c8);
    assert(loop.dbf_displacement == -4);
    assert(loop.dbf_target_offset == 0x182);

    auto altered_second_stage = second_stage;
    altered_second_stage[0x170] ^= 0x01;
    bool rejected = false;
    try {
        static_cast<void>(eon::parse_deuteros_atari_direct_vector_transfer_loop(
            altered_second_stage, stage, callees));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
}

void assert_deuteros_atari_direct_vector_transfer_tail(
    const std::vector<std::uint8_t>& second_stage,
    const eon::DeuterosAtariSecondStageProfile& stage,
    const eon::DeuterosAtariDispatchProfile& dispatch) {
    const auto callees = eon::parse_deuteros_atari_direct_vector_callees(second_stage, stage, dispatch);
    const auto loop = eon::parse_deuteros_atari_direct_vector_transfer_loop(
        second_stage, stage, callees);
    const auto state5_return = eon::parse_deuteros_atari_state5_return(second_stage, stage, dispatch);
    const auto callback = eon::parse_deuteros_atari_supervisor_callback(second_stage, stage);
    const auto continuation = eon::parse_deuteros_atari_supervisor_callback_continuation(
        second_stage, stage, callback);
    const auto profiles = eon::parse_deuteros_atari_post_callback_callee_profiles(
        second_stage, stage, continuation);
    const auto wrapper = eon::parse_deuteros_atari_raw_reader_wrapper(second_stage, stage, profiles);
    const auto tail = eon::parse_deuteros_atari_direct_vector_transfer_tail(
        second_stage, stage, callees, loop, wrapper, state5_return);
    assert(tail.tail_offset == 0x18c);
    assert(tail.tail_byte_count == 26);
    assert(tail.tail_sha256
        == "45ac9d176b63fa93e16475543939d2f16b4e98cc839b44d2ce2ba9358e978083");
    assert(tail.first_immediate_adjust_opcode == 0x0687);
    assert(tail.first_immediate_adjust_value == 0xb400);
    assert(tail.second_immediate_adjust_opcode == 0x0681);
    assert(tail.second_immediate_adjust_value == 0xb400);
    assert(tail.literal_load_opcode == 0x203c);
    assert(tail.literal_load_value == 0x4c800);
    assert(tail.range_wrapper_bsr_opcode == 0x6100);
    assert(tail.range_wrapper_bsr_displacement == -368);
    assert(tail.range_wrapper_bsr_target_offset == 0x30);
    assert(tail.dispatcher_return_bra_opcode == 0x6000);
    assert(tail.dispatcher_return_bra_displacement == -144);
    assert(tail.dispatcher_return_bra_target_offset == 0x114);

    auto altered_second_stage = second_stage;
    altered_second_stage[0x18c] ^= 0x01;
    bool rejected = false;
    try {
        static_cast<void>(eon::parse_deuteros_atari_direct_vector_transfer_tail(
            altered_second_stage, stage, callees, loop, wrapper, state5_return));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
}

void assert_deuteros_atari_state_selection_layout(const std::vector<std::uint8_t>& second_stage,
    const eon::DeuterosAtariSecondStageProfile& stage,
    const eon::DeuterosAtariDispatchProfile& dispatch) {
    const auto layout = eon::parse_deuteros_atari_state_selection_layout(second_stage, stage, dispatch);
    assert(layout.input_capture_offset == 0xc4);
    assert(layout.input_capture_byte_count == 12);
    assert(layout.input_capture_sha256
        == "03cf620d981a775fd1adabe55deea940e08760e3e49c62cd0643c22b5aa08082");
    assert(layout.source_longword_load_opcode == 0x2038);
    assert(layout.source_longword_address == 0x25fc);
    assert(layout.state_word_store_opcode == 0x31c0);
    assert(layout.state_word_address == 0x1eaa);
    assert(layout.table_lookup_offset == 0xf2);
    assert(layout.table_lookup_byte_count == 22);
    assert(layout.table_lookup_sha256
        == "8e8551a51a7b989e6d2b7d1535819dea658a4e3e64562737755125c13c8f0d3c");
    assert(layout.table_base_load_opcode == 0x43f8);
    assert(layout.table_base_address == 0x1eac);
    assert(layout.state_word_load_opcode == 0x3038);
    assert(layout.index_shift_opcode == 0xe548);
    assert(layout.indexed_vector_load_opcode == 0x2271);
    assert(layout.indexed_vector_displacement == 0);
    assert(layout.indirect_call_opcode == 0x4e91);

    auto altered_second_stage = second_stage;
    altered_second_stage[0xf2] ^= 0x01;
    bool rejected = false;
    try {
        static_cast<void>(eon::parse_deuteros_atari_state_selection_layout(
            altered_second_stage, stage, dispatch));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
}

void assert_deuteros_atari_state_selection_continuation(const std::vector<std::uint8_t>& second_stage,
    const eon::DeuterosAtariSecondStageProfile& stage,
    const eon::DeuterosAtariStateSelectionLayout& layout,
    const eon::DeuterosAtariRawReaderWrapperProfile& wrapper) {
    const auto continuation = eon::parse_deuteros_atari_state_selection_continuation(
        second_stage, stage, layout, wrapper);
    assert(continuation.continuation_offset == 0x108);
    assert(continuation.continuation_byte_count == 18);
    assert(continuation.continuation_sha256
        == "e9ae4bd51bb06c6cb57ac7f26e81497995f7639f99a12e2a149194a39589e16c");
    assert(continuation.indirect_call_opcode == 0x4e91);
    assert(continuation.d1_stack_save_opcode == 0x2f01);
    assert(continuation.raw_reader_argument_advance_opcode == 0xc4fc);
    assert(continuation.raw_reader_argument_advance_bytes == 0x1200);
    assert(continuation.d2_to_d7_opcode == 0x2e02);
    assert(continuation.raw_reader_wrapper_bsr_opcode == 0x6100);
    assert(continuation.raw_reader_wrapper_bsr_displacement == -226);
    assert(continuation.raw_reader_wrapper_target_offset == 0x30);
    assert(continuation.state_word_return_opcode == 0x3038);
    assert(continuation.state_word_address == 0x1eaa);
    assert(continuation.return_opcode == 0x4e75);

    auto altered_second_stage = second_stage;
    altered_second_stage[0x108] ^= 0x01;
    bool rejected = false;
    try {
        static_cast<void>(eon::parse_deuteros_atari_state_selection_continuation(
            altered_second_stage, stage, layout, wrapper));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
}

void assert_deuteros_atari_state1_service_boundary(const std::vector<std::uint8_t>& second_stage,
    const eon::DeuterosAtariSecondStageProfile& stage,
    const eon::DeuterosAtariDispatchProfile& dispatch) {
    const auto boundary = eon::parse_deuteros_atari_state1_service_boundary(
        second_stage, stage, dispatch);
    assert(boundary.callee_offset == 0x12e);
    assert(boundary.callee_byte_count == 34);
    assert(boundary.callee_sha256
        == "0bc76b22089d008e4ce90d63216c75acbe0786b0a06127fbd66ef0dc252949ac");
    assert(boundary.longword_push_opcode == 0x2f3c);
    assert(boundary.longword_argument == 0x2630);
    assert(boundary.selector_push_opcode == 0x3f3c);
    assert(boundary.xbios_selector == 0x26);
    assert(boundary.trap_opcode == 0x4e4e);
    assert(boundary.stack_cleanup_opcode == 0x5c8f);
    assert(boundary.stack_cleanup_bytes == 6);
    assert(boundary.destination_load_opcode == 0x223c);
    assert(boundary.destination == 0xb000);
    assert(boundary.byte_count_load_opcode == 0x203c);
    assert(boundary.byte_count == 0x5e400);
    assert(boundary.linear_sector_load_opcode == 0x243c);
    assert(boundary.linear_sector == 0x4c);
    assert(boundary.return_opcode == 0x4e75);

    auto altered_second_stage = second_stage;
    altered_second_stage[0x12e] ^= 0x01;
    bool rejected = false;
    try {
        static_cast<void>(eon::parse_deuteros_atari_state1_service_boundary(
            altered_second_stage, stage, dispatch));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
}

void assert_deuteros_amiga_post_exec_state_init(const std::vector<std::uint8_t>& system_adf,
    const eon::AmigaAdf& disk, const eon::DeuterosAmigaLoadPlan& plan) {
    const auto profile = eon::parse_deuteros_amiga_title_post_exec_state_init_profile(disk, plan);
    assert(profile.caller_address == 0x403fa);
    assert(profile.entry_address == 0x20510);
    assert(profile.cleared_word_address == 0x202c4);
    assert(profile.cleared_word_value == 0);
    assert(profile.initial_word_address == 0x2027e);
    assert(profile.initial_word_value == 0xf690);
    assert(profile.initial_long_address == 0x20280);
    assert(profile.initial_long_value == 1);
    assert(profile.copied_word_source_address == 0x20276);
    assert(profile.copied_word_destination_address == 0x2027c);
    assert(profile.return_address == 0x20536);
    assert(profile.caller_sha256
        == "f31dc5923e4b39eb1726fc9b05ac7f56c0209f5d60c9499b979ebfc7c08a58a2");
    assert(profile.routine_sha256
        == "60ee2fcb4a18f62cd2066aba2429e760a64f14cd3f07f3cfe8467972030008bc");
    for (const auto disk_offset : {0x9b3faU, 0x7c510U}) {
        auto altered = system_adf;
        altered[disk_offset] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_deuteros_amiga_title_post_exec_state_init_profile(
                eon::AmigaAdf(std::move(altered)), plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
}

void assert_deuteros_amiga_post_exec_third_service(const std::vector<std::uint8_t>& system_adf,
    const eon::AmigaAdf& disk, const eon::DeuterosAmigaLoadPlan& plan) {
    const auto profile = eon::parse_deuteros_amiga_title_post_exec_third_service_profile(disk, plan);
    assert(profile.caller_address == 0x40400);
    assert(profile.dispatch_entry_address == 0x1f37a);
    assert(profile.graphics_service_address == 0x20094);
    assert(profile.graphics_library_base_address == 0x12fec);
    assert((profile.graphics_library_vectors == std::array<std::int16_t, 3>{{-0x19e, -0x198, -0x1a4}}));
    assert(profile.status_byte_address == 0x20092);
    assert(profile.destination_pointer_literal == 0x1ffe6);
    assert(profile.destination_pointer_cell_address == 0x2008e);
    assert(profile.descriptor_address == 0x1ffda);
    assert((profile.descriptor_offsets == std::array<std::uint16_t, 3>{{0x0006, 0x0008, 0x0004}}));
    assert((profile.descriptor_values == std::array<std::uint16_t, 3>{{0x000a, 0x000a, 0x000c}}));
    assert(profile.service_return_address == 0x200fa);
    assert(profile.dispatcher_a6_literal == 0x1f372);
    assert(profile.dispatcher_tail_jump_address == 0x201d2);
    assert(profile.caller_sha256
        == "901b0ad5740a3e6aea3eba28b6aadf5ac5c187e961cc848f6f1a882b3592f464");
    assert(profile.dispatch_sha256
        == "58e85705bc821d42834936342b242162c749889b9d9c23c3d5896f7bcf06e4ff");
    assert(profile.service_sha256
        == "7427cdaa0f716496e21c5ef0f6a8d0850a9606a9b4ffe6e56df599109b0ca947");
    for (const auto disk_offset : {0x9b400U, 0x7a37aU, 0x7b094U}) {
        auto altered = system_adf;
        altered[disk_offset] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_deuteros_amiga_title_post_exec_third_service_profile(
                eon::AmigaAdf(std::move(altered)), plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
}

} // namespace

int main() {
    // Modern Scale2x is a renderer-only, in-memory reconstruction. This
    // asymmetric pattern proves it is not merely a texture filtering mode and
    // that it cannot write through its input span.
    std::vector<std::uint8_t> reconstruction_source(3U * 3U * 4U, 0);
    for (std::size_t pixel = 0; pixel < 9; ++pixel) reconstruction_source[pixel * 4U + 3U] = 255;
    const auto set_reconstruction_pixel = [&](const std::size_t x, const std::size_t y,
                                               const std::array<std::uint8_t, 4>& rgba) {
        std::copy(rgba.begin(), rgba.end(), reconstruction_source.begin() + (y * 3U + x) * 4U);
    };
    // Around E (1,1): B and D agree while F and H differ. Scale2x therefore
    // chooses D for E0 rather than retaining E in every output position.
    set_reconstruction_pixel(1, 0, {255, 0, 0, 255});
    set_reconstruction_pixel(0, 1, {255, 0, 0, 255});
    set_reconstruction_pixel(1, 1, {0, 0, 0, 255});
    set_reconstruction_pixel(2, 1, {0, 0, 255, 255});
    set_reconstruction_pixel(1, 2, {0, 255, 0, 255});
    const auto source_copy = reconstruction_source;
    const auto reconstructed = eon::reconstruct_rgba_scale2x(reconstruction_source, 3, 3);
    assert(reconstructed.width == 6 && reconstructed.height == 6);
    assert(reconstructed.rgba.size() == 144);
    assert(reconstruction_source == source_copy);
    const std::array<std::uint8_t, 4> expected_edge_pixel{{255, 0, 0, 255}};
    assert(std::equal(expected_edge_pixel.begin(), expected_edge_pixel.end(),
        reconstructed.rgba.begin() + (2U * 6U + 2U) * 4U));
    bool malformed_reconstruction_rejected = false;
    try {
        static_cast<void>(eon::reconstruct_rgba_scale2x(reconstruction_source, 2, 3));
    } catch (const std::runtime_error&) {
        malformed_reconstruction_rejected = true;
    }
    assert(malformed_reconstruction_rejected);
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
        char data_directory_option[] = "--data-dir";
        char* explicit_directory_args[] = {program, data_directory_option, custom_path};
        const auto explicit_directory = eon::parse_command_line(3, explicit_directory_args);
        assert(explicit_directory.request && !explicit_directory.request->data_directory_is_default);
        assert(explicit_directory.request->data_directory == "original-media");
        char inspect_option[] = "--inspect";
        char millennium[] = "millennium";
        char* inspect_args[] = {program, inspect_option};
        const auto inspect = eon::parse_command_line(2, inspect_args);
        assert(inspect.request && inspect.request->inspect_data);
        char game_option[] = "--game";
        char platform_option[] = "--platform";
        char dos[] = "dos";
        char* targeted_inspect_args[] = {
            program, inspect_option, game_option, millennium, platform_option, dos};
        const auto targeted_inspect = eon::parse_command_line(6, targeted_inspect_args);
        assert(targeted_inspect.request && targeted_inspect.request->inspect_data);
        assert(targeted_inspect.request->game == eon::Game::millennium);
        assert(targeted_inspect.request->platform == eon::Platform::dos);
        char verify_option[] = "--verify-data";
        char* conflicting_args[] = {program, inspect_option, verify_option, millennium};
        const auto conflict = eon::parse_command_line(4, conflicting_args);
        assert(!conflict.request);
        char trace_option[] = "--reference-trace";
        char trace_manifest[] = "capture.eontrace";
        char amiga[] = "amiga";
        char* trace_args[] = {program, data_option, custom_path, game_option, millennium,
            platform_option, amiga, trace_option, trace_manifest};
        const auto trace = eon::parse_command_line(9, trace_args);
        assert(trace.request && trace.request->reference_trace == "capture.eontrace");
        char* trace_without_platform[] = {program, data_option, custom_path, game_option, millennium,
            trace_option, trace_manifest};
        assert(!eon::parse_command_line(7, trace_without_platform).request);
        char resolution_option[] = "--resolution";
        char resolution[] = "1600x900";
        char aspect_option[] = "--aspect";
        char aspect[] = "square-pixels";
        char* display_args[] = {program, resolution_option, resolution, aspect_option, aspect};
        const auto display = eon::parse_command_line(5, display_args);
        assert(display.request && display.request->display.width == 1600
            && display.request->display.height == 900
            && display.request->display.aspect_ratio_index == 1);
        char unsupported_resolution[] = "1024x768";
        char* unsupported_display_args[] = {program, resolution_option, unsupported_resolution};
        assert(!eon::parse_command_line(3, unsupported_display_args).request);
        char unknown_aspect[] = "squished";
        char* unknown_aspect_args[] = {program, aspect_option, unknown_aspect};
        assert(!eon::parse_command_line(3, unknown_aspect_args).request);

        const std::vector<eon::ReleaseArchive> menu_releases{
            {eon::Game::millennium, eon::Platform::dos, "en", {}, {}},
            {eon::Game::deuteros, eon::Platform::amiga, "en", {}, {}},
            {eon::Game::deuteros, eon::Platform::atari_st, "en", {}, {}},
        };
        assert(eon::select_available_platform(
            menu_releases, eon::Game::millennium, eon::Platform::dos) == eon::Platform::dos);
        assert(eon::select_available_platform(
            menu_releases, eon::Game::deuteros, eon::Platform::dos) == eon::Platform::amiga);
        assert(!eon::select_available_platform({}, eon::Game::deuteros, eon::Platform::amiga));

        assert(eon::normalize_language("sv_SE.UTF-8") == "sv_SE");
        assert(eon::normalize_language("pt-BR") == "pt_BR");
        assert(eon::normalize_language("C") == "c");
    assert(eon::normalize_language("not a locale").empty());
    // The Unicode launcher renderer must never consult a system font when no
    // reviewed bundled asset is present. This is a no-renderer/no-font native
    // contract and therefore requires no host font in CI.
    assert(!eon::UnicodeTextRenderer::create(nullptr,
        std::filesystem::temp_directory_path() / "project-eon-no-host-font.ttf"));
        char language_option[] = "--language";
        char swedish[] = "sv_SE.UTF-8";
        char* language_args[] = {program, language_option, swedish};
        const auto language = eon::parse_command_line(3, language_args);
        assert(language.request && language.request->language == "sv_SE");

        const auto temporary_po = std::filesystem::temp_directory_path() / "project-eon-i18n-test.po";
        {
            std::ofstream po(temporary_po, std::ios::binary);
            po << "msgid \"\"\nmsgstr \"header\"\n\n"
                  "msgid \"Start game\"\nmsgstr \"Starta spel\"\n\n"
                  "#, fuzzy\nmsgid \"Ignore\"\nmsgstr \"Ignorera\"\n";
        }
        const auto translations = eon::Translator::from_po_file(temporary_po);
        assert(translations.translate("Start game") == "Starta spel");
        assert(translations.translate("Ignore") == "Ignore");
        assert(translations.translate("Missing") == "Missing");
        std::filesystem::remove(temporary_po);

        const auto swedish_catalog = eon::Translator::from_language("sv");
        assert(swedish_catalog.translate("ENTER / CLICK: START") == "ENTER / KLICKA: STARTA");
        const auto portuguese_catalog = eon::Translator::from_language("pt_BR.UTF-8");
        assert(!portuguese_catalog.empty());
        const auto chinese_catalog = eon::Translator::from_language("zh_CN.UTF-8");
        assert(!chinese_catalog.empty());
        const auto british_catalog = eon::Translator::from_language("en_GB.UTF-8");
        assert(!british_catalog.empty());
    }
    {
        // A minimal stored ZIP anchors parser integrity checks without using
        // synthetic game data. It has one empty entry named "a".
        std::vector<std::uint8_t> zip;
        const auto append16 = [&zip](const std::uint16_t value) {
            zip.push_back(static_cast<std::uint8_t>(value & 0xffU));
            zip.push_back(static_cast<std::uint8_t>(value >> 8U));
        };
        const auto append32 = [&append16](const std::uint32_t value) {
            append16(static_cast<std::uint16_t>(value & 0xffffU));
            append16(static_cast<std::uint16_t>(value >> 16U));
        };
        append32(0x04034b50U);
        append16(20); append16(0); append16(0); append16(0); append16(0);
        append32(0); append32(0); append32(0); append16(1); append16(0);
        zip.push_back('a');
        append32(0x02014b50U);
        append16(20); append16(20); append16(0); append16(0); append16(0); append16(0);
        append32(0); append32(0); append32(0); append16(1); append16(0); append16(0);
        append16(0); append16(0); append32(0); append32(0);
        zip.push_back('a');
        append32(0x06054b50U);
        append16(0); append16(0); append16(1); append16(1); append32(47); append32(31); append16(0);
        const eon::ZipArchive valid_zip(zip);
        assert(valid_zip.entries().size() == 1);
        assert(valid_zip.extract(valid_zip.entries().front()).empty());

        // EOCD comments are part of the original archive byte stream.  A
        // marker-looking sequence inside one is data, not a second archive
        // footer.  The parser must locate the EOCD whose declared comment
        // reaches the physical end rather than accepting the later marker.
        auto commented_zip = zip;
        commented_zip[98] = 31;
        commented_zip[99] = 0;
        commented_zip.insert(commented_zip.end(), 31, 0);
        commented_zip[108] = 0x50;
        commented_zip[109] = 0x4b;
        commented_zip[110] = 0x05;
        commented_zip[111] = 0x06;
        const eon::ZipArchive comment_zip(std::move(commented_zip));
        assert(comment_zip.entries().size() == 1);
        assert(comment_zip.extract(comment_zip.entries().front()).empty());

        auto truncated_comment = zip;
        truncated_comment[98] = 1;
        bool rejected_truncated_comment = false;
        try {
            static_cast<void>(eon::ZipArchive(std::move(truncated_comment)));
        } catch (const std::runtime_error&) {
            rejected_truncated_comment = true;
        }
        assert(rejected_truncated_comment);

        auto central_gap = zip;
        // The central directory's recorded length begins at EOCD - 10.
        central_gap[90] = 46;
        central_gap[91] = 0;
        bool rejected_central_gap = false;
        try {
            static_cast<void>(eon::ZipArchive(std::move(central_gap)));
        } catch (const std::runtime_error&) {
            rejected_central_gap = true;
        }
        assert(rejected_central_gap);

        auto mismatched_local_name = zip;
        mismatched_local_name[30] = 'b';
        bool rejected_local_name = false;
        try {
            const eon::ZipArchive local_name_zip(std::move(mismatched_local_name));
            static_cast<void>(local_name_zip.extract(local_name_zip.entries().front()));
        } catch (const std::runtime_error&) {
            rejected_local_name = true;
        }
        assert(rejected_local_name);

        auto invalid_central_size = zip;
        invalid_central_size[90] = 48;
        bool rejected_central_size = false;
        try {
            static_cast<void>(eon::ZipArchive(std::move(invalid_central_size)));
        } catch (const std::runtime_error&) {
            rejected_central_size = true;
        }
        assert(rejected_central_size);

        auto encrypted_entry = zip;
        encrypted_entry[39] = 1;
        bool rejected_encryption = false;
        try {
            static_cast<void>(eon::ZipArchive(std::move(encrypted_entry)));
        } catch (const std::runtime_error&) {
            rejected_encryption = true;
        }
        assert(rejected_encryption);

        // Archive names become preservation-inventory paths. They therefore
        // must be unambiguous and never resemble a host extraction path.
        auto absolute_name = zip;
        absolute_name[30] = '/';
        absolute_name[77] = '/';
        bool rejected_absolute_name = false;
        try {
            static_cast<void>(eon::ZipArchive(std::move(absolute_name)));
        } catch (const std::runtime_error&) {
            rejected_absolute_name = true;
        }
        assert(rejected_absolute_name);

        auto backslash_name = zip;
        backslash_name[30] = '\\';
        backslash_name[77] = '\\';
        bool rejected_backslash_name = false;
        try {
            static_cast<void>(eon::ZipArchive(std::move(backslash_name)));
        } catch (const std::runtime_error&) {
            rejected_backslash_name = true;
        }
        assert(rejected_backslash_name);

        // Two central records selecting one local member make a hash lookup
        // ambiguous even when their payload happens to match. Reject it at
        // open time rather than exposing a first-entry-wins inventory.
        auto duplicate_entry = zip;
        const std::vector<std::uint8_t> central_record(zip.begin() + 31, zip.begin() + 78);
        duplicate_entry.insert(duplicate_entry.begin() + 78, central_record.begin(), central_record.end());
        // The EOCD moved by one central record: it now declares two entries
        // and a 94-byte central directory beginning at its original offset.
        duplicate_entry[133] = 2;
        duplicate_entry[135] = 2;
        duplicate_entry[137] = 94;
        bool rejected_duplicate_entry = false;
        try {
            static_cast<void>(eon::ZipArchive(std::move(duplicate_entry)));
        } catch (const std::runtime_error&) {
            rejected_duplicate_entry = true;
        }
        assert(rejected_duplicate_entry);

        auto local_entry_in_central_directory = zip;
        local_entry_in_central_directory[73] = 31;
        bool rejected_local_entry_in_central_directory = false;
        try {
            static_cast<void>(eon::ZipArchive(std::move(local_entry_in_central_directory)));
        } catch (const std::runtime_error&) {
            rejected_local_entry_in_central_directory = true;
        }
        assert(rejected_local_entry_in_central_directory);
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
    assert(incremental_scanner.report().candidates == incremental_scanner.candidate_count());
    // Other user files may share a release's byte length, so only the hash
    // identity count is corpus-fixed. Every size-admitted candidate is either
    // completely hashed or explicitly accounted as unreadable.
    assert(incremental_scanner.report().size_candidates >= 6);
    assert(incremental_scanner.report().hashed_candidates
        + incremental_scanner.report().unreadable_candidates
        == incremental_scanner.report().size_candidates);
    assert(incremental_scanner.report().verified_occurrences == 6);
    assert(incremental_scanner.report().duplicate_occurrences == 0);
    assert(incremental_scanner.report().unreadable_candidates == 0);
    const auto releases = eon::find_release_archives(data_directory);
    // Six genuine outer archives: five platform/game pairs plus Spanish DOS.
    assert(releases.size() == 6);
    assert((eon::available_platforms(releases, eon::Game::millennium)
        == std::vector<eon::Platform>{eon::Platform::dos, eon::Platform::amiga,
            eon::Platform::atari_st}));
    assert((eon::available_platforms(releases, eon::Game::deuteros)
        == std::vector<eon::Platform>{eon::Platform::amiga, eon::Platform::atari_st}));
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
    assert(eon::deuteros_amiga_opening_supported(std::nullopt));
    assert(eon::deuteros_amiga_opening_supported(eon::Platform::amiga));
    assert(!eon::deuteros_amiga_opening_supported(eon::Platform::atari_st));

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
    // The profile manifest is an executable preservation contract: every
    // admitted span must belong to its exact leaf in its exact outer archive.
    for (const auto& profile : eon::parser_profile_manifest()) {
        assert(profile.offset <= profile.leaf_size);
        assert(profile.length <= profile.leaf_size - profile.offset);
        const auto release = std::find_if(releases.begin(), releases.end(),
            [&profile](const auto& candidate) { return candidate.sha256 == profile.release_sha256; });
        assert(release != releases.end());
        const auto leaves = eon::inventory_zip(release->path);
        const auto leaf = std::find_if(leaves.begin(), leaves.end(), [&profile](const auto& candidate) {
            return candidate.sha256 == profile.leaf_sha256 && candidate.size == profile.leaf_size;
        });
        assert(leaf != leaves.end());
        assert(eon::release_has_parser_profile(release->sha256, profile.id));
    }
    assert(!eon::release_has_parser_profile(
        "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
        "millennium-dos-spanish-startup"));
    // The declarative recovery map is a second, narrower admission layer:
    // every visible code-path fact retains its exact release/profile identity
    // and cannot become a hook or a cross-release fallback.
    assert(eon::recovery_map().size() == eon::parser_profile_manifest().size());
    for (const auto& profile : eon::parser_profile_manifest()) {
        const auto count = std::count_if(eon::recovery_map().begin(), eon::recovery_map().end(),
            [&profile](const auto& entry) {
                return entry.release_sha256 == profile.release_sha256
                    && entry.parser_profile_id == profile.id;
            });
        assert(count == 1);
    }
    for (const auto& entry : eon::recovery_map()) {
        assert(eon::release_has_parser_profile(entry.release_sha256, entry.parser_profile_id));
        assert(eon::release_has_recovery_map_entry(entry.release_sha256, entry.id));
        const auto mapped_release = std::find_if(releases.begin(), releases.end(),
            [&entry](const auto& candidate) { return candidate.sha256 == entry.release_sha256; });
        assert(mapped_release != releases.end());
        assert(mapped_release->game == entry.game);
        assert(mapped_release->platform == entry.platform);
        assert(mapped_release->language == entry.language);
    }
    assert(!eon::release_has_recovery_map_entry(
        "b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4",
        "millennium-dos-title-flow"));
    const auto amiga_map = eon::recovery_map_for_release(
        "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04");
    assert(amiga_map.size() == 3);
    for (const auto& entry : amiga_map) assert(entry.game == eon::Game::deuteros);

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
    // The original bootstrap/first-stage bytes differ across supplied Amiga
    // releases, but every genuine image carries this exact raw resident span.
    // Validate that common evidence directly from each image, including the
    // shorter Defjam [u] image whose resident interval is still complete.
    const std::array<std::string_view, 6> millennium_amiga_hashes{{
        "8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c",
        "41dfea33ee6eb2bc80573b7819bf313e8b214ac2910208586a1bee88fd14fb95",
        "756745ee6dff5e7ea3312ca37424c0089bf0db566fb28761d55ae02da26aadf0",
        "f3ec073ecdc40c8b858658aa9854ae9de66dc50787fee29a930c28d393d2a502",
        "fe83c10119ef9bf2953b6fcd9a13d07f2c276215aaa64e2e541402a527a616f2",
        "f12ac46debdbaca04f54475cb766324a180323419d560a3fbd271c901cca52c1",
    }};
    for (const auto hash : millennium_amiga_hashes) {
        const auto image = eon::extract_asset_by_sha256(amiga_millennium->path, hash);
        assert(image);
        const auto shared = eon::parse_millennium_amiga_shared_resident_layout(*image);
        assert(shared.disk_offset == 0x16400);
        assert(shared.length == 0x2c000);
        assert(shared.destination == 0x68000);
        assert(shared.raw_sha256
            == "d144abc05f891710dc99b30d87f020bd6e2ff7796ef86a847f07b8d97d55d18e");
    }
    {
        auto altered = *razor_adf;
        altered[0x17a24] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_amiga_shared_resident_layout(altered));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
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
    const eon::MillenniumAmigaBootstrapSession defjam_session(*defjam_adf);
    assert(defjam_session.plan().loader_magic == 0xa8d398fb);
    assert(defjam_session.shared_resident().raw_sha256
        == "d144abc05f891710dc99b30d87f020bd6e2ff7796ef86a847f07b8d97d55d18e");
    assert(defjam_session.resident_entry().entry_address == 0x68000);
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
    const auto defjam_opaque_invocation =
        eon::parse_millennium_amiga_bootstrap_opaque_invocation_boundary(
            defjam_loader_disk, defjam_plan);
    assert(defjam_opaque_invocation.entry_address == 0x7029e);
    assert(defjam_opaque_invocation.raw_disk_offset == 0x69e);
    assert(defjam_opaque_invocation.byte_count == 132);
    assert(defjam_opaque_invocation.sha256
        == "b8ca18e61e5372ba4387abd69f6796435671465ddaf48cd3a3e4b41e2528efdc");
    assert(defjam_opaque_invocation.first_stage_invocation_address == 0x702e4);
    assert(defjam_opaque_invocation.first_stage_target == 0x41000);
    assert(defjam_opaque_invocation.static_post_first_stage_address == 0x702e6);
    assert(defjam_opaque_invocation.resident_stage_jump_address == 0x70320);
    assert(defjam_opaque_invocation.resident_stage_target == 0x68000);
    const auto defjam_first_stage_anchors =
        eon::parse_millennium_amiga_first_stage_source_anchor_boundary(
            defjam_loader_disk, defjam_plan);
    assert(defjam_first_stage_anchors.raw_disk_offset == 0x24200);
    assert(defjam_first_stage_anchors.byte_count == 0x6e000);
    assert(defjam_first_stage_anchors.sha256
        == "5ed30d5fe99c0dfc905bbe639d626be558f022514c83bc5ff287ad91014ccf7a");
    assert((defjam_first_stage_anchors.anchor_stage_offsets
        == std::array<std::uint32_t, 3>{{0x4a3dc, 0x4a648, 0x4a936}}));
    assert((defjam_first_stage_anchors.window_stage_offsets
        == std::array<std::size_t, 2>{{0x4a5b0, 0x4a900}}));
    assert((defjam_first_stage_anchors.window_byte_counts
        == std::array<std::size_t, 2>{{0x160, 0x220}}));
    assert((defjam_first_stage_anchors.window_sha256 == std::array<std::string, 2>{{
        "97bb8cbe026ac3bba2c19cc296bc7cef00fbd0c8095c678f4cc303761b8b8309",
        "ee84336cbf4665bcd2bc48d054c024a20e4c5faaaf26cd5fdcc78e6b8f3931c9",
    }}));
    {
        auto altered = *defjam_adf;
        altered[0x69e] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_amiga_bootstrap_opaque_invocation_boundary(
                eon::AmigaAdf(std::move(altered)), defjam_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered = *defjam_adf;
        altered[0x6e7b0] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_amiga_first_stage_source_anchor_boundary(
                eon::AmigaAdf(std::move(altered)), defjam_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
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
    const auto defjam_predicate_gate = eon::parse_millennium_amiga_resident_predicate_gate(
        defjam_loader_disk, defjam_plan, defjam_splitter);
    assert(defjam_predicate_gate.entry_address == 0x68078);
    assert(defjam_predicate_gate.predicate_address == 0x7b816);
    assert(defjam_predicate_gate.nonzero_return_address == 0x68082);
    assert(defjam_predicate_gate.zero_continue_address == 0x68084);
    assert(defjam_predicate_gate.predicate_raw_disk_offset == 0x29c16);
    assert((defjam_predicate_gate.predicate_raw_prefix == std::array<std::uint8_t, 32>{{
        0x00, 0xc2, 0x40, 0x00, 0x6e, 0x00, 0xc2, 0x00,
        0x04, 0x80, 0x42, 0x00, 0x00, 0xc2, 0x20, 0x00,
        0x6a, 0x00, 0xc2, 0x00, 0x08, 0x4a, 0x00, 0xc2,
        0x00, 0x80, 0x62, 0x00, 0xc2, 0x10, 0x00, 0x80,
    }}));
    assert(defjam_predicate_gate.predicate_raw_prefix_sha256
        == "a16a4738b0f577643c343b344ba8b6c19d935daf97dd2291c86ddb2b29dcd96c");
    const auto defjam_predicate_zero_path =
        eon::parse_millennium_amiga_resident_predicate_zero_path_boundary(
            defjam_loader_disk, defjam_plan, defjam_predicate_gate);
    assert(defjam_predicate_zero_path.entry_address == 0x68084);
    assert(defjam_predicate_zero_path.selector_a1_offset == 0x12);
    assert(defjam_predicate_zero_path.selector_compare_value == 1);
    assert(defjam_predicate_zero_path.selector_not_equal_branch_address == 0x6808e);
    assert(defjam_predicate_zero_path.selector_not_equal_target == 0x680ca);
    assert(defjam_predicate_zero_path.equal_path_argument_a1_offset == 0x14);
    assert(defjam_predicate_zero_path.unknown_call_address == 0x68096);
    assert(defjam_predicate_zero_path.unknown_call_target == 0x7b90a);
    assert(defjam_predicate_zero_path.unknown_call_raw_disk_offset == 0x29d0a);
    assert((defjam_predicate_zero_path.unknown_call_raw_prefix == std::array<std::uint8_t, 32>{{
        0x42, 0x00, 0x00, 0xc2, 0x02, 0x00, 0x42, 0x00,
        0x42, 0x04, 0x42, 0x00, 0xc2, 0x10, 0x08, 0x42,
        0x00, 0xc2, 0x00, 0x10, 0x82, 0x4b, 0x00, 0x00,
        0x00, 0x50, 0x00, 0xa7, 0x81, 0xac, 0x00, 0x00,
    }}));
    assert(defjam_predicate_zero_path.unknown_call_raw_prefix_sha256
        == "bdb907adb3114dbaa58eb3bbe516ab91ffc4e1bf70e536bd47f497f49c8d5042");
    const auto defjam_predicate_not_equal_path =
        eon::parse_millennium_amiga_resident_predicate_not_equal_path_boundary(
            defjam_loader_disk, defjam_plan, defjam_predicate_zero_path);
    assert(defjam_predicate_not_equal_path.entry_address == 0x680ca);
    assert(defjam_predicate_not_equal_path.pushed_first_register == 0);
    assert(defjam_predicate_not_equal_path.pushed_second_register == 2);
    assert(defjam_predicate_not_equal_path.unknown_call_address == 0x680ce);
    assert(defjam_predicate_not_equal_path.unknown_call_target == 0x7b90a);
    const auto defjam_independent_entry =
        eon::parse_millennium_amiga_resident_independent_entry_gate(
            defjam_loader_disk, defjam_plan);
    assert(defjam_independent_entry.entry_address == 0x68508);
    assert(defjam_independent_entry.negative_d3_branch_address == 0x6850e);
    assert(defjam_independent_entry.negative_d3_target == 0x68598);
    assert(defjam_independent_entry.flag_test_address == 0x68512);
    assert(defjam_independent_entry.flag_address == 0x7b142);
    assert(defjam_independent_entry.flag_zero_branch_address == 0x68518);
    assert(defjam_independent_entry.flag_zero_target == 0x6854a);
    const auto defjam_negative_d3 = eon::parse_millennium_amiga_resident_negative_d3_continuation(
        defjam_loader_disk, defjam_plan, defjam_independent_entry);
    assert(defjam_negative_d3.entry_address == 0x68598);
    assert(defjam_negative_d3.external_jump_address == 0x685ee);
    assert(defjam_negative_d3.external_jump_target == 0x7bcf8);
    assert(defjam_negative_d3.return_address == 0x685fc);
    const auto defjam_negative_d3_terminal = eon::parse_millennium_amiga_resident_negative_d3_terminal(
        defjam_loader_disk, defjam_plan, defjam_negative_d3);
    assert(defjam_negative_d3_terminal.entry_address == 0x685f4);
    assert(defjam_negative_d3_terminal.first_add_immediate == 0x2800);
    assert(defjam_negative_d3_terminal.second_add_address == 0x685f8);
    assert(defjam_negative_d3_terminal.second_add_immediate == 0x2800);
    assert(defjam_negative_d3_terminal.return_address == 0x685fc);
    const auto defjam_post_negative_d3 =
        eon::parse_millennium_amiga_resident_post_negative_d3_terminal(
            defjam_loader_disk, defjam_plan, defjam_negative_d3_terminal);
    assert(defjam_post_negative_d3.entry_address == 0x685fe);
    assert((defjam_post_negative_d3.absolute_byte_store_addresses
        == std::array<std::uint32_t, 2>{{0x7b3b5, 0x7b3bc}}));
    assert(defjam_post_negative_d3.d0_test_address == 0x68610);
    assert(defjam_post_negative_d3.nonzero_branch_address == 0x68612);
    assert(defjam_post_negative_d3.nonzero_branch_target == 0x68616);
    assert(defjam_post_negative_d3.zero_return_address == 0x68614);
    assert(defjam_post_negative_d3.nonnegative_branch_address == 0x68616);
    assert(defjam_post_negative_d3.nonnegative_branch_target == 0x6861a);
    assert(defjam_post_negative_d3.negative_return_address == 0x68618);
    assert(defjam_post_negative_d3.raw_sha256
        == "a45ff5eca6e3594574b464574fa0aae3027bd2ea11472770708c96f4d21b56cc");
    const auto defjam_post_negative_d3_continuation =
        eon::parse_millennium_amiga_resident_post_negative_d3_continuation_boundary(
            defjam_loader_disk, defjam_plan, defjam_post_negative_d3);
    assert(defjam_post_negative_d3_continuation.entry_address == 0x6861a);
    assert((defjam_post_negative_d3_continuation.add_immediates
        == std::array<std::uint16_t, 3>{{0x2800, 0x2800, 0x2800}}));
    assert(defjam_post_negative_d3_continuation.range_base_immediate == 0x7d00);
    assert(defjam_post_negative_d3_continuation.compare_branch_address == 0x68636);
    assert(defjam_post_negative_d3_continuation.compare_branch_target == 0x6863a);
    assert(defjam_post_negative_d3_continuation.low_range_branch_address == 0x68642);
    assert(defjam_post_negative_d3_continuation.low_range_branch_target == 0x68650);
    assert(defjam_post_negative_d3_continuation.negative_range_branch_address == 0x68644);
    assert(defjam_post_negative_d3_continuation.negative_range_branch_target == 0x68694);
    assert(defjam_post_negative_d3_continuation.terminal_jump_address == 0x6864a);
    assert(defjam_post_negative_d3_continuation.terminal_jump_target == 0x7bef0);
    assert(defjam_post_negative_d3_continuation.raw_disk_offset == 0x16a1a);
    assert(defjam_post_negative_d3_continuation.byte_count == 54);
    assert(defjam_post_negative_d3_continuation.raw_sha256
        == "d3f6b63090429e11fb3a77e4573817649e2bb7996d06811ea2751078794534ce");
    {
        auto altered = *defjam_adf;
        altered[0x16a1a] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(
                eon::parse_millennium_amiga_resident_post_negative_d3_continuation_boundary(
                    eon::AmigaAdf(std::move(altered)), defjam_plan, defjam_post_negative_d3));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    const auto defjam_independent_zero_target =
        eon::parse_millennium_amiga_resident_independent_zero_target_boundary(
            defjam_loader_disk, defjam_plan, defjam_independent_entry);
    assert(defjam_independent_zero_target.entry_address == 0x6854a);
    assert(defjam_independent_zero_target.compare_immediate == 0x0120);
    assert(defjam_independent_zero_target.conditional_branch_address == 0x6854e);
    assert(defjam_independent_zero_target.conditional_branch_target == 0x68562);
    const auto defjam_independent_compare_target =
        eon::parse_millennium_amiga_resident_independent_compare_target_boundary(
            defjam_loader_disk, defjam_plan, defjam_independent_zero_target);
    assert(defjam_independent_compare_target.entry_address == 0x68562);
    assert(defjam_independent_compare_target.conditional_branch_address == 0x6856a);
    assert(defjam_independent_compare_target.conditional_branch_target == 0x6857a);
    const auto defjam_independent_branch_target = eon::parse_millennium_amiga_resident_independent_branch_target_boundary(defjam_loader_disk, defjam_plan, defjam_independent_compare_target);
    assert(defjam_independent_branch_target.entry_address == 0x6857a);
    assert(defjam_independent_branch_target.conditional_branch_address == 0x68580);
    assert(defjam_independent_branch_target.conditional_branch_target == 0x68586);
    const auto defjam_independent_branch_preparation = eon::parse_millennium_amiga_resident_independent_branch_preparation_boundary(defjam_loader_disk, defjam_plan, defjam_independent_branch_target);
    assert(defjam_independent_branch_preparation.entry_address == 0x68586);
    assert(defjam_independent_branch_preparation.unknown_call_address == 0x68590);
    assert(defjam_independent_branch_preparation.unknown_call_target == 0x7b26a);
    const auto defjam_separate_entry = eon::parse_millennium_amiga_resident_separate_entry_gate(defjam_loader_disk, defjam_plan);
    assert(defjam_separate_entry.entry_address == 0x68d50);
    assert(defjam_separate_entry.branch_address == 0x68d58);
    assert(defjam_separate_entry.branch_target == 0x68d62);
    const auto defjam_separate_branch = eon::parse_millennium_amiga_resident_separate_branch_boundary(defjam_loader_disk, defjam_plan, defjam_separate_entry);
    assert(defjam_separate_branch.entry_address == 0x68d62);
    assert(defjam_separate_branch.long_branch_address == 0x68d6e);
    assert(defjam_separate_branch.long_branch_target == 0x68d78);
    assert(defjam_separate_branch.unknown_call_address == 0x68d7c);
    assert(defjam_separate_branch.unknown_call_target == 0x778f0);
    const auto defjam_separate_post_call = eon::parse_millennium_amiga_resident_separate_post_call_boundary(defjam_loader_disk, defjam_plan, defjam_separate_branch);
    assert(defjam_separate_post_call.entry_address == 0x68d82);
    assert(defjam_separate_post_call.raw_disk_offset == 0x17182);
    assert(defjam_separate_post_call.sha256 == "e49e750f78946956c22d4cd80206139d38808d4ecb3b1579906aeaede0db7b77");
    assert(defjam_separate_post_call.d0_immediate == 0x2208);
    assert(defjam_separate_post_call.a5_source_address == 0x6934e);
    assert(defjam_separate_post_call.stored_d0_address == 0x7c256);
    assert(defjam_separate_post_call.following_call_address == 0x68d96);
    assert(defjam_separate_post_call.following_call_target == 0x7b342);
    assert(defjam_separate_post_call.following_target_raw_disk_offset == 0x29742);
    assert(defjam_separate_post_call.following_target_prefix_sha256 == "731d016983d29dcb23abad28f3f0f225bd3708073e8c0c8481a97a50b460cdcf");
    const auto defjam_separate_post_call_tail =
        eon::parse_millennium_amiga_resident_separate_post_call_tail_boundary(
            defjam_loader_disk, defjam_plan, defjam_separate_post_call);
    assert(defjam_separate_post_call_tail.entry_address == 0x68d9c);
    assert(defjam_separate_post_call_tail.raw_disk_offset == 0x1719c);
    assert(defjam_separate_post_call_tail.byte_count == 36);
    assert(defjam_separate_post_call_tail.sha256 == "08c660de1ed6d0b0f535e451c84450397383a923a1808fa9678d3ae85a8cc17b");
    assert((defjam_separate_post_call_tail.call_addresses
        == std::array<std::uint32_t, 6>{{0x68d9c, 0x68da2, 0x68da8, 0x68dae, 0x68db4, 0x68dba}}));
    assert((defjam_separate_post_call_tail.call_targets
        == std::array<std::uint32_t, 6>{{0x7dba8, 0x7d8a8, 0x7d480, 0x7b594, 0x7d5c8, 0x7b36c}}));
    assert((defjam_separate_post_call_tail.target_raw_disk_offsets
        == std::array<std::size_t, 6>{{0x2bfa8, 0x2bca8, 0x2b880, 0x29994, 0x2b9c8, 0x2976c}}));
    assert((defjam_separate_post_call_tail.target_prefix_sha256
        == std::array<std::string, 6>{{
            "b388a3622caeeccac01d793650e63e192de821abc789ca334b6ba00a1475ca34",
            "819055da14479352b3f672e6db10424bdebb90230350b0e8088eb0cb0acbd087",
            "dbb41359b827129e186a7cf2f4d79c7f45f11f4cbe53e964a0633b7ee7070df5",
            "e9aa8c8f766b3486163339990968f9829d29b69c3c991ed2a7fc71c483d16846",
            "de1fdcc69a46a7f661c191fa69cd64a693053f4026708400ca4bc6defe224c79",
            "cbe69ef816a594b6e9c0e8a27d5cacc660920df3a0aebe9a31849c113a3f909f",
        }}));
    const auto defjam_separate_post_call_tail_branch =
        eon::parse_millennium_amiga_resident_separate_post_call_tail_branch_boundary(
            defjam_loader_disk, defjam_plan, defjam_separate_post_call_tail);
    assert(defjam_separate_post_call_tail_branch.entry_address == 0x68dc0);
    assert(defjam_separate_post_call_tail_branch.raw_disk_offset == 0x171c0);
    assert(defjam_separate_post_call_tail_branch.sha256
        == "ef2fe6161118a1b0ac6cee838be9a4dc2b0483ba274a213d3ac653ea6f334e3b");
    assert(defjam_separate_post_call_tail_branch.compared_byte_address == 0x7c255);
    assert(defjam_separate_post_call_tail_branch.compare_immediate == 0x0c);
    assert(defjam_separate_post_call_tail_branch.conditional_branch_address == 0x68dca);
    assert(defjam_separate_post_call_tail_branch.conditional_branch_target == 0x68dec);
    assert(defjam_separate_post_call_tail_branch.target_raw_disk_offset == 0x171ec);
    assert(defjam_separate_post_call_tail_branch.target_prefix_sha256
        == "13ed782f5463fd93bbd4376777a1c01d8fd636018de8aef52f5710eb0da11a2b");
    const auto defjam_separate_comparison =
        eon::parse_millennium_amiga_resident_separate_comparison_boundary(
            defjam_loader_disk, defjam_plan, defjam_separate_post_call_tail_branch);
    assert(defjam_separate_comparison.entry_address == 0x68e6c);
    assert(defjam_separate_comparison.raw_disk_offset == 0x1726c);
    assert(defjam_separate_comparison.sha256
        == "8cb29601f0c76406930e37d44b29853501857c36f3cb833ccdd32e78418597d4");
    assert(defjam_separate_comparison.preceding_branch_address == 0x68e0c);
    assert(defjam_separate_comparison.preceding_branch_target == 0x68e6c);
    assert((defjam_separate_comparison.conditional_branch_addresses
        == std::array<std::uint32_t, 4>{{0x68e74, 0x68e78, 0x68e84, 0x68e88}}));
    assert((defjam_separate_comparison.conditional_branch_targets
        == std::array<std::uint32_t, 4>{{0x68e80, 0x68e7e, 0x68e90, 0x68e8e}}));
    assert(defjam_separate_comparison.continuation_raw_disk_offset == 0x17290);
    assert(defjam_separate_comparison.continuation_prefix_sha256
        == "8a81ad1a39efe0442addd9302b3b0e5e0c0bd72ecaf5904d2fa5e1c2834cd964");
    const auto defjam_separate_byte_gate =
        eon::parse_millennium_amiga_resident_separate_byte_gate_boundary(
            defjam_loader_disk, defjam_plan, defjam_separate_comparison);
    assert(defjam_separate_byte_gate.entry_address == 0x68e90);
    assert(defjam_separate_byte_gate.raw_disk_offset == 0x17290);
    assert(defjam_separate_byte_gate.sha256
        == "f4a047914e83ab873a037ea16a4f5aaa9a402c38f48a525efc69d9e49cca15a8");
    assert(defjam_separate_byte_gate.compared_byte_address == 0x7c24e);
    assert(defjam_separate_byte_gate.conditional_branch_address == 0x68eae);
    assert(defjam_separate_byte_gate.conditional_branch_target == 0x68ed6);
    assert(defjam_separate_byte_gate.target_raw_disk_offset == 0x172d6);
    assert(defjam_separate_byte_gate.target_prefix_sha256
        == "79871297097662cd29a3659d5399a17c847a8c46d6753e1d968cb27b83c5210b");
    assert(defjam_separate_byte_gate.fallthrough_raw_disk_offset == 0x172b2);
    assert(defjam_separate_byte_gate.fallthrough_prefix_sha256
        == "cd83cab5400642c141e3252fd28302a94e7169d1f5bc7a6021cbe78c5daacd02");
    const auto defjam_separate_byte_gate_target =
        eon::parse_millennium_amiga_resident_separate_byte_gate_target_boundary(
            defjam_loader_disk, defjam_plan, defjam_separate_byte_gate);
    assert(defjam_separate_byte_gate_target.entry_address == 0x68ed6);
    assert(defjam_separate_byte_gate_target.raw_disk_offset == 0x172d6);
    assert(defjam_separate_byte_gate_target.sha256
        == "b2d2c6cadc50725eb8b4f0b680c325586ed457b29232481b503f3e337d589341");
    assert((defjam_separate_byte_gate_target.conditional_branch_addresses
        == std::array<std::uint32_t, 2>{{0x68ede, 0x68eea}}));
    assert((defjam_separate_byte_gate_target.conditional_branch_targets
        == std::array<std::uint32_t, 2>{{0x68ef4, 0x68ef4}}));
    assert(defjam_separate_byte_gate_target.convergence_address == 0x68ef4);
    assert(defjam_separate_byte_gate_target.convergence_raw_disk_offset == 0x172f4);
    assert(defjam_separate_byte_gate_target.convergence_prefix_sha256
        == "93b0d20954d235c624406450161a359968e4f1baefcbaeb47ede08fda0cd1e71");
    const auto defjam_separate_byte_gate_convergence =
        eon::parse_millennium_amiga_resident_separate_byte_gate_convergence_boundary(
            defjam_loader_disk, defjam_plan, defjam_separate_byte_gate_target);
    assert(defjam_separate_byte_gate_convergence.entry_address == 0x68ef4);
    assert(defjam_separate_byte_gate_convergence.raw_disk_offset == 0x172f4);
    assert(defjam_separate_byte_gate_convergence.sha256
        == "d63b2de78fbc18f2a4213206d1f05947a604dafc5b23fea56f87b624cb7549ab");
    assert(defjam_separate_byte_gate_convergence.conditional_branch_address == 0x68f02);
    assert(defjam_separate_byte_gate_convergence.conditional_branch_target == 0x68f2a);
    assert(defjam_separate_byte_gate_convergence.target_raw_disk_offset == 0x1732a);
    assert(defjam_separate_byte_gate_convergence.target_prefix_sha256
        == "ba2a0127999eb628ef05008867728fd31952c6d4b268bdb38f35130bab9973ae");
    assert(defjam_separate_byte_gate_convergence.fallthrough_raw_disk_offset == 0x17306);
    assert(defjam_separate_byte_gate_convergence.fallthrough_prefix_sha256
        == "5b3ae299a769dcca25b96b3b588ab65b1c44843abf0ef1288a1a74741dec9993");
    const auto defjam_separate_byte_gate_taken_branch =
        eon::parse_millennium_amiga_resident_separate_byte_gate_taken_branch_boundary(
            defjam_loader_disk, defjam_plan, defjam_separate_byte_gate_convergence);
    assert(defjam_separate_byte_gate_taken_branch.entry_address == 0x68f2a);
    assert(defjam_separate_byte_gate_taken_branch.raw_disk_offset == 0x1732a);
    assert(defjam_separate_byte_gate_taken_branch.sha256
        == "a7f4be625a6a39615f0ace12a1a8e013b781575625858b4f0c257d171b0947f3");
    assert((defjam_separate_byte_gate_taken_branch.conditional_branch_addresses
        == std::array<std::uint32_t, 2>{{0x68f32, 0x68f3e}}));
    assert((defjam_separate_byte_gate_taken_branch.conditional_branch_targets
        == std::array<std::uint32_t, 2>{{0x68f48, 0x68f48}}));
    assert(defjam_separate_byte_gate_taken_branch.convergence_address == 0x68f48);
    assert(defjam_separate_byte_gate_taken_branch.external_call_address == 0x68f48);
    assert(defjam_separate_byte_gate_taken_branch.external_call_target == 0x7caa6);
    assert(defjam_separate_byte_gate_taken_branch.external_prefix_raw_disk_offset == 0x17348);
    assert(defjam_separate_byte_gate_taken_branch.external_prefix_sha256
        == "dde319f5e57db52df300956d4e3e59dc6dc7967f0ff582674d502109fcfa2f69");
    const auto defjam_separate_byte_gate_fallthrough =
        eon::parse_millennium_amiga_resident_separate_byte_gate_fallthrough_boundary(
            defjam_loader_disk, defjam_plan, defjam_separate_byte_gate_convergence);
    assert(defjam_separate_byte_gate_fallthrough.entry_address == 0x68f06);
    assert(defjam_separate_byte_gate_fallthrough.raw_disk_offset == 0x17306);
    assert(defjam_separate_byte_gate_fallthrough.sha256
        == "4a50d1c5f71ada9a3571e09b00437c51037c3949ff8e57a4b153ea032828d061");
    assert(defjam_separate_byte_gate_fallthrough.conditional_branch_address == 0x68f1a);
    assert(defjam_separate_byte_gate_fallthrough.conditional_branch_target == 0x68f48);
    assert(defjam_separate_byte_gate_fallthrough.other_path_entry_address == 0x68f1e);
    assert(defjam_separate_byte_gate_fallthrough.other_path_sha256
        == "fc1fca692a8fc07b5fd7c502ae2d772eeff63c0c3d33d298f9c4fac414f337da");
    assert(defjam_separate_byte_gate_fallthrough.other_path_branch_address == 0x68f26);
    assert(defjam_separate_byte_gate_fallthrough.other_path_branch_target == 0x68f48);
    assert(defjam_separate_byte_gate_fallthrough.convergence_address == 0x68f48);
    const auto defjam_post_external_call =
        eon::parse_millennium_amiga_resident_separate_post_external_call_boundary(
            defjam_loader_disk, defjam_plan, defjam_separate_byte_gate_taken_branch);
    assert(defjam_post_external_call.entry_address == 0x68f4e);
    assert(defjam_post_external_call.raw_disk_offset == 0x1734e);
    assert(defjam_post_external_call.byte_count == 42);
    assert(defjam_post_external_call.sha256
        == "3220d65f197163401c649a36d756ecf3005d2f342b81de5a7d4528f9a45da851");
    assert((defjam_post_external_call.call_addresses
        == std::array<std::uint32_t, 3>{{0x68f4e, 0x68f5a, 0x68f6c}}));
    assert((defjam_post_external_call.call_targets
        == std::array<std::uint32_t, 3>{{0x7d6d2, 0x7780a, 0x77b34}}));
    assert((defjam_post_external_call.call_target_raw_disk_offsets
        == std::array<std::size_t, 3>{{0x2bad2, 0x25c0a, 0x25f34}}));
    assert((defjam_post_external_call.address_literals
        == std::array<std::uint32_t, 2>{{0x7c21b, 0x7c25c}}));
    assert(defjam_post_external_call.terminal_jump_address == 0x68f72);
    assert(defjam_post_external_call.terminal_jump_target == 0x7c54e);
    assert(defjam_post_external_call.terminal_jump_target_raw_disk_offset == 0x2a94e);
    assert(defjam_post_external_call.terminal_jump_target_prefix_sha256
        == "502069bdbda2f35899d16237fd1d2aa477be20f0c950231fb71f32583f23de14");
    const auto defjam_terminal_jump_raw_target =
        eon::parse_millennium_amiga_resident_separate_terminal_jump_raw_target_boundary(
            defjam_loader_disk, defjam_plan, defjam_post_external_call);
    assert(defjam_terminal_jump_raw_target.jump_address == 0x68f72);
    assert(defjam_terminal_jump_raw_target.target_address == 0x7c54e);
    assert(defjam_terminal_jump_raw_target.raw_disk_offset == 0x2a94e);
    assert(defjam_terminal_jump_raw_target.byte_count == 256);
    assert(defjam_terminal_jump_raw_target.sha256
        == "0149a457e657e18805ff61675e80741fa78d25f201f120498193315804b87eea");
    auto invalid_post_external_call_disk_bytes = *defjam_adf;
    invalid_post_external_call_disk_bytes[0x1734e] ^= 0x01;
    bool invalid_post_external_call_rejected = false;
    try {
        const eon::AmigaAdf invalid_post_external_call_disk(
            std::move(invalid_post_external_call_disk_bytes));
        static_cast<void>(eon::parse_millennium_amiga_resident_separate_post_external_call_boundary(
            invalid_post_external_call_disk, defjam_plan,
            defjam_separate_byte_gate_taken_branch));
    } catch (const std::runtime_error&) {
        invalid_post_external_call_rejected = true;
    }
    assert(invalid_post_external_call_rejected);
    auto invalid_terminal_jump_raw_target_disk_bytes = *defjam_adf;
    invalid_terminal_jump_raw_target_disk_bytes[0x2a94e + 255] ^= 0x01;
    bool invalid_terminal_jump_raw_target_rejected = false;
    try {
        const eon::AmigaAdf invalid_terminal_jump_raw_target_disk(
            std::move(invalid_terminal_jump_raw_target_disk_bytes));
        static_cast<void>(
            eon::parse_millennium_amiga_resident_separate_terminal_jump_raw_target_boundary(
                invalid_terminal_jump_raw_target_disk, defjam_plan, defjam_post_external_call));
    } catch (const std::runtime_error&) {
        invalid_terminal_jump_raw_target_rejected = true;
    }
    assert(invalid_terminal_jump_raw_target_rejected);
    // Every supplied Millennium Amiga image shares the verified resident raw
    // range. One image is shorter than a standard ADF, so check the common
    // raw bytes directly rather than incorrectly forcing it through the ADF
    // filesystem abstraction. This neither accepts nor interprets the other
    // variants' altered boot paths.
    for (const auto hash : millennium_amiga_hashes) {
        const auto image = eon::extract_asset_by_sha256(amiga_millennium->path, hash);
        assert(image);
        const std::span bytes(*image);
        assert(bytes.size() >= 0x2bfc8);
        assert(eon::to_hex(eon::sha256(bytes.subspan(0x1719c, 36)))
            == defjam_separate_post_call_tail.sha256);
        assert(eon::to_hex(eon::sha256(bytes.subspan(0x171c0, 14)))
            == defjam_separate_post_call_tail_branch.sha256);
        assert(eon::to_hex(eon::sha256(bytes.subspan(0x171ec, 32)))
            == defjam_separate_post_call_tail_branch.target_prefix_sha256);
        assert(eon::to_hex(eon::sha256(bytes.subspan(0x1726c, 36)))
            == defjam_separate_comparison.sha256);
        assert(eon::to_hex(eon::sha256(bytes.subspan(0x17290, 32)))
            == defjam_separate_comparison.continuation_prefix_sha256);
        assert(eon::to_hex(eon::sha256(bytes.subspan(0x17290, 34)))
            == defjam_separate_byte_gate.sha256);
        assert(eon::to_hex(eon::sha256(bytes.subspan(0x172d6, 30)))
            == defjam_separate_byte_gate_target.sha256);
        assert(eon::to_hex(eon::sha256(bytes.subspan(0x172f4, 32)))
            == defjam_separate_byte_gate_target.convergence_prefix_sha256);
        assert(eon::to_hex(eon::sha256(bytes.subspan(0x172f4, 34)))
            == defjam_separate_byte_gate_convergence.sha256);
        assert(eon::to_hex(eon::sha256(bytes.subspan(0x1732a, 32)))
            == defjam_separate_byte_gate_convergence.target_prefix_sha256);
        assert(eon::to_hex(eon::sha256(bytes.subspan(0x17306, 32)))
            == defjam_separate_byte_gate_convergence.fallthrough_prefix_sha256);
        assert(eon::to_hex(eon::sha256(bytes.subspan(0x1732a, 36)))
            == defjam_separate_byte_gate_taken_branch.sha256);
        assert(eon::to_hex(eon::sha256(bytes.subspan(0x17348, 18)))
            == defjam_separate_byte_gate_taken_branch.external_prefix_sha256);
        assert(eon::to_hex(eon::sha256(bytes.subspan(0x17306, 24)))
            == defjam_separate_byte_gate_fallthrough.sha256);
        assert(eon::to_hex(eon::sha256(bytes.subspan(0x1731e, 12)))
            == defjam_separate_byte_gate_fallthrough.other_path_sha256);
        assert(eon::to_hex(eon::sha256(bytes.subspan(0x1734e, 42)))
            == defjam_post_external_call.sha256);
        for (std::size_t index = 0;
             index < defjam_post_external_call.call_target_raw_disk_offsets.size(); ++index) {
            assert(eon::to_hex(eon::sha256(bytes.subspan(
                defjam_post_external_call.call_target_raw_disk_offsets[index], 32)))
                == defjam_post_external_call.call_target_prefix_sha256[index]);
        }
        assert(eon::to_hex(eon::sha256(bytes.subspan(
            defjam_post_external_call.terminal_jump_target_raw_disk_offset, 32)))
            == defjam_post_external_call.terminal_jump_target_prefix_sha256);
        assert(eon::to_hex(eon::sha256(bytes.subspan(
            defjam_terminal_jump_raw_target.raw_disk_offset,
            defjam_terminal_jump_raw_target.byte_count)))
            == defjam_terminal_jump_raw_target.sha256);
        for (std::size_t index = 0;
             index < defjam_separate_post_call_tail.target_raw_disk_offsets.size(); ++index) {
            assert(eon::to_hex(eon::sha256(bytes.subspan(
                defjam_separate_post_call_tail.target_raw_disk_offsets[index], 32)))
                == defjam_separate_post_call_tail.target_prefix_sha256[index]);
        }
    }
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
    assert((splitter_words == std::array<std::uint16_t, 3>{{0xb146, 0x5279, 0x0007}}));
    const auto splitter_pre_helper = eon::split_millennium_amiga_resident_words_pre_helper(
        splitter_words);
    assert((splitter_pre_helper.magnitude_words
        == std::array<std::uint16_t, 3>{{0x3146, 0x5279, 0x0007}}));
    assert((splitter_pre_helper.sign_bytes == std::array<std::uint8_t, 3>{{1, 0, 0}}));
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
    // A ReleaseArchive is not merely a scanner cache. Every later use binds
    // its platform/language metadata and the exact bytes reopened from disk
    // to one manifest identity, closing the scan-to-use provenance gap.
    eon::verify_release_archive(*english_dos);
    const auto verified_dos_assets = eon::inventory_verified_release(*english_dos);
    assert(!verified_dos_assets.empty());
    auto forged_release_metadata = *english_dos;
    forged_release_metadata.language = "es";
    bool rejected_forged_release_metadata = false;
    try {
        eon::verify_release_archive(forged_release_metadata);
    } catch (const std::runtime_error&) {
        rejected_forged_release_metadata = true;
    }
    assert(rejected_forged_release_metadata);
    auto forged_release_hash = *english_dos;
    forged_release_hash.sha256.assign(64, '0');
    bool rejected_forged_release_hash = false;
    try {
        static_cast<void>(eon::extract_verified_release_asset(forged_release_hash,
            "5f796a7fe8bcf5113a65087f76853061f8d96065f9a3cbe66b6c61303b677a88"));
    } catch (const std::runtime_error&) {
        rejected_forged_release_hash = true;
    }
    assert(rejected_forged_release_hash);
    eon::ReleaseScanner direct_archive_scanner(english_dos->path);
    assert(direct_archive_scanner.candidate_count() == 1);
    assert(direct_archive_scanner.advance());
    assert(direct_archive_scanner.releases().size() == 1);
    assert(direct_archive_scanner.releases().front().sha256 == english_dos->sha256);
    assert(direct_archive_scanner.report().candidates == 1);
    assert(direct_archive_scanner.report().size_candidates == 1);
    assert(direct_archive_scanner.report().hashed_candidates == 1);
    assert(direct_archive_scanner.report().verified_occurrences == 1);
    assert(direct_archive_scanner.report().duplicate_occurrences == 0);
    assert(direct_archive_scanner.report().unreadable_candidates == 0);
    const auto sfx1_bytes = eon::extract_verified_release_asset(*english_dos,
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
    const auto title_exit = eon::parse_millennium_dos_title_exit_closure(*titles_bytes);
    const auto millennium_title_transition = eon::parse_millennium_dos_title_transition(title_lib, title_flow);
    assert(title_flow.title_entry_address == 0x1b80);
    assert(title_flow.title_selection_callee_entry_address == 0x1725);
    assert(title_flow.title_selection_callee_branch_address == 0x172f);
    assert(title_flow.title_selection_callee_branch_target == 0x1732);
    assert(title_flow.title_selection_callee_fallthrough_return == 0x1731);
    assert(title_flow.title_selection_callee_jle_target_call_address == 0x173d);
    assert(title_flow.title_selection_callee_jle_target_call_target == 0x1390);
    assert(title_flow.title_selection_nested_callee_call_address == 0x13bb);
    assert(title_flow.title_selection_nested_callee_call_target == 0x13c);
    assert(title_flow.title_selection_nested_callee_terminal_address == 0x14b);
    assert(title_flow.title_resource_index == 0);
    assert(title_flow.intro_transition_steps == 37);
    assert(title_flow.intro_step_stride == 0x170);
    assert(millennium_title_transition.original_step_stride == 0x170);
    assert(millennium_title_transition.patches.size() == 37);
    assert(millennium_title_transition.patches.front().resource_name == "P01");
    assert(millennium_title_transition.patches.front().bitmap.width == 16);
    assert(millennium_title_transition.patches.front().bitmap.height == 23);
    assert(eon::to_hex(eon::sha256(millennium_title_transition.patches.front().bitmap.pixels))
        == "330db310a838487f4afea0011c1ba5f381e4ed7ad97d95e4745e7be2d2d8aaa1");
    assert((millennium_title_transition.patches[1].mode_two_logical_to_dac
        == std::vector<std::uint8_t>{0x00, 0xcc, 0x00}));
    assert(millennium_title_transition.patches.back().resource_name == "P25");
    assert(eon::to_hex(eon::sha256(millennium_title_transition.patches.back().bitmap.pixels)
        ) == "d7e44c796aed167010cdef9ab7ccef38b3b260854b51b2fba818972f30dd35dd");
    assert(title_flow.input_interrupt == 0x21);
    assert(title_flow.input_service == 0x06);
    assert(title_flow.input_parameter == 0xff);
    assert(title_flow.input_nonzero_exit_address == 0x1c54);
    assert(title_flow.input_exit_first_call_address == 0x1c54);
    assert(title_flow.input_exit_first_call_target == 0x1968);
    assert(title_flow.input_exit_loading_text_address == 0x1884);
    assert(title_flow.input_exit_loading_text == "    LOADING    2");
    assert(title_flow.input_exit_private_driver_entry_address == 0x1968);
    assert(title_exit.nonzero_entry_address == 0x1c54);
    assert(title_exit.nonzero_byte_count == 22);
    assert(title_exit.private_driver_target_address == 0x1968);
    assert(title_exit.post_driver_target_address == 0x12c0);
    assert(title_exit.status_storage_address == 0x1a0e);
    assert(title_exit.stack_restore_source_address == 0x1aa0);
    assert(title_exit.final_local_call_target_address == 0x0916);
    assert(title_exit.exit_stub_address == 0x1a0f);
    assert(title_exit.exit_stub_preceding_call_target_address == 0x112e);
    assert(title_exit.exit_interrupt == 0x21 && title_exit.exit_service == 0x4c);
    {
        auto altered_title_exit = *titles_bytes;
        altered_title_exit[0x1c64 - 0x100] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_dos_title_exit_closure(altered_title_exit));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    assert(title_flow.input_exit_private_driver_loop_address == 0x1931);
    assert(title_flow.input_exit_private_driver_wrapper_address == 0x0122);
    assert(title_flow.input_exit_private_driver_function == 0x13);
    assert(title_flow.input_exit_private_driver_call_count == 5);
    assert(title_flow.input_exit_private_driver_helper_address == 0x1917);
    assert(title_flow.input_exit_helper_selector_iterations == 15);
    assert(title_flow.input_exit_helper_selector_state_address == 0x1181);
    assert(title_flow.input_exit_helper_selector_accumulator_address == 0x18f7);
    assert(title_flow.input_exit_helper_selector_mask == 0x3ff);
    assert(title_flow.input_exit_helper_selector_range == 0x24);
    assert(title_flow.input_exit_helper_selector_subtract == 0x18);
    assert(title_flow.input_exit_helper_resource_index_bias == 1);
    assert(title_flow.input_exit_helper_resource_loader_address == 0x1712);
    assert(title_flow.input_exit_helper_patch_offset_builder_address == 0x1712);
    assert(title_flow.input_exit_helper_patch_offset_stride == 0x170);
    assert(title_flow.input_exit_helper_patch_offset_cell_address == 0x1341);
    assert(title_flow.input_exit_helper_position_table_address == 0x1768);
    assert(title_flow.input_exit_helper_position_count == 15);
    assert(title_flow.input_exit_helper_position_stride == 4);
    assert(title_flow.input_exit_helper_private_driver_function == 6);
    assert(title_flow.input_exit_helper_driver_record_address == 0x1349);
    assert(title_flow.input_exit_helper_driver_record_source_offset_cell_address == 0x1349);
    assert(title_flow.input_exit_helper_driver_record_segment_cell_address == 0x134b);
    assert(title_flow.input_exit_helper_driver_record_table_second_cell_address == 0x134f);
    assert(title_flow.input_exit_helper_driver_record_table_first_cell_address == 0x1351);
    assert(title_flow.input_exit_helper_decoded_height_cell_address == 0x1357);
    assert(title_flow.input_exit_helper_decoded_width_cell_address == 0x1359);
    assert(title_flow.title_buffer_setup_address == 0x135e);
    assert(title_flow.title_buffer_source_offset_cell_address == 0x1341);
    assert(title_flow.title_buffer_source_segment_cell_address == 0x1343);
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
    assert(title_flow.launcher_pre_title_callee_branch_address == 0x2d4);
    assert(title_flow.launcher_pre_title_callee_branch_target == 0x2e2);
    assert(title_flow.launcher_pre_title_callee_fallthrough_jump_address == 0x2e0);
    assert(title_flow.launcher_pre_title_callee_fallthrough_jump_target == 0x269);
    assert(title_flow.launcher_pre_title_callee_jnc_target_branch_address == 0x2ed);
    assert(title_flow.launcher_pre_title_callee_jnc_target_branch_target == 0x2d6);
    assert(title_flow.launcher_pre_title_callee_jc_target_jump_address == 0x2e0);
    assert(title_flow.launcher_pre_title_callee_jc_target_jump_target == 0x269);
    assert(title_flow.launcher_pre_title_callee_join_branch_address == 0x2b2);
    assert(title_flow.launcher_pre_title_callee_join_branch_target == 0x2c8);
    assert(title_flow.launcher_pre_title_callee_join_branch_terminal_address == 0x2ce);
    assert(title_flow.launcher_private_interrupt_loader_call_address == 0x204);
    assert(title_flow.launcher_private_interrupt_loader_call_target == 0x2cf);
    assert(title_flow.launcher_private_interrupt_install_address == 0x209);
    assert(title_flow.launcher_private_interrupt_number == 0x91);
    assert(title_flow.launcher_private_interrupt_handler_offset == 0);
    assert(title_flow.launcher_private_interrupt_saved_offset_cell == 0x5e7);
    assert(title_flow.launcher_private_interrupt_saved_segment_cell == 0x5e9);
    assert(title_flow.launcher_private_interrupt_restore_address == 0x269);
    assert(title_flow.launcher_private_interrupt_handler_loader_entry == 0x2cf);
    assert(title_flow.launcher_private_interrupt_handler_destination_offset == 0);
    assert(title_flow.launcher_private_interrupt_handler_open_service == 0x3d);
    assert(title_flow.launcher_private_interrupt_handler_seek_end_service == 0x42);
    assert(title_flow.launcher_private_interrupt_handler_allocate_service == 0x48);
    assert(title_flow.launcher_private_interrupt_handler_rewind_service == 0x42);
    assert(title_flow.launcher_private_interrupt_handler_read_service == 0x3f);
    assert(title_flow.launcher_private_interrupt_handler_close_service == 0x3e);
    assert(title_flow.launcher_video_selection_scan_address == 0x19d);
    assert(title_flow.launcher_video_selection_default_detector_address == 0x5a1);
    assert(title_flow.launcher_video_selection_map_address == 0x1de);
    assert(title_flow.launcher_private_interrupt_handler_first_selector == 1);
    assert(title_flow.launcher_private_interrupt_handler_first_program_address == 0x617);
    assert(title_flow.launcher_private_interrupt_handler_first_program == "ega640.bin");
    assert(title_flow.launcher_private_interrupt_handler_other_selector == 2);
    assert(title_flow.launcher_private_interrupt_handler_other_program_address == 0x5f9);
    assert(title_flow.launcher_private_interrupt_handler_other_program == "mcga.bin");
    const auto sound_selection = eon::parse_millennium_dos_sound_selection(*mill_bytes);
    assert(sound_selection.selector_entry_address == 0x511);
    assert(sound_selection.selector_byte_count == 100);
    assert(sound_selection.prompt_address == 0x407);
    assert(sound_selection.filename_table_address == 0x62a);
    assert(sound_selection.selection_table_address == 0x66e);
    assert(sound_selection.ibm_speaker_table_slot == 0);
    assert(sound_selection.sound_blaster_table_slot == 3);
    assert(sound_selection.covox_table_slot == 4);
    assert(sound_selection.ibm_speaker_filename == "sibm.drv");
    assert(sound_selection.sound_blaster_filename == "ssbl.drv");
    assert(sound_selection.covox_filename == "scvx.drv");
    assert(sound_selection.missing_srol_table_slot == 2);
    assert(sound_selection.missing_srol_filename == "srol.drv");
    const auto sound_blaster = eon::extract_asset_by_sha256(english_dos->path,
        "be5a00e0b71d893a3aeaaa1127b1e5b870fe734dc876e636c6a933b6444f1b72");
    const auto covox = eon::extract_asset_by_sha256(english_dos->path,
        "99e110b91534206a6b83680a3e11cceadd0e5ddf863560aed53dcbd2c49df7c4");
    assert(sound_blaster && covox);
    const auto sound_blaster_leaf = eon::admit_millennium_dos_sound_driver_leaf(*sound_blaster);
    const auto covox_leaf = eon::admit_millennium_dos_sound_driver_leaf(*covox);
    assert(sound_blaster_leaf.kind == eon::MillenniumDosSoundDriverKind::sound_blaster);
    assert(sound_blaster_leaf.original_filename == "ssbl.drv" && sound_blaster_leaf.byte_size == 9194);
    assert(covox_leaf.kind == eon::MillenniumDosSoundDriverKind::covox_sound_master);
    assert(covox_leaf.original_filename == "scvx.drv" && covox_leaf.byte_size == 4053);
    bool rejected_sound_leaf = false;
    try {
        static_cast<void>(eon::admit_millennium_dos_sound_driver_leaf(*mill_bytes));
    } catch (const std::runtime_error&) {
        rejected_sound_leaf = true;
    }
    assert(rejected_sound_leaf);
    const auto ega640 = eon::extract_asset_by_sha256(english_dos->path,
        "ba003dd155fee868980f6ece933c33f9b22af68ed376cd64f4e027abd65baf6a");
    const auto mcga = eon::extract_asset_by_sha256(english_dos->path,
        "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228");
    assert(ega640 && mcga);
    const auto ega_profile = eon::parse_millennium_dos_video_driver(*ega640,
        eon::MillenniumDosVideoDriverKind::ega640);
    const auto mcga_profile = eon::parse_millennium_dos_video_driver(*mcga,
        eon::MillenniumDosVideoDriverKind::mcga);
    assert(ega_profile.dispatch_table_address == 0x20);
    assert(ega_profile.function_zero_address == 0x1c8);
    assert(ega_profile.function_zero_input_offset == 0);
    assert(ega_profile.function_zero_cached_mode_address == 0x8c);
    assert(ega_profile.function_zero_cached_mode_unknown_sentinel == 0xff);
    assert(ega_profile.function_zero_cached_mode_query_interrupt_site == 0x1d7);
    assert(ega_profile.function_zero_cached_mode_unknown_branch_target == 0x1dc);
    assert(ega_profile.function_four_address == 0xc17);
    assert(ega_profile.function_zero_video_mode == 0x0e);
    assert(ega_profile.function_zero_set_mode_interrupt_site == 0x1de);
    assert(ega_profile.function_zero_verify_mode_interrupt_site == 0x1e3);
    assert(ega_profile.function_zero_mode_match_return == 0x1ec);
    assert(ega_profile.function_zero_mode_mismatch_return == 0x1ea);
    assert(ega_profile.function_four_input_offset == 0 && ega_profile.function_four_input_mask == 3);
    assert(ega_profile.function_four_state_address == 0x8d);
    assert(ega_profile.function_six_address == 0x8a6);
    assert(ega_profile.function_six_source_pointer_offset == 0);
    assert(ega_profile.function_six_source_pointer_load_address == 0x8d9);
    assert(ega_profile.function_six_source_word_zero_read_address == 0x8eb);
    assert(ega_profile.function_six_source_word_two_read_address == 0x8df);
    assert(ega_profile.function_six_source_word_four_read_address == 0x8dc);
    assert(ega_profile.function_six_source_nested_pointer_load_address == 0);
    assert(ega_profile.function_six_screen_width == 0x140);
    assert(ega_profile.function_six_horizontal_offset == 8);
    assert(ega_profile.function_six_height_offset == 0x10);
    assert(ega_profile.function_thirteen_address == 0xd37);
    assert(ega_profile.function_thirteen_status_port == 0x3da);
    assert(ega_profile.function_thirteen_retrace_mask == 0x08);
    assert(ega_profile.function_thirty_one_address == 0x235);
    assert(ega_profile.function_thirty_one_state_address == 0x8a);
    assert(ega_profile.function_thirty_one_return_ah == 0x04);
    assert(mcga_profile.dispatch_table_address == 0x32);
    assert(mcga_profile.function_zero_address == 0x1e6);
    assert(mcga_profile.function_zero_input_offset == 0);
    assert(mcga_profile.function_zero_cached_mode_address == 0xae);
    assert(mcga_profile.function_zero_cached_mode_unknown_sentinel == 0xff);
    assert(mcga_profile.function_zero_cached_mode_query_interrupt_site == 0x1f5);
    assert(mcga_profile.function_zero_cached_mode_unknown_branch_target == 0x1fa);
    assert(mcga_profile.function_four_address == 0x815);
    assert(mcga_profile.function_zero_video_mode == 0x13);
    assert(mcga_profile.function_zero_set_mode_interrupt_site == 0x1fc);
    assert(mcga_profile.function_zero_verify_mode_interrupt_site == 0x201);
    assert(mcga_profile.function_zero_mode_match_return == 0x20a);
    assert(mcga_profile.function_zero_mode_mismatch_return == 0x208);
    assert(mcga_profile.function_four_input_offset == 0 && mcga_profile.function_four_input_mask == 3);
    assert(mcga_profile.function_four_state_address == 0xaf);
    assert(mcga_profile.function_six_address == 0x705);
    assert(mcga_profile.function_six_source_pointer_offset == 0);
    assert(mcga_profile.function_six_source_pointer_load_address == 0x72e);
    assert(mcga_profile.function_six_source_word_zero_read_address == 0);
    assert(mcga_profile.function_six_source_word_two_read_address == 0x731);
    assert(mcga_profile.function_six_source_word_four_read_address == 0);
    assert(mcga_profile.function_six_source_nested_pointer_load_address == 0x736);
    assert(mcga_profile.function_six_screen_width == 0x140);
    assert(mcga_profile.function_thirteen_address == 0x905);
    assert(mcga_profile.function_thirteen_status_port == 0x3da);
    assert(mcga_profile.function_thirty_one_address == 0x24c);
    assert(mcga_profile.function_thirty_one_state_address == 0xac);
    assert(mcga_profile.function_thirty_one_return_ah == 0x01);
    assert(title_flow.launcher_title_offset == 0x58f);
    assert(title_flow.launcher_game_offset == 0x59a);
    assert(title_flow.launcher_title_program == "TITLES.EXE");
    assert(title_flow.launcher_game_program == "2200ad.exe");
    assert(title_flow.title_private_interrupt_wrapper_address == 0x0122);
    assert(title_flow.title_private_interrupt_record_address == 0x1ac4);
    assert(title_flow.title_private_interrupt_function == 0);
    assert(title_flow.title_private_interrupt_result_word_address == 0x1a9c);
    assert(title_flow.title_private_interrupt_equal_branch_target == 0x1ac6);
    assert(title_flow.title_private_interrupt_other_branch_target == 0x1ada);
    assert(title_flow.launcher_exec_helper_address == 0x031c);
    assert(title_flow.launcher_exec_param_block_address == 0x067a);
    assert(title_flow.launcher_exec_saved_stack_address == 0x05f7);
    assert(title_flow.launcher_exec_interrupt_site == 0x0337);
    assert(title_flow.launcher_exec_result_interrupt_site == 0x0348);
    assert(title_flow.launcher_exec_carry_branch_address == 0x0345);
    assert(title_flow.launcher_exec_noncarry_return_address == 0x034b);
    eon::MillenniumDosTitleSession title_session(title_flow);
    assert(!title_session.handed_off());
    assert(!title_session.poll_console(false));
    assert(!title_session.handed_off());
    assert(title_session.poll_console(true));
    assert(title_session.handed_off());
    assert(!title_session.poll_console(true));
    const auto game_executable = eon::extract_asset_by_sha256(english_dos->path,
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57");
    const auto gx_overlay = eon::extract_asset_by_sha256(english_dos->path,
        "093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb");
    assert(game_executable && game_executable->size() == 54'391);
    assert(gx_overlay && gx_overlay->size() == 46'634);
    const auto sound_effect_names = eon::parse_millennium_dos_sound_effect_name_table_evidence(
        *game_executable);
    assert(sound_effect_names.table_address == 0xcfdd && sound_effect_names.table_byte_count == 126);
    assert(sound_effect_names.table_sha256
        == "5bc252a34057b25239c81ce4ead178c294456e9af233bdd98d2d6f0f3cb4d008");
    assert(sound_effect_names.filenames.front() == "SFX1.VOC");
    assert(sound_effect_names.filenames.back() == "SFXE.VOC");
    auto altered_sound_effect_names = *game_executable;
    altered_sound_effect_names[0xcfdd - 0x100] = 'X';
    bool rejected_sound_effect_names = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_sound_effect_name_table_evidence(
            altered_sound_effect_names));
    } catch (const std::runtime_error&) {
        rejected_sound_effect_names = true;
    }
    assert(rejected_sound_effect_names);
    const auto gx_overlay_load = eon::parse_millennium_dos_gx_overlay_load_evidence(
        *game_executable, *gx_overlay);
    assert(gx_overlay_load.source_name_address == 0x11c2);
    assert(gx_overlay_load.loader_entry_address == 0x11ce);
    assert(gx_overlay_load.loader_segment_cell_address == 0x0118);
    assert(gx_overlay_load.first_call_address == 0x11d1);
    assert(gx_overlay_load.first_call_target == 0x053a);
    assert(gx_overlay_load.second_call_address == 0x11e4);
    assert(gx_overlay_load.second_call_target == 0x0574);
    assert(gx_overlay_load.third_call_address == 0x11ec);
    assert(gx_overlay_load.third_call_target == 0x0596);
    assert(gx_overlay_load.caller_call_address == 0xd335);
    assert(gx_overlay_load.caller_target == 0x11ce);
    const auto static_data_load = eon::parse_millennium_dos_static_data_load_evidence(*game_executable);
    assert(static_data_load.source_name_address == 0x100d);
    assert(static_data_load.loader_entry_address == 0x101a);
    assert(static_data_load.caller_call_address == 0xd332 && static_data_load.caller_target == 0x101a);
    assert(static_data_load.open_call_address == 0x101d);
    assert(static_data_load.read_call_address == 0x102e);
    assert(static_data_load.close_call_address == 0x1036);
    const auto gx_overlay_adapter = eon::parse_millennium_dos_gx_overlay_adapter_evidence(
        *game_executable, gx_overlay_load);
    assert(gx_overlay_adapter.entry_address == 0x6c52);
    assert(gx_overlay_adapter.overlay_entry_offset == 0);
    assert(gx_overlay_adapter.far_transfer_address == 0x6c68);
    assert(gx_overlay_adapter.continuation_address == 0x6c69);
    assert(gx_overlay_adapter.return_address == 0x6c72);
    const auto gx_overlay_dispatcher = eon::parse_millennium_dos_gx_overlay_dispatcher_evidence(
        *gx_overlay, gx_overlay_adapter);
    assert(gx_overlay_dispatcher.entry_offset == 0);
    assert(gx_overlay_dispatcher.far_return_offset == 0x14);
    assert(gx_overlay_dispatcher.table_offset == 0x15);
    assert(gx_overlay_dispatcher.observed_selector_targets[0x0e] == 0x0090);
    assert(gx_overlay_dispatcher.observed_selector_targets[0x0f] == 0x009f);
    assert(gx_overlay_dispatcher.observed_selector_targets[0x12] == 0x0097);
    assert(gx_overlay_dispatcher.observed_selector_targets[0x14] == 0x00a7);
    const auto gx_overlay_selector = eon::parse_millennium_dos_gx_overlay_selector_evidence(
        *game_executable, *gx_overlay, gx_overlay_adapter, gx_overlay_dispatcher);
    assert(gx_overlay_selector.caller_entry_address == 0xd343);
    assert(gx_overlay_selector.selector_source_address == 0xda05);
    assert((gx_overlay_selector.matching_selector_values
        == std::array<std::uint8_t, 3>{{0x03, 0x04, 0x02}}));
    assert((gx_overlay_selector.overlay_targets
        == std::array<std::uint16_t, 4>{{0x000e, 0x0012, 0x0014, 0x000f}}));
    assert(gx_overlay_selector.dx_storage_address == 0x4b6e);
    assert(gx_overlay_selector.adapter_call_address == 0xd373);
    assert(gx_overlay_selector.adapter_target == 0x6c52);
    const auto gx_overlay_startup_records =
        eon::parse_millennium_dos_gx_overlay_startup_record_evidence(
            *gx_overlay, gx_overlay_selector);
    assert(gx_overlay_startup_records.first_entry_offset == 0x90);
    assert(gx_overlay_startup_records.entry_span_byte_count == 94);
    assert((gx_overlay_startup_records.entry_offsets
        == std::array<std::uint16_t, 4>{{0x90, 0x97, 0x9f, 0xa7}}));
    assert((gx_overlay_startup_records.source_record_offsets
        == std::array<std::uint16_t, 4>{{0x70, 0x80, 0x78, 0x88}}));
    assert((gx_overlay_startup_records.source_records[0]
        == std::array<std::uint8_t, 8>{{0xc7,0x0d,0x24,0x00,0xa0,0x05,0xa2,0x05}}));
    assert((gx_overlay_startup_records.source_records[2]
        == std::array<std::uint8_t, 8>{{0x1f,0x37,0x20,0x01,0xa0,0x05,0x10,0x2d}}));
    assert(gx_overlay_startup_records.shared_copy_entry_offset == 0xb2);
    assert(gx_overlay_startup_records.copy_destination_offset == 0x65);
    assert(gx_overlay_startup_records.copy_word_count == 4);
    assert(gx_overlay_startup_records.copied_last_byte_storage_offset == 0x6d);
    assert((gx_overlay_startup_records.state_word_storage_offsets
        == std::array<std::uint16_t, 3>{{0xf4, 0xf0, 0xf2}}));
    assert(gx_overlay_startup_records.terminal_word_storage_offset == 0x5c);
    assert(gx_overlay_startup_records.terminal_word_value == 0x47ea);
    const auto gx_overlay_dispatch13 = eon::parse_millennium_dos_gx_overlay_dispatch13_evidence(
        *gx_overlay, gx_overlay_dispatcher);
    assert(gx_overlay_dispatch13.entry_offset == 0x08d0);
    assert(gx_overlay_dispatch13.byte_count == 148);
    assert((gx_overlay_dispatch13.call_offsets
        == std::array<std::uint16_t, 7>{{0x08d4,0x08d7,0x08fd,0x093d,0x0940,0x094f,0x0952}}));
    assert((gx_overlay_dispatch13.call_targets
        == std::array<std::uint16_t, 7>{{0x0802,0x0453,0x2454,0x0454,0x099b,0x06fc,0x0796}}));
    assert((gx_overlay_dispatch13.zeroed_word_storage_offsets
        == std::array<std::uint16_t, 3>{{0x00f0,0x00f2,0x00f4}}));
    assert(gx_overlay_dispatch13.first_result_compare_value == 0x20);
    assert(gx_overlay_dispatch13.local_back_edge_target_offset == 0x08fc);
    {
        auto altered_gx_overlay = *gx_overlay;
        altered_gx_overlay[0xb2] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_dos_gx_overlay_startup_record_evidence(
                altered_gx_overlay, gx_overlay_selector));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_gx_overlay = *gx_overlay;
        altered_gx_overlay[0x0900] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_dos_gx_overlay_dispatch13_evidence(
                altered_gx_overlay, gx_overlay_dispatcher));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    const auto game_flow = eon::parse_millennium_dos_game_flow(*game_executable);
    assert(game_flow.entry_address == 0xd2b0);
    assert(game_flow.startup_address == 0xd2b4);
    assert(game_flow.startup_stack_pointer == 0xda00);
    assert(game_flow.startup_first_call_address == 0x0124);
    assert(game_flow.startup_first_call_interrupt == 0x91);
    assert(game_flow.startup_first_call_return_address == 0x0130);
    assert(game_flow.startup_first_call_return_site == 0xd2c8);
    assert(game_flow.startup_result_word_address == 0xd128);
    assert(game_flow.startup_result_high_byte_first_address == 0x4368);
    assert(game_flow.startup_result_high_byte_second_address == 0xda05);
    assert(game_flow.startup_stack_snapshot_address == 0xd12c);
    assert(game_flow.startup_mode_compare_address == 0xd2d9);
    assert(game_flow.startup_mode_byte_address == 0xda05);
    assert(game_flow.startup_mode_equal_value == 1);
    assert(game_flow.startup_equal_call_address == 0xd1a1);
    assert(game_flow.startup_other_call_address == 0xd1b5);
    assert(game_flow.startup_equal_path_private_call_site == 0xd1a9);
    assert(game_flow.startup_equal_path_next_call_address == 0x044e);
    assert(game_flow.startup_equal_followup_write_address == 0xda05);
    assert(game_flow.startup_equal_followup_write_value == 1);
    assert(game_flow.startup_other_path_private_call_site == 0xd1bd);
    assert(game_flow.startup_other_path_next_call_address == 0x0466);
    assert(game_flow.startup_other_followup_table_address == 0x0456);
    assert(game_flow.startup_other_followup_table_size == 16);
    assert(game_flow.startup_other_followup_table_values[0] == 0x00);
    assert(game_flow.startup_other_followup_table_values[7] == 0x07);
    assert(game_flow.startup_other_followup_table_values[8] == 0x38);
    assert(game_flow.startup_other_followup_table_values[15] == 0x3f);
    assert(game_flow.startup_other_followup_interrupt_site == 0x0476);
    assert(game_flow.startup_other_followup_interrupt_number == 0x10);
    assert(game_flow.startup_other_followup_video_function == 0x10);
    assert(game_flow.startup_other_followup_video_subfunction == 0x00);
    const auto ega_writes = eon::millennium_dos_startup_ega_palette_register_writes(game_flow);
    assert(ega_writes.size() == 16);
    assert((ega_writes[0] == eon::MillenniumDosEgaPaletteRegisterWrite{0, 0x00}));
    assert((ega_writes[8] == eon::MillenniumDosEgaPaletteRegisterWrite{8, 0x38}));
    assert((ega_writes[15] == eon::MillenniumDosEgaPaletteRegisterWrite{15, 0x3f}));
    assert(game_flow.startup_nonzero_dx_branch_address == 0xd44b);
    const auto startup_allocation = eon::parse_millennium_dos_startup_allocation_boundary(
        *game_executable);
    assert(startup_allocation.executable_sha256
        == "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57");
    assert(startup_allocation.continuation_entry_address == 0xd2e5);
    assert(startup_allocation.allocator_call_address == 0xd2e8);
    assert(startup_allocation.allocator_entry_address == 0xd1fa);
    assert(startup_allocation.allocator_first_external_interrupt_site == 0xd201);
    assert(startup_allocation.allocator_first_external_interrupt == 0x21);
    assert(startup_allocation.allocator_first_external_service == 0x4a);
    assert(startup_allocation.post_allocator_result_storage_address == 0xd128);
    assert(startup_allocation.dx_test_address == 0xd2ee);
    assert(startup_allocation.dx_zero_branch_address == 0xd2f0);
    assert(startup_allocation.dx_zero_branch_target == 0xd2f5);
    assert(startup_allocation.dx_nonzero_jump_address == 0xd2f2);
    assert(startup_allocation.dx_nonzero_jump_target == 0xd44b);
    assert(startup_allocation.dx_zero_path_first_call_address == 0xd2f5);
    assert(startup_allocation.dx_zero_path_first_call_target == 0x1161);
    const auto startup_zero_path = eon::parse_millennium_dos_startup_zero_path_boundary(
        *game_executable);
    assert(startup_zero_path.executable_sha256
        == "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57");
    assert(startup_zero_path.zero_path_entry_address == 0xd2f5);
    assert(startup_zero_path.selector_entry_address == 0x1161);
    assert(startup_zero_path.selector_mode_byte_address == 0xda05);
    assert((startup_zero_path.selector_matching_values
        == std::array<std::uint8_t, 3>{{0x01, 0x03, 0x02}}));
    assert((startup_zero_path.selector_name_addresses
        == std::array<std::uint16_t, 4>{{0x1131, 0x113d, 0x1155, 0x1149}}));
    assert(startup_zero_path.selector_call_address == 0x117c);
    assert(startup_zero_path.selector_call_target == 0x053a);
    assert(startup_zero_path.security_name_address == 0x2f6a);
    assert(startup_zero_path.first_external_interrupt_site == 0x0550);
    assert(startup_zero_path.first_external_interrupt == 0x21);
    assert(startup_zero_path.first_external_service == 0x3d);
    assert(startup_zero_path.first_external_access_mode == 0x02);
    const auto startup_zero_continuation =
        eon::parse_millennium_dos_startup_zero_continuation_boundary(*game_executable);
    assert(startup_zero_continuation.executable_sha256
        == "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57");
    assert(startup_zero_continuation.continuation_entry_address == 0xd2f8);
    assert(startup_zero_continuation.continuation_byte_count == 23);
    assert(startup_zero_continuation.continuation_sha256
        == "9c7b13c4e0b99e8529e78063b91ae92d967b9fc6de66ebeeaacec01563e4a9d9");
    assert(startup_zero_continuation.source_byte_address == 0x0082);
    assert(startup_zero_continuation.source_byte_subtract_immediate == 0x30);
    assert(startup_zero_continuation.decoded_byte_storage_address == 0x0122);
    assert(startup_zero_continuation.first_local_call_address == 0xd305);
    assert(startup_zero_continuation.first_local_call_target == 0xd07a);
    assert(startup_zero_continuation.first_external_interrupt_site == 0xd30d);
    assert(startup_zero_continuation.first_external_interrupt == 0x21);
    assert(startup_zero_continuation.first_external_service == 0x48);
    assert(startup_zero_continuation.allocation_request_paragraphs == 0xfa00);
    const auto startup_post_allocation =
        eon::parse_millennium_dos_startup_post_allocation_boundary(*game_executable);
    assert(startup_post_allocation.executable_sha256
        == "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57");
    assert(startup_post_allocation.entry_address == 0xd30f);
    assert(startup_post_allocation.byte_count == 10);
    assert(startup_post_allocation.cs_override_store_address == 0xd30f);
    assert(startup_post_allocation.cs_override_store_target_address == 0xd130);
    assert(startup_post_allocation.es_from_ax_address == 0xd314);
    assert(startup_post_allocation.first_external_interrupt_site == 0xd318);
    assert(startup_post_allocation.first_external_interrupt == 0x21);
    assert(startup_post_allocation.first_external_service == 0x49);
    assert(startup_post_allocation.boundary_sha256
        == "f583faad7bddba301c431adb94fa9d53d5b197dcba2f447b0b654df6f1b452ce");
    const auto startup_post_release =
        eon::parse_millennium_dos_startup_post_release_continuation(*game_executable);
    assert(startup_post_release.executable_sha256
        == "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57");
    assert(startup_post_release.entry_address == 0xd31a);
    assert(startup_post_release.byte_count == 30);
    assert(startup_post_release.restore_dx_address == 0xd31c);
    assert(startup_post_release.first_far_pointer_load_address == 0xd31d);
    assert(startup_post_release.far_pointer_address == 0x1042);
    assert(startup_post_release.second_far_pointer_load_address == 0xd326);
    assert(startup_post_release.first_call_address == 0xd32f);
    assert(startup_post_release.first_call_target == 0x6bf2);
    assert(startup_post_release.static_data_call_address == 0xd332);
    assert(startup_post_release.static_data_call_target == 0x101a);
    assert(startup_post_release.gx_loader_call_address == 0xd335);
    assert(startup_post_release.gx_loader_call_target == 0x11ce);
    assert(startup_post_release.continuation_sha256
        == "4d94bf904471cf96a03ce6dd111c0720f396e08ebf2f4603469377db0dc669ef");
    const auto startup_post_gx_loader =
        eon::parse_millennium_dos_startup_post_gx_loader_boundary(*game_executable);
    assert(startup_post_gx_loader.executable_sha256
        == "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57");
    assert(startup_post_gx_loader.entry_address == 0xd338);
    assert(startup_post_gx_loader.byte_count == 11);
    assert(startup_post_gx_loader.push_cs_address == 0xd338);
    assert(startup_post_gx_loader.pop_es_address == 0xd339);
    assert(startup_post_gx_loader.bx_literal == 0xd1a0);
    assert(startup_post_gx_loader.ax_literal == 0x0022);
    assert(startup_post_gx_loader.private_call_address == 0xd340);
    assert(startup_post_gx_loader.private_call_target == 0x0124);
    assert(startup_post_gx_loader.private_interrupt == 0x91);
    assert(startup_post_gx_loader.boundary_sha256
        == "64e7dddae2ca6942cddaa4c564d61203b26c469fc898bb923b2ba227d93876ab");
    const auto private_int91 = eon::parse_millennium_dos_private_int91_wrapper(*game_executable);
    assert(private_int91.executable_sha256
        == "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57");
    assert(private_int91.entry_address == 0x0124);
    assert(private_int91.byte_count == 13);
    assert(private_int91.caller_call_address == 0xd340);
    assert(private_int91.caller_call_target == 0x0124);
    assert(private_int91.push_ds_address == 0x0124);
    assert(private_int91.push_si_address == 0x0125);
    assert(private_int91.push_di_address == 0x0126);
    assert(private_int91.push_bp_address == 0x0127);
    assert(private_int91.push_es_address == 0x0128);
    assert(private_int91.private_interrupt_site == 0x0129);
    assert(private_int91.private_interrupt == 0x91);
    assert(private_int91.pop_es_address == 0x012b);
    assert(private_int91.pop_bp_address == 0x012c);
    assert(private_int91.pop_di_address == 0x012d);
    assert(private_int91.pop_si_address == 0x012e);
    assert(private_int91.pop_ds_address == 0x012f);
    assert(private_int91.return_address == 0x0130);
    assert(private_int91.wrapper_sha256
        == "5d17daad68e9062dc6852ae76740db4afdcb81555ba9fb7d15d4e4aa8d088175");
    const auto post_int91_selector =
        eon::parse_millennium_dos_post_int91_caller_selector(*game_executable);
    assert(post_int91_selector.executable_sha256
        == "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57");
    assert(post_int91_selector.return_site_address == 0xd343);
    assert(post_int91_selector.byte_count == 51);
    assert(post_int91_selector.source_byte_address == 0xda05);
    assert(post_int91_selector.first_compare_address == 0xd34d);
    assert(post_int91_selector.first_compare_value == 0x03);
    assert(post_int91_selector.second_compare_address == 0xd358);
    assert(post_int91_selector.second_compare_value == 0x04);
    assert(post_int91_selector.third_compare_address == 0xd363);
    assert(post_int91_selector.third_compare_value == 0x02);
    assert(post_int91_selector.shared_store_address == 0xd36e);
    assert(post_int91_selector.shared_store_target_address == 0x4b6e);
    assert(post_int91_selector.first_call_address == 0xd373);
    assert(post_int91_selector.first_call_target == 0x6c52);
    assert(post_int91_selector.selector_sha256
        == "571626e83b0787401f89c8586c12dfb4d4221c44e0a9786727d2314b09327091");
    const auto post_overlay_adapter =
        eon::parse_millennium_dos_post_overlay_adapter_continuation(*game_executable);
    assert(post_overlay_adapter.executable_sha256
        == "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57");
    assert(post_overlay_adapter.return_site_address == 0xd376);
    assert(post_overlay_adapter.byte_count == 39);
    assert((post_overlay_adapter.initial_call_addresses
        == std::array<std::uint16_t, 6>{{0xd376, 0xd379, 0xd37c, 0xd37f, 0xd382, 0xd385}}));
    assert((post_overlay_adapter.initial_call_targets
        == std::array<std::uint16_t, 6>{{0xd152, 0x4f08, 0x4111, 0x40af, 0x42b2, 0x107a}}));
    assert(post_overlay_adapter.mode_compare_address == 0xd388);
    assert(post_overlay_adapter.mode_byte_address == 0xda05);
    assert(post_overlay_adapter.mode_equal_value == 0x01);
    assert(post_overlay_adapter.equal_branch_address == 0xd38d);
    assert(post_overlay_adapter.equal_branch_target == 0xd394);
    assert(post_overlay_adapter.other_call_address == 0xd38f);
    assert(post_overlay_adapter.other_call_target == 0xd1b5);
    assert(post_overlay_adapter.other_jump_address == 0xd392);
    assert(post_overlay_adapter.convergence_address == 0xd397);
    assert(post_overlay_adapter.equal_call_address == 0xd394);
    assert(post_overlay_adapter.equal_call_target == 0xd1a1);
    assert(post_overlay_adapter.first_push_cs_address == 0xd397);
    assert(post_overlay_adapter.first_pop_ds_address == 0xd398);
    assert(post_overlay_adapter.first_pop_es_address == 0xd39a);
    assert(post_overlay_adapter.second_pop_es_address == 0xd39c);
    assert(post_overlay_adapter.continuation_sha256
        == "1df4b30f14434eae3a44463402710bcd1b162200a923c0b9cc1f827faf3763ac");
    const auto post_overlay_loop =
        eon::parse_millennium_dos_post_overlay_adapter_loop(*game_executable);
    assert(post_overlay_loop.executable_sha256
        == "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57");
    assert(post_overlay_loop.entry_address == 0xd39d);
    assert(post_overlay_loop.byte_count == 69);
    assert((post_overlay_loop.call_addresses == std::array<std::uint16_t, 15>{
        0xd39d, 0xd3a3, 0xd3a6, 0xd3ab, 0xd3ae, 0xd3b1, 0xd3b7, 0xd3c6,
        0xd3c9, 0xd3cc, 0xd3cf, 0xd3d2, 0xd3d5, 0xd3d8, 0xd3db}));
    assert((post_overlay_loop.call_targets == std::array<std::uint16_t, 15>{
        0x446a, 0x5b1f, 0x6178, 0x799c, 0x52f9, 0x7b7f, 0x09e4, 0x11a4,
        0x0b0c, 0x0ea4, 0x0b5b, 0x0ebb, 0x7601, 0x7bcb, 0x0f05}));
    assert(post_overlay_loop.first_al_test_address == 0xd3ba);
    assert(post_overlay_loop.first_nonzero_branch_address == 0xd3bc);
    assert(post_overlay_loop.first_nonzero_branch_target == 0xd3c6);
    assert(post_overlay_loop.native_byte_load_address == 0xd3be);
    assert(post_overlay_loop.native_byte_address == 0x07f9);
    assert(post_overlay_loop.native_byte_xor_address == 0xd3c1);
    assert(post_overlay_loop.native_byte_xor_literal == 0x01);
    assert(post_overlay_loop.native_byte_store_address == 0xd3c3);
    assert(post_overlay_loop.loop_al_test_address == 0xd3de);
    assert(post_overlay_loop.loop_zero_branch_address == 0xd3e0);
    assert(post_overlay_loop.loop_zero_branch_target == 0xd3d2);
    assert(post_overlay_loop.following_dispatch_address == 0xd3e2);
    assert(post_overlay_loop.loop_sha256
        == "1bbb4fcc18668021306de1e0014a9baab1f526af1514fa7ce9d1a61780972cf0");
    const auto post_overlay_dispatch =
        eon::parse_millennium_dos_post_overlay_dispatch_prefix(*game_executable);
    assert(post_overlay_dispatch.executable_sha256
        == "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57");
    assert(post_overlay_dispatch.entry_address == 0xd3e2);
    assert(post_overlay_dispatch.byte_count == 49);
    assert(post_overlay_dispatch.first_action_compare_address == 0xd3e4);
    assert(post_overlay_dispatch.first_action_value == 0x0b);
    assert(post_overlay_dispatch.first_action_branch_target == 0xd40e);
    assert(post_overlay_dispatch.guard_byte_address == 0xda3a);
    assert(post_overlay_dispatch.guard_nonzero_branch_target == 0xd3d2);
    assert(post_overlay_dispatch.second_action_value == 0x0c);
    assert(post_overlay_dispatch.second_action_call_target == 0xd570);
    assert(post_overlay_dispatch.action_base_value == 0x3b);
    assert(post_overlay_dispatch.action_limit_value == 0x0a);
    assert(post_overlay_dispatch.table_base_address == 0x2fbf);
    assert(post_overlay_dispatch.scaled_call_target == 0x76f1);
    assert(post_overlay_dispatch.function_key_loop_jump_target == 0xd3d2);
    assert(post_overlay_dispatch.first_action_call_target == 0x11a4);
    assert(post_overlay_dispatch.first_action_loop_jump_target == 0xd3d2);
    assert(post_overlay_dispatch.prefix_sha256
        == "7abec93ec23f7ca3c4b400e16b9e746da7b0b9a1dd4bec88ba891ef04b322065");
    const auto startup_nonzero_path = eon::parse_millennium_dos_startup_nonzero_path_boundary(
        *game_executable);
    assert(startup_nonzero_path.executable_sha256
        == "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57");
    assert(startup_nonzero_path.nonzero_entry_address == 0xd44b);
    assert(startup_nonzero_path.immediate_al_value == 0x08);
    assert(startup_nonzero_path.short_jump_address == 0xd44d);
    assert(startup_nonzero_path.continuation_entry_address == 0xd41b);
    assert(startup_nonzero_path.continuation_byte_storage_address == 0x2fb2);
    assert(startup_nonzero_path.continuation_stack_source_address == 0xd12c);
    assert(startup_nonzero_path.continuation_first_call_address == 0xd423);
    assert(startup_nonzero_path.continuation_first_call_target == 0x09e4);
    assert(startup_nonzero_path.first_external_interrupt_site == 0x09e7);
    assert(startup_nonzero_path.first_external_interrupt == 0x33);
    assert(startup_nonzero_path.first_external_service == 0x00);
    const auto english_startup_callees = eon::parse_millennium_dos_english_game_startup_callees(
        *game_executable);
    assert(english_startup_callees.executable_sha256
        == "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57");
    assert(english_startup_callees.equal_entry_address == 0xd1a1);
    assert(english_startup_callees.equal_byte_count == 20);
    assert(english_startup_callees.equal_sha256
        == "6f59df77c567324b41dd6159a6fbac7d8970626fc40e8b908f9f58746a993a3e");
    assert(english_startup_callees.equal_private_function == 4);
    assert(english_startup_callees.equal_private_record_address == 0xd19f);
    assert(english_startup_callees.equal_private_call_address == 0xd1a9);
    assert(english_startup_callees.equal_private_target_address == 0x0124);
    assert(english_startup_callees.equal_followup_call_address == 0xd1ac);
    assert(english_startup_callees.equal_followup_target_address == 0x044e);
    assert(english_startup_callees.equal_result_value == 1);
    assert(english_startup_callees.equal_result_storage_address == 0xda05);
    assert(english_startup_callees.equal_return_address == 0xd1b4);
    assert(english_startup_callees.other_entry_address == 0xd1b5);
    assert(english_startup_callees.other_byte_count == 28);
    assert(english_startup_callees.other_sha256
        == "2f61098eb45bb48ea7a38ab2fcc2e065ae0d0b2ad08ea9973e3fe464943fba9b");
    assert(english_startup_callees.other_private_function == 4);
    assert(english_startup_callees.other_private_record_address == 0xd19f);
    assert(english_startup_callees.other_private_call_address == 0xd1bd);
    assert(english_startup_callees.other_private_target_address == 0x0124);
    assert(english_startup_callees.other_followup_call_address == 0xd1c0);
    assert(english_startup_callees.other_followup_target_address == 0x0466);
    assert(english_startup_callees.other_result_source_address == 0xda05);
    assert(english_startup_callees.other_compare_value == 2);
    assert(english_startup_callees.other_equal_store_address == 0x0107);
    assert(english_startup_callees.other_return_address == 0xd1d0);
    const auto english_startup_followups = eon::parse_millennium_dos_english_game_startup_followups(
        *game_executable, english_startup_callees);
    assert(english_startup_followups.equal_entry_address == 0x044e);
    assert(english_startup_followups.equal_byte_count == 8);
    assert(english_startup_followups.equal_sha256
        == "38889279a8b89e0e600bb25298015ccd8aadc09ea3858a1790097b3f7ff4ea8f");
    assert(english_startup_followups.equal_literal_value == 1);
    assert(english_startup_followups.equal_storage_address == 0xda05);
    assert(english_startup_followups.equal_return_address == 0x0455);
    assert(english_startup_followups.palette_entry_address == 0x0466);
    assert(english_startup_followups.palette_byte_count == 23);
    assert(english_startup_followups.palette_sha256
        == "b17db26fa4fa8b7307fb767ff98351bd6dcca202829dd2d9348ff4991942d779");
    assert(english_startup_followups.palette_table_address == 0x0456);
    assert((english_startup_followups.palette_table_values
        == std::array<std::uint8_t, 16>{0, 1, 2, 3, 4, 5, 6, 7, 0x38, 0x39, 0x3a,
            0x3b, 0x3c, 0x3d, 0x3e, 0x3f}));
    assert(english_startup_followups.palette_table_sha256
        == "ce46bce999708ea5109a857b0b6ecc02ece34eaf431cd148ef1aa1c0e80aed0a");
    assert(english_startup_followups.palette_initial_cx == 16);
    assert(english_startup_followups.bios_interrupt == 0x10);
    assert(english_startup_followups.bios_ax == 0x1000);
    assert(english_startup_followups.palette_return_address == 0x047c);
    {
        auto altered_startup_allocation = *game_executable;
        altered_startup_allocation[0xd2e8 - 0x100] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_dos_startup_allocation_boundary(
                altered_startup_allocation));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_startup_zero_path = *game_executable;
        altered_startup_zero_path[0x117c - 0x100] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_dos_startup_zero_path_boundary(
                altered_startup_zero_path));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_startup_zero_continuation = *game_executable;
        altered_startup_zero_continuation[0xd305 - 0x100] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_dos_startup_zero_continuation_boundary(
                altered_startup_zero_continuation));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_startup_post_allocation = *game_executable;
        altered_startup_post_allocation[0xd314 - 0x100] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_dos_startup_post_allocation_boundary(
                altered_startup_post_allocation));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_startup_post_release = *game_executable;
        altered_startup_post_release[0xd32f - 0x100] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_dos_startup_post_release_continuation(
                altered_startup_post_release));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_startup_post_gx_loader = *game_executable;
        altered_startup_post_gx_loader[0xd340 - 0x100] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_dos_startup_post_gx_loader_boundary(
                altered_startup_post_gx_loader));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_private_int91 = *game_executable;
        altered_private_int91[0x0129 - 0x100] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_dos_private_int91_wrapper(
                altered_private_int91));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_post_int91_selector = *game_executable;
        altered_post_int91_selector[0xd36e - 0x100] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_dos_post_int91_caller_selector(
                altered_post_int91_selector));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_post_overlay_adapter = *game_executable;
        altered_post_overlay_adapter[0xd388 - 0x100] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_dos_post_overlay_adapter_continuation(
                altered_post_overlay_adapter));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_post_overlay_loop = *game_executable;
        altered_post_overlay_loop[0xd3de - 0x100] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_dos_post_overlay_adapter_loop(
                altered_post_overlay_loop));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_startup_nonzero_path = *game_executable;
        altered_startup_nonzero_path[0xd44d - 0x100] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_dos_startup_nonzero_path_boundary(
                altered_startup_nonzero_path));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_english_startup = *game_executable;
        altered_english_startup[0xd1a9 - 0x100] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_dos_english_game_startup_callees(
                altered_english_startup));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_english_startup = *game_executable;
        altered_english_startup[0x0476 - 0x100] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_dos_english_game_startup_followups(
                altered_english_startup, english_startup_callees));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_allocator_prefix = *game_executable;
        altered_allocator_prefix[0xd201 - 0x100] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_dos_startup_allocation_boundary(
                altered_allocator_prefix));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    assert(game_flow.main_loop_address == 0xd3d2);
    assert(game_flow.action_poll_address == 0x0f05);
    assert(game_flow.special_action_0 == 0x0b && game_flow.special_action_1 == 0x0c);
    // Action $0b is English-DOS-only dispatch evidence. Its handler changes
    // one explicitly observed native byte before stopping at helper $0666.
    const auto first_special_action_zero =
        eon::evaluate_millennium_dos_first_special_action_prefix(*game_executable, 0);
    assert(first_special_action_zero.action == 0x0b);
    assert(first_special_action_zero.dispatch_branch_address == 0xd3e6);
    assert(first_special_action_zero.dispatch_call_address == 0xd40e);
    assert(first_special_action_zero.handler_address == 0x11a4);
    assert(first_special_action_zero.runtime_byte_address == 0x07f9);
    assert(first_special_action_zero.toggled_runtime_byte == 1);
    assert(first_special_action_zero.selected_ax_value == 0x018f);
    assert(first_special_action_zero.helper_call_address == 0x11b7);
    assert(first_special_action_zero.helper_address == 0x0666);
    const auto first_special_action_nonzero =
        eon::evaluate_millennium_dos_first_special_action_prefix(*game_executable, 0x5a);
    assert(first_special_action_nonzero.toggled_runtime_byte == 0x5b);
    assert(first_special_action_nonzero.selected_ax_value == 0x018e);
    const auto shared_helper_from_zero_action = eon::evaluate_millennium_dos_shared_helper_prefix(
        *game_executable, first_special_action_zero.selected_ax_value);
    assert(shared_helper_from_zero_action.entry_address == 0x0666);
    assert(shared_helper_from_zero_action.caller_ax == 0x018f);
    assert(shared_helper_from_zero_action.source_segment_cell_address == 0x0116);
    assert(shared_helper_from_zero_action.scratch_byte_address == 0x05c8);
    assert(shared_helper_from_zero_action.scratch_byte_value == 0);
    assert(shared_helper_from_zero_action.shifted_ax == 0x031e);
    assert(shared_helper_from_zero_action.lodsw_address == 0x0678);
    assert(shared_helper_from_zero_action.first_helper_call_address == 0x067b);
    assert(shared_helper_from_zero_action.first_helper_address == 0x05f7);
    assert(shared_helper_from_zero_action.raw_sha256
        == "8dc7586f3809a14f3ed6acd601cd42486841adb9d9cb09d3e9b1ed727329e485");
    const auto shared_helper_from_nonzero_action = eon::evaluate_millennium_dos_shared_helper_prefix(
        *game_executable, first_special_action_nonzero.selected_ax_value);
    assert(shared_helper_from_nonzero_action.shifted_ax == 0x031c);
    const auto second_special_action_blocked =
        eon::evaluate_millennium_dos_second_special_action_prefix(*game_executable, 1);
    assert(second_special_action_blocked.runtime_byte_address == 0xda3a);
    assert(second_special_action_blocked.blocked_loop_address == 0xd3d2);
    assert(second_special_action_blocked.outcome
        == eon::MillenniumDosSecondSpecialActionOutcome::blocked_by_runtime_byte);
    const auto second_special_action_admitted =
        eon::evaluate_millennium_dos_second_special_action_prefix(*game_executable, 0);
    assert(second_special_action_admitted.handler_address == 0xd570);
    assert(second_special_action_admitted.selected_ax_value == 0x000d);
    assert(second_special_action_admitted.helper_call_address == 0xd573);
    assert(second_special_action_admitted.helper_address == 0x6c52);
    assert(second_special_action_admitted.outcome
        == eon::MillenniumDosSecondSpecialActionOutcome::helper_boundary);
    {
        auto altered_special_handler = *game_executable;
        altered_special_handler[0x11a4 - 0x100] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::evaluate_millennium_dos_first_special_action_prefix(
                altered_special_handler, 0));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_shared_helper = *game_executable;
        altered_shared_helper[0x0666 - 0x100] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::evaluate_millennium_dos_shared_helper_prefix(
                altered_shared_helper, 0x018f));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
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
    auto altered_startup_profile = *game_executable;
    altered_startup_profile[0xd2b4 - 0x100] ^= 0x01;
    bool rejected_altered_startup_profile = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_game_flow(altered_startup_profile));
    } catch (const std::runtime_error&) {
        rejected_altered_startup_profile = true;
    }
    assert(rejected_altered_startup_profile);
    auto altered_startup_first_call = *game_executable;
    altered_startup_first_call[0x0124 - 0x100] ^= 0x01;
    bool rejected_altered_startup_first_call = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_game_flow(altered_startup_first_call));
    } catch (const std::runtime_error&) {
        rejected_altered_startup_first_call = true;
    }
    assert(rejected_altered_startup_first_call);
    auto altered_startup_selector_path = *game_executable;
    altered_startup_selector_path[0xd1a9 - 0x100] ^= 0x01;
    bool rejected_altered_startup_selector_path = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_game_flow(altered_startup_selector_path));
    } catch (const std::runtime_error&) {
        rejected_altered_startup_selector_path = true;
    }
    assert(rejected_altered_startup_selector_path);
    auto altered_startup_other_followup = *game_executable;
    altered_startup_other_followup[0x0476 - 0x100] ^= 0x01;
    bool rejected_altered_startup_other_followup = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_game_flow(altered_startup_other_followup));
    } catch (const std::runtime_error&) {
        rejected_altered_startup_other_followup = true;
    }
    assert(rejected_altered_startup_other_followup);
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
    {
        bool rejected = false;
        try {
            static_cast<void>(game_session.observe_first_special_action(0));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    eon::MillenniumDosGameSession observed_game_session(game_flow, *game_executable);
    const auto observed_first_special = observed_game_session.observe_first_special_action(0x5a);
    assert(observed_first_special.action == 0x0b);
    assert(observed_game_session.last_special_action() == std::optional<std::uint8_t>{0x0b});
    assert(observed_game_session.last_first_special_action_trace());
    assert(observed_game_session.last_special_runtime_byte_effect());
    assert(observed_game_session.last_special_runtime_byte_effect()->address == 0x07f9);
    assert(observed_game_session.last_special_runtime_byte_effect()->previous
        == std::optional<std::uint8_t>{0x5a});
    assert(observed_game_session.last_special_runtime_byte_effect()->value == 0x5b);
    assert(observed_game_session.reconstructed_runtime_byte(0x07f9)
        == std::optional<std::uint8_t>{0x5b});
    const auto observed_second_special = observed_game_session.observe_second_special_action(1);
    assert(observed_second_special.action == 0x0c);
    assert(observed_second_special.outcome
        == eon::MillenniumDosSecondSpecialActionOutcome::blocked_by_runtime_byte);
    assert(observed_game_session.last_special_action() == std::optional<std::uint8_t>{0x0c});
    assert(observed_game_session.last_second_special_action_trace());
    assert(!observed_game_session.last_special_runtime_byte_effect());
    // The prior observed write remains a private in-memory trace, but action
    // $0c itself cannot supply a replacement for either native byte.
    assert(observed_game_session.reconstructed_runtime_byte(0x07f9)
        == std::optional<std::uint8_t>{0x5b});
    {
        auto altered_special_handler = *game_executable;
        altered_special_handler[0x11a4 - 0x100] ^= 0x01;
        eon::MillenniumDosGameSession altered_session(game_flow, altered_special_handler);
        bool rejected = false;
        try {
            static_cast<void>(altered_session.observe_first_special_action(0));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
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
    assert(game_session.last_fifth_function_key_trace()->third_call_address == 0x4bf7);
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
    assert(game_flow.eighth_function_key.repeated_call_address == 0x09fa);
    assert(game_flow.eighth_function_key.repeat_shift_register == 3);
    const std::array<std::uint8_t, 1> f8_single_return{{0x04}};
    const auto f8_single_loop = eon::evaluate_millennium_dos_eighth_function_key_repeat_loop(
        *game_executable, f8_single_return);
    assert(f8_single_loop.call_address == 0x7312);
    assert(f8_single_loop.helper_address == 0x09fa);
    assert(f8_single_loop.shift_address == 0x7315);
    assert(f8_single_loop.return_address == 0x7319);
    assert((f8_single_loop.shifted_bl_values == std::vector<std::uint8_t>{0x02}));
    assert(f8_single_loop.final_bl == 0x02);
    const std::array<std::uint8_t, 2> f8_repeated_returns{{0x01, 0x00}};
    const auto f8_repeated_loop = eon::evaluate_millennium_dos_eighth_function_key_repeat_loop(
        *game_executable, f8_repeated_returns);
    assert((f8_repeated_loop.shifted_bl_values == std::vector<std::uint8_t>{0x00, 0x00}));
    const auto f8_enabled_preflight = eon::evaluate_millennium_dos_eighth_function_key_preflight(
        *game_executable, 1, 3);
    assert(f8_enabled_preflight.entry_address == 0x731a);
    assert(f8_enabled_preflight.enabled_byte_address == 0xda39);
    assert(f8_enabled_preflight.helper_address == 0x7b47);
    assert(f8_enabled_preflight.outcome
        == eon::MillenniumDosEighthFunctionKeyPreflightOutcome::helper_boundary);
    assert(f8_enabled_preflight.return_address == 0x7324);
    assert(!f8_enabled_preflight.decremented_counter_byte);
    const auto f8_empty_preflight = eon::evaluate_millennium_dos_eighth_function_key_preflight(
        *game_executable, 0, 0);
    assert(f8_empty_preflight.outcome
        == eon::MillenniumDosEighthFunctionKeyPreflightOutcome::returns);
    assert(f8_empty_preflight.return_address == 0x732c);
    const auto f8_table_preflight = eon::evaluate_millennium_dos_eighth_function_key_preflight(
        *game_executable, 0, 3);
    assert(f8_table_preflight.outcome
        == eon::MillenniumDosEighthFunctionKeyPreflightOutcome::table_jump_boundary);
    assert(f8_table_preflight.decremented_counter_byte == std::optional<std::uint8_t>{2});
    assert(f8_table_preflight.translation_table_address == 0xdb4b);
    assert(f8_table_preflight.translation_index == std::optional<std::uint8_t>{2});
    assert(f8_table_preflight.table_jump_address == 0x7948);
    const auto f8_table_jump = eon::evaluate_millennium_dos_eighth_function_key_table_jump_prefix(
        *game_executable, 2);
    assert(f8_table_jump.entry_address == 0x7948);
    assert(f8_table_jump.reset_runtime_byte_address == 0xda09);
    assert(f8_table_jump.selected_runtime_byte_address == 0xda06);
    assert(f8_table_jump.selected_runtime_byte_value == 2);
    assert(f8_table_jump.selector_table_address == 0x78f4);
    assert(f8_table_jump.selected_pointer == 0x7815);
    assert(f8_table_jump.next_gate_runtime_byte_address == 0x6e2f);
    assert(f8_table_jump.zero_gate_address == 0x7968);
    assert(f8_table_jump.nonzero_gate_address == 0x799a);
    {
        auto altered_f8_loop = *game_executable;
        altered_f8_loop[0x7312 - 0x100] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::evaluate_millennium_dos_eighth_function_key_repeat_loop(
                altered_f8_loop, f8_single_return));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_f8_preflight = *game_executable;
        altered_f8_preflight[0x731a - 0x100] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::evaluate_millennium_dos_eighth_function_key_preflight(
                altered_f8_preflight, 0, 0));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        bool rejected = false;
        try {
            static_cast<void>(eon::evaluate_millennium_dos_eighth_function_key_table_jump_prefix(
                *game_executable, 10));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_f8_table_jump = *game_executable;
        altered_f8_table_jump[0x78f4 - 0x100] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::evaluate_millennium_dos_eighth_function_key_table_jump_prefix(
                altered_f8_table_jump, 2));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    const auto f8_selected_record_return =
        eon::evaluate_millennium_dos_eighth_function_key_selected_record_gate(
            *game_executable, 2, 1);
    assert(f8_selected_record_return.entry_address == 0x7961);
    assert(f8_selected_record_return.selected_pointer == 0x7815);
    assert(f8_selected_record_return.gate_runtime_byte_address == 0x6e2f);
    assert(f8_selected_record_return.gate_runtime_byte_value == 1);
    assert(f8_selected_record_return.nonzero_gate_address == 0x799a);
    assert(f8_selected_record_return.return_address == 0x799b);
    assert(f8_selected_record_return.outcome
        == eon::MillenniumDosEighthFunctionKeySelectedRecordOutcome::returns_without_record);
    // A nonzero $6e2f returns before the selected record is dereferenced.
    assert(!f8_selected_record_return.record_byte_0);
    assert(!f8_selected_record_return.record_byte_4);
    assert(!f8_selected_record_return.first_helper_address);
    const auto f8_selected_record_boundary =
        eon::evaluate_millennium_dos_eighth_function_key_selected_record_gate(
            *game_executable, 2, 0);
    assert(f8_selected_record_boundary.selected_pointer == 0x7815);
    assert(f8_selected_record_boundary.zero_gate_address == 0x7968);
    assert(f8_selected_record_boundary.record_byte_0 == std::optional<std::uint8_t>{0x04});
    assert(f8_selected_record_boundary.record_word_1 == std::optional<std::uint16_t>{0x2873});
    assert(f8_selected_record_boundary.record_byte_3 == std::optional<std::uint8_t>{1});
    assert(f8_selected_record_boundary.record_byte_4 == std::optional<std::uint8_t>{0x84});
    assert(f8_selected_record_boundary.first_helper_call_address
        == std::optional<std::uint16_t>{0x797f});
    assert(f8_selected_record_boundary.first_helper_address == std::optional<std::uint16_t>{0x7924});
    assert(f8_selected_record_boundary.outcome
        == eon::MillenniumDosEighthFunctionKeySelectedRecordOutcome::first_helper_boundary);
    const auto f8_last_selected_record =
        eon::evaluate_millennium_dos_eighth_function_key_selected_record_gate(
            *game_executable, 9, 0);
    assert(f8_last_selected_record.selected_pointer == 0x7886);
    assert(f8_last_selected_record.record_byte_4 == std::optional<std::uint8_t>{0x0a});
    {
        bool rejected = false;
        try {
            static_cast<void>(eon::evaluate_millennium_dos_eighth_function_key_selected_record_gate(
                *game_executable, 10, 0));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_f8_record_table = *game_executable;
        altered_f8_record_table[0x78f4 - 0x100] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::evaluate_millennium_dos_eighth_function_key_selected_record_gate(
                altered_f8_record_table, 2, 0));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_f8_record_bank = *game_executable;
        altered_f8_record_bank[0x77f8 - 0x100] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::evaluate_millennium_dos_eighth_function_key_selected_record_gate(
                altered_f8_record_bank, 2, 0));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_f8_record_interpreter = *game_executable;
        altered_f8_record_interpreter[0x797f - 0x100] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::evaluate_millennium_dos_eighth_function_key_selected_record_gate(
                altered_f8_record_interpreter, 2, 0));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    assert(game_session.observe_action(0x42) == std::optional<std::size_t>{7});
    assert(game_session.last_eighth_function_key_trace());
    assert(game_session.last_eighth_function_key_trace()->handler_address == 0x7306);
    assert(game_session.last_eighth_function_key_trace()->repeated_call_address == 0x09fa);
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
    const auto dos_control_text = eon::parse_millennium_dos_control_text_evidence(*static_data);
    assert((dos_control_text.pointer_indices
        == std::array<std::size_t, 5>{{271, 350, 390, 398, 399}}));
    assert(dos_control_text.literals[0].record_offset == 0x12a7);
    assert(dos_control_text.literals[0].literal == "left button / space");
    assert(dos_control_text.literals[1].literal == "press space bar to continue...");
    assert(dos_control_text.literals[2].literal == "press left button to continue...");
    assert(dos_control_text.literals[3].literal == "MOUSE MODE");
    assert(dos_control_text.literals[4].literal == "KEYBOARD MODE");
    auto altered_dos_control_text = *static_data;
    altered_dos_control_text[0x2bea] ^= 0x01;
    bool rejected_altered_dos_control_text = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_control_text_evidence(altered_dos_control_text));
    } catch (const std::runtime_error&) {
        rejected_altered_dos_control_text = true;
    }
    assert(rejected_altered_dos_control_text);
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
        == "c0a556f3e618585967b9ed3d6c0606f958434c94def1afd0940658786a88dd17");

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
    const auto* ibm = disk.find("IBM.COM");
    const auto* spanish_titles = disk.find("TITLES.EXE");
    const auto* graphics = disk.find("GX.LIB");
    const auto* spanish_title = disk.find("TITLE.LIB");
    const auto* spanish_static_data = disk.find("2200AD4.BIN");
    const auto* spanish_manual = disk.find("MILL.BAT");
    assert(executable && ibm && spanish_titles && spanish_manual && executable->size == 54'566);
    const auto spanish_title_boundary = eon::parse_millennium_dos_spanish_title_boundary(
        disk.read(*spanish_titles));
    assert(spanish_title_boundary.sha256 == "02082c35e18cee330f7d1b88098f502e68011f7e47a3a649961f6f03d1d14fe7");
    assert(spanish_title_boundary.post_title_entry_address == 0x1968);
    assert(spanish_title_boundary.private_driver_function == 0x13);
    assert(spanish_title_boundary.private_driver_call_count == 5);
    assert(graphics && graphics->size == 311'420);
    const auto* spanish_ega = disk.find("EGA640.BIN");
    const auto* spanish_mcga = disk.find("MCGA.BIN");
    assert(spanish_ega && spanish_mcga);
    const auto spanish_ega_profile = eon::parse_millennium_dos_spanish_video_driver(
        disk.read(*spanish_ega), eon::MillenniumDosVideoDriverKind::ega640);
    const auto spanish_mcga_profile = eon::parse_millennium_dos_spanish_video_driver(
        disk.read(*spanish_mcga), eon::MillenniumDosVideoDriverKind::mcga);
    assert(spanish_ega_profile.byte_size == 4630 && spanish_ega_profile.function_six_address == 0x8a6);
    assert(spanish_mcga_profile.byte_size == 4346 && spanish_mcga_profile.function_thirteen_address == 0x905);
    assert(spanish_title && spanish_title->size == 18'998);
    assert(spanish_static_data && spanish_static_data->size == 13'254);
    const auto spanish_launch_manual = eon::parse_millennium_dos_spanish_launch_manual(
        disk.read(*spanish_manual));
    assert(spanish_launch_manual.original_text.size() == 437);
    assert(spanish_launch_manual.original_text.find("IBM e") != std::string::npos);
    assert(spanish_launch_manual.original_text.find("IBM m") != std::string::npos);
    assert(eon::to_hex(eon::sha256(disk.read(*executable)))
        == "9f7d6f28f71eb7f2f6bb48cb3977efbf45049fc74083f8cbc865ec25396330c6");
    const auto spanish_ibm_handoff = eon::parse_millennium_dos_spanish_ibm_handoff_evidence(
        disk.read(*ibm), disk.read(*spanish_titles), disk.read(*executable));
    assert(spanish_ibm_handoff.caller_entry_address == 0x023d);
    assert(spanish_ibm_handoff.title_name_address == 0x071d);
    assert(spanish_ibm_handoff.game_name_address == 0x0728);
    assert(spanish_ibm_handoff.first_call_address == 0x0240);
    assert(spanish_ibm_handoff.second_call_address == 0x024c);
    assert(spanish_ibm_handoff.callee_address == 0x0339);
    assert(spanish_ibm_handoff.callee_return_address == 0x0368);
    assert(spanish_ibm_handoff.exec_parameter_block_address == 0x0708);
    assert(spanish_ibm_handoff.exec_ax == 0x4b00);
    assert(spanish_ibm_handoff.exec_interrupt == 0x21);
    assert(spanish_ibm_handoff.carry_branch_address == 0x0362);
    assert(spanish_ibm_handoff.carry_branch_target_address == 0x0369);
    assert(spanish_ibm_handoff.child_status_ah == 0x4d);
    assert(spanish_ibm_handoff.child_status_interrupt == 0x21);
    const auto spanish_game_startup = eon::parse_millennium_dos_spanish_game_startup_evidence(
        disk.read(*executable));
    assert(spanish_game_startup.executable_sha256
        == "9f7d6f28f71eb7f2f6bb48cb3977efbf45049fc74083f8cbc865ec25396330c6");
    assert(spanish_game_startup.entry_jump_target_address == 0xd2cd);
    assert(spanish_game_startup.startup_entry_address == 0xd2cd);
    assert(spanish_game_startup.startup_byte_count == 70);
    assert(spanish_game_startup.startup_sha256
        == "acbfcacc4cfac948944e42181f2fe0dfec11b9ab2c9b79b8aff79d958c5469c6");
    assert(spanish_game_startup.stack_pointer == 0xda00);
    assert(spanish_game_startup.private_function == 0x001f);
    assert(spanish_game_startup.private_record_address == 0xd1bb);
    assert(spanish_game_startup.private_call_address == 0xd2e2);
    assert(spanish_game_startup.private_wrapper_address == 0x0124);
    assert(spanish_game_startup.private_result_word_address == 0xd14a);
    assert(spanish_game_startup.result_compare_address == 0xd2f6);
    assert(spanish_game_startup.compared_al_value == 1);
    assert(spanish_game_startup.equal_call_address == 0xd2fa);
    assert(spanish_game_startup.equal_call_target_address == 0xd1be);
    assert(spanish_game_startup.other_call_address == 0xd2ff);
    assert(spanish_game_startup.other_call_target_address == 0xd1d2);
    const auto spanish_game_startup_callees = eon::parse_millennium_dos_spanish_game_startup_callees(
        disk.read(*executable), spanish_game_startup);
    assert(spanish_game_startup_callees.equal_entry_address == 0xd1be);
    assert(spanish_game_startup_callees.equal_byte_count == 20);
    assert(spanish_game_startup_callees.equal_sha256
        == "fdfc8f02550ee226dea27b1ac0204d1ead083c9d5585e18103bfe67435f0a5bb");
    assert(spanish_game_startup_callees.equal_private_function == 4);
    assert(spanish_game_startup_callees.equal_private_record_address == 0xd1bc);
    assert(spanish_game_startup_callees.equal_private_call_address == 0xd1c6);
    assert(spanish_game_startup_callees.equal_private_target_address == 0x0124);
    assert(spanish_game_startup_callees.equal_followup_call_address == 0xd1c9);
    assert(spanish_game_startup_callees.equal_followup_target_address == 0x044e);
    assert(spanish_game_startup_callees.equal_result_value == 1);
    assert(spanish_game_startup_callees.equal_result_storage_address == 0xda05);
    assert(spanish_game_startup_callees.equal_return_address == 0xd1d1);
    assert(spanish_game_startup_callees.other_entry_address == 0xd1d2);
    assert(spanish_game_startup_callees.other_byte_count == 28);
    assert(spanish_game_startup_callees.other_sha256
        == "6b8180c8f3b01e1f8810b2132756486dc761aee980949643129eeb53f6e86472");
    assert(spanish_game_startup_callees.other_private_function == 4);
    assert(spanish_game_startup_callees.other_private_record_address == 0xd1bc);
    assert(spanish_game_startup_callees.other_private_call_address == 0xd1da);
    assert(spanish_game_startup_callees.other_private_target_address == 0x0124);
    assert(spanish_game_startup_callees.other_followup_call_address == 0xd1dd);
    assert(spanish_game_startup_callees.other_followup_target_address == 0x0466);
    assert(spanish_game_startup_callees.other_result_source_address == 0xda05);
    assert(spanish_game_startup_callees.other_compare_value == 2);
    assert(spanish_game_startup_callees.other_equal_store_address == 0x0107);
    assert(spanish_game_startup_callees.other_return_address == 0xd1ed);
    const auto spanish_game_startup_followups = eon::parse_millennium_dos_spanish_game_startup_followups(
        disk.read(*executable), spanish_game_startup_callees);
    assert(spanish_game_startup_followups.equal_entry_address == 0x044e);
    assert(spanish_game_startup_followups.equal_byte_count == 8);
    assert(spanish_game_startup_followups.equal_sha256
        == "38889279a8b89e0e600bb25298015ccd8aadc09ea3858a1790097b3f7ff4ea8f");
    assert(spanish_game_startup_followups.equal_literal_value == 1);
    assert(spanish_game_startup_followups.equal_storage_address == 0xda05);
    assert(spanish_game_startup_followups.equal_return_address == 0x0455);
    assert(spanish_game_startup_followups.palette_entry_address == 0x0466);
    assert(spanish_game_startup_followups.palette_byte_count == 23);
    assert(spanish_game_startup_followups.palette_sha256
        == "b17db26fa4fa8b7307fb767ff98351bd6dcca202829dd2d9348ff4991942d779");
    assert(spanish_game_startup_followups.palette_table_address == 0x0456);
    assert((spanish_game_startup_followups.palette_table_values
        == std::array<std::uint8_t, 16>{0, 1, 2, 3, 4, 5, 6, 7, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f}));
    assert(spanish_game_startup_followups.palette_table_sha256
        == "ce46bce999708ea5109a857b0b6ecc02ece34eaf431cd148ef1aa1c0e80aed0a");
    assert(spanish_game_startup_followups.palette_initial_cx == 16);
    assert(spanish_game_startup_followups.bios_interrupt == 0x10);
    assert(spanish_game_startup_followups.bios_ax == 0x1000);
    assert(spanish_game_startup_followups.palette_return_address == 0x047c);
    {
        auto altered_spanish_game = disk.read(*executable);
        altered_spanish_game[0xd1cd] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_dos_spanish_game_startup_evidence(
                altered_spanish_game));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_spanish_game = disk.read(*executable);
        altered_spanish_game[0x034e] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_dos_spanish_game_startup_followups(
                altered_spanish_game, spanish_game_startup_callees));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_spanish_game = disk.read(*executable);
        altered_spanish_game[0xd0be] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_dos_spanish_game_startup_callees(
                altered_spanish_game, spanish_game_startup));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    // The complete Spanish executable ABI remains a separate preservation
    // boundary, but its F8 selected-record gate bytes are independently
    // accepted from the genuine FAT12 file in place.
    const auto spanish_f8_selected_record =
        eon::evaluate_millennium_dos_eighth_function_key_selected_record_gate(
            disk.read(*executable), 2, 0);
    assert(spanish_f8_selected_record.selected_pointer == 0x7815);
    assert(spanish_f8_selected_record.record_byte_4 == std::optional<std::uint8_t>{0x84});
    assert(spanish_f8_selected_record.first_helper_address
        == std::optional<std::uint16_t>{0x7924});
    {
        bool rejected = false;
        try {
            static_cast<void>(eon::evaluate_millennium_dos_second_special_action_prefix(
                disk.read(*executable), 0));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    // Identical handler bytes alone do not establish Spanish action-$0b
    // reachability: the evaluator requires the English dispatch slice too.
    {
        bool rejected = false;
        try {
            static_cast<void>(eon::evaluate_millennium_dos_first_special_action_prefix(
                disk.read(*executable), 0));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
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
    const auto spanish_control_text = eon::parse_millennium_dos_control_text_evidence(
        disk.read(*spanish_static_data));
    assert(spanish_control_text.source_sha256
        == "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31");
    assert((spanish_control_text.pointer_indices
        == std::array<std::size_t, 5>{{271, 350, 390, 398, 399}}));
    assert(spanish_control_text.literals[0].record_offset == 0x1351);
    assert(spanish_control_text.literals[0].literal_offset == 0x1359);
    assert(spanish_control_text.literals[0].literal == "boton / espacio");
    assert(spanish_control_text.literals[1].literal == "pulsa espacio para continuar..");
    assert(spanish_control_text.literals[2].literal == "pulsa el boton izquierdo para seguir");
    assert(spanish_control_text.literals[3].literal == "MODO RATON");
    assert(spanish_control_text.literals[4].literal == "MODO TECLADO");
    {
        auto altered_spanish_control_text = disk.read(*spanish_static_data);
        altered_spanish_control_text[0x2eb6] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_dos_control_text_evidence(
                altered_spanish_control_text));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }

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
    const auto atari_root_inventory = eon::inventory_millennium_atari_equinox_root(atari_disk);
    assert(atari_root_inventory.files.size() == 13);
    assert(atari_root_inventory.files.front().name == "DESKTOP.INF");
    assert(atari_root_inventory.files.front().first_cluster == 2);
    assert(atari_root_inventory.files.front().size == 555);
    assert(atari_root_inventory.files.front().sha256
        == "ce2aa85b442be281f25c22456c0d081d01b51108e96716bba9f867b7e791ab19");
    assert(atari_root_inventory.files[5].name == "MILL22E.INF");
    assert(atari_root_inventory.files[5].first_cluster == 122);
    assert(atari_root_inventory.files[5].size == 302'892);
    assert(atari_root_inventory.files[5].sha256
        == "9aeb6aafceab228521725ffe687cd3d95406d7f272bca77f855ebb600664b2af");
    assert(atari_root_inventory.files.back().name == "MILENIUM.TOS");
    assert(atari_root_inventory.files.back().first_cluster == 540);
    assert(atari_root_inventory.files.back().size == 49'269);
    assert(atari_root_inventory.files.back().sha256
        == "4584ddc459e3bf03e642f3156fbedb74aa33a847db4937beb5635eb492e93686");
    const auto* atari_save_i_entry = atari_disk.find("2200SAVE.I");
    const auto* atari_save_ii_entry = atari_disk.find("2200SAVE.II");
    const auto* atari_save_iii_entry = atari_disk.find("2200SAVE.III");
    const auto* atari_save_iv_entry = atari_disk.find("2200SAVE.IV");
    assert(atari_save_i_entry && atari_save_ii_entry && atari_save_iii_entry && atari_save_iv_entry);
    const auto authenticated_dos_save = eon::authenticate_millennium_save(
        eon::MillenniumSavePlatform::dos, "2200SAVE.I", *initial_save);
    const auto authenticated_atari_save_i = eon::authenticate_millennium_save(
        eon::MillenniumSavePlatform::atari_st, "2200SAVE.I", atari_disk.read(*atari_save_i_entry));
    const auto authenticated_atari_save_ii = eon::authenticate_millennium_save(
        eon::MillenniumSavePlatform::atari_st, "2200SAVE.II", atari_disk.read(*atari_save_ii_entry));
    const auto authenticated_atari_save_iii = eon::authenticate_millennium_save(
        eon::MillenniumSavePlatform::atari_st, "2200SAVE.III", atari_disk.read(*atari_save_iii_entry));
    const auto authenticated_atari_save_iv = eon::authenticate_millennium_save(
        eon::MillenniumSavePlatform::atari_st, "2200SAVE.IV", atari_disk.read(*atari_save_iv_entry));
    const auto dos_atari_i = eon::compare_millennium_saves(authenticated_dos_save,
        authenticated_atari_save_i);
    assert(dos_atari_i.shared_bytes == 7'313);
    assert(dos_atari_i.equal_positions == 6'030);
    assert(dos_atari_i.different_positions == 1'283);
    assert(dos_atari_i.common_prefix_bytes == 0 && dos_atari_i.common_suffix_bytes == 0);
    assert(dos_atari_i.left_only_bytes == 2'225 && dos_atari_i.right_only_bytes == 0);
    const auto atari_i_ii = eon::compare_millennium_saves(authenticated_atari_save_i,
        authenticated_atari_save_ii);
    assert(atari_i_ii.shared_bytes == 7'313 && atari_i_ii.equal_positions == 6'719);
    assert(atari_i_ii.different_positions == 594);
    assert(atari_i_ii.common_prefix_bytes == 22 && atari_i_ii.common_suffix_bytes == 6);
    const auto atari_iii_iv = eon::compare_millennium_saves(authenticated_atari_save_iii,
        authenticated_atari_save_iv);
    assert(atari_iii_iv.equal_positions == 6'607 && atari_iii_iv.different_positions == 706);
    assert(atari_iii_iv.common_prefix_bytes == 4 && atari_iii_iv.common_suffix_bytes == 8);
    auto altered_atari_save = atari_disk.read(*atari_save_i_entry);
    altered_atari_save[0] ^= 0x01;
    bool rejected_altered_atari_save = false;
    try {
        static_cast<void>(eon::authenticate_millennium_save(
            eon::MillenniumSavePlatform::atari_st, "2200SAVE.I", altered_atari_save));
    } catch (const std::runtime_error&) {
        rejected_altered_atari_save = true;
    }
    assert(rejected_altered_atari_save);
    {
        auto altered_atari_image = *atari_image;
        // DATA12.BIN starts at FAT12 cluster 442: first data byte is $2400
        // and each cluster is two 512-byte sectors. The original file is
        // shorter than one cluster, so this alters only that supplied file.
        altered_atari_image[0x2400 + (442 - 2) * 1024] ^= 0x01;
        bool rejected = false;
        try {
            const eon::Fat12Disk altered_disk(altered_atari_image);
            static_cast<void>(eon::inventory_millennium_atari_equinox_root(altered_disk));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    const auto atari_physical_dump = eon::extract_asset_by_sha256(atari_release->path,
        "081d8bc102b8c7669c5cb21abace9b08532bc0b34164f11465d0c87b63a422fd");
    assert(atari_physical_dump && atari_physical_dump->size() == 423'696);
    const auto atari_control_text = eon::parse_millennium_atari_physical_control_text_evidence(
        *atari_physical_dump);
    assert(atari_control_text.span_offset == 0x12420);
    assert(atari_control_text.span_size == 368);
    assert(atari_control_text.literals[0].literal == "SAVE GAME");
    assert(atari_control_text.literals[1].literal == "LOAD GAME");
    assert(atari_control_text.literals[2].literal == "press left button to continue...");
    assert(atari_control_text.literals[3].literal == "MOUSE MODE");
    assert(atari_control_text.literals[4].literal == "KEYBOARD MODE");
    auto altered_atari_control_text = *atari_physical_dump;
    altered_atari_control_text[0x12572] ^= 0x01;
    bool rejected_altered_atari_control_text = false;
    try {
        static_cast<void>(eon::parse_millennium_atari_physical_control_text_evidence(
            altered_atari_control_text));
    } catch (const std::runtime_error&) {
        rejected_altered_atari_control_text = true;
    }
    assert(rejected_altered_atari_control_text);
    const auto* atari_auxiliary_entry = atari_disk.find("MILL22B.INF");
    assert(atari_auxiliary_entry);
    const auto atari_auxiliary_resource = eon::probe_millennium_atari_auxiliary_resource_name(atari_disk);
    assert(atari_auxiliary_resource.container_filename == "MILL22B.INF");
    assert(atari_auxiliary_resource.first_cluster == atari_auxiliary_entry->first_cluster);
    assert(atari_auxiliary_resource.size == 84'720);
    assert(atari_auxiliary_resource.sha256
        == "e315b0ec01f2fe429fdce101765577b893d031389c540de1fbe43eca121d53e9");
    assert(atari_auxiliary_resource.literal_file_offset == 0x11600);
    assert(atari_auxiliary_resource.literal_filename == "MILL22E.INF");
    assert(atari_auxiliary_resource.preceding_return_file_offset == 0x115fe);
    {
        auto altered_atari_image = *atari_image;
        constexpr std::array<std::uint8_t, 11> auxiliary_name{{
            'M', 'I', 'L', 'L', '2', '2', 'B', ' ', 'I', 'N', 'F',
        }};
        const auto name = std::search(altered_atari_image.begin(), altered_atari_image.end(),
            auxiliary_name.begin(), auxiliary_name.end());
        assert(name != altered_atari_image.end());
        *name ^= 0x01;
        bool rejected = false;
        try {
            const eon::Fat12Disk altered_disk(altered_atari_image);
            static_cast<void>(eon::probe_millennium_atari_auxiliary_resource_name(altered_disk));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
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
    const auto atari_fopen_fallthrough = eon::parse_millennium_atari_fopen_fallthrough(
        atari_target, atari_trap);
    assert(atari_fopen_fallthrough.target_address == 0x77000);
    assert(atari_fopen_fallthrough.entry_offset == 0x1a);
    assert(atari_fopen_fallthrough.byte_count == 26);
    assert(atari_fopen_fallthrough.sha256
        == "663d5f1418326aa9c0efde064ad95bda21c84d7f23241ce3505f21f1f07474d0");
    assert(atari_fopen_fallthrough.fread_buffer_address == 0x2a500);
    assert(atari_fopen_fallthrough.fread_byte_count == 0x20000);
    assert(atari_fopen_fallthrough.handle_push_opcode == 0x3f00);
    assert(atari_fopen_fallthrough.fread_function == 0x3f);
    assert(atari_fopen_fallthrough.fread_trap_offset == 0x2c);
    assert(atari_fopen_fallthrough.stack_cleanup_opcode == 0xdffc);
    assert(atari_fopen_fallthrough.stack_cleanup_bytes == 12);
    const auto atari_fread_config_transfer = eon::parse_millennium_atari_fread_config_transfer_boundary(
        atari_target, atari_fopen_fallthrough);
    assert(atari_fread_config_transfer.target_address == 0x77000);
    assert(atari_fread_config_transfer.entry_offset == 0x34);
    assert(atari_fread_config_transfer.byte_count == 14);
    assert(atari_fread_config_transfer.sha256
        == "845d677c7c17d2152f0e89e0a396b6bbfb1ed6a75479a325b39310bbf0d99e58");
    assert(atari_fread_config_transfer.trap_opcode == 0x4e41);
    assert(atari_fread_config_transfer.stack_cleanup_opcode == 0xdffc);
    assert(atari_fread_config_transfer.stack_cleanup_bytes == 12);
    assert(atari_fread_config_transfer.config_jsr_opcode == 0x4eb9);
    assert(atari_fread_config_transfer.config_buffer_address == 0x2a500);
    const auto atari_config = eon::probe_millennium_atari_config(atari_disk);
    assert(atari_config.requested_filename == "MILL22A.inf");
    assert(atari_config.root_entry_count == 13);
    assert(atari_config.present);
    assert(atari_config.first_cluster == 3);
    assert(atari_config.size == 7'506);
    assert(atari_config.first_word == 0x4ef9);
    assert(atari_config.first_longword_operand == 0x2aa88);
    assert(atari_config.sha256 == "74d7d630779fd811aedcdbe31b14e54198eb9ffd673df512dd70b6165c4a37b6");
    const eon::MillenniumAtariBootstrapSession atari_session(
        atari_disk, atari_disk.read(*atari_executable));
    assert(atari_session.target().target_address == 0x77000);
    assert(atari_session.bss_source().original_data_bytes == 0xbc);
    assert(atari_session.bss_source().bss_zero_bytes == 0x146);
    assert(atari_session.fopen_boundary().fopen_filename == "MILL22A.inf");
    assert(atari_session.fopen_boundary().fopen_access_mode == 2);
    assert(atari_session.fopen_boundary().fopen_function == 0x3d);
    assert(atari_session.fopen_fallthrough().fread_function == 0x3f);
    assert(atari_session.fopen_fallthrough().fread_buffer_address == 0x2a500);
    assert(atari_session.fread_config_transfer().config_buffer_address == 0x2a500);
    assert(atari_session.root_inventory().files.size() == 13);
    assert(atari_session.root_inventory().files[11].name == "DATA12.BIN");
    assert(atari_session.root_inventory().files[11].sha256
        == "6f1e8ab7720c530f8cf5bfc07497824ff731ce977a15d941dad5acd999c6eeda");
    assert(atari_session.config().present);
    assert(atari_session.config().size == 7506);
    const auto atari_config_payload = atari_disk.read(*atari_disk.find("MILL22A.inf"));
    const auto atari_config_entry = eon::parse_millennium_atari_config_entry(atari_config_payload);
    assert(atari_config_entry.proven_load_base == 0x2a4de);
    assert(atari_config_entry.entry_address == 0x2aa88);
    assert(atari_config_entry.entry_file_offset == 0x5aa);
    assert(atari_config_entry.initial_trap_selector == 0x15);
    assert(atari_config_entry.initial_trap_longword_argument == 0);
    assert(atari_config_entry.second_trap_selector == 0x06);
    assert(atari_config_entry.second_trap_longword_argument == 0x2a612);
    assert((atari_config_entry.jsr_targets == std::vector<std::uint32_t>{
        0x2b55a, 0x2aa68, 0x2aa0c, 0x2b2be, 0x2b448, 0x2aa0c}));
    assert(atari_config_entry.final_pea_address == 0x2ab0a);
    assert(atari_config_entry.final_trap_selector == 0x26);
    assert(atari_config_entry.return_offset == 0x628);
    const auto atari_config_load_address_boundary = eon::parse_millennium_atari_fread_config_load_address_boundary(
        atari_fread_config_transfer, atari_config_payload, atari_config_entry);
    assert(atari_config_load_address_boundary.fread_destination_address == 0x2a500);
    assert(atari_config_load_address_boundary.payload_initial_jump_opcode == 0x4ef9);
    assert(atari_config_load_address_boundary.payload_initial_jump_target_address == 0x2aa88);
    assert(atari_config_load_address_boundary.payload_initial_jump_target_file_offset_from_destination == 0x588);
    assert(atari_config_load_address_boundary.payload_initial_jump_sha256
        == "5c2fb1d412ca66ba8928a77c22eb0351ab5d3d6fd9c04cff1b037f25a94c7829");
    assert(atari_config_load_address_boundary.independent_entry_load_base == 0x2a4de);
    assert(atari_config_load_address_boundary.independent_entry_file_offset == 0x5aa);
    assert(atari_config_load_address_boundary.independent_entry_offset_delta == 34);
    const auto atari_trap_argument_strings = eon::parse_millennium_atari_config_trap_argument_strings(
        atari_config_payload, atari_config_entry);
    assert(atari_trap_argument_strings.proven_load_base == 0x2a4de);
    assert(atari_trap_argument_strings.argument_address == 0x2a612);
    assert(atari_trap_argument_strings.file_offset == 0x134);
    assert((atari_trap_argument_strings.strings
        == std::array<std::string, 2>{"MILL22D.INF", "MILL22C.INF"}));
    assert(atari_trap_argument_strings.sha256
        == "815bea3862908e01557486cae7d42132853c94348b49b920f9d3e88e14956c51");
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
    const auto atari_third_routine = eon::parse_millennium_atari_config_third_routine(
        atari_config_payload, atari_config_entry);
    assert(atari_third_routine.target_address == 0x2b2be);
    assert(atari_third_routine.target_file_offset == 0xde0);
    assert(atari_third_routine.terminal_return_address == 0x2b3a4);
    assert(atari_third_routine.byte_count == 232);
    assert(atari_third_routine.sha256
        == "85c58759b0cb2f067734fb006aa543fc74926422187506914c823ceaaf9c6cd8");
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
    const auto atari_fourth_prelude = eon::parse_millennium_atari_config_fourth_prelude(
        atari_config_payload, atari_fourth_jsr);
    assert(atari_fourth_prelude.prelude_address == 0x2b426);
    assert(atari_fourth_prelude.prelude_file_offset == 0xf48);
    assert(atari_fourth_prelude.byte_count == 34);
    assert(atari_fourth_prelude.sha256
        == "6f135d6e68a1b6c48826ae484223166f4e6061cd4b6b5cbc2d0dfcc2bc8fb550");
    assert(atari_fourth_prelude.d0_setup_opcode == 0x203c);
    assert(atari_fourth_prelude.d0_initial_value == 0);
    assert(atari_fourth_prelude.d1_setup_opcode == 0x323c);
    assert(atari_fourth_prelude.d1_initial_value == 7);
    assert(atari_fourth_prelude.first_dbf_opcode == 0x51c9);
    assert(atari_fourth_prelude.first_dbf_displacement == -4);
    assert(atari_fourth_prelude.first_dbf_target_address == 0x2b430);
    assert(atari_fourth_prelude.a3_push_opcode == 0x2f0b);
    assert(atari_fourth_prelude.a5_initial_address == 0x2b3c8);
    assert(atari_fourth_prelude.second_d0_setup_opcode == 0x303c);
    assert(atari_fourth_prelude.second_d0_initial_value == 0x17);
    assert(atari_fourth_prelude.second_dbf_opcode == 0x51c8);
    assert(atari_fourth_prelude.second_dbf_displacement == -4);
    assert(atari_fourth_prelude.second_dbf_target_address == 0x2b442);
    assert(atari_fourth_prelude.continuation_address == 0x2b448);
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
    const auto atari_fourth_post_outer_tail = eon::parse_millennium_atari_config_fourth_post_outer_tail(
        atari_config_payload, atari_fourth_post_outer);
    assert(atari_fourth_post_outer_tail.tail_address == 0x2b48c);
    assert(atari_fourth_post_outer_tail.tail_file_offset == 0xfae);
    assert(atari_fourth_post_outer_tail.tail_bytes == 26);
    assert(atari_fourth_post_outer_tail.sha256
        == "34d497b9c4408944ea24d4eede21838f691c43d5a0d772db922187bed0e87fc8");
    assert(atari_fourth_post_outer_tail.initial_stack_cleanup_opcode == 0x5c8f);
    assert(atari_fourth_post_outer_tail.d0_load_opcode == 0x203c);
    assert(atari_fourth_post_outer_tail.d0_initial_value == 0x4e20);
    assert(atari_fourth_post_outer_tail.d0_decrement_opcode == 0x5380);
    assert(atari_fourth_post_outer_tail.d0_nonzero_branch_opcode == 0x66fc);
    assert(atari_fourth_post_outer_tail.d0_nonzero_branch_displacement == -4);
    assert(atari_fourth_post_outer_tail.d0_nonzero_branch_target_address == 0x2b494);
    assert(atari_fourth_post_outer_tail.d7_backedge_opcode == 0x51cf);
    assert(atari_fourth_post_outer_tail.d7_backedge_displacement == -78);
    assert(atari_fourth_post_outer_tail.d7_backedge_target_address == 0x2b44c);
    assert(atari_fourth_post_outer_tail.selector_push_opcode == 0x3f3c);
    assert(atari_fourth_post_outer_tail.selector == 0x0006);
    assert(atari_fourth_post_outer_tail.trap_opcode == 0x4e4e);
    assert(atari_fourth_post_outer_tail.final_stack_cleanup_opcode == 0x5c8f);
    assert(atari_fourth_post_outer_tail.return_opcode == 0x4e75);
    const auto atari_fourth_post_outer_recurrence = eon::parse_millennium_atari_config_fourth_post_outer_recurrence(
        atari_config_payload, atari_fourth_post_outer_tail, atari_fourth_loop);
    assert(atari_fourth_post_outer_recurrence.prefix_address == 0x2b44c);
    assert(atari_fourth_post_outer_recurrence.prefix_file_offset == 0xf6e);
    assert(atari_fourth_post_outer_recurrence.prefix_bytes == 24);
    assert(atari_fourth_post_outer_recurrence.sha256
        == "85f6e69ef8d058c021e0c70fe51375ef2f09a2c67c798c73f066ffdb6f14a187");
    assert(atari_fourth_post_outer_recurrence.continuation_address == 0x2b464);
    const auto atari_jsr_inventory = eon::inventory_millennium_atari_config_absolute_jsrs(atari_config_payload);
    assert(atari_jsr_inventory.encodings.size() == 19);
    assert((atari_jsr_inventory.encodings.front() == std::pair<std::uint32_t, std::uint32_t>{0x50c, 0x2a5aa}));
    assert((atari_jsr_inventory.encodings[9] == std::pair<std::uint32_t, std::uint32_t>{0x60a, 0x2aa0c}));
    assert((atari_jsr_inventory.encodings.back() == std::pair<std::uint32_t, std::uint32_t>{0xdb2, 0x2aa78}));
    const auto atari_residual_jsr_body = eon::parse_millennium_atari_config_residual_jsr_body(
        atari_config_payload, atari_jsr_inventory);
    assert(atari_residual_jsr_body.callsite_file_offset == 0xdac);
    assert(atari_residual_jsr_body.target_address == 0x2b576);
    assert(atari_residual_jsr_body.target_file_offset == 0x1098);
    assert(atari_residual_jsr_body.terminal_return_address == 0x2b5f8);
    assert(atari_residual_jsr_body.byte_count == 132);
    assert(atari_residual_jsr_body.sha256
        == "07e36fd52b00af1557c0da08efc7388d9d7cf6567e9c24102267db80b34adcd8");
    assert(atari_residual_jsr_body.first_opcode == 0x7000);
    assert(atari_residual_jsr_body.return_opcode == 0x4e75);
    auto invalid_atari_config_payload = atari_config_payload;
    invalid_atari_config_payload[0x5b9] ^= 0x01;
    bool invalid_atari_config_rejected = false;
    try {
        static_cast<void>(eon::parse_millennium_atari_config_entry(invalid_atari_config_payload));
    } catch (const std::runtime_error&) {
        invalid_atari_config_rejected = true;
    }
    assert(invalid_atari_config_rejected);
    auto invalid_atari_config_header_payload = atari_config_payload;
    invalid_atari_config_header_payload[0] ^= 0x01;
    bool invalid_atari_config_load_address_rejected = false;
    try {
        static_cast<void>(eon::parse_millennium_atari_fread_config_load_address_boundary(
            atari_fread_config_transfer, invalid_atari_config_header_payload, atari_config_entry));
    } catch (const std::runtime_error&) {
        invalid_atari_config_load_address_rejected = true;
    }
    assert(invalid_atari_config_load_address_rejected);
    auto invalid_atari_trap_argument_payload = atari_config_payload;
    invalid_atari_trap_argument_payload[0x134] ^= 0x01;
    bool invalid_atari_trap_argument_rejected = false;
    try {
        static_cast<void>(eon::parse_millennium_atari_config_trap_argument_strings(
            invalid_atari_trap_argument_payload, atari_config_entry));
    } catch (const std::runtime_error&) {
        invalid_atari_trap_argument_rejected = true;
    }
    assert(invalid_atari_trap_argument_rejected);
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
    auto invalid_atari_fourth_prelude_payload = atari_config_payload;
    invalid_atari_fourth_prelude_payload[0xf54] ^= 0x01;
    bool invalid_atari_fourth_prelude_rejected = false;
    try {
        static_cast<void>(eon::parse_millennium_atari_config_fourth_prelude(
            invalid_atari_fourth_prelude_payload, atari_fourth_jsr));
    } catch (const std::runtime_error&) {
        invalid_atari_fourth_prelude_rejected = true;
    }
    assert(invalid_atari_fourth_prelude_rejected);
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
    auto invalid_atari_fourth_post_outer_tail_payload = atari_config_payload;
    invalid_atari_fourth_post_outer_tail_payload[0xfae] ^= 0x01;
    bool invalid_atari_fourth_post_outer_tail_rejected = false;
    try {
        static_cast<void>(eon::parse_millennium_atari_config_fourth_post_outer_tail(
            invalid_atari_fourth_post_outer_tail_payload, atari_fourth_post_outer));
    } catch (const std::runtime_error&) {
        invalid_atari_fourth_post_outer_tail_rejected = true;
    }
    assert(invalid_atari_fourth_post_outer_tail_rejected);
    auto invalid_atari_fourth_post_outer_recurrence_payload = atari_config_payload;
    invalid_atari_fourth_post_outer_recurrence_payload[0xf6e] ^= 0x01;
    bool invalid_atari_fourth_post_outer_recurrence_rejected = false;
    try {
        static_cast<void>(eon::parse_millennium_atari_config_fourth_post_outer_recurrence(
            invalid_atari_fourth_post_outer_recurrence_payload, atari_fourth_post_outer_tail,
            atari_fourth_loop));
    } catch (const std::runtime_error&) {
        invalid_atari_fourth_post_outer_recurrence_rejected = true;
    }
    assert(invalid_atari_fourth_post_outer_recurrence_rejected);
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
    auto invalid_atari_residual_jsr_payload = atari_config_payload;
    invalid_atari_residual_jsr_payload[0x1098] ^= 0x01;
    bool invalid_atari_residual_jsr_rejected = false;
    try {
        static_cast<void>(eon::parse_millennium_atari_config_residual_jsr_body(
            invalid_atari_residual_jsr_payload, atari_jsr_inventory));
    } catch (const std::runtime_error&) {
        invalid_atari_residual_jsr_rejected = true;
    }
    assert(invalid_atari_residual_jsr_rejected);
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
    auto invalid_atari_fopen_fallthrough = atari_target;
    invalid_atari_fopen_fallthrough.bytes[0x1a] ^= 0x01;
    bool invalid_atari_fopen_fallthrough_rejected = false;
    try {
        static_cast<void>(eon::parse_millennium_atari_fopen_fallthrough(
            invalid_atari_fopen_fallthrough, atari_trap));
    } catch (const std::runtime_error&) {
        invalid_atari_fopen_fallthrough_rejected = true;
    }
    assert(invalid_atari_fopen_fallthrough_rejected);
    auto invalid_atari_fread_config_transfer = atari_target;
    invalid_atari_fread_config_transfer.bytes[0x34] ^= 0x01;
    bool invalid_atari_fread_config_transfer_rejected = false;
    try {
        static_cast<void>(eon::parse_millennium_atari_fread_config_transfer_boundary(
            invalid_atari_fread_config_transfer, atari_fopen_fallthrough));
    } catch (const std::runtime_error&) {
        invalid_atari_fread_config_transfer_rejected = true;
    }
    assert(invalid_atari_fread_config_transfer_rejected);
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
    const eon::DeuterosAtariBootstrapSession deuteros_atari_session(*deuteros_st_disk1);
    assert(deuteros_atari_session.first_stage_sha256()
        == "dad3594c53375bd8285ef33e2d685bd38a5b38d930f2ea1305d117d63667f168");
    assert(deuteros_atari_session.second_stage_sha256()
        == "2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7");
    assert(deuteros_atari_session.first_stage().next_destination == 0x70000);
    assert(deuteros_atari_session.second_stage().direct_entry == 0x1ec4);
    assert(deuteros_atari_session.post_callback_callees().first_callee_offset == 0x800);
    assert(deuteros_atari_session.first_callee_continuation().continuation_offset == 0x1116);
    assert(deuteros_atari_session.post_callback_callees().second_callee_bsr_target_offset == 0x30);
    assert(deuteros_atari_session.second_callee_continuation().copy_loop_target_offset == 0x1156);
    assert(deuteros_atari_session.raw_reader_wrapper().raw_reader_bsr_target_offset == 0x60);
    assert(deuteros_atari_session.state_selection_layout().source_longword_address == 0x25fc);
    assert(deuteros_atari_session.state_selection_continuation().raw_reader_wrapper_target_offset == 0x30);
    assert(deuteros_atari_session.dispatch().state0_destination == 0x13200);
    assert(deuteros_atari_session.state1_service_boundary().xbios_selector == 0x26);
    assert(deuteros_atari_session.state0_raw_load_plan().source_offset == 0x4800);
    assert(deuteros_atari_session.state0_raw_load_plan().requests.size() == 4);
    assert(deuteros_atari_session.state1_raw_load_plan().source_offset == 0x55800);
    assert(deuteros_atari_session.state1_raw_load_plan().requests.size() == 84);
    assert(deuteros_atari_session.state5_raw_load_plan().first_read.source_offset == 0x55800);
    assert(deuteros_atari_session.state5_raw_load_plan().second_read.source_offset == 0x60c00);
    assert(deuteros_atari_session.state5_return().branch_target_offset == 0x114);
    assert(deuteros_atari_session.supervisor_callback().callback_address == 0x1fa6);
    assert(deuteros_atari_session.supervisor_callback_continuation().ram_word_address == 0x25f4);
    assert(deuteros_atari_session.supervisor_callback_continuation().branch_target_offset == 0xf2);
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
    assert(deuteros_first_stage_profile.checksum_byte_count == 0x43b);
    assert(deuteros_first_stage_profile.checksum_seed == 0x22225555);
    assert(deuteros_first_stage_profile.checksum_expected == 0x7ae26af7);
    assert(eon::calculate_deuteros_atari_first_stage_checksum(
        deuteros_first_stage, deuteros_first_stage_profile)
        == deuteros_first_stage_profile.checksum_expected);
    {
        auto altered_first_stage = deuteros_first_stage;
        altered_first_stage[deuteros_first_stage_profile.checksum_start_offset] ^= 0x01;
        assert(eon::calculate_deuteros_atari_first_stage_checksum(
            altered_first_stage, deuteros_first_stage_profile)
            != deuteros_first_stage_profile.checksum_expected);
    }
    assert(deuteros_first_stage_profile.next_track == 2);
    assert(deuteros_first_stage_profile.next_side == 0);
    assert(deuteros_first_stage_profile.next_sector == 1);
    assert(deuteros_first_stage_profile.next_sector_count == 9);
    assert(deuteros_first_stage_profile.next_destination == 0x70000);
    assert(deuteros_first_stage_profile.copy_source == 0x70000);
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
    assert(deuteros_second_stage_profile.direct_entry_source_offset == 0xc4);
    assert(deuteros_second_stage_profile.dispatch_state_address == 0x1eaa);
    assert(deuteros_second_stage_profile.dispatch_table_address == 0x1eac);
    assert(deuteros_second_stage_profile.dispatch_raw_reader_address == 0x70030);
    const auto deuteros_dispatch = eon::parse_deuteros_atari_dispatch(deuteros_second_stage);
    assert((deuteros_dispatch.vector_addresses
        == std::array<std::uint32_t, 6>{{0x1f1a, 0x1f2e, 0x1f50, 0x1f1a, 0x1f1a, 0x1f52}}));
    assert((deuteros_dispatch.state0_alias_addresses
        == std::array<std::uint32_t, 3>{{0x1f50, 0x1f1a, 0x1f1a}}));
    assert(deuteros_dispatch.state0_destination == 0x13200);
    assert(deuteros_dispatch.state0_byte_count == 0x4800);
    assert(deuteros_dispatch.state0_linear_sector == 4);
    const auto deuteros_state0_plan = eon::build_deuteros_atari_state0_raw_load_plan(
        deuteros_second_stage_profile, deuteros_dispatch);
    assert(deuteros_state0_plan.destination == 0x13200);
    assert(deuteros_state0_plan.byte_count == 0x4800);
    assert(deuteros_state0_plan.source_linear_sector == 4);
    assert(deuteros_state0_plan.source_offset == 0x4800);
    std::vector<std::uint8_t> deuteros_state0_bytes;
    const std::array<std::string_view, 4> state0_chunk_hashes{{
        "2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7",
        "c5cef5d02d47d09a758487e873ce1e86a9905b0e62241fc3bff7a8bf9114718a",
        "2515d3507aa37eaf5bbc0dd12f72a8dcc44712e4773a1e9e3f57517f8a21777c",
        "510e1793d5d08ef18d5bc5039f5843aa403024c63abaad000078c61f65011e34",
    }};
    for (std::size_t index = 0; index < deuteros_state0_plan.requests.size(); ++index) {
        const auto& request = deuteros_state0_plan.requests[index];
        assert(request.track == 2 + index / 2U);
        assert(request.side == index % 2U);
        assert(request.first_sector == 1);
        assert(request.sector_count == 9);
        assert(request.source_offset == 0x4800 + index * 0x1200);
        const auto chunk = deuteros_disk1.read_sectors(request.track, request.side,
            request.first_sector, request.sector_count);
        assert(eon::to_hex(eon::sha256(chunk)) == state0_chunk_hashes[index]);
        deuteros_state0_bytes.insert(deuteros_state0_bytes.end(), chunk.begin(), chunk.end());
    }
    assert(deuteros_state0_bytes.size() == deuteros_state0_plan.byte_count);
    assert(eon::to_hex(eon::sha256(deuteros_state0_bytes))
        == "88afae4bd5182d916183b01bf688ab524d739749e84a092eda1435e386b57b58");
    const auto state0_duplicate = eon::parse_deuteros_atari_state0_duplicate_stage_prefix(
        deuteros_state0_bytes, deuteros_second_stage);
    assert(state0_duplicate.byte_count == 0x1200);
    assert(state0_duplicate.sha256
        == "2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7");
    assert(state0_duplicate.direct_entry_offset == 0);
    assert(state0_duplicate.dispatcher_offset == 0xc4);
    {
        auto altered_state0 = deuteros_state0_bytes;
        altered_state0[0x11ff] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_deuteros_atari_state0_duplicate_stage_prefix(
                altered_state0, deuteros_second_stage));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    assert(deuteros_dispatch.state1_destination == 0xb000);
    assert(deuteros_dispatch.state1_byte_count == 0x5e400);
    assert(deuteros_dispatch.state1_linear_sector == 0x4c);
    assert_deuteros_atari_state1_service_boundary(deuteros_second_stage,
        deuteros_second_stage_profile, deuteros_dispatch);
    const auto deuteros_state1_plan = eon::build_deuteros_atari_state1_raw_load_plan(
        deuteros_second_stage_profile, deuteros_dispatch);
    assert(deuteros_state1_plan.destination == 0xb000);
    assert(deuteros_state1_plan.byte_count == 0x5e400);
    assert(deuteros_state1_plan.source_offset == 0x55800);
    assert(deuteros_state1_plan.requests.size() == 84);
    assert(deuteros_state1_plan.requests.front().track == 38);
    assert(deuteros_state1_plan.requests.front().side == 0);
    assert(deuteros_state1_plan.requests.back().track == 79);
    assert(deuteros_state1_plan.requests.back().side == 1);
    assert(deuteros_state1_plan.requests.back().sector_count == 7);
    std::vector<std::uint8_t> deuteros_state1_bytes;
    for (const auto& request : deuteros_state1_plan.requests) {
        const auto chunk = deuteros_disk1.read_sectors(request.track, request.side,
            request.first_sector, request.sector_count);
        assert(chunk.size() == static_cast<std::size_t>(request.sector_count) * 512U);
        deuteros_state1_bytes.insert(deuteros_state1_bytes.end(), chunk.begin(), chunk.end());
    }
    assert(deuteros_state1_bytes.size() == deuteros_state1_plan.byte_count);
    assert(eon::to_hex(eon::sha256(deuteros_state1_bytes))
        == "0d5ccb3a337fcbd4d34d34b3ad24f20c3bb2edca7e7b734b8abb14f6c0a30f47");
    const auto deuteros_state1_skipped_ascii = eon::parse_deuteros_atari_state1_skipped_ascii_block(
        deuteros_state1_bytes, deuteros_state1_plan);
    assert(deuteros_state1_skipped_ascii.branch_relative_offset == 0x48000);
    assert(deuteros_state1_skipped_ascii.branch_displacement == 0x09c2);
    assert(deuteros_state1_skipped_ascii.ascii_relative_offset == 0x4800a);
    assert(deuteros_state1_skipped_ascii.ascii_byte_count == 0x438);
    assert(deuteros_state1_skipped_ascii.printable_run_count == 18);
    assert(deuteros_state1_skipped_ascii.ascii_sha256
        == "8dd46e7c760a38d07273b18a4cbd3c03eb44a6b57c8c401580dd47fa4646484e");
    {
        auto altered_state1 = deuteros_state1_bytes;
        altered_state1[0x4800a] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_deuteros_atari_state1_skipped_ascii_block(
                altered_state1, deuteros_state1_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    assert(deuteros_dispatch.state5_first_destination == 0xb000);
    assert(deuteros_dispatch.state5_first_byte_count == 0xb400);
    assert(deuteros_dispatch.state5_first_reader_argument == 0x55800);
    assert(deuteros_dispatch.state5_copy_source == 0x57a00);
    assert(deuteros_dispatch.state5_copy_destination == 0xb006);
    assert(deuteros_dispatch.state5_copy_byte_count == 0x9393);
    assert(deuteros_dispatch.state5_second_destination == 0x16400);
    assert(deuteros_dispatch.state5_second_byte_count == 0x4c800);
    assert(deuteros_dispatch.state5_second_reader_argument == 0x60c00);
    const auto deuteros_state5_plan = eon::build_deuteros_atari_state5_raw_load_plan(
        deuteros_second_stage_profile, deuteros_dispatch);
    assert(deuteros_state5_plan.first_read.requests.size() == 10);
    assert(deuteros_state5_plan.first_read.source_offset == 0x55800);
    assert(deuteros_state5_plan.first_read.requests.front().track == 38);
    assert(deuteros_state5_plan.first_read.requests.front().side == 0);
    assert(deuteros_state5_plan.copy_source == 0x57a00);
    assert(deuteros_state5_plan.copy_destination == 0xb006);
    assert(deuteros_state5_plan.copy_byte_count == 0x9393);
    assert(deuteros_state5_plan.second_read.requests.size() == 68);
    assert(deuteros_state5_plan.second_read.source_offset == 0x60c00);
    assert(deuteros_state5_plan.second_read.requests.front().track == 43);
    assert(deuteros_state5_plan.second_read.requests.front().side == 0);
    const auto deuteros_state5_return = eon::parse_deuteros_atari_state5_return(
        deuteros_second_stage, deuteros_second_stage_profile, deuteros_dispatch);
    assert(deuteros_state5_return.branch_offset == 0x1a2);
    assert(deuteros_state5_return.branch_displacement == -144);
    assert(deuteros_state5_return.branch_target_offset == 0x114);
    assert(deuteros_state5_return.branch_sha256
        == "4d11113ca2040c3c0d8e9fe7fc7ef2b65175cc580b8a4b81466908ae7c537896");
    assert(deuteros_state5_return.dispatcher_tail_offset == 0x114);
    assert(deuteros_state5_return.dispatcher_tail_sha256
        == "506215d03a2272be5f938a8926864075fc50a79d8c2fc23f22955d290fe0c98f");
    assert(deuteros_state5_return.state_word_address == 0x1eaa);
    assert(deuteros_state5_return.move_word_opcode == 0x3038);
    assert(deuteros_state5_return.return_opcode == 0x4e75);
    {
        auto altered_second_stage = deuteros_second_stage;
        altered_second_stage[0x1a2] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_deuteros_atari_state5_return(
                altered_second_stage, deuteros_second_stage_profile, deuteros_dispatch));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    const auto deuteros_supervisor_callback = eon::parse_deuteros_atari_supervisor_callback(
        deuteros_second_stage, deuteros_second_stage_profile);
    assert(deuteros_supervisor_callback.callsite_offset == 0xd2);
    assert(deuteros_supervisor_callback.callsite_bytes == 10);
    assert(deuteros_supervisor_callback.callsite_sha256
        == "11b26d5900e614547617a9c95611515e8238184756a0a18c7ff18b1ec372657b");
    assert(deuteros_supervisor_callback.callback_address == 0x1fa6);
    assert(deuteros_supervisor_callback.callback_offset == 0x1a6);
    assert(deuteros_supervisor_callback.callback_bytes == 12);
    assert(deuteros_supervisor_callback.callback_sha256
        == "1f8bdb0e61454fef9acb0dc3abcf7bfed2621828937380b415ab85d4f57ef143");
    assert(deuteros_supervisor_callback.callback_push_opcode == 0x2f3c);
    assert(deuteros_supervisor_callback.xbios_selector == 0x0026);
    assert(deuteros_supervisor_callback.trap_opcode == 0x4e4e);
    assert(deuteros_supervisor_callback.callback_return_address_load_opcode == 0x2017);
    assert(deuteros_supervisor_callback.callback_stack_address == 0x7b000);
    assert(deuteros_supervisor_callback.callback_stack_move_opcode == 0x2f00);
    assert(deuteros_supervisor_callback.callback_return_opcode == 0x4e75);
    const auto deuteros_supervisor_callback_continuation =
        eon::parse_deuteros_atari_supervisor_callback_continuation(
            deuteros_second_stage, deuteros_second_stage_profile, deuteros_supervisor_callback);
    assert(deuteros_supervisor_callback_continuation.continuation_offset == 0xde);
    assert(deuteros_supervisor_callback_continuation.continuation_bytes == 20);
    assert(deuteros_supervisor_callback_continuation.continuation_sha256
        == "ed326a1d22a28ce5646b242c947c5120cb0855d6d05080e35ce398d48d459f56");
    assert(deuteros_supervisor_callback_continuation.ram_read_opcode == 0x2038);
    assert(deuteros_supervisor_callback_continuation.ram_word_address == 0x25f4);
    assert(deuteros_supervisor_callback_continuation.compare_opcode == 0xb0bc);
    assert(deuteros_supervisor_callback_continuation.compare_immediate == 0x71100);
    assert(deuteros_supervisor_callback_continuation.branch_opcode == 0x6708);
    assert(deuteros_supervisor_callback_continuation.branch_displacement == 8);
    assert(deuteros_supervisor_callback_continuation.branch_target_offset == 0xf2);
    assert(deuteros_supervisor_callback_continuation.first_bsr_opcode == 0x6100);
    assert(deuteros_supervisor_callback_continuation.first_bsr_displacement == 0x714);
    assert(deuteros_supervisor_callback_continuation.first_bsr_target_offset == 0x800);
    assert(deuteros_supervisor_callback_continuation.second_bsr_opcode == 0x6100);
    assert(deuteros_supervisor_callback_continuation.second_bsr_displacement == 0x1032);
    assert(deuteros_supervisor_callback_continuation.second_bsr_target_offset == 0x1122);
    assert_deuteros_atari_post_callback_callees(deuteros_second_stage, deuteros_second_stage_profile,
        deuteros_supervisor_callback_continuation);
    assert_deuteros_atari_first_callee_continuation(deuteros_second_stage,
        deuteros_second_stage_profile, eon::parse_deuteros_atari_post_callback_callee_profiles(
            deuteros_second_stage, deuteros_second_stage_profile,
            deuteros_supervisor_callback_continuation));
    assert_deuteros_atari_second_callee_continuation(deuteros_second_stage,
        deuteros_second_stage_profile, eon::parse_deuteros_atari_post_callback_callee_profiles(
            deuteros_second_stage, deuteros_second_stage_profile,
            deuteros_supervisor_callback_continuation));
    assert_deuteros_atari_raw_reader_wrapper(deuteros_second_stage, deuteros_second_stage_profile,
        eon::parse_deuteros_atari_post_callback_callee_profiles(deuteros_second_stage,
            deuteros_second_stage_profile, deuteros_supervisor_callback_continuation));
    assert_deuteros_atari_raw_reader_call_layout(deuteros_second_stage, deuteros_second_stage_profile,
        eon::parse_deuteros_atari_raw_reader_wrapper(deuteros_second_stage,
            deuteros_second_stage_profile,
            eon::parse_deuteros_atari_post_callback_callee_profiles(deuteros_second_stage,
                deuteros_second_stage_profile, deuteros_supervisor_callback_continuation)));
    assert_deuteros_atari_direct_vector_callees(deuteros_second_stage,
        deuteros_second_stage_profile, deuteros_dispatch);
    assert_deuteros_atari_direct_vector_transfer_loop(deuteros_second_stage,
        deuteros_second_stage_profile, deuteros_dispatch);
    assert_deuteros_atari_direct_vector_transfer_tail(deuteros_second_stage,
        deuteros_second_stage_profile, deuteros_dispatch);
    assert_deuteros_atari_state_selection_layout(deuteros_second_stage, deuteros_second_stage_profile,
        deuteros_dispatch);
    assert_deuteros_atari_state_selection_continuation(deuteros_second_stage,
        deuteros_second_stage_profile,
        eon::parse_deuteros_atari_state_selection_layout(deuteros_second_stage,
            deuteros_second_stage_profile, deuteros_dispatch),
        eon::parse_deuteros_atari_raw_reader_wrapper(deuteros_second_stage,
            deuteros_second_stage_profile,
            eon::parse_deuteros_atari_post_callback_callee_profiles(deuteros_second_stage,
                deuteros_second_stage_profile, deuteros_supervisor_callback_continuation)));
    {
        auto altered_second_stage = deuteros_second_stage;
        altered_second_stage[0xde] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_deuteros_atari_supervisor_callback_continuation(
                altered_second_stage, deuteros_second_stage_profile, deuteros_supervisor_callback));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_second_stage = deuteros_second_stage;
        altered_second_stage[0x1a6] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_deuteros_atari_supervisor_callback(
                altered_second_stage, deuteros_second_stage_profile));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    const auto materialize_atari_plan = [&deuteros_disk1](const auto& plan) {
        std::vector<std::uint8_t> bytes;
        for (const auto& request : plan.requests) {
            const auto chunk = deuteros_disk1.read_sectors(request.track, request.side,
                request.first_sector, request.sector_count);
            bytes.insert(bytes.end(), chunk.begin(), chunk.end());
        }
        return bytes;
    };
    assert(eon::to_hex(eon::sha256(materialize_atari_plan(deuteros_state5_plan.first_read)))
        == "9659b21315e5c0528020be0b41eb75d57428f41b3b632fabfebe16d34038d298");
    assert(eon::to_hex(eon::sha256(materialize_atari_plan(deuteros_state5_plan.second_read)))
        == "6b3e27702649ac201c4ecf92ad54f40656fd4d8633fadf5790014da34ce03ac6");
    auto deuteros_state5_bytes = materialize_atari_plan(deuteros_state5_plan.first_read);
    const auto deuteros_state5_second_bytes = materialize_atari_plan(deuteros_state5_plan.second_read);
    deuteros_state5_bytes.insert(deuteros_state5_bytes.end(), deuteros_state5_second_bytes.begin(),
        deuteros_state5_second_bytes.end());
    const auto deuteros_state5_state1_prefix = eon::validate_deuteros_atari_state5_state1_prefix(
        deuteros_state1_plan, deuteros_state5_plan, deuteros_state1_bytes, deuteros_state5_bytes);
    assert(deuteros_state5_state1_prefix.source_offset == 0x55800);
    assert(deuteros_state5_state1_prefix.byte_count == 0x57c00);
    assert(deuteros_state5_state1_prefix.sha256
        == "ed55ad2a893a87af9f11d269faa6358420c47ed6beb1fee7a177e9beaed1e77c");
    {
        auto altered_state5_bytes = deuteros_state5_bytes;
        altered_state5_bytes[0] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::validate_deuteros_atari_state5_state1_prefix(
                deuteros_state1_plan, deuteros_state5_plan, deuteros_state1_bytes,
                altered_state5_bytes));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    assert(deuteros_second_stage_profile.raw_read_routine_offset == 0x60);
    assert(deuteros_second_stage_profile.raw_read_max_sector_count == 9);
    assert(deuteros_second_stage_profile.side_switch_track == 0x50);
    assert(deuteros_disk2.boot_profile().boot_checksum == 0x1234);
    assert(deuteros_disk2.boot_profile().boot_branch_target == 0x22);
    assert(deuteros_disk2.boot_profile().killer_boot_signature);
    assert(deuteros_disk2.boot_profile().has_killer_boot_vector_setup);
    assert(deuteros_disk2.boot_profile().killer_boot_entry_offset == 0x30);
    assert(deuteros_disk2.boot_profile().killer_boot_vector_source_offset == 0xf0);
    assert(deuteros_disk2.boot_profile().killer_boot_vector_destination == 0x8);
    assert(deuteros_disk2.boot_profile().killer_boot_vector_longword_count == 10);
    assert(deuteros_disk2.boot_profile().killer_boot_continuation == 0x12);
    assert(deuteros_disk2.boot_profile().has_killer_boot_continuation_profile);
    assert(deuteros_disk2.boot_profile().killer_boot_relocated_byte_count == 40);
    assert(deuteros_disk2.boot_profile().killer_boot_relocated_sha256
        == "21a5d61e2289fe2f2141d3710fad31faf42e96f59c5fba768819380e8f595a8d");
    assert(deuteros_disk2.boot_profile().killer_boot_clear_start == 0x30);
    assert(deuteros_disk2.boot_profile().killer_boot_clear_stride == 0x20);
    assert(deuteros_disk2.boot_profile().killer_boot_clear_longword_count == 8);
    const auto deuteros_disk2_boot = deuteros_disk2.read_sectors(0, 0, 1, 1);
    const auto deuteros_killer_handoff = eon::parse_deuteros_atari_killer_boot_handoff(
        deuteros_disk2_boot, deuteros_disk2.boot_profile());
    assert(deuteros_killer_handoff.setup_offset == 0xd8);
    assert(deuteros_killer_handoff.setup_byte_count == 24);
    assert(deuteros_killer_handoff.setup_sha256
        == "1ce81773d11374cac65ce69742a475e0731cbc8798f7c7bd374c04a2d2a7d150");
    assert(deuteros_killer_handoff.source_offset == 0xf0);
    assert(deuteros_killer_handoff.byte_count == 40);
    assert(deuteros_killer_handoff.destination == 0x8);
    assert(deuteros_killer_handoff.continuation_address == 0x12);
    assert(deuteros_killer_handoff.continuation_relocated_offset == 10);
    assert(deuteros_killer_handoff.continuation_first_opcode == 0x41fa);
    assert(deuteros_killer_handoff.vector_jump_relocated_offset == 8);
    assert(deuteros_killer_handoff.vector_jump_opcode == 0x4ed0);
    assert(deuteros_killer_handoff.vector_jump_pointer_address == 0x4);
    {
        auto altered_boot = deuteros_disk2_boot;
        altered_boot[0xd8] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_deuteros_atari_killer_boot_handoff(
                altered_boot, deuteros_disk2.boot_profile()));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_disk2 = *deuteros_st_disk2;
        altered_disk2[0xf0 + 39] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::DeuterosAtariDisk(std::move(altered_disk2)));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }

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
    const eon::DeuterosAmigaTitleStageSession title_stage_session(system_disk, load_plan);
    assert(title_stage_session.stage().disk_offset == 0x6e000);
    assert(title_stage_session.stage().length == 0x6ca00);
    assert(title_stage_session.stage().destination == 0x13000);
    assert(title_stage_session.stage().entry_address == 0x40426);
    assert(title_stage_session.original_bytes().size() == 0x6ca00);
    assert(title_stage_session.original_sha256()
        == "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03");
    // The timer-gated $4069a route has a wholly local prefix before its first
    // graphics.library vector. It transforms the real title RGB4 words in
    // memory only; this test does not imply that the timer gate or vector has
    // been executed.
    const auto transition_prefix = eon::execute_deuteros_amiga_title_transition_prefix(
        system_disk, load_plan, 0x5a5a);
    assert(transition_prefix.entry_address == 0x4069a);
    assert(transition_prefix.active_flag_address == 0x202c6);
    assert(transition_prefix.active_flag_value == 1);
    assert(transition_prefix.saved_display_word_address == 0x202b8);
    assert(transition_prefix.saved_display_word == 0x5a5a);
    assert(transition_prefix.cleared_display_word == 0);
    assert(transition_prefix.source_palette_address == 0x1ed24);
    assert(transition_prefix.work_palette_address == 0x40678);
    const std::array<std::uint16_t, 16> expected_transition_palette{{
        0x0000, 0x0453, 0x0342, 0x0231, 0x0110, 0x0500, 0x0610, 0x0300,
        0x0014, 0x0067, 0x0040, 0x0430, 0x0770, 0x0700, 0x0400, 0x0777,
    }};
    assert(transition_prefix.work_palette_words == expected_transition_palette);
    std::array<std::uint8_t, 32> transition_palette_bytes{};
    for (std::size_t index = 0; index < expected_transition_palette.size(); ++index) {
        transition_palette_bytes[index * 2U] = static_cast<std::uint8_t>(
            expected_transition_palette[index] >> 8U);
        transition_palette_bytes[index * 2U + 1U] = static_cast<std::uint8_t>(
            expected_transition_palette[index]);
    }
    assert(eon::to_hex(eon::sha256(transition_palette_bytes))
        == "e8f4bdf6b52bc849b626145464ccbc2701c6869cc97e62ef9dcfecb660a01aa8");
    assert(transition_prefix.graphics_library_base_address == 0x12fec);
    assert(transition_prefix.graphics_library_vector == -0xc0);
    assert(transition_prefix.graphics_source_address == 0x12e12);
    assert(transition_prefix.graphics_destination_address == 0x40678);
    assert(transition_prefix.graphics_word_count == 16);
    const auto below_title_transition = eon::evaluate_deuteros_amiga_title_timer_gate(
        system_disk, load_plan, 0xea5f, 0);
    assert(below_title_transition.entry_address == 0x4059e);
    assert(below_title_transition.elapsed_counter_address == 0x40410);
    assert(below_title_transition.elapsed_threshold == 0xea60);
    assert(below_title_transition.inhibit_word_address == 0x22d34);
    assert(below_title_transition.inhibit_word_value == 0x0011);
    assert(below_title_transition.skipped_target_address == 0x405c6);
    assert(below_title_transition.transition_address == 0x4069a);
    assert(!below_title_transition.dispatches_transition);
    assert(!below_title_transition.counter_reset_after_transition_return);
    const auto inhibited_title_transition = eon::evaluate_deuteros_amiga_title_timer_gate(
        system_disk, load_plan, 0xea60, 0x0011);
    assert(!inhibited_title_transition.dispatches_transition);
    const auto title_transition = eon::evaluate_deuteros_amiga_title_timer_gate(
        system_disk, load_plan, 0xffffffffU, 0);
    assert(title_transition.dispatches_transition);
    assert(title_transition.counter_reset_after_transition_return);
    const std::array<std::uint8_t, 1> nonmatching_title_response{{0x42}};
    const auto ordinary_title_response = eon::evaluate_deuteros_amiga_title_zero_response_loop(
        system_disk, load_plan, nonmatching_title_response);
    assert(ordinary_title_response.entry_address == 0x405c6);
    assert(ordinary_title_response.state_word_address == 0x1bf36);
    assert(ordinary_title_response.initial_state_word == 0);
    assert(ordinary_title_response.final_state_word == 0);
    assert(ordinary_title_response.helper_address == 0x1f238);
    assert(ordinary_title_response.response_match_value == 0x43);
    assert(ordinary_title_response.custom_address == 0xdff180);
    assert(ordinary_title_response.custom_write_words.empty());
    assert(ordinary_title_response.return_loop_address == 0x40574);
    const std::array<std::uint8_t, 3> matching_title_responses{{0x43, 0x42, 0x43}};
    const auto repeated_title_response = eon::evaluate_deuteros_amiga_title_zero_response_loop(
        system_disk, load_plan, matching_title_responses);
    assert(repeated_title_response.final_state_word == 0x0101);
    assert((repeated_title_response.custom_write_words
        == std::vector<std::uint16_t>{0x0f00, 0x0f00}));
    const std::array<std::uint8_t, 1> immediate_post_transition_return{{0x1b}};
    const auto immediate_post_transition =
        eon::evaluate_deuteros_amiga_title_post_transition_response_loop(
            system_disk, load_plan, immediate_post_transition_return);
    assert(immediate_post_transition.entry_address == 0x4077e);
    assert(immediate_post_transition.feedback_tail_address == 0x407ba);
    assert(immediate_post_transition.control_word_address == 0x407e6);
    assert(immediate_post_transition.initial_control_word == 0);
    assert(immediate_post_transition.final_control_word == 0);
    assert(immediate_post_transition.helper_address == 0x1f238);
    assert(immediate_post_transition.return_response == 0x1b);
    assert(immediate_post_transition.loop_response == 0x20);
    assert(immediate_post_transition.increment_response == 0x2e);
    assert(immediate_post_transition.decrement_response == 0x2c);
    assert(immediate_post_transition.control_low_byte_writes.empty());
    assert(immediate_post_transition.helper_loop_address == 0x4078c);
    assert(immediate_post_transition.return_address == 0x407e4);
    const std::array<std::uint8_t, 5> post_transition_responses{{0x2e, 0x2c, 0x20, 0x42, 0x1b}};
    const auto mixed_post_transition =
        eon::evaluate_deuteros_amiga_title_post_transition_response_loop(
            system_disk, load_plan, post_transition_responses);
    assert(mixed_post_transition.final_control_word == 0);
    assert((mixed_post_transition.control_low_byte_writes == std::vector<std::uint8_t>{1, 0}));
    // The first known title exit conditionally copies a fixed, genuine byte
    // range before its existing profile-2 tail. This test models only that
    // copy: neither original helper nor the following BSR is executed.
    const auto first_title_exit_copy = eon::evaluate_deuteros_amiga_first_title_exit_copy(
        system_disk, load_plan);
    assert(first_title_exit_copy.entry_address == 0x37f56);
    assert((first_title_exit_copy.preceding_helper_addresses
        == std::array<std::uint32_t, 2>{{0x3880a, 0x204fa}}));
    assert(first_title_exit_copy.source_address == 0x13006);
    assert(first_title_exit_copy.source_disk_offset == 0x6e006);
    assert(first_title_exit_copy.destination_address == 0x66000);
    assert(first_title_exit_copy.byte_count == 0x9392);
    assert(first_title_exit_copy.source_sha256
        == "2951d0ae6dd01f84c1fb9b6cbb766c15378af1abb9a91fa5ded748d70b3e90eb");
    assert(first_title_exit_copy.copied_bytes.size() == first_title_exit_copy.byte_count);
    assert(first_title_exit_copy.copied_bytes == std::vector<std::uint8_t>(
        system_disk.bytes(0x6e006, 0x9392).begin(), system_disk.bytes(0x6e006, 0x9392).end()));
    assert(first_title_exit_copy.stop_before_subroutine_address == 0x37f7a);
    // The next straight-line tail is conditional on the unresolved BSR
    // returning. It exposes instruction destinations only, never controller
    // data or a performed bootstrap jump.
    const auto first_title_exit_return_tail =
        eon::evaluate_deuteros_amiga_first_title_exit_return_tail(system_disk, load_plan, true);
    assert(first_title_exit_return_tail.entry_address == 0x37f7e);
    assert(first_title_exit_return_tail.preceding_subroutine_address == 0x37f7a);
    assert(first_title_exit_return_tail.controller_source_address == 0x206a0);
    assert(first_title_exit_return_tail.controller_destination_address == 0x12ff8);
    assert(first_title_exit_return_tail.bootstrap_profile_address == 0x12ffc);
    assert(first_title_exit_return_tail.bootstrap_profile_value == 2);
    assert(first_title_exit_return_tail.jump_target_address == 0x12800);
    const auto first_title_exit_subroutine =
        eon::parse_deuteros_amiga_first_title_exit_subroutine_profile(system_disk, load_plan);
    assert(first_title_exit_subroutine.entry_address == 0x37f9a);
    assert(first_title_exit_subroutine.initial_d1_value == 0x12800);
    assert(first_title_exit_subroutine.initial_d7_value == 0x2c00);
    assert(first_title_exit_subroutine.initial_d0_value == 0x600);
    assert(first_title_exit_subroutine.initial_service_address == 0x208c0);
    assert(first_title_exit_subroutine.first_work_address == 0x1eefa);
    assert(first_title_exit_subroutine.first_work_word_offset == 0x1c);
    assert(first_title_exit_subroutine.first_work_word_value == 0x000a);
    assert(first_title_exit_subroutine.first_work_long_offset == 0x28);
    assert(first_title_exit_subroutine.first_work_long_value == 0x1ef48);
    assert((first_title_exit_subroutine.exec_argument_addresses
        == std::array<std::uint32_t, 5>{{0x1eefa, 0x1eefa, 0x1eed8, 0x2063e, 0x20676}}));
    assert((first_title_exit_subroutine.exec_vectors
        == std::array<std::int16_t, 5>{{-0x1ce, -0x1c2, -0x168, -0x1c2, -0x168}}));
    assert(first_title_exit_subroutine.compare_first_address == 0x20698);
    assert(first_title_exit_subroutine.compare_second_address == 0x2069c);
    assert(first_title_exit_subroutine.unequal_branch_address == 0x38014);
    assert(first_title_exit_subroutine.return_address == 0x38030);
    {
        bool rejected = false;
        try {
            static_cast<void>(eon::evaluate_deuteros_amiga_first_title_exit_return_tail(
                system_disk, load_plan, false));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    // The second exit has a separate tail after four unresolved original
    // calls. It records raw destinations only after their returns are made
    // explicit; no controller value or jump is reconstructed.
    const auto second_title_exit_return_tail =
        eon::evaluate_deuteros_amiga_second_title_exit_return_tail(system_disk, load_plan, true);
    assert(second_title_exit_return_tail.entry_address == 0x38046);
    assert((second_title_exit_return_tail.preceding_helper_addresses
        == std::array<std::uint32_t, 4>{{0x3880a, 0x204fa, 0x37efa, 0x37f9a}}));
    assert(second_title_exit_return_tail.controller_source_address == 0x206a0);
    assert(second_title_exit_return_tail.controller_destination_address == 0x12ff8);
    assert(second_title_exit_return_tail.bootstrap_profile_address == 0x12ffc);
    assert(second_title_exit_return_tail.bootstrap_profile_value == 4);
    assert(second_title_exit_return_tail.jump_target_address == 0x12800);
    {
        bool rejected = false;
        try {
            static_cast<void>(eon::evaluate_deuteros_amiga_second_title_exit_return_tail(
                system_disk, load_plan, false));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    // Third profile tail: the preceding four original calls are still an ABI
    // boundary, while the verified return continuation is distinct from the
    // profile-four exit above.
    const auto third_title_exit_return_tail =
        eon::evaluate_deuteros_amiga_third_title_exit_return_tail(system_disk, load_plan, true);
    assert(third_title_exit_return_tail.entry_address == 0x38076);
    assert((third_title_exit_return_tail.preceding_helper_addresses
        == std::array<std::uint32_t, 4>{{0x3880a, 0x204fa, 0x37efa, 0x37f9a}}));
    assert(third_title_exit_return_tail.controller_source_address == 0x206a0);
    assert(third_title_exit_return_tail.controller_destination_address == 0x12ff8);
    assert(third_title_exit_return_tail.bootstrap_profile_address == 0x12ffc);
    assert(third_title_exit_return_tail.bootstrap_profile_value == 3);
    assert(third_title_exit_return_tail.jump_target_address == 0x12800);
    {
        bool rejected = false;
        try {
            static_cast<void>(eon::evaluate_deuteros_amiga_third_title_exit_return_tail(
                system_disk, load_plan, false));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_title_stage_disk = *amiga_disk1;
        altered_title_stage_disk[0x9b69a] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_title_stage_disk));
            static_cast<void>(eon::execute_deuteros_amiga_title_transition_prefix(
                altered_disk, load_plan, 0));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_title_stage_disk = *amiga_disk1;
        altered_title_stage_disk[0x93046] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_title_stage_disk));
            static_cast<void>(eon::evaluate_deuteros_amiga_second_title_exit_return_tail(
                altered_disk, load_plan, true));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_title_stage_disk = *amiga_disk1;
        altered_title_stage_disk[0x9b5aa] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_title_stage_disk));
            static_cast<void>(eon::evaluate_deuteros_amiga_title_timer_gate(
                altered_disk, load_plan, 0, 0));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_title_stage_disk = *amiga_disk1;
        altered_title_stage_disk[0x9b5c6] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_title_stage_disk));
            static_cast<void>(eon::evaluate_deuteros_amiga_title_zero_response_loop(
                altered_disk, load_plan, nonmatching_title_response));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_title_stage_disk = *amiga_disk1;
        altered_title_stage_disk[0x9b7ba] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_title_stage_disk));
            static_cast<void>(eon::evaluate_deuteros_amiga_title_post_transition_response_loop(
                altered_disk, load_plan, immediate_post_transition_return));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_title_stage_disk = *amiga_disk1;
        altered_title_stage_disk[0x92f56] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_title_stage_disk));
            static_cast<void>(eon::evaluate_deuteros_amiga_first_title_exit_copy(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_title_stage_disk = *amiga_disk1;
        altered_title_stage_disk[0x92f7e] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_title_stage_disk));
            static_cast<void>(eon::evaluate_deuteros_amiga_first_title_exit_return_tail(
                altered_disk, load_plan, true));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_title_stage_disk = *amiga_disk1;
        altered_title_stage_disk[0x6e006] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_title_stage_disk));
            static_cast<void>(eon::evaluate_deuteros_amiga_first_title_exit_copy(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
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
    assert(title_stage.transition_first_phase_source_address == 0x12e12);
    assert(title_stage.transition_first_phase_first_work_address == 0x1ffda);
    assert(title_stage.transition_first_phase_second_work_address == 0x20056);
    assert(title_stage.transition_first_phase_work_pointer_address == 0x2008e);
    assert(title_stage.transition_poll_loop_address == 0x4071a);
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
    assert((title_stage.bootstrap_profile_table_entries
        == std::array<std::uint32_t, 6>{{0x12b1c, 0x12b30, 0x12b44,
            0x12b1c, 0x12b1c, 0x12b46}}));
    assert(title_stage.bootstrap_profile_five_address == 0x12b46);
    assert(title_stage.bootstrap_profile_five_first_call_address == 0x12932);
    assert(title_stage.bootstrap_profile_five_helper_controller_cell == 0x12822);
    assert(title_stage.bootstrap_profile_five_helper_long_offset == 0x24);
    assert(title_stage.bootstrap_profile_five_helper_long_value == 1);
    assert(title_stage.bootstrap_profile_five_helper_word_offset == 0x1c);
    assert(title_stage.bootstrap_profile_five_helper_word_value == 9);
    assert(title_stage.bootstrap_profile_five_helper_byte_offset == 0x1e);
    assert(title_stage.bootstrap_profile_five_helper_byte_value == 0);
    assert(title_stage.bootstrap_profile_five_helper_library_base == 4);
    assert(title_stage.bootstrap_profile_five_helper_library_vector == -0x1c8);
    assert(title_stage.initialization_stack_address == 0x40b62);
    assert(title_stage.initialization_exec_base_address == 4);
    assert((title_stage.initialization_exec_vectors
        == std::array<std::int16_t, 2>{{-0x96, -0x9c}}));
    assert(title_stage.initialization_exec_allocation_size == 0x7fff0);
    assert((title_stage.initialization_internal_calls
        == std::array<std::uint32_t, 11>{{0x1ed80, 0x1f172, 0x1f182, 0x1ef74,
            0x206d4, 0x206be, 0x403e6, 0x403f4, 0x204c8, 0x389e2, 0x37180}}));
    assert(title_stage.initialization_copy_source_address == 0x1f168);
    assert((title_stage.initialization_copy_destinations
        == std::array<std::uint32_t, 2>{{0x1f974, 0x410d8}}));
    assert(title_stage.initialization_custom_base_address == 0xdff000);
    assert((title_stage.initialization_custom_offsets
        == std::array<std::uint16_t, 4>{{0x40, 0x42, 0x9a, 0x96}}));
    assert((title_stage.initialization_custom_values
        == std::array<std::uint16_t, 4>{{0x7fff, 0x7fff, 0xc000, 0x87ff}}));
    assert(title_stage.initialization_mode_five_call_address == 0x36a8c);
    assert(title_stage.initialization_normal_call_address == 0x1fb9a);
    assert(title_stage.title_exit_resolved_profile == 0);
    assert(title_stage.title_exit_main_stage_entry_address == 0x21734);
    const auto post_exec_pointer_seed =
        eon::parse_deuteros_amiga_title_post_exec_pointer_seed_profile(system_disk, load_plan);
    assert(post_exec_pointer_seed.call_site_address == 0x404c2);
    assert(post_exec_pointer_seed.caller_d1_literal == 0x13000);
    assert(post_exec_pointer_seed.callee_address == 0x403e6);
    assert(post_exec_pointer_seed.literal_value == 0x1c482);
    assert(post_exec_pointer_seed.destination_address == 0x1f97c);
    assert(post_exec_pointer_seed.return_address == 0x403f2);
    assert(post_exec_pointer_seed.call_site_sha256
        == "a617235dd94a6c0b3f5fb9f9e078652ed8f1e45213e85c80b10ec165a6b7216f");
    assert(post_exec_pointer_seed.callee_sha256
        == "1e1ccdae97d5849873d3d2e785f5a8be585ffa0e104b5c550ecade6bc37a33a2");
    const auto post_exec_service_batch =
        eon::parse_deuteros_amiga_title_post_exec_service_batch_profile(system_disk, load_plan);
    assert(post_exec_service_batch.call_site_address == 0x404ce);
    assert(post_exec_service_batch.callee_address == 0x403f4);
    assert((post_exec_service_batch.direct_callee_addresses
        == std::array<std::uint32_t, 4>{{0x403c8, 0x20510, 0x1f37a, 0x40698}}));
    assert(post_exec_service_batch.return_address == 0x4040e);
    assert(post_exec_service_batch.call_site_sha256
        == "555513267ef304f2a5cec2303f8565db8e4ed9ecb2abd7bc87b73dbe5d6c0976");
    assert(post_exec_service_batch.callee_sha256
        == "5353ab8b18d63a51e12ef2f586a68d872981fa491ca13531198f18a2a38edf07");
    const auto post_exec_fourth_service =
        eon::parse_deuteros_amiga_title_post_exec_fourth_service_profile(system_disk, load_plan);
    assert(post_exec_fourth_service.caller_address == 0x40406);
    assert(post_exec_fourth_service.callee_address == 0x40698);
    assert(post_exec_fourth_service.caller_return_address == 0x4040c);
    assert(post_exec_fourth_service.batch_return_address == 0x4040e);
    assert(post_exec_fourth_service.caller_sha256
        == "b214a93028755289cb8dcefb5e4013d307dc2e8a4bb27ae2e798a7bf10298606");
    assert(post_exec_fourth_service.callee_sha256
        == "1ceeabf0c6a5a30bad12cdac0e3ab015a7188a42e6aebb556aad00bb9cd693ad");
    const auto post_exec_graphics_vector =
        eon::parse_deuteros_amiga_title_post_exec_graphics_vector_profile(system_disk, load_plan);
    assert(post_exec_graphics_vector.caller_address == 0x403f4);
    assert(post_exec_graphics_vector.entry_address == 0x403c8);
    assert(post_exec_graphics_vector.a1_literal == 0x1ed24);
    assert(post_exec_graphics_vector.a0_literal == 0x12e12);
    assert(post_exec_graphics_vector.d0_literal == 0x14);
    assert(post_exec_graphics_vector.graphics_library_base_address == 0x12fec);
    assert(post_exec_graphics_vector.graphics_library_vector == -0xc0);
    assert(post_exec_graphics_vector.return_address == 0x403e6);
    assert(post_exec_graphics_vector.caller_sha256
        == "2a90f1020af64bd1a6f7f6e7e7503bea4133a2a569bba55987f6edb23442cec3");
    assert(post_exec_graphics_vector.routine_sha256
        == "3f9cf2302a4078faddd0796fc05268386d46c4be64f294b8082ba085b9609f5f");
    assert_deuteros_amiga_post_exec_state_init(*amiga_disk1, system_disk, load_plan);
    assert_deuteros_amiga_post_exec_third_service(*amiga_disk1, system_disk, load_plan);
    const auto post_exec_tail_dispatch =
        eon::parse_deuteros_amiga_title_post_exec_tail_dispatch_profile(system_disk, load_plan);
    assert(post_exec_tail_dispatch.caller_address == 0x1f386);
    assert(post_exec_tail_dispatch.entry_address == 0x201d2);
    assert((post_exec_tail_dispatch.local_call_addresses
        == std::array<std::uint32_t, 4>{{0x200fa, 0x20118, 0x20118, 0x200dc}}));
    assert(post_exec_tail_dispatch.return_address == 0x2021e);
    assert(post_exec_tail_dispatch.routine_sha256
        == "6947fb7ffcbfaadd0ce420648741b46539f5dce188e4c26ba7fd18351852c658");
    const auto post_exec_tail_first_callee =
        eon::parse_deuteros_amiga_title_post_exec_tail_first_callee_profile(system_disk, load_plan);
    assert(post_exec_tail_first_callee.caller_address == 0x201d6);
    assert(post_exec_tail_first_callee.caller_continuation_address == 0x201da);
    assert(post_exec_tail_first_callee.entry_address == 0x200fa);
    assert(post_exec_tail_first_callee.a0_literal == 0x12e12);
    assert(post_exec_tail_first_callee.a1_literal == 0x1ffda);
    assert(post_exec_tail_first_callee.a2_pointer_cell_address == 0x2008e);
    assert(post_exec_tail_first_callee.graphics_library_base_address == 0x12fec);
    assert(post_exec_tail_first_callee.graphics_library_vector == -0x1a4);
    assert(post_exec_tail_first_callee.vector_return_address == 0x20116);
    assert(post_exec_tail_first_callee.routine_return_address == 0x20118);
    assert(post_exec_tail_first_callee.caller_sha256
        == "fd55349ce2476b466426a5addfa7eedae100cddaac5a480512c6eff31a06a450");
    assert(post_exec_tail_first_callee.routine_sha256
        == "6e36c860c280c651947ad0ea6ef868759fbc7bfac67d89af219135e4751e6e6f");
    const auto post_exec_tail_second_callee =
        eon::parse_deuteros_amiga_title_post_exec_tail_second_callee_profile(system_disk, load_plan);
    assert(post_exec_tail_second_callee.caller_address == 0x201fe);
    assert(post_exec_tail_second_callee.caller_continuation_address == 0x20202);
    assert(post_exec_tail_second_callee.entry_address == 0x20118);
    assert((post_exec_tail_second_callee.selection_cells
        == std::array<std::uint32_t, 6>{{0x1ffc8, 0x1ffca, 0x1ffcc,
            0x1ffce, 0x1ffd0, 0x1ffd2}}));
    assert(post_exec_tail_second_callee.a0_literal == 0x12e12);
    assert(post_exec_tail_second_callee.a1_literal == 0x1ffda);
    assert(post_exec_tail_second_callee.d0_addend == 0x10);
    assert(post_exec_tail_second_callee.d1_adjustment_opcode == 0x5d41);
    assert(post_exec_tail_second_callee.d1_shift_opcode == 0xe248);
    assert(post_exec_tail_second_callee.graphics_library_base_address == 0x12fec);
    assert(post_exec_tail_second_callee.graphics_library_vector == -0x1aa);
    assert(post_exec_tail_second_callee.vector_return_address == 0x201ba);
    assert(post_exec_tail_second_callee.routine_return_address == 0x201c0);
    assert(post_exec_tail_second_callee.caller_sha256
        == "8919a0658d9b7a79bca49d3ca3f38227e3ee6a043491ebac0dbb395504b33fd9");
    assert(post_exec_tail_second_callee.routine_sha256
        == "9b16e7cdc97495a1b52656d49c7a3612e7e1617ce88996e2c5e7138e3f183ec3");
    const auto post_exec_tail_third_callee =
        eon::parse_deuteros_amiga_title_post_exec_tail_third_callee_profile(system_disk, load_plan);
    assert(post_exec_tail_third_callee.caller_address == 0x20212);
    assert(post_exec_tail_third_callee.caller_continuation_address == 0x20216);
    assert(post_exec_tail_third_callee.entry_address == 0x20118);
    assert(post_exec_tail_third_callee.routine_return_address == 0x201c0);
    assert(post_exec_tail_third_callee.caller_sha256
        == "a760d59c7213517e7d3427b30915f9c586be5448e40a0a3980f9dded55f9f994");
    assert(post_exec_tail_third_callee.routine_sha256
        == "9b16e7cdc97495a1b52656d49c7a3612e7e1617ce88996e2c5e7138e3f183ec3");
    const auto post_exec_tail_fourth_callee =
        eon::parse_deuteros_amiga_title_post_exec_tail_fourth_callee_profile(system_disk, load_plan);
    assert(post_exec_tail_fourth_callee.caller_address == 0x20216);
    assert(post_exec_tail_fourth_callee.caller_continuation_address == 0x2021a);
    assert(post_exec_tail_fourth_callee.entry_address == 0x200dc);
    assert(post_exec_tail_fourth_callee.a0_literal == 0x12e12);
    assert(post_exec_tail_fourth_callee.a1_literal == 0x1ffda);
    assert(post_exec_tail_fourth_callee.a2_pointer_cell_address == 0x2008e);
    assert(post_exec_tail_fourth_callee.graphics_library_base_address == 0x12fec);
    assert(post_exec_tail_fourth_callee.graphics_library_vector == -0x1a4);
    assert(post_exec_tail_fourth_callee.vector_return_address == 0x200f8);
    assert(post_exec_tail_fourth_callee.routine_return_address == 0x200fa);
    assert(post_exec_tail_fourth_callee.caller_sha256
        == "6b8c80452bd43c82d8ce91fa551b3067dfc33bb85e553d555aaec65ea6a8ce26");
    assert(post_exec_tail_fourth_callee.routine_sha256
        == "6e36c860c280c651947ad0ea6ef868759fbc7bfac67d89af219135e4751e6e6f");
    const auto post_exec_tail_return =
        eon::parse_deuteros_amiga_title_post_exec_tail_return_profile(system_disk, load_plan);
    assert(post_exec_tail_return.continuation_address == 0x404d4);
    assert(post_exec_tail_return.source_table_address == 0x12ff4);
    assert((post_exec_tail_return.destination_addresses
        == std::array<std::uint32_t, 2>{{0x37ef2, 0x37ef6}}));
    assert(post_exec_tail_return.local_service_call_address == 0x404ea);
    assert(post_exec_tail_return.local_service_address == 0x204c8);
    assert(post_exec_tail_return.service_a1_literal == 0x204aa);
    assert((post_exec_tail_return.service_a1_offsets
        == std::array<std::uint16_t, 4>{{0x0008, 0x0009, 0x000e, 0x0012}}));
    assert((post_exec_tail_return.service_long_literals
        == std::array<std::uint32_t, 2>{{0x204c0, 0x202ca}}));
    assert(post_exec_tail_return.exec_base_address == 0x0004);
    assert(post_exec_tail_return.exec_vector == -0x0a8);
    assert(post_exec_tail_return.vector_return_address == 0x204f8);
    assert(post_exec_tail_return.routine_return_address == 0x204fa);
    assert(post_exec_tail_return.continuation_sha256
        == "32a750150f115f5c012e99811313916078a8657c6100b50e92acadca0708965d");
    assert(post_exec_tail_return.routine_sha256
        == "76f4163c15e6761168f1d267e3feae94f0430975efa75b1c3576d7b88947e596");
    const auto post_exec_tail_return_continuation =
        eon::parse_deuteros_amiga_title_post_exec_tail_return_continuation_profile(
            system_disk, load_plan);
    assert(post_exec_tail_return_continuation.continuation_address == 0x404f0);
    assert(post_exec_tail_return_continuation.preceding_local_service_address == 0x204c8);
    assert(post_exec_tail_return_continuation.preceding_exec_vector_return_address == 0x204f8);
    assert(post_exec_tail_return_continuation.preceding_local_return_address == 0x204fa);
    assert((post_exec_tail_return_continuation.direct_call_addresses
        == std::array<std::uint32_t, 13>{{0x389e2, 0x1fb9a, 0x38912, 0x2022a,
            0x41bb4, 0x41bb4, 0x20e18, 0x20ba8, 0x37180, 0x36a8c,
            0x1fb9a, 0x222c0, 0x23e4e}}));
    assert(post_exec_tail_return_continuation.indirect_call_pointer_literal == 0x20cfe);
    assert(post_exec_tail_return_continuation.indirect_call_address == 0x4053e);
    assert(post_exec_tail_return_continuation.mode_cell_address == 0x4040e);
    assert(post_exec_tail_return_continuation.mode_value == 5);
    assert((post_exec_tail_return_continuation.mode_call_targets
        == std::array<std::uint32_t, 2>{{0x36a8c, 0x1fb9a}}));
    assert(post_exec_tail_return_continuation.timer_counter_address == 0x40410);
    assert(post_exec_tail_return_continuation.timer_limit == 0xea60);
    assert(post_exec_tail_return_continuation.timer_inhibit_cell_address == 0x22d34);
    assert(post_exec_tail_return_continuation.timer_inhibit_value == 0x11);
    assert(post_exec_tail_return_continuation.timer_local_call_target == 0x4069a);
    assert(post_exec_tail_return_continuation.terminal_flag_cell_address == 0x1bf36);
    assert(post_exec_tail_return_continuation.stop_before_address == 0x40618);
    assert(post_exec_tail_return_continuation.sha256
        == "10a96a2c80f83b32530ed9355cb2988bcac233c49f66d93484b31d0c0e3667c6");
    const auto post_exec_tail_flag_gate =
        eon::parse_deuteros_amiga_title_post_exec_tail_flag_gate_profile(system_disk, load_plan);
    assert(post_exec_tail_flag_gate.entry_address == 0x40616);
    assert(post_exec_tail_flag_gate.preceding_profile_stop_address == 0x40618);
    assert((post_exec_tail_flag_gate.source_word_addresses
        == std::array<std::uint32_t, 2>{{0x1ffce, 0x1ffd4}}));
    assert(post_exec_tail_flag_gate.first_compare_value == 0x00b4);
    assert(post_exec_tail_flag_gate.first_branch_target == 0x4063a);
    assert(post_exec_tail_flag_gate.second_branch_target == 0x4063a);
    assert(post_exec_tail_flag_gate.absolute_jump_target == 0x37f56);
    assert((post_exec_tail_flag_gate.direct_call_targets
        == std::array<std::uint32_t, 3>{{0x1f3f8, 0x1f238, 0x1f238}}));
    assert(post_exec_tail_flag_gate.response_compare_value == 0x0043);
    assert(post_exec_tail_flag_gate.mode_cell_address == 0x1bf36);
    assert(post_exec_tail_flag_gate.mode_compare_value == 0x0101);
    assert((post_exec_tail_flag_gate.word_literals
        == std::array<std::uint16_t, 2>{{0x00f0, 0x0f00}}));
    assert(post_exec_tail_flag_gate.custom_chip_base_address == 0xdff000);
    assert(post_exec_tail_flag_gate.custom_chip_word_offset == 0x0180);
    assert(post_exec_tail_flag_gate.local_loop_address == 0x40658);
    assert(post_exec_tail_flag_gate.exit_branch_target == 0x40576);
    assert(post_exec_tail_flag_gate.stop_after_address == 0x40674);
    assert(post_exec_tail_flag_gate.sha256
        == "fcf7c15552302b6b902352380a5b5d454eba190be2a7e89af9701822eac1f80e");
    const auto post_exec_tail_flag_gate_first_callee =
        eon::parse_deuteros_amiga_title_post_exec_tail_flag_gate_first_callee_profile(
            system_disk, load_plan);
    assert(post_exec_tail_flag_gate_first_callee.caller_address == 0x40632);
    assert(post_exec_tail_flag_gate_first_callee.caller_continuation_address == 0x40638);
    assert(post_exec_tail_flag_gate_first_callee.entry_address == 0x1f3f8);
    assert(post_exec_tail_flag_gate_first_callee.tested_byte_address == 0x1ee16);
    assert(post_exec_tail_flag_gate_first_callee.zero_return_address == 0x1f400);
    assert(post_exec_tail_flag_gate_first_callee.first_loop_word_address == 0x1ffd4);
    assert(post_exec_tail_flag_gate_first_callee.first_loop_mask == 3);
    assert(post_exec_tail_flag_gate_first_callee.first_loop_branch_address == 0x1f40c);
    assert(post_exec_tail_flag_gate_first_callee.first_loop_branch_target == 0x1f402);
    assert(post_exec_tail_flag_gate_first_callee.second_loop_word_address == 0x1ffd4);
    assert(post_exec_tail_flag_gate_first_callee.second_loop_shift_count == 1);
    assert(post_exec_tail_flag_gate_first_callee.second_loop_branch_address == 0x1f416);
    assert(post_exec_tail_flag_gate_first_callee.second_loop_branch_target == 0x1f40e);
    assert(post_exec_tail_flag_gate_first_callee.terminal_return_address == 0x1f41a);
    assert(post_exec_tail_flag_gate_first_callee.caller_sha256
        == "c3998d07f8e89408b9332ae19f449256087b1eb8843256751c03e52700cbbec4");
    assert(post_exec_tail_flag_gate_first_callee.routine_sha256
        == "101f4026b51a3c0bef3758f4244fffd3fe12c93d76e37b44d0728295b5e27aa6");
    const auto post_exec_tail_flag_gate_copy_callee =
        eon::parse_deuteros_amiga_title_post_exec_tail_flag_gate_copy_callee_profile(
            system_disk, load_plan);
    assert((post_exec_tail_flag_gate_copy_callee.caller_addresses
        == std::array<std::uint32_t, 2>{{0x40638, 0x40662}}));
    assert((post_exec_tail_flag_gate_copy_callee.caller_continuation_addresses
        == std::array<std::uint32_t, 2>{{0x4063e, 0x40668}}));
    assert(post_exec_tail_flag_gate_copy_callee.entry_address == 0x1f238);
    assert(post_exec_tail_flag_gate_copy_callee.gate_word_address == 0x1eed6);
    assert(post_exec_tail_flag_gate_copy_callee.zero_branch_target == 0x1f252);
    assert(post_exec_tail_flag_gate_copy_callee.source_address == 0x1eec0);
    assert(post_exec_tail_flag_gate_copy_callee.destination_address == 0x1eec0);
    assert(post_exec_tail_flag_gate_copy_callee.transferred_byte_count == 1);
    assert(post_exec_tail_flag_gate_copy_callee.delay_loop_counter == 0x13);
    assert(post_exec_tail_flag_gate_copy_callee.copy_loop_address == 0x1f24c);
    assert(post_exec_tail_flag_gate_copy_callee.copy_loop_branch_address == 0x1f24e);
    assert(post_exec_tail_flag_gate_copy_callee.copy_loop_branch_target == 0x1f24e);
    assert(post_exec_tail_flag_gate_copy_callee.increment_address == 0x1f252);
    assert(post_exec_tail_flag_gate_copy_callee.terminal_return_address == 0x1f25a);
    assert(post_exec_tail_flag_gate_copy_callee.caller_sha256
        == "88e2b3531aa5cb582d1ed1a672f9a524c89cbdf572c7a7d77c8cc7f4e6db695d");
    assert(post_exec_tail_flag_gate_copy_callee.routine_sha256
        == "9c0ffcff9d88feedca2b8079b14f5a32fb51dac94bee60e1c477c746e7c6c4f0");
    {
        auto altered_tail_flag_gate_disk = *amiga_disk1;
        altered_tail_flag_gate_disk[0x9b616] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_tail_flag_gate_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_post_exec_tail_flag_gate_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    for (const auto disk_offset : {0x9b632U, 0x7a3f8U}) {
        auto altered_tail_flag_gate_first_callee_disk = *amiga_disk1;
        altered_tail_flag_gate_first_callee_disk[disk_offset] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_tail_flag_gate_first_callee_disk));
            static_cast<void>(
                eon::parse_deuteros_amiga_title_post_exec_tail_flag_gate_first_callee_profile(
                    altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    for (const auto disk_offset : {0x9b638U, 0x9b662U, 0x7a238U}) {
        auto altered_tail_flag_gate_copy_callee_disk = *amiga_disk1;
        altered_tail_flag_gate_copy_callee_disk[disk_offset] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_tail_flag_gate_copy_callee_disk));
            static_cast<void>(
                eon::parse_deuteros_amiga_title_post_exec_tail_flag_gate_copy_callee_profile(
                    altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_tail_return_continuation_disk = *amiga_disk1;
        altered_tail_return_continuation_disk[0x9b4f0] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_tail_return_continuation_disk));
            static_cast<void>(
                eon::parse_deuteros_amiga_title_post_exec_tail_return_continuation_profile(
                    altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_tail_return_disk = *amiga_disk1;
        altered_tail_return_disk[0x9b4d4] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_tail_return_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_post_exec_tail_return_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_tail_fourth_callee_disk = *amiga_disk1;
        altered_tail_fourth_callee_disk[0x7b216] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_tail_fourth_callee_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_post_exec_tail_fourth_callee_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_tail_third_callee_disk = *amiga_disk1;
        altered_tail_third_callee_disk[0x7b212] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_tail_third_callee_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_post_exec_tail_third_callee_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_tail_second_callee_disk = *amiga_disk1;
        altered_tail_second_callee_disk[0x7b118] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_tail_second_callee_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_post_exec_tail_second_callee_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_tail_first_callee_disk = *amiga_disk1;
        altered_tail_first_callee_disk[0x7b0fa] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_tail_first_callee_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_post_exec_tail_first_callee_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_tail_dispatch_disk = *amiga_disk1;
        altered_tail_dispatch_disk[0x7b1d2] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_tail_dispatch_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_post_exec_tail_dispatch_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_pointer_seed_disk = *amiga_disk1;
        altered_pointer_seed_disk[0x9b4c2] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_pointer_seed_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_post_exec_pointer_seed_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_pointer_seed_disk = *amiga_disk1;
        altered_pointer_seed_disk[0x9b3e6] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_pointer_seed_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_post_exec_pointer_seed_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_pointer_seed_disk = *amiga_disk1;
        altered_pointer_seed_disk[0x9b3f2] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_pointer_seed_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_post_exec_pointer_seed_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_service_batch_disk = *amiga_disk1;
        altered_service_batch_disk[0x9b4ce] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_service_batch_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_post_exec_service_batch_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_service_batch_disk = *amiga_disk1;
        altered_service_batch_disk[0x9b3f4] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_service_batch_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_post_exec_service_batch_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_fourth_service_disk = *amiga_disk1;
        altered_fourth_service_disk[0x9b406] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_fourth_service_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_post_exec_fourth_service_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_fourth_service_disk = *amiga_disk1;
        altered_fourth_service_disk[0x9b698] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_fourth_service_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_post_exec_fourth_service_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_graphics_vector_disk = *amiga_disk1;
        altered_graphics_vector_disk[0x9b3f4] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_graphics_vector_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_post_exec_graphics_vector_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_graphics_vector_disk = *amiga_disk1;
        altered_graphics_vector_disk[0x9b3c8] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_graphics_vector_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_post_exec_graphics_vector_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    const auto title_graphics_setup = eon::parse_deuteros_amiga_title_graphics_setup_profile(
        system_disk, load_plan);
    assert(title_graphics_setup.entry_address == 0x1ed80);
    assert(title_graphics_setup.library_name_address == 0x1ed02);
    assert(title_graphics_setup.library_name == "graphics.library");
    assert(title_graphics_setup.exec_base_address == 4);
    assert(title_graphics_setup.exec_vector == -0x228);
    assert(title_graphics_setup.zero_result_loop_address == 0x1edf6);
    assert(title_graphics_setup.nonzero_result_store_address == 0x1ed96);
    assert(title_graphics_setup.nonzero_result_destination_address == 0x12fec);
    assert(title_graphics_setup.first_return_address == 0x1eda2);
    assert(title_graphics_setup.following_entry_address == 0x1f172);
    assert(title_graphics_setup.palette_copy_entry_address == 0x1eda6);
    assert(title_graphics_setup.external_display_base_source_address == 0x12ff4);
    assert((title_graphics_setup.external_display_base_destinations
        == std::array<std::uint32_t, 2>{{0x1f168, 0x1f164}}));
    assert(title_graphics_setup.palette_source_address == 0x1ed24);
    assert(title_graphics_setup.palette_destination_address == 0x12ecc);
    assert(title_graphics_setup.palette_words.front() == 0x0000);
    assert(title_graphics_setup.palette_words[1] == 0x09a7);
    assert(title_graphics_setup.palette_words.back() == 0x0cc0);
    assert(title_graphics_setup.derived_pointer_source_address == 0x1f168);
    assert(title_graphics_setup.derived_pointer_destination_address == 0x1f16e);
    assert(title_graphics_setup.derived_pointer_addend == 0x7d00);
    assert(title_graphics_setup.following_return_address == 0x1f182);
    assert(title_graphics_setup.first_callee_sha256
        == "42c96aa502e36711ed274b9ddf4d2d1de53abfebb4ebdf88fa99346d2b03e30b");
    assert(title_graphics_setup.following_callee_sha256
        == "d6b37bc6431a1fe9145ae9403a5165028ccfd856a6529d1752f824b166807223");
    assert(title_graphics_setup.palette_sha256
        == "5903a1c83619d7667c04ac1f3c923dfaa3a1ce0d090d6fd95109616a9b506a55");
    // This caller-connected clear loop is locally complete, but its target
    // remains the graphics setup's externally initialized pointer cell.
    const auto title_display_clear = eon::parse_deuteros_amiga_title_display_clear_profile(
        system_disk, load_plan);
    assert(title_display_clear.entry_address == 0x1f182);
    assert(title_display_clear.destination_pointer_address == 0x1f168);
    assert(title_display_clear.initial_loop_counter == 0x1f3f);
    assert(title_display_clear.iteration_count == 0x1f40);
    assert(title_display_clear.value == 0);
    assert(title_display_clear.write_width_bytes == 4);
    assert(title_display_clear.return_address == 0x1f194);
    assert(title_display_clear.sha256
        == "9b02afb723e201cacb93d18d87613dee0f56369707867989209a41d9430ec5f3");
    // The contiguous byte combiner remains static: its input registers and
    // its graphics-setup pointer cell are intentionally not materialized.
    const auto title_byte_combine =
        eon::parse_deuteros_amiga_title_four_pass_byte_combine_profile(system_disk, load_plan);
    assert(title_byte_combine.entry_address == 0x1f196);
    assert(title_byte_combine.first_coordinate_minimum == 0x40);
    assert(title_byte_combine.first_coordinate_limit == 0x138);
    assert(title_byte_combine.second_coordinate_minimum == 0x24);
    assert(title_byte_combine.second_coordinate_limit == 0x70);
    assert(title_byte_combine.first_coordinate_origin == 0x40);
    assert(title_byte_combine.second_coordinate_origin == 0x24);
    assert(title_byte_combine.second_coordinate_stride == 0x28);
    assert(title_byte_combine.source_table_address == 0x1f8ec);
    assert(title_byte_combine.source_table_selector_mask == 0x000f);
    assert(title_byte_combine.source_table_selector_shift == 3);
    assert(title_byte_combine.destination_pointer_address == 0x1f168);
    assert(title_byte_combine.pass_stride == 0x1f40);
    assert(title_byte_combine.pass_count == 4);
    assert(title_byte_combine.return_address == 0x1f22e);
    assert(title_byte_combine.sha256
        == "31fc346d9d2647001899a2e939482aa97bd8bc94221ae81384787997928bb42b");
    // The known title response paths call this local wait/shift helper, but
    // its pending word and byte region remain original runtime state.
    const auto title_response_queue =
        eon::parse_deuteros_amiga_title_response_queue_profile(system_disk, load_plan);
    assert(title_response_queue.entry_address == 0x1f230);
    assert(title_response_queue.pending_word_address == 0x1eed6);
    assert(title_response_queue.wait_branch_address == 0x1f230);
    assert(title_response_queue.empty_branch_address == 0x1f258);
    assert(title_response_queue.return_address == 0x1f258);
    assert(title_response_queue.byte_region_address == 0x1eec0);
    assert(title_response_queue.shift_initial_loop_counter == 0x13);
    assert(title_response_queue.shift_byte_count == 0x14);
    assert(title_response_queue.sha256
        == "ed2794b7bb16f17ca9690b367c9465c75ff52838356bf6b46d9744cb16da1054");
    const auto title_callback =
        eon::parse_deuteros_amiga_title_callback_registration_profile(system_disk, load_plan);
    assert(title_callback.registration_entry_address == 0x1ef74);
    assert(title_callback.descriptor_address == 0x1ef48);
    assert(title_callback.descriptor_callback_offset == 0x12);
    assert(title_callback.callback_address == 0x1f056);
    assert(title_callback.request_address == 0x1eefa);
    assert(title_callback.request_command_offset == 0x1c);
    assert(title_callback.request_descriptor_offset == 0x28);
    assert(title_callback.request_command_value == 9);
    assert(title_callback.exec_base_address == 4);
    assert(title_callback.exec_vector == -0x1ce);
    assert(title_callback.registration_return_address == 0x1f052);
    assert(title_callback.callback_a0_event_offset == 4);
    assert((title_callback.callback_early_return_values == std::array<std::uint8_t, 3>{{6, 15, 16}}));
    assert(title_callback.callback_event_mirror_address == 0x1ef2e);
    assert(title_callback.callback_second_event_value == 2);
    assert(title_callback.callback_second_event_gate_address == 0x1ee16);
    assert(title_callback.callback_second_event_a0_word_offset == 6);
    assert(title_callback.callback_second_event_special_word == 0x00ff);
    assert((title_callback.callback_second_event_copy_source_offsets
        == std::array<std::uint16_t, 2>{{0x000a, 0x000c}}));
    assert((title_callback.callback_second_event_copy_destinations
        == std::array<std::uint32_t, 2>{{0x1ee10, 0x1ee12}}));
    assert(title_callback.callback_second_event_service_address == 0x20118);
    assert(title_callback.callback_second_event_mask == 0x007f);
    assert((title_callback.callback_second_event_accepted_values
        == std::array<std::uint8_t, 2>{{0x68, 0x69}}));
    assert(title_callback.callback_second_event_transform_source_offset == 8);
    assert(title_callback.callback_second_event_transform_destination_address == 0x1ffd4);
    assert(title_callback.callback_producer_value == 1);
    assert(title_callback.callback_a0_word_offset == 6);
    assert(title_callback.callback_producer_selector_offset == 8);
    assert(title_callback.callback_producer_selector_mask == 7);
    assert(title_callback.callback_producer_second_half_adjustment == 0x50);
    assert(title_callback.callback_pending_limit == 0x14);
    assert(title_callback.callback_pending_word_address == 0x1eed6);
    assert(title_callback.callback_source_table_address == 0x1ee20);
    assert(title_callback.callback_source_table_byte_count == 0xa0);
    assert(title_callback.callback_source_table_sha256
        == "2f00ffdf05ab28379e97e91e98fa764e45769d7ea55363846543becf7552e265");
    assert(title_callback.callback_destination_address == 0x1eec0);
    assert(title_callback.registration_sha256
        == "f571a8e5e48c29fe3d6f493e503e2a3a0b3328ac4cafb425808eff48804c4f27");
    assert(title_callback.callback_sha256
        == "ff4b055b2d5128465c891debcad00ff4e53cbf661de47b9ee3d6278f33d5e5f8");
    {
        auto altered_title_stage_disk = *amiga_disk1;
        altered_title_stage_disk[0x79d80] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_title_stage_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_graphics_setup_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_title_stage_disk = *amiga_disk1;
        altered_title_stage_disk[0x7a182] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_title_stage_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_display_clear_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_title_stage_disk = *amiga_disk1;
        altered_title_stage_disk[0x7a196] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_title_stage_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_four_pass_byte_combine_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_title_stage_disk = *amiga_disk1;
        altered_title_stage_disk[0x7a230] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_title_stage_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_response_queue_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_title_stage_disk = *amiga_disk1;
        altered_title_stage_disk[0x7a056] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_title_stage_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_callback_registration_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        // A malformed copy is used only to prove that the parser fails closed;
        // no replacement game data is constructed or executed.
        auto altered_title_stage_disk = *amiga_disk1;
        altered_title_stage_disk[0x79e20] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_title_stage_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_callback_registration_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
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
    const auto channel_request_continuation = eon::parse_deuteros_amiga_channel_request_continuation(
        system_disk, load_plan);
    assert(channel_request_continuation.entry_address == 0x21892);
    assert(channel_request_continuation.first_longword_address == 0x2126a);
    assert(channel_request_continuation.first_zero_branch_address == 0x21898);
    assert(channel_request_continuation.first_zero_branch_target == 0x218a2);
    assert((channel_request_continuation.local_call_addresses
        == std::array<std::uint32_t, 2>{0x2189a, 0x2189e}));
    assert((channel_request_continuation.local_call_targets
        == std::array<std::uint32_t, 2>{0x2229c, 0x224a2}));
    assert(channel_request_continuation.following_call_address == 0x218a2);
    assert(channel_request_continuation.following_call_target == 0x22a5a);
    assert(channel_request_continuation.repeated_longword_address == 0x2079e);
    assert(channel_request_continuation.equal_branch_address == 0x218b6);
    assert(channel_request_continuation.equal_branch_target == 0x218ae);
    assert(channel_request_continuation.later_call_address == 0x218b8);
    assert(channel_request_continuation.later_call_target == 0x208ba);
    assert(channel_request_continuation.input_test_address == 0x218be);
    assert(channel_request_continuation.input_test_bit == 6);
    assert(channel_request_continuation.input_zero_branch_address == 0x218c6);
    assert(channel_request_continuation.input_zero_branch_target == 0x218be);
    assert(channel_request_continuation.final_branch_address == 0x218c8);
    assert(channel_request_continuation.final_branch_target == 0x217f6);
    assert(channel_request_continuation.raw_sha256
        == "120fba90e0b4fa9e96d8a6cf95fbac512d67d7daa42c3776ce0d3066b3f02ee9");
    auto altered_channel_request_system_adf = *amiga_disk1;
    altered_channel_request_system_adf[0x7092] ^= 0x01;
    bool rejected_altered_channel_request_continuation = false;
    try {
        const eon::AmigaAdf altered_disk(altered_channel_request_system_adf);
        const auto altered_plan = eon::parse_deuteros_amiga_load_plan(altered_disk);
        static_cast<void>(eon::parse_deuteros_amiga_channel_request_continuation(
            altered_disk, altered_plan));
    } catch (const std::runtime_error&) {
        rejected_altered_channel_request_continuation = true;
    }
    assert(rejected_altered_channel_request_continuation);
    const auto channel_request_first_callee = eon::parse_deuteros_amiga_channel_request_first_callee(
        system_disk, load_plan, channel_request_continuation);
    assert(channel_request_first_callee.entry_address == 0x2229c);
    assert(channel_request_first_callee.first_word_address == 0x2229a);
    assert(channel_request_first_callee.first_word_value == 0x0100);
    assert(channel_request_first_callee.cleared_byte_address == 0x207ea);
    assert(channel_request_first_callee.input_test_address == 0x222ac);
    assert(channel_request_first_callee.input_test_bit == 5);
    assert(channel_request_first_callee.input_zero_branch_address == 0x222b4);
    assert(channel_request_first_callee.input_zero_branch_target == 0x2232c);
    assert(channel_request_first_callee.loop_initial_counter == 0x000f);
    assert(channel_request_first_callee.loop_branch_address == 0x222e0);
    assert(channel_request_first_callee.loop_branch_target == 0x222be);
    assert(channel_request_first_callee.vector_base_address == 0x12fec);
    assert((channel_request_first_callee.vector_call_addresses
        == std::array<std::uint32_t, 2>{0x222fc, 0x22312}));
    assert((channel_request_first_callee.vector_a0_addresses
        == std::array<std::uint32_t, 2>{0x12e12, 0x12f12}));
    assert(channel_request_first_callee.final_word_address == 0x2229a);
    assert(channel_request_first_callee.final_subtract_value == 8);
    assert(channel_request_first_callee.final_nonzero_branch_address == 0x2231c);
    assert(channel_request_first_callee.final_nonzero_branch_target == 0x2232c);
    assert((channel_request_first_callee.final_service_call_addresses
        == std::array<std::uint32_t, 2>{0x2231e, 0x22324}));
    assert(channel_request_first_callee.final_service_target == 0x21698);
    assert(channel_request_first_callee.return_address == 0x2232a);
    assert(channel_request_first_callee.raw_sha256
        == "d1a162af50f92b60d03b1da4ab186a547e46d145b0599cfbbeff7fb5af324ac1");
    auto altered_channel_request_callee_system_adf = *amiga_disk1;
    altered_channel_request_callee_system_adf[0x7a9c] ^= 0x01;
    bool rejected_altered_channel_request_callee = false;
    try {
        const eon::AmigaAdf altered_disk(altered_channel_request_callee_system_adf);
        const auto altered_plan = eon::parse_deuteros_amiga_load_plan(altered_disk);
        const auto altered_continuation = eon::parse_deuteros_amiga_channel_request_continuation(
            altered_disk, altered_plan);
        static_cast<void>(eon::parse_deuteros_amiga_channel_request_first_callee(
            altered_disk, altered_plan, altered_continuation));
    } catch (const std::runtime_error&) {
        rejected_altered_channel_request_callee = true;
    }
    assert(rejected_altered_channel_request_callee);
    const auto channel_request_second_callee = eon::parse_deuteros_amiga_channel_request_second_callee(
        system_disk, load_plan, channel_request_continuation);
    assert(channel_request_second_callee.entry_address == 0x224a2);
    assert(channel_request_second_callee.copied_longword_source_address == 0x224e6);
    assert(channel_request_second_callee.copied_longword_destination_address == 0x006c);
    assert((channel_request_second_callee.cleared_word_addresses
        == std::array<std::uint32_t, 4>{0xdff0a8, 0xdff0b8, 0xdff0c8, 0xdff0d8}));
    assert(channel_request_second_callee.final_word_value == 0x000f);
    assert(channel_request_second_callee.final_word_address == 0xdff096);
    assert(channel_request_second_callee.return_address == 0x224ca);
    assert(channel_request_second_callee.raw_sha256
        == "d4e9a1ee0065537a627cdd9ee8827f11d5fa28e0f860aacb21bbdc7e11784bd1");
    auto altered_channel_request_second_callee_system_adf = *amiga_disk1;
    altered_channel_request_second_callee_system_adf[0x7ca2] ^= 0x01;
    bool rejected_altered_channel_request_second_callee = false;
    try {
        const eon::AmigaAdf altered_disk(altered_channel_request_second_callee_system_adf);
        const auto altered_plan = eon::parse_deuteros_amiga_load_plan(altered_disk);
        const auto altered_continuation = eon::parse_deuteros_amiga_channel_request_continuation(
            altered_disk, altered_plan);
        static_cast<void>(eon::parse_deuteros_amiga_channel_request_second_callee(
            altered_disk, altered_plan, altered_continuation));
    } catch (const std::runtime_error&) {
        rejected_altered_channel_request_second_callee = true;
    }
    assert(rejected_altered_channel_request_second_callee);
    const auto channel_request_following_service =
        eon::parse_deuteros_amiga_channel_request_following_service(
            system_disk, load_plan, channel_request_continuation);
    assert(channel_request_following_service.entry_address == 0x22a5a);
    assert(channel_request_following_service.initialized_byte_address == 0x22a30);
    assert(channel_request_following_service.initialized_byte_value == 0);
    assert(channel_request_following_service.initial_mask_value == 0x000f);
    assert(channel_request_following_service.execution_entry_address == 0x22ab8);
    assert(channel_request_following_service.embedded_table_address == 0x22a6a);
    assert(channel_request_following_service.descriptor_base_address == 0x22a6e);
    assert(channel_request_following_service.descriptor_stride == 0x000e);
    assert(channel_request_following_service.source_record_address == 0x22aaa);
    assert(channel_request_following_service.source_payload_addend == 0x32a24);
    assert(channel_request_following_service.flag_cell_address == 0x22a6c);
    assert((channel_request_following_service.flag_write_values
        == std::array<std::uint8_t, 4>{1, 2, 4, 8}));
    assert(channel_request_following_service.return_address == 0x22b88);
    assert(channel_request_following_service.raw_sha256
        == "d5fdbdacd004d2cf377ea0dbaefb9d8b308ba23b568cfb3785456622bde49d19");
    auto altered_channel_request_following_service_system_adf = *amiga_disk1;
    altered_channel_request_following_service_system_adf[0x825a] ^= 0x01;
    bool rejected_altered_channel_request_following_service = false;
    try {
        const eon::AmigaAdf altered_disk(altered_channel_request_following_service_system_adf);
        const auto altered_plan = eon::parse_deuteros_amiga_load_plan(altered_disk);
        const auto altered_continuation = eon::parse_deuteros_amiga_channel_request_continuation(
            altered_disk, altered_plan);
        static_cast<void>(eon::parse_deuteros_amiga_channel_request_following_service(
            altered_disk, altered_plan, altered_continuation));
    } catch (const std::runtime_error&) {
        rejected_altered_channel_request_following_service = true;
    }
    assert(rejected_altered_channel_request_following_service);
    const auto channel_request_adjacent_entry = eon::parse_deuteros_amiga_channel_request_adjacent_entry(
        system_disk, load_plan, channel_request_following_service);
    assert(channel_request_adjacent_entry.entry_address == 0x22b8a);
    assert(channel_request_adjacent_entry.tested_byte_address == 0x22a30);
    assert(channel_request_adjacent_entry.zero_branch_address == 0x22b90);
    assert(channel_request_adjacent_entry.zero_branch_target == 0x22b94);
    assert(channel_request_adjacent_entry.early_return_address == 0x22b92);
    assert(channel_request_adjacent_entry.multiply_immediate == 0x000e);
    assert(channel_request_adjacent_entry.pointer_cell_address == 0x22aa6);
    assert(channel_request_adjacent_entry.negative_branch_address == 0x22ba2);
    assert(channel_request_adjacent_entry.negative_branch_target == 0x22ba8);
    assert(channel_request_adjacent_entry.descriptor_base_address == 0x22a6e);
    assert(channel_request_adjacent_entry.copy_field_offset == 0x000a);
    assert(channel_request_adjacent_entry.descriptor_stride == 0x000e);
    assert(channel_request_adjacent_entry.final_return_address == 0x22be8);
    assert(channel_request_adjacent_entry.raw_sha256
        == "10ed8be15c107dbb56ca98eb8d17ffd2bce3910dd169d67ba058447c9031b1ff");
    auto altered_channel_request_adjacent_entry_system_adf = *amiga_disk1;
    altered_channel_request_adjacent_entry_system_adf[0x838a] ^= 0x01;
    bool rejected_altered_channel_request_adjacent_entry = false;
    try {
        const eon::AmigaAdf altered_disk(altered_channel_request_adjacent_entry_system_adf);
        const auto altered_plan = eon::parse_deuteros_amiga_load_plan(altered_disk);
        const auto altered_continuation = eon::parse_deuteros_amiga_channel_request_continuation(
            altered_disk, altered_plan);
        const auto altered_service = eon::parse_deuteros_amiga_channel_request_following_service(
            altered_disk, altered_plan, altered_continuation);
        static_cast<void>(eon::parse_deuteros_amiga_channel_request_adjacent_entry(
            altered_disk, altered_plan, altered_service));
    } catch (const std::runtime_error&) { rejected_altered_channel_request_adjacent_entry = true; }
    assert(rejected_altered_channel_request_adjacent_entry);

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
            assert(events.title_handoff);
            break;
        }
    }
    assert(live_input_alternate == 0x0b38);
    const auto& live_title_stage = live_input_opening.title_stage_session();
    assert(live_title_stage);
    assert(live_title_stage->stage().entry_address == 0x40426);
    assert(live_title_stage->original_sha256()
        == "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03");
    const auto& live_title_entry = live_input_opening.title_entry_prefix();
    assert(live_title_entry);
    assert(live_title_entry->incoming_profile == 1);
    assert(live_title_entry->controller_transfer_address == 0x206a0);
    assert(live_title_entry->mode_word_address == 0x4040e);
    assert(live_title_entry->mode_word_value == 1);
    assert(live_title_entry->normal_mode_byte_address == 0x19d52);
    assert(live_title_entry->normal_mode_byte_value == 1);
    assert(live_title_entry->stop_before_exec_address == 0x40450);
    const auto mode_five_entry = eon::execute_deuteros_amiga_title_entry_mode_five_prefix(
        system_disk, load_plan, 0x0105);
    assert(mode_five_entry.mode_word_value == 0x0105);
    assert(mode_five_entry.low_byte_destination_address == 0x3717e);
    assert(mode_five_entry.low_byte_value == 5);
    assert(mode_five_entry.literal_word_destination_address == 0x38092);
    assert(mode_five_entry.literal_word_value == 0x0101);
    assert(mode_five_entry.stop_before_exec_address == 0x40450);
    {
        auto altered_mode_five_disk = *amiga_disk1;
        altered_mode_five_disk[0x9b438] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_mode_five_disk));
            static_cast<void>(eon::execute_deuteros_amiga_title_entry_mode_five_prefix(
                altered_disk, load_plan, 5));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        bool rejected = false;
        try {
            static_cast<void>(eon::execute_deuteros_amiga_title_entry_prefix(
                system_disk, load_plan, 2));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_title_entry_disk = *amiga_disk1;
        altered_title_entry_disk[0x2f0e] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_title_entry_disk));
            static_cast<void>(eon::execute_deuteros_amiga_title_entry_prefix(
                altered_disk, load_plan, 1));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    assert(live_input_opening.title_handed_off());
    assert(live_input_opening.ticks() == 82);
    assert(live_input_opening.vblank_counter() == 82 * 4);
    const auto frame_at_title_handoff = live_input_opening.rgba_frame();
    assert(frame_at_title_handoff);
    const auto post_handoff_events = live_input_opening.tick(true);
    assert(post_handoff_events.sounds.empty());
    assert(post_handoff_events.alternate_resources.empty());
    assert(!post_handoff_events.title_handoff);
    assert(!post_handoff_events.transition_requested);
    assert(live_input_opening.ticks() == 82);
    assert(live_input_opening.vblank_counter() == 82 * 4);
    assert(live_input_opening.rgba_frame() == frame_at_title_handoff);
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
    // and combines it with selectors one/zero.  The selector-one table starts
    // with $ffff, so the recovered four-plane write formula makes set bits
    // palette index one and clear bits index zero. The in-memory frame update
    // must retain that exact bitplane result, rather than rasterising a
    // replacement host font.
    eon::DeuterosAmigaFrame alternate_frame;
    alternate_frame.color_indices.assign(
        static_cast<std::size_t>(eon::DeuterosAmigaFrame::width)
            * eon::DeuterosAmigaFrame::height, 0);
    eon::apply_deuteros_amiga_alternate_renderer(
        alternate_frame, system_disk, load_plan, *alternate_trace);
    {
        auto altered_renderer_disk_bytes = *amiga_disk1;
        altered_renderer_disk_bytes[0x6f68] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_renderer_disk(std::move(altered_renderer_disk_bytes));
            eon::apply_deuteros_amiga_alternate_renderer(
                alternate_frame, altered_renderer_disk, load_plan, *alternate_trace);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    // The third raw $201b0+($70-$20)*8 row for `p` is $7c: the final
    // two pixels are clear and therefore retain the selector-zero index.
    const std::array<std::uint8_t, 8> expected_p_row{0, 1, 1, 1, 1, 1, 0, 0};
    const auto p_row = alternate_frame.color_indices.begin()
        + static_cast<std::size_t>(194) * eon::DeuterosAmigaFrame::width + 120;
    assert(std::equal(expected_p_row.begin(), expected_p_row.end(), p_row));
    // $20580 writes the global display planes.  The renderer-only recovery
    // must preserve those original-backed pixels through a subsequent empty
    // channel pass, rather than retaining them only in a transient preview.
    {
        eon::DeuterosAmigaCompositor compositor;
        std::vector<eon::DeuterosAmigaChannelState> no_channels;
        static_cast<void>(compositor.compose(system_disk, first_bundle,
            first_indexed_blob, no_channels));
        eon::apply_deuteros_amiga_alternate_renderer(
            compositor.global_video_frame(), system_disk, load_plan, *alternate_trace);
        static_cast<void>(compositor.compose(system_disk, first_bundle,
            first_indexed_blob, no_channels));
        const auto persisted_p_row = compositor.global_video_frame().color_indices.begin()
            + static_cast<std::size_t>(194) * eon::DeuterosAmigaFrame::width + 120;
        assert(std::equal(expected_p_row.begin(), expected_p_row.end(), persisted_p_row));
    }
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
