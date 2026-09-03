#include "platform/game_data.hpp"
#include "display_geometry.hpp"
#include "launcher.hpp"
#include "i18n.hpp"
#include "launcher_text.hpp"
#include "presentation_preferences.hpp"
#include "engine/deuteros_amiga_opening.hpp"
#include "engine/deuteros_amiga_opening_runner.hpp"
#include "engine/release_runtime.hpp"
#include "engine/release_runtime_capability.hpp"
#include "engine/menu_runtime_launch.hpp"
#include "engine/native_session_controller.hpp"
#include "engine/runtime_host.hpp"
#include "engine/deuteros_atari_bootstrap_session.hpp"
#include "engine/millennium_amiga_bootstrap_session.hpp"
#include "data/zip_archive.hpp"
#include "data/amiga_adf.hpp"
#include "data/atari_st_prg.hpp"
#include "data/atari_st_stx.hpp"
#include "data/amiga_ofs.hpp"
#include "data/creative_voice.hpp"
#include "data/deuteros_amiga_bundle.hpp"
#include "data/deuteros_amiga_data_disk.hpp"
#include "data/deuteros_amiga_audio.hpp"
#include "data/deuteros_amiga_alternate_renderer.hpp"
#include "engine/deuteros_amiga_paula.hpp"
#include "data/deuteros_amiga_channel_vm.hpp"
#include "data/deuteros_amiga_frame.hpp"
#include "data/deuteros_amiga_loader.hpp"
#include "data/deuteros_amiga_title_stage.hpp"
#include "data/deuteros_amiga_reference_trace.hpp"
#include "data/deuteros_amiga_main_stage_reference_trace.hpp"
#include "data/deuteros_amiga_title_display_reference_trace.hpp"
#include "data/deuteros_amiga_title_bridge_reference_trace.hpp"
#include "data/deuteros_atari_boot.hpp"
#include "data/deuteros_atari_reference_trace.hpp"
#include "data/fat12.hpp"
#include "data/millennium_dos_bitmap.hpp"
#include "data/millennium_control_text.hpp"
#include "data/millennium_dos_game_data.hpp"
#include "data/millennium_dos_game_flow.hpp"
#include "data/millennium_dos_gameplay_screen.hpp"
#include "data/millennium_dos_voice_bank.hpp"
#include "data/millennium_dos_gx_catalog.hpp"
#include "data/millennium_dos_last_screen.hpp"
#include "data/millennium_save_comparison.hpp"
#include "data/millennium_amiga_loader.hpp"
#include "data/millennium_dos_lib.hpp"
#include "data/millennium_dos_title_flow.hpp"
#include "data/millennium_dos_title_exit.hpp"
#include "data/millennium_dos_title_transition.hpp"
#include "data/millennium_dos_title_presentation.hpp"
#include "data/millennium_dos_video_driver.hpp"
#include "data/millennium_dos_sound_driver.hpp"
#include "engine/millennium_dos_sound_selection_session.hpp"
#include "data/millennium_dos_reference_trace.hpp"
#include "data/millennium_amiga_reference_trace.hpp"
#include "data/reference_trace.hpp"
#include "data/reference_trace_registry.hpp"
#include "data/modern_pixel_reconstruction.hpp"
#include "presentation/modern_presentation_pipeline.hpp"
#include "data/modern_asset_pack.hpp"
#include "engine/millennium_dos_title_session.hpp"
#include "engine/millennium_dos_game_session.hpp"
#include "engine/millennium_dos_gx_startup_session.hpp"
#include "engine/millennium_dos_gx_startup_trace_admission.hpp"
#include "engine/millennium_dos_save_session.hpp"
#include "engine/millennium_atari_bootstrap_session.hpp"
#include "data/sha256.hpp"
#include "data/function_map.hpp"
#include "data/recovery_map.hpp"
#include "data/runtime_diagnostics.hpp"
#include "data/release_manifest.hpp"
#include "data/startup_boundary.hpp"
#include "data/static_control_flow.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
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
#include <string>
#include <type_traits>
#include <vector>

#include <zlib.h>

