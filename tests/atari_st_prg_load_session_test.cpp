#include "data/atari_st_prg.hpp"
#include "data/fat12.hpp"
#include "data/sha256.hpp"
#include "engine/atari_st_prg_load_session.hpp"
#include "engine/millennium_atari_bootstrap_session.hpp"
#include "engine/millennium_atari_config_consumer_session.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

std::vector<std::uint8_t> structural_prg_fixture() {
    // Parser-only structural fixture: 8 TEXT bytes, 4 zeroed BSS bytes, one
    // relocation at image +2, and the required relocation terminator.
    std::vector<std::uint8_t> bytes(28 + 8 + 4 + 1, 0);
    bytes[0] = 0x60;
    bytes[1] = 0x1a;
    bytes[5] = 8;
    bytes[13] = 4;
    bytes[28] = 0x4e;
    bytes[29] = 0x71;
    bytes[32] = 0x01;
    bytes[33] = 0x20;
    bytes[28 + 8 + 3] = 2;
    return bytes;
}

template<typename Function>
void rejects(Function&& function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
}

} // namespace

int main(const int argc, const char* const argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--stdin-exact-disk") {
        const std::vector<std::uint8_t> disk_bytes(
            std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
        assert(eon::to_hex(eon::sha256(disk_bytes))
            == "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7");
        const eon::Fat12Disk disk(disk_bytes);
        const auto* entry = disk.find("MILENIUM.TOS");
        assert(entry && !entry->directory());
        const auto program = disk.read(*entry);
        const eon::MillenniumAtariBootstrapSession session(disk, program);
        const auto& exact = session.native_prg_image();
        assert(exact.entry_address == 0x10000 && exact.image.size() == 130392);
        assert(exact.relocation_effects.size() == 227);
        assert(exact.materialized_image_sha256
            == "92eac35edb2b5db721dd5353cfc3260dfb5fb4120026b76788659aaa342f887c");
        // The PRG image itself is bounded to 24-bit ST RAM. The runtime map
        // additionally admits the original sign-extended hardware address.
        eon::NativeRuntimeMemory memory;
        const auto image_result = memory.apply(eon::make_atari_st_prg_load_effect_batch(
            exact, "millennium-atari-1-prg"));
        assert(image_result.accepted);
        const auto& gemdos = session.read_only_gemdos();
        assert(gemdos.checkpoint().config_jsr_instruction_address == 0x7703c
            && gemdos.checkpoint().config_jsr_target_address == 0x2a500);
        const auto config_result = memory.apply(
            gemdos.make_fread_effect_batch("millennium-atari-1-config"));
        assert(config_result.accepted);
        const auto memory_checkpoint = memory.checkpoint();
        assert(memory_checkpoint.applied_batch_count == 2
            && memory_checkpoint.initialized_bytes.size() == exact.image.size());
        assert(memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
            std::nullopt, 0x2a500}) == 0x4e);
        assert(memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
            std::nullopt, 0x2a501}) == 0xf9);
        eon::MillenniumAtariConfigConsumerSession consumer(1, memory,
            gemdos.checkpoint(), session.fread_config_load_address_boundary(),
            session.fread_mapped_config_prelude());
        const auto& consumer_checkpoint = consumer.checkpoint();
        assert(consumer_checkpoint.state
                == eon::MillenniumAtariConfigConsumerState::status_register_boundary
            && consumer_checkpoint.jsr_instruction_address == 0x7703c
            && consumer_checkpoint.jsr_return_address == 0x77042
            && consumer_checkpoint.jsr_target_address == 0x2a500
            && consumer_checkpoint.entry_jump_target_address == 0x2aa88
            && consumer_checkpoint.boundary_instruction_address == 0x2aa88
            && consumer_checkpoint.boundary_opcode == 0x40c0
            && consumer_checkpoint.local_control_transfers_executed == 2
            && !consumer_checkpoint.return_address_materialized
            && !consumer_checkpoint.status_register_read
            && !consumer_checkpoint.hardware_write_executed);
        assert(!consumer.revoke(2).accepted && consumer.revoke(1).accepted);
        eon::MillenniumAtariConfigConsumerSession user_consumer(1, memory,
            gemdos.checkpoint(), session.fread_config_load_address_boundary(),
            session.fread_mapped_config_prelude());
        assert(!user_consumer.observe_status_register(
            {1, 1, 0x2aa88, 0x2000, eon::MillenniumAtariObservedPrivilege::user}).accepted);
        assert(user_consumer.observe_status_register(
            {1, 1, 0x2aa88, 0x0000, eon::MillenniumAtariObservedPrivilege::user}).accepted);
        const auto& user_path = user_consumer.checkpoint();
        assert(user_path.state == eon::MillenniumAtariConfigConsumerState::xbios_trap_boundary
            && user_path.branch_taken && user_path.status_register_read
            && !user_path.hardware_write_executed && user_path.hardware_writes.empty()
            && user_path.resulting_status_register == 0x0004
            && user_path.converged_jsr_target == 0x2a51c
            && user_path.xbios_trap_address == 0x2a520
            && user_path.xbios_selector == 2 && user_path.local_instruction_count == 5);
        assert(user_consumer.make_hardware_effect_batches("user").empty());
        assert(!user_consumer.observe_xbios_selector_two(
            {1, 1, 0x2a520, 2, 0x12345678}).accepted);
        assert(!user_consumer.observe_xbios_selector_two(
            {1, 2, 0x2a522, 2, 0x12345678}).accepted);
        assert(user_consumer.observe_xbios_selector_two(
            {1, 2, 0x2a520, 2, 0x12345678}).accepted);
        const auto& user_selector_three = user_consumer.checkpoint();
        assert(user_selector_three.state
                == eon::MillenniumAtariConfigConsumerState::xbios_selector_three_boundary
            && user_selector_three.selector_two_result_observed
            && user_selector_three.selector_two_result_d0 == 0x12345678
            && user_selector_three.selector_two_store_address == 0x2a50a
            && user_selector_three.selector_two_stack_cleanup_bytes == 2
            && user_selector_three.xbios_trap_address == 0x2a52e
            && user_selector_three.xbios_selector == 3
            && user_selector_three.local_instruction_count == 8);
        auto user_result_memory = memory;
        assert(user_result_memory.apply(user_consumer.make_selector_two_result_effect_batch(
            "user-selector-two-result")).accepted);
        assert(user_result_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
            std::nullopt, 0x2a50a}) == 0x12);
        assert(user_result_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
            std::nullopt, 0x2a50d}) == 0x78);
        assert(!user_consumer.observe_xbios_selector_three(
            {1, 2, 0x2a52e, 3, 0xa1b2c3d4}).accepted);
        assert(user_consumer.observe_xbios_selector_three(
            {1, 3, 0x2a52e, 3, 0xa1b2c3d4}).accepted);
        const auto& user_selector_four = user_consumer.checkpoint();
        assert(user_selector_four.state
                == eon::MillenniumAtariConfigConsumerState::xbios_selector_four_boundary
            && user_selector_four.selector_three_result_d0 == 0xa1b2c3d4
            && user_selector_four.selector_three_store_address == 0x2a50e
            && user_selector_four.xbios_trap_address == 0x2a53c
            && user_selector_four.xbios_selector == 4
            && user_selector_four.local_instruction_count == 11);
        assert(user_result_memory.apply(user_consumer.make_selector_three_result_effect_batch(
            "user-selector-three-result")).accepted);
        assert(user_result_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
            std::nullopt, 0x2a50e}) == 0xa1);
        assert(user_result_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
            std::nullopt, 0x2a511}) == 0xd4);
        assert(user_consumer.observe_xbios_selector_four(
            {1, 4, 0x2a53c, 4, 0x1234beef}).accepted);
        const auto& line_a = user_consumer.checkpoint();
        assert(line_a.state == eon::MillenniumAtariConfigConsumerState::line_a_init_boundary
            && line_a.selector_four_result_d0_word == 0xbeef
            && line_a.selector_four_store_address == 0x2a512
            && line_a.selector_four_stack_cleanup_bytes == 2
            && line_a.line_a_init_address == 0x2a546
            && line_a.line_a_init_opcode == 0xa000);
        assert(user_result_memory.apply(user_consumer.make_selector_four_result_effect_batch(
            "user-selector-four-result")).accepted);
        assert(user_result_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
            std::nullopt, 0x2a512}) == 0xbe);
        assert(user_result_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
            std::nullopt, 0x2a513}) == 0xef);
        assert(user_consumer.observe_line_a(
            {1, 5, 0x2a546, 0x00f00000, 0x11223344, 0x55667788}).accepted);
        const auto& selector_21 = user_consumer.checkpoint();
        assert(selector_21.state
                == eon::MillenniumAtariConfigConsumerState::xbios_selector_21_boundary
            && selector_21.xbios_trap_address == 0x2aab0
            && selector_21.xbios_selector == 0x15
            && selector_21.line_a_a3_store_address == 0x2a514
            && selector_21.line_a_a4_store_address == 0x2a518);
        assert(user_result_memory.apply(user_consumer.make_line_a_result_effect_batch(
            "user-line-a-result")).accepted);
        assert(user_result_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
            std::nullopt, 0x2a514}) == 0x11);
        assert(user_result_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
            std::nullopt, 0x2a51b}) == 0x88);
        assert(user_consumer.observe_xbios_selector_21(
            {1, 6, 0x2aab0, 0x15, 0xabcdef01}).accepted);
        const auto& selector_6 = user_consumer.checkpoint();
        assert(selector_6.state
                == eon::MillenniumAtariConfigConsumerState::xbios_selector_6_boundary
            && selector_6.selector_21_result_d0 == 0xabcdef01
            && selector_6.selector_21_stack_cleanup_bytes == 6
            && selector_6.selector_6_pointer_argument == 0x2a612
            && selector_6.xbios_trap_address == 0x2aabe
            && selector_6.xbios_selector == 6);
        assert(user_consumer.observe_xbios_selector_6(
            {1, 7, 0x2aabe, 6, 0x12345678}).accepted);
        const auto& jsr_boundary = user_consumer.checkpoint();
        assert(jsr_boundary.state
                == eon::MillenniumAtariConfigConsumerState::jsr_2b55a_boundary
            && jsr_boundary.selector_6_result_d0 == 0x12345678
            && jsr_boundary.selector_6_stack_cleanup_bytes == 6
            && jsr_boundary.next_jsr_address == 0x2aac2
            && jsr_boundary.next_jsr_target == 0x2b55a);
        assert(!user_consumer.observe_bchg_2b55a(
            {1, 8, 0x2b55a, 1, 0x2a500, 0x4e}).accepted);
        assert(user_consumer.execute_jsr_2b55a().accepted);
        assert(user_consumer.checkpoint().state
                == eon::MillenniumAtariConfigConsumerState::bsr_2b59a_boundary
            && user_consumer.checkpoint().bsr_instruction_address == 0x2b55e
            && user_consumer.checkpoint().bsr_target == 0x2b59a);
        assert(user_consumer.execute_bsr_2b59a().accepted);
        assert(user_consumer.checkpoint().state
                == eon::MillenniumAtariConfigConsumerState::d0_indexed_write_boundary
            && user_consumer.checkpoint().bsr_return_address == 0x2b562
            && user_consumer.checkpoint().callee_a3 == 0x2b0e8
            && user_consumer.checkpoint().callee_clear_address == 0x2b6b8
            && user_consumer.checkpoint().indexed_instruction_address == 0x2b5a6);
        auto bsr_memory = memory;
        assert(bsr_memory.apply(user_consumer.make_bsr_2b59a_effect_batch("bsr")).accepted);
        assert(bsr_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
            std::nullopt, 0x2b6b8}) == 0);
        const auto indexed_source = *memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
            std::nullopt, 0x2bdfd});
        assert(user_consumer.observe_d0_indexed_byte(
            {1, 9, 0x2b5a6, 0, 0x2bdfd, indexed_source}).accepted);
        assert(bsr_memory.apply(user_consumer.make_d0_indexed_effect_batch("indexed")).accepted);
        assert(bsr_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
            std::nullopt, 0x2b6b0}) == indexed_source);
        assert(bsr_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
            std::nullopt, 0x2b6b1}) == indexed_source);
        assert(user_consumer.execute_a1_setup().accepted);
        assert(user_consumer.checkpoint().state
                == eon::MillenniumAtariConfigConsumerState::d0_indexed_word_boundary
            && user_consumer.checkpoint().setup_a1 == 0x2b61e
            && user_consumer.checkpoint().indexed_word_instruction_address == 0x2b5de);
        assert(bsr_memory.apply(user_consumer.make_a1_setup_effect_batch("a1")).accepted);
        assert(bsr_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x2b639})==1);
        const auto word_hi=*memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x2bdfe});
        const auto word_lo=*memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x2bdff});
        const auto indexed_word=static_cast<std::uint16_t>((word_hi<<8U)|word_lo);
        assert(user_consumer.observe_d0_indexed_word({1,10,0x2b5de,0,0x2bdfe,indexed_word}).accepted);
        assert(bsr_memory.apply(user_consumer.make_d0_indexed_word_effect_batch("word")).accepted);
        assert(user_consumer.checkpoint().a0_indexed_instruction_address==0x2b5ec);
        assert(user_consumer.observe_a0_indexed_word({1,11,0x2b5ec,0,0x26ee4,0}).accepted);
        assert(user_consumer.checkpoint().state==eon::MillenniumAtariConfigConsumerState::loop_branch_boundary
            && user_consumer.checkpoint().loop_a0_value==0x56eee4
            && user_consumer.checkpoint().loop_d0_value==2
            && user_consumer.checkpoint().loop_d7_value==1
            && user_consumer.checkpoint().loop_branch_target==0x2b5b8);
        assert(bsr_memory.apply(user_consumer.make_a0_indexed_tail_effect_batch("tail")).accepted);
        assert(bsr_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x2b620})==0x00);
        assert(bsr_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x2b623})==0xe4);
        assert(user_consumer.execute_loop_iteration_setup().accepted);
        assert(user_consumer.checkpoint().state==eon::MillenniumAtariConfigConsumerState::d0_indexed_word_boundary
            && user_consumer.checkpoint().loop_iteration==1
            && user_consumer.checkpoint().loop_current_a1==0x2b64e);
        assert(bsr_memory.apply(user_consumer.make_loop_iteration_setup_effect_batch("loop1")).accepted);
        assert(bsr_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x2b669})==1);
        assert(user_consumer.observe_d0_indexed_word({1,12,0x2b5de,2,0x2be00,0x0d24}).accepted);
        assert(user_consumer.observe_a0_indexed_word({1,13,0x2b5ec,0,0x26ee4,0}).accepted);
        assert(user_consumer.checkpoint().loop_d7_value==0);
        assert(user_consumer.execute_loop_iteration_setup().accepted);
        assert(user_consumer.checkpoint().loop_iteration==2 && user_consumer.checkpoint().loop_current_a1==0x2b67e);
        assert(user_consumer.observe_d0_indexed_word({1,14,0x2b5de,2,0x2be00,0x0d24}).accepted);
        assert(user_consumer.observe_a0_indexed_word({1,15,0x2b5ec,0,0x26ee4,0}).accepted);
        assert(user_consumer.checkpoint().loop_d7_value==0xffff && user_consumer.checkpoint().loop_branch_target==0x2b600);
        assert(user_consumer.execute_loop_epilogue().accepted);
        assert(user_consumer.checkpoint().state==eon::MillenniumAtariConfigConsumerState::movem_restore_boundary
            && user_consumer.checkpoint().movem_instruction_address==0x2b562);
        assert(bsr_memory.apply(user_consumer.make_loop_epilogue_effect_batch("epilogue")).accepted);
        std::array<std::uint32_t,15> restored{};
        for(std::size_t i=0;i<restored.size();++i)restored[i]=static_cast<std::uint32_t>(i+1U);
        assert(user_consumer.observe_movem_frame({1,16,0x2b562,0x80000,restored,0x2aac8}).accepted);
        assert(user_consumer.checkpoint().state==eon::MillenniumAtariConfigConsumerState::jsr_2aa68_boundary
            && user_consumer.checkpoint().restored_stack_address==0x80040
            && user_consumer.checkpoint().next_jsr_address==0x2aac8
            && user_consumer.checkpoint().next_jsr_target==0x2aa68);
        assert(user_consumer.execute_jsr_2aa68().accepted);
        assert(user_consumer.checkpoint().state==eon::MillenniumAtariConfigConsumerState::xbios_selector_38_boundary
            && user_consumer.checkpoint().xbios_trap_address==0x2aa72
            && user_consumer.checkpoint().xbios_selector==0x26
            && user_consumer.checkpoint().selector_38_pointer_argument==0x2aa42);
        assert(user_consumer.observe_xbios_selector_38({1,17,0x2aa72,0x26,0x12345678}).accepted);
        assert(user_consumer.checkpoint().state==eon::MillenniumAtariConfigConsumerState::jsr_2aa0c_boundary
            && user_consumer.checkpoint().caller_d7==0x2a640
            && user_consumer.checkpoint().next_jsr_address==0x2aad4
            && user_consumer.checkpoint().next_jsr_target==0x2aa0c);
        assert(user_consumer.execute_jsr_2aa0c().accepted);
        assert(user_consumer.checkpoint().state==eon::MillenniumAtariConfigConsumerState::gemdos_selector_61_boundary
            && user_consumer.checkpoint().next_jsr_address==0x2aa0c
            && user_consumer.checkpoint().next_jsr_target==0x2a5aa
            && user_consumer.checkpoint().gemdos_trap_address==0x2a5b4
            && user_consumer.checkpoint().gemdos_selector==0x3d
            && user_consumer.checkpoint().gemdos_open_mode==2
            && user_consumer.checkpoint().gemdos_filename_pointer==0x2a640);
        assert(!user_consumer.execute_jsr_2aa0c().accepted);
        auto negative_fopen=user_consumer;
        assert(user_consumer.observe_gemdos_selector_61({1,18,0x2a5b4,0x3d,7}).accepted);
        assert(user_consumer.checkpoint().state==eon::MillenniumAtariConfigConsumerState::jsr_2a5c2_boundary
            && user_consumer.checkpoint().gemdos_handle_store_address==0x2a5fa
            && user_consumer.checkpoint().fopen_branch_target==0x2aa1c
            && user_consumer.checkpoint().next_jsr_address==0x2aa28
            && user_consumer.checkpoint().next_jsr_target==0x2a5c2);
        assert(bsr_memory.apply(user_consumer.make_gemdos_selector_61_effect_batch("fopen-positive")).accepted);
        assert(bsr_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x2a5fa})==0
            && bsr_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x2a5fb})==7);
        assert(user_consumer.execute_jsr_2a5c2().accepted);
        assert(user_consumer.checkpoint().state==eon::MillenniumAtariConfigConsumerState::gemdos_selector_63_boundary
            && user_consumer.checkpoint().gemdos_63_trap_address==0x2a5d0
            && user_consumer.checkpoint().gemdos_63_selector==0x3f
            && user_consumer.checkpoint().gemdos_63_handle==7
            && user_consumer.checkpoint().gemdos_63_buffer==0x7d42
            && user_consumer.checkpoint().gemdos_63_count==0x2c24a);
        assert(!user_consumer.execute_jsr_2a5c2().accepted);
        assert(!user_consumer.observe_gemdos_selector_63({1,19,0x2a5d2,0x3f,0}).accepted);
        assert(user_consumer.observe_gemdos_selector_63({1,19,0x2a5d0,0x3f,0x2c24a}).accepted);
        assert(user_consumer.checkpoint().state==eon::MillenniumAtariConfigConsumerState::gemdos_selector_62_boundary
            && user_consumer.checkpoint().gemdos_63_result_d0==0x2c24a
            && user_consumer.checkpoint().gemdos_63_stack_cleanup_bytes==12
            && user_consumer.checkpoint().gemdos_62_trap_address==0x2a5e6
            && user_consumer.checkpoint().gemdos_62_selector==0x3e
            && user_consumer.checkpoint().gemdos_62_handle==7);
        assert(!user_consumer.observe_gemdos_selector_63({1,20,0x2a5d0,0x3f,0}).accepted);
        assert(!user_consumer.observe_gemdos_selector_62({1,20,0x2a5e8,0x3e,0}).accepted);
        assert(user_consumer.observe_gemdos_selector_62({1,20,0x2a5e6,0x3e,0}).accepted);
        assert(user_consumer.checkpoint().state==eon::MillenniumAtariConfigConsumerState::fread_prefix_boundary
            && user_consumer.checkpoint().fread_prefix_a4==0x2c24c);
        auto single_cell_planes=user_consumer;
        assert(!user_consumer.observe_fread_prefix({1,21,0x2c24a,0x1122,0x2c24e,0x3344}).accepted);
        assert(user_consumer.observe_fread_prefix({1,21,0x2c24c,0x1122,0x2c24e,0x3344}).accepted);
        assert(user_consumer.checkpoint().state==eon::MillenniumAtariConfigConsumerState::jsr_2b2be_boundary
            && user_consumer.checkpoint().fread_prefix_d6==0x1122
            && user_consumer.checkpoint().fread_prefix_d7==0x3344
            && user_consumer.checkpoint().caller_a5==user_consumer.checkpoint().selector_three_result_d0
            && user_consumer.checkpoint().next_jsr_address==0x2aaec
            && user_consumer.checkpoint().next_jsr_target==0x2b2be);
        assert(bsr_memory.apply(user_consumer.make_fread_prefix_effect_batch("fread-prefix")).accepted);
        assert(bsr_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x2c24c})==0x11
            && bsr_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x2c24f})==0x44);
        assert(user_consumer.execute_jsr_2b2be().accepted);
        assert(user_consumer.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_source_byte_boundary
            && user_consumer.checkpoint().game_init_a3==0x2b2ba
            && user_consumer.checkpoint().game_init_d6==0x0449
            && user_consumer.checkpoint().game_init_d7==0x0044
            && user_consumer.checkpoint().game_init_source_address==0x2c250);
        assert(bsr_memory.apply(user_consumer.make_game_init_setup_effect_batch("game-init-setup")).accepted);
        assert(bsr_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x2b2ba})==0x04
            && bsr_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x2b2bd})==0x44);
        auto bit6_clear=user_consumer;
        auto bit7_set=user_consumer;
        auto bit6_only=user_consumer;
        assert(!user_consumer.observe_game_init_source_byte({1,22,0x2b2e0,0x2c250,0x12}).accepted);
        assert(user_consumer.observe_game_init_source_byte({1,22,0x2b2de,0x2c250,0x12}).accepted);
        assert(user_consumer.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_zero_copy_boundary
            && user_consumer.checkpoint().game_init_next_instruction==0x2b2ea
            && user_consumer.checkpoint().game_init_source_address==0x2c251);
        assert(bsr_memory.apply(user_consumer.make_game_init_source_byte_effect_batch("game-init-byte")).accepted);
        assert(bsr_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x2c250})==0x12);
        const auto zero_destination=user_consumer.checkpoint().caller_a5;
        assert(!user_consumer.observe_game_init_zero_pair({1,23,0x2b2ec,0x2c251,0x56,0x2c252,0x78}).accepted);
        assert(user_consumer.observe_game_init_zero_pair({1,23,0x2b2ea,0x2c251,0x56,0x2c252,0x78}).accepted);
        assert(user_consumer.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_zero_counter_branch_boundary
            && user_consumer.checkpoint().game_init_next_instruction==0x2b2f2
            && user_consumer.checkpoint().game_init_source_address==0x2c253
            && user_consumer.checkpoint().caller_a5==zero_destination+8
            && user_consumer.checkpoint().game_init_d6==0x0448
            && user_consumer.checkpoint().game_init_zero_pair_prefix_sha256=="8b97786735b1f1be41f931a62098f2f1080b5067b2db2a9835125619ad3b7623");
        assert(bsr_memory.apply(user_consumer.make_game_init_zero_pair_effect_batch("game-init-zero-pair")).accepted);
        assert(bsr_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x2c251})==0x56
            && bsr_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,zero_destination})==0x56
            && bsr_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,zero_destination+1})==0x78);
        assert(user_consumer.execute_game_init_zero_counter_branch().accepted);
        assert(user_consumer.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_zero_copy_boundary
            && user_consumer.checkpoint().game_init_next_instruction==0x2b2ea
            && user_consumer.checkpoint().game_init_d2==0x11
            && user_consumer.checkpoint().game_init_source_address==0x2c253
            && user_consumer.checkpoint().caller_a5==zero_destination+8
            && user_consumer.checkpoint().game_init_zero_counter_continuation_sha256=="9b3476f5d2ecb028149eec6ee575cd79c7c9f94589a7e7398d794ecd176f04ef");
        assert(!user_consumer.execute_game_init_zero_counter_branch().accepted);
        assert(single_cell_planes.observe_fread_prefix({1,21,0x2c24c,0x0001,0x2c24e,0x0001}).accepted);
        assert(single_cell_planes.execute_jsr_2b2be().accepted);
        std::uint64_t plane_sequence=22;
        std::uint32_t plane_source=0x2c250;
        for(std::uint32_t plane=1;plane<=4;++plane){
            assert(single_cell_planes.observe_game_init_source_byte({1,plane_sequence++,0x2b2de,plane_source,0x01}).accepted);
            assert(single_cell_planes.observe_game_init_zero_pair({1,plane_sequence++,0x2b2ea,plane_source+1U,0x12,plane_source+2U,0x34}).accepted);
            assert(single_cell_planes.execute_game_init_zero_counter_branch().accepted);
            plane_source+=3U;
            if(plane<4)assert(single_cell_planes.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_source_byte_boundary
                && single_cell_planes.checkpoint().game_init_completed_planes==plane
                && single_cell_planes.checkpoint().game_init_d6==1
                && single_cell_planes.checkpoint().game_init_d7==1);
        }
        assert(single_cell_planes.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_complete
            && single_cell_planes.checkpoint().game_init_next_instruction==0x2b3c6
            && single_cell_planes.checkpoint().game_init_completed_planes==4);
        const auto palette_clear_destination=single_cell_planes.checkpoint().caller_a5;
        assert(single_cell_planes.execute_game_init_return().accepted
            && single_cell_planes.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_jsr_2b448_boundary
            && single_cell_planes.checkpoint().game_init_a3==0x2a64c
            && single_cell_planes.checkpoint().game_init_a0==0x2a66c
            && single_cell_planes.checkpoint().next_jsr_address==0x2aafe
            && single_cell_planes.checkpoint().next_jsr_target==0x2b448
            && single_cell_planes.checkpoint().game_init_caller_2b448_sha256=="155575e295ad1e7831c0eef9809316db6f68321beb0661c03b7c14bb141f793e");
        assert(single_cell_planes.execute_game_init_palette_copy_prefix().accepted
            && single_cell_planes.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_palette_transform_boundary
            && single_cell_planes.checkpoint().game_init_palette_clear_destination==palette_clear_destination
            && single_cell_planes.checkpoint().game_init_palette_copy_destination==0x2b3c8
            && single_cell_planes.checkpoint().game_init_palette_source_sha256=="a2263d35c251e787a9a5705a5277bcf641321817f825e7689081280fbd157dfe"
            && single_cell_planes.checkpoint().game_init_palette_copy_prefix_sha256=="748d9b2df05839b68583069e29ff34954477ce7a367b0a88ef9e9bad7abfa0ca"
            && single_cell_planes.checkpoint().game_init_next_instruction==0x2b486);
        auto palette_memory=bsr_memory;
        assert(palette_memory.apply(single_cell_planes.make_game_init_palette_copy_effect_batch("palette-copy-prefix")).accepted
            && palette_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,palette_clear_destination})==0
            && palette_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x2b3c8})==static_cast<std::uint8_t>(single_cell_planes.checkpoint().game_init_palette_source_longs[0]>>24U));
        eon::MillenniumAtariGameInitPaletteWordsObservation palette_words{1,99,0x2b486,0x2b3c8,0x2b428,{}};
        for(std::size_t i=0;i<palette_words.destination_words.size();++i){const auto hi=palette_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x2b428U+static_cast<std::uint32_t>(i*2U)});const auto lo=palette_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x2b429U+static_cast<std::uint32_t>(i*2U)});assert(hi&&lo);palette_words.destination_words[i]=static_cast<std::uint16_t>((*hi<<8U)|*lo);}
        const auto first_source_long=single_cell_planes.checkpoint().game_init_palette_source_longs[0];
        const auto first_sum=static_cast<std::uint16_t>(first_source_long>>24U)
            +static_cast<std::uint8_t>(first_source_long>>16U);
        const auto expected_first_word=static_cast<std::uint16_t>(palette_words.destination_words[0]
            +(first_sum>0xffU?0x0100U:0U));
        auto bad_palette_words=palette_words;bad_palette_words.instruction_address=0x2b488;
        assert(!single_cell_planes.observe_game_init_palette_words(bad_palette_words).accepted);
        assert(single_cell_planes.observe_game_init_palette_words(palette_words).accepted
            && single_cell_planes.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_palette_xbios_selector_6_boundary
            && single_cell_planes.checkpoint().game_init_palette_arithmetic_sha256=="0866601f1a271ee74b399dd544b5b1ced15693e600c30034531a094dbc41d746"
            && single_cell_planes.checkpoint().game_init_palette_xbios_trap_address==0x2b4ac
            && single_cell_planes.checkpoint().game_init_palette_xbios_selector==6
            && single_cell_planes.checkpoint().game_init_palette_xbios_pointer==0x2b428
            && single_cell_planes.checkpoint().game_init_palette_result_bytes[0]==static_cast<std::uint8_t>(first_sum)
            && single_cell_planes.checkpoint().game_init_palette_result_words[0]==expected_first_word);
        assert(palette_memory.apply(single_cell_planes.make_game_init_palette_arithmetic_effect_batch("palette-arithmetic")).accepted
            && palette_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x2b3c8})==single_cell_planes.checkpoint().game_init_palette_result_bytes[0]);
        assert(!single_cell_planes.observe_game_init_palette_xbios_selector_6({1,100,0x2b4ae,6,0x12345678}).accepted);
        assert(single_cell_planes.observe_game_init_palette_xbios_selector_6({1,100,0x2b4ac,6,0x12345678}).accepted
            && single_cell_planes.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_palette_outer_recurrence_boundary
            && single_cell_planes.checkpoint().game_init_palette_xbios_result_observed
            && single_cell_planes.checkpoint().game_init_palette_xbios_result_d0==0x12345678
            && single_cell_planes.checkpoint().game_init_palette_xbios_stack_cleanup_bytes==6
            && single_cell_planes.checkpoint().game_init_palette_post_xbios_sha256=="9e3fd4aeca606c5560b204d12a20a77de12552ded7fa64a0677cca56c4676bf1"
            && single_cell_planes.checkpoint().game_init_palette_delay_initial_d0==0x4e20
            && single_cell_planes.checkpoint().game_init_palette_delay_iterations==0x4e20
            && single_cell_planes.checkpoint().game_init_palette_delay_final_d0==0
            && single_cell_planes.checkpoint().game_init_d7==5
            && single_cell_planes.checkpoint().game_init_palette_outer_backedge_address==0x2b44c
            && single_cell_planes.checkpoint().game_init_next_instruction==0x2b44c);
        assert(!single_cell_planes.observe_game_init_palette_xbios_selector_6({1,101,0x2b4ac,6,0}).accepted);
        assert(!single_cell_planes.execute_game_init_return().accepted
            && !single_cell_planes.execute_game_init_palette_copy_prefix().accepted);
        assert(bit6_clear.observe_game_init_source_byte({1,22,0x2b2de,0x2c250,0x80}).accepted
            && bit6_clear.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_bit6_clear_boundary
            && bit6_clear.checkpoint().game_init_next_instruction==0x2b3b8);
        assert(bit7_set.observe_game_init_source_byte({1,22,0x2b2de,0x2c250,0xc2}).accepted
            && bit7_set.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_bit7_set_boundary
            && bit7_set.checkpoint().game_init_next_instruction==0x2b376);
        assert(bit6_only.observe_game_init_source_byte({1,22,0x2b2de,0x2c250,0x42}).accepted
            && bit6_only.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_second_source_boundary
            && bit6_only.checkpoint().game_init_next_instruction==0x2b338);
        const auto alternate_destination=bit6_only.checkpoint().caller_a5;
        const eon::MillenniumAtariGameInitAlternateWrite expected_replicated_first{alternate_destination,0xabab};
        const eon::MillenniumAtariGameInitAlternateWrite expected_replicated_second{alternate_destination+8U,0xabab};
        assert(!bit6_only.observe_game_init_replicated_byte({1,23,0x2b33a,0x2c251,0xab}).accepted);
        assert(bit6_only.observe_game_init_replicated_byte({1,23,0x2b338,0x2c251,0xab}).accepted
            && bit6_only.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_source_byte_boundary
            && bit6_only.checkpoint().game_init_source_address==0x2c252
            && bit6_only.checkpoint().game_init_alternate_writes.size()==2
            && bit6_only.checkpoint().game_init_alternate_writes[0]==expected_replicated_first
            && bit6_only.checkpoint().game_init_alternate_writes[1]==expected_replicated_second
            && bit6_only.checkpoint().game_init_alternate_run_sha256=="6429d7b0634cff176ec01486b3f4e05bd648e3de11a67edd151f8345724b6701");
        auto alternate_memory=bsr_memory;
        assert(alternate_memory.apply(bit6_only.make_game_init_alternate_effect_batch("replicated-run")).accepted
            && alternate_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,alternate_destination})==0xab
            && alternate_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,alternate_destination+8U})==0xab);
        assert(bit7_set.observe_game_init_swapped_pair({1,23,0x2b37a,0x2c251,0x12,0x2c252,0x34}).accepted
            && bit7_set.checkpoint().game_init_alternate_writes.size()==2
            && bit7_set.checkpoint().game_init_alternate_writes.front().value==0x3412
            && bit7_set.checkpoint().game_init_source_address==0x2c253
            && bit7_set.checkpoint().game_init_alternate_run_sha256=="dbf80460ade3c9cc5fba8b4a62937920cc9e131052d3a48bfc8b0981e150a9b9");
        assert(bit6_clear.observe_game_init_extended_run({1,23,0x2b3c0,0x2c251,0x02,0x2c252,0x56,0x2c253,0x78}).accepted
            && bit6_clear.checkpoint().game_init_alternate_writes.size()==2
            && bit6_clear.checkpoint().game_init_alternate_writes.front().value==0x7856
            && bit6_clear.checkpoint().game_init_source_address==0x2c254);
        assert(!user_consumer.execute_jsr_2b2be().accepted);
        assert(negative_fopen.observe_gemdos_selector_61({1,18,0x2a5b4,0x3d,-33}).accepted);
        assert(negative_fopen.checkpoint().state==eon::MillenniumAtariConfigConsumerState::fopen_failure_spin
            && negative_fopen.checkpoint().fopen_branch_target==0x2a632);
        assert(!negative_fopen.execute_jsr_2a5c2().accepted);
        assert(!negative_fopen.observe_gemdos_selector_61({1,19,0x2a5b4,0x3d,-1}).accepted);
        assert(!user_consumer.observe_status_register(
            {1, 2, 0x2aa88, 0, eon::MillenniumAtariObservedPrivilege::user}).accepted);

        eon::MillenniumAtariConfigConsumerSession supervisor_consumer(1, memory,
            gemdos.checkpoint(), session.fread_config_load_address_boundary(),
            session.fread_mapped_config_prelude());
        assert(supervisor_consumer.observe_status_register(
            {1, 8, 0x2aa88, 0x2700,
                eon::MillenniumAtariObservedPrivilege::supervisor}).accepted);
        const auto& supervisor_path = supervisor_consumer.checkpoint();
        assert(!supervisor_path.branch_taken && supervisor_path.hardware_write_executed
            && supervisor_path.hardware_writes.size() == 3
            && supervisor_path.resulting_status_register == 0x0300
            && supervisor_path.local_instruction_count == 10);
        auto hardware_memory = memory;
        const auto hardware_batches =
            supervisor_consumer.make_hardware_effect_batches("supervisor");
        assert(hardware_batches.size() == 2);
        for (const auto& batch : hardware_batches) assert(hardware_memory.apply(batch).accepted);
        assert(hardware_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
            std::nullopt, 0xffff8800U}) == 0x0e);
        assert(hardware_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
            std::nullopt, 0xffff8802U}) == 0xff);
        assert(supervisor_consumer.observe_xbios_selector_two(
            {1, 9, 0x2a520, 2, 0x89abcdef}).accepted);
        assert(hardware_memory.apply(supervisor_consumer.make_selector_two_result_effect_batch(
            "supervisor-selector-two-result")).accepted);
        assert(hardware_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
            std::nullopt, 0x2a50a}) == 0x89);
        assert(hardware_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
            std::nullopt, 0x2a50d}) == 0xef);
        assert(supervisor_consumer.observe_xbios_selector_three(
            {1, 10, 0x2a52e, 3, 0x10203040}).accepted);
        assert(hardware_memory.apply(supervisor_consumer.make_selector_three_result_effect_batch(
            "supervisor-selector-three-result")).accepted);
        assert(supervisor_consumer.observe_xbios_selector_four(
            {1, 11, 0x2a53c, 4, 0x50607080}).accepted);
        rejects([&] {
            const eon::NativeRuntimeMemory empty_memory(0x01000000U);
            static_cast<void>(eon::MillenniumAtariConfigConsumerSession(1, empty_memory,
                gemdos.checkpoint(), session.fread_config_load_address_boundary(),
                session.fread_mapped_config_prelude()));
        });
        rejects([&] {
            static_cast<void>(eon::MillenniumAtariConfigConsumerSession(2, memory,
                gemdos.checkpoint(), session.fread_config_load_address_boundary(),
                session.fread_mapped_config_prelude()));
        });
        eon::MillenniumAtariReadOnlyGemdosSession mutable_gemdos(1, disk,
            session.fopen_boundary(), session.fread_frame_prefix(),
            session.fread_config_transfer());
        assert(!mutable_gemdos.revoke(2).accepted);
        assert(mutable_gemdos.revoke(1).accepted);
        assert(mutable_gemdos.checkpoint().state
            == eon::MillenniumAtariReadOnlyGemdosState::revoked);
        bool revoked_batch_rejected = false;
        try {
            static_cast<void>(mutable_gemdos.make_fread_effect_batch("stale"));
        } catch (const std::runtime_error&) {
            revoked_batch_rejected = true;
        }
        assert(revoked_batch_rejected);
        return 0;
    }
    assert(argc == 1);
    const auto bytes = structural_prg_fixture();
    const auto prg = eon::parse_atari_st_prg(bytes);
    assert(prg.text_bytes == 8 && prg.data_bytes == 0 && prg.bss_bytes == 4);
    assert(prg.relocations.size() == 1 && prg.relocations.front().offset == 2);

    const auto loaded = eon::materialize_atari_st_prg_load(bytes, prg, 0x1000, 0x10000);
    assert(loaded.load_base == 0x1000 && loaded.entry_address == 0x1000);
    assert(loaded.image.size() == 12 && loaded.relocation_effects.size() == 1);
    assert((loaded.relocation_effects.front()
        == eon::AtariStPrgRelocationEffect{2, 0x1002, 0x120, 0x1120}));
    assert(loaded.image[2] == 0 && loaded.image[3] == 0
        && loaded.image[4] == 0x11 && loaded.image[5] == 0x20);
    for (std::size_t index = 8; index < loaded.image.size(); ++index) {
        assert(loaded.image[index] == 0);
    }

    auto mismatched = prg;
    mismatched.relocations.front().original_value ^= 1U;
    rejects([&] { static_cast<void>(
        eon::materialize_atari_st_prg_load(bytes, mismatched, 0x1000, 0x10000)); });
    rejects([&] { static_cast<void>(
        eon::materialize_atari_st_prg_load(bytes, prg, 0xfff8, 0x10000)); });
    rejects([&] { static_cast<void>(
        eon::materialize_atari_st_prg_load(bytes, prg, 0xff00, 0x1000)); });
}
