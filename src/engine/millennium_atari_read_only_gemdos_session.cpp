#include "engine/millennium_atari_read_only_gemdos_session.hpp"

#include "data/sha256.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace eon {

MillenniumAtariReadOnlyGemdosSession::MillenniumAtariReadOnlyGemdosSession(
    const std::uint64_t generation, const Fat12Disk& disk,
    const MillenniumAtariTrapEntry& fopen,
    const MillenniumAtariFreadFramePrefixExecution& fread,
    const MillenniumAtariFreadConfigTransferBoundary& transfer) {
    constexpr std::string_view disk_sha256 =
        "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7";
    constexpr std::string_view payload_sha256 =
        "74d7d630779fd811aedcdbe31b14e54198eb9ffd673df512dd70b6165c4a37b6";
    constexpr std::string_view filename = "MILL22A.inf";
    if (generation == 0 || to_hex(sha256(disk.bytes())) != disk_sha256
        || fopen.fopen_filename != filename || fopen.fopen_function != 0x3d
        || fopen.fopen_access_mode != 2 || fread.function != 0x3f
        || fread.buffer_address != 0x2a500 || fread.byte_count_argument != 0x20000
        || fread.stop_before_trap_offset != 0x2c || transfer.trap_opcode != 0x4e41
        || transfer.config_jsr_opcode != 0x4eb9
        || transfer.config_buffer_address != fread.buffer_address) {
        throw std::runtime_error("Unsupported Millennium Atari ST GEMDOS request");
    }
    const auto* entry = disk.find(filename);
    if (!entry || entry->directory() || entry->size != 7506U) {
        throw std::runtime_error("Millennium Atari ST configuration source is unavailable");
    }
    payload_ = disk.read(*entry);
    if (payload_.size() != entry->size || to_hex(sha256(payload_)) != payload_sha256
        || payload_.size() > fread.byte_count_argument) {
        throw std::runtime_error("Millennium Atari ST configuration source changed");
    }
    checkpoint_ = {generation, MillenniumAtariReadOnlyGemdosState::config_jsr_boundary,
        std::string(disk_sha256), std::string(filename), std::string(payload_sha256),
        static_cast<std::uint32_t>(payload_.size()), fopen.fopen_access_mode, 1,
        fread.byte_count_argument, static_cast<std::uint32_t>(payload_.size()),
        fread.buffer_address, fread.buffer_address, true, false, false};
}

NativeRuntimeEffectBatch MillenniumAtariReadOnlyGemdosSession::make_fread_effect_batch(
    std::string id) const {
    if (checkpoint_.state != MillenniumAtariReadOnlyGemdosState::config_jsr_boundary
        || checkpoint_.generation == 0 || id.empty() || payload_.empty()
        || checkpoint_.fread_return_bytes != payload_.size()
        || checkpoint_.payload_sha256 != to_hex(sha256(payload_))) {
        throw std::runtime_error("Revoked or changed Millennium Atari ST Fread payload");
    }
    NativeRuntimeEffectBatch batch{std::move(id), true, {}};
    batch.effects.reserve(payload_.size());
    for (std::size_t index = 0; index < payload_.size(); ++index) {
        batch.effects.push_back({index + 1,
            {NativeRuntimeAddressSpace::linear, std::nullopt,
                static_cast<std::uint64_t>(checkpoint_.fread_destination_address) + index},
            MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian,
            payload_[index]});
    }
    return batch;
}

MillenniumAtariReadOnlyGemdosResult MillenniumAtariReadOnlyGemdosSession::revoke(
    const std::uint64_t generation) {
    if (checkpoint_.state == MillenniumAtariReadOnlyGemdosState::revoked) {
        return {false, "Millennium Atari ST GEMDOS generation is already revoked"};
    }
    if (generation == 0 || generation != checkpoint_.generation) {
        return {false, "Millennium Atari ST GEMDOS revocation generation is stale"};
    }
    std::fill(payload_.begin(), payload_.end(), 0);
    payload_.clear();
    checkpoint_.state = MillenniumAtariReadOnlyGemdosState::revoked;
    checkpoint_.compatibility_handle = 0;
    return {true, {}};
}

} // namespace eon
