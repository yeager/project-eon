#include "engine/millennium_atari_save_load_session.hpp"

#include "data/sha256.hpp"

#include <array>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace eon {
namespace {

void append_bytes(NativeRuntimeEffectBatch& batch,
    const std::span<const std::uint8_t> payload, const std::size_t source,
    const std::uint32_t destination) {
    if (source > payload.size() || payload.size() - source < 1U)
        throw std::runtime_error("Millennium Atari save source range is truncated");
    batch.effects.push_back({batch.effects.size() + 1U,
        {NativeRuntimeAddressSpace::linear, std::nullopt,
            static_cast<std::uint64_t>(destination)},
        MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian,
        payload[source]});
}

void append_range(NativeRuntimeEffectBatch& batch,
    const std::span<const std::uint8_t> payload, const std::size_t source,
    const std::size_t length, const std::uint32_t destination) {
    if (source > payload.size() || length > payload.size() - source)
        throw std::runtime_error("Millennium Atari save source range is truncated");
    for (std::size_t index = 0; index < length; ++index)
        append_bytes(batch, payload, source + index,
            destination + static_cast<std::uint32_t>(index));
}

} // namespace

MillenniumAtariSaveLoadSession::MillenniumAtariSaveLoadSession(
    const std::uint64_t generation, const Fat12Disk& save_disk,
    const Fat12Disk& equinox_disk, const std::uint8_t slot) {
    constexpr std::string_view save_disk_sha256 =
        "16072aa944d08388f5341d29b164e69d6d16ba516142bbf5b9bf1d4da78330a0";
    constexpr std::string_view equinox_disk_sha256 =
        "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7";
    constexpr std::string_view module_sha256 =
        "e315b0ec01f2fe429fdce101765577b893d031389c540de1fbe43eca121d53e9";
    constexpr std::array<std::string_view, 3> filenames{
        "2200SAVE.I", "2200SAVE.II", "2200SAVE.III"};
    constexpr std::array<std::string_view, 3> payload_hashes{
        "d94e075d228949f325cd9c3a13da7d5158f8ae5820a4cb320381645cb031634b",
        "4164255267a6553e748db821e56c616b0935aadf2d4bee8c1dce426558a6b715",
        "7948a169acdeb3abfa4bebdc7f1cde1431c17328b3cba081519374b29d523e45"};
    constexpr std::string_view slot_table_sha256 =
        "27779e6f71c57806c653873b03b63e0279e5dfaa88614915ad6b1c870a54140b";
    constexpr std::string_view reader_sha256 =
        "3ba177dc5981cc6f5ea6c95266be0c514c227478834b6e5cac2feae7f0798c2f";
    constexpr std::string_view consumer_sha256 =
        "182389db54b7cf1fab678da226937906352e131b3b4d227f57e7522c67ebe358";
    if (generation == 0 || slot >= filenames.size()
        || to_hex(sha256(save_disk.bytes())) != save_disk_sha256
        || to_hex(sha256(equinox_disk.bytes())) != equinox_disk_sha256) {
        throw std::runtime_error("Unsupported Millennium Atari save-media identity");
    }
    const auto* module_entry = equinox_disk.find("MILL22B.INF");
    if (!module_entry || module_entry->directory() || module_entry->size != 84720U)
        throw std::runtime_error("Millennium Atari save consumer source is unavailable");
    const auto module = equinox_disk.read(*module_entry);
    if (to_hex(sha256(module)) != module_sha256
        || to_hex(sha256(std::span<const std::uint8_t>(module).subspan(0xa318, 56)))
            != slot_table_sha256
        || to_hex(sha256(std::span<const std::uint8_t>(module).subspan(0xa542, 74)))
            != reader_sha256
        || to_hex(sha256(std::span<const std::uint8_t>(module).subspan(0xa592, 284)))
            != consumer_sha256) {
        throw std::runtime_error("Millennium Atari save consumer source changed");
    }
    const auto* entry = save_disk.find(filenames[slot]);
    if (!entry || entry->directory() || entry->size != 7313U)
        throw std::runtime_error("Millennium Atari save is truncated or unavailable");
    payload_ = save_disk.read(*entry);
    const auto payload_sha256 = to_hex(sha256(payload_));
    if (payload_.size() != 7313U || payload_sha256 != payload_hashes[slot]
        || payload_[0x155] != 0x81U) {
        throw std::runtime_error("Millennium Atari save differs from its admitted source");
    }
    checkpoint_ = {generation, slot, std::string(save_disk_sha256),
        std::string(filenames[slot]), payload_sha256,
        static_cast<std::uint32_t>(payload_.size()), 7313U, true, false};
}

NativeRuntimeEffectBatch MillenniumAtariSaveLoadSession::make_effect_batch(
    std::string id) const {
    if (checkpoint_.generation == 0 || id.empty() || payload_.size() != 7313U
        || checkpoint_.copied_source_bytes != 7313U
        || payload_[0x155] != 0x81U
        || to_hex(sha256(payload_)) != checkpoint_.payload_sha256) {
        throw std::runtime_error("Revoked or changed Millennium Atari save payload");
    }
    NativeRuntimeEffectBatch batch{std::move(id), true, {}};
    batch.effects.reserve(7313U);
    const auto source = std::span<const std::uint8_t>(payload_);
    append_range(batch, source, 0, 344, 0x122cc);
    append_range(batch, source, 344, 4, 0x19330);
    append_range(batch, source, 348, 2640, 0x268d6);
    append_range(batch, source, 2988, 4024, 0x12438);
    append_range(batch, source, 7012, 6, 0x13b3c);

    for (std::size_t index = 0; index < 7; ++index)
        append_range(batch, source, 7018 + index * 2, 2,
            0x13448U + static_cast<std::uint32_t>(index * 6));
    for (std::size_t index = 0; index < 61; ++index)
        append_range(batch, source, 7032 + index * 2, 2,
            0x1347aU + static_cast<std::uint32_t>(index * 0x1c));
    for (std::size_t index = 0; index < 38; ++index) {
        const auto source_offset = 7154 + index * 4;
        const auto destination = 0x15174U + static_cast<std::uint32_t>(index * 0x0c);
        append_range(batch, source, source_offset, 2, destination);
        append_bytes(batch, source, source_offset + 3, destination + 8U);
        append_bytes(batch, source, source_offset + 2, destination + 6U);
    }
    constexpr std::array<std::uint32_t, 7> tail_destinations{
        0x13731, 0x151a0, 0x151f4, 0x15200, 0x13769, 0x13785, 0x137a1};
    for (std::size_t index = 0; index < tail_destinations.size(); ++index)
        append_bytes(batch, source, 7306 + index, tail_destinations[index]);
    if (batch.effects.size() != 7313U)
        throw std::runtime_error("Millennium Atari save copy plan length mismatch");
    return batch;
}

} // namespace eon
