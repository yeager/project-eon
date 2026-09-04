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