namespace {

static_assert(!std::is_convertible_v<eon::RuntimeHost*, eon::NativeSessionController*>);
static_assert(!std::is_convertible_v<const eon::RuntimeHost*, const eon::NativeSessionController*>);

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

void assert_original_data_source_classification() {
    const auto test_tmpdir = std::getenv("EON_TEST_TMPDIR");
    assert(test_tmpdir && *test_tmpdir);
    const auto temporary_root = std::filesystem::path(test_tmpdir);
    std::filesystem::create_directories(temporary_root);
    const auto nonce = std::to_string(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const auto root = temporary_root / ("project-eon-data-source-" + nonce);
    assert(eon::classify_original_data_source(root) == eon::OriginalDataSourceKind::missing);
    std::filesystem::create_directories(root);
    assert(eon::classify_original_data_source(root) == eon::OriginalDataSourceKind::directory);
    const auto archive = root / "original-media.zip";
    {
        std::ofstream output(archive, std::ios::binary);
        output << "fixture";
    }
    assert(eon::classify_original_data_source(archive) == eon::OriginalDataSourceKind::archive);
    assert(eon::is_original_data_source(eon::OriginalDataSourceKind::directory));
    assert(eon::is_original_data_source(eon::OriginalDataSourceKind::archive));
    assert(!eon::is_original_data_source(eon::OriginalDataSourceKind::missing));
    assert(!eon::is_original_data_source(eon::OriginalDataSourceKind::unsupported));
    std::error_code symlink_error;
    const auto redirected = root / "redirected-media.zip";
    std::filesystem::create_symlink(archive, redirected, symlink_error);
    if (!symlink_error) {
        assert(eon::classify_original_data_source(redirected)
            == eon::OriginalDataSourceKind::unsupported);
    }
    eon::ReleaseScanner scanner(root);
    const auto initial_snapshot = scanner.snapshot();
    assert(initial_snapshot.source_kind == eon::OriginalDataSourceKind::directory);
    assert(initial_snapshot.discovering && !initial_snapshot.complete);
    assert(initial_snapshot.candidate_count == 0 && initial_snapshot.scanned_count == 0);
    assert(initial_snapshot.unique_release_count == 0
        && initial_snapshot.unique_unbound_direct_media_count == 0);
    while (!scanner.advance(1)) {
    }
    const auto complete_snapshot = scanner.snapshot();
    assert(complete_snapshot.complete && !complete_snapshot.discovering);
    assert(complete_snapshot.source_kind == eon::OriginalDataSourceKind::directory);
    assert(complete_snapshot.candidate_count == 1 && complete_snapshot.scanned_count == 1);
    assert(complete_snapshot.unique_release_count == 0);
    assert(complete_snapshot.unique_unbound_direct_media_count == 0);
    assert(complete_snapshot.report.candidates == 1);
    assert(complete_snapshot.report.size_rejected_candidates == 1);
    assert(complete_snapshot.report.verified_direct_media_occurrences == 0);
    assert(scanner.unbound_direct_media().empty());
    assert(complete_snapshot.report.symlink_rejected_entries == (symlink_error ? 0U : 1U));
    std::filesystem::remove_all(root);
}

void assert_modern_asset_pack_admission() {
    const auto test_tmpdir = std::getenv("EON_TEST_TMPDIR");
    assert(test_tmpdir && *test_tmpdir);
    const auto temporary_root = std::filesystem::path(test_tmpdir);
    std::filesystem::create_directories(temporary_root);
    const auto nonce = std::to_string(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const auto root = temporary_root / ("project-eon-modern-pack-" + nonce);
    const auto pack_root = root / "independent-title";
    const auto textures = pack_root / "textures";
    std::filesystem::create_directories(textures);
    const auto asset = textures / "title.rgba";
    {
        std::ofstream output(asset, std::ios::binary);
        output << "abc";
    }
    {
        std::ofstream output(pack_root / "pack.eonmodern", std::ios::binary);
        output << "schema\tproject-eon.modern-asset-pack/v1\n"
               << "id\tindependent-title\n"
               << "version\t1.0.0\n"
               << "license\tCC0-1.0\n"
               << "provenance\tindependently-created\n"
               << "game\tmillennium\n"
               << "platform\tdos\n"
               << "source_release_sha256\te6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123\n"
               << "asset\tmillennium.dos.title textures/title.rgba 3 "
                  "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n";
    }
    const auto discovered = eon::discover_modern_asset_packs(root);
    assert(discovered.size() == 1);
    assert(discovered.front().accepted());
    assert(discovered.front().pack.id == "independent-title");
    assert(discovered.front().pack.assets.size() == 1);
    assert(discovered.front().pack.assets.front().path == asset);

    // A pack remains inadmissible when declared bytes change after discovery.
    {
        std::ofstream output(asset, std::ios::binary | std::ios::trunc);
        output << "abd";
    }
    const auto changed = eon::validate_modern_asset_pack(pack_root / "pack.eonmodern");
    assert(!changed.accepted());
    assert(!changed.error.empty());
    // External grammar bytes only: the runtime target must have the exact
    // hash-bound ID, release identity, bounded RGBA PNG structure, and one
    // finite title mapping. SDL_image later decodes these rehashed in-memory
    // bytes; this native test has no SDL decoder dependency.
    const auto render_root = root / "render-title";
    std::filesystem::create_directories(render_root);
    const auto png = render_root / "title.png";
    // The runtime's mapping is decoder-independent, so construct bounded,
    // fully decodable external PNG grammar bytes with correct CRCs here.
    // These are not game data or replacement art: each pixel is transparent
    // and the fixture only exercises pack admission before SDL_image.
    const auto mapped_png = [](const std::uint32_t width, const std::uint32_t height) {
        std::vector<std::uint8_t> bytes{0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
        const auto append_u32 = [&bytes](const std::uint32_t value) {
            bytes.push_back(static_cast<std::uint8_t>(value >> 24U));
            bytes.push_back(static_cast<std::uint8_t>(value >> 16U));
            bytes.push_back(static_cast<std::uint8_t>(value >> 8U));
            bytes.push_back(static_cast<std::uint8_t>(value));
        };
        const auto append_chunk = [&bytes, &append_u32](const std::array<char, 4>& type,
                                                         const std::vector<std::uint8_t>& data) {
            append_u32(static_cast<std::uint32_t>(data.size()));
            bytes.insert(bytes.end(), type.begin(), type.end());
            bytes.insert(bytes.end(), data.begin(), data.end());
            uLong crc = crc32(0L, Z_NULL, 0);
            crc = crc32(crc, reinterpret_cast<const Bytef*>(type.data()), type.size());
            if (!data.empty()) crc = crc32(crc, data.data(), static_cast<uInt>(data.size()));
            append_u32(static_cast<std::uint32_t>(crc));
        };
        std::vector<std::uint8_t> header;
        const auto append_header_u32 = [&header](const std::uint32_t value) {
            header.push_back(static_cast<std::uint8_t>(value >> 24U));
            header.push_back(static_cast<std::uint8_t>(value >> 16U));
            header.push_back(static_cast<std::uint8_t>(value >> 8U));
            header.push_back(static_cast<std::uint8_t>(value));
        };
        append_header_u32(width);
        append_header_u32(height);
        header.insert(header.end(), {8U, 6U, 0U, 0U, 0U});
        append_chunk({'I', 'H', 'D', 'R'}, header);
        const auto row_size = static_cast<std::size_t>(width) * 4U + 1U;
        std::vector<std::uint8_t> raw(row_size * height, 0U);
        uLongf compressed_size = compressBound(static_cast<uLong>(raw.size()));
        std::vector<std::uint8_t> compressed(compressed_size);
        assert(compress2(compressed.data(), &compressed_size, raw.data(),
            static_cast<uLong>(raw.size()), Z_BEST_COMPRESSION) == Z_OK);
        compressed.resize(compressed_size);
        append_chunk({'I', 'D', 'A', 'T'}, compressed);
        append_chunk({'I', 'E', 'N', 'D'}, {});
        return bytes;
    };
    const auto png_bytes = mapped_png(640U, 400U);
    {
        std::ofstream output(png, std::ios::binary);
        output.write(reinterpret_cast<const char*>(png_bytes.data()),
            static_cast<std::streamsize>(png_bytes.size()));
    }
    const auto release_hash = "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123";
    const auto write_render_manifest = [&](const std::string_view asset_id, const std::size_t asset_size,
                                           const std::string_view hash) {
        std::ofstream output(render_root / "pack.eonmodern", std::ios::binary | std::ios::trunc);
        output << "schema\tproject-eon.modern-asset-pack/v1\nid\trender-title\nversion\t1\n"
               << "license\tCC0-1.0\nprovenance\tindependently-created\ngame\tmillennium\nplatform\tdos\n"
               << "source_release_sha256\t" << release_hash << "\nasset\t" << asset_id
               << " title.png " << asset_size << ' ' << hash << '\n';
    };
    write_render_manifest("millennium.dos.title.png-640x400", png_bytes.size(),
        eon::to_hex(eon::sha256(png_bytes)));
    const auto surface = eon::load_millennium_dos_title_modern_surface(
        render_root / "pack.eonmodern", release_hash);
    assert(surface.pack_id == "render-title" && surface.asset_id == "millennium.dos.title.png-640x400"
        && surface.width == 640 && surface.height == 400
        && surface.png == png_bytes);
    const auto png_4x = mapped_png(1280U, 800U);
    {
        std::ofstream output(png, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(png_4x.data()),
            static_cast<std::streamsize>(png_4x.size()));
    }
    write_render_manifest("millennium.dos.title.png-1280x800", png_4x.size(),
        eon::to_hex(eon::sha256(png_4x)));
    const auto surface_4x = eon::load_millennium_dos_title_modern_surface(
        render_root / "pack.eonmodern", release_hash);
    assert(surface_4x.asset_id == "millennium.dos.title.png-1280x800"
        && surface_4x.width == 1280 && surface_4x.height == 800 && surface_4x.png == png_4x);
    const auto png_2x = render_root / "title-2x.png";
    const auto png_4x_path = render_root / "title-4x.png";
    {
        std::ofstream output(png_2x, std::ios::binary);
        output.write(reinterpret_cast<const char*>(png_bytes.data()),
            static_cast<std::streamsize>(png_bytes.size()));
    }
    {
        std::ofstream output(png_4x_path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(png_4x.data()),
            static_cast<std::streamsize>(png_4x.size()));
    }
    {
        std::ofstream output(render_root / "pack.eonmodern", std::ios::binary | std::ios::trunc);
        output << "schema\tproject-eon.modern-asset-pack/v1\nid\trender-title\nversion\t1\n"
               << "license\tCC0-1.0\nprovenance\tindependently-created\ngame\tmillennium\nplatform\tdos\n"
               << "source_release_sha256\t" << release_hash << "\nasset\tmillennium.dos.title.png-640x400"
               << " title-2x.png " << png_bytes.size() << ' ' << eon::to_hex(eon::sha256(png_bytes))
               << "\nasset\tmillennium.dos.title.png-1280x800 title-4x.png " << png_4x.size() << ' '
               << eon::to_hex(eon::sha256(png_4x)) << '\n';
    }
    const auto preferred_surface = eon::load_millennium_dos_title_modern_surface(
        render_root / "pack.eonmodern", release_hash);
    assert(preferred_surface.asset_id == "millennium.dos.title.png-1280x800"
        && preferred_surface.png == png_4x);
    const auto title_resolver = eon::ModernAssetPackPresentationResolver::create(
        render_root / "pack.eonmodern", eon::ModernAssetPackPresentationTarget::millennium_dos_title,
        eon::Game::millennium, eon::Platform::dos, release_hash);
    const auto resolved_title = title_resolver.resolve(0);
    assert(resolved_title.asset_id == "millennium.dos.title.png-1280x800"
        && resolved_title.png == png_4x);
    bool title_tick_rejected = false;
    try { static_cast<void>(title_resolver.resolve(1));
    } catch (const std::runtime_error&) { title_tick_rejected = true; }
    assert(title_tick_rejected);
    bool title_identity_rejected = false;
    try { static_cast<void>(eon::ModernAssetPackPresentationResolver::create(
        render_root / "pack.eonmodern", eon::ModernAssetPackPresentationTarget::millennium_dos_title,
        eon::Game::millennium, eon::Platform::amiga, release_hash));
    } catch (const std::runtime_error&) { title_identity_rejected = true; }
    assert(title_identity_rejected);
    const auto render_validation = eon::validate_modern_asset_pack(render_root / "pack.eonmodern");
    assert(render_validation.accepted());
    const auto render_preflight = eon::preflight_modern_asset_pack(render_root / "pack.eonmodern",
        eon::Game::millennium, eon::Platform::dos, release_hash);
    assert(render_preflight.accepted);
    assert(render_preflight.pack_id == "render-title");
    assert(render_preflight.provenance == "independently-created");
    assert(render_preflight.targets.millennium_dos_title_640x400);
    const auto wrong_release_preflight = eon::preflight_modern_asset_pack(render_root / "pack.eonmodern",
        eon::Game::millennium, eon::Platform::dos,
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    assert(!wrong_release_preflight.accepted);
    assert(wrong_release_preflight.error == "Modern asset pack does not match the selected original release");
    const auto render_targets = eon::modern_asset_pack_renderer_targets(render_validation.pack);
    assert(render_targets.millennium_dos_title_640x400);
    assert(render_targets.millennium_dos_title_1280x800);
    assert(render_targets.deuteros_amiga_opening_640x400_frames == 0);
    {
        std::ofstream output(png, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(png_bytes.data()),
            static_cast<std::streamsize>(png_bytes.size()));
    }
    write_render_manifest("millennium.dos.title.png-640x400", png_bytes.size(),
        eon::to_hex(eon::sha256(png_bytes)));
    bool wrong_release_rejected = false;
    try { static_cast<void>(eon::load_millennium_dos_title_modern_surface(
        render_root / "pack.eonmodern", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
    } catch (const std::runtime_error&) { wrong_release_rejected = true; }
    assert(wrong_release_rejected);
    write_render_manifest("millennium.dos.title.not-supported", png_bytes.size(),
        eon::to_hex(eon::sha256(png_bytes)));
    bool wrong_id_rejected = false;
    try { static_cast<void>(eon::load_millennium_dos_title_modern_surface(
        render_root / "pack.eonmodern", release_hash));
    } catch (const std::runtime_error&) { wrong_id_rejected = true; }
    assert(wrong_id_rejected);
    write_render_manifest("millennium.dos.title.png-640x400", png_bytes.size(),
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    bool wrong_hash_rejected = false;
    try { static_cast<void>(eon::load_millennium_dos_title_modern_surface(
        render_root / "pack.eonmodern", release_hash));
    } catch (const std::runtime_error&) { wrong_hash_rejected = true; }
    assert(wrong_hash_rejected);
    const auto malformed_png = mapped_png(65'537U, 400U);
    {
        std::ofstream output(png, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(malformed_png.data()),
            static_cast<std::streamsize>(malformed_png.size()));
    }
    write_render_manifest("millennium.dos.title.png-640x400", malformed_png.size(),
        eon::to_hex(eon::sha256(malformed_png)));
    bool malformed_rejected = false;
    try { static_cast<void>(eon::load_millennium_dos_title_modern_surface(
        render_root / "pack.eonmodern", release_hash));
    } catch (const std::runtime_error&) { malformed_rejected = true; }
    assert(malformed_rejected);
    // Correct release and asset hashes alone must not smuggle a chunk with a
    // bad internal checksum into SDL_image.  The PNG gate validates every
    // chunk CRC before decoder input.
    auto bad_crc_png = png_bytes;
    bad_crc_png[32] ^= 0x01U; // IHDR checksum's final byte.
    {
        std::ofstream output(png, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(bad_crc_png.data()),
            static_cast<std::streamsize>(bad_crc_png.size()));
    }
    write_render_manifest("millennium.dos.title.png-640x400", bad_crc_png.size(),
        eon::to_hex(eon::sha256(bad_crc_png)));
    bool bad_crc_rejected = false;
    try { static_cast<void>(eon::load_millennium_dos_title_modern_surface(
        render_root / "pack.eonmodern", release_hash));
    } catch (const std::runtime_error&) { bad_crc_rejected = true; }
    assert(bad_crc_rejected);
    // Correct PNG chunk checksums cannot stand in for a decodable, bounded
    // RGBA image. Break the zlib header and recompute IDAT's CRC so this
    // reaches the new pre-SDL decompression gate rather than the CRC gate.
    auto bad_deflate_png = png_bytes;
    constexpr std::size_t idat_type_offset = 37;
    constexpr std::size_t idat_data_offset = 41;
    const auto idat_length = (static_cast<std::size_t>(bad_deflate_png[33]) << 24U)
        | (static_cast<std::size_t>(bad_deflate_png[34]) << 16U)
        | (static_cast<std::size_t>(bad_deflate_png[35]) << 8U) | bad_deflate_png[36];
    assert(idat_length != 0U && idat_data_offset + idat_length + 4U <= bad_deflate_png.size());
    bad_deflate_png[idat_data_offset] = 0U;
    uLong idat_crc = crc32(0L, Z_NULL, 0);
    idat_crc = crc32(idat_crc, bad_deflate_png.data() + idat_type_offset,
        static_cast<uInt>(4U + idat_length));
    const auto idat_crc_offset = idat_data_offset + idat_length;
    bad_deflate_png[idat_crc_offset] = static_cast<std::uint8_t>(idat_crc >> 24U);
    bad_deflate_png[idat_crc_offset + 1U] = static_cast<std::uint8_t>(idat_crc >> 16U);
    bad_deflate_png[idat_crc_offset + 2U] = static_cast<std::uint8_t>(idat_crc >> 8U);
    bad_deflate_png[idat_crc_offset + 3U] = static_cast<std::uint8_t>(idat_crc);
    {
        std::ofstream output(png, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(bad_deflate_png.data()),
            static_cast<std::streamsize>(bad_deflate_png.size()));
    }
    write_render_manifest("millennium.dos.title.png-640x400", bad_deflate_png.size(),
        eon::to_hex(eon::sha256(bad_deflate_png)));
    bool bad_deflate_rejected = false;
    try { static_cast<void>(eon::load_millennium_dos_title_modern_surface(
        render_root / "pack.eonmodern", release_hash));
    } catch (const std::runtime_error&) { bad_deflate_rejected = true; }
    assert(bad_deflate_rejected);
    auto trailing_png = png_bytes;
    trailing_png.push_back(0);
    {
        std::ofstream output(png, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(trailing_png.data()),
            static_cast<std::streamsize>(trailing_png.size()));
    }
    write_render_manifest("millennium.dos.title.png-640x400", trailing_png.size(),
        eon::to_hex(eon::sha256(trailing_png)));
    bool trailing_rejected = false;
    try { static_cast<void>(eon::load_millennium_dos_title_modern_surface(
        render_root / "pack.eonmodern", release_hash));
    } catch (const std::runtime_error&) { trailing_rejected = true; }
    assert(trailing_rejected);
    // The recovered Deuteros opening is not a generic movie. Its one finite
    // externally redrawable route is 82 VM-composed frames ending at the
    // held-input handoff. All frames must be present at one exact tier before
    // any renderer can select a single one.
    const auto deuteros_root = root / "deuteros-opening";
    std::filesystem::create_directories(deuteros_root / "opening");
    const auto deuteros_png = mapped_png(640U, 400U);
    const auto deuteros_png_hash = eon::to_hex(eon::sha256(deuteros_png));
    std::ofstream deuteros_manifest(deuteros_root / "pack.eonmodern", std::ios::binary);
    deuteros_manifest << "schema\tproject-eon.modern-asset-pack/v1\n"
        << "id\tdeuteros-held-opening\nversion\t1\nlicense\tCC0-1.0\n"
        << "provenance\tindependently-created\ngame\tdeuteros\nplatform\tamiga\n"
        << "source_release_sha256\tf4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04\n";
    for (std::size_t frame = 1; frame <= eon::deuteros_amiga_held_opening_frame_count; ++frame) {
        auto number = std::to_string(frame);
        number.insert(number.begin(), 3U - number.size(), '0');
        const auto path = deuteros_root / "opening" / (number + ".png");
        std::ofstream output(path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(deuteros_png.data()),
            static_cast<std::streamsize>(deuteros_png.size()));
        deuteros_manifest << "asset\tdeuteros.amiga.opening.held-v1.frame-" << number
            << ".png-640x400 opening/" << number << ".png " << deuteros_png.size()
            << ' ' << deuteros_png_hash << '\n';
    }
    deuteros_manifest.close();
    const auto deuteros_sequence = eon::load_deuteros_amiga_held_opening_modern_sequence(
        deuteros_root / "pack.eonmodern",
        "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04");
    assert(deuteros_sequence.width == 640 && deuteros_sequence.height == 400);
    const auto deuteros_first = eon::load_deuteros_amiga_held_opening_modern_frame(deuteros_sequence, 1);
    const auto deuteros_last = eon::load_deuteros_amiga_held_opening_modern_frame(deuteros_sequence, 82);
    assert(deuteros_first.png == deuteros_png && deuteros_last.png == deuteros_png);
    const auto opening_resolver = eon::ModernAssetPackPresentationResolver::create(
        deuteros_root / "pack.eonmodern",
        eon::ModernAssetPackPresentationTarget::deuteros_amiga_held_opening,
        eon::Game::deuteros, eon::Platform::amiga,
        "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04");
    assert(opening_resolver.resolve(1).png == deuteros_png);
    bool opening_terminal_without_handoff_rejected = false;
    try { static_cast<void>(opening_resolver.resolve(82));
    } catch (const std::runtime_error&) { opening_terminal_without_handoff_rejected = true; }
    assert(opening_terminal_without_handoff_rejected);
    assert(opening_resolver.resolve(82, true).png == deuteros_png);
    bool opening_zero_tick_rejected = false;
    try { static_cast<void>(opening_resolver.resolve(0));
    } catch (const std::runtime_error&) { opening_zero_tick_rejected = true; }
    assert(opening_zero_tick_rejected);
    bool opening_tick_rejected = false;
    try { static_cast<void>(eon::load_deuteros_amiga_held_opening_modern_frame(deuteros_sequence, 83));
    } catch (const std::runtime_error&) { opening_tick_rejected = true; }
    assert(opening_tick_rejected);
    {
        std::ofstream output(deuteros_root / "opening" / "082.png", std::ios::binary | std::ios::trunc);
        output << "changed";
    }
    bool changed_opening_frame_rejected = false;
    try { static_cast<void>(eon::load_deuteros_amiga_held_opening_modern_frame(deuteros_sequence, 82));
    } catch (const std::runtime_error&) { changed_opening_frame_rejected = true; }
    assert(changed_opening_frame_rejected);
    // A non-symlink final file is insufficient when an intermediate component
    // points outside the selected pack directory.
    const auto external = root / "outside";
    std::filesystem::create_directories(external);
    const auto linked_textures = pack_root / "textures";
    std::filesystem::remove_all(linked_textures);
    std::error_code symlink_error;
    std::filesystem::create_directory_symlink(external, linked_textures, symlink_error);
    if (!symlink_error) {
        const auto symlinked = eon::validate_modern_asset_pack(pack_root / "pack.eonmodern");
        assert(!symlinked.accepted());
        assert(symlinked.error.find("symlink") != std::string::npos);
    }
    std::filesystem::remove_all(root);
}

} // namespace

int main() {
    // Native lifecycle is a finite state machine independent of SDL windows,
    // card focus and F10 presentation settings. Every coordinator snapshot
    // must have exactly one visible lifecycle state, while an empty/rejected
    // coordinator never becomes a generic active game.
    assert(eon::native_session_state_for({}, eon::ReleaseRuntimeAdmission::unselected)
        == eon::NativeSessionState::menu);
    assert(eon::native_session_state_for({}, eon::ReleaseRuntimeAdmission::archive_rejected)
        == eon::NativeSessionState::admission_rejected);
    eon::RuntimeSessionSnapshot state_fixture;
    const std::array state_cases{
        std::pair{eon::RuntimeSessionKind::millennium_dos_title,
            eon::NativeSessionState::millennium_dos_title},
        std::pair{eon::RuntimeSessionKind::millennium_dos_sound_driver_boundary,
            eon::NativeSessionState::millennium_dos_sound_driver_boundary},
        std::pair{eon::RuntimeSessionKind::millennium_dos_title_handoff_boundary,
            eon::NativeSessionState::millennium_dos_title_handoff_boundary},
        std::pair{eon::RuntimeSessionKind::millennium_amiga_bootstrap,
            eon::NativeSessionState::millennium_amiga_bootstrap},
        std::pair{eon::RuntimeSessionKind::millennium_atari_bootstrap,
            eon::NativeSessionState::millennium_atari_bootstrap},
        std::pair{eon::RuntimeSessionKind::deuteros_amiga_opening,
            eon::NativeSessionState::deuteros_amiga_opening},
        std::pair{eon::RuntimeSessionKind::deuteros_amiga_title_stage,
            eon::NativeSessionState::deuteros_amiga_title_stage_boundary},
        std::pair{eon::RuntimeSessionKind::deuteros_atari_bootstrap,
            eon::NativeSessionState::deuteros_atari_bootstrap},
    };
    for (const auto& [kind, expected] : state_cases) {
        state_fixture.kind = kind;
        assert(eon::native_session_state_for(state_fixture, eon::ReleaseRuntimeAdmission::active)
            == expected);
    }
    const std::array presentation_cases{
        std::pair{eon::RuntimeSessionKind::millennium_dos_title,
            eon::RuntimePresentationKind::millennium_dos_title},
        std::pair{eon::RuntimeSessionKind::millennium_dos_sound_driver_boundary,
            eon::RuntimePresentationKind::millennium_dos_sound_driver_boundary},
        std::pair{eon::RuntimeSessionKind::millennium_dos_title_handoff_boundary,
            eon::RuntimePresentationKind::millennium_dos_title_handoff_boundary},
        std::pair{eon::RuntimeSessionKind::millennium_amiga_bootstrap,
            eon::RuntimePresentationKind::millennium_amiga_bootstrap},
        std::pair{eon::RuntimeSessionKind::millennium_atari_bootstrap,
            eon::RuntimePresentationKind::millennium_atari_bootstrap},
        std::pair{eon::RuntimeSessionKind::deuteros_amiga_opening,
            eon::RuntimePresentationKind::deuteros_amiga_opening},
        std::pair{eon::RuntimeSessionKind::deuteros_amiga_title_stage,
            eon::RuntimePresentationKind::deuteros_amiga_title_stage_boundary},
        std::pair{eon::RuntimeSessionKind::deuteros_atari_bootstrap,
            eon::RuntimePresentationKind::deuteros_atari_bootstrap},
    };
    eon::ResolvedLaunchRequest presentation_launch;
    for (const auto& [kind, expected] : presentation_cases) {
        const auto snapshot = eon::make_runtime_session_snapshot(presentation_launch, kind);
        const auto state = eon::native_session_state_for(snapshot, eon::ReleaseRuntimeAdmission::active);
        const auto presentation = eon::runtime_presentation_for(
            state, eon::ReleaseRuntimeAdmission::active, snapshot);
        assert(presentation && presentation->kind == expected && presentation->state == state
            && presentation->boundary == snapshot.boundary
            && presentation->capabilities == snapshot.capabilities
            && presentation->input_contract == snapshot.input_contract
            && presentation->state_label == eon::native_session_state_label(state)
            && presentation->boundary_label == eon::runtime_session_boundary_label(snapshot.boundary));
        assert(!eon::runtime_presentation_for(eon::NativeSessionState::menu,
            eon::ReleaseRuntimeAdmission::active, snapshot));
    }
    assert(!eon::runtime_presentation_for(eon::NativeSessionState::menu,
        eon::ReleaseRuntimeAdmission::unselected, {}));
    auto invalid_presentation_snapshot = eon::make_runtime_session_snapshot(
        presentation_launch, eon::RuntimeSessionKind::millennium_dos_title);
    invalid_presentation_snapshot.boundary = eon::RuntimeSessionBoundary::bootstrap_boundary;
    assert(!eon::runtime_presentation_for(eon::NativeSessionState::millennium_dos_title,
        eon::ReleaseRuntimeAdmission::active, invalid_presentation_snapshot));
    assert(eon::runtime_input_contract_for_session(eon::RuntimeSessionKind::millennium_dos_title)
        == eon::RuntimeInputContract::millennium_dos_startup_observation);
    assert(eon::runtime_input_contract_for_session(eon::RuntimeSessionKind::deuteros_amiga_opening)
        == eon::RuntimeInputContract::deuteros_amiga_opening_held_signal);
    assert(eon::runtime_input_contract_for_session(
        eon::RuntimeSessionKind::millennium_dos_sound_driver_boundary)
        == eon::RuntimeInputContract::none);
    assert(eon::runtime_input_contract_identifier(
        eon::RuntimeInputContract::millennium_dos_startup_observation)
        == "millennium-dos-startup-observation");
    assert(eon::runtime_input_contract_accepts_observation(
        eon::RuntimeInputContract::millennium_dos_startup_observation,
        eon::RuntimeInputObservationKind::ascii_character));
    assert(eon::runtime_input_contract_accepts_observation(
        eon::RuntimeInputContract::millennium_dos_startup_observation,
        eon::RuntimeInputObservationKind::character_available));
    assert(!eon::runtime_input_contract_accepts_observation(
        eon::RuntimeInputContract::millennium_dos_startup_observation,
        eon::RuntimeInputObservationKind::opening_input_held));
    assert(eon::runtime_input_contract_accepts_observation(
        eon::RuntimeInputContract::deuteros_amiga_opening_held_signal,
        eon::RuntimeInputObservationKind::opening_input_held));
    assert(!eon::runtime_input_contract_accepts_observation(
        eon::RuntimeInputContract::none, eon::RuntimeInputObservationKind::ascii_character));
    assert(eon::native_session_state_label(eon::NativeSessionState::returning_to_menu)
        == "RETURNING TO MENU");
    eon::NativeSessionController state_controller;
    assert(state_controller.state() == eon::NativeSessionState::menu && !state_controller.is_live());
    state_controller.begin_return_to_menu();
    assert(state_controller.state() == eon::NativeSessionState::returning_to_menu);
    state_controller.finish_return_to_menu();
    assert(state_controller.is_menu() && !state_controller.is_live());
    eon::RuntimeHost runtime_host;
    assert(runtime_host.generation() == 0 && !runtime_host.revoking()
        && runtime_host.is_menu());
    runtime_host.set_input_suppressed(true);
    assert(runtime_host.input_suppressed()
        && runtime_host.observe_input(eon::RuntimeInputObservation::available_character())
            == eon::RuntimeInputDisposition::rejected);
    runtime_host.set_input_suppressed(false);
    assert(!runtime_host.input_suppressed());
    const auto idle_host_advance = runtime_host.advance(1'000);
    assert(!idle_host_advance.opening_started && !idle_host_advance.opening_active
        && idle_host_advance.opening.events.empty());
    const auto idle_host_snapshot = runtime_host.snapshot();
    assert(idle_host_snapshot.generation == 0 && !idle_host_snapshot.revoking
        && !idle_host_snapshot.input_suppressed
        && idle_host_snapshot.admission == eon::ReleaseRuntimeAdmission::unselected
        && idle_host_snapshot.state == eon::NativeSessionState::menu
        && !idle_host_snapshot.session && !idle_host_snapshot.presentation);
    runtime_host.begin_source_revocation();
    assert(runtime_host.generation() == 1 && runtime_host.revoking());
    const auto revoking_host_snapshot = runtime_host.snapshot();
    assert(revoking_host_snapshot.revoking && !revoking_host_snapshot.input_suppressed
        && !revoking_host_snapshot.session
        && !revoking_host_snapshot.presentation);
    // A duplicated front-end teardown must neither manufacture another
    // generation nor revive the controller while SDL objects are draining.
    runtime_host.begin_source_revocation();
    assert(runtime_host.generation() == 1 && runtime_host.revoking());
    assert(runtime_host.observe_input(
        eon::RuntimeInputObservation::available_character()) == eon::RuntimeInputDisposition::rejected);
    runtime_host.finish_source_revocation();
    assert(!runtime_host.revoking() && runtime_host.is_menu());

    // The input modal is source-generation state. Teardown, including a
    // route change while F10 is open, must not carry suppression into the
    // next independently admitted session.
    runtime_host.set_input_suppressed(true);
    assert(runtime_host.input_suppressed());
    runtime_host.begin_source_revocation();
    runtime_host.finish_source_revocation();
    assert(runtime_host.is_menu() && !runtime_host.input_suppressed());

    // Static-control-flow sidecars are external preservation evidence, not a
    // media parser or a dispatch table.  The native reader therefore accepts
    // only the exact v1 envelope and preserves the unclassified boundary.
    const std::string static_flow_sidecar = R"json({
      "classification":"static-candidate-unclassified",
      "documents":[
        {
          "address_space":"runtime",
          "archive_sha256":"0000000000000000000000000000000000000000000000000000000000000000",
          "classification":"static-candidate-unclassified",
          "cpu":"i8086",
          "ranges":[
            {"edges":[
              {"classification":"static-candidate-unclassified","instruction_size":3,"kind":"call","runtime_address":256,"source_offset":0,"target_runtime_address":261,"target_scope":"within-declared-range"},
              {"classification":"static-candidate-unclassified","instruction_size":1,"kind":"return","runtime_address":259,"source_offset":3,"target":"return-address-unproven"}
            ],"length":4,"runtime_address":256,"sha256":"1111111111111111111111111111111111111111111111111111111111111111","source_offset":0},
            {"edges":[
              {"classification":"static-candidate-unclassified","instruction_size":2,"interrupt_vector":33,"kind":"interrupt","runtime_address":261,"source_offset":4}
            ],"length":2,"runtime_address":260,"sha256":"2222222222222222222222222222222222222222222222222222222222222222","source_offset":4}
          ],
          "schema":"project-eon.static-control-flow/v1",
          "source":"FIXTURE.EXE",
          "source_kind":"verified-direct-media-member",
          "direct_media_set_sha256":"8888888888888888888888888888888888888888888888888888888888888888",
          "source_sha256":"3333333333333333333333333333333333333333333333333333333333333333"
        },
        {
          "address_space":"image-relative-unrelocated",
          "archive_sha256":"4444444444444444444444444444444444444444444444444444444444444444",
          "classification":"static-candidate-unclassified",
          "container_sha256":"7777777777777777777777777777777777777777777777777777777777777777",
          "cpu":"m68000",
          "ranges":[{"edges":[
            {"classification":"static-candidate-unclassified","instruction_size":2,"kind":"trap","source_offset":8,"image_relative_address":8,"trap_vector":1}
          ],"length":2,"image_relative_address":8,"sha256":"5555555555555555555555555555555555555555555555555555555555555555","source_offset":8}],
          "schema":"project-eon.static-control-flow/v1",
          "source":"FIXTURE.PRG",
          "source_kind":"nested-fat12-root-prg-text-data",
          "source_sha256":"6666666666666666666666666666666666666666666666666666666666666666"
        }
      ],
      "schema":"project-eon.static-control-flow-set/v1"
    })json";
    const auto static_flow_summary = eon::parse_static_control_flow_sidecar(static_flow_sidecar);
    assert(static_flow_summary.document_count == 2);
    assert(static_flow_summary.range_count == 3);
    assert(static_flow_summary.edge_count == 4);
    assert(static_flow_summary.declared_byte_count == 8);
    assert(static_flow_summary.archive_document_counts.at(
        "0000000000000000000000000000000000000000000000000000000000000000") == 1);
    assert(static_flow_summary.archive_document_counts.at(
        "4444444444444444444444444444444444444444444444444444444444444444") == 1);
    assert(static_flow_summary.release_document_counts.at(
        "0000000000000000000000000000000000000000000000000000000000000000") == 1);
    assert(static_flow_summary.release_document_counts.at(
        "4444444444444444444444444444444444444444444444444444444444444444") == 1);
    assert(static_flow_summary.cpu_counts.at("i8086") == 1);
    assert(static_flow_summary.cpu_counts.at("m68000") == 1);
    assert(static_flow_summary.edge_kind_counts.at("call") == 1);
    assert(static_flow_summary.edge_kind_counts.at("return") == 1);
    assert(static_flow_summary.edge_kind_counts.at("interrupt") == 1);
    assert(static_flow_summary.edge_kind_counts.at("trap") == 1);
    assert(static_flow_summary.target_scope_counts.at("within-declared-range") == 1);
    assert(static_flow_summary.documents.size() == 2);
    assert(static_flow_summary.documents.front().direct_media_set_sha256
        && *static_flow_summary.documents.front().direct_media_set_sha256
            == "8888888888888888888888888888888888888888888888888888888888888888");
    assert(!static_flow_summary.documents.back().direct_media_set_sha256);
    assert(static_flow_summary.declared_ranges.size() == 3);
    auto runtime_default_sidecar = static_flow_sidecar;
    const auto runtime_address_space = runtime_default_sidecar.find("\"address_space\":\"runtime\",");
    assert(runtime_address_space != std::string::npos);
    runtime_default_sidecar.erase(runtime_address_space,
        std::string("\"address_space\":\"runtime\",").size());
    assert(eon::parse_static_control_flow_sidecar(runtime_default_sidecar).document_count == 2);
    for (const std::string malformed : {
             std::string("{}"),
             std::string(static_flow_sidecar).replace(
                 static_flow_sidecar.find("static-candidate-unclassified"),
                 std::string("static-candidate-unclassified").size(), "reachable"),
             std::string(static_flow_sidecar).replace(
                 static_flow_sidecar.find("\"target_scope\":\"within-declared-range\""),
                 std::string("\"target_scope\":\"within-declared-range\"").size(),
                 "\"target_scope\":\"outside-declared-range\""),
             std::string(static_flow_sidecar).replace(
                 static_flow_sidecar.find("\"source_kind\":\"verified-direct-media-member\""), 0,
                 "\"unexpected\":true,\"source_kind\":\"verified-direct-media-member\""),
             std::string(static_flow_sidecar).replace(
                 static_flow_sidecar.find("\"direct_media_set_sha256\":\"8888888888888888888888888888888888888888888888888888888888888888\","),
                 std::string("\"direct_media_set_sha256\":\"8888888888888888888888888888888888888888888888888888888888888888\",").size(), "")}) {
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_static_control_flow_sidecar(malformed));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }
    const auto four_by_three = eon::fit_display_aspect_viewport(64.0F, 250.0F,
        576.0F, 400.0F, 4.0F / 3.0F);
    assert(std::fabs(four_by_three.x - 85.333336F) < 0.001F);
    assert(four_by_three.y == 250.0F);
    assert(std::fabs(four_by_three.width - 533.333313F) < 0.001F);
    assert(four_by_three.height == 400.0F);
    const auto wide = eon::fit_display_aspect_viewport(64.0F, 250.0F,
        576.0F, 400.0F, 16.0F / 9.0F);
    assert(wide.x == 64.0F);
    assert(std::fabs(wide.y - 288.0F) < 0.001F);
    assert(wide.width == 576.0F);
    assert(std::fabs(wide.height - 324.0F) < 0.001F);
    for (const auto invalid_ratio : {0.0F, -1.0F}) {
        bool rejected = false;
        try {
            static_cast<void>(eon::fit_display_aspect_viewport(0.0F, 0.0F,
                1.0F, 1.0F, invalid_ratio));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }
  const auto millennium_startup = eon::startup_boundary_for_release(
      "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123");
  assert(millennium_startup.has_value());
  assert(millennium_startup->parser_profile_id == "millennium-dos-launcher");
  assert(millennium_startup->source_address == "MILL.COM+0x0");
  const std::array expected_startup_boundaries{
      std::pair{"b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4", "millennium-dos-spanish-startup"},
      std::pair{"2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400", "millennium-amiga-defjam-bootstrap"},
      std::pair{"ec0424445d494809d2661492e289af71b056a429dde13b053a472ccc8347d4dd", "millennium-amiga-defjam-direct-bootstrap"},
      std::pair{"0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69", "millennium-atari-equinox-direct-bootstrap"},
      std::pair{"ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01", "millennium-atari-equinox-bootstrap"},
      std::pair{"f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04", "deuteros-amiga-clean-main-stage"},
      std::pair{"c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653", "deuteros-atari-replicants-first-stage"},
  };
  for (const auto& [release_sha256, parser_profile_id] : expected_startup_boundaries) {
    const auto boundary = eon::startup_boundary_for_release(release_sha256);
    assert(boundary.has_value());
    assert(boundary->parser_profile_id == parser_profile_id);
  }
  assert(!eon::startup_boundary_for_release(
      "0000000000000000000000000000000000000000000000000000000000000000"));

    // These synthetic strings exercise only the strict external-record
    // grammar. They are not game data or a capture fixture and never invoke
    // a DOS service, alter media, or advance a game session.
    {
        constexpr std::string_view valid_events =
            "event\t1 10 interrupt image=mill.com pc=0x0209 int=0x21 ax=0x2591 dx=0x0000\n"
            "event\t2 20 file image=mill.com pc=0x02cf op=driver-load path=mcga.bin\n"
            "event\t3 30 exec image=mill.com pc=0x0337 int=0x21 ax=0x4b00 path=titles.exe\n"
            "event\t4 40 interrupt image=titles.exe pc=0x0127 int=0x91 ax=0x0000 es=cs bx=0x1ac4\n"
            "event\t5 50 interrupt image=2200ad.exe pc=0x0124 int=0x91 ax=0x001f es=cs bx=0xd19e\n";
        eon::MillenniumDosEnglishReferenceTraceDiagnostics diagnostics;
        std::string trace_error;
        assert(eon::validate_millennium_dos_english_reference_events(
            valid_events, diagnostics, trace_error));
        assert(diagnostics.event_count == 5 && diagnostics.interrupt_count == 3
            && diagnostics.file_count == 1 && diagnostics.exec_count == 1);
        assert(!eon::validate_millennium_dos_english_reference_events(
            "event\t1 10 interrupt image=mill.com pc=0x0209 int=0x21 ax=0x2591 dx=0x0001\n",
            diagnostics, trace_error));
        assert(!eon::validate_millennium_dos_english_reference_events(
            "event\t1 10 exec image=mill.com pc=0x0337 int=0x21 ax=0x4b00 path=titles.exe\n"
            "event\t1 20 exec image=mill.com pc=0x0337 int=0x21 ax=0x4b00 path=2200ad.exe\n",
            diagnostics, trace_error));
        assert(!eon::validate_millennium_dos_english_reference_events(
            "event\t1 10 interrupt image=2200ad.exe pc=0x0124 int=0x91 ax=0x001f es=cs bx=0xd19f\n",
            diagnostics, trace_error));
    }
    // This exact five-record schema retains a real title-wrapper return
    // observation as diagnostics only. It never turns either AX word into a
    // title-session input or a private-driver ABI implementation.
    {
        constexpr std::string_view valid_events =
            "event\t1 1 file image=mill.com pc=0x02cf op=driver-load path=mcga.bin\n"
            "event\t2 2 interrupt image=mill.com pc=0x0209 int=0x21 ax=0x2591 dx=0x0000\n"
            "event\t3 3 interrupt image=titles.exe pc=0x0127 int=0x91 ax=0x0000 es=cs bx=0x1ac4\n"
            "event\t4 4 private-return image=titles.exe pc=0x0129 int=0x91 ax=0x0101\n"
            "event\t5 5 private-return image=titles.exe pc=0x0129 int=0x91 ax=0x0000\n";
        eon::MillenniumDosTitleInitReferenceTraceDiagnostics diagnostics;
        std::string trace_error;
        assert(eon::validate_millennium_dos_title_init_reference_events(
            valid_events, diagnostics, trace_error));
        assert(diagnostics.event_count == 5 && diagnostics.interrupt_count == 2
            && diagnostics.file_count == 1 && diagnostics.private_return_count == 2);
        assert(!eon::validate_millennium_dos_title_init_reference_events(
            "event\t1 1 file image=mill.com pc=0x02cf op=driver-load path=mcga.bin\n"
            "event\t2 2 interrupt image=mill.com pc=0x0209 int=0x21 ax=0x2591 dx=0x0000\n"
            "event\t3 3 interrupt image=titles.exe pc=0x0127 int=0x91 ax=0x0000 es=cs bx=0x1ac4\n"
            "event\t4 4 private-return image=titles.exe pc=0x0129 int=0x91 ax=0x0000\n"
            "event\t5 5 private-return image=titles.exe pc=0x0129 int=0x91 ax=0x0101\n",
            diagnostics, trace_error));
    }
    // These records exercise the GX trace grammar only. They are not game
    // data or a capture fixture; validation reports opaque provenance and
    // never starts a GX session or supplies any observed value to one.
    {
        constexpr std::string_view valid_events =
            "event\t1 10 private-return image=2200ad.exe pc=0x0129 int=0x91 ax=0x0000\n"
            "event\t2 20 mode-read image=2200ad.exe pc=0xd349 address=0xda05 value=0x03\n"
            "event\t3 30 adapter-return image=2200gx.exe pc=0x00ed op=retf return_pc=0xd376\n"
            "event\t4 40 local-return image=2200ad.exe call_pc=0xd376 return_pc=0xd379\n"
            "event\t5 50 local-return image=2200ad.exe call_pc=0xd379 return_pc=0xd37c\n"
            "event\t6 60 local-return image=2200ad.exe call_pc=0xd37c return_pc=0xd37f\n"
            "event\t7 70 local-return image=2200ad.exe call_pc=0xd37f return_pc=0xd382\n"
            "event\t8 80 local-return image=2200ad.exe call_pc=0xd382 return_pc=0xd385\n"
            "event\t9 90 local-return image=2200ad.exe call_pc=0xd385 return_pc=0xd388\n"
            "event\t10 100 mode-read image=2200ad.exe pc=0xd388 address=0xda05 value=0x01\n";
        eon::MillenniumDosGxStartupReferenceTraceDiagnostics diagnostics;
        std::string trace_error;
        assert(eon::validate_millennium_dos_gx_startup_reference_events(
            valid_events, diagnostics, trace_error));
        assert(diagnostics.event_count == 10 && diagnostics.private_return_count == 1
            && diagnostics.mode_read_count == 2 && diagnostics.adapter_return_count == 1
            && diagnostics.local_return_count == 6);
        assert(!eon::validate_millennium_dos_gx_startup_reference_events(
            "event\t1 10 private-return image=2200ad.exe pc=0x0129 int=0x91 ax=0x0000\n",
            diagnostics, trace_error));
        assert(!eon::validate_millennium_dos_gx_startup_reference_events(
            "event\t1 10 private-return image=2200ad.exe pc=0x0129 int=0x91 ax=0X0000\n"
            "event\t2 20 mode-read image=2200ad.exe pc=0xd349 address=0xda05 value=0x03\n"
            "event\t3 30 adapter-return image=2200gx.exe pc=0x00ed op=retf return_pc=0xd376\n"
            "event\t4 40 local-return image=2200ad.exe call_pc=0xd376 return_pc=0xd379\n"
            "event\t5 50 local-return image=2200ad.exe call_pc=0xd379 return_pc=0xd37c\n"
            "event\t6 60 local-return image=2200ad.exe call_pc=0xd37c return_pc=0xd37f\n"
            "event\t7 70 local-return image=2200ad.exe call_pc=0xd37f return_pc=0xd382\n"
            "event\t8 80 local-return image=2200ad.exe call_pc=0xd382 return_pc=0xd385\n"
            "event\t9 90 local-return image=2200ad.exe call_pc=0xd385 return_pc=0xd388\n"
            "event\t10 100 mode-read image=2200ad.exe pc=0xd388 address=0xda05 value=0x01\n",
            diagnostics, trace_error));
        const auto observations = eon::parse_millennium_dos_gx_startup_reference_observations(
            valid_events, trace_error);
        assert(observations && observations->private_return_ax == 0x0000
            && observations->initial_mode_byte == 0x03 && observations->post_overlay_mode_byte == 0x01);
        const auto rejected_observations = eon::parse_millennium_dos_gx_startup_reference_observations(
            "event\t1 10 private-return image=2200ad.exe pc=0x0129 int=0x91 ax=0x0000\n", trace_error);
        assert(!rejected_observations);
    }
    // These two records name only the verified caller-side handoffs. They do
    // not model execution, read completion, or a return from the opaque stage.
    {
        constexpr std::string_view valid_events =
            "event\t1 10 cpu image=bootstrap-loader pc=0x702e4 op=jsr-indirect a3=0x41000\n"
            "event\t2 20 cpu image=bootstrap-loader pc=0x70320 op=jmp-indirect a3=0x68000 d6=0xa8d398fb\n";
        eon::MillenniumAmigaReferenceTraceDiagnostics diagnostics;
        std::string trace_error;
        assert(eon::validate_millennium_amiga_english_reference_events(
            valid_events, diagnostics, trace_error));
        assert(diagnostics.event_count == 2 && diagnostics.cpu_count == 2);
        assert(!eon::validate_millennium_amiga_english_reference_events(
            "event\t1 10 cpu image=bootstrap-loader pc=0x702e4 op=jsr-indirect a3=0x41001\n",
            diagnostics, trace_error));
    }
    // These synthetic strings exercise the external-record grammar only.
    // They are not Atari media, an emulator trace fixture, or a request to
    // call XBIOS or replay any boot state.
    {
        constexpr std::string_view valid_events =
            "event\t1 10 trap pc=0x00001edc incoming_a7=0x00001000 incoming_sr=0x2700 selector=0x0026 callback=0x00001fa6 return_pc=0x00001ede return_a7=0x0000100c return_sr=0x2000 return_d0=0x00000000\n"
            "event\t2 20 callback entry_pc=0x00001fa6 incoming_a7=0x00001000 stack_longword=0x00001ede outgoing_a7=0x0007b000 return_pc=0x00001ede return_a7=0x0007affc return_sr=0x2000 return_d0=0x00001ede\n"
            "event\t3 30 state ram_25f4=0x00071100 ram_25f4_provenance=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa ram_25fc=0x00000001 ram_25fc_provenance=bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb branch_pc=0x00001ef2 state_word=0x0001\n"
            "event\t4 40 table base=0x00001eac shifted_index=0x0002 target_a1=0x00001f50 entry_pc=0x00001f50 return_pc=0x00001f08 return_d1=0x00000000 return_d2=0x00000000\n"
            "event\t5 50 frame site=0x00001e9c input_frame=0008 result_frame=00000000\n"
            "event\t6 60 raw-reader entry_pc=0x00001e60 trap_pc=0x00001e9c call_a7=0x00002000 return_pc=0x00001e9e return_a7=0x00002014 return_sr=0x2000 return_d0=0x00000000\n";
        eon::DeuterosAtariReferenceTraceDiagnostics diagnostics;
        std::string trace_error;
        assert(eon::validate_deuteros_atari_reference_events(valid_events, diagnostics, trace_error));
        assert(diagnostics.event_count == 6 && diagnostics.trap_count == 1 && diagnostics.callback_count == 1
            && diagnostics.frame_count == 1 && diagnostics.state_count == 1 && diagnostics.table_count == 1
            && diagnostics.raw_reader_count == 1);
        assert(!eon::validate_deuteros_atari_reference_events(
            "event\t1 10 trap pc=0x00001edc incoming_a7=0x00001000 incoming_sr=0x2700 selector=0x0026 callback=0x00001fa6 return_pc=0x00001ede return_a7=0x0000100c return_sr=0x2000 return_d0=0x00000000 extra=forbidden\n",
            diagnostics, trace_error));
        assert(!eon::validate_deuteros_atari_reference_events(
            "event\t1 10 table base=0x00001eac shifted_index=0x0002 target_a1=0x00001f51 entry_pc=0x00001f51 return_pc=0x00001f08 return_d1=0x00000000 return_d2=0x00000000\n",
            diagnostics, trace_error));
    }
    // These strings validate only declared title-stage ABI observations. They
    // are not Amiga media, a trace fixture, or a request to execute an ABI.
    {
        constexpr std::string_view valid_events =
            "event\t1 10 exec site=0x00040450 exec_base_address=0x00000004 vector=-0x0096 result_d0=0x00000000 result_sr=0x2000\n"
            "event\t2 20 open-library site=0x0001ed80 name_address=0x0001ed02 exec_base_address=0x00000004 vector=-0x0228 result_d0=0x00012fec result_sr=0x2000\n"
            "event\t3 30 graphics site=0x0004069a graphics_base_address=0x00012fec vector=-0x00c0 result_d0=0x00000000 result_sr=0x2000\n"
            "event\t4 40 custom-register site=0x0004046c base=0x00dff000 offset=0x0040 value=0x7fff result_d0=0x00000000 result_sr=0x2000\n"
            "event\t5 50 callback site=0x0001ef74 callback=0x0001f056 exec_base_address=0x00000004 vector=-0x01ce result_d0=0x00000000 result_sr=0x2000\n"
            "event\t6 60 callback site=0x0001f056 incoming_a0=0x00001000 result_d0=0x00000001 result_sr=0x2000\n";
        eon::DeuterosAmigaReferenceTraceDiagnostics diagnostics;
        std::string trace_error;
        assert(eon::validate_deuteros_amiga_title_reference_events(valid_events, diagnostics, trace_error));
        assert(diagnostics.event_count == 6 && diagnostics.exec_count == 1
            && diagnostics.open_library_count == 1 && diagnostics.graphics_count == 1
            && diagnostics.custom_register_count == 1 && diagnostics.callback_count == 2);
        assert(!eon::validate_deuteros_amiga_title_reference_events(
            "event\t1 10 graphics site=0x0004069a graphics_base_address=0x00012fed vector=-0x00c0 result_d0=0x00000000 result_sr=0x2000\n",
            diagnostics, trace_error));
    }
    // This is one raw debugger observation from the clean main stage. It
    // deliberately records no inferred registers, copying semantics, or
    // gameplay state; overlapping Amiga RAM makes those claims unsafe here.
    {
        constexpr std::string_view valid_events =
            "event\t1 10 main-copy-loop-pc pc=0x000210d4 opcode=0x51c8\n";
        eon::DeuterosAmigaMainStageReferenceTraceDiagnostics diagnostics;
        std::string trace_error;
        assert(eon::validate_deuteros_amiga_main_stage_reference_events(
            valid_events, diagnostics, trace_error));
        assert(diagnostics.event_count == 1 && diagnostics.main_copy_loop_pc_count == 1);
        assert(!eon::validate_deuteros_amiga_main_stage_reference_events(
            "event\t1 10 main-copy-loop-pc pc=0x000210d4 opcode=0x51c9\n",
            diagnostics, trace_error));
        assert(!eon::validate_deuteros_amiga_main_stage_reference_events(
            "event\t1 10 main-copy-loop-pc pc=0x000210d4 opcode=0x51c8\n"
            "event\t2 20 main-copy-loop-pc pc=0x000210d4 opcode=0x51c8\n",
            diagnostics, trace_error));
    }
    // These synthetic records exercise the v3 title-bridge capture grammar
    // only. They are neither Amiga media nor an executable ABI transcript.
    {
        constexpr std::string_view valid_events =
            "event\t1 10 exec-return site=0x00040450 exec_base_address=0x00000004 vector=-0x0096 result_d0=0x00000000 result_sr=0x2000\n"
            "event\t2 20 exec-return site=0x00040450 exec_base_address=0x00000004 vector=-0x009c result_d0=0x00000000 result_sr=0x2000\n"
            "event\t3 30 open-library-return site=0x0001ed80 name_address=0x0001ed02 exec_base_address=0x00000004 vector=-0x0228 result_d0=0x00012fec result_sr=0x2000\n"
            "event\t4 40 graphics-call site=0x0004069a graphics_base_address=0x00012fec vector=-0x00c0\n"
            "event\t5 50 custom-register-call site=0x0004046c base=0x00dff000 offset=0x0040 value=0x7fff\n"
            "event\t6 60 custom-register-return site=0x0004046c base=0x00dff000 offset=0x0040 value=0x7fff result_d0=0x00000000 result_sr=0x2000\n"
            "event\t7 70 graphics-return site=0x0004069a graphics_base_address=0x00012fec vector=-0x00c0 result_d0=0x00000000 result_sr=0x2000\n"
            "event\t8 80 callback-registration-return site=0x0001ef74 callback=0x0001f056 exec_base_address=0x00000004 vector=-0x01ce result_d0=0x00000000 result_sr=0x2000\n"
            "event\t9 90 queue-snapshot phase=pre queue_address=0x0001eec0 queue_bytes=0000000000000000000000000000000000000000 pending_address=0x0001eed6 pending_word=0x0000 source_table_address=0x0001ee20 source_table_size=160 source_table_sha256=2f00ffdf05ab28379e97e91e98fa764e45769d7ea55363846543becf7552e265\n"
            "event\t10 100 callback-entry site=0x0001f056 incoming_a0=0x00001000 frame_04_0d=00000000000000000000\n"
            "event\t11 110 queue-snapshot phase=post queue_address=0x0001eec0 queue_bytes=0000000000000000000000000000000000000000 pending_address=0x0001eed6 pending_word=0x0000 source_table_address=0x0001ee20 source_table_size=160 source_table_sha256=2f00ffdf05ab28379e97e91e98fa764e45769d7ea55363846543becf7552e265\n"
            "event\t12 120 selector-entry site=0x0001fe7a incoming_d0=0x00000000\n"
            "event\t13 130 local-call call_site=0x0001fe84 callee=0x0001fea8 return_pc=0x0001fe88\n"
            "event\t14 140 local-return call_site=0x0001fe84 callee=0x0001fea8 return_pc=0x0001fe88 result_d0=0x00000000 result_sr=0x2000\n"
            "event\t15 150 local-call call_site=0x0001fe92 callee=0x0001fea8 return_pc=0x0001fe96\n"
            "event\t16 160 local-return call_site=0x0001fe92 callee=0x0001fea8 return_pc=0x0001fe96 result_d0=0x00000000 result_sr=0x2000\n"
            "event\t17 170 dispatch-snapshot phase=pre site=0x0001fbe6 cell_1f98c=0x00 cell_1f98e=0x00 cell_1f99c=0x00000000 cell_1f974=0x00000000 cell_1f970=0x00000000 cell_1f96c=0x00000000 cell_1f994=0x00000000 cell_1f998=0x00000000\n"
            "event\t18 180 dispatch-snapshot phase=post site=0x0001fbe6 cell_1f98c=0x00 cell_1f98e=0x00 cell_1f99c=0x00000000 cell_1f974=0x00000000 cell_1f970=0x00000000 cell_1f96c=0x00000000 cell_1f994=0x00000000 cell_1f998=0x00000000\n";
        eon::DeuterosAmigaTitleBridgeReferenceTraceDiagnostics diagnostics;
        std::string trace_error;
        assert(eon::validate_deuteros_amiga_title_bridge_reference_events(valid_events, diagnostics, trace_error));
        assert(diagnostics.event_count == 18 && diagnostics.exec_return_count == 2
            && diagnostics.open_library_return_count == 1 && diagnostics.graphics_call_count == 1
            && diagnostics.graphics_return_count == 1 && diagnostics.custom_register_call_count == 1
            && diagnostics.custom_register_return_count == 1 && diagnostics.callback_registration_return_count == 1
            && diagnostics.queue_snapshot_count == 2 && diagnostics.callback_entry_count == 1
            && diagnostics.selector_entry_count == 1 && diagnostics.local_call_count == 2
            && diagnostics.local_return_count == 2 && diagnostics.dispatch_snapshot_count == 2);
        assert(!eon::validate_deuteros_amiga_title_bridge_reference_events(
            "event\t1 10 exec-return site=0x00040450 exec_base_address=0x00000004 vector=-0x0096 result_d0=0x00000000 result_sr=0x2000\n"
            "event\t2 20 exec-return site=0x00040450 exec_base_address=0x00000004 vector=-0x009c result_d0=0x00000000 result_sr=0x2000\n"
            "event\t3 30 open-library-return site=0x0001ed80 name_address=0x0001ed02 exec_base_address=0x00000004 vector=-0x0228 result_d0=0x00012fec result_sr=0x2000\n"
            "event\t4 40 graphics-return site=0x0004069a graphics_base_address=0x00012fec vector=-0x00c0 result_d0=0x00000000 result_sr=0x2000\n",
            diagnostics, trace_error));
        // A graphics return cannot be reassigned across a later outstanding
        // custom-register call merely because each individual record is
        // otherwise well formed.
        assert(!eon::validate_deuteros_amiga_title_bridge_reference_events(
            "event\t1 10 exec-return site=0x00040450 exec_base_address=0x00000004 vector=-0x0096 result_d0=0x00000000 result_sr=0x2000\n"
            "event\t2 20 exec-return site=0x00040450 exec_base_address=0x00000004 vector=-0x009c result_d0=0x00000000 result_sr=0x2000\n"
            "event\t3 30 open-library-return site=0x0001ed80 name_address=0x0001ed02 exec_base_address=0x00000004 vector=-0x0228 result_d0=0x00012fec result_sr=0x2000\n"
            "event\t4 40 graphics-call site=0x0004069a graphics_base_address=0x00012fec vector=-0x00c0\n"
            "event\t5 50 custom-register-call site=0x0004046c base=0x00dff000 offset=0x0040 value=0x7fff\n"
            "event\t6 60 graphics-return site=0x0004069a graphics_base_address=0x00012fec vector=-0x00c0 result_d0=0x00000000 result_sr=0x2000\n",
            diagnostics, trace_error));
        // The fixed display values are genuine debugger observations. The
        // all-zero future hash fields below exercise grammar only; they are
        // not pixels, audio, or a title-stage replay fixture.
        std::string title_display_events{valid_events};
        title_display_events +=
            "event\t19 190 display-layout site=0x0001eda6 base_source_address=0x00012ff4 base_destination_a=0x0001f168 base_destination_b=0x0001f164 display_base=0x0000ab00 display_list=0x00000420 copper_list_sha256=cf827847c13dbeafeea72c86f2c4fb90a6d717bf548f0914b2f203abb94293f6\n"
            "event\t20 200 bitplane-layout site=0x0001f182 base_pointer_address=0x0001f168 bitplane_count=0x0004 plane0=0x0000b5f0 plane1=0x0000d530 plane2=0x0000f470 plane3=0x000113b0 plane_stride=0x1f40 bplcon0=0x4200 bpl1mod=0x0000 bpl2mod=0x0000 ddfstrt=0x0038 ddfstop=0x00d0 width_pixels=0x0140 height_lines=0x00c8 bytes_per_row=0x0028 modulo=0x0000\n"
            "event\t21 210 palette-checkpoint site=0x0001eda6 source_address=0x0001ed24 destination_address=0x00012ecc word_count=0x0014 rgb4_sha256=5903a1c83619d7667c04ac1f3c923dfaa3a1ce0d090d6fd95109616a9b506a55 rgba_palette_format=rgba8888-rgb4-expanded-nibbles rgba_palette_sha256=0000000000000000000000000000000000000000000000000000000000000000\n"
            "event\t22 220 input-checkpoint callback_site=0x0001f056 selector_site=0x0001fe7a queue_sha256=0000000000000000000000000000000000000000000000000000000000000000 input_timeline_sha256=0000000000000000000000000000000000000000000000000000000000000000\n"
            "event\t23 230 frame-checkpoint display_base=0x0000ab00 rgba_width=0x0140 rgba_height=0x00c8 rgba_format=rgba8888-row-major bitplanes_sha256=fad588ff5f6e0ec471cb4889987dab4a40c11d7da6e532564d48475149c68490 rgba_sha256=0000000000000000000000000000000000000000000000000000000000000000\n"
            "event\t24 240 audio-checkpoint sample_rate=0x00002710 channels=0x02 sample_frames=0x00000001 pcm_format=s16le-interleaved pcm_sha256=0000000000000000000000000000000000000000000000000000000000000000\n";
        eon::DeuterosAmigaTitleDisplayReferenceTraceDiagnostics display_diagnostics;
        assert(eon::validate_deuteros_amiga_title_display_reference_events(
            title_display_events, display_diagnostics, trace_error));
        assert(display_diagnostics.event_count == 24
            && display_diagnostics.bridge_event_count == 18
            && display_diagnostics.display_layout_count == 1
            && display_diagnostics.bitplane_layout_count == 1
            && display_diagnostics.palette_checkpoint_count == 1
            && display_diagnostics.input_checkpoint_count == 1
            && display_diagnostics.frame_checkpoint_count == 1
            && display_diagnostics.audio_checkpoint_count == 1
            && display_diagnostics.copper_list_sha256
                == "cf827847c13dbeafeea72c86f2c4fb90a6d717bf548f0914b2f203abb94293f6"
            && display_diagnostics.rgb4_palette_sha256
                == "5903a1c83619d7667c04ac1f3c923dfaa3a1ce0d090d6fd95109616a9b506a55"
            && display_diagnostics.bitplanes_sha256
                == "fad588ff5f6e0ec471cb4889987dab4a40c11d7da6e532564d48475149c68490"
            && display_diagnostics.rgba_palette_sha256 == std::string(64, '0')
            && display_diagnostics.rgba_sha256 == std::string(64, '0')
            && display_diagnostics.audio_sample_rate == "0x00002710"
            && display_diagnostics.audio_channels == "0x02"
            && display_diagnostics.audio_sample_frames == "0x00000001"
            && display_diagnostics.pcm_sha256 == std::string(64, '0'));
        assert(!eon::validate_deuteros_amiga_title_display_reference_events(
            title_display_events, display_diagnostics, trace_error,
            "1111111111111111111111111111111111111111111111111111111111111111"));
        auto restarted_suffix{title_display_events};
        restarted_suffix.replace(restarted_suffix.find("event\t19 190"),
            std::string_view("event\t19 190").size(), "event\t18 190");
        assert(!eon::validate_deuteros_amiga_title_display_reference_events(
            restarted_suffix, display_diagnostics, trace_error));
        auto unspecified_pixel_format{title_display_events};
        unspecified_pixel_format.replace(unspecified_pixel_format.find("rgba8888-row-major"),
            std::string_view("rgba8888-row-major").size(), "rgba8888-unknown");
        assert(!eon::validate_deuteros_amiga_title_display_reference_events(
            unspecified_pixel_format, display_diagnostics, trace_error));
        auto unspecified_pcm_format{title_display_events};
        unspecified_pcm_format.replace(unspecified_pcm_format.find("s16le-interleaved"),
            std::string_view("s16le-interleaved").size(), "f32le-interleaved");
        assert(!eon::validate_deuteros_amiga_title_display_reference_events(
            unspecified_pcm_format, display_diagnostics, trace_error));
        auto zero_audio_frames{title_display_events};
        zero_audio_frames.replace(zero_audio_frames.find("sample_frames=0x00000001"),
            std::string_view("sample_frames=0x00000001").size(), "sample_frames=0x00000000");
        assert(!eon::validate_deuteros_amiga_title_display_reference_events(
            zero_audio_frames, display_diagnostics, trace_error));
        title_display_events.replace(title_display_events.find("site=0x0001eda6"),
            std::string_view("site=0x0001eda6").size(), "site=0x0001eda7");
        assert(!eon::validate_deuteros_amiga_title_display_reference_events(
            title_display_events, display_diagnostics, trace_error));
        title_display_events = valid_events;
        title_display_events +=
            "event\t19 190 display-layout site=0x0001eda6 base_source_address=0x00012ff4 base_destination_a=0x0001f168 base_destination_b=0x0001f164 display_base=0x0000ab00 display_list=0x00000420 copper_list_sha256=cf827847c13dbeafeea72c86f2c4fb90a6d717bf548f0914b2f203abb94293f6\n"
            "event\t20 200 bitplane-layout site=0x0001f182 base_pointer_address=0x0001f168 bitplane_count=0x0004 plane0=0x0000b5f0 plane1=0x0000d530 plane2=0x0000f470 plane3=0x000113b0 plane_stride=0x1f40 bplcon0=0x4200 bpl1mod=0x0000 bpl2mod=0x0000 ddfstrt=0x0038 ddfstop=0x00d0 width_pixels=0x0141 height_lines=0x00c8 bytes_per_row=0x0028 modulo=0x0000\n"
            "event\t21 210 palette-checkpoint site=0x0001eda6 source_address=0x0001ed24 destination_address=0x00012ecc word_count=0x0014 rgb4_sha256=5903a1c83619d7667c04ac1f3c923dfaa3a1ce0d090d6fd95109616a9b506a55 rgba_palette_format=rgba8888-rgb4-expanded-nibbles rgba_palette_sha256=0000000000000000000000000000000000000000000000000000000000000000\n"
            "event\t22 220 input-checkpoint callback_site=0x0001f056 selector_site=0x0001fe7a queue_sha256=0000000000000000000000000000000000000000000000000000000000000000 input_timeline_sha256=0000000000000000000000000000000000000000000000000000000000000000\n"
            "event\t23 230 frame-checkpoint display_base=0x0000ab00 rgba_width=0x0140 rgba_height=0x00c8 rgba_format=rgba8888-row-major bitplanes_sha256=fad588ff5f6e0ec471cb4889987dab4a40c11d7da6e532564d48475149c68490 rgba_sha256=0000000000000000000000000000000000000000000000000000000000000000\n"
            "event\t24 240 audio-checkpoint sample_rate=0x00002710 channels=0x02 sample_frames=0x00000000 pcm_format=s16le-interleaved pcm_sha256=0000000000000000000000000000000000000000000000000000000000000000\n";
        assert(!eon::validate_deuteros_amiga_title_display_reference_events(
            title_display_events, display_diagnostics, trace_error));
        title_display_events.replace(title_display_events.find("width_pixels=0x0141"),
            std::string_view("width_pixels=0x0141").size(), "width_pixels=0x0140");
        title_display_events.replace(title_display_events.find("plane_stride=0x1f40"),
            std::string_view("plane_stride=0x1f40").size(), "plane_stride=0x1f41");
        assert(!eon::validate_deuteros_amiga_title_display_reference_events(
            title_display_events, display_diagnostics, trace_error));
        title_display_events.replace(title_display_events.find("plane_stride=0x1f41"),
            std::string_view("plane_stride=0x1f41").size(), "plane_stride=0x1f40");
        title_display_events.replace(title_display_events.find("rgba_width=0x0140"),
            std::string_view("rgba_width=0x0140").size(), "rgba_width=0x0141");
        assert(!eon::validate_deuteros_amiga_title_display_reference_events(
            title_display_events, display_diagnostics, trace_error));
    }
    assert_original_data_source_classification();
    assert_modern_asset_pack_admission();
    // Modern reconstruction cache identity is renderer-only and complete:
    // a release, source, VM tick or reconstruction-mode change must never
    // reuse a derived texture. It deliberately contains no source path,
    // input, save or original media bytes.
    const eon::ModernReconstructionCacheKey reconstruction_key{
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "deuteros.amiga.opening", 7, eon::ModernPixelReconstruction::scale2x};
    assert(reconstruction_key == reconstruction_key);
    assert((reconstruction_key != eon::ModernReconstructionCacheKey{
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "deuteros.amiga.opening", 7, eon::ModernPixelReconstruction::scale4x}));
    assert((reconstruction_key != eon::ModernReconstructionCacheKey{
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "deuteros.amiga.opening", 8, eon::ModernPixelReconstruction::scale2x}));
    assert((reconstruction_key != eon::ModernReconstructionCacheKey{
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
        "deuteros.amiga.opening", 7, eon::ModernPixelReconstruction::scale2x}));
    assert((reconstruction_key != eon::ModernReconstructionCacheKey{
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "millennium.dos.title", 7, eon::ModernPixelReconstruction::scale2x}));
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
    const auto reconstructed_4x = eon::reconstruct_rgba_scale4x(reconstruction_source, 3, 3);
    assert(reconstructed_4x.width == 12 && reconstructed_4x.height == 12);
    assert(reconstructed_4x.rgba.size() == 576);
    assert(reconstruction_source == source_copy);
    bool malformed_reconstruction_rejected = false;
    try {
        static_cast<void>(eon::reconstruct_rgba_scale2x(reconstruction_source, 2, 3));
    } catch (const std::runtime_error&) {
        malformed_reconstruction_rejected = true;
    }
    assert(malformed_reconstruction_rejected);
    // The presentation pipeline owns only transient reconstructed pixels. It
    // revokes them whenever the complete renderer source identity changes;
    // a failed source is remembered for that key and cannot fall back to a
    // previous release/frame/mode surface.
    eon::ModernPresentationPipeline reconstruction_pipeline;
    const auto* pipeline_surface = reconstruction_pipeline.resolve(reconstruction_key,
        reconstruction_source, 3, 3);
    assert(pipeline_surface && pipeline_surface->width == 6 && pipeline_surface->height == 6);
    assert(reconstruction_pipeline.matches(reconstruction_key));
    assert(!reconstruction_pipeline.failure());
    const auto changed_pipeline_key = eon::ModernReconstructionCacheKey{
        reconstruction_key.release_sha256, reconstruction_key.source_id, 8,
        eon::ModernPixelReconstruction::scale4x};
    const auto* changed_pipeline_surface = reconstruction_pipeline.resolve(changed_pipeline_key,
        reconstruction_source, 3, 3);
    assert(changed_pipeline_surface && changed_pipeline_surface->width == 12
        && changed_pipeline_surface->height == 12);
    const auto malformed_pipeline_key = eon::ModernReconstructionCacheKey{
        reconstruction_key.release_sha256, reconstruction_key.source_id, 9,
        eon::ModernPixelReconstruction::scale2x};
    assert(!reconstruction_pipeline.resolve(malformed_pipeline_key, reconstruction_source, 2, 3));
    assert(reconstruction_pipeline.matches(malformed_pipeline_key));
    assert(reconstruction_pipeline.failure());
    reconstruction_pipeline.reset();
    assert(!reconstruction_pipeline.attempted_key() && !reconstruction_pipeline.failure());
    // Release-card pagination is presentation state only. It keeps the
    // complete identity-list index stable across page boundaries, so the SDL
    // pointer route cannot reinterpret a visible card as a page-local release.
    const auto empty_release_page = eon::release_card_page_for_focus(0, 0);
    assert(empty_release_page.first_identity == 0 && empty_release_page.visible_count == 0
        && empty_release_page.page == 0 && empty_release_page.page_count == 0);
    const auto first_release_page = eon::release_card_page_for_focus(5, 0);
    assert(first_release_page.first_identity == 0 && first_release_page.visible_count == 4
        && first_release_page.page == 0 && first_release_page.page_count == 2);
    const auto second_release_page = eon::release_card_page_for_focus(5, 4);
    assert(second_release_page.first_identity == 4 && second_release_page.visible_count == 1
        && second_release_page.page == 1 && second_release_page.page_count == 2);
    const auto bounded_release_page = eon::release_card_page_for_focus(5, 99);
    assert(bounded_release_page.first_identity == 4 && bounded_release_page.visible_count == 1
        && bounded_release_page.page == 1 && bounded_release_page.page_count == 2);
    // Release-page controls are presentation navigation only. They move a
    // whole four-card window without choosing an archive or changing the
    // selected source identity, so a touch/mouse page tap cannot launch a
    // page-local card by accident.
    const std::vector<eon::ReleaseArchive> paged_release_identities{
        {eon::Game::millennium, eon::Platform::amiga, "en",
            "1111111111111111111111111111111111111111111111111111111111111111", {}},
        {eon::Game::millennium, eon::Platform::amiga, "en",
            "2222222222222222222222222222222222222222222222222222222222222222", {}},
        {eon::Game::millennium, eon::Platform::amiga, "en",
            "3333333333333333333333333333333333333333333333333333333333333333", {}},
        {eon::Game::millennium, eon::Platform::amiga, "en",
            "4444444444444444444444444444444444444444444444444444444444444444", {}},
        {eon::Game::millennium, eon::Platform::amiga, "en",
            "5555555555555555555555555555555555555555555555555555555555555555", {}},
    };
    eon::LauncherInteractionController paged_controller;
    paged_controller.session.focus_game(paged_release_identities, eon::Game::millennium);
    assert(paged_controller.session.choose_platform(paged_release_identities, eon::Platform::amiga));
    assert(paged_controller.session.route.page == eon::LauncherPage::releases);
    const auto page_source = paged_controller.source_identity();
    assert(paged_controller.page_releases(paged_release_identities, 1));
    assert(paged_controller.focus.release == 4
        && !paged_controller.session.route.release_is_selected()
        && !paged_controller.source_changed_since(page_source));
    assert(paged_controller.page_releases(paged_release_identities, 1));
    assert(paged_controller.focus.release == 0);
    assert(paged_controller.page_releases(paged_release_identities, -1));
    assert(paged_controller.focus.release == 4);
    assert(paged_controller.activate_card(paged_release_identities, 4)
        == eon::LauncherInteractionEffect::none);
    assert(paged_controller.session.route.release_sha256
        == "5555555555555555555555555555555555555555555555555555555555555555");
    assert(!paged_controller.page_releases(paged_release_identities, 0));
    {
        char program[] = "project-eon";
        char* args[] = {program};
        const auto defaults = eon::parse_command_line(1, args);
        assert(defaults.request && defaults.request->data_directory_is_default);
        assert(!defaults.request->data_directory.empty());
        assert(defaults.request->language == "en");
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
        char game_option[] = "--game";
        char millennium[] = "millennium";
        char* inspect_args[] = {program, inspect_option};
        const auto inspect = eon::parse_command_line(2, inspect_args);
        assert(inspect.request && inspect.request->inspect_data);
        char static_control_flow_option[] = "--static-control-flow-sidecar";
#if defined(_WIN32)
        char external_static_control_flow[] = "C:\\project-eon-cache\\flow.json";
#else
        char external_static_control_flow[] = "/var/cache/project-eon/flow.json";
#endif
        char inspect_json_option[] = "--inspect-json";
        char* static_control_flow_args[] = {program, inspect_json_option,
            static_control_flow_option, external_static_control_flow};
        const auto static_control_flow = eon::parse_command_line(4, static_control_flow_args);
        assert(static_control_flow.request && static_control_flow.request->static_control_flow_sidecar
            && *static_control_flow.request->static_control_flow_sidecar == external_static_control_flow);
        char relative_static_control_flow[] = "flow.json";
        char* relative_static_control_flow_args[] = {program, inspect_json_option,
            static_control_flow_option, relative_static_control_flow};
        assert(!eon::parse_command_line(4, relative_static_control_flow_args).request);
        char* inspect_json_args[] = {program, inspect_json_option};
        const auto inspect_json = eon::parse_command_line(2, inspect_json_args);
        assert(inspect_json.request && inspect_json.request->inspect_data
            && inspect_json.request->inspect_json);
        char* static_control_flow_text_args[] = {program, inspect_option,
            static_control_flow_option, external_static_control_flow};
        assert(!eon::parse_command_line(4, static_control_flow_text_args).request);
        char* static_control_flow_without_inspect_args[] = {program,
            static_control_flow_option, external_static_control_flow};
        assert(!eon::parse_command_line(3, static_control_flow_without_inspect_args).request);
        char inventory_option[] = "--inventory";
        char* inspect_json_inventory_args[] = {program, inspect_json_option, inventory_option};
        assert(!eon::parse_command_line(3, inspect_json_inventory_args).request);
        char inspect_save_option[] = "--inspect-save";
        char inspect_save_path[] = "/original/2200SAVE.I";
        char* inspect_save_args[] = {program, inspect_save_option, inspect_save_path};
        const auto inspect_save = eon::parse_command_line(3, inspect_save_args);
        assert(inspect_save.request && inspect_save.request->inspect_save
            && *inspect_save.request->inspect_save == "/original/2200SAVE.I");
        char* inspect_save_with_game_args[] = {program, inspect_save_option, inspect_save_path,
            game_option, millennium};
        assert(!eon::parse_command_line(5, inspect_save_with_game_args).request);
        char* inspect_save_with_data_args[] = {program, data_option, custom_path,
            inspect_save_option, inspect_save_path};
        assert(!eon::parse_command_line(5, inspect_save_with_data_args).request);
        char presentation_option[] = "--presentation";
        char modern_presentation[] = "modern";
        char* inspect_save_with_presentation_args[] = {program, inspect_save_option, inspect_save_path,
            presentation_option, modern_presentation};
        assert(!eon::parse_command_line(5, inspect_save_with_presentation_args).request);
        char modern_packs_option[] = "--modern-packs";
        char pack_root[] = "separately-installed-modern-packs";
        char* modern_pack_args[] = {program, inspect_option, modern_packs_option, pack_root};
        const auto modern_packs = eon::parse_command_line(4, modern_pack_args);
        assert(modern_packs.request && modern_packs.request->modern_pack_root
            && *modern_packs.request->modern_pack_root == pack_root);
        char* modern_pack_without_inspect_args[] = {program, modern_packs_option, pack_root};
        assert(!eon::parse_command_line(3, modern_pack_without_inspect_args).request);
        char modern_pack_option[] = "--modern-pack";
        char modern_manifest[] = "explicit/pack.eonmodern";
        char modern[] = "modern";
        char platform_option[] = "--platform";
        char dos[] = "dos";
        char* selected_modern_pack_args[] = {program, game_option, millennium, platform_option, dos,
            presentation_option, modern, modern_pack_option, modern_manifest};
        const auto selected_modern_pack = eon::parse_command_line(9, selected_modern_pack_args);
        assert(selected_modern_pack.request && selected_modern_pack.request->modern_pack_manifest
            && *selected_modern_pack.request->modern_pack_manifest == modern_manifest);
        char deuteros[] = "deuteros";
        char deuteros_amiga[] = "amiga";
        char* deuteros_modern_pack_args[] = {program, game_option, deuteros, platform_option, deuteros_amiga,
            presentation_option, modern, modern_pack_option, modern_manifest};
        const auto deuteros_modern_pack = eon::parse_command_line(9, deuteros_modern_pack_args);
        assert(deuteros_modern_pack.request && deuteros_modern_pack.request->modern_pack_manifest
            && *deuteros_modern_pack.request->modern_pack_manifest == modern_manifest);
        char* modern_pack_missing_presentation[] = {program, game_option, millennium, platform_option, dos,
            modern_pack_option, modern_manifest};
        const auto original_pack_ignored = eon::parse_command_line(7, modern_pack_missing_presentation);
        assert(original_pack_ignored.request && original_pack_ignored.request->modern_pack_manifest
            && original_pack_ignored.request->presentation == eon::Presentation::original);
        char* modern_pack_with_inspect[] = {program, inspect_option, game_option, millennium, platform_option, dos,
            presentation_option, modern, modern_pack_option, modern_manifest};
        assert(!eon::parse_command_line(10, modern_pack_with_inspect).request);
        char* targeted_inspect_args[] = {
            program, inspect_option, game_option, millennium, platform_option, dos};
        const auto targeted_inspect = eon::parse_command_line(6, targeted_inspect_args);
        assert(targeted_inspect.request && targeted_inspect.request->inspect_data);
        assert(targeted_inspect.request->game == eon::Game::millennium);
        assert(targeted_inspect.request->platform == eon::Platform::dos);
        char* inspect_game_only_args[] = {program, inspect_option, game_option, millennium};
        assert(eon::parse_command_line(4, inspect_game_only_args).request);
        char* direct_game_without_platform_args[] = {program, game_option, millennium};
        const auto direct_game_without_platform = eon::parse_command_line(3,
            direct_game_without_platform_args);
        assert(!direct_game_without_platform.request);
        assert(direct_game_without_platform.error.find("requires --platform") != std::string::npos);
        char launch_check_option[] = "--launch-check";
        char* launch_check_args[] = {program, game_option, millennium, platform_option, dos,
            launch_check_option};
        const auto launch_check = eon::parse_command_line(6, launch_check_args);
        assert(launch_check.request && launch_check.request->launch_check);
        char launch_check_json_option[] = "--launch-check-json";
        char* launch_check_json_args[] = {program, game_option, millennium, platform_option, dos,
            launch_check_json_option};
        const auto launch_check_json = eon::parse_command_line(6, launch_check_json_args);
        assert(launch_check_json.request && launch_check_json.request->launch_check
            && launch_check_json.request->launch_check_json);
        char runtime_diagnostics_json_option[] = "--runtime-diagnostics-json";
        char* runtime_diagnostics_json_args[] = {program, game_option, millennium,
            platform_option, dos, runtime_diagnostics_json_option};
        const auto runtime_diagnostics_json = eon::parse_command_line(6,
            runtime_diagnostics_json_args);
        assert(runtime_diagnostics_json.request && runtime_diagnostics_json.request->launch_check
            && runtime_diagnostics_json.request->runtime_diagnostics_json
            && !runtime_diagnostics_json.request->launch_check_json);
        char* runtime_diagnostics_without_target_args[] = {program,
            runtime_diagnostics_json_option};
        assert(!eon::parse_command_line(2, runtime_diagnostics_without_target_args).request);
        char* launch_check_without_target_args[] = {program, launch_check_option};
        assert(!eon::parse_command_line(2, launch_check_without_target_args).request);
        char* launch_check_with_inspect_args[] = {program, inspect_option, game_option, millennium,
            platform_option, dos, launch_check_option};
        assert(!eon::parse_command_line(7, launch_check_with_inspect_args).request);
        char release_language_option[] = "--release-language";
        char spanish_release[] = "es";
        char* spanish_release_args[] = {program, game_option, millennium, platform_option, dos,
            release_language_option, spanish_release};
        const auto spanish_release_request = eon::parse_command_line(7, spanish_release_args);
        assert(spanish_release_request.request
            && spanish_release_request.request->release_language == "es");
        char* spanish_inspect_args[] = {program, inspect_option, game_option, millennium,
            platform_option, dos, release_language_option, spanish_release};
        const auto spanish_inspect = eon::parse_command_line(8, spanish_inspect_args);
        assert(spanish_inspect.request && spanish_inspect.request->inspect_data
            && spanish_inspect.request->release_language == "es");
        char invalid_release[] = "sv";
        char* invalid_release_args[] = {program, game_option, millennium, platform_option, dos,
            release_language_option, invalid_release};
        assert(!eon::parse_command_line(7, invalid_release_args).request);
        char* missing_release_scope_args[] = {program, release_language_option, spanish_release};
        assert(!eon::parse_command_line(3, missing_release_scope_args).request);
        char release_sha256_option[] = "--release-sha256";
        char release_sha256[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        char* exact_release_args[] = {program, game_option, millennium, platform_option, dos,
            release_sha256_option, release_sha256};
        const auto exact_release_request = eon::parse_command_line(7, exact_release_args);
        assert(exact_release_request.request
            && exact_release_request.request->release_sha256 == release_sha256);
        char malformed_release_sha256[] = "not-a-sha";
        char* malformed_release_args[] = {program, game_option, millennium, platform_option, dos,
            release_sha256_option, malformed_release_sha256};
        assert(!eon::parse_command_line(7, malformed_release_args).request);
        char* spanish_pack_args[] = {program, game_option, millennium, platform_option, dos,
            presentation_option, modern, release_language_option, spanish_release,
            modern_pack_option, modern_manifest};
        const auto spanish_pack = eon::parse_command_line(11, spanish_pack_args);
        assert(!spanish_pack.request
            && spanish_pack.error.find("no cross-edition art fallback") != std::string::npos);
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
        char trace_json_option[] = "--reference-trace-json";
        char* trace_json_args[] = {program, data_option, custom_path, game_option, millennium,
            platform_option, amiga, trace_option, trace_manifest, trace_json_option};
        const auto trace_json = eon::parse_command_line(10, trace_json_args);
        assert(trace_json.request && trace_json.request->reference_trace_json);
        char* trace_json_without_trace[] = {program, trace_json_option};
        assert(!eon::parse_command_line(2, trace_json_without_trace).request);
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
        assert((eon::supported_platforms(eon::Game::millennium)
            == std::vector<eon::Platform>{eon::Platform::dos, eon::Platform::amiga,
                eon::Platform::atari_st}));
        assert(eon::platform_coverage(eon::Game::millennium, eon::Platform::dos)
            == eon::PlatformCoverage::recovered_startup);
        assert(eon::platform_coverage(eon::Game::millennium, eon::Platform::amiga)
            == eon::PlatformCoverage::bootstrap_only);
        assert(eon::platform_coverage(eon::Game::millennium, eon::Platform::atari_st)
            == eon::PlatformCoverage::bootstrap_only);
        assert(eon::platform_coverage(eon::Game::deuteros, eon::Platform::amiga)
            == eon::PlatformCoverage::recovered_opening);
        assert(eon::platform_coverage(eon::Game::deuteros, eon::Platform::atari_st)
            == eon::PlatformCoverage::bootstrap_only);
        assert(eon::name(eon::PlatformCoverage::recovered_startup) == "RECOVERED STARTUP");
        assert(eon::name(eon::PlatformCoverage::recovered_opening) == "RECOVERED OPENING");
        assert(eon::name(eon::PlatformCoverage::bootstrap_only) == "BOOTSTRAP ONLY");
        const eon::ReleaseArchive spanish_dos{eon::Game::millennium, eon::Platform::dos, "es",
            "b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4", {}};
        assert(eon::platform_coverage(spanish_dos) == eon::PlatformCoverage::bootstrap_only);
        const auto deuteros_supported = eon::supported_platforms(eon::Game::deuteros);
        assert((deuteros_supported
            == std::vector<eon::Platform>{eon::Platform::amiga, eon::Platform::atari_st}));
        // Support is a property of the recovered project catalogue, not of
        // whichever archives happen to have been scanned. Deuteros must not
        // show a DOS card merely because no Deuteros DOS archive exists.
        assert(std::find(deuteros_supported.begin(), deuteros_supported.end(), eon::Platform::dos)
            == deuteros_supported.end());
        assert(eon::select_available_platform(
            menu_releases, eon::Game::millennium, eon::Platform::dos) == eon::Platform::dos);
        assert(eon::select_available_platform(
            menu_releases, eon::Game::deuteros, eon::Platform::dos) == eon::Platform::amiga);
        assert(!eon::select_available_platform({}, eon::Game::deuteros, eon::Platform::amiga));
        const std::vector<eon::ReleaseArchive> multilingual_menu_releases{
            {eon::Game::millennium, eon::Platform::dos, "en", {}, {}},
            {eon::Game::millennium, eon::Platform::dos, "es", {}, {}},
            {eon::Game::millennium, eon::Platform::amiga, "en", {}, {}},
        };
        assert((eon::available_release_languages(multilingual_menu_releases,
            eon::Game::millennium, eon::Platform::dos)
            == std::vector<std::string>{"en", "es"}));
        assert(eon::platform_card_status(multilingual_menu_releases,
            eon::Game::millennium, eon::Platform::dos)
            == eon::PlatformCardStatus::ready);
        assert(eon::platform_card_startable(eon::PlatformCardStatus::ready));
        assert(eon::platform_card_status(multilingual_menu_releases,
            eon::Game::millennium, eon::Platform::atari_st)
            == eon::PlatformCardStatus::unavailable);
        assert(!eon::platform_card_selectable(eon::PlatformCardStatus::unavailable));
        assert(!eon::platform_card_startable(eon::PlatformCardStatus::unavailable));
        assert(eon::platform_card_status(menu_releases,
            eon::Game::deuteros, eon::Platform::atari_st)
            == eon::PlatformCardStatus::ready);
        assert(eon::platform_card_startable(eon::PlatformCardStatus::ready));
        assert(eon::select_available_release_language(multilingual_menu_releases,
            eon::Game::millennium, eon::Platform::dos, std::nullopt) == "en");
        assert(eon::select_available_release_language(multilingual_menu_releases,
            eon::Game::millennium, eon::Platform::dos, std::string{"es"}) == "es");
        assert(eon::select_available_release_language(multilingual_menu_releases,
            eon::Game::millennium, eon::Platform::amiga, std::nullopt) == "en");
        // The immutable original language is an explicit selection filter,
        // not a UI-locale preference. With one verified release in each
        // language, the Spanish CLI route must not first select English and
        // then reject it merely because the request was Spanish.
        const auto selected_spanish = eon::resolve_release_identity(multilingual_menu_releases,
            eon::Game::millennium, eon::Platform::dos, std::nullopt, "es");
        assert(selected_spanish && selected_spanish->language == "es");
        const auto selected_english = eon::resolve_release_identity(multilingual_menu_releases,
            eon::Game::millennium, eon::Platform::dos, std::nullopt, "en");
        assert(selected_english && selected_english->language == "en");
        const std::vector<eon::ReleaseArchive> duplicate_english_releases{
            {eon::Game::millennium, eon::Platform::amiga, "en",
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}},
            {eon::Game::millennium, eon::Platform::amiga, "en",
                "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", {}},
        };
        assert(eon::available_release_identities(duplicate_english_releases,
            eon::Game::millennium, eon::Platform::amiga).size() == 2);
        assert(!eon::select_available_release_sha256(duplicate_english_releases,
            eon::Game::millennium, eon::Platform::amiga, std::nullopt));
        assert(eon::platform_card_status(duplicate_english_releases,
            eon::Game::millennium, eon::Platform::amiga)
            == eon::PlatformCardStatus::release_selection_required);
        const auto exact_duplicate = eon::resolve_release_identity(duplicate_english_releases,
            eon::Game::millennium, eon::Platform::amiga,
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", "en");
        assert(exact_duplicate && exact_duplicate->sha256
            == "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
        eon::LaunchRequest menu_candidate;
        menu_candidate.game = eon::Game::millennium;
        menu_candidate.platform = eon::Platform::amiga;
        menu_candidate.release_sha256
            = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
        menu_candidate.release_language = "en";
        menu_candidate.presentation = eon::Presentation::modern;
        menu_candidate.presentation_explicit = true;
        const auto resolved_menu_request = eon::resolve_launch_request_identity(
            menu_candidate, duplicate_english_releases);
        assert(resolved_menu_request
            && resolved_menu_request->request.game == eon::Game::millennium
            && resolved_menu_request->request.platform == eon::Platform::amiga
            && resolved_menu_request->request.release_language == "en"
            && resolved_menu_request->request.release_sha256 == menu_candidate.release_sha256
            && resolved_menu_request->request.presentation == eon::Presentation::modern
            && resolved_menu_request->request.presentation_explicit
            && resolved_menu_request->release.sha256 == menu_candidate.release_sha256
            && resolved_menu_request->release.language == "en");
        menu_candidate.release_language = "es";
        assert(!eon::resolve_launch_request_identity(menu_candidate, duplicate_english_releases));
        menu_candidate.platform.reset();
        assert(!eon::resolve_launch_request_identity(menu_candidate, duplicate_english_releases));

        // The SDL-free runtime boundary rejects forged/stale identity DTOs
        // before retaining any media source. It has no previous selection to
        // fall back to when revalidation cannot open the asserted archive.
        eon::ReleaseRuntimeCoordinator runtime_coordinator;
        eon::ResolvedLaunchRequest forged_runtime_launch;
        forged_runtime_launch.request.game = eon::Game::millennium;
        forged_runtime_launch.request.platform = eon::Platform::amiga;
        forged_runtime_launch.request.release_language = "en";
        forged_runtime_launch.request.release_sha256 =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        forged_runtime_launch.release = duplicate_english_releases.front();
        forged_runtime_launch.release.platform = eon::Platform::dos;
        assert(!runtime_coordinator.acquire(forged_runtime_launch));
        assert(!runtime_coordinator.active());
        assert(runtime_coordinator.admission() == eon::ReleaseRuntimeAdmission::identity_rejected);
        forged_runtime_launch.request.platform = eon::Platform::dos;
        forged_runtime_launch.release.path = "missing-release-archive.zip";
        assert(!runtime_coordinator.acquire(forged_runtime_launch));
        assert(!runtime_coordinator.active()
            && runtime_coordinator.admission() == eon::ReleaseRuntimeAdmission::archive_rejected);
        assert(runtime_coordinator.rejection() == eon::ReleaseRuntimeRejection::original_media);
        assert(eon::release_runtime_admission_label(
            eon::ReleaseRuntimeAdmission::identity_rejected) == "REJECTED: IDENTITY");
        assert(eon::release_runtime_admission_label(
            eon::ReleaseRuntimeAdmission::archive_rejected) == "REJECTED: ARCHIVE HASH");
        assert(eon::release_runtime_admission_label(
            eon::ReleaseRuntimeAdmission::adapter_rejected) == "REJECTED: ADAPTER");
        assert(eon::release_runtime_rejection_label(
            eon::ReleaseRuntimeRejection::input_contract) == "INPUT CONTRACT");
        // The common CLI/card-menu admission gate clears any previous runtime
        // before rejecting an absent route candidate.
        assert(eon::admit_runtime_launch(runtime_coordinator, std::nullopt,
            duplicate_english_releases).admission == eon::ReleaseRuntimeAdmission::identity_rejected);
        assert(eon::admit_runtime_launch(runtime_coordinator, std::nullopt,
            duplicate_english_releases).rejection == eon::ReleaseRuntimeRejection::launch_identity);
        assert(!runtime_coordinator.active());
        runtime_coordinator.reset();
        assert(!runtime_coordinator.active()
            && runtime_coordinator.admission() == eon::ReleaseRuntimeAdmission::unselected);
        // A launch requested while the explicit source-resource teardown is
        // in progress cannot borrow the prior READY status or reset it early.
        eon::NativeSessionController returning_controller;
        returning_controller.begin_return_to_menu();
        const auto transition_launch = returning_controller.launch_direct(menu_candidate,
            duplicate_english_releases);
        assert(transition_launch.admission == eon::ReleaseRuntimeAdmission::identity_rejected);
        assert(transition_launch.rejection == eon::ReleaseRuntimeRejection::lifecycle_transition);
        assert(!transition_launch.accepted() && !transition_launch.active_launch);
        returning_controller.finish_return_to_menu();
        assert(returning_controller.is_menu());

        // The DOS asset factory is equally strict before it reaches any
        // archive path: its recognised title evidence cannot become an
        // Amiga/Atari fallback or silently stand in for an unrecognised
        // language. These calls must fail without reading the placeholder.
        eon::ReleaseArchive wrong_dos_platform = duplicate_english_releases.front();
        wrong_dos_platform.platform = eon::Platform::amiga;
        assert(!eon::load_millennium_dos_runtime(wrong_dos_platform));
        eon::ReleaseArchive wrong_dos_language = duplicate_english_releases.front();
        wrong_dos_language.platform = eon::Platform::dos;
        wrong_dos_language.language = "sv";
        assert(!eon::load_millennium_dos_runtime(wrong_dos_language));

        // The whole card route is SDL-independent: input devices only choose
        // cards, while platform/release admission and back-navigation have
        // one deterministic state model.
        eon::LauncherRouteState route;
        route.focus_game(duplicate_english_releases, eon::Game::millennium);
        assert(route.game == eon::Game::millennium && route.platform == eon::Platform::amiga);
        route.enter_platforms();
        assert(route.page == eon::LauncherPage::platforms);
        assert(route.choose_platform(duplicate_english_releases, eon::Platform::amiga));
        assert(route.page == eon::LauncherPage::releases && !route.release_is_selected());
        assert(!route.choose_release(duplicate_english_releases,
            "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"));
        assert(route.choose_release(duplicate_english_releases,
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
        assert(route.page == eon::LauncherPage::profiles && route.release_is_selected());
        const auto route_candidate = route.launch_request(menu_candidate);
        assert(route_candidate && route_candidate->release_sha256
            == "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
        const auto route_launch = route.resolve_launch(menu_candidate, duplicate_english_releases);
        assert(route_launch && route_launch->release.sha256
            == "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
        const auto route_admission = eon::launch_runtime_candidate(route_candidate,
            duplicate_english_releases, runtime_coordinator);
        assert(route_admission.admission == eon::ReleaseRuntimeAdmission::archive_rejected);
        assert(route_admission.rejection == eon::ReleaseRuntimeRejection::original_media);
        assert(!runtime_coordinator.active());

        // The final card-menu handoff is itself SDL-free and has no adapter
        // shortcut.  A fully selected card route still re-resolves only
        // against the scanner identities supplied at activation time, then
        // exposes a launch identity solely if the coordinator acquired it.
        eon::LauncherSessionState menu_runtime_session;
        menu_runtime_session.route = route;
        menu_runtime_session.choose_modern();
        eon::ReleaseRuntimeCoordinator menu_runtime_coordinator;
        const auto menu_runtime_result = eon::launch_menu_runtime(menu_runtime_session,
            menu_candidate, duplicate_english_releases, menu_runtime_coordinator);
        assert(menu_runtime_result.admission.admission
            == eon::ReleaseRuntimeAdmission::archive_rejected);
        assert(!menu_runtime_result.accepted() && !menu_runtime_result.active_launch
            && !menu_runtime_coordinator.active());
        // A selected hash missing from the current scanner snapshot cannot
        // follow an older route or card index into a prior adapter.
        const std::vector<eon::ReleaseArchive> stale_menu_releases{
            duplicate_english_releases.back()};
        const auto stale_menu_result = eon::launch_menu_runtime(menu_runtime_session,
            menu_candidate, stale_menu_releases, menu_runtime_coordinator);
        assert(stale_menu_result.admission.admission
            == eon::ReleaseRuntimeAdmission::identity_rejected);
        assert(stale_menu_result.admission.rejection == eon::ReleaseRuntimeRejection::launch_identity);
        assert(!stale_menu_result.accepted() && !stale_menu_result.active_launch
            && !menu_runtime_coordinator.active());
        // Custom has no launch bypass: before renderer confirmation there is
        // no candidate for the same common admission gate to resolve.
        menu_runtime_session.begin_custom();
        const auto pending_custom_result = eon::launch_menu_runtime(menu_runtime_session,
            menu_candidate, duplicate_english_releases, menu_runtime_coordinator);
        assert(pending_custom_result.admission.admission
            == eon::ReleaseRuntimeAdmission::identity_rejected);
        assert(pending_custom_result.admission.rejection == eon::ReleaseRuntimeRejection::launch_identity);
        assert(!pending_custom_result.accepted() && !pending_custom_result.active_launch
            && !menu_runtime_coordinator.active());

        // Scanner completion can reveal a second container after a platform
        // had one automatic release. That auto-selection must be revoked and
        // shown as a release-card choice; an identity picked explicitly by
        // the user remains exact and stable.
        const std::vector<eon::ReleaseArchive> initially_unique_releases{
            {eon::Game::millennium, eon::Platform::amiga, "en",
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}},
        };
        eon::LauncherRouteState incremental_route;
        incremental_route.focus_game(initially_unique_releases, eon::Game::millennium);
        assert(incremental_route.choose_platform(initially_unique_releases, eon::Platform::amiga));
        assert(incremental_route.page == eon::LauncherPage::profiles
            && incremental_route.release_is_selected() && !incremental_route.release_explicit);
        assert(incremental_route.reconcile_releases(duplicate_english_releases));
        assert(incremental_route.page == eon::LauncherPage::releases
            && !incremental_route.release_is_selected());
        assert(incremental_route.choose_release(duplicate_english_releases,
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
        assert(incremental_route.release_explicit);
        assert(!incremental_route.reconcile_releases(duplicate_english_releases));
        assert(incremental_route.page == eon::LauncherPage::profiles
            && incremental_route.release_is_selected());

        // Cache invalidation is keyed to the complete release provenance,
        // not card focus or presentation. The SDL layer may therefore safely
        // retain resources for an identical selection and must revoke them
        // for a platform, language, or outer-container hash change.
        eon::LauncherInteractionController source_controller;
        source_controller.session.focus_game(duplicate_english_releases, eon::Game::millennium);
        const auto no_change = source_controller.source_identity();
        assert(!source_controller.source_changed_since(no_change));
        assert(source_controller.session.choose_release(duplicate_english_releases,
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
        assert(source_controller.source_changed_since(no_change));
        const auto first_identity = source_controller.source_identity();
        source_controller.session.choose_modern();
        assert(!source_controller.source_changed_since(first_identity));
        source_controller.session.choose_original();
        assert(!source_controller.source_changed_since(first_identity));
        assert(source_controller.session.choose_release(duplicate_english_releases,
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"));
        assert(source_controller.source_changed_since(first_identity));
        const std::vector<eon::ReleaseArchive> bilingual_identity_releases{
            {eon::Game::millennium, eon::Platform::amiga, "en",
                "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", {}},
            {eon::Game::millennium, eon::Platform::amiga, "es",
                "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc", {}},
        };
        eon::LauncherInteractionController language_controller;
        language_controller.session.focus_game(bilingual_identity_releases, eon::Game::millennium);
        assert(language_controller.session.choose_release(bilingual_identity_releases,
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"));
        const auto english_identity = language_controller.source_identity();
        assert(language_controller.session.choose_release(bilingual_identity_releases,
            "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"));
        assert(language_controller.source_changed_since(english_identity));

        // Changing games must revoke the previous outer identity even if the
        // same platform card remains available. No old Millennium Amiga hash
        // may be visible while Deuteros Amiga is being selected.
        const std::vector<eon::ReleaseArchive> shared_amiga_games{
            {eon::Game::millennium, eon::Platform::amiga, "en",
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}},
            {eon::Game::deuteros, eon::Platform::amiga, "en",
                "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", {}},
        };
        eon::LauncherRouteState shared_platform_route;
        shared_platform_route.focus_game(shared_amiga_games, eon::Game::millennium);
        assert(shared_platform_route.choose_platform(shared_amiga_games, eon::Platform::amiga));
        assert(shared_platform_route.release_is_selected());
        shared_platform_route.focus_game(shared_amiga_games, eon::Game::deuteros);
        assert(shared_platform_route.platform == eon::Platform::amiga);
        assert(!shared_platform_route.release_language && !shared_platform_route.release_sha256
            && !shared_platform_route.release_is_selected());

        // Presentation and Custom confirmation have the same SDL-free owner
        // as the card route. A Custom card cannot bypass its renderer-only
        // panel, while Original/Modern resolve the identical release identity.
        eon::LauncherSessionState session;
        session.route = route;
        session.choose_original();
        const auto original_launch = session.resolve_launch(menu_candidate, duplicate_english_releases);
        assert(original_launch && original_launch->request.presentation == eon::Presentation::original
            && original_launch->release.sha256 == route_launch->release.sha256);
        session.choose_modern();
        const auto modern_launch = session.resolve_launch(menu_candidate, duplicate_english_releases);
        assert(modern_launch && modern_launch->request.presentation == eon::Presentation::modern
            && modern_launch->release.sha256 == route_launch->release.sha256);
        session.begin_custom();
        assert(!session.custom_profile_ready && session.custom_profile_pending && !session.can_launch()
            && !session.resolve_launch(menu_candidate, duplicate_english_releases));
        session.confirm_custom();
        assert(session.custom_profile_ready && !session.custom_profile_pending && session.can_launch());
        const auto custom_launch = session.resolve_launch(menu_candidate, duplicate_english_releases);
        assert(custom_launch && custom_launch->request.presentation == eon::Presentation::modern
            && custom_launch->release.sha256 == route_launch->release.sha256);
        session.invalidate_custom();
        assert(!session.custom_profile_ready && session.can_launch());
        session.back(duplicate_english_releases);
        assert(session.route.page == eon::LauncherPage::releases && !session.custom_profile_ready);
        session.back(duplicate_english_releases);
        assert(session.route.page == eon::LauncherPage::platforms);
        session.back(duplicate_english_releases);
        assert(session.route.page == eon::LauncherPage::games);
        session.begin_custom();
        session.reset_for_data(eon::Game::deuteros);
        assert(session.route.page == eon::LauncherPage::games
            && session.route.game == eon::Game::deuteros
            && !session.route.platform && !session.route.release_language
            && !session.route.release_sha256
            && session.presentation == eon::Presentation::original
            && !session.custom_profile_ready && !session.custom_profile_pending);

        // Every device uses this controller's card actions.  The keyboard
        // route (move + activate) and pointer route (activate_card) must
        // arrive at the same exact release identity; Custom cannot launch
        // until the renderer-owned dialog explicitly confirms it.
        eon::LauncherInteractionController keyboard_controller;
        keyboard_controller.synchronize(duplicate_english_releases);
        assert(keyboard_controller.session.route.game == eon::Game::millennium
            && keyboard_controller.session.route.platform == eon::Platform::amiga);
        assert(keyboard_controller.activate(duplicate_english_releases)
            == eon::LauncherInteractionEffect::none);
        assert(keyboard_controller.session.route.page == eon::LauncherPage::platforms);
        assert(keyboard_controller.activate(duplicate_english_releases)
            == eon::LauncherInteractionEffect::none);
        assert(keyboard_controller.session.route.page == eon::LauncherPage::releases);
        keyboard_controller.focus.release = 1;
        assert(keyboard_controller.activate(duplicate_english_releases)
            == eon::LauncherInteractionEffect::none);
        assert(keyboard_controller.session.route.page == eon::LauncherPage::profiles
            && keyboard_controller.session.route.release_sha256
                == "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
        keyboard_controller.focus.profile = 2;
        assert(keyboard_controller.activate(duplicate_english_releases)
            == eon::LauncherInteractionEffect::open_custom_settings);
        assert(!keyboard_controller.session.can_launch());
        keyboard_controller.session.confirm_custom();
        assert(keyboard_controller.activate(duplicate_english_releases)
            == eon::LauncherInteractionEffect::launch);
        const auto controller_launch = keyboard_controller.session.resolve_launch(
            menu_candidate, duplicate_english_releases);
        assert(controller_launch && controller_launch->release.sha256
            == "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
        eon::LauncherInteractionController pointer_controller;
        assert(pointer_controller.activate_card(duplicate_english_releases, pointer_controller.focus.platform)
            == eon::LauncherInteractionEffect::none);
        assert(pointer_controller.session.route.page == eon::LauncherPage::platforms);
        assert(pointer_controller.activate_card(duplicate_english_releases, pointer_controller.focus.platform)
            == eon::LauncherInteractionEffect::none);
        assert(pointer_controller.session.route.page == eon::LauncherPage::releases);
        assert(pointer_controller.activate_card(duplicate_english_releases, 1)
            == eon::LauncherInteractionEffect::none);
        assert(pointer_controller.session.route.release_sha256
            == keyboard_controller.session.route.release_sha256);
        pointer_controller.reset_for_data(eon::Game::deuteros);
        assert(pointer_controller.session.route.page == eon::LauncherPage::games
            && pointer_controller.focus.game == 1
            && !pointer_controller.session.route.platform
            && pointer_controller.session.presentation == eon::Presentation::original);

        // Card focus is deliberately independent from release identity. It
        // is shared presentation behaviour for keyboard/gamepad navigation,
        // while a pointer route may set a bounded index directly.
        eon::LauncherCardFocus focus;
        assert(focus.current(eon::LauncherPage::games) == 0);
        focus.move(eon::LauncherPage::games, 2, -1);
        assert(focus.game == 1);
        focus.move(eon::LauncherPage::games, 2, 1);
        assert(focus.game == 0);
        focus.set(eon::LauncherPage::platforms, 3, 99);
        assert(focus.platform == 2);
        focus.first(eon::LauncherPage::platforms, 3);
        assert(focus.platform == 0);
        focus.last(eon::LauncherPage::releases, 2);
        assert(focus.release == 1);
        focus.set(eon::LauncherPage::profiles, 3, 2);
        focus.reset_after_platform_change();
        assert(focus.release == 0 && focus.profile == 0 && focus.game == 0);
        focus.set(eon::LauncherPage::platforms, 3, 2);
        focus.set(eon::LauncherPage::releases, 2, 1);
        focus.set(eon::LauncherPage::profiles, 3, 2);
        focus.reset_after_game_change();
        assert(focus.platform == 0 && focus.release == 0 && focus.profile == 0);
        // A scan may remove cards after focus was set. Stepping must first
        // bound that stale presentation index to the current page.
        focus.set(eon::LauncherPage::platforms, 3, 2);
        focus.move(eon::LauncherPage::platforms, 2, 1);
        assert(focus.platform == 0);
        focus.set(eon::LauncherPage::platforms, 3, 2);
        focus.move(eon::LauncherPage::platforms, 2, -1);
        assert(focus.platform == 0);
        focus.move(eon::LauncherPage::profiles, 0, 1);
        assert(focus.profile == 0);

        assert(eon::normalize_language("sv_SE.UTF-8") == "sv_SE");
        assert(eon::normalize_language("pt-BR") == "pt_BR");
        assert(eon::normalize_language("C") == "c");
    assert(eon::normalize_language("not a locale").empty());
        const auto& launcher_languages = eon::supported_launcher_languages();
        assert(launcher_languages.size() == 21 && launcher_languages.front() == "en"
            && launcher_languages.back() == "zh_CN");
        assert(std::find(launcher_languages.begin(), launcher_languages.end(), "sv")
            != launcher_languages.end());
        assert(eon::canonical_launcher_language("pt") == "pt_BR");
        assert(eon::canonical_launcher_language("zh") == "zh_CN");
        assert(eon::canonical_launcher_language("sv_SE.UTF-8") == "sv");
        assert(eon::canonical_launcher_language("en_GB") == "en_GB");
        assert(eon::canonical_launcher_language("not a locale") == "en");
        assert(eon::launcher_language_autonym("sv_SE.UTF-8") == "Svenska");
        assert(eon::launcher_language_autonym("zh") == "简体中文");
        assert(eon::launcher_language_autonym("not a locale") == "English");
        for (const auto locale : launcher_languages) {
            assert(!eon::launcher_language_autonym(locale).empty());
        }
    // The Unicode launcher renderer must never consult a system font when no
    // reviewed bundled asset is present. This is a no-renderer/no-font native
    // contract and therefore requires no host font in CI.
    const auto test_tmpdir = std::getenv("EON_TEST_TMPDIR");
    assert(test_tmpdir && *test_tmpdir);
    const auto temporary_root = std::filesystem::path(test_tmpdir);
    std::filesystem::create_directories(temporary_root);
    const auto preferences_path = temporary_root / "presentation-preferences.ini";
    const eon::PresentationPreferences preferences{2, 1, 3, 2, 0, true, true, false, true, "sv"};
    assert(eon::save_presentation_preferences(preferences_path, preferences));
    const auto loaded_preferences = eon::load_presentation_preferences(preferences_path);
    assert(loaded_preferences && loaded_preferences->output_resolution_index == 2
        && loaded_preferences->aspect_ratio_index == 1
        && loaded_preferences->modern_preset_index == 3
        && loaded_preferences->render_pacing_index == 2
        && loaded_preferences->pixel_reconstruction_index == 0 && loaded_preferences->smooth_scaling
        && loaded_preferences->scanlines && !loaded_preferences->frame && loaded_preferences->reduced_motion
        && loaded_preferences->launcher_language == "sv");
    const eon::PresentationPreferences invalid_reconstruction{2, 1, 3, 2, 3, true, true, false, false, "en"};
    assert(!eon::save_presentation_preferences(preferences_path, invalid_reconstruction));
    {
        std::ofstream malformed_preferences(preferences_path, std::ios::binary | std::ios::trunc);
        malformed_preferences << "project-eon-presentation-preferences=1\nresolution=9\n";
    }
    assert(!eon::load_presentation_preferences(preferences_path));
    {
        std::ofstream v1_preferences(preferences_path, std::ios::binary | std::ios::trunc);
        v1_preferences << "project-eon-presentation-preferences=1\n"
                       << "resolution=0\naspect=0\npreset=0\npacing=0\nreconstruction=1\n"
                       << "scaling=1\nscanlines=0\nframe=1\n";
    }
    const auto migrated_preferences = eon::load_presentation_preferences(preferences_path);
    assert(migrated_preferences && migrated_preferences->launcher_language == "en");
    assert(eon::save_launcher_language_preference(preferences_path, "ja"));
    const auto language_preferences = eon::load_presentation_preferences(preferences_path);
    assert(language_preferences && language_preferences->launcher_language == "ja");
    assert(!eon::save_launcher_language_preference(preferences_path, "invalid"));
    std::filesystem::remove(preferences_path);
    assert(!eon::UnicodeTextRenderer::create(nullptr,
        temporary_root / "project-eon-no-host-font.ttf"));
        char language_option[] = "--language";
        char swedish[] = "sv_SE.UTF-8";
        char* language_args[] = {program, language_option, swedish};
        const auto language = eon::parse_command_line(3, language_args);
        assert(language.request && language.request->language == "sv_SE"
            && language.request->language_explicit);

        const auto temporary_po = temporary_root / "project-eon-i18n-test.po";
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
        // Every launcher locale is runtime data, not merely an installed PO
        // filename. Exercise the same normalizer and lookup route used by
        // `--language`; a missing catalog must not be hidden by English's
        // intentional no-catalog fallback.
        constexpr std::array<std::string_view, 20> shipped_catalogs{{
            "ar", "de", "el", "en_GB", "es", "fi", "fr", "hi", "it", "ja",
            "ko", "nl", "no", "pl", "pt_BR", "ru", "sv", "tr", "uk", "zh_CN",
        }};
        for (const auto catalog_name : shipped_catalogs) {
            const auto catalog = eon::Translator::from_language(
                std::string(catalog_name) + ".UTF-8");
            assert(!catalog.empty());
            assert(!catalog.translate("ENTER / CLICK: START").empty());
        }
        const auto english_catalog = eon::Translator::from_language("en_US.UTF-8");
        assert(english_catalog.empty());
        assert(english_catalog.translate("ENTER / CLICK: START") == "ENTER / CLICK: START");
        assert(eon::Translator::from_language("zz").empty());
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
    assert(incremental_scanner.discovering());
    assert(!incremental_scanner.done());
    const auto scanned_before = incremental_scanner.scanned_count();
    static_cast<void>(incremental_scanner.advance(1));
    assert(incremental_scanner.scanned_count() == scanned_before);
    while (!incremental_scanner.advance(2)) {
    }
    assert(incremental_scanner.done());
    assert(!incremental_scanner.discovering());
    assert(incremental_scanner.candidate_count() >= 6);
    assert(incremental_scanner.releases().size() == 6);
    assert(incremental_scanner.report().candidates == incremental_scanner.candidate_count());
    assert(incremental_scanner.report().candidates
        == incremental_scanner.report().size_rejected_candidates
            + incremental_scanner.report().hashed_candidates
            + incremental_scanner.report().unreadable_candidates);
    // Other user files may share a release's byte length, so only the hash
    // identity count is corpus-fixed. Every size-admitted candidate is either
    // completely hashed or explicitly accounted as unreadable.
    assert(incremental_scanner.report().size_candidates >= 6);
    assert(incremental_scanner.report().hashed_candidates
        + incremental_scanner.report().unreadable_candidates
        == incremental_scanner.report().size_candidates);
    assert(incremental_scanner.report().verified_occurrences == 6);
    // The supplied corpus contains recognised outer archives. A leaf found
    // outside such an archive would remain unbound evidence and cannot alter
    // this release count or platform admission table.
    assert(incremental_scanner.report().verified_direct_media_occurrences == 0);
    assert(incremental_scanner.unbound_direct_media().empty());
    assert(incremental_scanner.report().hash_rejected_candidates
        <= incremental_scanner.report().hashed_candidates);
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
    std::set<std::string> absent_direct_profile_releases;
    for (const auto& profile : eon::parser_profile_manifest()) {
        assert(profile.offset <= profile.leaf_size);
        assert(profile.length <= profile.leaf_size - profile.offset);
        const auto release = std::find_if(releases.begin(), releases.end(),
            [&profile](const auto& candidate) { return candidate.sha256 == profile.release_sha256; });
        // The manifest also accepts two exact direct containers embedded in
        // the supplied catalogue archives. They are not top-level files in
        // this fixture, so verify their compiled identity without extracting
        // or copying commercial bytes merely to manufacture an occurrence.
        if (release == releases.end()) {
            absent_direct_profile_releases.emplace(profile.release_sha256);
            assert(eon::release_has_parser_profile(profile.release_sha256, profile.id));
            continue;
        }
        const auto leaves = eon::inventory_zip(release->path);
        const auto leaf = std::find_if(leaves.begin(), leaves.end(), [&profile](const auto& candidate) {
            return candidate.sha256 == profile.leaf_sha256 && candidate.size == profile.leaf_size;
        });
        assert(leaf != leaves.end());
        assert(eon::release_has_parser_profile(release->sha256, profile.id));
    }
    assert((absent_direct_profile_releases == std::set<std::string>{
        "0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69",
        "ec0424445d494809d2661492e289af71b056a429dde13b053a472ccc8347d4dd"}));
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
        if (mapped_release != releases.end()) {
            assert(mapped_release->game == entry.game);
            assert(mapped_release->platform == entry.platform);
            assert(mapped_release->language == entry.language);
        } else {
            // As above, direct containers are admissible but absent from the
            // six physical catalogue files exercised by this fixture.
            const auto manifest_release = std::find_if(eon::release_manifest().begin(),
                eon::release_manifest().end(), [&entry](const auto& candidate) {
                    return candidate.sha256 == entry.release_sha256;
                });
            assert(manifest_release != eon::release_manifest().end());
            assert(manifest_release->game == entry.game);
            assert(manifest_release->platform == entry.platform);
            assert(manifest_release->language == entry.language);
        }
    }
    assert(!eon::release_has_recovery_map_entry(
        "b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4",
        "millennium-dos-title-flow"));
    const auto amiga_map = eon::recovery_map_for_release(
        "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04");
    assert(amiga_map.size() == 3);
    for (const auto& entry : amiga_map) assert(entry.game == eon::Game::deuteros);
    // Function-map rows are kept in preservation-document order. In
    // particular, Deuteros Amiga's title handoff comes after an Atari row;
    // diagnostics must filter the complete declaration rather than assume
    // same-release rows are contiguous.
    for (const auto& manifest_release : eon::release_manifest()) {
        const auto functions = eon::function_map_for_release(manifest_release.sha256);
        const auto declared_count = std::count_if(eon::function_map().begin(), eon::function_map().end(),
            [&manifest_release](const auto& entry) {
                return entry.release_sha256 == manifest_release.sha256;
            });
        assert(functions.size() == static_cast<std::size_t>(declared_count));
        for (const auto& entry : functions) {
            assert(entry.release_sha256 == manifest_release.sha256);
        }
    }
    const auto deuteros_amiga_functions = eon::function_map_for_release(
        "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04");
    assert(deuteros_amiga_functions.size() == 9);
    assert(std::any_of(deuteros_amiga_functions.begin(), deuteros_amiga_functions.end(), [](const auto& entry) {
        return entry.id == "deuteros-amiga-en-title-exec-boundary";
    }));
    assert(std::any_of(deuteros_amiga_functions.begin(), deuteros_amiga_functions.end(), [](const auto& entry) {
        return entry.id == "deuteros-amiga-en-channel-request-continuation";
    }));
    assert(std::any_of(deuteros_amiga_functions.begin(), deuteros_amiga_functions.end(), [](const auto& entry) {
        return entry.id == "deuteros-amiga-en-opening-title-command";
    }));
    const auto millennium_dos_functions = eon::function_map_for_release(
        "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123");
    assert(millennium_dos_functions.size() == 12);
    assert(std::any_of(millennium_dos_functions.begin(), millennium_dos_functions.end(), [](const auto& entry) {
        return entry.id == "millennium-dos-en-title-availability-poll"
            && entry.runtime_address == "$0d0a"
            && entry.runtime_status == "availability boundary only";
    }));
    for (const auto title_boundary_id : {"millennium-dos-en-title-nonzero-branch",
             "millennium-dos-en-title-exit-closure", "millennium-dos-en-title-private-driver-setup",
             "millennium-dos-en-title-private-driver-helper"}) {
        assert(std::any_of(millennium_dos_functions.begin(), millennium_dos_functions.end(),
            [title_boundary_id](const auto& entry) { return entry.id == title_boundary_id; }));
    }
    for (const auto& entry : eon::function_map()) {
        assert(eon::function_map_entry_is_well_formed(entry));
    }
    // A direct range binding is source provenance only. It must use the exact
    // release, CPU, address space, leaf hash and coordinate, without claiming
    // anything about code classification or runtime execution.
    const auto& coverage_entry = eon::function_map().front();
    eon::StaticControlFlowSummary coverage_sidecar;
    coverage_sidecar.documents.push_back({std::string(coverage_entry.release_sha256),
        std::string(coverage_entry.cpu), std::string(coverage_entry.address_space)});
    coverage_sidecar.declared_ranges.push_back({0, std::string(coverage_entry.source_span_sha256.empty()
        ? coverage_entry.source_asset_sha256 : coverage_entry.source_span_sha256),
        0, 0x1000});
    const auto coverage = eon::function_map_sidecar_coverage(coverage_sidecar);
    const auto same_release_entries = eon::function_map_for_release(coverage_entry.release_sha256);
    assert(coverage.function_entry_count == same_release_entries.size());
    assert(coverage.direct_range_binding_count == 1);
    assert(coverage.not_declared_by_sidecar_count + coverage.direct_range_binding_count
        == coverage.function_entry_count);
    coverage_sidecar.documents.front().address_space = "runtime";
    const auto mismatched_space = eon::function_map_sidecar_coverage(coverage_sidecar);
    assert(mismatched_space.direct_range_binding_count == 0);
    auto malformed_function = eon::function_map().front();
    malformed_function.source_asset_sha256 = "not-a-sha256";
    assert(!eon::function_map_entry_is_well_formed(malformed_function));
    malformed_function = eon::function_map().front();
    malformed_function.source_span_sha256 = "not-a-sha256";
    assert(!eon::function_map_entry_is_well_formed(malformed_function));
    malformed_function = eon::function_map().front();
    malformed_function.cpu = "unknown-cpu";
    assert(!eon::function_map_entry_is_well_formed(malformed_function));
    malformed_function = eon::function_map().front();
    malformed_function.address_space = "runtime";
    malformed_function.runtime_address = "+0x0000";
    assert(!eon::function_map_entry_is_well_formed(malformed_function));
    malformed_function.address_space = "image-relative-unrelocated";
    assert(eon::function_map_entry_is_well_formed(malformed_function));
    // CLI JSON and the F10 panel consume this one SDL-free composition. It
    // has to retain only rows that match the selected immutable release,
    // while reporting no trace admission merely because media was scanned.
    for (const auto& release : releases) {
        const auto verified_media = eon::VerifiedReleaseMedia::open(release);
        assert(eon::function_map_entries_are_attested_by_media(verified_media));
        const auto capability = eon::release_runtime_capability_for(release);
        assert(capability.has_value());
        const auto diagnostics = eon::runtime_diagnostics_for_release(release);
        assert(diagnostics.game == release.game);
        assert(diagnostics.platform == release.platform);
        assert(diagnostics.language == release.language);
        assert(diagnostics.release_sha256 == release.sha256);
        assert(diagnostics.coverage == eon::platform_coverage(release));
        assert(diagnostics.coverage == capability->coverage);
        assert(diagnostics.trace_admission == "not-loaded");
        assert(diagnostics.recovery_boundaries.size()
            == eon::recovery_map_for_release(release.sha256).size());
        assert(diagnostics.functions.size()
            == eon::function_map_for_release(release.sha256).size());
        if (const auto startup = eon::startup_boundary_for_release(release.sha256)) {
            assert(diagnostics.startup_boundary.has_value());
            assert(diagnostics.startup_boundary->parser_profile_id == startup->parser_profile_id);
            assert(diagnostics.startup_boundary->source_address == startup->source_address);
        } else {
            assert(!diagnostics.startup_boundary.has_value());
        }
        for (const auto& boundary : diagnostics.recovery_boundaries) {
            assert(eon::release_has_recovery_map_entry(release.sha256, boundary.id));
        }
        for (const auto& function : diagnostics.functions) {
            assert(eon::release_has_function_map_entry(release.sha256, function.id));
        }
    }
    assert(eon::release_runtime_capabilities().size() == eon::release_manifest().size());
    assert(eon::release_runtime_capability_manifest_is_valid());
    for (const auto& capability : eon::release_runtime_capabilities()) {
        assert(eon::runtime_session_declaration_is_valid(capability.initial_kind,
            capability.initial_boundary, capability.initial_capabilities));
    }
    assert(!eon::runtime_session_declaration_is_valid(
        eon::RuntimeSessionKind::millennium_dos_title,
        eon::RuntimeSessionBoundary::bootstrap_boundary, {true, false, true}));
    assert(!eon::runtime_session_declaration_is_valid(
        eon::RuntimeSessionKind::deuteros_amiga_opening,
        eon::RuntimeSessionBoundary::recovered_presentation_boundary, {true, false, false}));
    assert(eon::direct_media_set_manifest_is_valid());
    assert(eon::recovery_map_manifest_is_valid());
    assert(eon::function_map_manifest_is_valid());
    assert(eon::startup_boundary_manifest_is_valid());
    assert(eon::declarative_provenance_manifests_are_valid());
    for (const auto& manifest_release : eon::release_manifest()) {
        const eon::ReleaseArchive release{manifest_release.game, manifest_release.platform,
            std::string(manifest_release.language), std::string(manifest_release.sha256), {}};
        assert(eon::release_runtime_capability_for(release).has_value());
    }

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
    const auto& defjam_resident_evidence = defjam_session.resident_evidence();
    assert(&defjam_session.resident_entry() == &defjam_resident_evidence.entry);
    assert(defjam_resident_evidence.splitter.entry_address == 0x68016);
    assert(defjam_resident_evidence.staging_callsites.size() == 2);
    assert(defjam_resident_evidence.first_post_helper_chain.staging_entry_address == 0x69624);
    assert(defjam_resident_evidence.second_post_helper_chain.staging_entry_address == 0x69b88);
    assert(defjam_resident_evidence.separate_post_external_call.terminal_jump_address == 0x68f72);
    assert(defjam_resident_evidence.separate_terminal_jump_target.target_address == 0x7c54e);
    assert(defjam_resident_evidence.independent_entry.entry_address == 0x68508);
    assert(defjam_session.post_negative_d3_terminal().entry_address == 0x685fe);
    assert(defjam_session.post_negative_d3_continuation().entry_address == 0x6861a);
    assert(defjam_session.plan().loader_magic == 0xa8d398fb);
    assert(defjam_session.shared_resident().raw_sha256
        == "d144abc05f891710dc99b30d87f020bd6e2ff7796ef86a847f07b8d97d55d18e");
    assert(defjam_session.resident_entry().entry_address == 0x68000);
    assert(defjam_session.opaque_invocation_boundary().entry_address == 0x7029e);
    assert(defjam_session.opaque_invocation_boundary().first_stage_target == 0x41000);
    assert(defjam_session.opaque_invocation_boundary().resident_stage_target == 0x68000);
    assert(defjam_session.first_stage_source_anchors().raw_disk_offset == 0x24200);
    assert(defjam_session.first_stage_source_anchors().byte_count == 0x6e000);
    assert(defjam_session.first_stage_source_anchors().sha256
        == defjam_session.plan().first_stage.raw_sha256);
    {
        auto altered = *defjam_adf;
        altered.back() ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::MillenniumAmigaBootstrapSession(std::move(altered)));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
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
    const auto defjam_relocation =
        eon::parse_millennium_amiga_bootstrap_relocation_boundary(
            defjam_loader_disk, defjam_plan);
    assert(defjam_relocation.entry_address == 0x70000);
    assert(defjam_relocation.verified_loaded_start == 0x70000);
    assert(defjam_relocation.verified_loaded_end_exclusive == 0x70400);
    assert(defjam_relocation.copy_source_address == 0x70032);
    assert(defjam_relocation.copy_destination_address == 0x66032);
    assert(defjam_relocation.copy_byte_count == 0x3cf);
    assert(defjam_relocation.copy_source_end_inclusive == 0x70400);
    assert(defjam_relocation.relocated_continuation_address == 0x6629e);
    assert(defjam_relocation.raw_continuation_source_address == 0x7029e);
    assert(defjam_relocation.raw_continuation_source_address
        == defjam_opaque_invocation.entry_address);
    assert(defjam_relocation.sha256
        == "341e6cff049ff9cda953ad0c91f9a064ed2d2cdc1782b417f27ecad7c9b279b4");
    assert(defjam_session.relocation_boundary().copy_source_end_inclusive
        == defjam_session.relocation_boundary().verified_loaded_end_exclusive);
    {
        auto altered = *defjam_adf;
        altered[0x436] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_amiga_bootstrap_relocation_boundary(
                eon::AmigaAdf(std::move(altered)), defjam_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
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
    // Execute only the fully local $685fe prefix after binding it to the
    // supplied Defjam bytes.  These are controlled register inputs, not a
    // claim that an original caller reaches this independent entry.
    const auto local_zero = eon::execute_millennium_amiga_resident_post_negative_d3_terminal_prefix(
        defjam_post_negative_d3, {0xaabbccdd, 0x11220000, 0x33445566});
    assert(local_zero.d0 == 0xaabb0000);
    assert(local_zero.d1 == 0x11225566);
    assert(local_zero.d2 == 0x33445566);
    assert((local_zero.absolute_byte_writes == std::array<std::uint8_t, 2>{{0, 0}}));
    assert(local_zero.stop == eon::MillenniumAmigaResidentPostNegativeD3TerminalStop::zero_return);
    assert(local_zero.next_address == 0x68614);
    const auto local_negative = eon::execute_millennium_amiga_resident_post_negative_d3_terminal_prefix(
        defjam_post_negative_d3, {0, 0x12348001, 0x56789abc});
    assert(local_negative.d0 == 0x00008001);
    assert(local_negative.d1 == 0x12349abc);
    assert(local_negative.stop
        == eon::MillenniumAmigaResidentPostNegativeD3TerminalStop::negative_return);
    assert(local_negative.next_address == 0x68618);
    const auto local_continue = eon::execute_millennium_amiga_resident_post_negative_d3_terminal_prefix(
        defjam_post_negative_d3, {0, 0x12340001, 0x56789abc});
    assert(local_continue.stop
        == eon::MillenniumAmigaResidentPostNegativeD3TerminalStop::nonnegative_continuation_boundary);
    assert(local_continue.next_address == 0x6861a);
    {
        auto detached_terminal = defjam_post_negative_d3;
        detached_terminal.raw_sha256[0] = '0';
        bool rejected = false;
        try {
            static_cast<void>(
                eon::execute_millennium_amiga_resident_post_negative_d3_terminal_prefix(
                    detached_terminal, {}));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
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
    // The BPL target is now a bounded, call-free execution prefix.  It keeps
    // word-width arithmetic and both unresolved branches explicit rather
    // than treating the terminal external jump as a host call.
    const auto continuation_external =
        eon::execute_millennium_amiga_resident_post_negative_d3_continuation_prefix(
            defjam_post_negative_d3_continuation,
            {0xaabbccdd, 0x12340002, 0x56780001, 0x00000040, 0x11110000, 0x22220000, 0x33445566});
    assert(continuation_external.d1 == 0x12342802);
    assert(continuation_external.d2 == 0x56782801);
    assert(continuation_external.d3 == 0x00002840);
    assert(continuation_external.d6 == 0x1111a502);
    assert(continuation_external.d7 == 0x2222a501);
    assert((continuation_external.restored_registers
        == std::array<std::uint32_t, 5>{{0xaabbccdd, 0x12340002, 0x56780001, 0x00000040, 0x33445566}}));
    assert(continuation_external.stop
        == eon::MillenniumAmigaResidentPostNegativeD3ContinuationStop::external_jump_boundary);
    assert(continuation_external.next_address == 0x7bef0);
    const auto continuation_low_range =
        eon::execute_millennium_amiga_resident_post_negative_d3_continuation_prefix(
            defjam_post_negative_d3_continuation, {0, 0, 0, 0xd800});
    assert(continuation_low_range.d3 == 0);
    assert(continuation_low_range.stop
        == eon::MillenniumAmigaResidentPostNegativeD3ContinuationStop::low_range_branch_boundary);
    assert(continuation_low_range.next_address == 0x68650);
    const auto continuation_negative_range =
        eon::execute_millennium_amiga_resident_post_negative_d3_continuation_prefix(
            defjam_post_negative_d3_continuation, {0, 0, 0, 0x6000});
    assert(continuation_negative_range.d3 == 0x8800);
    assert(continuation_negative_range.stop
        == eon::MillenniumAmigaResidentPostNegativeD3ContinuationStop::negative_range_branch_boundary);
    assert(continuation_negative_range.next_address == 0x68694);
    const auto continuation_pair_fallthrough =
        eon::execute_millennium_amiga_resident_post_negative_d3_continuation_prefix(
            defjam_post_negative_d3_continuation, {0, 0x12340000, 0x56781000, 0x40});
    assert(continuation_pair_fallthrough.d1 == 0x56781001);
    assert(continuation_pair_fallthrough.d2 == 0x12342800);
    {
        auto detached_continuation = defjam_post_negative_d3_continuation;
        detached_continuation.terminal_jump_target = 0;
        bool rejected = false;
        try {
            static_cast<void>(
                eon::execute_millennium_amiga_resident_post_negative_d3_continuation_prefix(
                    detached_continuation, {}));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
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
    const auto defjam_independent_post_call_tail =
        eon::parse_millennium_amiga_resident_independent_post_call_tail_boundary(
            defjam_loader_disk, defjam_plan, defjam_independent_branch_preparation);
    assert(defjam_independent_post_call_tail.entry_address == 0x68596);
    assert(defjam_independent_post_call_tail.raw_disk_offset == 0x16996);
    assert(defjam_independent_post_call_tail.byte_count == 104);
    assert(defjam_independent_post_call_tail.sha256
        == "eeed978d0afd278cc48868c0d2b76205304ddfa80b174d2aac95dc50b80dd551");
    assert((defjam_independent_post_call_tail.absolute_byte_addresses
        == std::array<std::uint32_t, 6>{{0x7b3b0, 0x7b3b1, 0x7b3b4,
            0x7b3ba, 0x7b3bb, 0x7b3bc}}));
    assert(defjam_independent_post_call_tail.external_jump_address == 0x685ee);
    assert(defjam_independent_post_call_tail.external_jump_target == 0x7bcf8);
    assert(defjam_independent_post_call_tail.negative_path_address == 0x685f4);
    assert(defjam_independent_post_call_tail.negative_path_return_address == 0x685fc);
    assert(defjam_independent_post_call_tail.nonnegative_return_address == 0x685fe);
    {
        auto altered = *defjam_adf;
        altered[0x16996 + 103] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(
                eon::parse_millennium_amiga_resident_independent_post_call_tail_boundary(
                    eon::AmigaAdf(std::move(altered)), defjam_plan,
                    defjam_independent_branch_preparation));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
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
        assert(eon::to_hex(eon::sha256(bytes.subspan(0x16996, 104)))
            == defjam_independent_post_call_tail.sha256);
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
    // The coordinator uses this one verified, in-memory view for every
    // adapter leaf during a single admission. It is an exact rehash of the
    // supplied archive, never an extracted media cache.
    const auto verified_dos_media = eon::VerifiedReleaseMedia::open(*english_dos);
    const auto borrowed_title = verified_dos_media.borrow(
        "6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678");
    const auto borrowed_title_again = verified_dos_media.borrow(
        "6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678");
    assert(borrowed_title && borrowed_title_again && !borrowed_title->empty());
    assert(borrowed_title->data() == borrowed_title_again->data());
    const auto verified_title = verified_dos_media.extract(
        "6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678");
    assert(verified_title && !verified_title->empty());
    assert(std::equal(verified_title->begin(), verified_title->end(), borrowed_title->begin()));
    const auto verified_dos_runtime = eon::load_millennium_dos_runtime(verified_dos_media);
    assert(verified_dos_runtime && verified_dos_runtime->language == "en");
    const auto verified_dos_assets = eon::inventory_verified_release(*english_dos);
    assert(!verified_dos_assets.empty());
    // This exercises the SDL-free factory against the genuine recognised
    // English archive whenever a developer supplied corpus is configured.
    // The factory may decode only its hash-verified leaves and must retain
    // both title pixels and the bounded title/startup evidence together.
    const auto english_dos_runtime = eon::load_millennium_dos_runtime(*english_dos);
    assert(english_dos_runtime && english_dos_runtime->language == "en");
    // P00's own codec-2 header, not the Modern renderer target tier, fixes
    // the recovered Original title at 320x200 indexed pixels.
    assert(english_dos_runtime->title.width == 320 && english_dos_runtime->title.height == 200);
    assert(english_dos_runtime->title.rgba_frames.size() == 1);
    assert(english_dos_runtime->gx_canvas && english_dos_runtime->static_game_data
        && english_dos_runtime->static_data_evidence && english_dos_runtime->title_flow
        && english_dos_runtime->sound_selection && english_dos_runtime->sound_selection_prompt
        && english_dos_runtime->game_flow && english_dos_runtime->ega_video_driver
        && english_dos_runtime->mcga_video_driver && english_dos_runtime->initial_save);
    // The executable selection table and each independently supplied driver
    // leaf are both exact-hash admissions. The runtime keeps descriptors, not
    // archive bytes or a host audio implementation.
    assert(english_dos_runtime->sound_blaster_driver && english_dos_runtime->covox_driver);
    assert(english_dos_runtime->static_game_data->celestial_labels.size() == 41);
    assert(english_dos_runtime->static_game_data->celestial_labels[4].text == "Earth ");
    assert(english_dos_runtime->static_data_evidence->pointer_count == 435);
    assert(english_dos_runtime->sound_blaster_driver->original_filename == "ssbl.drv");
    assert(english_dos_runtime->covox_driver->original_filename == "scvx.drv");
    const auto spanish_dos = std::find_if(releases.begin(), releases.end(), [](const auto& release) {
        return release.game == eon::Game::millennium
            && release.platform == eon::Platform::dos && release.language == "es";
    });
    assert(spanish_dos != releases.end());
    const auto spanish_dos_runtime = eon::load_millennium_dos_runtime(*spanish_dos);
    assert(spanish_dos_runtime && spanish_dos_runtime->language == "es");
    assert(spanish_dos_runtime->title.width == 320 && spanish_dos_runtime->title.height == 200);
    assert(spanish_dos_runtime->title.rgba_frames.size() == 1);
    assert(spanish_dos_runtime->spanish_title_boundary && spanish_dos_runtime->static_game_data
        && spanish_dos_runtime->static_data_evidence && !spanish_dos_runtime->gx_canvas
        && !spanish_dos_runtime->title_flow && !spanish_dos_runtime->game_flow
        && !spanish_dos_runtime->initial_save);
    assert(spanish_dos_runtime->static_game_data->celestial_labels[4].text == "Tierra ");
    // The coordinator publishes an adapter only after the exact outer archive
    // has been rehashed and that adapter has fully parsed its own leaves.
    eon::ResolvedLaunchRequest admitted_dos_launch;
    admitted_dos_launch.release = *english_dos;
    admitted_dos_launch.request.game = eon::Game::millennium;
    admitted_dos_launch.request.platform = eon::Platform::dos;
    admitted_dos_launch.request.release_language = "en";
    admitted_dos_launch.request.release_sha256 = english_dos->sha256;
    eon::ReleaseRuntimeCoordinator admitted_dos_runtime;
    assert(admitted_dos_runtime.acquire(admitted_dos_launch));
    assert(eon::release_runtime_admission_label(admitted_dos_runtime.admission()) == "READY");
    assert(admitted_dos_runtime.active() && admitted_dos_runtime.millennium_dos_presentation());
    assert(admitted_dos_runtime.session_snapshot());
    const auto& dos_session_snapshot = *admitted_dos_runtime.session_snapshot();
    assert(dos_session_snapshot.game == eon::Game::millennium
        && dos_session_snapshot.platform == eon::Platform::dos
        && dos_session_snapshot.language == "en"
        && dos_session_snapshot.release_sha256 == english_dos->sha256
        && dos_session_snapshot.kind == eon::RuntimeSessionKind::millennium_dos_title
        && dos_session_snapshot.boundary
            == eon::RuntimeSessionBoundary::recovered_presentation_boundary
        && dos_session_snapshot.capabilities.decoded_presentation
        && !dos_session_snapshot.capabilities.audio_observations
        && dos_session_snapshot.capabilities.admitted_input
        && dos_session_snapshot.input_contract
            == eon::RuntimeInputContract::millennium_dos_startup_observation);
    assert(eon::runtime_session_kind_label(dos_session_snapshot.kind) == "MILLENNIUM DOS TITLE");
    assert(eon::runtime_session_boundary_label(dos_session_snapshot.boundary)
        == "RECOVERED PRESENTATION BOUNDARY");
    // The lifecycle controller shares the exact direct-launch gate, but
    // turns its admitted snapshot and input boundary into explicit native
    // states. It cannot retain a boundary or adapter after menu revocation.
    eon::LaunchRequest controlled_dos_request;
    controlled_dos_request.game = eon::Game::millennium;
    controlled_dos_request.platform = eon::Platform::dos;
    controlled_dos_request.release_language = "en";
    controlled_dos_request.release_sha256 = english_dos->sha256;
    eon::NativeSessionController controlled_dos_runtime;
    assert(controlled_dos_runtime.launch_direct(controlled_dos_request, releases).accepted());
    assert(controlled_dos_runtime.state() == eon::NativeSessionState::millennium_dos_title);
    assert(controlled_dos_runtime.observe_input(eon::RuntimeInputObservation::ascii('1'))
        == eon::RuntimeInputDisposition::boundary_reached);
    assert(controlled_dos_runtime.state()
        == eon::NativeSessionState::millennium_dos_sound_driver_boundary);
    controlled_dos_runtime.reset();
    assert(controlled_dos_runtime.state() == eon::NativeSessionState::menu
        && !controlled_dos_runtime.active());
    // Return is an explicit revocation interval. No input, tick, or fresh
    // admission can reach a coordinator-owned adapter until SDL has finished
    // releasing its borrowed resources and the controller completes reset.
    assert(controlled_dos_runtime.launch_direct(controlled_dos_request, releases).accepted());
    controlled_dos_runtime.begin_return_to_menu();
    assert(controlled_dos_runtime.state() == eon::NativeSessionState::returning_to_menu);
    assert(controlled_dos_runtime.observe_input(eon::RuntimeInputObservation::ascii('1'))
        == eon::RuntimeInputDisposition::rejected);
    assert(!controlled_dos_runtime.tick_deuteros_amiga_opening());
    assert(!controlled_dos_runtime.launch_direct(controlled_dos_request, releases).accepted());
    assert(controlled_dos_runtime.state() == eon::NativeSessionState::returning_to_menu);
    controlled_dos_runtime.finish_return_to_menu();
    assert(controlled_dos_runtime.is_menu() && !controlled_dos_runtime.active());
    // The coordinator admits only the literal source-level observations for
    // the English sound chooser. It rejects an availability result while that
    // chooser is active, rejects an unknown ASCII byte, then stops at the
    // documented driver boundary for the exact `1` selection.
    const auto initial_startup_input = admitted_dos_runtime.millennium_dos_startup_input();
    assert(initial_startup_input && initial_startup_input->sound_selection_active);
    assert(admitted_dos_runtime.observe_input(eon::RuntimeInputObservation::available_character())
        == eon::RuntimeInputDisposition::rejected);
    assert(admitted_dos_runtime.observe_input(eon::RuntimeInputObservation::ascii('x'))
        == eon::RuntimeInputDisposition::ignored);
    assert(admitted_dos_runtime.observe_input(eon::RuntimeInputObservation::ascii('1'))
        == eon::RuntimeInputDisposition::boundary_reached);
    const auto selected_startup_input = admitted_dos_runtime.millennium_dos_startup_input();
    assert(selected_startup_input && selected_startup_input->selected_original_filename == "ssbl.drv");
    assert(admitted_dos_runtime.session_snapshot());
    const auto& sound_driver_snapshot = *admitted_dos_runtime.session_snapshot();
    assert(sound_driver_snapshot.kind == eon::RuntimeSessionKind::millennium_dos_sound_driver_boundary
        && sound_driver_snapshot.boundary == eon::RuntimeSessionBoundary::bootstrap_boundary
        && !sound_driver_snapshot.capabilities.decoded_presentation
        && !sound_driver_snapshot.capabilities.audio_observations
        && !sound_driver_snapshot.capabilities.admitted_input
        && sound_driver_snapshot.input_contract == eon::RuntimeInputContract::none);
    assert(eon::runtime_session_kind_label(sound_driver_snapshot.kind)
        == "MILLENNIUM DOS SOUND DRIVER BOUNDARY");
    assert(admitted_dos_runtime.observe_input(eon::RuntimeInputObservation::ascii('2'))
        == eon::RuntimeInputDisposition::rejected);
    admitted_dos_runtime.reset();
    assert(!admitted_dos_runtime.active() && !admitted_dos_runtime.millennium_dos_presentation()
        && !admitted_dos_runtime.session_snapshot());
    assert(eon::release_runtime_admission_label(admitted_dos_runtime.admission()) == "NOT SELECTED");
    // The Spanish release has no recovered sound-driver route. Its one
    // availability observation must instead become the explicit TITLES.EXE
    // return boundary and refuse further host input.
    eon::ResolvedLaunchRequest admitted_spanish_launch;
    admitted_spanish_launch.release = *spanish_dos;
    admitted_spanish_launch.request.game = eon::Game::millennium;
    admitted_spanish_launch.request.platform = eon::Platform::dos;
    admitted_spanish_launch.request.release_language = "es";
    admitted_spanish_launch.request.release_sha256 = spanish_dos->sha256;
    eon::ReleaseRuntimeCoordinator admitted_spanish_runtime;
    assert(admitted_spanish_runtime.acquire(admitted_spanish_launch));
    assert(admitted_spanish_runtime.observe_input(eon::RuntimeInputObservation::available_character())
        == eon::RuntimeInputDisposition::boundary_reached);
    assert(admitted_spanish_runtime.session_snapshot());
    const auto& spanish_handoff_snapshot = *admitted_spanish_runtime.session_snapshot();
    assert(spanish_handoff_snapshot.kind
            == eon::RuntimeSessionKind::millennium_dos_title_handoff_boundary
        && spanish_handoff_snapshot.boundary == eon::RuntimeSessionBoundary::bootstrap_boundary
        && !spanish_handoff_snapshot.capabilities.decoded_presentation
        && !spanish_handoff_snapshot.capabilities.audio_observations
        && !spanish_handoff_snapshot.capabilities.admitted_input
        && spanish_handoff_snapshot.input_contract == eon::RuntimeInputContract::none);
    assert(eon::runtime_session_kind_label(spanish_handoff_snapshot.kind)
        == "MILLENNIUM DOS TITLE HANDOFF BOUNDARY");
    assert(admitted_spanish_runtime.observe_input(eon::RuntimeInputObservation::available_character())
        == eon::RuntimeInputDisposition::rejected);
    // Every recognised outer identity admits exactly one engine-owned startup
    // adapter. Reusing the coordinator also proves a prior platform's object
    // is destroyed before the next hash-checked release can become active.
    eon::ReleaseRuntimeCoordinator all_release_runtime;
    for (const auto& release : releases) {
        const auto verified_media = eon::VerifiedReleaseMedia::open(release);
        assert(eon::verified_release_media_has_declared_profile_ranges(verified_media));
        eon::ResolvedLaunchRequest launch;
        launch.release = release;
        launch.request.game = release.game;
        launch.request.platform = release.platform;
        launch.request.release_language = release.language;
        launch.request.release_sha256 = release.sha256;
        assert(all_release_runtime.acquire(launch));
        assert(all_release_runtime.session_snapshot());
        const auto& session_snapshot = *all_release_runtime.session_snapshot();
        assert(session_snapshot.game == release.game && session_snapshot.platform == release.platform
            && session_snapshot.language == release.language
            && session_snapshot.release_sha256 == release.sha256);
        if (release.game == eon::Game::millennium && release.platform == eon::Platform::dos) {
            const auto assets = all_release_runtime.millennium_dos_presentation();
            assert(assets);
            assert(session_snapshot.kind == eon::RuntimeSessionKind::millennium_dos_title
                && session_snapshot.capabilities.decoded_presentation
                && session_snapshot.capabilities.admitted_input);
            if (release.language == "en") {
                assert(assets->assets.voice_bank && assets->assets.voice_bank->voices.size() == 14);
            } else {
                assert(!assets->assets.voice_bank);
            }
        } else if (release.game == eon::Game::millennium && release.platform == eon::Platform::amiga) {
            assert(all_release_runtime.millennium_amiga_bootstrap_presentation());
            assert(session_snapshot.kind == eon::RuntimeSessionKind::millennium_amiga_bootstrap
                && !session_snapshot.capabilities.decoded_presentation
                && !session_snapshot.capabilities.admitted_input);
        } else if (release.game == eon::Game::millennium && release.platform == eon::Platform::atari_st) {
            const auto presentation = all_release_runtime.millennium_atari_bootstrap_presentation();
            assert(presentation && presentation->config.present
                && presentation->fopen_boundary.fopen_filename == presentation->config.requested_filename);
            assert(session_snapshot.kind == eon::RuntimeSessionKind::millennium_atari_bootstrap
                && !session_snapshot.capabilities.decoded_presentation
                && !session_snapshot.capabilities.admitted_input);
            eon::LaunchRequest atari_request;
            atari_request.game = release.game;
            atari_request.platform = release.platform;
            atari_request.release_language = release.language;
            atari_request.release_sha256 = release.sha256;
            eon::RuntimeHost atari_host;
            assert(atari_host.launch_direct(atari_request, releases).accepted());
            const auto host_presentation = atari_host.millennium_atari_bootstrap_presentation();
            assert(host_presentation && host_presentation->config.present
                && host_presentation->fopen_boundary.fopen_filename
                    == presentation->fopen_boundary.fopen_filename
                && host_presentation->fread_frame_prefix.buffer_address
                    == presentation->fread_frame_prefix.buffer_address);
            atari_host.begin_source_revocation();
            assert(!atari_host.active() && !atari_host.session_snapshot()
                && !atari_host.millennium_atari_bootstrap_presentation());
            atari_host.finish_source_revocation();
        } else if (release.game == eon::Game::deuteros && release.platform == eon::Platform::amiga) {
            assert(session_snapshot.kind == eon::RuntimeSessionKind::deuteros_amiga_opening
                && session_snapshot.capabilities.decoded_presentation
                && session_snapshot.capabilities.audio_observations
                && session_snapshot.capabilities.admitted_input);
            eon::LaunchRequest opening_request;
            opening_request.game = release.game;
            opening_request.platform = release.platform;
            opening_request.release_language = release.language;
            opening_request.release_sha256 = release.sha256;

            // Exercise the production host boundary with a real, admitted
            // release. A front-end modal must not let a physical held signal
            // leak through while it owns input, and its value-only snapshot
            // must stop exposing the release as soon as teardown begins.
            eon::RuntimeHost hosted_opening;
            assert(hosted_opening.launch_direct(opening_request, releases).accepted());
            const auto hosted_admission = hosted_opening.snapshot();
            assert(hosted_admission.session
                && hosted_admission.session->release_sha256 == release.sha256
                && hosted_admission.presentation
                && hosted_admission.presentation->kind
                    == eon::RuntimePresentationKind::deuteros_amiga_opening
                && hosted_admission.presentation->input_contract
                    == eon::RuntimeInputContract::deuteros_amiga_opening_held_signal);
            const auto hosted_start = hosted_opening.advance(1'000);
            assert(hosted_start.opening_started && hosted_start.opening_active
                && hosted_start.opening.events.empty());
            assert(hosted_opening.advance(1'019).opening.events.empty());
            assert(hosted_opening.observe_input(
                eon::RuntimeInputObservation::opening_input_held(true))
                == eon::RuntimeInputDisposition::observed);
            hosted_opening.set_input_suppressed(true);
            assert(hosted_opening.input_suppressed()
                && hosted_opening.observe_input(eon::RuntimeInputObservation::opening_input_held(true))
                    == eon::RuntimeInputDisposition::rejected);
            const auto hosted_modal_tick = hosted_opening.advance(1'020);
            assert(hosted_modal_tick.opening_active && hosted_modal_tick.opening.events.size() == 1);
            hosted_opening.set_input_suppressed(false);
            assert(!hosted_opening.input_suppressed()
                && hosted_opening.observe_input(eon::RuntimeInputObservation::opening_input_held(true))
                    == eon::RuntimeInputDisposition::observed);
            hosted_opening.begin_source_revocation();
            const auto hosted_revoking = hosted_opening.snapshot();
            assert(hosted_revoking.revoking && !hosted_revoking.session
                && !hosted_revoking.presentation);
            assert(!hosted_opening.active() && !hosted_opening.session_snapshot()
                && !hosted_opening.deuteros_amiga_opening_presentation()
                && !hosted_opening.deuteros_amiga_title_stage_boundary()
                && !hosted_opening.render_deuteros_amiga_opening_audio(1));
            hosted_opening.finish_source_revocation();
            assert(hosted_opening.snapshot().state == eon::NativeSessionState::menu);

            eon::NativeSessionController opening_controller;
            assert(opening_controller.launch_direct(opening_request, releases).accepted());
            assert(opening_controller.state() == eon::NativeSessionState::deuteros_amiga_opening);
            assert(!opening_controller.deuteros_amiga_opening_checkpoint());
            assert(opening_controller.start_deuteros_amiga_opening_scheduler(1'000));
            assert(opening_controller.advance_deuteros_amiga_opening_scheduler(1'019).events.empty());
            assert(opening_controller.observe_input(
                eon::RuntimeInputObservation::opening_input_held(true))
                == eon::RuntimeInputDisposition::observed);
            const auto first_advance = opening_controller.advance_deuteros_amiga_opening_scheduler(1'020);
            assert(first_advance.events.size() == 1 && !first_advance.resynchronized);
            const auto first_checkpoint = opening_controller.deuteros_amiga_opening_checkpoint();
            assert(first_checkpoint && first_checkpoint->tick > 0
                && first_checkpoint->vblank_counter > 0
                && first_checkpoint->indexed_frame_sha256.size() == 64
                && first_checkpoint->rgba_frame_sha256.size() == 64);
            const auto opening_presentation = opening_controller.deuteros_amiga_opening_presentation();
            assert(opening_presentation && opening_presentation->checkpoint.tick == first_checkpoint->tick
                && opening_presentation->checkpoint.rgba_frame_sha256
                    == first_checkpoint->rgba_frame_sha256
                && opening_presentation->rgba_frame
                && opening_presentation->rgba_frame->size()
                    == static_cast<std::size_t>(eon::DeuterosAmigaFrame::width)
                        * eon::DeuterosAmigaFrame::height * 4U);
            assert(!opening_controller.deuteros_amiga_title_stage_boundary());
            const auto catch_up = opening_controller.advance_deuteros_amiga_opening_scheduler(1'100);
            assert(catch_up.events.size() == eon::DeuterosAmigaOpeningRunner::maximum_catch_up_ticks
                && !catch_up.resynchronized);
            const auto resynchronized = opening_controller.advance_deuteros_amiga_opening_scheduler(1'220);
            assert(resynchronized.events.size() == eon::DeuterosAmigaOpeningRunner::maximum_catch_up_ticks
                && resynchronized.resynchronized);
            assert(opening_controller.observe_input(
                eon::RuntimeInputObservation::ascii('1'))
                == eon::RuntimeInputDisposition::rejected);
            // The exact opening handoff must publish a title-stage boundary,
            // not retain an input-capable opening snapshot after its final
            // recovered frame. This loop supplies only the already admitted
            // physical held observation; it never fabricates title input.
            bool title_handoff_observed = false;
            for (std::size_t tick = 0; tick < 128 && !title_handoff_observed; ++tick) {
                const auto handoff_events = opening_controller.advance_deuteros_amiga_opening_scheduler(
                    1'240 + tick * 20U);
                title_handoff_observed = handoff_events.title_handoff;
            }
            assert(title_handoff_observed);
            assert(!opening_controller.deuteros_amiga_opening_scheduler_active()
                && opening_controller.advance_deuteros_amiga_opening_scheduler(9'999).events.empty());
            assert(opening_controller.session_snapshot());
            const auto title_snapshot = *opening_controller.session_snapshot();
            assert(title_snapshot.kind == eon::RuntimeSessionKind::deuteros_amiga_title_stage
                && title_snapshot.boundary == eon::RuntimeSessionBoundary::bootstrap_boundary
                && !title_snapshot.capabilities.decoded_presentation
                && !title_snapshot.capabilities.audio_observations
                && !title_snapshot.capabilities.admitted_input);
            assert(eon::runtime_session_kind_label(title_snapshot.kind)
                == "DEUTEROS AMIGA TITLE STAGE");
            assert(opening_controller.state()
                == eon::NativeSessionState::deuteros_amiga_title_stage_boundary);
            assert(!opening_controller.deuteros_amiga_opening_checkpoint());
            assert(!opening_controller.deuteros_amiga_opening_presentation());
            assert(!opening_controller.render_deuteros_amiga_opening_audio(960));
            const auto title_boundary = opening_controller.deuteros_amiga_title_stage_boundary();
            assert(title_boundary && title_boundary->stage.entry_address == 0x40426
                && title_boundary->original_sha256
                    == "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03"
                && title_boundary->entry_prefix_state.writes[0].address == 0x4040e
                && title_boundary->exec_prelude.stack_pointer_value == 0x40b62
                && title_boundary->local_prefix_executed
                && title_boundary->graphics_setup_palette.size() == 20
                && title_boundary->alternate_renderer_trace);
            assert(opening_controller.observe_input(
                eon::RuntimeInputObservation::opening_input_held(true))
                == eon::RuntimeInputDisposition::rejected);
            assert(!opening_controller.tick_deuteros_amiga_opening());
            assert(!opening_controller.start_deuteros_amiga_opening_scheduler(10'000));
            opening_controller.begin_return_to_menu();
            assert(!opening_controller.deuteros_amiga_opening_scheduler_active()
                && opening_controller.advance_deuteros_amiga_opening_scheduler(10'020).events.empty());
            opening_controller.finish_return_to_menu();
            assert(opening_controller.state() == eon::NativeSessionState::menu);
        } else if (release.game == eon::Game::deuteros && release.platform == eon::Platform::atari_st) {
            assert(session_snapshot.kind == eon::RuntimeSessionKind::deuteros_atari_bootstrap
                && !session_snapshot.capabilities.decoded_presentation
                && !session_snapshot.capabilities.admitted_input);
            const auto checkpoint = all_release_runtime.deuteros_atari_bootstrap_checkpoint();
            assert(checkpoint && checkpoint->relocated_dispatcher_address == 0x1ec4
                && checkpoint->state1_xbios_selector == 0x26);
            eon::LaunchRequest atari_request;
            atari_request.game = release.game;
            atari_request.platform = release.platform;
            atari_request.release_language = release.language;
            atari_request.release_sha256 = release.sha256;
            eon::NativeSessionController atari_controller;
            assert(atari_controller.launch_direct(atari_request, releases).accepted());
            assert(atari_controller.state() == eon::NativeSessionState::deuteros_atari_bootstrap);
            const auto controller_checkpoint = atari_controller.deuteros_atari_bootstrap_checkpoint();
            assert(controller_checkpoint
                && controller_checkpoint->first_stage_sha256 == checkpoint->first_stage_sha256
                && controller_checkpoint->second_stage_sha256 == checkpoint->second_stage_sha256);
            const auto controller_presentation = atari_controller.deuteros_atari_bootstrap_presentation();
            assert(controller_presentation
                && controller_presentation->checkpoint.first_stage_sha256
                    == controller_checkpoint->first_stage_sha256
                && controller_presentation->first_stage_disk_offset == 0x9d800
                && controller_presentation->first_stage_length == 0x1200
                && controller_presentation->copy_execution.relocated_entry_address == 0x1ec4
                && controller_presentation->entry_execution.stop_before_dispatcher_source_offset == 0xc4);
            atari_controller.reset();
            assert(atari_controller.is_menu() && !atari_controller.deuteros_atari_bootstrap_checkpoint()
                && !atari_controller.deuteros_atari_bootstrap_presentation());
        } else {
            assert(false && "unrecognised release reached runtime admission");
        }
        if (release.game != eon::Game::millennium || release.platform != eon::Platform::dos) {
            assert(all_release_runtime.observe_input(eon::RuntimeInputObservation::ascii('1'))
                == eon::RuntimeInputDisposition::rejected);
        }
        // No platform-specific direct accessor may bypass the host revocation
        // interval. Exercise the complete public façade for every recognised
        // source, rather than only the presentation that happened to be
        // visible in the preceding adapter-specific assertions.
        eon::RuntimeHost revocation_host;
        eon::LaunchRequest revocation_request;
        revocation_request.game = release.game;
        revocation_request.platform = release.platform;
        revocation_request.release_language = release.language;
        revocation_request.release_sha256 = release.sha256;
        assert(revocation_host.launch_direct(revocation_request, releases).accepted());
        revocation_host.begin_source_revocation();
        assert(revocation_host.revoking()
            && !revocation_host.millennium_dos_presentation()
            && !revocation_host.millennium_dos_startup_input()
            && !revocation_host.render_deuteros_amiga_opening_audio(1)
            && !revocation_host.deuteros_amiga_opening_presentation()
            && !revocation_host.deuteros_amiga_title_stage_boundary()
            && !revocation_host.deuteros_atari_bootstrap_checkpoint()
            && !revocation_host.deuteros_atari_bootstrap_presentation()
            && !revocation_host.millennium_amiga_bootstrap_presentation()
            && !revocation_host.millennium_atari_bootstrap_presentation());
        revocation_host.finish_source_revocation();
        assert(revocation_host.is_menu());
    }
    all_release_runtime.reset();
    assert(!all_release_runtime.session_snapshot());
    assert(!all_release_runtime.active() && !all_release_runtime.millennium_dos_presentation()
        && !all_release_runtime.millennium_amiga_bootstrap_presentation());
    auto forged_release_metadata = *english_dos;
    forged_release_metadata.language = "es";
    assert(!eon::is_recognised_release_identity(forged_release_metadata));
    bool rejected_forged_release_metadata = false;
    try {
        eon::verify_release_archive(forged_release_metadata);
    } catch (const std::runtime_error&) {
        rejected_forged_release_metadata = true;
    }
    assert(rejected_forged_release_metadata);
    bool rejected_forged_diagnostics = false;
    try {
        static_cast<void>(eon::runtime_diagnostics_for_release(forged_release_metadata));
    } catch (const std::runtime_error&) {
        rejected_forged_diagnostics = true;
    }
    assert(rejected_forged_diagnostics);
    auto forged_release_hash = *english_dos;
    forged_release_hash.sha256.assign(64, '0');
    assert(!eon::is_recognised_release_identity(forged_release_hash));
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
    assert(direct_archive_scanner.report().size_rejected_candidates == 0);
    assert(direct_archive_scanner.report().size_candidates == 1);
    assert(direct_archive_scanner.report().hashed_candidates == 1);
    assert(direct_archive_scanner.report().hash_rejected_candidates == 0);
    assert(direct_archive_scanner.report().verified_occurrences == 1);
    assert(direct_archive_scanner.report().verified_direct_media_occurrences == 0);
    assert(direct_archive_scanner.unbound_direct_media().empty());
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
    assert(title_lib.source_sha256()
        == "6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678");
    assert(title_lib.directory_offset() == 0x4813);
    assert(title_lib.entries().size() == 38);
    assert(title_lib.entries().front().name == "P00");
    assert(title_lib.entries().front().offset == 6);
    assert(title_lib.entries().front().size == 10'555);
    assert(title_lib.read(title_lib.entries().front()).data() == title_bytes->data() + 6);
    assert(gx_lib.read(gx_lib.entries().front()).data()
        == gx_bytes->data() + gx_lib.entries().front().offset);
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
    assert(title_palette.auxiliary_translation.source_offset == 0x28f3);
    assert(title_palette.auxiliary_translation.length == 36);
    assert(eon::to_hex(eon::sha256(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(title_palette.dac_rgb6.data()), 768)))
        == "b6dd34314102e429fdd98390b1fda27d3ea94d16bfcefa2983e3e319a2a20eae");
    assert(eon::to_hex(eon::sha256(title_palette.logical_to_dac))
        == "cd7a7f81dd75249a8669e0f4c1792d99b37f3ea28c54319a3f2e84b4a86ff3e2");
    assert(title_palette.auxiliary_translation.sha256
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
    const auto title_presentation = eon::parse_millennium_dos_title_presentation_assets(
        title_lib, title_flow);
    assert(title_presentation.title_library_sha256 == title_lib.source_sha256());
    assert(title_presentation.base_resource_name == "P00");
    assert(title_presentation.base_resource_offset == 0x000006);
    assert(title_presentation.base_resource_size == 10'555);
    assert(title_presentation.base_resource_sha256
        == "14ca6d3c86eba5e9e2afaed21fca9fc6dd1da9e357e305859a495b6dfd69919d");
    assert(title_presentation.base_rgba == title_rgba);
    assert(title_presentation.transition.patches.size() == 37);
    assert(title_presentation.transition.patches.front().resource_name == "P01");
    assert(title_presentation.transition.patches.back().resource_name == "P25");
    auto altered_titles = *titles_bytes;
    altered_titles.back() ^= 0x01;
    bool rejected_altered_titles = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_title_flow(altered_titles, *mill_bytes));
    } catch (const std::runtime_error&) {
        rejected_altered_titles = true;
    }
    assert(rejected_altered_titles);
    auto altered_launcher = *mill_bytes;
    altered_launcher.back() ^= 0x01;
    bool rejected_altered_launcher = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_title_flow(*titles_bytes, altered_launcher));
    } catch (const std::runtime_error&) {
        rejected_altered_launcher = true;
    }
    assert(rejected_altered_launcher);
    const auto title_exit = eon::parse_millennium_dos_title_exit_closure(*titles_bytes);
    const auto millennium_title_transition = eon::parse_millennium_dos_title_transition(title_lib, title_flow);
    auto altered_title_library_bytes = *title_bytes;
    // P00 is outside the P01..P25 transition bank, so this exercises the
    // complete-leaf gate rather than merely breaking a decoded patch record.
    altered_title_library_bytes[6] ^= 0x01;
    const eon::MillenniumDosLib altered_title_library(std::move(altered_title_library_bytes));
    bool rejected_altered_title_library = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_title_transition(
            altered_title_library, title_flow));
    } catch (const std::runtime_error&) {
        rejected_altered_title_library = true;
    }
    assert(rejected_altered_title_library);
    bool rejected_altered_title_presentation = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_title_presentation_assets(
            altered_title_library, title_flow));
    } catch (const std::runtime_error&) {
        rejected_altered_title_presentation = true;
    }
    assert(rejected_altered_title_presentation);
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
    assert(millennium_title_transition.source_bank_offset == 0x2941);
    assert(millennium_title_transition.source_bank_size == 7'890);
    assert(millennium_title_transition.source_bank_sha256
        == "f0ecbfd374b1c6122b407b29a6fe4a872a45a0a21e9ef6584e74829e06b5514d");
    assert(title_lib.bytes().subspan(millennium_title_transition.source_bank_offset,
        millennium_title_transition.source_bank_size).data()
        == title_lib.read(*title_p01).data());
    assert(millennium_title_transition.patches.front().resource_name == "P01");
    assert(millennium_title_transition.patches.front().source_offset == 0x2941);
    assert(millennium_title_transition.patches.front().source_size == 213);
    assert(millennium_title_transition.patches.front().source_sha256
        == "ed4cf68627d93c10545d741facfa43701774e0bb8fa28c14292877dc81b556b2");
    assert(millennium_title_transition.patches.front().bitmap.width == 16);
    assert(millennium_title_transition.patches.front().bitmap.height == 23);
    assert(eon::to_hex(eon::sha256(millennium_title_transition.patches.front().bitmap.pixels))
        == "330db310a838487f4afea0011c1ba5f381e4ed7ad97d95e4745e7be2d2d8aaa1");
    assert((millennium_title_transition.patches[1].mode_two_logical_to_dac
        == std::vector<std::uint8_t>{0x00, 0xcc, 0x00}));
    assert(millennium_title_transition.patches.back().resource_name == "P25");
    assert(millennium_title_transition.patches.back().source_offset == 0x473e);
    assert(millennium_title_transition.patches.back().source_size == 213);
    assert(millennium_title_transition.patches.back().source_sha256
        == "b523a32da572fe7e5e93ad5f8b51675c04d85053934e051d03223a9fa1e19ba1");
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
    assert(sound_selection.prompt_byte_count == 155);
    assert(sound_selection.prompt_sha256
        == "d84297ee58abeaa4ca09d60a533fe0b05ea4b805af46629d32c031b11700cad0");
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
    const auto original_sound_prompt = eon::extract_millennium_dos_sound_selection_prompt(
        *mill_bytes, sound_selection);
    assert(original_sound_prompt.size() == 154);
    assert(original_sound_prompt.ends_with('$') == false);
    assert(original_sound_prompt.find("0 = IBM Speaker") != std::string::npos);
    auto altered_sound_prompt = *mill_bytes;
    altered_sound_prompt[0x307] ^= 1U;
    bool rejected_sound_prompt = false;
    try {
        static_cast<void>(eon::extract_millennium_dos_sound_selection_prompt(
            altered_sound_prompt, sound_selection));
    } catch (const std::runtime_error&) {
        rejected_sound_prompt = true;
    }
    assert(rejected_sound_prompt);
    eon::MillenniumDosSoundSelectionSession ibm_sound_session(sound_selection);
    assert(ibm_sound_session.awaiting_choice());
    assert(!ibm_sound_session.accept_ascii_character('x'));
    assert(ibm_sound_session.awaiting_choice());
    assert(ibm_sound_session.accept_ascii_character('0'));
    assert(!ibm_sound_session.awaiting_choice());
    assert(ibm_sound_session.choice() == eon::MillenniumDosSoundEffectChoice::ibm_speaker);
    assert(ibm_sound_session.selected_table_slot() == 0);
    assert(ibm_sound_session.selected_original_filename() == "sibm.drv");
    // The parser proves only literal character input. A second character must
    // not become an invented re-selection/driver switch in the host runtime.
    assert(!ibm_sound_session.accept_ascii_character('1'));
    assert(ibm_sound_session.selected_original_filename() == "sibm.drv");
    eon::MillenniumDosSoundSelectionSession sound_blaster_session(sound_selection);
    assert(sound_blaster_session.accept_ascii_character('1'));
    assert(sound_blaster_session.choice() == eon::MillenniumDosSoundEffectChoice::sound_blaster);
    assert(sound_blaster_session.selected_table_slot() == 3);
    assert(sound_blaster_session.selected_original_filename() == "ssbl.drv");
    eon::MillenniumDosSoundSelectionSession covox_sound_session(sound_selection);
    assert(covox_sound_session.accept_ascii_character('2'));
    assert(covox_sound_session.choice() == eon::MillenniumDosSoundEffectChoice::covox_sound_master);
    assert(covox_sound_session.selected_table_slot() == 4);
    assert(covox_sound_session.selected_original_filename() == "scvx.drv");
    const auto sound_blaster = eon::extract_asset_by_sha256(english_dos->path,
        "be5a00e0b71d893a3aeaaa1127b1e5b870fe734dc876e636c6a933b6444f1b72");
    const auto covox = eon::extract_asset_by_sha256(english_dos->path,
        "99e110b91534206a6b83680a3e11cceadd0e5ddf863560aed53dcbd2c49df7c4");
    assert(sound_blaster && covox);
    const auto sound_blaster_leaf = eon::admit_millennium_dos_sound_driver_leaf(*sound_blaster);
    const auto covox_leaf = eon::admit_millennium_dos_sound_driver_leaf(*covox);
    assert(sound_blaster_leaf.kind == eon::MillenniumDosSoundDriverKind::sound_blaster);
    assert(sound_blaster_leaf.original_filename == "ssbl.drv" && sound_blaster_leaf.byte_size == 9194);
    assert(sound_blaster_leaf.sha256
        == "be5a00e0b71d893a3aeaaa1127b1e5b870fe734dc876e636c6a933b6444f1b72");
    assert(covox_leaf.kind == eon::MillenniumDosSoundDriverKind::covox_sound_master);
    assert(covox_leaf.original_filename == "scvx.drv" && covox_leaf.byte_size == 4053);
    assert(covox_leaf.sha256
        == "99e110b91534206a6b83680a3e11cceadd0e5ddf863560aed53dcbd2c49df7c4");
    eon::MillenniumDosSoundSelectionSession admitted_ibm_sound_session(
        sound_selection, sound_blaster_leaf, covox_leaf);
    assert(admitted_ibm_sound_session.accept_ascii_character('0'));
    assert(!admitted_ibm_sound_session.selected_driver_is_admitted());
    assert(!admitted_ibm_sound_session.selected_driver());
    eon::MillenniumDosSoundSelectionSession admitted_sound_blaster_session(
        sound_selection, sound_blaster_leaf, covox_leaf);
    assert(admitted_sound_blaster_session.accept_ascii_character('1'));
    assert(admitted_sound_blaster_session.selected_driver_is_admitted());
    assert(admitted_sound_blaster_session.selected_driver()
        && admitted_sound_blaster_session.selected_driver()->sha256 == sound_blaster_leaf.sha256);
    eon::MillenniumDosSoundSelectionSession admitted_covox_sound_session(
        sound_selection, sound_blaster_leaf, covox_leaf);
    assert(admitted_covox_sound_session.accept_ascii_character('2'));
    assert(admitted_covox_sound_session.selected_driver_is_admitted());
    assert(admitted_covox_sound_session.selected_driver()
        && admitted_covox_sound_session.selected_driver()->sha256 == covox_leaf.sha256);
    auto mismatched_sound_leaf = sound_blaster_leaf;
    mismatched_sound_leaf.original_filename = "scvx.drv";
    bool rejected_mismatched_sound_leaf = false;
    try {
        static_cast<void>(eon::MillenniumDosSoundSelectionSession(
            sound_selection, mismatched_sound_leaf, covox_leaf));
    } catch (const std::runtime_error&) {
        rejected_mismatched_sound_leaf = true;
    }
    assert(rejected_mismatched_sound_leaf);
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
    // A byte outside every parsed instruction prefix is still part of the
    // original driver identity. Do not let a look-alike dispatch table cross
    // the hash boundary merely because the local anchors happen to match.
    auto altered_mcga = *mcga;
    altered_mcga.back() ^= 0x01;
    bool rejected_altered_mcga = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_video_driver(altered_mcga,
            eon::MillenniumDosVideoDriverKind::mcga));
    } catch (const std::runtime_error&) {
        rejected_altered_mcga = true;
    }
    assert(rejected_altered_mcga);
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
    // Revisiting the title constructs a new one-shot session; an observed
    // hand-off from an earlier visit cannot leak into it.
    eon::MillenniumDosTitleSession restarted_title_session(title_flow);
    assert(!restarted_title_session.handed_off());
    assert(restarted_title_session.poll_console(true));
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
    const auto english_media = eon::VerifiedReleaseMedia::open(*english_dos);
    const auto voice_bank = eon::parse_millennium_dos_voice_bank(english_media);
    assert(voice_bank.name_table.table_sha256 == sound_effect_names.table_sha256
        && voice_bank.voices.size() == sound_effect_names.filenames.size()
        && voice_bank.voices.front().original_filename == "SFX1.VOC"
        && voice_bank.voices.back().original_filename == "SFXE.VOC"
        && voice_bank.total_unsigned_pcm_sample_count > 0
        && !voice_bank.sample_rates.empty());
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
    // The post-adapter continuation cannot pass even its first local call
    // unless a genuine trace observed the overlay's RETF and that call's
    // return.  Once all six returns and the original mode byte are observed,
    // it reaches the selected existing private-INT boundary without assigning
    // an effect to any of the opaque calls.
    const auto post_overlay_before_return = eon::evaluate_millennium_dos_post_overlay_continuation(
        *game_executable);
    assert(post_overlay_before_return.outcome
        == eon::MillenniumDosPostOverlayContinuationOutcome::adapter_return_boundary);
    assert(post_overlay_before_return.boundary_address == 0xd373);
    const auto post_overlay_first_call = eon::evaluate_millennium_dos_post_overlay_continuation(
        *game_executable, true);
    assert(post_overlay_first_call.outcome
        == eon::MillenniumDosPostOverlayContinuationOutcome::local_call_boundary);
    assert(post_overlay_first_call.boundary_address == 0xd376);
    const auto post_overlay_fourth_call = eon::evaluate_millennium_dos_post_overlay_continuation(
        *game_executable, true, {true, true, true, false, false, false});
    assert(post_overlay_fourth_call.boundary_address == 0xd37f);
    const auto post_overlay_mode_boundary = eon::evaluate_millennium_dos_post_overlay_continuation(
        *game_executable, true, {true, true, true, true, true, true});
    assert(post_overlay_mode_boundary.outcome
        == eon::MillenniumDosPostOverlayContinuationOutcome::mode_byte_boundary);
    assert(post_overlay_mode_boundary.boundary_address == 0xd388);
    const auto post_overlay_equal = eon::evaluate_millennium_dos_post_overlay_continuation(
        *game_executable, true, {true, true, true, true, true, true}, 1);
    assert(post_overlay_equal.outcome
        == eon::MillenniumDosPostOverlayContinuationOutcome::private_interrupt_boundary);
    assert(post_overlay_equal.selected_callee_address == 0xd1a1);
    assert(post_overlay_equal.selected_private_call_address == 0xd1a9);
    assert(post_overlay_equal.private_wrapper_address == 0x0124);
    assert(post_overlay_equal.private_interrupt == 0x91);
    assert(post_overlay_equal.boundary_address == 0x0129);
    const auto post_overlay_other = eon::evaluate_millennium_dos_post_overlay_continuation(
        *game_executable, true, {true, true, true, true, true, true}, 3);
    assert(post_overlay_other.selected_callee_address == 0xd1b5);
    assert(post_overlay_other.selected_private_call_address == 0xd1bd);
    bool rejected_post_overlay_out_of_order_mode = false;
    try {
        static_cast<void>(eon::evaluate_millennium_dos_post_overlay_continuation(
            *game_executable, true, {}, 1));
    } catch (const std::runtime_error&) {
        rejected_post_overlay_out_of_order_mode = true;
    }
    assert(rejected_post_overlay_out_of_order_mode);
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
    // The connected English startup evaluator runs only bytes following
    // explicitly observed private-INT returns.  It stops at each unobserved
    // ABI boundary and reports local stores without touching original media.
    const auto english_initial_boundary = eon::evaluate_millennium_dos_english_startup_prefix(
        *game_executable);
    assert(english_initial_boundary.outcome
        == eon::MillenniumDosEnglishStartupPrefixOutcome::first_private_interrupt_boundary);
    assert(english_initial_boundary.entry_address == 0xd2b0);
    assert(english_initial_boundary.stack_pointer == 0xda00);
    assert(english_initial_boundary.first_private_call_address == 0xd2c5);
    assert(english_initial_boundary.private_wrapper_address == 0x0124);
    assert(english_initial_boundary.private_interrupt == 0x91);
    assert(english_initial_boundary.boundary_address == 0x0129);
    const auto english_selected_boundary = eon::evaluate_millennium_dos_english_startup_prefix(
        *game_executable, 0x0100);
    assert(english_selected_boundary.outcome
        == eon::MillenniumDosEnglishStartupPrefixOutcome::selected_private_interrupt_boundary);
    assert(english_selected_boundary.selector_byte == 1);
    assert(english_selected_boundary.selected_entry_address == 0xd1a1);
    assert(english_selected_boundary.selected_private_call_address == 0xd1a9);
    assert((english_selected_boundary.local_writes == std::vector<eon::MillenniumDosEnglishStartupPrefixWrite>{
        {0xd128, 0x0100, 2}, {0x4368, 1, 1}, {0xda05, 1, 1}, {0xd12c, 0xda00, 2}}));
    const auto english_equal_return = eon::evaluate_millennium_dos_english_startup_prefix(
        *game_executable, 0x0100, 0xffff);
    assert(english_equal_return.outcome
        == eon::MillenniumDosEnglishStartupPrefixOutcome::equal_return);
    assert(english_equal_return.boundary_address == 0x0455);
    assert((english_equal_return.local_writes.back()
        == eon::MillenniumDosEnglishStartupPrefixWrite{0xda05, 1, 1}));
    const auto english_palette_boundary = eon::evaluate_millennium_dos_english_startup_prefix(
        *game_executable, 0x0200, 0);
    assert(english_palette_boundary.outcome
        == eon::MillenniumDosEnglishStartupPrefixOutcome::palette_bios_interrupt_boundary);
    assert(english_palette_boundary.selected_entry_address == 0xd1b5);
    assert(english_palette_boundary.boundary_address == 0x0476);
    assert((english_palette_boundary.first_palette_request
        == eon::MillenniumDosEgaPaletteRegisterWrite{0, 0}));
    assert((english_palette_boundary.local_writes.back()
        == eon::MillenniumDosEnglishStartupPrefixWrite{0x0107, 0xb800, 2}));
    bool rejected_out_of_order_startup_observation = false;
    try {
        static_cast<void>(eon::evaluate_millennium_dos_english_startup_prefix(
            *game_executable, std::nullopt, 0));
    } catch (const std::runtime_error&) {
        rejected_out_of_order_startup_observation = true;
    }
    assert(rejected_out_of_order_startup_observation);
    // The next real startup fragment is separately connected from the
    // conditional GX-loader return. It accepts only observed native values,
    // applies the one original CS-store, and stops before the overlay's far
    // transfer rather than fabricating a loader/INT result.
    const auto post_gx_initial = eon::evaluate_millennium_dos_post_gx_startup_prefix(
        *game_executable);
    assert(post_gx_initial.outcome
        == eon::MillenniumDosPostGxStartupPrefixOutcome::private_interrupt_boundary);
    assert(post_gx_initial.entry_address == 0xd338);
    assert(post_gx_initial.private_call_address == 0xd340);
    assert(post_gx_initial.private_wrapper_address == 0x0124);
    assert(post_gx_initial.private_interrupt == 0x91);
    assert(post_gx_initial.boundary_address == 0x0129);
    const auto post_gx_default = eon::evaluate_millennium_dos_post_gx_startup_prefix(
        *game_executable, 0x1234, 0);
    assert(post_gx_default.outcome
        == eon::MillenniumDosPostGxStartupPrefixOutcome::overlay_adapter_boundary);
    assert(post_gx_default.selected_ax == 0x000e);
    assert(post_gx_default.selected_dx == 0x0028);
    assert((post_gx_default.local_writes
        == std::vector<eon::MillenniumDosPostGxStartupPrefixWrite>{{0x4b6e, 0x0028, 2}}));
    assert(post_gx_default.boundary_address == 0xd373);
    const auto post_gx_three = eon::evaluate_millennium_dos_post_gx_startup_prefix(
        *game_executable, 0, 3);
    assert(post_gx_three.selected_ax == 0x0012);
    assert(post_gx_three.selected_dx == 0x0050);
    const auto post_gx_four = eon::evaluate_millennium_dos_post_gx_startup_prefix(
        *game_executable, 0, 4);
    assert(post_gx_four.selected_ax == 0x0014);
    assert(post_gx_four.selected_dx == 0x00a0);
    const auto post_gx_two = eon::evaluate_millennium_dos_post_gx_startup_prefix(
        *game_executable, 0, 2);
    assert(post_gx_two.selected_ax == 0x000f);
    assert(post_gx_two.selected_dx == 0x0140);
    bool rejected_post_gx_out_of_order_observation = false;
    try {
        static_cast<void>(eon::evaluate_millennium_dos_post_gx_startup_prefix(
            *game_executable, std::nullopt, 3));
    } catch (const std::runtime_error&) {
        rejected_post_gx_out_of_order_observation = true;
    }
    assert(rejected_post_gx_out_of_order_observation);
    bool rejected_post_gx_missing_mode_observation = false;
    try {
        static_cast<void>(eon::evaluate_millennium_dos_post_gx_startup_prefix(
            *game_executable, 0, std::nullopt));
    } catch (const std::runtime_error&) {
        rejected_post_gx_missing_mode_observation = true;
    }
    assert(rejected_post_gx_missing_mode_observation);
    // The four selected GX startup entries are call-free and return through
    // the original adapter.  The evaluator advances there only after both
    // the private-wrapper and adapter returns were explicitly observed; all
    // reconstructed writes remain overlay-relative.
    const auto gx_startup_initial = eon::evaluate_millennium_dos_gx_overlay_startup(
        *game_executable, *gx_overlay);
    assert(gx_startup_initial.outcome
        == eon::MillenniumDosGxOverlayStartupOutcome::private_interrupt_boundary);
    assert(gx_startup_initial.boundary_address == 0x0129);
    const auto gx_startup_adapter = eon::evaluate_millennium_dos_gx_overlay_startup(
        *game_executable, *gx_overlay, 0, 3);
    assert(gx_startup_adapter.outcome
        == eon::MillenniumDosGxOverlayStartupOutcome::overlay_adapter_boundary);
    assert(gx_startup_adapter.selected_ax == 0x0012);
    assert(gx_startup_adapter.boundary_address == 0xd373);
    const auto gx_startup_default = eon::evaluate_millennium_dos_gx_overlay_startup(
        *game_executable, *gx_overlay, 0, 0, true);
    assert(gx_startup_default.outcome
        == eon::MillenniumDosGxOverlayStartupOutcome::overlay_return);
    assert(gx_startup_default.selected_overlay_entry_offset == 0x0090);
    assert(gx_startup_default.selected_source_record_offset == 0x0070);
    assert(gx_startup_default.boundary_address == 0xd376);
    assert((gx_startup_default.overlay_writes
        == std::vector<eon::MillenniumDosGxOverlayStartupWrite>{
            {0x0065, 0x0dc7, 2}, {0x0067, 0x0024, 2}, {0x0069, 0x05a0, 2},
            {0x006b, 0x05a2, 2}, {0x006d, 0x00, 1}, {0x00f4, 0x00f4, 2},
            {0x00f0, 0x00f2, 2}, {0x00f2, 0x00f6, 2}, {0x005c, 0x47ea, 2}}));
    const auto gx_startup_two = eon::evaluate_millennium_dos_gx_overlay_startup(
        *game_executable, *gx_overlay, 0, 2, true);
    assert(gx_startup_two.selected_overlay_entry_offset == 0x009f);
    assert(gx_startup_two.selected_source_record_offset == 0x0078);
    assert((gx_startup_two.overlay_writes.front()
        == eon::MillenniumDosGxOverlayStartupWrite{0x005a, 0xb800, 2}));
    assert((gx_startup_two.overlay_writes[5]
        == eon::MillenniumDosGxOverlayStartupWrite{0x006d, 3, 1}));
    const auto gx_startup_three = eon::evaluate_millennium_dos_gx_overlay_startup(
        *game_executable, *gx_overlay, 0, 3, true);
    assert(gx_startup_three.selected_overlay_entry_offset == 0x0097);
    assert(gx_startup_three.selected_source_record_offset == 0x0080);
    assert((gx_startup_three.overlay_writes[4]
        == eon::MillenniumDosGxOverlayStartupWrite{0x006d, 2, 1}));
    const auto gx_startup_four = eon::evaluate_millennium_dos_gx_overlay_startup(
        *game_executable, *gx_overlay, 0, 4, true);
    assert(gx_startup_four.selected_overlay_entry_offset == 0x00a7);
    assert(gx_startup_four.selected_source_record_offset == 0x0088);
    assert((gx_startup_four.overlay_writes[4]
        == eon::MillenniumDosGxOverlayStartupWrite{0x006d, 1, 1}));
    eon::MillenniumDosGxStartupSession gx_session(*game_executable, *gx_overlay);
    assert(gx_session.state() == eon::MillenniumDosGxStartupSessionState::awaiting_private_return);
    bool rejected_gx_mode_before_private_return = false;
    try { gx_session.observe_mode_byte(3); } catch (const std::runtime_error&) { rejected_gx_mode_before_private_return = true; }
    assert(rejected_gx_mode_before_private_return);
    gx_session.observe_private_return(0);
    gx_session.observe_mode_byte(3);
    assert(gx_session.evaluation()->selected_ax == 0x0012);
    assert(!gx_session.overlay_byte(0x65));
    gx_session.observe_adapter_return();
    assert(gx_session.state()
        == eon::MillenniumDosGxStartupSessionState::awaiting_post_overlay_call_returns);
    assert(gx_session.evaluation()->boundary_address == 0xd376);
    assert(gx_session.post_overlay_evaluation());
    assert(gx_session.post_overlay_evaluation()->boundary_address == 0xd376);
    assert(gx_session.overlay_byte(0x65) == 0x8f);
    assert(gx_session.overlay_byte(0x66) == 0x1b);
    assert(gx_session.overlay_byte(0x6d) == 2);
    bool rejected_gx_repeat_adapter_return = false;
    try { gx_session.observe_adapter_return(); } catch (const std::runtime_error&) { rejected_gx_repeat_adapter_return = true; }
    assert(rejected_gx_repeat_adapter_return);
    bool rejected_gx_mode_before_post_overlay_returns = false;
    try { gx_session.observe_post_overlay_mode_byte(1); } catch (const std::runtime_error&) {
        rejected_gx_mode_before_post_overlay_returns = true;
    }
    assert(rejected_gx_mode_before_post_overlay_returns);
    for (std::size_t call = 0; call < 6; ++call) gx_session.observe_post_overlay_call_return();
    assert(gx_session.state() == eon::MillenniumDosGxStartupSessionState::awaiting_post_overlay_mode_byte);
    assert(gx_session.observed_post_overlay_call_return_count() == 6);
    assert(gx_session.post_overlay_evaluation()->boundary_address == 0xd388);
    gx_session.observe_post_overlay_mode_byte(1);
    assert(gx_session.state()
        == eon::MillenniumDosGxStartupSessionState::post_overlay_private_interrupt_boundary);
    assert(gx_session.post_overlay_evaluation()->selected_callee_address == 0xd1a1);
    assert(gx_session.post_overlay_evaluation()->selected_private_call_address == 0xd1a9);
    bool rejected_gx_repeat_post_overlay_mode = false;
    try { gx_session.observe_post_overlay_mode_byte(1); } catch (const std::runtime_error&) {
        rejected_gx_repeat_post_overlay_mode = true;
    }
    assert(rejected_gx_repeat_post_overlay_mode);
    // Admission consumes a complete strict trace into a fresh, call-free
    // session. The strings are grammar fixtures only, not captured media;
    // genuine use additionally requires validate_reference_trace provenance.
    {
        // Every declarative trace descriptor is itself an admission boundary.
        // A truncated media identity must be rejected by this invariant before
        // it can make a real capture impossible to validate.
        const auto is_sha256 = [](const std::string_view value) {
            return value.size() == 64 && std::all_of(value.begin(), value.end(), [](const char character) {
                return (character >= '0' && character <= '9')
                    || (character >= 'a' && character <= 'f');
            });
        };
        for (const auto& descriptor : eon::reference_trace_adapter_registry()) {
            assert(is_sha256(descriptor.release_sha256));
            if (!descriptor.source_media_sha256.empty()) assert(is_sha256(descriptor.source_media_sha256));
            if (!descriptor.source_stage_sha256.empty()) assert(is_sha256(descriptor.source_stage_sha256));
        }
        const auto* title_display_v5 = eon::reference_trace_adapter_descriptor(
            "deuteros-amiga-en-title-display-artifacts-v5");
        assert(title_display_v5);
        assert(title_display_v5->source_media_sha256
            == "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38");
    }
    {
        constexpr std::string_view valid_gx_trace =
            "event\t1 10 private-return image=2200ad.exe pc=0x0129 int=0x91 ax=0x0000\n"
            "event\t2 20 mode-read image=2200ad.exe pc=0xd349 address=0xda05 value=0x03\n"
            "event\t3 30 adapter-return image=2200gx.exe pc=0x00ed op=retf return_pc=0xd376\n"
            "event\t4 40 local-return image=2200ad.exe call_pc=0xd376 return_pc=0xd379\n"
            "event\t5 50 local-return image=2200ad.exe call_pc=0xd379 return_pc=0xd37c\n"
            "event\t6 60 local-return image=2200ad.exe call_pc=0xd37c return_pc=0xd37f\n"
            "event\t7 70 local-return image=2200ad.exe call_pc=0xd37f return_pc=0xd382\n"
            "event\t8 80 local-return image=2200ad.exe call_pc=0xd382 return_pc=0xd385\n"
            "event\t9 90 local-return image=2200ad.exe call_pc=0xd385 return_pc=0xd388\n"
            "event\t10 100 mode-read image=2200ad.exe pc=0xd388 address=0xda05 value=0x01\n";
        const auto admission = eon::admit_millennium_dos_gx_startup_trace(
            *game_executable, *gx_overlay, valid_gx_trace);
        assert(admission.session && admission.error.empty());
        assert(admission.session->state()
            == eon::MillenniumDosGxStartupSessionState::post_overlay_private_interrupt_boundary);
        assert(admission.session->overlay_byte(0x65) == 0x8f);
        const auto incomplete_admission = eon::admit_millennium_dos_gx_startup_trace(
            *game_executable, *gx_overlay,
            "event\t1 10 private-return image=2200ad.exe pc=0x0129 int=0x91 ax=0x0000\n");
        assert(!incomplete_admission.session && !incomplete_admission.error.empty());

        // The release-runtime gate owns the second rehash and leaf admission
        // for a trace. These are grammar fixtures paired with the supplied,
        // genuine archive—not replacement media or an emulation request.
        const auto trace_events_path = std::filesystem::path(std::getenv("EON_TEST_TMPDIR"))
            / "gx-runtime-gate-events.eontrace";
        {
            std::ofstream output(trace_events_path, std::ios::binary | std::ios::trunc);
            output << valid_gx_trace;
        }
        eon::ReferenceTrace runtime_trace;
        runtime_trace.source_release = *english_dos;
        runtime_trace.events_path = trace_events_path;
        runtime_trace.adapter = "millennium-dos-en-gx-startup-v2";
        runtime_trace.event_count = 10;
        runtime_trace.event_size = valid_gx_trace.size();
        runtime_trace.event_sha256 = eon::to_hex(eon::sha256(
            std::vector<std::uint8_t>(valid_gx_trace.begin(), valid_gx_trace.end())));
        eon::ReleaseRuntimeCoordinator trace_gate;
        const auto runtime_trace_admission =
            trace_gate.admit_millennium_dos_gx_startup_reference_trace(runtime_trace);
        assert(runtime_trace_admission.session && runtime_trace_admission.error.empty());
        assert(runtime_trace_admission.session->state()
            == eon::MillenniumDosGxStartupSessionState::post_overlay_private_interrupt_boundary);
        assert(runtime_trace_admission.session->overlay_byte(0x65) == 0x8f);
        // The trace gate remains a non-launching operation: no active release,
        // adapter, session snapshot, input route, or opening tick can appear.
        assert(!trace_gate.active() && !trace_gate.session_snapshot()
            && !trace_gate.millennium_dos_presentation());
        assert(trace_gate.observe_input(eon::RuntimeInputObservation::available_character())
            == eon::RuntimeInputDisposition::rejected);
        assert(!trace_gate.tick_deuteros_amiga_opening());
        {
            std::ofstream output(trace_events_path, std::ios::binary | std::ios::trunc);
            output << valid_gx_trace << "# changed after validation\n";
        }
        const auto changed_trace_admission =
            trace_gate.admit_millennium_dos_gx_startup_reference_trace(runtime_trace);
        assert(!changed_trace_admission.session
            && changed_trace_admission.error == "Reference trace events changed after validation");
        assert(!trace_gate.active() && !trace_gate.session_snapshot()
            && !trace_gate.millennium_dos_presentation());
        std::filesystem::remove(trace_events_path);
    }
    {
        auto altered_gx_overlay = *gx_overlay;
        altered_gx_overlay[0x90] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::MillenniumDosGxStartupSession(
                *game_executable, altered_gx_overlay));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
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
            static_cast<void>(game_session.observe_first_special_action({0x07f9, 0}));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    eon::MillenniumDosGameSession observed_game_session(game_flow, *game_executable);
    const auto observed_first_special = observed_game_session.observe_first_special_action({0x07f9, 0x5a});
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
    const auto observed_shared_helper = observed_game_session
        .observe_first_special_action_shared_helper_prefix();
    assert(observed_shared_helper.entry_address == 0x0666);
    assert(observed_shared_helper.caller_ax == observed_first_special.selected_ax_value);
    assert(observed_shared_helper.shifted_ax == static_cast<std::uint16_t>(
        observed_first_special.selected_ax_value << 1U));
    assert(observed_game_session.last_shared_helper_prefix());
    {
        bool rejected = false;
        try {
            static_cast<void>(observed_game_session.observe_first_special_action_shared_helper_prefix());
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        bool rejected = false;
        try {
            static_cast<void>(observed_game_session.observe_first_special_action({0xda3a, 0x5a}));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
        assert(observed_game_session.last_first_special_action_trace());
        assert(observed_game_session.reconstructed_runtime_byte(0x07f9)
            == std::optional<std::uint8_t>{0x5b});
    }
    const auto observed_second_special = observed_game_session.observe_second_special_action({0xda3a, 1});
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
            static_cast<void>(altered_session.observe_first_special_action({0x07f9, 0}));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    // An actual F8 poll can be followed only by explicitly observed native
    // bytes.  This exercises the whole call-free prefix through the external
    // XLAT boundary and the subsequent local gate without naming a game
    // action or executing any helper.
    eon::MillenniumDosGameSession observed_f8_session(game_flow, *game_executable);
    const auto observed_f8_preflight = observed_f8_session.observe_eighth_function_key_preflight({
        .action = {0x0f05, 0x42}, .enabled_byte = {0xda39, 0}, .counter_byte = {0xda0a, 3}});
    assert(observed_f8_preflight.outcome
        == eon::MillenniumDosEighthFunctionKeyPreflightOutcome::table_jump_boundary);
    assert(observed_f8_session.last_runtime_byte_effect());
    assert(observed_f8_session.last_runtime_byte_effect()->address == 0xda30);
    // Runtime effects have no equality operator; inspect the one exact DEC.
    assert(observed_f8_session.last_eighth_function_key_runtime_effects().size() == 1);
    assert(observed_f8_session.last_eighth_function_key_runtime_effects().front().address == 0xda0a);
    assert(observed_f8_session.last_eighth_function_key_runtime_effects().front().previous
        == std::optional<std::uint8_t>{3});
    assert(observed_f8_session.reconstructed_runtime_byte(0xda0a)
        == std::optional<std::uint8_t>{2});
    const auto observed_f8_table = observed_f8_session.observe_eighth_function_key_table_jump(2);
    assert(observed_f8_table.selected_pointer == 0x7815);
    assert(observed_f8_session.last_eighth_function_key_runtime_effects().size() == 3);
    assert(observed_f8_session.reconstructed_runtime_byte(0xda09)
        == std::optional<std::uint8_t>{0});
    assert(observed_f8_session.reconstructed_runtime_byte(0xda06)
        == std::optional<std::uint8_t>{2});
    const auto observed_f8_gate = observed_f8_session.observe_eighth_function_key_selected_record_gate({0x6e2f, 0});
    assert(observed_f8_gate.outcome
        == eon::MillenniumDosEighthFunctionKeySelectedRecordOutcome::first_helper_boundary);
    assert(observed_f8_gate.first_helper_address == std::optional<std::uint16_t>{0x7924});
    eon::MillenniumDosGameSession returned_f8_session(game_flow, *game_executable);
    const auto returned_f8_preflight = returned_f8_session.observe_eighth_function_key_preflight({
        .action = {0x0f05, 0x42}, .enabled_byte = {0xda39, 0}, .counter_byte = {0xda0a, 0}});
    assert(returned_f8_preflight.outcome == eon::MillenniumDosEighthFunctionKeyPreflightOutcome::returns);
    const std::array<std::uint8_t, 2> observed_f8_helper_returns{{0x01, 0x00}};
    const auto returned_f8_loop = returned_f8_session.observe_eighth_function_key_repeat_loop(
        observed_f8_helper_returns);
    assert((returned_f8_loop.shifted_bl_values == std::vector<std::uint8_t>{0x00, 0x00}));
    assert(returned_f8_session.last_eighth_function_key_repeat_loop());
    {
        bool rejected = false;
        try {
            static_cast<void>(returned_f8_session.observe_eighth_function_key_repeat_loop(
                observed_f8_helper_returns));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        eon::MillenniumDosGameSession detached_f8_session(game_flow, *game_executable);
        bool rejected = false;
        try {
            static_cast<void>(detached_f8_session.observe_eighth_function_key_table_jump(2));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        eon::MillenniumDosGameSession altered_f8_session(game_flow, *game_executable);
        bool rejected = false;
        try {
            static_cast<void>(altered_f8_session.observe_eighth_function_key_preflight({
                .action = {0x0f05, 0x42}, .enabled_byte = {0xda39, 0}, .counter_byte = {0xda0b, 3}}));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    const auto observe_game_action = [&game_session](const std::uint8_t action) {
        return game_session.observe_action({0x0f05, action});
    };
    assert(!observe_game_action(0));
    assert(!game_session.last_function_key_index());
    assert(observe_game_action(0x3b) == std::optional<std::size_t>{0});
    assert(!game_session.last_first_function_key_trace());
    {
        bool rejected = false;
        try {
            static_cast<void>(game_session.observe_first_function_key_display_selector_return());
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    eon::MillenniumDosGameSession observed_f1_session(game_flow, *game_executable);
    assert(observed_f1_session.observe_action({0x0f05, 0x3b}) == std::optional<std::size_t>{0});
    const auto observed_f1 = observed_f1_session.observe_first_function_key_display_selector_return();
    assert(observed_f1.selected_record_address == 0x12cc);
    assert(observed_f1_session.last_first_function_key_trace());
    {
        bool rejected = false;
        try {
            static_cast<void>(observed_f1_session.observe_first_function_key_display_selector_return());
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    assert(observe_game_action(0x3c) == std::optional<std::size_t>{1});
    assert(game_session.last_second_function_key_trace());
    assert(game_session.last_second_function_key_trace()->first_record_address == 0x1384);
    assert(game_session.last_second_function_key_trace()->record_stride == 0x00c0);
    assert(!game_session.last_first_function_key_trace());
    assert(observe_game_action(0x3d) == std::optional<std::size_t>{2});
    assert(game_session.last_third_function_key_trace());
    assert(game_session.last_third_function_key_trace()->callback_address == 0x712a);
    assert(!game_session.last_second_function_key_trace());
    assert(observe_game_action(0x3e) == std::optional<std::size_t>{3});
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
    assert(observe_game_action(0x3f) == std::optional<std::size_t>{4});
    assert(game_session.last_fifth_function_key_trace());
    assert(game_session.last_fifth_function_key_trace()->third_call_address == 0x4bf7);
    assert(!game_session.last_fourth_function_key_trace());
    assert(observe_game_action(0x40) == std::optional<std::size_t>{5});
    assert(game_session.last_sixth_function_key_trace());
    assert(game_session.last_sixth_function_key_trace()->callback_word_value == 0x3207);
    // All F6 stores follow two native calls and the observed restoration path
    // itself ends at a native call. Unlike F8, no private runtime overlay is
    // justified from this trace.
    assert(!game_session.last_runtime_byte_effect());
    assert(!game_session.reconstructed_runtime_byte(0x75a8));
    assert(!game_session.reconstructed_runtime_byte(0x75ae));
    assert(!game_session.last_fifth_function_key_trace());
    assert(observe_game_action(0x41) == std::optional<std::size_t>{6});
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
    assert(observe_game_action(0x42) == std::optional<std::size_t>{7});
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
    assert(observe_game_action(0x42) == std::optional<std::size_t>{7});
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
    assert(observe_game_action(0x43) == std::optional<std::size_t>{8});
    assert(game_session.last_ninth_function_key_trace());
    assert(game_session.last_ninth_function_key_trace()->handler_address == 0x7339);
    assert(game_session.last_ninth_function_key_trace()->limit_value == 9);
    assert(!game_session.last_eighth_function_key_trace());
    assert(!game_session.last_runtime_byte_effect());
    assert(game_session.reconstructed_runtime_byte(0xda30) == std::optional<std::uint8_t>{0});
    assert(observe_game_action(0x44) == std::optional<std::size_t>{9});
    assert(game_session.last_tenth_function_key_trace());
    assert(game_session.last_tenth_function_key_trace()->handler_address == 0x7384);
    assert(game_session.last_tenth_function_key_trace()->limit_value == 2);
    assert(!game_session.last_first_function_key_trace());
    assert(!game_session.last_ninth_function_key_trace());
    assert(!observe_game_action(0x45));
    assert(!observe_game_action(0x0b));
    {
        bool rejected = false;
        try {
            static_cast<void>(game_session.observe_action({0x0f06, 0x3b}));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
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
    assert(text_catalog.records.front().source_size == 2);
    assert(text_catalog.records.front().sha256
        == "869f1dfb999a452f497a4cf7f44db2d6ee661f74a9e7e05251bc1420e50672d4");
    const auto static_data_evidence = eon::parse_millennium_dos_static_data_evidence(*static_data);
    assert(static_data_evidence.source_size == 12'494);
    assert(static_data_evidence.celestial_table_offset == 0x3d2);
    assert(static_data_evidence.celestial_label_count == 41);
    assert(static_data_evidence.pointer_count == 435);
    assert(static_data_evidence.raw_record_count == 434);
    assert(static_data_evidence.topology_anchors[2].table_index == 251);
    assert(static_data_evidence.topology_anchors[2].target_offset == 0x0ff1);
    {
        auto altered_static_data = *static_data;
        altered_static_data[251 * 2] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_dos_static_data_evidence(altered_static_data));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
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
    const auto gx_catalog = eon::inspect_millennium_dos_gx_bitmap_catalog(*gx_bytes);
    auto altered_gx_library = *gx_bytes;
    // The final directory byte is outside IMG00/IMG01; the complete-library
    // gate rejects it before any unrelated resource can be treated as source
    // evidence for the recovered gameplay canvas.
    altered_gx_library.back() ^= 0x01;
    bool rejected_altered_gx_library = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_gameplay_screen(altered_gx_library));
    } catch (const std::runtime_error&) {
        rejected_altered_gx_library = true;
    }
    assert(rejected_altered_gx_library);
    bool rejected_altered_gx_catalog = false;
    try {
        static_cast<void>(eon::inspect_millennium_dos_gx_bitmap_catalog(altered_gx_library));
    } catch (const std::runtime_error&) {
        rejected_altered_gx_catalog = true;
    }
    assert(rejected_altered_gx_catalog);
    assert(gameplay_canvas.canvas.width == 320 && gameplay_canvas.canvas.height == 167);
    assert(gameplay_canvas.palette_resource_auxiliary.length != 0);
    assert(gameplay_canvas.canvas_auxiliary.length != 0);
    const auto gx_source = std::span<const std::uint8_t>(*gx_bytes);
    assert(eon::to_hex(eon::sha256(gx_source.subspan(
        gameplay_canvas.palette_resource_auxiliary.source_offset,
        gameplay_canvas.palette_resource_auxiliary.length)))
        == gameplay_canvas.palette_resource_auxiliary.sha256);
    assert(eon::to_hex(eon::sha256(gx_source.subspan(
        gameplay_canvas.canvas_auxiliary.source_offset,
        gameplay_canvas.canvas_auxiliary.length)))
        == gameplay_canvas.canvas_auxiliary.sha256);
    assert(gx_catalog.source_sha256 == "4adf9991226deab4749ac07ad637851994f57d11f6dc45f3f5ce862b5bc34c2f");
    assert(gx_catalog.resource_count == 180 && gx_catalog.resources.size() == 180);
    assert(gx_catalog.bitmap_decoder_admitted_count + gx_catalog.bitmap_decoder_boundary_count
        == gx_catalog.resource_count);
    assert(gx_catalog.decoded_pixel_count > 500'000);
    assert(gx_catalog.resources.front().name == "IMG00");
    assert(gx_catalog.resources.front().bitmap_decoder_admitted);
    assert(gx_catalog.resources[1].name == "IMG01");
    assert(gx_catalog.resources[1].bitmap_decoder_admitted);
    assert(gx_catalog.resources[25].name == "IMG19");
    assert(gx_catalog.resources[25].source_sha256
        == "e86a92133716dc7a54cc4d113a72af25d307c0e338bf77491205d19493403838");
    assert(!gx_catalog.resources[25].bitmap_decoder_admitted);
    assert(gx_catalog.resources[25].decoder_boundary == "Millennium DOS bitmap run overruns output");
    assert(gx_catalog.resources[25].decoded_pixels_sha256.empty());
    assert(gx_catalog.resources[1].width == 320 && gx_catalog.resources[1].height == 167);
    assert(gx_catalog.resources.back().name == "IMGB3");
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
    auto altered_last_library = *last_bytes;
    // The directory tail is not part of the single bitmap resource.  The
    // leaf-identity gate must still reject it before rendering begins.
    altered_last_library.back() ^= 0x01;
    bool rejected_altered_last_library = false;
    try {
        static_cast<void>(eon::parse_millennium_dos_last_screen(altered_last_library));
    } catch (const std::runtime_error&) {
        rejected_altered_last_library = true;
    }
    assert(rejected_altered_last_library);
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
    assert(spanish_title_boundary.input_interrupt == 0x21);
    assert(spanish_title_boundary.input_service == 0x06);
    assert(spanish_title_boundary.input_parameter == 0xff);
    assert(spanish_title_boundary.input_nonzero_exit_address == 0x1c54);
    assert(spanish_title_boundary.post_title_entry_address == 0x1968);
    assert(spanish_title_boundary.private_driver_function == 0x13);
    assert(spanish_title_boundary.private_driver_call_count == 5);
    const auto spanish_title_presentation =
        eon::parse_millennium_dos_spanish_title_presentation_evidence(
            disk.read(*spanish_titles), disk.read(*spanish_title));
    assert(spanish_title_presentation.titles_sha256
        == "02082c35e18cee330f7d1b88098f502e68011f7e47a3a649961f6f03d1d14fe7");
    assert(spanish_title_presentation.title_library_sha256
        == "30d6ccb95e7f501d59e72fc2e34583302116bd88f6eceaae989f6ad986ef7f19");
    assert(spanish_title_presentation.selection_entry_address == 0x1c14);
    assert(spanish_title_presentation.selected_resource_index == 0);
    assert(spanish_title_presentation.selection_callee_address == 0x1725);
    assert(spanish_title_presentation.codec_call_address == 0x1c1a);
    assert(spanish_title_presentation.codec_call_target == 0x1004);
    assert(spanish_title_presentation.transition_call_address == 0x1c1d);
    assert(spanish_title_presentation.transition_call_target == 0x1941);
    assert(spanish_title_presentation.selected_resource_name == "P00"
        && spanish_title_presentation.selected_resource_offset == 6
        && spanish_title_presentation.selected_resource_size == 10'555
        && spanish_title_presentation.selected_resource_sha256
            == "91c315133e58634d7327c7d3a3e95ecaa035580200f609f161db6b044261b43b");
    eon::MillenniumDosTitleSession spanish_title_session(spanish_title_boundary);
    assert(!spanish_title_session.handed_off());
    assert(!spanish_title_session.poll_console(false));
    assert(spanish_title_session.poll_console(true));
    assert(spanish_title_session.handed_off());
    assert(!spanish_title_session.poll_console(true));
    {
        auto altered_boundary = spanish_title_boundary;
        altered_boundary.input_service = 0x05;
        bool rejected = false;
        try {
            static_cast<void>(eon::MillenniumDosTitleSession(altered_boundary));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
    assert(rejected);
    }
    {
        auto altered_spanish_titles = disk.read(*spanish_titles);
        altered_spanish_titles[0x1c14 - 0x100] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_dos_spanish_title_presentation_evidence(
                altered_spanish_titles, disk.read(*spanish_title)));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
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
    // MillenniumDosLib borrows the supplied byte span, so retain the direct
    // FAT12 read for every resource view and decode below.
    const auto spanish_title_bytes = disk.read(*spanish_title);
    const eon::MillenniumDosLib spanish_title_lib(spanish_title_bytes);
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
    const auto spanish_static_evidence = eon::parse_millennium_dos_static_data_evidence(
        disk.read(*spanish_static_data));
    assert(spanish_static_evidence.source_size == 13'254);
    assert(spanish_static_evidence.celestial_table_offset == 0x3db);
    assert(spanish_static_evidence.pointer_count == 435);
    assert(spanish_static_evidence.raw_record_count == 434);
    assert(spanish_static_evidence.topology_anchors[4].table_index == 401);
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
    {
        auto altered_spanish_static_data = disk.read(*spanish_static_data);
        altered_spanish_static_data[0x03db] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_millennium_dos_static_data_evidence(
                altered_spanish_static_data));
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
    const eon::Fat12Disk atari_disk{std::span<const std::uint8_t>(*atari_image)};
    assert(atari_disk.bytes().data() == atari_image->data());
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
    const eon::AtariStStxPhysicalDisk atari_stx(*atari_physical_dump);
    assert(atari_stx.track_count() == 80);
    assert(atari_stx.sectors().size() == 800);
    const auto stx_boot = atari_stx.sector(0, 0, 1);
    assert(stx_boot.size() == 512);
    assert(eon::to_hex(eon::sha256(stx_boot))
        == "d0601ec6e1bbea0d5f4d5ba37130148e6670225b6337d001f4d4e6b8fc45fd08");
    const auto stx_loader = atari_stx.sector(1, 0, 9);
    assert(stx_loader.size() == 512);
    assert(eon::to_hex(eon::sha256(stx_loader))
        == "096869a11a3f601c587bb915c6c93d7985f8eb2185dc2d0f2839286df9905dad");
    assert(std::equal(stx_loader.begin() + 0xbe, stx_loader.begin() + 0xc9,
        "MILL22B.inf"));
    const eon::AtariStStxFat12Root stx_root(atari_stx);
    assert(stx_root.bytes_per_sector() == 512);
    assert(stx_root.sectors_per_cluster() == 2);
    assert(stx_root.total_sectors() == 800);
    assert(stx_root.root_start_lba() == 11 && stx_root.root_sector_count() == 7);
    assert(stx_root.fat_start_lba() == 1 && stx_root.sectors_per_fat() == 5);
    assert(!stx_root.fat_mirrors_match());
    assert(stx_root.fat_primary_sha256()
        == "2421cedef5612bca7bbc90168a7338d904f82ea1fdc09214c684424b428d9417");
    assert(stx_root.fat_secondary_sha256()
        == "22c2c826ed3de246e506187e16aea375dc2fee09a03abbc9140ebdd251640879");
    assert(stx_root.entries().size() == 6);
    assert(stx_root.entries()[0].name == "EXEC.TOS" && stx_root.entries()[0].first_cluster == 2
        && stx_root.entries()[0].size == 221);
    assert(stx_root.entries()[2].name == "MILL22B.INF" && stx_root.entries()[2].first_cluster == 11
        && stx_root.entries()[2].size == 84'720);
    // These mutations retain a structurally indexable STX container but must
    // fail the narrower sector-backed FAT12 boundary rather than being treated
    // as a usable namespace.
    auto invalid_stx_bpb = *atari_physical_dump;
    invalid_stx_bpb[0xcb] ^= 0x01; // T0/H0/S1 BPB bytes-per-sector low byte.
    bool invalid_stx_bpb_rejected = false;
    try {
        const eon::AtariStStxPhysicalDisk disk(std::move(invalid_stx_bpb));
        static_cast<void>(eon::AtariStStxFat12Root(disk));
    } catch (const std::runtime_error&) {
        invalid_stx_bpb_rejected = true;
    }
    assert(invalid_stx_bpb_rejected);
    auto invalid_stx_root_name = *atari_physical_dump;
    invalid_stx_root_name[0x1b70] = 0x01; // LBA 11 / T1/H0/S2, first root name byte.
    bool invalid_stx_root_name_rejected = false;
    try {
        const eon::AtariStStxPhysicalDisk disk(std::move(invalid_stx_root_name));
        static_cast<void>(eon::AtariStStxFat12Root(disk));
    } catch (const std::runtime_error&) {
        invalid_stx_root_name_rejected = true;
    }
    assert(invalid_stx_root_name_rejected);
    auto altered_stx_primary_fat = *atari_physical_dump;
    altered_stx_primary_fat[0x2d0] ^= 0x01; // One physical primary-FAT byte.
    const eon::AtariStStxPhysicalDisk altered_stx_disk(std::move(altered_stx_primary_fat));
    const eon::AtariStStxFat12Root altered_stx_root(altered_stx_disk);
    assert(!altered_stx_root.fat_mirrors_match());
    assert(altered_stx_root.fat_primary_sha256() != stx_root.fat_primary_sha256());
    assert(altered_stx_root.fat_secondary_sha256() == stx_root.fat_secondary_sha256());
    auto invalid_stx_header = *atari_physical_dump;
    invalid_stx_header[0] = 0;
    bool invalid_stx_header_rejected = false;
    try {
        static_cast<void>(eon::AtariStStxPhysicalDisk(std::move(invalid_stx_header)));
    } catch (const std::runtime_error&) {
        invalid_stx_header_rejected = true;
    }
    assert(invalid_stx_header_rejected);
    // The per-sector CHRN must describe the same physical side as the track
    // record.  Otherwise a malformed container could forge a second logical
    // identity for bytes belonging to this track.
    auto inconsistent_stx_side = *atari_physical_dump;
    inconsistent_stx_side[0x29] = 1; // Track 0's first sector descriptor: ID head.
    bool inconsistent_stx_side_rejected = false;
    try {
        static_cast<void>(eon::AtariStStxPhysicalDisk(std::move(inconsistent_stx_side)));
    } catch (const std::runtime_error&) {
        inconsistent_stx_side_rejected = true;
    }
    assert(inconsistent_stx_side_rejected);
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
    const auto atari_execution = eon::execute_millennium_atari_bootstrap_prefix(
        atari_disk.read(*atari_executable), atari_prg, atari_bootstrap, atari_bss_entry);
    assert(atari_execution.initial_pc_offset == 0);
    assert(atari_execution.branch_pc_offset == 0x24);
    assert(atari_execution.first_copy_longwords == 0x36);
    assert(atari_execution.bss_entry_address == 0x1d636);
    assert(atari_execution.second_copy_words == 0x101);
    assert(atari_execution.target_address == 0x77000);
    assert(atari_execution.stop_before_trap_address == 0x7700e);
    assert(atari_execution.target_prefix_bytes_executed == 14);
    assert(atari_execution.relative_stack_pointer_delta == -8);
    assert((atari_execution.fopen_frame_bytes == std::vector<std::uint8_t>{
        0x00, 0x3d, 0x00, 0x01, 0xd6, 0xe4, 0x00, 0x02}));
    assert(atari_execution.copied_stage_bytes.size() == 0xd8);
    assert(atari_execution.target.bytes == atari_target.bytes);
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
    const auto atari_fopen_result_gate = eon::execute_millennium_atari_fopen_result_gate(
        atari_target, atari_trap);
    assert(atari_fopen_result_gate.target_address == 0x77000);
    assert(atari_fopen_result_gate.entry_offset == 0x10);
    assert(atari_fopen_result_gate.byte_count == 10);
    assert(atari_fopen_result_gate.sha256
        == "d124b586e52a783689925186d8cc93366870526fd894567b7c55761a617807c7");
    assert(atari_fopen_result_gate.opaque_handle_push_opcode == 0x3f00);
    assert(atari_fopen_result_gate.fclose_selector_push_opcode == 0x3f3c);
    assert(atari_fopen_result_gate.fclose_selector == 0x3e);
    assert(atari_fopen_result_gate.result_test_opcode == 0x4a80);
    assert(atari_fopen_result_gate.negative_branch_opcode == 0x6bfe);
    assert(atari_fopen_result_gate.negative_branch_displacement == -2);
    assert(atari_fopen_result_gate.negative_successor_offset == 0x18);
    assert(atari_fopen_result_gate.nonnegative_successor_offset == 0x1a);
    assert(atari_fopen_result_gate.relative_stack_pointer_delta == -4);
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
    const auto atari_fread_frame_prefix = eon::execute_millennium_atari_fread_frame_prefix(
        atari_target, atari_fopen_fallthrough);
    assert(atari_fread_frame_prefix.target_address == 0x77000);
    assert(atari_fread_frame_prefix.entry_offset == 0x1a);
    assert(atari_fread_frame_prefix.byte_count == 18);
    assert(atari_fread_frame_prefix.sha256
        == "663d5f1418326aa9c0efde064ad95bda21c84d7f23241ce3505f21f1f07474d0");
    assert(atari_fread_frame_prefix.buffer_address == 0x2a500);
    assert(atari_fread_frame_prefix.byte_count_argument == 0x20000);
    assert(atari_fread_frame_prefix.function == 0x3f);
    assert(atari_fread_frame_prefix.opaque_handle_frame_offset == 2);
    assert(atari_fread_frame_prefix.opaque_handle_frame_bytes == 2);
    assert(atari_fread_frame_prefix.relative_stack_pointer_delta == -12);
    assert(atari_fread_frame_prefix.stop_before_trap_offset == 0x2c);
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
    {
        auto altered = *atari_image;
        altered.back() ^= 0x01;
        bool rejected = false;
        try {
            const eon::Fat12Disk altered_disk(std::move(altered));
            static_cast<void>(eon::MillenniumAtariBootstrapSession(
                altered_disk, altered_disk.read(*altered_disk.find("MILENIUM.TOS"))));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_program = atari_disk.read(*atari_executable);
        altered_program.back() ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::MillenniumAtariBootstrapSession(atari_disk, altered_program));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    assert(atari_session.target().target_address == 0x77000);
    assert(atari_session.execution().stop_before_trap_address == 0x7700e);
    assert(atari_session.execution().target_prefix_bytes_executed == 14);
    assert(atari_session.execution().relative_stack_pointer_delta == -8);
    assert(atari_session.bss_source().original_data_bytes == 0xbc);
    assert(atari_session.bss_source().bss_zero_bytes == 0x146);
    assert(atari_session.fopen_boundary().fopen_filename == "MILL22A.inf");
    assert(atari_session.fopen_boundary().fopen_access_mode == 2);
    assert(atari_session.fopen_boundary().fopen_function == 0x3d);
    assert(atari_session.fopen_result_gate().negative_successor_offset == 0x18);
    assert(atari_session.fopen_result_gate().nonnegative_successor_offset == 0x1a);
    assert(atari_session.fopen_fallthrough().fread_function == 0x3f);
    assert(atari_session.fopen_fallthrough().fread_buffer_address == 0x2a500);
    assert(atari_session.fread_frame_prefix().opaque_handle_frame_bytes == 2);
    assert(atari_session.fread_frame_prefix().stop_before_trap_offset == 0x2c);
    assert(atari_session.fread_config_transfer().config_buffer_address == 0x2a500);
    assert(atari_session.root_inventory().files.size() == 13);
    assert(atari_session.root_inventory().files[11].name == "DATA12.BIN");
    assert(atari_session.root_inventory().files[11].sha256
        == "6f1e8ab7720c530f8cf5bfc07497824ff731ce977a15d941dad5acd999c6eeda");
    assert(atari_session.config().present);
    assert(atari_session.config().size == 7506);
    assert(atari_session.config_entry().proven_load_base == 0x2a4de);
    assert(atari_session.config_entry().entry_address == 0x2aa88);
    assert(atari_session.fread_config_load_address_boundary().fread_destination_address == 0x2a500);
    assert(atari_session.fread_config_load_address_boundary().independent_entry_offset_delta == 34);
    assert(atari_session.fread_mapped_config_prelude().mapped_entry_file_offset == 0x588);
    assert(atari_session.fread_mapped_config_prelude().continuation_address == 0x2aaaa);
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
    const auto atari_fread_mapped_prelude = eon::parse_millennium_atari_fread_mapped_config_prelude(
        atari_fread_config_transfer, atari_config_payload, atari_config_entry);
    assert(atari_fread_mapped_prelude.fread_destination_address == 0x2a500);
    assert(atari_fread_mapped_prelude.mapped_entry_address == 0x2aa88);
    assert(atari_fread_mapped_prelude.mapped_entry_file_offset == 0x588);
    assert(atari_fread_mapped_prelude.continuation_address == 0x2aaaa);
    assert(atari_fread_mapped_prelude.byte_count == 34);
    assert(atari_fread_mapped_prelude.sha256
        == "dede20eddbd8015da1d1a4f2f5e53424c2bc2195bff238d830ea24c9f522ea59");
    assert(atari_fread_mapped_prelude.initial_opcode == 0x40c0);
    assert(atari_fread_mapped_prelude.conditional_branch_opcode == 0x6714);
    assert(atari_fread_mapped_prelude.conditional_branch_target_address == 0x2aaa4);
    assert(atari_fread_mapped_prelude.converged_jsr_address == 0x2aaa4);
    assert(atari_fread_mapped_prelude.converged_jsr_opcode == 0x4eb9);
    assert(atari_fread_mapped_prelude.converged_jsr_target_address == 0x2a51c);
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
    auto invalid_atari_fread_mapped_prelude_payload = atari_config_payload;
    invalid_atari_fread_mapped_prelude_payload[0x588] ^= 0x01;
    bool invalid_atari_fread_mapped_prelude_rejected = false;
    try {
        static_cast<void>(eon::parse_millennium_atari_fread_mapped_config_prelude(
            atari_fread_config_transfer, invalid_atari_fread_mapped_prelude_payload,
            atari_config_entry));
    } catch (const std::runtime_error&) {
        invalid_atari_fread_mapped_prelude_rejected = true;
    }
    assert(invalid_atari_fread_mapped_prelude_rejected);
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
    std::size_t millennium_named_config_files = 0;
    std::size_t millennium_exact_config_files = 0;
    std::size_t millennium_exact_program_files = 0;
    for (const auto& asset : eon::inventory_zip(atari_release->path)) {
        if (asset.kind != eon::AssetKind::atari_st_disk) continue;
        ++millennium_st_images;
        const auto candidate = eon::extract_asset_by_sha256(atari_release->path, asset.sha256);
        assert(candidate);
        try {
            const eon::Fat12Disk candidate_disk(*candidate);
            ++millennium_fat12_images;
            const auto candidate_config = eon::probe_millennium_atari_config(candidate_disk);
            if (candidate_config.present) {
                ++millennium_named_config_files;
                if (candidate_config.sha256
                    == "74d7d630779fd811aedcdbe31b14e54198eb9ffd673df512dd70b6165c4a37b6") {
                    ++millennium_exact_config_files;
                }
            }
            if (const auto* candidate_program = candidate_disk.find("MILENIUM.TOS")) {
                if (!candidate_program->directory()
                    && eon::to_hex(eon::sha256(candidate_disk.read(*candidate_program)))
                        == "4584ddc459e3bf03e642f3156fbedb74aa33a847db4937beb5635eb492e93686") {
                    ++millennium_exact_program_files;
                }
            }
        } catch (const std::runtime_error&) {
            // Protected/raw supplied ST media have no FAT12 file namespace.
        }
    }
    assert(millennium_st_images == 7);
    assert(millennium_fat12_images == 5);
    assert(millennium_named_config_files == 4);
    assert(millennium_exact_config_files == 4);
    assert(millennium_exact_program_files == 1);
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
    auto invalid_atari_fopen_result_gate = atari_target;
    invalid_atari_fopen_result_gate.bytes[0x18] ^= 0x01;
    bool invalid_atari_fopen_result_gate_rejected = false;
    try {
        static_cast<void>(eon::execute_millennium_atari_fopen_result_gate(
            invalid_atari_fopen_result_gate, atari_trap));
    } catch (const std::runtime_error&) {
        invalid_atari_fopen_result_gate_rejected = true;
    }
    assert(invalid_atari_fopen_result_gate_rejected);
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
    auto invalid_atari_fread_frame_prefix = atari_target;
    invalid_atari_fread_frame_prefix.bytes[0x2a] ^= 0x01;
    bool invalid_atari_fread_frame_prefix_rejected = false;
    try {
        static_cast<void>(eon::execute_millennium_atari_fread_frame_prefix(
            invalid_atari_fread_frame_prefix, atari_fopen_fallthrough));
    } catch (const std::runtime_error&) {
        invalid_atari_fread_frame_prefix_rejected = true;
    }
    assert(invalid_atari_fread_frame_prefix_rejected);
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
    std::size_t deuteros_atari_leaf_count = 0;
    std::size_t deuteros_atari_protected_geometry_count = 0;
    std::size_t deuteros_atari_valid_boot_profile_count = 0;
    std::size_t deuteros_atari_replicants_boot_count = 0;
    std::size_t deuteros_atari_killer_boot_count = 0;
    std::size_t deuteros_atari_nonstandard_leaf_count = 0;
    std::size_t deuteros_atari_invalid_branch_count = 0;
    std::size_t deuteros_atari_invalid_bpb_count = 0;
    std::size_t deuteros_atari_invalid_checksum_count = 0;
    for (const auto& asset : eon::inventory_verified_release(*deuteros_atari)) {
        if (asset.kind != eon::AssetKind::atari_st_disk) continue;
        ++deuteros_atari_leaf_count;
        const auto image = eon::extract_verified_release_asset(*deuteros_atari, asset.sha256);
        assert(image);
        const auto evidence = eon::inspect_deuteros_atari_media(*image);
        assert(evidence.image_size == image->size());
        if (!evidence.standard_protected_geometry) {
            ++deuteros_atari_nonstandard_leaf_count;
            continue;
        }
        ++deuteros_atari_protected_geometry_count;
        switch (evidence.boot_envelope_status) {
        case eon::DeuterosAtariMediaEvidence::BootEnvelopeStatus::invalid_branch:
            ++deuteros_atari_invalid_branch_count;
            break;
        case eon::DeuterosAtariMediaEvidence::BootEnvelopeStatus::invalid_bpb:
            ++deuteros_atari_invalid_bpb_count;
            break;
        case eon::DeuterosAtariMediaEvidence::BootEnvelopeStatus::invalid_checksum:
            ++deuteros_atari_invalid_checksum_count;
            break;
        case eon::DeuterosAtariMediaEvidence::BootEnvelopeStatus::nonstandard_geometry:
        case eon::DeuterosAtariMediaEvidence::BootEnvelopeStatus::valid:
            break;
        }
        if (!evidence.valid_boot_profile) continue;
        assert(evidence.boot_envelope_status
            == eon::DeuterosAtariMediaEvidence::BootEnvelopeStatus::valid);
        ++deuteros_atari_valid_boot_profile_count;
        assert(evidence.boot_checksum == 0x1234);
        if (evidence.recovered_replicants_first_stage) ++deuteros_atari_replicants_boot_count;
        if (evidence.killer_boot_signature) ++deuteros_atari_killer_boot_count;
    }
    assert(deuteros_atari_leaf_count == 11);
    assert(deuteros_atari_protected_geometry_count == 10);
    assert(deuteros_atari_valid_boot_profile_count == 9);
    assert(deuteros_atari_replicants_boot_count == 3);
    assert(deuteros_atari_killer_boot_count == 2);
    assert(deuteros_atari_nonstandard_leaf_count == 1);
    assert(deuteros_atari_invalid_branch_count == 1);
    assert(deuteros_atari_invalid_bpb_count == 0);
    assert(deuteros_atari_invalid_checksum_count == 0);
    const auto deuteros_st_disk1 = eon::extract_asset_by_sha256(deuteros_atari->path,
        "aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee");
    const auto deuteros_st_disk2 = eon::extract_asset_by_sha256(deuteros_atari->path,
        "5501ce3fd79c9b37cf695692a8012267db23dacd8a2cc64c0c7b7e4305971193");
    assert(deuteros_st_disk1 && deuteros_st_disk2);
    const eon::DeuterosAtariDisk deuteros_disk1(*deuteros_st_disk1);
    const eon::DeuterosAtariDisk deuteros_disk2(*deuteros_st_disk2);
    // The preflight-only disk view deliberately borrows the supplied bytes.
    // It must derive the same static boot facts without manufacturing a full
    // second disk buffer; sessions still use the owning constructor.
    const eon::DeuterosAtariDisk deuteros_disk1_view{
        std::span<const std::uint8_t>(*deuteros_st_disk1)};
    assert(deuteros_disk1_view.boot_profile().boot_checksum
        == deuteros_disk1.boot_profile().boot_checksum);
    assert(deuteros_disk1_view.boot_profile().first_stage_offset
        == deuteros_disk1.boot_profile().first_stage_offset);
    const eon::DeuterosAtariBootstrapSession deuteros_atari_session(*deuteros_st_disk1);
    {
        auto altered_deuteros_st_disk1 = *deuteros_st_disk1;
        // The tail is outside the two recovered boot-stage sector reads. A
        // session for this exact Replicants Disk 1 must reject it before it
        // exposes any static bootstrap evidence.
        altered_deuteros_st_disk1.back() ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::DeuterosAtariBootstrapSession(
                std::move(altered_deuteros_st_disk1)));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    assert(deuteros_atari_session.first_stage_sha256()
        == "dad3594c53375bd8285ef33e2d685bd38a5b38d930f2ea1305d117d63667f168");
    assert(deuteros_atari_session.second_stage_sha256()
        == "2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7");
    const auto atari_checkpoint = deuteros_atari_session.checkpoint();
    assert(atari_checkpoint.first_stage_sha256 == deuteros_atari_session.first_stage_sha256());
    assert(atari_checkpoint.second_stage_sha256 == deuteros_atari_session.second_stage_sha256());
    assert(atari_checkpoint.first_stage_entry_offset == 0x9c4);
    assert(atari_checkpoint.relocated_dispatcher_address == 0x1ec4);
    assert(atari_checkpoint.state1_xbios_selector == 0x26);
    assert(atari_checkpoint.state0_raw_request_count == 4);
    assert(atari_checkpoint.state0_duplicate_byte_count == 0x1200);
    assert(atari_checkpoint.state0_duplicate_direct_entry_offset == 0);
    assert(atari_checkpoint.state0_duplicate_dispatcher_offset == 0xc4);
    assert(atari_checkpoint.state0_duplicate_sha256
        == "2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7");
    assert(atari_checkpoint.state1_raw_request_count == 84);
    assert(atari_checkpoint.state1_skipped_ascii_branch_relative_offset == 0x48000);
    assert(atari_checkpoint.state1_skipped_ascii_relative_offset == 0x4800a);
    assert(atari_checkpoint.state1_skipped_ascii_byte_count == 0x438);
    assert(atari_checkpoint.state1_skipped_ascii_printable_run_count == 18);
    assert(atari_checkpoint.state1_skipped_ascii_sha256
        == "8dd46e7c760a38d07273b18a4cbd3c03eb44a6b57c8c401580dd47fa4646484e");
    assert(atari_checkpoint.state5_first_source_offset == 0x55800);
    assert(atari_checkpoint.state5_second_source_offset == 0x60c00);
    assert(atari_checkpoint.state5_state1_prefix_source_offset == 0x55800);
    assert(atari_checkpoint.state5_state1_prefix_byte_count == 0x57c00);
    assert(atari_checkpoint.state5_state1_prefix_sha256
        == "ed55ad2a893a87af9f11d269faa6358420c47ed6beb1fee7a177e9beaed1e77c");
    assert(atari_checkpoint.state1_display_branch_relative_offset == 0x48000);
    assert(atari_checkpoint.state1_display_service_relative_offset == 0x489c6);
    assert(atari_checkpoint.state1_display_xbios_selector == 5);
    assert(deuteros_atari_session.first_stage().next_destination == 0x70000);
    assert(deuteros_atari_session.second_stage().direct_entry == 0x1ec4);
    assert(deuteros_atari_session.first_stage_copy_execution().source_address == 0x70000);
    assert(deuteros_atari_session.first_stage_copy_execution().destination_address == 0x1e00);
    assert(deuteros_atari_session.first_stage_copy_execution().relocated_entry_address == 0x1ec4);
    assert(deuteros_atari_session.entry_execution().join_offset == 0x18);
    assert(deuteros_atari_session.entry_execution().dispatcher_entry == 0x1ec4);
    assert(deuteros_atari_session.entry_execution().stop_before_dispatcher_source_offset == 0xc4);
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
    const auto deuteros_copy_execution = eon::execute_deuteros_atari_first_stage_copy_prefix(
        deuteros_second_stage, deuteros_first_stage_profile, deuteros_second_stage_profile);
    assert(deuteros_copy_execution.source_address == 0x70000);
    assert(deuteros_copy_execution.destination_address == 0x1e00);
    assert(deuteros_copy_execution.byte_count == 0x1200);
    assert(deuteros_copy_execution.source_sha256
        == "2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7");
    assert(deuteros_copy_execution.relocated_sha256 == deuteros_copy_execution.source_sha256);
    assert(deuteros_copy_execution.direct_entry_source_offset == 0xc4);
    assert(deuteros_copy_execution.relocated_entry_address == 0x1ec4);
    {
        auto altered_second_stage = deuteros_second_stage;
        altered_second_stage[0xc4] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::execute_deuteros_atari_first_stage_copy_prefix(
                altered_second_stage, deuteros_first_stage_profile, deuteros_second_stage_profile));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_first_stage_profile = deuteros_first_stage_profile;
        altered_first_stage_profile.copy_byte_count = 0x1000;
        bool rejected = false;
        try {
            static_cast<void>(eon::execute_deuteros_atari_first_stage_copy_prefix(
                deuteros_second_stage, altered_first_stage_profile, deuteros_second_stage_profile));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_second_stage_profile = deuteros_second_stage_profile;
        altered_second_stage_profile.direct_entry = 0x1ec6;
        bool rejected = false;
        try {
            static_cast<void>(eon::execute_deuteros_atari_first_stage_copy_prefix(
                deuteros_second_stage, deuteros_first_stage_profile, altered_second_stage_profile));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    const auto deuteros_entry_execution = eon::execute_deuteros_atari_second_stage_entry_prefix(
        deuteros_second_stage, deuteros_second_stage_profile);
    assert(deuteros_entry_execution.join_offset == 0x18);
    assert(deuteros_entry_execution.executed_byte_count == 12);
    assert(deuteros_entry_execution.sha256
        == "b40da514f09891a46ce07d1def675f82f77b7752f8153beb7638bdf5aea973ee");
    assert(deuteros_entry_execution.stack_load_opcode == 0x4ff9);
    assert(deuteros_entry_execution.application_stack == 0x2478);
    assert(deuteros_entry_execution.jump_opcode == 0x4ef9);
    assert(deuteros_entry_execution.dispatcher_entry == 0x1ec4);
    assert(deuteros_entry_execution.stop_before_dispatcher_source_offset == 0xc4);
    {
        auto altered_second_stage = deuteros_second_stage;
        altered_second_stage[0x1e] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::execute_deuteros_atari_second_stage_entry_prefix(
                altered_second_stage, deuteros_second_stage_profile));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
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
    assert(deuteros_state1_skipped_ascii.presentation_marker_offset == 0);
    assert(deuteros_state1_skipped_ascii.presentation_marker_byte_count == 55);
    assert(deuteros_state1_skipped_ascii.presentation_marker_sha256
        == "785ebbc9d234032ee38c1cb5444ac1b5d46db21151ffad08d7b1898d6e6ce52a");
    assert((deuteros_state1_skipped_ascii.game_name_marker_offsets
        == std::array<std::size_t, 2>{{0x3c, 0x78}}));
    assert(deuteros_state1_skipped_ascii.game_name_marker_byte_count == 55);
    assert(deuteros_state1_skipped_ascii.game_name_marker_sha256
        == "f0eb99896cde59d36a075e624092cbf02de3ce0d201ca3c5050c13f9c65720dc");
    assert(deuteros_state1_skipped_ascii.ascii_sha256
        == "8dd46e7c760a38d07273b18a4cbd3c03eb44a6b57c8c401580dd47fa4646484e");
    const auto deuteros_state1_display = eon::parse_deuteros_atari_state1_display_service_boundary(
        deuteros_state1_bytes, deuteros_state1_plan);
    assert(deuteros_state1_display.branch_relative_offset == 0x48000);
    assert(deuteros_state1_display.branch_displacement == 0x09c2);
    assert(deuteros_state1_display.branch_target_relative_offset == 0x489c6);
    assert(deuteros_state1_display.branch_sha256
        == "6321ea5a7fcf59fb3f07d02b6bd333a62b9c897be5a67b233a83b3c935a38bf6");
    assert(deuteros_state1_display.service_setup_relative_offset == 0x489c6);
    assert(deuteros_state1_display.service_setup_byte_count == 18);
    assert(deuteros_state1_display.service_setup_sha256
        == "a07c7766104d5bf581862d24de4e594b60414625824e8360b1677cf92e88c6f3");
    assert(deuteros_state1_display.first_longword_push_opcode == 0x2f3c);
    assert(deuteros_state1_display.first_longword_argument == 0xffffffffU);
    assert(deuteros_state1_display.second_longword_push_opcode == 0x2f17);
    assert(deuteros_state1_display.selector_push_opcode == 0x3f3c);
    assert(deuteros_state1_display.xbios_selector == 5);
    assert(deuteros_state1_display.trap_opcode == 0x4e4e);
    assert(deuteros_state1_display.stack_cleanup_opcode == 0x4fef);
    assert(deuteros_state1_display.stack_cleanup_bytes == 12);
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
    {
        auto altered_state1 = deuteros_state1_bytes;
        altered_state1[0x489c6] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_deuteros_atari_state1_display_service_boundary(
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
    const auto deuteros_killer_execution = eon::execute_deuteros_atari_killer_boot_prefix(
        deuteros_disk2_boot, deuteros_disk2.boot_profile());
    assert(deuteros_killer_execution.relocation_destination == 0x8);
    assert(deuteros_killer_execution.relocated_bytes == std::vector<std::uint8_t>(
        deuteros_disk2_boot.begin() + 0xf0, deuteros_disk2_boot.begin() + 0x118));
    assert(deuteros_killer_execution.relocated_longwords[0] == 0x0000000c);
    assert(deuteros_killer_execution.relocated_longwords[2] == 0x4ed041fa);
    assert(deuteros_killer_execution.continuation_address == 0x12);
    assert(deuteros_killer_execution.first_clear_address == 0x32);
    assert((deuteros_killer_execution.cleared_longword_addresses
        == std::array<std::uint32_t, 8>{{0x32, 0x36, 0x3a, 0x3e, 0x42, 0x46, 0x4a, 0x4e}}));
    assert(deuteros_killer_execution.next_clear_address == 0x52);
    assert(deuteros_killer_execution.loop_target_address == 0x30);
    const auto deuteros_killer_decoder = eon::parse_deuteros_atari_killer_boot_decoder_boundary(
        deuteros_disk2_boot, deuteros_disk2.boot_profile());
    assert(deuteros_killer_decoder.caller_offset == 0x6c);
    assert(deuteros_killer_decoder.caller_byte_count == 8);
    assert(deuteros_killer_decoder.caller_sha256
        == "5e21bb3b7a3bc300d36f330a3112efbc5388515eb0441f23d9205bcc26df3d95");
    assert(deuteros_killer_decoder.decoder_offset == 0xc6);
    assert(deuteros_killer_decoder.decoder_byte_count == 18);
    assert(deuteros_killer_decoder.decoder_sha256
        == "218908b4c5751ffa0b5b19aaebd278df41e29a8f70cd6285a0e05ee9e07f5c04");
    assert(deuteros_killer_decoder.source_address == 0x1156);
    assert(deuteros_killer_decoder.source_offset == 0x156);
    assert(deuteros_killer_decoder.encoded_byte_count == 52);
    assert(deuteros_killer_decoder.encoded_sha256
        == "56ca6d45903d6cd36809ebbba04adcf398197a84e1e41e1bf0e1e3d53de9e7f2");
    assert(deuteros_killer_decoder.xor_immediate == 0xb9);
    assert(deuteros_killer_decoder.gemdos_selector == 9);
    assert(deuteros_killer_decoder.trap_opcode == 0x4e41);
    const auto deuteros_killer_decoded = eon::decode_deuteros_atari_killer_boot_message(
        deuteros_disk2_boot, deuteros_killer_decoder);
    assert(deuteros_killer_decoded.size() == 52);
    assert(deuteros_killer_decoded.back() == 0);
    assert(eon::to_hex(eon::sha256(deuteros_killer_decoded))
        == "9dfdd91bcc5c6b21d7d0751be79a527449045168d77f2f12240598384f898485");
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
        auto altered_boot = deuteros_disk2_boot;
        altered_boot[0x116] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::execute_deuteros_atari_killer_boot_prefix(
                altered_boot, deuteros_disk2.boot_profile()));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_boot = deuteros_disk2_boot;
        altered_boot[0x156] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::parse_deuteros_atari_killer_boot_decoder_boundary(
                altered_boot, deuteros_disk2.boot_profile()));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_boot = deuteros_disk2_boot;
        altered_boot[0x156] ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::decode_deuteros_atari_killer_boot_message(
                altered_boot, deuteros_killer_decoder));
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
    const eon::AmigaAdf system_disk{std::span<const std::uint8_t>(*amiga_disk1)};
    const eon::AmigaAdf data_disk{std::span<const std::uint8_t>(*amiga_disk2)};
    assert(system_disk.bytes(0, 1).data() == amiga_disk1->data());
    assert(data_disk.bytes(0, 1).data() == amiga_disk2->data());
    assert(system_disk.kind() == eon::AmigaDiskKind::dos);
    assert(data_disk.kind() == eon::AmigaDiskKind::deuteros_data);
    assert(system_disk.identifier() == std::string("DOS\0", 4));
    assert(data_disk.identifier() == std::string("DEU\0", 4));
    assert(system_disk.boot_checksum_valid());
    assert(data_disk.boot_checksum_valid());
    assert(system_disk.root_block() == 880);
    assert(data_disk.root_block() == 880);
    const auto data_header = eon::inspect_deuteros_amiga_data_disk_header(data_disk);
    assert(data_header.identifier == std::string("DEU\0", 4));
    assert(data_header.root_block == 880);
    assert(data_header.boot_checksum_valid);
    assert(data_header.sector_count == 1760);
    assert(data_header.header_prefix_length == 0xc8);
    assert(data_header.header_prefix_sha256
        == "3494ee5dc34793d7f09fdf2d8141be2ce5a0f07c78d6be5ccc12397bca7d9c06");
    assert(data_header.data_marker_count == 11);
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
    const auto title_handoff_route = eon::parse_deuteros_amiga_title_handoff_route(
        system_disk, load_plan);
    assert(title_handoff_route.resource_command_disk_offset == 0x1c28a);
    assert(title_handoff_route.resource_relative_offset == 0x0b38);
    assert(title_handoff_route.resource_runtime_address == 0x3355c);
    assert(title_handoff_route.bootstrap_profile_return_cell == 0x12ffc);
    assert(title_handoff_route.bootstrap_profile_value == 1);
    assert(title_handoff_route.command_sha256
        == "9f3880bf72d32f0fc119b941527dfe6004e18ad7e0fdfc40fe87eb6a13fe9c41");
    // Profile one is a raw title/game stage, not an archive to unpack.  Its
    // on-disk JMP vector enters the loaded interval at this exact address.
    assert(load_plan.title_stage.disk_offset == 0x6e000);
    assert(load_plan.title_stage.length == 0x6ca00);
    assert(load_plan.title_stage.destination == 0x13000);
    assert(load_plan.title_stage.entry_address == 0x40426);
    eon::DeuterosAmigaTitleStageSession title_stage_session(system_disk, load_plan, 1);
    assert(title_stage_session.stage().disk_offset == 0x6e000);
    assert(title_stage_session.stage().length == 0x6ca00);
    assert(title_stage_session.stage().destination == 0x13000);
    assert(title_stage_session.stage().entry_address == 0x40426);
    assert(title_stage_session.original_bytes().size() == 0x6ca00);
    assert(title_stage_session.original_sha256()
        == "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03");
    const auto title_palette_evidence = title_stage_session.transition_palette_evidence();
    assert((title_palette_evidence[0] == eon::RgbColor{0, 0, 0}));
    assert((title_palette_evidence[1] == eon::RgbColor{153, 170, 119}));
    const auto title_graphics_setup_palette = title_stage_session.graphics_setup_palette_evidence();
    assert((title_graphics_setup_palette[0] == eon::RgbColor{0, 0, 0}));
    assert((title_graphics_setup_palette[1] == eon::RgbColor{153, 170, 119}));
    assert((title_graphics_setup_palette[19] == eon::RgbColor{204, 204, 0}));
    assert(title_stage_session.graphics_setup().palette_source_address == 0x1ed24);
    assert(title_stage_session.graphics_setup().palette_destination_address == 0x12ecc);
    assert(title_stage_session.graphics_setup().palette_words[19] == 0x0cc0);
    assert(title_stage_session.display_clear().entry_address == 0x1f182);
    assert(title_stage_session.display_clear().destination_pointer_address == 0x1f168);
    assert(title_stage_session.display_clear().iteration_count == 0x1f40);
    assert(title_stage_session.entry_prefix().incoming_profile == 1);
    assert(title_stage_session.entry_prefix().stop_before_exec_address == 0x40450);
    assert(!title_stage_session.local_prefix_executed());
    const auto local_prefix_advance = title_stage_session.execute_local_prefix();
    assert(local_prefix_advance);
    assert(local_prefix_advance->writes == title_stage_session.entry_prefix_state().writes);
    assert(local_prefix_advance->stack_pointer_value == 0x40b62);
    assert(local_prefix_advance->exec_boundary_address == 0x40456);
    assert(title_stage_session.local_prefix_executed());
    assert(!title_stage_session.execute_local_prefix());
    {
        bool rejected = false;
        try {
            static_cast<void>(eon::DeuterosAmigaTitleStageSession(system_disk, load_plan, 2));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        // The live handoff must not admit an otherwise intact entry prefix
        // when the next original graphics-setup helper has changed.
        auto altered_graphics_setup_disk = *amiga_disk1;
        altered_graphics_setup_disk[0x6e000 + (0x1eda6 - 0x13000)] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_graphics_setup_disk));
            static_cast<void>(eon::DeuterosAmigaTitleStageSession(altered_disk, load_plan, 1));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
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
    assert(eon::to_hex(eon::sha256(system_disk.bytes(first_title_exit_copy.source_disk_offset,
        first_title_exit_copy.byte_count))) == first_title_exit_copy.source_sha256);
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
    {
        auto altered_title_stage_disk = *amiga_disk1;
        // The final stage byte lies outside the profile's decoded opcode
        // windows. Full-stage identity must reject it before those windows
        // can be treated as English title-stage evidence.
        altered_title_stage_disk[0x6e000 + 0x6ca00 - 1] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_title_stage_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_stage(altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
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
    const auto post_exec_pointer_route =
        eon::parse_deuteros_amiga_title_post_exec_pointer_route_profile(system_disk, load_plan);
    assert(post_exec_pointer_route.caller_address == 0x40504);
    assert(post_exec_pointer_route.caller_continuation_address == 0x4050a);
    assert(post_exec_pointer_route.entry_address == 0x2022a);
    assert(post_exec_pointer_route.entry_local_call_target == 0x20238);
    assert(post_exec_pointer_route.entry_clear_flag_address == 0x1ffd9);
    assert(post_exec_pointer_route.entry_return_address == 0x20236);
    assert(post_exec_pointer_route.selected_flag_address == 0x1ffd8);
    assert(post_exec_pointer_route.selected_pointer_cell_address == 0x2008e);
    assert(post_exec_pointer_route.selected_pointer_literal == 0x1ffe6);
    assert(post_exec_pointer_route.selected_flag_value == 1);
    assert(post_exec_pointer_route.selected_branch_target == 0x200dc);
    assert(post_exec_pointer_route.alternate_entry_address == 0x20258);
    assert(post_exec_pointer_route.alternate_flag_address == 0x1ffd8);
    assert(post_exec_pointer_route.alternate_pointer_cell_address == 0x2008e);
    assert(post_exec_pointer_route.alternate_pointer_literal == 0x2001e);
    assert(post_exec_pointer_route.alternate_branch_target == 0x200fa);
    assert(post_exec_pointer_route.alternate_return_address == 0x20274);
    assert(post_exec_pointer_route.caller_sha256
        == "ce9c44a0a83e370fdf54b5ec8ef0ffd72c170b007419176403293d2a54f91188");
    assert(post_exec_pointer_route.routine_sha256
        == "a7f7c0c3efa60284b3d292249b3560da4d832ff0c5dfa34711b72604760b39a9");
    const auto paired_local_route = eon::parse_deuteros_amiga_title_post_exec_paired_local_route_profile(system_disk, load_plan);
    assert((paired_local_route.caller_addresses == std::array<std::uint32_t, 2>{{0x4050e, 0x40518}}));
    assert((paired_local_route.d0_literals == std::array<std::uint16_t, 2>{{0x004d, 0x004e}}));
    assert(paired_local_route.entry_address == 0x41bb4);
    assert(paired_local_route.clear_bit_branch_target == 0x41c32);
    assert(paired_local_route.high_block_return_address == 0x41f30);
    assert(paired_local_route.sha256[3] == "765489ec36d727a326bfae44e34918cb85070d4ed3ef959cdcba9c41a102dd7e");
    const auto service_route = eon::parse_deuteros_amiga_title_post_exec_service_route_profile(system_disk, load_plan);
    assert(service_route.caller_address == 0x4052a && service_route.entry_address == 0x20e18);
    assert((service_route.external_call_targets == std::array<std::uint32_t, 3>{{0x1fb9a, 0x1ff08, 0x22bca}}));
    assert(service_route.nested_return_address == 0x20bf0 && service_route.continuation_target == 0x20bf2);
    const auto service_continuation =
        eon::parse_deuteros_amiga_title_post_exec_service_continuation_profile(
            system_disk, load_plan);
    assert(service_continuation.entry_address == service_route.continuation_target);
    assert(service_continuation.first_external_call_target == 0x1f9b8);
    assert((service_continuation.local_service_call_targets
        == std::array<std::uint32_t, 2>{{0x41bb4, 0x41bb4}}));
    assert(service_continuation.graphics_dispatch_target == 0x41ad2);
    assert((service_continuation.table_addresses
        == std::array<std::uint32_t, 2>{{0x20a3c, 0x20a6c}}));
    assert(service_continuation.return_address == 0x20cb8);
    assert(service_continuation.sha256
        == "98f43a011e13678af312563611740122ee9eb4fc163d1290a2c5e3dc66315385");
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
        auto altered_pointer_route_caller_disk = *amiga_disk1;
        altered_pointer_route_caller_disk[0x9b504] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_pointer_route_caller_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_post_exec_pointer_route_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_pointer_route_disk = *amiga_disk1;
        altered_pointer_route_disk[0x7b22a] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_pointer_route_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_post_exec_pointer_route_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    for (const auto disk_offset : std::array<std::size_t, 4>{{0x9b50a, 0x9cbb4, 0x9cc32, 0x9ceb0}}) {
        auto altered_paired_local_route_disk = *amiga_disk1;
        altered_paired_local_route_disk[disk_offset] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_paired_local_route_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_post_exec_paired_local_route_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    // The post-Exec service route binds its caller, the external-call prefix,
    // and its locally bounded subroute independently. Any change to one of
    // those original spans must reject the complete profile.
    for (const auto disk_offset : std::array<std::size_t, 3>{{0x9b51e, 0x7be18, 0x7bba8}}) {
        auto altered_service_route_disk = *amiga_disk1;
        altered_service_route_disk[disk_offset] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_service_route_disk));
            static_cast<void>(eon::parse_deuteros_amiga_title_post_exec_service_route_profile(
                altered_disk, load_plan));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_service_continuation_disk = *amiga_disk1;
        altered_service_continuation_disk[0x7bbf2] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_service_continuation_disk));
            static_cast<void>(
                eon::parse_deuteros_amiga_title_post_exec_service_continuation_profile(
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
    eon::DeuterosAmigaTitleResponseQueueInput response_queue_input;
    response_queue_input.pending_count = 2;
    for (std::size_t index = 0; index < response_queue_input.bytes.size(); ++index) {
        response_queue_input.bytes[index] = static_cast<std::uint8_t>(index + 0x20U);
    }
    const auto response_queue_once = eon::evaluate_deuteros_amiga_title_response_queue_once(
        system_disk, load_plan, response_queue_input);
    assert(response_queue_once.response_low_byte == 0x20);
    assert(response_queue_once.pending_count_after == 1);
    assert(response_queue_once.shifted_bytes[0] == 0x21);
    assert(response_queue_once.shifted_bytes[19] == 0x34);
    assert(response_queue_once.shifted_bytes[20] == 0x34);
    assert(response_queue_once.return_address == 0x1f258);
    bool response_queue_empty_rejected = false;
    try {
        static_cast<void>(eon::evaluate_deuteros_amiga_title_response_queue_once(
            system_disk, load_plan, {}));
    } catch (const std::runtime_error&) {
        response_queue_empty_rejected = true;
    }
    assert(response_queue_empty_rejected);
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
    // This models only the accepted byte-one callback arm with explicitly
    // supplied frame values. It does not bind an Amiga callback to host input.
    const auto title_callback_producer = eon::evaluate_deuteros_amiga_title_callback_producer(
        system_disk, load_plan, {3, 0, 2});
    assert(title_callback_producer.mirrored_event_address == 0x1ef2e);
    assert(title_callback_producer.mirrored_event_value == 1);
    assert(title_callback_producer.selector_word_address == 0x1ee0e);
    assert(title_callback_producer.selector_word_value == 0);
    assert(title_callback_producer.source_table_address == 0x1ee20);
    assert(title_callback_producer.source_table_index == 3);
    assert(title_callback_producer.queued_byte == 0x33);
    assert(title_callback_producer.destination_address == 0x1eec0);
    assert(title_callback_producer.destination_offset == 2);
    assert(title_callback_producer.pending_count_after == 3);
    const auto title_callback_second_half = eon::evaluate_deuteros_amiga_title_callback_producer(
        system_disk, load_plan, {3, 7, 0});
    assert(title_callback_second_half.source_table_index == 0x53);
    assert(title_callback_second_half.queued_byte == 0x33);
    bool callback_producer_rejected = false;
    try {
        static_cast<void>(eon::evaluate_deuteros_amiga_title_callback_producer(
            system_disk, load_plan, {0x50, 0, 0}));
    } catch (const std::runtime_error&) {
        callback_producer_rejected = true;
    }
    assert(callback_producer_rejected);
    // The independent byte-two arm accepts only explicit original-frame and
    // gate values. Its service route remains a reported boundary.
    const auto callback_second_event_service =
        eon::evaluate_deuteros_amiga_title_callback_second_event(
            system_disk, load_plan, {true, 0x00ff, 0, 0x1234, 0xabcd});
    assert(callback_second_event_service.mirrored_event_address == 0x1ef2e);
    assert(callback_second_event_service.mirrored_event_value == 2);
    assert((callback_second_event_service.copied_word_destinations
        == std::array<std::uint32_t, 2>{{0x1ee10, 0x1ee12}}));
    assert((callback_second_event_service.copied_word_values
        == std::array<std::uint16_t, 2>{{0x1234, 0xabcd}}));
    assert(callback_second_event_service.copied_words_written);
    assert(callback_second_event_service.stop
        == eon::DeuterosAmigaTitleCallbackSecondEventStop::external_service_boundary);
    assert(callback_second_event_service.next_address == 0x20118);
    const auto callback_second_event_transform =
        eon::evaluate_deuteros_amiga_title_callback_second_event(
            system_disk, load_plan, {true, 0x0068, 0x6000, 0, 0});
    assert(callback_second_event_transform.transformed_word_destination == 0x1ffd4);
    assert(callback_second_event_transform.transformed_word_value == 3);
    assert(callback_second_event_transform.transformed_word_written);
    assert(callback_second_event_transform.stop
        == eon::DeuterosAmigaTitleCallbackSecondEventStop::ordinary_return);
    const auto callback_second_event_gate =
        eon::evaluate_deuteros_amiga_title_callback_second_event(
            system_disk, load_plan, {false, 0x0068, 0x6000, 0, 0});
    assert(callback_second_event_gate.stop
        == eon::DeuterosAmigaTitleCallbackSecondEventStop::gate_return);
    assert(!callback_second_event_gate.transformed_word_written);
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
    assert((load_plan.resource_disk_offsets == std::array<std::uint32_t, 2>{
        0x1b800, 0x4ba00}));
    const auto main_resource_catalog = eon::inspect_deuteros_amiga_main_resource_catalog(
        system_disk, load_plan);
    assert(main_resource_catalog.entries.size() == 2);
    assert(main_resource_catalog.entries[0].source_disk_offset == 0x1b800);
    assert(main_resource_catalog.entries[0].source_length == 0x2f3f4);
    assert(main_resource_catalog.entries[0].source_sha256
        == "96c562cc08f32024a82f39c2c6c40b5407b7df4d2c674b1392fcfd594bdee1c0");
    assert(main_resource_catalog.entries[1].source_disk_offset == 0x4ba00);
    assert(main_resource_catalog.entries[1].source_length == 0x215f0);
    assert(main_resource_catalog.entries[1].source_sha256
        == "a8d3b2666a9dd0b66a06a521206443bb57dfce7f462fa134ada0fa03e669092e");
    assert(main_resource_catalog.total_source_bytes == 0x509e4);
    bool unproven_resource_rejected = false;
    try {
        static_cast<void>(eon::read_deuteros_amiga_main_resource(system_disk, load_plan, 2));
    } catch (const std::runtime_error&) {
        unproven_resource_rejected = true;
    }
    assert(unproven_resource_rejected);

    // The main loader's probe and body pass start at the same physical ADF
    // offset. These are bounded views of the genuine first two complete
    // resources, not extracted files or inferred data formats.
    const auto transferred_bundle0 = eon::read_deuteros_amiga_main_resource(
        system_disk, load_plan, 0);
    assert(transferred_bundle0);
    assert(transferred_bundle0->source_disk_offset == 0x1b800);
    assert(transferred_bundle0->probe_destination_address == 0x2ad24);
    assert(transferred_bundle0->payload_destination_address == 0x32a24);
    assert(transferred_bundle0->payload_length == 0x2f3f4);
    assert(transferred_bundle0->payload.data() == system_disk.bytes(0x1b800, 0x2f3f4).data());
    assert(transferred_bundle0->payload.size() == 0x2f3f4);
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
    assert(transferred_bundle1->payload.data() == system_disk.bytes(0x4ba00, 0x215f0).data());
    assert(transferred_bundle1->payload.size() == 0x215f0);
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
    assert(sound_bank.table_relative_offset == 0x121b4);
    assert(sound_bank.table_length == 0x12a);
    assert(sound_bank.table_sha256
        == "04491b3f24bc635cfc7be4cfdad4536dc83fa8c3056848092aecb662594b68a4");
    assert(sound_bank.trailing_bytes.relative_offset == 0x122da);
    assert(sound_bank.trailing_bytes.length == 4);
    assert(sound_bank.trailing_bytes.sha256
        == "3f82cccd0194a3cda5510304a0696c3a9436c38e798c73441c1d9d9d6868ce0d");
    assert(sound_bank.sounds[1].descriptor_relative_offset == 0x0e);
    assert(sound_bank.sounds[1].descriptor_sha256
        == "61b726d283ffc7966dcf70a203a6eab6ed9ba62ce1991c70d09f5ee813e42e20");
    assert(sound_bank.sounds[1].sample_relative_offset == 0x2a8b);
    assert(sound_bank.sounds[1].length_words == 0x40bc);
    assert(sound_bank.sounds[1].period == 0x1c0 && sound_bank.sounds[2].period == 0x1c2);
    assert(sound_bank.sounds[1].volume == 0x3f);
    assert(sound_bank.sounds[1].pcm.size() == sound_bank.sounds[2].pcm.size());
    assert(std::equal(sound_bank.sounds[1].pcm.begin(), sound_bank.sounds[1].pcm.end(),
        sound_bank.sounds[2].pcm.begin()));
    assert(sound_bank.sounds[1].pcm.data()
        == system_disk.bytes(first_bundle.disk_offset + sound_bank.sounds[1].sample_relative_offset,
            sound_bank.sounds[1].pcm.size()).data());
    assert(eon::to_hex(eon::sha256(sound_bank.sounds[1].pcm))
        == "f23fcd05f543be31726271b08ebfe7d907acfe31d1780aaf286fd2db701ae5d5");
    assert(sound_bank.sounds[1].pcm_sha256
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

    // At the source DMA boundary render must return only original held PCM
    // frames; it must not pad a host audio callback with invented silence.
    // A 1 Hz host rate makes the genuine entry's original AUDxPER consume its
    // full 0x40bc-word DMA span in a handful of frames.
    eon::DeuterosAmigaPaulaMixer boundary_paula(sound_bank, 1);
    assert(boundary_paula.submit({1, 1}));
    const auto boundary_audio = boundary_paula.render(100);
    assert(boundary_audio.size() == 10);
    assert(!boundary_paula.has_active_channels());
    assert(boundary_paula.render(100).empty());
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
    const auto first_bitmap_catalog = eon::inspect_deuteros_amiga_bitmap_catalog(
        system_disk, first_bundle, first_indexed_blob);
    assert(first_bitmap_catalog.record_count == 142);
    assert(first_bitmap_catalog.records.size() == 142);
    assert(first_bitmap_catalog.records[1].source_relative_offset == 0x12ef2);
    assert(first_bitmap_catalog.records[1].source_size
        == first_indexed_blob.record_offsets[2] - first_indexed_blob.record_offsets[1]);
    assert(first_bitmap_catalog.records[1].width == 48);
    assert(first_bitmap_catalog.records[1].height == 17);
    assert(first_bitmap_catalog.records[1].decoded_pixels_sha256
        == "fca175276cfe376b85e936f455aa9e89d1a0d4c89a61d2b6ce317fa6aa58a6a3");
    eon::DeuterosAmigaOpening live_opening(*amiga_disk1, *amiga_disk2);
    {
        auto altered_system_adf = *amiga_disk1;
        // The tail is outside the opening's first decoded bundles. The live
        // runtime must still reject a non-identical source before it creates
        // any VM, palette, or renderer state.
        altered_system_adf.back() ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::DeuterosAmigaOpening(
                std::move(altered_system_adf), *amiga_disk2));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    {
        auto altered_data_adf = *amiga_disk2;
        altered_data_adf.back() ^= 0x01;
        bool rejected = false;
        try {
            static_cast<void>(eon::DeuterosAmigaOpening(*amiga_disk1, std::move(altered_data_adf)));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    static_cast<void>(live_opening.tick());
    const auto live_tick2 = live_opening.tick();
    assert(live_tick2.palette == 1);
    assert(live_opening.input_gate());
    assert(live_opening.palette_index() == 1);
    assert(live_opening.active_channel_count() == 4);
    assert(live_opening.vblank_counter() == 8);
    static_cast<void>(live_opening.tick());
    const auto live_frame = live_opening.rgba_frame();
    assert(live_frame && live_frame->size() == 320U * 200U * 4U);
    assert(live_opening.frame_composed_on_last_tick());
    assert(live_opening.ticks() == 3);
    assert(live_opening.active_channel_count() == 4);
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
            first_input_alternate = events.alternate_resources.front().resource_relative_offset;
            assert(events.alternate_resources.front().command_stream_offset == 0x0a8a);
            assert(events.alternate_resources.front().command_disk_offset == 0x1c28a);
            assert(events.alternate_resources.front().channel_index == 3);
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
    eon::DeuterosAmigaOpening live_input_opening(*amiga_disk1, *amiga_disk2);
    assert(live_input_opening.title_handoff_route().resource_relative_offset == 0x0b38);
    assert(live_input_opening.title_handoff_route().bootstrap_profile_value == 1);
    std::optional<std::uint32_t> live_input_alternate;
    // Every checkpoint is an RGBA SHA-256 derived locally from the clean,
    // hash-recognised Amiga ADF. This covers the complete caller-proven held
    // input route, rather than treating only its first and final frame as
    // representative of the original opening.
    constexpr std::array<const char*, 82> held_opening_rgba_hashes{{
        "846f860302bb0486601324e073c35fb69c413b5c9237cf1152bf151a055d3f90",
        "846f860302bb0486601324e073c35fb69c413b5c9237cf1152bf151a055d3f90",
        "61806921c859c5e1031cb2471ce3f9b006bd78efc9038630fa58050015e31c8c",
        "8308eefee299217d7418c35e98ddaf1b67647d71af162bbfdf1df2c0c14b49f4",
        "6c7c310d2cc8c6350dddcf43e20253a9260aa5b16c7eda340e51afa17201e1ad",
        "c0bcedf2cc92e50beac7604502e25b5f36166e2714303b1d03d0772ab64a6dd6",
        "ca5d19ce13f3f67e45749ead71df337dcb2e0e4c9065b867fc86d6c8c5c870b2",
        "e30338d7f06391f316c397a0012ce3d2775e75d1ce75d2cac1ad221dd9f0699d",
        "3af11050d7f8eafe067b41a318768831a1a9fc98144ddaf31751e1f8790d17c8",
        "ad6549a11fc9a8c14cba78fe2d887e155ad8f3f3a113167cd85d2058ce3a7331",
        "2496f5bdec7a4cc934240505807896a2c671a34d420a0391cc2996b04c3e970b",
        "c5a2a028e3d34be3bd84182dc2cd2e229f3b8b55173f43799536d0ae45fd77b1",
        "cfd2cdf17088e66a5af2468123f575bd22d9b3699002b3260b2ee483b826a155",
        "b8ce150b523d0179f26908b8d1210805804f9b0265d4d42e1329f673b65e45b4",
        "0e33faafa6cd109c661c7c2ddb7b2fd6423e7c32fbb0efff907c1504dcda19ad",
        "4e7fe8eb4cb8294e95b4d60ee7e7b9d87996ecaa4cbba37a1158c425f82b26f8",
        "0be1960638e020c9c9630573af43cbee12169c586ed071ffb67853a3a459ec01",
        "3737b1f01324b1dcf9be0485b337d5fc6f9655a490b5d40b4d056e44fc34f8ed",
        "ae4fb04804a82e51ed6c18ef10c09bb6981e7c755a20d43fc860c1d8970c03ce",
        "4f6495ff8184a4a301e34fc29b6008587347573ea16d30e26c614cb04b55f3d1",
        "a4758a0abe81fbcc7690734ee8f3d85891a1d7ba55354e263144fc90c26d1e49",
        "fcffb0083580ad95b4c01e266fbcd558cc20d7bb48b1f8b3602678e4c8a21274",
        "c71e5892dcb932962528a7c1b1c5d2dc050bf90b32a8dc1db3315311abb388a9",
        "819271324b7938f09d3ec80a73d0969dc722c3b3205b49617b620efb0a2a56a4",
        "5d87ba1e995f10429cfd35d2f4a6998212c619f81fba0db72925aab5630150dc",
        "2842fbc0535eea99decd5107bf97943e9c26600c64cf6bdad5b70fb42822bfb0",
        "3392004a58448e09111fca532054819b8b1093cd6020794555f253ac048deca8",
        "97a1d8fd180c9796ee44f6583b3e20d59e2d4cd2d602b3c64eae28caaf1dd47d",
        "693a62152d530f0ab6167297b4619963f9955827b0236bbb6982908e5f543750",
        "261d1e01999999c9af97582bff4fb2e5668940bd1352a9d47a6c57e144b4f77f",
        "6975061bda451c59c7f7e0816ff26b7978041970199a2e8d0d8171b953b1c7bc",
        "96510965d05f7c39275ff329a593c1eab6ead6b836e4f363cdd9036c1d9151f3",
        "05385f3b5a236138f1c28c224fb1dab0819b8c830ce2bb6f515274d22773e77b",
        "0bad73eac16ba1ebff5e5786bf8cb886be7e7d5a312ed7eb04eb761b9ab0962c",
        "37e5befe11bbb05c0de039413025e94bd3f73d9e85417fe6a43b0c348a10bf34",
        "36212bf139161e29d9317302e5b25ce6860df2cd0ee29ebceb83cdccdb3aa50a",
        "7dacc43ec42f4c9bbb4bee92e1156c48911abd900bc18aa3db8803dee8e613d0",
        "bced9d9a2d33b9d9826a563f1046e932347e2faf308248668fb4b764c3cf92f2",
        "ba78a82eac8c891effb07c988b8b2cea486425ebc6683540a128e2c27f0e0156",
        "5afe297af096520ffbca2930ee31607aa6fd19042f3937648545b812bc8718cd",
        "242fcc03e05029ce1b43adab6985a44fdd82e71773bba3d6cb1930197138b1e4",
        "7836b7a5a9775c71dfe8a3093ce88e142c7feffcce7796cb764908c740f3bb96",
        "069071a485cc2e6532cef7ff11bead8ff2a99f24c3d032ca0dedf073df6f97bd",
        "97fdbe232c0414c7df77563202c01a500a7bc2d78bbbbc5e57be1abed47d7d5e",
        "97fdbe232c0414c7df77563202c01a500a7bc2d78bbbbc5e57be1abed47d7d5e",
        "a0dd7a807b7d54d99cddd7172eb5ae1464a183008368a178ab608873030282c6",
        "1e0de9b03d94bc86ab84e012adce822be9f7a6d3c4ff9f03f8ef99c56bf1fbec",
        "f1bb9f557047c1053ec73617ae77e9e19f31a0d3abdd7a76df5099c3fab54112",
        "793bd0cd1e07f837ae093550a6809fe1d0effd14533299253b4a088baf877c72",
        "03b9734cfe596897e5fd41dcb64afdd9aa7543bff05c1838a8a593e6c4f720ed",
        "48b47ce0956971c06f4de8971e9248dd4104cbb7a16ba999f8c82ddbab09676b",
        "03b9734cfe596897e5fd41dcb64afdd9aa7543bff05c1838a8a593e6c4f720ed",
        "48b47ce0956971c06f4de8971e9248dd4104cbb7a16ba999f8c82ddbab09676b",
        "e0a005ad3dcdf6d89380897bcadf68615974d7930a82b447a239416d331c2635",
        "48b47ce0956971c06f4de8971e9248dd4104cbb7a16ba999f8c82ddbab09676b",
        "e0a005ad3dcdf6d89380897bcadf68615974d7930a82b447a239416d331c2635",
        "d826df93f49160643f68bc75cd12f387064fc1953039d3c364e64c77ea6a2214",
        "e0a005ad3dcdf6d89380897bcadf68615974d7930a82b447a239416d331c2635",
        "d826df93f49160643f68bc75cd12f387064fc1953039d3c364e64c77ea6a2214",
        "d08282099cc61484d87124b3d5050e8ef9bf7e480c72da2c777eac19d0c4c012",
        "d826df93f49160643f68bc75cd12f387064fc1953039d3c364e64c77ea6a2214",
        "d08282099cc61484d87124b3d5050e8ef9bf7e480c72da2c777eac19d0c4c012",
        "261622fbeab8559c5223ea5f6b0415831a4e3fa3057fa35bfaf71c645a36a26d",
        "261622fbeab8559c5223ea5f6b0415831a4e3fa3057fa35bfaf71c645a36a26d",
        "3a9bb2db9e80f963ebb9f3b54cb2a47b0e866313733c1de91021d46e6ab74034",
        "3a9bb2db9e80f963ebb9f3b54cb2a47b0e866313733c1de91021d46e6ab74034",
        "98c49a44e4539b640f4b434b63f173f6fc805ff5a075bc5b2d274526cecff001",
        "8a2374773914dc65790531bda367debc5c71d351009665fe5048befe6a588a63",
        "107813a59abd55eacd5789bbccee74a8085ba74acf2b718117e507491d3b7ba8",
        "677ea098147c617942981ab17004e0398c93b94cce33bde9e5ac6450a0a7c2fa",
        "2169847d1515257dec22613ae5afdc5df16f2f14af7efdc120455984f0e5e846",
        "23437a56e7d6ac6802a25ff14be2a3aa8838564034cc10c7f8cc91bdc059127d",
        "577e4f2fcbc5a1712c45a12a210227bad8a50885192dad6607f2bf6c2abc7c1a",
        "5be2a1771202ac48d6226dc374502cfe8c2c42cd9fd97e125114130b086beaec",
        "ac2ad9cff5c3f959e64f6ef04474a15f6057a513adc3a99ed9c1f1718b0e26dc",
        "480a676f68e763655b0af746fe65ce7122321eb4ab79e68f41a9f2424566ea8c",
        "480a676f68e763655b0af746fe65ce7122321eb4ab79e68f41a9f2424566ea8c",
        "480a676f68e763655b0af746fe65ce7122321eb4ab79e68f41a9f2424566ea8c",
        "ad06495417836dc0a434263d1fad2ee758d3009b7cf49b4cf303c022385390f1",
        "ad06495417836dc0a434263d1fad2ee758d3009b7cf49b4cf303c022385390f1",
        "a2a69be7dc758c8aa0a57409bab3093a27cec39e3ccef3c3657d4d1e5596ae2a",
        "61eed88676355d0a136c943ffaa37396ba5220b7ea751b8cab6d0b125b3dd4c9",
    }};
    for (std::size_t tick = 1; tick <= held_opening_rgba_hashes.size(); ++tick) {
        const auto events = live_input_opening.tick(true);
        const auto rgba = live_input_opening.rgba_frame();
        assert(rgba);
        assert(eon::to_hex(eon::sha256(*rgba)) == held_opening_rgba_hashes[tick - 1]);
        if (!events.alternate_resources.empty()) {
            assert(events.alternate_resources.size() == 1);
            live_input_alternate = events.alternate_resources.front().resource_relative_offset;
            assert(events.alternate_resources.front().command_stream_offset == 0x0a8a);
            assert(events.alternate_resources.front().command_disk_offset == 0x1c28a);
            assert(events.alternate_resources.front().channel_index == 3);
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
    const auto& live_title_entry = live_title_stage->entry_prefix();
    assert(live_title_entry.incoming_profile == 1);
    assert(live_title_entry.controller_transfer_address == 0x206a0);
    assert(live_title_entry.mode_word_address == 0x4040e);
    assert(live_title_entry.mode_word_value == 1);
    assert(live_title_entry.normal_mode_byte_address == 0x19d52);
    assert(live_title_entry.normal_mode_byte_value == 1);
    assert(live_title_entry.stop_before_exec_address == 0x40450);
    // The live handoff now retains its complete, local pre-Exec RAM effect
    // as sparse original instruction writes. It does not allocate or expose
    // a guessed title-stage address space.
    const auto& live_title_entry_state = live_title_stage->entry_prefix_state();
    assert(live_title_entry_state.incoming_profile == 1);
    assert(live_title_entry_state.stop_before_exec_address == 0x40450);
    assert(live_title_entry_state.writes[0].address == 0x4040e);
    assert(live_title_entry_state.writes[0].width_bytes == 2);
    assert(live_title_entry_state.writes[0].value == 1);
    assert(live_title_entry_state.writes[1].address == 0x19d52);
    assert(live_title_entry_state.writes[1].width_bytes == 1);
    assert(live_title_entry_state.writes[1].value == 1);
    const auto materialized_title_entry =
        eon::materialize_deuteros_amiga_title_entry_prefix_state(system_disk, load_plan, 1);
    assert(materialized_title_entry.writes[0].address == live_title_entry_state.writes[0].address);
    assert(materialized_title_entry.writes[0].width_bytes == live_title_entry_state.writes[0].width_bytes);
    assert(materialized_title_entry.writes[0].value == live_title_entry_state.writes[0].value);
    assert(materialized_title_entry.writes[1].address == live_title_entry_state.writes[1].address);
    assert(materialized_title_entry.writes[1].width_bytes == live_title_entry_state.writes[1].width_bytes);
    assert(materialized_title_entry.writes[1].value == live_title_entry_state.writes[1].value);
    // Exactly one additional instruction is wholly local after the sparse
    // prefix: the literal A7 setup. The next instruction reads the unknown
    // Exec base, so recovered execution stops before that read.
    const auto& live_exec_prelude = live_title_stage->exec_prelude();
    assert(live_exec_prelude.incoming_profile == 1);
    assert(live_exec_prelude.entry_address == 0x40450);
    assert(live_exec_prelude.stack_pointer_value == 0x40b62);
    assert(live_exec_prelude.stop_before_exec_base_read_address == 0x40456);
    const auto materialized_exec_prelude =
        eon::execute_deuteros_amiga_title_exec_prelude(system_disk, load_plan, 1);
    assert(materialized_exec_prelude.entry_address == live_exec_prelude.entry_address);
    assert(materialized_exec_prelude.stack_pointer_value == live_exec_prelude.stack_pointer_value);
    assert(materialized_exec_prelude.stop_before_exec_base_read_address
        == live_exec_prelude.stop_before_exec_base_read_address);
    {
        auto altered_exec_prelude_disk = *amiga_disk1;
        altered_exec_prelude_disk[0x9b450] ^= 0x01;
        bool rejected = false;
        try {
            const eon::AmigaAdf altered_disk(std::move(altered_exec_prelude_disk));
            static_cast<void>(eon::execute_deuteros_amiga_title_exec_prelude(
                altered_disk, load_plan, 1));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
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
    assert(live_input_opening.title_stage_session()->local_prefix_executed());
    assert(live_input_opening.ticks() == 82);
    assert(live_input_opening.vblank_counter() == 82 * 4);
    const auto frame_at_title_handoff = live_input_opening.rgba_frame();
    assert(frame_at_title_handoff);
    // This is the caller-connected live result of the input channel's
    // verified $0f -> $fe -> $20580 route.  It protects the final original
    // 320x200 compositor surface at the title handoff, including its bounded
    // `please wait` glyph writes, from being silently replaced by a host UI.
    assert(eon::to_hex(eon::sha256(*frame_at_title_handoff))
        == "61eed88676355d0a136c943ffaa37396ba5220b7ea751b8cab6d0b125b3dd4c9");
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
    const auto second_bitmap_catalog = eon::inspect_deuteros_amiga_bitmap_catalog(
        system_disk, second_bundle, second_indexed_blob);
    assert(second_bitmap_catalog.record_count == 74);
    assert(second_bitmap_catalog.records.size() == 74);
    assert(second_bitmap_catalog.records.front().source_relative_offset == 0x15c92);
    assert(second_bitmap_catalog.records.front().source_size == 0xf2e);
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
