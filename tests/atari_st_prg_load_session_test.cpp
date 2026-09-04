#include "data/atari_st_prg.hpp"
#include "data/fat12.hpp"
#include "data/sha256.hpp"
#include "engine/atari_st_prg_load_session.hpp"

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
        const eon::MillenniumAtariPrgLoadSession session(program);
        const auto& exact = session.checkpoint();
        assert(exact.entry_address == 0x10000 && exact.image.size() == 130392);
        assert(exact.relocation_effects.size() == 227);
        assert(exact.materialized_image_sha256
            == "92eac35edb2b5db721dd5353cfc3260dfb5fb4120026b76788659aaa342f887c");
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
