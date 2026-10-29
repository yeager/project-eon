#include "data/fat12.hpp"
#include "data/sha256.hpp"
#include "engine/millennium_atari_save_load_session.hpp"

#include <cassert>
#include <array>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

int main(const int argc, const char* const argv[]) {
    if (argc != 2 || std::string_view(argv[1]) != "--stdin-exact-save-disks") return 2;
    const std::vector<std::uint8_t> bytes{
        std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>()};
    constexpr std::size_t save_disk_size = 368'640;
    constexpr std::size_t equinox_disk_size = 819'200;
    assert(bytes.size() == save_disk_size + equinox_disk_size);
    const eon::Fat12Disk save_disk(std::span<const std::uint8_t>(bytes).first(save_disk_size));
    const eon::Fat12Disk equinox_disk(std::span<const std::uint8_t>(bytes).subspan(save_disk_size));
    for (std::uint8_t slot = 0; slot < 3; ++slot) {
        const eon::MillenniumAtariSaveLoadSession session(1, save_disk, equinox_disk, slot);
        const auto& checkpoint = session.checkpoint();
        const auto* entry = save_disk.find(checkpoint.filename);
        assert(entry && !entry->directory());
        const auto payload = save_disk.read(*entry);
        assert(checkpoint.generation == 1 && checkpoint.slot == slot
            && checkpoint.source_opened_read_only && !checkpoint.source_mutated
            && checkpoint.payload_bytes == 7313 && checkpoint.copied_source_bytes == 7313
            && checkpoint.payload_sha256 == eon::to_hex(eon::sha256(payload)));
        const auto batch = session.make_effect_batch("atari-save-test-" + std::to_string(slot));
        assert(batch.fully_admitted && batch.effects.size() == 7313);
        // Independently expand the consumer's bounded DBRA transfer groups.
        // Check every byte, not a handful of landmarks, so an omitted,
        // duplicated, reordered, or misaddressed save field cannot pass.
        std::vector<std::pair<std::size_t, std::uint32_t>> expected;
        expected.reserve(7313);
        const auto append_range = [&](const std::size_t source,
            const std::size_t length, const std::uint32_t destination) {
            for (std::size_t index = 0; index < length; ++index)
                expected.emplace_back(source + index,
                    destination + static_cast<std::uint32_t>(index));
        };
        append_range(0, 344, 0x122cc);
        append_range(344, 4, 0x19330);
        append_range(348, 2640, 0x268d6);
        append_range(2988, 4024, 0x12438);
        append_range(7012, 6, 0x13b3c);
        for (std::size_t index = 0; index < 7; ++index)
            append_range(7018 + index * 2, 2,
                0x13448U + static_cast<std::uint32_t>(index * 6));
        for (std::size_t index = 0; index < 61; ++index)
            append_range(7032 + index * 2, 2,
                0x1347aU + static_cast<std::uint32_t>(index * 0x1c));
        for (std::size_t index = 0; index < 38; ++index) {
            const auto source = 7154 + index * 4;
            const auto destination = 0x15174U + static_cast<std::uint32_t>(index * 0x0c);
            append_range(source, 2, destination);
            expected.emplace_back(source + 3, destination + 8U);
            expected.emplace_back(source + 2, destination + 6U);
        }
        constexpr std::array<std::uint32_t, 7> tail_destinations{
            0x13731, 0x151a0, 0x151f4, 0x15200, 0x13769, 0x13785, 0x137a1};
        for (std::size_t index = 0; index < tail_destinations.size(); ++index)
            expected.emplace_back(7306 + index, tail_destinations[index]);
        assert(expected.size() == batch.effects.size());
        std::vector<bool> consumed(payload.size(), false);
        for (std::size_t index = 0; index < expected.size(); ++index) {
            const auto [source, destination] = expected[index];
            const auto& effect = batch.effects[index];
            assert(source < payload.size() && !consumed[source]);
            consumed[source] = true;
            assert(effect.order == index + 1
                && effect.location.address_space == eon::NativeRuntimeAddressSpace::linear
                && !effect.location.segment && effect.location.offset == destination
                && effect.width == eon::MemoryTransferElementWidth::byte
                && effect.byte_order == eon::NativeRuntimeByteOrder::big_endian
                && effect.value == payload[source]);
        }
        for (const auto was_consumed : consumed) assert(was_consumed);
    }
    bool rejected_truncated_slot = false;
    try {
        static_cast<void>(eon::MillenniumAtariSaveLoadSession(1, save_disk, equinox_disk, 3));
    } catch (const std::runtime_error&) {
        rejected_truncated_slot = true;
    }
    assert(rejected_truncated_slot);
}
