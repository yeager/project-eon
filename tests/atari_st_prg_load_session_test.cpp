#include "data/atari_st_prg.hpp"
#include "data/fat12.hpp"
#include "data/sha256.hpp"
#include "engine/atari_st_prg_load_session.hpp"
#include "engine/millennium_atari_bootstrap_session.hpp"
#include "engine/millennium_atari_config_consumer_session.hpp"
#include "engine/millennium_atari_post_config_entry_session.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <span>
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
        const std::vector<std::uint8_t> disk_bytes{
            std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>()};
        assert(eon::to_hex(eon::sha256(disk_bytes))
            == "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7");
        const eon::Fat12Disk disk(disk_bytes);
        const auto* entry = disk.find("MILENIUM.TOS");
        assert(entry && !entry->directory());
        const auto program = disk.read(*entry);
        const eon::MillenniumAtariBootstrapSession session(disk, program);
        assert(session.execution().second_copy_instruction_count == 518
            && session.execution().second_copy_stop_address == 0x77000
            && session.target().bytes.size() == 0x202);
        const auto post_config_batch = session.make_post_config_fread_effect_batch(
            84720, "exact-post-config-read");
        const auto* post_config_entry = disk.find("MILL22B.INF");
        assert(post_config_entry && !post_config_entry->directory());
        const auto post_config_bytes = disk.read(*post_config_entry);
        assert(post_config_batch.fully_admitted
            && post_config_batch.effects.size() == post_config_bytes.size()
            && post_config_batch.effects.front().location.offset == 0x11e00
            && post_config_batch.effects.back().location.offset == 0x11e00 + 84719
            && post_config_batch.effects.front().value == post_config_bytes.front()
            && post_config_batch.effects.back().value == post_config_bytes.back());
        const auto& post_config_module = session.post_config_module_entry();
        assert(post_config_module.load_address == 0x11e00
            && post_config_module.initial_jump_address == 0x1c62c
            && post_config_module.initial_jump_file_offset == 0xa82c
            && post_config_module.entry_prologue_bytes == 24
            && post_config_module.file_sha256
                == "e315b0ec01f2fe429fdce101765577b893d031389c540de1fbe43eca121d53e9"
            && post_config_module.entry_prologue_sha256
                == "f97319598c3c193dc292abbf86c0b94c814f4b9ecafcdba612bb0116660c93b6");
        const std::span<const std::uint8_t> module_span(post_config_bytes);
        assert(eon::to_hex(eon::sha256(module_span.subspan(0xa854, 10)))
            == "b553e819435703a8ba790781ccc1137f9e88d1d4827a47d289695445f7256131");
        assert(eon::to_hex(eon::sha256(module_span.subspan(0x18, 42)))
            == "569b54f0d351f7db543b15c4e0227fd9b21e078dadffaa45ee767d1d1ceae474");
        assert(eon::to_hex(eon::sha256(module_span.subspan(0x18, 44)))
            == "3b64ebbfce7fcec18135159b6d2338fcf3695e67e378c1983d28c63cb0b35e88");
        assert(eon::to_hex(eon::sha256(module_span.subspan(0x42, 2)))
            == "20b64b56f584ec6c5184846cbeb974347b5e4c25cb49628ce1426fd0d2128ae7");
        assert(eon::to_hex(eon::sha256(module_span.subspan(0x44, 22)))
            == "5ab9d1696078069db836401df103614c0cb0862dd25d8dde549ee7f96dd2aa06");
        assert(eon::to_hex(eon::sha256(module_span.subspan(0xa85e, 0x60)))
            == "3eac6059a1d4d063b2d8f107b236aa76f63e324aea672e5fc49561ac25309a35");
        assert(eon::to_hex(eon::sha256(module_span.subspan(0xa7a0, 14)))
            == "08f40fb3653f5428f32760d322ce08e49e55f940b74bee9472ada9637e13ba0c");
        assert(eon::to_hex(eon::sha256(module_span.subspan(0x1160c, 0x16)))
            == "e773ba06fbe710796e42e0b323b3ed2b63a9d97d6b087e1f8386e060c1096be2");
        assert(eon::to_hex(eon::sha256(module_span.subspan(0x3020, 0x28)))
            == "5a59557110f2435a7c795c82d11a2bde496acacf30be05f532024a62f21a9f39");
        assert(eon::to_hex(eon::sha256(module_span.subspan(0xe30e, 0x18)))
            == "02d0c1902077eccefdd88b85e64823bd1a004f7375afa357ac679074c8af3377");
        assert(eon::to_hex(eon::sha256(module_span.subspan(0xe1fc, 0x33)))
            == "07c62426cd5874d72fac9ef033d9c5a53512248062c7b59afa7f2f6d855066d5");
        assert(eon::to_hex(eon::sha256(module_span.subspan(0xe29e, 0x3c)))
            == "1c40ffe44b9d4365635b1e802e7472bc32f45dcd0cbe64f474b7361074c71e33");
        assert(eon::to_hex(eon::sha256(module_span.subspan(0xb8, 6)))
            == "418247414cff2845909ba70c0283db550e2719ff7b39ca694994faf43c5a13ce");
        assert(eon::to_hex(eon::sha256(module_span.subspan(0xc0cf, 4)))
            == "dec3e22f141ef825023ad1a061227f60183668a5d254310d9f057c06a466183a");
        assert(eon::to_hex(eon::sha256(module_span.subspan(0xc0cf, 0x2c)))
            == "c4c430dfed566d9f150ff3bc759f97d9642d6b819db86a00d638cd44cfc2f037");
        assert(eon::to_hex(eon::sha256(module_span.subspan(0xe20c, 0x11c)))
            == "6ecf4af4b37b7e5d4bee56a475cc1d12cc765239d8858cdcf92188876ac00ac9");
        assert(eon::to_hex(eon::sha256(module_span.subspan(0xe1a0, 0x28)))
            == "2fcb317f626e51083cc64349aa163c086c887339e6acc0b3977c9a50fa3e60d9");
        assert(eon::to_hex(eon::sha256(module_span.subspan(0xe1ca, 0x28)))
            == "163b347c6a3fe378ece8ee2382581ff88a6f28f682dbb10df84e7f15860b781c");
        assert(eon::to_hex(eon::sha256(module_span.subspan(0xe328, 0x128)))
            == "7b9320e4f8536869521d87f6c51b05834d5b6450856715d16d15dce0c9b0a990");
        assert(eon::to_hex(eon::sha256(module_span.subspan(0xe3e0, 0x70)))
            == "b17d55dd892c07ea84a53cd1c6ba2a07e212ed2f6cfa9f3632363c6cdaabf970");
        assert(eon::to_hex(eon::sha256(module_span.subspan(0xb8, 8)))
            == "e4d0962ff2756c0df3139a525645b0c1997a25fa2edae517f55ff70123d6122e");
        assert(eon::to_hex(eon::sha256(module_span.subspan(0xaf36, 2)))
            == "7c8f3f81b07b41558863745af3bf20d0efc594947659ed7b9390a0b6d9d77244");
        assert(module_span[0x42] == 0xa0 && module_span[0x43] == 0x00);
        auto changed_post_config_module = post_config_bytes;
        changed_post_config_module[0xa82c] ^= 0x01;
        rejects([&] { static_cast<void>(eon::parse_millennium_atari_post_config_module_entry(
            changed_post_config_module)); });
        rejects([&] { static_cast<void>(eon::parse_millennium_atari_post_config_module_entry(
            std::span<const std::uint8_t>(post_config_bytes).first(5))); });
        eon::NativeRuntimeMemory post_config_memory;
        assert(post_config_memory.apply(post_config_batch).accepted);
        for (std::size_t index = 0; index < post_config_bytes.size(); ++index) {
            assert(post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                std::nullopt, 0x11e00U + index}) == post_config_bytes[index]);
        }
        {
            eon::MillenniumAtariPostConfigEntrySession entry_session(
                1, post_config_module, post_config_bytes);
            const eon::MillenniumAtariPostConfigEntryJumpObservation jump{
                1, 1, 0x11e00};
            assert(entry_session.observe_entry_jump(jump, post_config_memory).accepted);
            const eon::MillenniumAtariStatusRegisterObservation supervisor_sr{
                1, 2, 0x1c62c, 0x2700,
                eon::MillenniumAtariObservedPrivilege::supervisor};
            assert(entry_session.observe_status_register(supervisor_sr, post_config_memory).accepted);
            const auto& checkpoint = entry_session.checkpoint();
            assert(checkpoint.state == eon::MillenniumAtariPostConfigEntryState::xbios_trap_boundary
                && checkpoint.entry_jump_observed && checkpoint.status_register_observed
                && checkpoint.supervisor_branch_taken && checkpoint.resulting_data_register == 0x07ff
                && checkpoint.resulting_status_register == 0x0300
                && checkpoint.local_jsr_return_address == 0x1c64e
                && !checkpoint.local_jsr_return_address_materialized
                && checkpoint.xbios_trap_address == 0x1f92e
                && checkpoint.xbios_selector == 0x26
                && checkpoint.xbios_pointer_argument == 0x1f934
                && checkpoint.relative_stack_delta == -6
                && checkpoint.relative_stack_bytes
                    == std::vector<std::uint8_t>({0, 0x26, 0, 1, 0xf9, 0x34})
                && checkpoint.hardware_writes.size() == 3
                && checkpoint.hardware_writes[0].address == 0xffff8800U
                && checkpoint.hardware_writes[0].value == 0x07
                && checkpoint.hardware_writes[1].address == 0xffff8802U
                && checkpoint.hardware_writes[1].value == 0xff
                && checkpoint.hardware_writes[2].address == 0xffff8800U
                && checkpoint.hardware_writes[2].value == 0x0e);
            {
                eon::MillenniumAtariPostConfigEntrySession returned_to_buffer(
                    1, post_config_module, post_config_bytes);
                assert(!returned_to_buffer.execute_entry_jump(
                    post_config_memory, 1, 0x77042).accepted);
                assert(returned_to_buffer.checkpoint().state
                    == eon::MillenniumAtariPostConfigEntryState::entry_jump_boundary);
                assert(returned_to_buffer.execute_entry_jump(
                    post_config_memory, 1, 0x11e00).accepted);
                const auto& jump_checkpoint = returned_to_buffer.checkpoint();
                assert(jump_checkpoint.state
                    == eon::MillenniumAtariPostConfigEntryState::status_register_boundary
                    && jump_checkpoint.entry_jump_executed
                    && !jump_checkpoint.entry_jump_observed
                    && jump_checkpoint.last_sequence == 1);
                const eon::MillenniumAtariStatusRegisterObservation observed_sr{
                    1, 2, 0x1c62c, 0x0000,
                    eon::MillenniumAtariObservedPrivilege::user};
                assert(returned_to_buffer.observe_status_register(
                    observed_sr, post_config_memory).accepted);
            }
            auto returned = entry_session;
            eon::MillenniumAtariPostConfigXbiosReturnObservation xbios_return;
            xbios_return.generation = 1;
            xbios_return.sequence = 3;
            xbios_return.trap_address = 0x1f92e;
            xbios_return.selector = 0x26;
            xbios_return.argument_stack_address = 0x8000;
            xbios_return.argument_bytes = {0x00, 0x26, 0x00, 0x01, 0xf9, 0x34};
            xbios_return.post_service_pc = 0x1f930;
            xbios_return.post_service_sr = 0x2300;
            xbios_return.post_service_data[0] = 0xdeadbeef;
            xbios_return.post_service_address[7] = 0x8000;
            xbios_return.rts_stack_address = 0x8006;
            xbios_return.rts_stack_bytes = {0x00, 0x01, 0xc6, 0x4e};
            xbios_return.memory_effects.push_back({0xffff8800U, 0x07, 0x0e});
            const std::array<std::uint8_t, 6> expected_selector_15_frame{
                0x00, 0x15, 0, 0, 0, 0};
            assert(returned.observe_xbios_26_return(xbios_return).accepted);
            const auto& returned_checkpoint = returned.checkpoint();
            assert(returned_checkpoint.state
                    == eon::MillenniumAtariPostConfigEntryState::xbios_selector_15_boundary
                && returned_checkpoint.xbios_26_return_observed
                && returned_checkpoint.xbios_26_trap_address == 0x1f92e
                && returned_checkpoint.xbios_26_selector == 0x26
                && returned_checkpoint.xbios_26_argument_stack_address == 0x8000
                && returned_checkpoint.xbios_26_argument_bytes == xbios_return.argument_bytes
                && returned_checkpoint.xbios_26_post_service_pc == 0x1f930
                && returned_checkpoint.xbios_26_post_service_sr == 0x2300
                && returned_checkpoint.xbios_26_post_service_data[0] == 0xdeadbeef
                && returned_checkpoint.xbios_26_post_service_address[7] == 0x8000
                && returned_checkpoint.xbios_26_rts_stack_address == 0x8006
                && returned_checkpoint.xbios_26_rts_return_address == 0x1c64e
                && returned_checkpoint.xbios_26_memory_effects == xbios_return.memory_effects
                && returned_checkpoint.local_jsr_return_address_materialized
                && returned_checkpoint.selector_15_trap_address == 0x1c654
                && returned_checkpoint.xbios_trap_address == 0x1c654
                && returned_checkpoint.xbios_selector == 0x15
                && returned_checkpoint.selector_15_argument_stack_address == 0x8004
                && returned_checkpoint.selector_15_argument_bytes == expected_selector_15_frame);
            auto bad_return = entry_session;
            auto bad_observation = xbios_return;
            bad_observation.rts_stack_bytes[3] ^= 1;
            assert(!bad_return.observe_xbios_26_return(bad_observation).accepted);
            assert(bad_return.checkpoint().state
                == eon::MillenniumAtariPostConfigEntryState::xbios_trap_boundary);
            auto changed_post_service_a7 = entry_session;
            bad_observation = xbios_return;
            bad_observation.post_service_address[7] = 0x8001;
            bad_observation.rts_stack_address = 0x8007;
            assert(!changed_post_service_a7.observe_xbios_26_return(bad_observation).accepted);
            assert(changed_post_service_a7.checkpoint().state
                == eon::MillenniumAtariPostConfigEntryState::xbios_trap_boundary);
            auto wrong_rts_stack = entry_session;
            bad_observation = xbios_return;
            bad_observation.rts_stack_address = 0x8007;
            assert(!wrong_rts_stack.observe_xbios_26_return(bad_observation).accepted);
            assert(wrong_rts_stack.checkpoint().state
                == eon::MillenniumAtariPostConfigEntryState::xbios_trap_boundary);
            auto wrapping_frame = entry_session;
            bad_observation = xbios_return;
            bad_observation.argument_stack_address = 0xfffffffAU;
            bad_observation.post_service_address[7] = 0xfffffffAU;
            bad_observation.rts_stack_address = 0;
            assert(!wrapping_frame.observe_xbios_26_return(bad_observation).accepted);
            assert(wrapping_frame.checkpoint().state
                == eon::MillenniumAtariPostConfigEntryState::xbios_trap_boundary);

            auto post_config = returned;
            eon::MillenniumAtariPostConfigXbiosResultObservation service_result;
            service_result.generation = 1;
            service_result.sequence = 4;
            service_result.trap_address = 0x1c654;
            service_result.selector = 0x15;
            service_result.argument_stack_address = 0x8004;
            service_result.argument_bytes = {0x00, 0x15, 0, 0, 0, 0};
            service_result.post_service_pc = 0x1c656;
            service_result.post_service_a7 = 0x8004;
            service_result.result_d0 = 0xfedcba98;
            auto stale_result = post_config;
            auto bad_service_result = service_result;
            bad_service_result.sequence = 3;
            assert(!stale_result.observe_xbios_result(bad_service_result, post_config_memory).accepted);
            assert(stale_result.checkpoint().state
                == eon::MillenniumAtariPostConfigEntryState::xbios_selector_15_boundary);
            bad_service_result = service_result;
            bad_service_result.argument_bytes[0] = 0x01;
            assert(!stale_result.observe_xbios_result(bad_service_result, post_config_memory).accepted);
            bad_service_result = service_result;
            bad_service_result.post_service_a7++;
            assert(!stale_result.observe_xbios_result(bad_service_result, post_config_memory).accepted);

            auto altered_module_memory = post_config_memory;
            const eon::NativeRuntimeEffectBatch alter_prefix{
                "alter-post-config-prefix", true,
                {{1, {eon::NativeRuntimeAddressSpace::linear, std::nullopt, 0x11e18},
                    eon::MemoryTransferElementWidth::byte, eon::NativeRuntimeByteOrder::big_endian, 0}}};
            assert(altered_module_memory.apply(alter_prefix).accepted);
            assert(!post_config.observe_xbios_result(service_result, altered_module_memory).accepted);
            assert(post_config.checkpoint().state
                == eon::MillenniumAtariPostConfigEntryState::xbios_selector_15_boundary);

            assert(post_config.observe_xbios_result(service_result, post_config_memory).accepted);
            assert(post_config.checkpoint().state
                    == eon::MillenniumAtariPostConfigEntryState::xbios_selector_2_boundary
                && post_config.checkpoint().selector_15_return_observed
                && post_config.checkpoint().selector_15_result_d0 == 0xfedcba98
                && post_config.checkpoint().post_config_call_stack_address == 0x8006
                && post_config.checkpoint().post_config_result_trap_address == 0x11e1c
                && post_config.checkpoint().post_config_result_argument_bytes
                    == std::vector<std::uint8_t>({0, 2}));
            const auto make_result = [](std::uint64_t sequence, std::uint32_t trap,
                std::uint16_t selector, std::uint32_t pc, std::uint32_t d0) {
                eon::MillenniumAtariPostConfigXbiosResultObservation result;
                result.generation = 1;
                result.sequence = sequence;
                result.trap_address = trap;
                result.selector = selector;
                result.argument_stack_address = 0x8004;
                result.argument_bytes = {0, static_cast<std::uint8_t>(selector)};
                result.post_service_pc = pc;
                result.post_service_a7 = 0x8004;
                result.result_d0 = d0;
                return result;
            };
            auto selector2 = make_result(5, 0x11e1c, 2, 0x11e1e, 0x11223344);
            auto wrong_order = post_config;
            auto selector3 = make_result(6, 0x11e2a, 3, 0x11e2c, 0x00080000);
            assert(!wrong_order.observe_xbios_result(selector3, post_config_memory).accepted);
            auto wrong_selector = post_config;
            auto bad_selector2 = selector2;
            bad_selector2.selector = 3;
            bad_selector2.argument_bytes = {0, 3};
            assert(!wrong_selector.observe_xbios_result(bad_selector2, post_config_memory).accepted);
            auto wrong_trap = post_config;
            bad_selector2 = selector2;
            bad_selector2.trap_address++;
            assert(!wrong_trap.observe_xbios_result(bad_selector2, post_config_memory).accepted);
            assert(post_config.observe_xbios_result(selector2, post_config_memory).accepted);
            auto selector2_effects = post_config.make_xbios_result_effect_batch("post-config-selector-2");
            assert(selector2_effects.fully_admitted && selector2_effects.effects.size() == 1
                && selector2_effects.effects[0].location.offset == 0x11e06
                && selector2_effects.effects[0].width == eon::MemoryTransferElementWidth::longword
                && selector2_effects.effects[0].value == 0x11223344);
            auto delayed_store = post_config;
            assert(!delayed_store.observe_xbios_result(selector3, post_config_memory).accepted);
            assert(post_config_memory.apply(selector2_effects).accepted);
            assert(post_config.observe_xbios_result(selector3, post_config_memory).accepted);
            assert(post_config.checkpoint().state
                == eon::MillenniumAtariPostConfigEntryState::xbios_selector_4_boundary
                && post_config.checkpoint().post_config_raw_result_d0[0] == 0x11223344);
            auto selector3_effects = post_config.make_xbios_result_effect_batch("post-config-selector-3");
            assert(selector3_effects.effects.size() == 1
                && selector3_effects.effects[0].location.offset == 0x11e0a
                && selector3_effects.effects[0].width == eon::MemoryTransferElementWidth::longword
                && selector3_effects.effects[0].value == 0x00080000);
            assert(post_config_memory.apply(selector3_effects).accepted);
            const auto selector4 = make_result(7, 0x11e38, 4, 0x11e3a, 0x1234abcd);
            assert(post_config.observe_xbios_result(selector4, post_config_memory).accepted);
            assert(post_config.checkpoint().state
                == eon::MillenniumAtariPostConfigEntryState::line_a_instruction_boundary
                && post_config.checkpoint().post_config_raw_result_d0[1] == 0x00080000
                && post_config.checkpoint().post_config_raw_result_d0[2] == 0x1234abcd
                && post_config.checkpoint().post_config_result_write_address == 0x11e0e
                && post_config.checkpoint().post_config_result_write_width == 2
                && post_config.checkpoint().post_config_result_write_value == 0xabcd
                && post_config.checkpoint().line_a_instruction_address == 0x11e42
                && post_config.checkpoint().line_a_instruction_opcode == 0xa000);
            const auto selector4_effects = post_config.make_xbios_result_effect_batch(
                "post-config-selector-4");
            assert(selector4_effects.effects.size() == 1
                && selector4_effects.effects[0].location.offset == 0x11e0e
                && selector4_effects.effects[0].width == eon::MemoryTransferElementWidth::word
                && selector4_effects.effects[0].value == 0xabcd);
            assert(post_config_memory.apply(selector4_effects).accepted);
            assert(post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x11e06}) == 0x11
                && post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x11e09}) == 0x44
                && post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x11e0a}) == 0x00
                && post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x11e0d}) == 0x00
                && post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x11e0e}) == 0xab
                && post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x11e0f}) == 0xcd);
            assert(!post_config.observe_xbios_result(selector4, post_config_memory).accepted);

            eon::MillenniumAtariPostConfigLineAObservation line_a;
            line_a.generation = 1;
            line_a.sequence = 8;
            line_a.instruction_address = 0x11e42;
            line_a.instruction_opcode = 0xa000;
            line_a.post_service_pc = 0x11e44;
            line_a.post_service_data[0] = 0x76543210;
            line_a.post_service_address[0] = 0x3000;
            line_a.post_service_address[7] = 0x8006;
            line_a.memory_reads = {{{0x3008, {0x12, 0x34, 0xab, 0xcd}},
                {0x300c, {0x89, 0xab, 0xcd, 0xef}}}};
            line_a.rts_stack_address = 0x8006;
            line_a.rts_stack_bytes = {0x00, 0x01, 0xc6, 0x5e};
            const eon::NativeRuntimeEffectBatch fclose_value{
                "post-config-fclose-value", true,
                {{1, {eon::NativeRuntimeAddressSpace::linear, std::nullopt, 0x11dfc},
                    eon::MemoryTransferElementWidth::longword,
                    eon::NativeRuntimeByteOrder::big_endian, 0x361436a7}}};
            assert(post_config_memory.apply(fclose_value).accepted);
            auto bad_line_a = post_config;
            auto bad_line_a_observation = line_a;
            bad_line_a_observation.instruction_opcode = 0xa001;
            assert(!bad_line_a.observe_line_a_return(bad_line_a_observation,
                post_config_memory).accepted);
            bad_line_a_observation = line_a;
            bad_line_a_observation.rts_stack_bytes[3] ^= 1;
            assert(!bad_line_a.observe_line_a_return(bad_line_a_observation,
                post_config_memory).accepted);
            bad_line_a_observation = line_a;
            bad_line_a_observation.memory_reads[1].address++;
            assert(!bad_line_a.observe_line_a_return(bad_line_a_observation,
                post_config_memory).accepted);
            bad_line_a_observation = line_a;
            bad_line_a_observation.post_service_address[0] = 0xfffffff8;
            assert(!bad_line_a.observe_line_a_return(bad_line_a_observation,
                post_config_memory).accepted);
            assert(bad_line_a.checkpoint().state
                == eon::MillenniumAtariPostConfigEntryState::line_a_instruction_boundary);

            auto mismatch_path = post_config;
            eon::NativeRuntimeMemory mismatch_memory = post_config_memory;
            const eon::NativeRuntimeEffectBatch other_fclose_value{
                "post-config-other-fclose-value", true,
                {{1, {eon::NativeRuntimeAddressSpace::linear, std::nullopt, 0x11dfc},
                    eon::MemoryTransferElementWidth::longword,
                    eon::NativeRuntimeByteOrder::big_endian, 0x01020304}}};
            assert(mismatch_memory.apply(other_fclose_value).accepted);
            assert(mismatch_path.observe_line_a_return(line_a, mismatch_memory).accepted);

            assert(post_config.observe_line_a_return(line_a, post_config_memory).accepted);
            const auto& line_a_checkpoint = post_config.checkpoint();
            assert(line_a_checkpoint.state
                    == eon::MillenniumAtariPostConfigEntryState::post_line_a_subroutine_boundary
                && line_a_checkpoint.line_a_return_observed
                && line_a_checkpoint.line_a_post_service_pc == 0x11e44
                && line_a_checkpoint.line_a_rts_return_address == 0x1c65e
                && line_a_checkpoint.line_a_result_a3 == 0x1234abcd
                && line_a_checkpoint.line_a_result_a4 == 0x89abcdef
                && line_a_checkpoint.caller_compare_equal
                && line_a_checkpoint.caller_final_d1 == 0x2d4a6
                && line_a_checkpoint.first_local_jsr_address == 0x1c5a0
                && line_a_checkpoint.first_local_jsr_return_address == 0x1c6be
                && line_a_checkpoint.first_local_jsr_stack_address == 0x1cad6
                && line_a_checkpoint.next_local_call_boundary_address == 0x1c6be);
            const auto line_a_effects = post_config.make_line_a_continuation_effect_batch(
                "post-config-line-a-continuation");
            assert(line_a_effects.effects.size() == 10);
            assert(line_a_effects.effects[2].location.offset == 0x1ff66
                && line_a_effects.effects[2].value == 0x76543210
                && line_a_effects.effects[2].value
                    != post_config.checkpoint().post_config_raw_result_d0[1]);
            assert(post_config_memory.apply(line_a_effects).accepted);
            assert(post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x1ff69}) == 0x10);
            assert(post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x11e13}) == 0xcd
                && post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x11e17}) == 0xef
                && post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x1cad9}) == 0xbe
                && post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x1242f}) == 0xe0);

            eon::MillenniumAtariPostConfigLocalCallObservation local_call;
            local_call.generation = 1;
            local_call.sequence = 9;
            local_call.instruction_address = 0x2340c;
            local_call.data_registers[0] = 0x1cae0;
            local_call.data_registers[1] = 0x2d4a6;
            local_call.address_registers = line_a.post_service_address;
            local_call.address_registers[7] = 0x1cad6;
            local_call.rts_stack_address = 0x1cad6;
            local_call.rts_stack_bytes = {0x00, 0x01, 0xc6, 0xc4};
            local_call.copy_source_address = 0x00081680;
            local_call.copy_source_bytes.resize(0x17c0U * 4U);
            for (std::size_t index = 0; index < local_call.copy_source_bytes.size(); ++index) {
                local_call.copy_source_bytes[index] = static_cast<std::uint8_t>(index & 0xffU);
            }
            for (std::size_t index = 2; index < local_call.data_registers.size(); ++index) {
                local_call.data_registers[index] = line_a.post_service_data[index];
            }
            local_call.data_registers[6] = 0x361436a7;
            auto wrong_local_call = post_config;
            auto bad_local_call = local_call;
            bad_local_call.copy_source_bytes.pop_back();
            assert(!wrong_local_call.observe_local_call_2340c(bad_local_call,
                post_config_memory).accepted);
            bad_local_call = local_call;
            bad_local_call.rts_stack_bytes[3] ^= 1;
            assert(!wrong_local_call.observe_local_call_2340c(bad_local_call,
                post_config_memory).accepted);
            bad_local_call = local_call;
            bad_local_call.copy_source_address++;
            assert(!wrong_local_call.observe_local_call_2340c(bad_local_call,
                post_config_memory).accepted);
            const auto local_call_result = post_config.observe_local_call_2340c(
                local_call, post_config_memory);
            if (!local_call_result.accepted) std::cerr << local_call_result.error << '\n';
            assert(local_call_result.accepted);
            assert(post_config.checkpoint().state
                    == eon::MillenniumAtariPostConfigEntryState::bytecode_command_boundary
                && post_config.checkpoint().local_call_trap_address == 0
                && post_config.checkpoint().local_call_trap_selector == 0
                && post_config.checkpoint().local_call_trap_stack_address == 0
                && post_config.checkpoint().local_call_entry_return_address == 0x1c6c4
                && post_config.checkpoint().local_call_copy_byte_count == 0x5f00
                && post_config.checkpoint().local_call_macro_address == 0x1decf
                && post_config.checkpoint().local_call_macro_next_address == 0x1ded2
                && post_config.checkpoint().local_call_macro_store_value == 0x00081e01
                && post_config.checkpoint().bytecode_next_address == 0x1ded2
                && post_config.checkpoint().bytecode_dispatch_return_address == 0x20026
                && post_config.checkpoint().next_local_call_boundary_address == 0x1ded2);
            const auto local_call_effects = post_config.make_local_call_2340c_effect_batch(
                "post-config-local-call-2340c");
            assert(local_call_effects.effects.size() == 0x17c0U * 2U + 2U);
            assert(local_call_effects.effects[0].location.offset == 0x81680
                && local_call_effects.effects[0x17c0U].location.offset == 0x27326
                && local_call_effects.effects[0x17c0U * 2U].location.offset == 0x1ff76
                && local_call_effects.effects[0x17c0U * 2U + 1U].location.offset == 0x1ff66
                && local_call_effects.effects[0x17c0U * 2U].order
                    < local_call_effects.effects[0x17c0U * 2U + 1U].order);
            assert(post_config_memory.apply(local_call_effects).accepted);
            assert(post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x27326}) == 0
                && post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x27327}) == 1
                && post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x2d225}) == 0xff
                && post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x1ff76}) == 1
                && post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x1ff66}) == 0x00
                && post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x1ff67}) == 0x08
                && post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x1ff68}) == 0x1e
                && post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x1ff69}) == 0x01);

            const auto bytecode_continuation_result =
                post_config.execute_bytecode_continuation(post_config_memory);
            if (!bytecode_continuation_result.accepted) {
                std::cerr << bytecode_continuation_result.error << '\n';
            }
            assert(bytecode_continuation_result.accepted);
            assert(post_config.checkpoint().state
                    == eon::MillenniumAtariPostConfigEntryState::local_call_trap_boundary
                && post_config.checkpoint().local_call_trap_address == 0x11ebc
                && post_config.checkpoint().local_call_trap_selector == 7
                && post_config.checkpoint().local_call_trap_stack_address == 0x1cad0
                && post_config.checkpoint().bytecode_next_address == 0x1defb
                && post_config.checkpoint().bytecode_final_screen_pointer == 0x82340
                && post_config.checkpoint().bytecode_continuation_effects.size() == 1056);
            const auto bytecode_effects = post_config.make_bytecode_continuation_effect_batch(
                "post-config-bytecode-continuation");
            assert(bytecode_effects.effects.size() == 1061
                && bytecode_effects.effects.front().location.offset >= 0x81680
                && bytecode_effects.effects[1056].location.offset == 0x1ff5e
                && bytecode_effects.effects[1057].location.offset == 0x1ff62
                && bytecode_effects.effects[1058].location.offset == 0x1ff66
                && bytecode_effects.effects[1059].location.offset == 0x1cad2
                && bytecode_effects.effects[1060].location.offset == 0x1cad0
                && post_config_memory.apply(bytecode_effects).accepted);
            assert(post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x1ff5e}) == 0x00
                && post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x1ff5f}) == 0x02
                && post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x1ff60}) == 0x38
                && post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x1ff61}) == 0x5c
                && post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x1cad0}) == 0
                && post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x1cad1}) == 7
                && post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x1cad2}) == 0
                && post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x1cad3}) == 0x02
                && post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x1cad4}) == 0x34
                && post_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x1cad5}) == 0x22);

            // Exercise the later typed GEMDOS boundaries with explicit test
            // observations. The test supplies the returned input flag and
            // stack frames; neither service result is synthesized by the
            // session itself.
            auto gemdos_path = post_config;
            auto gemdos_memory = post_config_memory;
            constexpr std::uint32_t fopen_stack = 0x70000U;
            eon::NativeRuntimeEffectBatch fopen_inputs{
                "post-config-fopen-test-inputs", true, {}};
            const auto append_byte = [&](const std::uint32_t address,
                                         const std::uint8_t value) {
                fopen_inputs.effects.push_back({fopen_inputs.effects.size() + 1U,
                    {eon::NativeRuntimeAddressSpace::linear, std::nullopt, address},
                    eon::MemoryTransferElementWidth::byte,
                    eon::NativeRuntimeByteOrder::big_endian, value});
            };
            for (std::size_t index = 0; index < local_call.copy_source_bytes.size(); ++index) {
                append_byte(local_call.copy_source_address + static_cast<std::uint32_t>(index),
                    local_call.copy_source_bytes[index]);
            }
            append_byte(0x1fa0fU, 0);
            const std::array<std::uint8_t, 12> fopen_stack_bytes{
                0x00, 0x3d, 0x00, 0x01, 0x20, 0x4a, 0x00, 0x02,
                0x00, 0x01, 0x1f, 0xe6};
            for (std::size_t index = 0; index < fopen_stack_bytes.size(); ++index) {
                append_byte(fopen_stack + static_cast<std::uint32_t>(index), fopen_stack_bytes[index]);
            }
            assert(gemdos_memory.apply(fopen_inputs).accepted);
            const eon::MillenniumAtariPostConfigCrawcinBranchObservation crawcin{
                1, 10, 0x1fc6eU, 0x1fa0fU, 0, 0x1fc84U};
            assert(gemdos_path.observe_crawcin_flag_branch(crawcin, gemdos_memory).accepted);
            const eon::MillenniumAtariPostConfigFopenObservation fopen{
                1, 11, 0x11fd8U, fopen_stack, fopen_stack,
                {0x00, 0x3d, 0x00, 0x01, 0x20, 0x4a, 0x00, 0x02}};
            assert(gemdos_path.observe_gemdos_fopen(fopen, gemdos_memory).accepted);
            const eon::MillenniumAtariPostConfigFopenReturnObservation fopen_return{
                1, 12, 0x11fd8U, 0x11fdaU, fopen_stack, 0x2700U, 0x42U,
                0x11fe6U, fopen_stack + 12U};
            assert(gemdos_path.observe_gemdos_fopen_return(
                fopen_return, gemdos_memory).accepted);
            assert(gemdos_path.checkpoint().gemdos_fcreate_boundary_prepared
                && gemdos_path.checkpoint().gemdos_fopen_stored_word == 0x42U
                && gemdos_path.checkpoint().gemdos_fopen_suffix_instruction_count == 4U);
            const auto fopen_effects = gemdos_path.make_gemdos_fopen_result_effect_batch(
                "post-config-fopen-test-result");
            assert(fopen_effects.effects.size() == 9U
                && gemdos_memory.apply(fopen_effects).accepted);
            constexpr std::uint32_t fcreate_stack = fopen_stack + 4U;
            const eon::MillenniumAtariPostConfigFcreateObservation fcreate{
                1, 13, 0x11ff6U, fcreate_stack, fcreate_stack,
                {0x00, 0x3c, 0x00, 0x01, 0x20, 0x4a, 0x00, 0x00}};
            assert(gemdos_path.observe_gemdos_fcreate(fcreate, gemdos_memory).accepted);
            const eon::NativeRuntimeEffectBatch fcreate_return_address{
                "post-config-fcreate-test-return-address", true,
                {{1, {eon::NativeRuntimeAddressSpace::linear, std::nullopt,
                      fcreate_stack + 8U},
                    eon::MemoryTransferElementWidth::longword,
                    eon::NativeRuntimeByteOrder::big_endian, 0x12004U}}};
            assert(gemdos_memory.apply(fcreate_return_address).accepted);
            const eon::MillenniumAtariPostConfigFcreateReturnObservation fcreate_return{
                1, 14, 0x11ff6U, 0x11ff8U, fcreate_stack, 0x2700U, 0,
                0x12004U, fcreate_stack + 12U};
            assert(gemdos_path.observe_gemdos_fcreate_return(
                fcreate_return, gemdos_memory).accepted);
            assert(gemdos_path.checkpoint().state
                    == eon::MillenniumAtariPostConfigEntryState::gemdos_fcreate_result_observed
                && gemdos_path.checkpoint().gemdos_fcreate_stored_word == 0
                && gemdos_path.checkpoint().gemdos_fcreate_suffix_instruction_count == 4U);
            const auto fcreate_effects = gemdos_path.make_gemdos_fcreate_result_effect_batch(
                "post-config-fcreate-test-result");
            assert(fcreate_effects.effects.size() == 1U
                && gemdos_memory.apply(fcreate_effects).accepted
                && gemdos_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x12056U}) == 0
                && gemdos_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x12057U}) == 0);

            assert(mismatch_path.checkpoint().state
                    == eon::MillenniumAtariPostConfigEntryState::caller_transfer_boundary
                && !mismatch_path.checkpoint().caller_compare_equal
                && mismatch_path.checkpoint().next_local_call_boundary_address == 0x1c676);
            const auto mismatch_effects = mismatch_path.make_line_a_continuation_effect_batch(
                "post-config-line-a-mismatch");
            assert(mismatch_effects.effects.size() == 3);
            const auto hardware_batch = entry_session.make_hardware_effect_batch(
                "millennium-atari-post-config-hardware");
            assert(hardware_batch.fully_admitted && hardware_batch.effects.size() == 3);
            assert(!entry_session.observe_status_register(supervisor_sr, post_config_memory).accepted);
        }
        {
            eon::MillenniumAtariPostConfigEntrySession entry_session(
                1, post_config_module, post_config_bytes);
            assert(entry_session.observe_entry_jump({1, 1, 0x11e00}, post_config_memory).accepted);
            const eon::MillenniumAtariStatusRegisterObservation user_sr{
                1, 2, 0x1c62c, 0x0300,
                eon::MillenniumAtariObservedPrivilege::user};
            assert(entry_session.observe_status_register(user_sr, post_config_memory).accepted);
            const auto& checkpoint = entry_session.checkpoint();
            assert(!checkpoint.supervisor_branch_taken && checkpoint.hardware_writes.empty()
                && checkpoint.resulting_data_register == 0x0300
                && checkpoint.resulting_status_register == 0x0304
                && checkpoint.xbios_trap_address == 0x1f92e);
            assert(entry_session.make_hardware_effect_batch("millennium-atari-user-path")
                .effects.empty());
        }
        {
            const auto short_batch = session.make_post_config_fread_effect_batch(
                6, "short-entry-only-read");
            eon::NativeRuntimeMemory short_memory;
            assert(short_memory.apply(short_batch).accepted);
            eon::MillenniumAtariPostConfigEntrySession entry_session(
                1, post_config_module, post_config_bytes);
            assert(entry_session.observe_entry_jump({1, 1, 0x11e00}, short_memory).accepted);
            const eon::MillenniumAtariStatusRegisterObservation user_sr{
                1, 2, 0x1c62c, 0x0300,
                eon::MillenniumAtariObservedPrivilege::user};
            assert(!entry_session.observe_status_register(user_sr, short_memory).accepted);
            assert(entry_session.checkpoint().state
                == eon::MillenniumAtariPostConfigEntryState::status_register_boundary);
        }
        {
            auto altered_batch = session.make_post_config_fread_effect_batch(
                84720, "altered-post-config-helper");
            const auto helper_effect = std::find_if(altered_batch.effects.begin(),
                altered_batch.effects.end(), [&](const auto& effect) {
                    return effect.location.offset == 0x1f924;
                });
            assert(helper_effect != altered_batch.effects.end());
            helper_effect->value ^= 0x01;
            eon::NativeRuntimeMemory altered_memory;
            assert(altered_memory.apply(altered_batch).accepted);
            eon::MillenniumAtariPostConfigEntrySession entry_session(
                1, post_config_module, post_config_bytes);
            assert(entry_session.observe_entry_jump({1, 1, 0x11e00}, altered_memory).accepted);
            const eon::MillenniumAtariStatusRegisterObservation user_sr{
                1, 2, 0x1c62c, 0x0300,
                eon::MillenniumAtariObservedPrivilege::user};
            assert(!entry_session.observe_status_register(user_sr, altered_memory).accepted);
            assert(entry_session.checkpoint().state
                == eon::MillenniumAtariPostConfigEntryState::status_register_boundary);
        }
        const auto short_post_config = session.make_post_config_fread_effect_batch(
            113, "short-post-config-read");
        const auto failed_post_config = session.make_post_config_fread_effect_batch(
            -1, "failed-post-config-read");
        assert(short_post_config.effects.size() == 113
            && failed_post_config.fully_admitted && failed_post_config.effects.empty());
        rejects([&] { static_cast<void>(session.make_post_config_fread_effect_batch(
            0x20000, "oversized-post-config-read")); });
        const auto& post_config_filename = session.post_config_filename();
        assert(post_config_filename.runtime_address == 0x1d6d8
            && post_config_filename.source_address == 0x1d652
            && post_config_filename.source_offset == 0x86
            && post_config_filename.program_file_offset == 0x121c
            && post_config_filename.nul_terminated_byte_count == 12
            && post_config_filename.filename == "MILL22B.inf"
            && post_config_filename.nul_terminated_sha256
                == "393a936fc20d9f40ecace75f74947833d28e947e3ba9a987761a7d1eb92a575b"
            && post_config_filename.caller_span_sha256
                == "dc2a50400e22fdbe4870f790d4f70c7446caa379dc68281a0445db4ee027fe4d");
        {
            auto changed_target = session.target();
            changed_target.bytes[0x42] ^= 0x01;
            bool rejected = false;
            try {
                static_cast<void>(eon::parse_millennium_atari_post_config_filename(
                    program, session.bss_source(), changed_target));
            } catch (const std::runtime_error&) {
                rejected = true;
            }
            assert(rejected);
        }
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
        // $77042 is a return inside the separately materialized $77000
        // bootstrap target.  It is deliberately absent from the resident
        // PRG/config memory used to acquire the config consumer.
        assert(!memory.read_byte({eon::NativeRuntimeAddressSpace::linear,
            std::nullopt, 0x77042}));
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
        const auto read_native_word=[&memory](const std::uint32_t address){
            const auto hi=*memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,address});
            const auto lo=*memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,address+1U});
            return static_cast<std::uint16_t>((hi<<8U)|lo);
        };
        assert(user_consumer.observe_d0_indexed_word({1,10,0x2b5de,0,0x2bdfe,indexed_word}).accepted);
        assert(bsr_memory.apply(user_consumer.make_d0_indexed_word_effect_batch("word")).accepted);
        assert(user_consumer.checkpoint().a0_indexed_instruction_address==0x2b5ec);
        const auto first_a0_source=static_cast<std::uint32_t>(0x2b0e8LL+static_cast<std::int16_t>(indexed_word));
        const auto first_a0_word=read_native_word(first_a0_source);
        assert(user_consumer.observe_a0_indexed_word({1,11,0x2b5ec,0,first_a0_source,first_a0_word}).accepted);
        assert(user_consumer.checkpoint().state==eon::MillenniumAtariConfigConsumerState::loop_branch_boundary
            && user_consumer.checkpoint().loop_a0_value==0x56eee4
            && user_consumer.checkpoint().loop_d0_value==static_cast<std::uint16_t>(first_a0_word+2U)
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
        const auto second_d0=user_consumer.checkpoint().loop_d0_value;
        const auto second_d0_source=static_cast<std::uint32_t>(0x2bdfeLL+static_cast<std::int16_t>(second_d0));
        const auto second_indexed_word=read_native_word(second_d0_source);
        assert(!user_consumer.observe_d0_indexed_word({1,12,0x2b5de,static_cast<std::uint16_t>(second_d0+2U),second_d0_source+2U,read_native_word(second_d0_source+2U)}).accepted);
        assert(user_consumer.observe_d0_indexed_word({1,12,0x2b5de,second_d0,second_d0_source,second_indexed_word}).accepted);
        const auto second_a0_source=static_cast<std::uint32_t>(0x2b0e8LL+static_cast<std::int16_t>(second_indexed_word));
        assert(user_consumer.observe_a0_indexed_word({1,13,0x2b5ec,0,second_a0_source,read_native_word(second_a0_source)}).accepted);
        assert(user_consumer.checkpoint().loop_d7_value==0);
        assert(user_consumer.execute_loop_iteration_setup().accepted);
        assert(user_consumer.checkpoint().loop_iteration==2 && user_consumer.checkpoint().loop_current_a1==0x2b67e);
        const auto third_d0=user_consumer.checkpoint().loop_d0_value;
        const auto third_d0_source=static_cast<std::uint32_t>(0x2bdfeLL+static_cast<std::int16_t>(third_d0));
        const auto third_indexed_word=read_native_word(third_d0_source);
        assert(user_consumer.observe_d0_indexed_word({1,14,0x2b5de,third_d0,third_d0_source,third_indexed_word}).accepted);
        const auto third_a0_source=static_cast<std::uint32_t>(0x2b0e8LL+static_cast<std::int16_t>(third_indexed_word));
        assert(user_consumer.observe_a0_indexed_word({1,15,0x2b5ec,0,third_a0_source,read_native_word(third_a0_source)}).accepted);
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
            && user_consumer.checkpoint().gemdos_63_buffer==0x2c24a
            && user_consumer.checkpoint().gemdos_63_count==0x7d42);
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
            && single_cell_planes.checkpoint().game_init_palette_outer_backedge_address==0x2b46e
            && single_cell_planes.checkpoint().game_init_next_instruction==0x2b46e);
        assert(!single_cell_planes.observe_game_init_palette_xbios_selector_6({1,101,0x2b4ac,6,0}).accepted);
        for(std::uint64_t pass=0;pass<6;++pass){
            eon::MillenniumAtariGameInitPaletteRecurrenceObservation recurrence{
                1,101U+pass*2U,0x2b46e,0x2b3c8,0x2b428,{},{}};
            for(std::size_t i=0;i<recurrence.source_bytes.size();++i){const auto value=palette_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x2b3c8U+static_cast<std::uint32_t>(i)});assert(value);recurrence.source_bytes[i]=*value;}
            for(std::size_t i=0;i<recurrence.destination_words.size();++i){const auto hi=palette_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x2b428U+static_cast<std::uint32_t>(i*2U)});const auto lo=palette_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x2b429U+static_cast<std::uint32_t>(i*2U)});assert(hi&&lo);recurrence.destination_words[i]=static_cast<std::uint16_t>((*hi<<8U)|*lo);}
            auto bad_recurrence=recurrence;bad_recurrence.instruction_address=0x2b470;
            assert(!single_cell_planes.observe_game_init_palette_recurrence(bad_recurrence).accepted);
            assert(single_cell_planes.observe_game_init_palette_recurrence(recurrence).accepted
                && single_cell_planes.checkpoint().game_init_palette_recurrence_sha256=="a50d1864336da9b76c9594f94b2eb736108d738d0aefc6443a91c8e8fdd7088b"
                && single_cell_planes.checkpoint().game_init_palette_completed_passes==pass+2U);
            assert(palette_memory.apply(single_cell_planes.make_game_init_palette_arithmetic_effect_batch(
                "palette-recurrence-"+std::to_string(pass))).accepted);
            assert(single_cell_planes.observe_game_init_palette_xbios_selector_6(
                {1,102U+pass*2U,0x2b4ac,6,static_cast<std::uint32_t>(0x90000000U+pass)}).accepted);
        }
        assert(single_cell_planes.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_palette_terminal_xbios_selector_6_boundary
            && single_cell_planes.checkpoint().game_init_d7==0xffff
            && single_cell_planes.checkpoint().game_init_palette_terminal_trap_address==0x2b4c2);
        assert(single_cell_planes.observe_game_init_palette_xbios_selector_6({1,113,0x2b4c2,6,0x76543210}).accepted
            && single_cell_planes.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_palette_rts_boundary
            && single_cell_planes.checkpoint().game_init_palette_terminal_result_d0==0x76543210
            && single_cell_planes.checkpoint().game_init_palette_terminal_sha256=="876ea72e7f61e2604ffa34d0fae7a6c1b3f880aa43e88006af18e1f67677c967"
            && single_cell_planes.checkpoint().game_init_palette_rts_address==0x2b4c6);
        assert(!single_cell_planes.observe_game_init_palette_rts({1,114,0x2b4c6,0x00ff0000,0x2ab06}).accepted);
        assert(single_cell_planes.observe_game_init_palette_rts({1,114,0x2b4c6,0x00ff0000,0x2ab04}).accepted
            && single_cell_planes.checkpoint().state==eon::MillenniumAtariConfigConsumerState::jsr_2aa0c_boundary
            && single_cell_planes.checkpoint().game_init_palette_rts_stack_address==0x00ff0000
            && single_cell_planes.checkpoint().game_init_palette_rts_return_address==0x2ab04
            && single_cell_planes.checkpoint().game_init_palette_caller_continuation_sha256=="ae672762da7616abc67d0a1e5a5aaf3ab540b96b94b9689b31f8a11a8de256d7"
            && single_cell_planes.checkpoint().caller_d7==0x2a634
            && single_cell_planes.checkpoint().next_jsr_address==0x2ab0a
            && single_cell_planes.checkpoint().next_jsr_target==0x2aa0c);
        assert(single_cell_planes.execute_jsr_2aa0c().accepted
            && single_cell_planes.checkpoint().state==eon::MillenniumAtariConfigConsumerState::gemdos_selector_61_boundary
            && single_cell_planes.checkpoint().gemdos_filename_pointer==0x2a634
            && single_cell_planes.checkpoint().gemdos_trap_address==0x2a5b4);
        auto second_config_failure=single_cell_planes;
        assert(!single_cell_planes.observe_game_init_second_config_fopen({1,115,0x2a5b6,0x3d,7}).accepted);
        assert(single_cell_planes.observe_game_init_second_config_fopen({1,115,0x2a5b4,0x3d,7}).accepted
            && single_cell_planes.checkpoint().state==eon::MillenniumAtariConfigConsumerState::jsr_2a5c2_boundary
            && single_cell_planes.checkpoint().gemdos_61_result_d0==7
            && single_cell_planes.checkpoint().fopen_branch_target==0x2aa1c
            && single_cell_planes.checkpoint().fopen_positive_d0==0x7d42
            && single_cell_planes.checkpoint().fopen_positive_d1==0x2c24a);
        auto second_config_memory=palette_memory;
        assert(second_config_memory.apply(single_cell_planes.make_gemdos_selector_61_effect_batch("second-config-fopen")).accepted
            && second_config_memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x2a5fb})==7);
        assert(second_config_failure.observe_game_init_second_config_fopen({1,115,0x2a5b4,0x3d,-1}).accepted
            && second_config_failure.checkpoint().state==eon::MillenniumAtariConfigConsumerState::fopen_failure_spin
            && second_config_failure.checkpoint().fopen_branch_target==0x2a632);
        assert(single_cell_planes.execute_jsr_2a5c2().accepted
            && single_cell_planes.checkpoint().state==eon::MillenniumAtariConfigConsumerState::gemdos_selector_63_boundary);
        assert(!single_cell_planes.observe_game_init_second_config_fread({1,116,0x2a5d2,0x3f,0x7d42}).accepted);
        assert(single_cell_planes.observe_game_init_second_config_fread({1,116,0x2a5d0,0x3f,0x7d42}).accepted
            && single_cell_planes.checkpoint().state==eon::MillenniumAtariConfigConsumerState::gemdos_selector_62_boundary
            && single_cell_planes.checkpoint().gemdos_63_result_d0==0x7d42);
        assert(!single_cell_planes.observe_game_init_second_config_fclose({1,117,0x2a5e8,0x3e,0}).accepted);
        assert(single_cell_planes.observe_game_init_second_config_fclose({1,117,0x2a5e6,0x3e,0}).accepted
            && single_cell_planes.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_second_config_rts_boundary
            && single_cell_planes.checkpoint().gemdos_62_result_d0==0
            && single_cell_planes.checkpoint().game_init_second_config_rts_address==0x2a5ec);
        assert(!single_cell_planes.observe_game_init_second_config_rts({1,118,0x2a5ec,0x00fe0000,0x2ab12}).accepted);
        assert(single_cell_planes.observe_game_init_second_config_rts({1,118,0x2a5ec,0x00fe0000,0x2ab10}).accepted
            && single_cell_planes.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_post_second_config_xbios_38_boundary
            && single_cell_planes.checkpoint().game_init_second_config_rts_stack_address==0x00fe0000
            && single_cell_planes.checkpoint().game_init_second_config_rts_return_address==0x2ab10
            && single_cell_planes.checkpoint().game_init_second_config_caller_sha256=="eea2683953b1fe18e3e7b88e1744fa10a9684444fe183d283efee9f54302c1a0"
            && single_cell_planes.checkpoint().game_init_a3==0x2a64c
            && single_cell_planes.checkpoint().game_init_a0==0x2a66c
            && single_cell_planes.checkpoint().game_init_second_config_xbios_pointer==0x2ab2c
            && single_cell_planes.checkpoint().xbios_trap_address==0x2ab24
            && single_cell_planes.checkpoint().xbios_selector==0x26);
        assert(!single_cell_planes.observe_game_init_second_config_xbios_38({1,119,0x2ab26,0x26,0xabcdef01}).accepted);
        assert(single_cell_planes.observe_game_init_second_config_xbios_38({1,119,0x2ab24,0x26,0xabcdef01}).accepted
            && single_cell_planes.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_config_final_rts_boundary
            && single_cell_planes.checkpoint().game_init_second_config_xbios_result_d0==0xabcdef01
            && single_cell_planes.checkpoint().game_init_second_config_xbios_cleanup_bytes==6
            && single_cell_planes.checkpoint().game_init_second_config_xbios_return_sha256=="2b1d33a613d225ccb932ee7c7ad5efb29dcdd736ba28ad3c4b75162694bc09ed"
            && single_cell_planes.checkpoint().game_init_config_final_rts_address==0x2ab28);
        assert(!single_cell_planes.observe_game_init_config_final_rts({1,120,0x2ab28,0x00fd0000,0x77044}).accepted);
        assert(single_cell_planes.observe_game_init_config_final_rts({1,120,0x2ab28,0x00fd0000,0x77042}).accepted
            && single_cell_planes.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_post_config_gemdos_61_boundary
            && single_cell_planes.checkpoint().game_init_config_final_rts_stack_address==0x00fd0000
            && single_cell_planes.checkpoint().game_init_config_final_rts_return_address==0x77042
            && single_cell_planes.checkpoint().game_init_post_config_caller_sha256=="dc2a50400e22fdbe4870f790d4f70c7446caa379dc68281a0445db4ee027fe4d"
            && single_cell_planes.checkpoint().game_init_post_config_preserved_stack_long==0x11e00
            && single_cell_planes.checkpoint().gemdos_trap_address==0x77056
            && single_cell_planes.checkpoint().gemdos_selector==0x3d
            && single_cell_planes.checkpoint().gemdos_open_mode==2
            && single_cell_planes.checkpoint().gemdos_filename_pointer==0x1d6d8);
        auto post_config_failure=single_cell_planes;
        assert(!post_config_failure.observe_game_init_post_config_fopen({1,121,0x77058,0x3d,-1}).accepted);
        assert(post_config_failure.observe_game_init_post_config_fopen({1,121,0x77056,0x3d,-1}).accepted
            && post_config_failure.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_post_config_fopen_failure_spin
            && post_config_failure.checkpoint().game_init_post_config_fopen_result_d0==-1
            && post_config_failure.checkpoint().game_init_post_config_handle_word==0xffff
            && post_config_failure.checkpoint().game_init_post_config_fopen_branch_sha256=="d124b586e52a783689925186d8cc93366870526fd894567b7c55761a617807c7"
            && post_config_failure.checkpoint().game_init_post_config_failure_spin_address==0x77060);
        assert(single_cell_planes.observe_game_init_post_config_fopen({1,121,0x77056,0x3d,7}).accepted
            && single_cell_planes.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_post_config_gemdos_63_boundary
            && single_cell_planes.checkpoint().game_init_post_config_fopen_result_d0==7
            && single_cell_planes.checkpoint().game_init_post_config_handle_word==7
            && single_cell_planes.checkpoint().game_init_post_config_fopen_branch_sha256=="2ceb9e3c6a8c2882f13708d64367b0a9f8bf18ee7456ea396a3e600734825476"
            && single_cell_planes.checkpoint().gemdos_trap_address==0x77074
            && single_cell_planes.checkpoint().gemdos_selector==0x3f
            && single_cell_planes.checkpoint().game_init_post_config_fread_buffer==0x11e00
            && single_cell_planes.checkpoint().game_init_post_config_fread_count==0x20000);
        auto post_config_short_read=single_cell_planes;
        assert(!post_config_short_read.observe_game_init_post_config_fread({1,122,0x77076,0x3f,-1}).accepted);
        assert(post_config_short_read.observe_game_init_post_config_fread({1,122,0x77074,0x3f,-1}).accepted
            && post_config_short_read.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_post_config_gemdos_62_boundary
            && post_config_short_read.checkpoint().game_init_post_config_fread_result_d0==-1
            && post_config_short_read.checkpoint().game_init_post_config_fread_cleanup_bytes==12
            && post_config_short_read.checkpoint().game_init_post_config_fread_return_sha256=="368338a18784d37b5867fa551121703b2fb0ab613db51cbc5b2c08e14f474558"
            && post_config_short_read.checkpoint().gemdos_trap_address==0x7707c
            && post_config_short_read.checkpoint().gemdos_selector==0x3e);
        assert(single_cell_planes.observe_game_init_post_config_fread({1,122,0x77074,0x3f,0x20000}).accepted);
        assert(!single_cell_planes.observe_game_init_post_config_fclose({1,123,0x7707e,0x3e,-1}).accepted);
        assert(single_cell_planes.observe_game_init_post_config_fclose({1,123,0x7707c,0x3e,-1}).accepted
            && single_cell_planes.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_post_config_rts_boundary
            && single_cell_planes.checkpoint().game_init_post_config_fclose_result_d0==-1
            && single_cell_planes.checkpoint().game_init_post_config_fclose_cleanup_bytes==12
            && single_cell_planes.checkpoint().game_init_post_config_fclose_return_sha256=="aa177208872c4125af13601feb4566003e5fb01c851c44f8b7f4904fb5f52b52"
            && single_cell_planes.checkpoint().game_init_post_config_write_addresses[0]==0x2ab2c
            && single_cell_planes.checkpoint().game_init_post_config_write_addresses[1]==0x11dfc
            && single_cell_planes.checkpoint().game_init_post_config_write_value==0x361436a7
            && single_cell_planes.checkpoint().game_init_post_config_rts_address==0x770ba);
        const auto post_config_effects=single_cell_planes.make_game_init_post_config_effect_batch("post-config");
        assert(post_config_effects.fully_admitted && post_config_effects.effects.size()==2
            && post_config_effects.effects[0].location.offset==0x2ab2c
            && post_config_effects.effects[1].location.offset==0x11dfc
            && post_config_effects.effects[0].value==0x361436a7
            && post_config_effects.effects[1].value==0x361436a7);
        assert(!single_cell_planes.observe_game_init_post_config_rts({1,124,0x770b8,0x00fd0000,0x123456}).accepted);
        assert(!single_cell_planes.observe_game_init_post_config_rts({1,124,0x770ba,0x00fd0000,0x123457}).accepted);
        assert(!single_cell_planes.observe_game_init_post_config_rts({1,124,0x770ba,0x00fd0000,0x1000000}).accepted);
        assert(single_cell_planes.observe_game_init_post_config_rts({1,124,0x770ba,0x00fd0000,0x123456}).accepted
            && single_cell_planes.checkpoint().state==eon::MillenniumAtariConfigConsumerState::game_init_post_config_complete
            && single_cell_planes.checkpoint().game_init_post_config_rts_stack_address==0x00fd0000
            && single_cell_planes.checkpoint().game_init_post_config_rts_return_address==0x123456);
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
