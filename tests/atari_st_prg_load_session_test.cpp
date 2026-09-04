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
