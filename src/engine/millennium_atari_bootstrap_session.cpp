#include "engine/millennium_atari_bootstrap_session.hpp"

#include "data/sha256.hpp"

#include <stdexcept>
#include <string_view>
#include <utility>

namespace eon {

MillenniumAtariBootstrapSession::MillenniumAtariBootstrapSession(
    const Fat12Disk& disk, const std::span<const std::uint8_t> program)
    : prg_load_(program) {
    constexpr std::string_view equinox_disk_sha256 =
        "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7";
    constexpr std::string_view equinox_program_sha256 =
        "4584ddc459e3bf03e642f3156fbedb74aa33a847db4937beb5635eb492e93686";
    if (to_hex(sha256(disk.bytes())) != equinox_disk_sha256
        || to_hex(sha256(program)) != equinox_program_sha256) {
        throw std::runtime_error("Unsupported Millennium Atari ST Equinox media");
    }
    const auto prg = parse_atari_st_prg(program);
    bootstrap_ = parse_millennium_atari_bootstrap(program, prg);
    bss_entry_ = parse_millennium_atari_bss_entry(program, prg, bootstrap_);
    bss_source_ = materialize_millennium_atari_bss_source(program, prg, bootstrap_, bss_entry_);
    execution_ = execute_millennium_atari_bootstrap_prefix(program, prg, bootstrap_, bss_entry_);
    target_ = execution_.target;
    post_config_filename_ = parse_millennium_atari_post_config_filename(
        program, bss_source_, target_);
    trap_ = parse_millennium_atari_trap_entry(bss_source_, target_);
    fopen_result_gate_ = execute_millennium_atari_fopen_result_gate(target_, trap_);
    fopen_fallthrough_ = parse_millennium_atari_fopen_fallthrough(target_, trap_);
    fread_frame_prefix_ = execute_millennium_atari_fread_frame_prefix(target_, fopen_fallthrough_);
    fread_config_transfer_ = parse_millennium_atari_fread_config_transfer_boundary(
        target_, fopen_fallthrough_);
    root_inventory_ = inventory_millennium_atari_equinox_root(disk);
    config_ = probe_millennium_atari_config(disk);
    constexpr std::string_view expected_hash =
        "74d7d630779fd811aedcdbe31b14e54198eb9ffd673df512dd70b6165c4a37b6";
    if (trap_.fopen_filename != config_.requested_filename || !config_.present
        || config_.size != 7506 || config_.sha256 != expected_hash) {
        throw std::runtime_error("Unsupported Millennium Atari ST Fopen boundary");
    }
    // FAT12 supplies immutable source evidence for the name named by Fopen.
    // Keep the loader/config disagreement explicit: this read does not stand
    // in for the native Fread buffer, and the session still ends before TRAP #1.
    const auto* entry = disk.find(config_.requested_filename);
    if (!entry || entry->directory()) {
        throw std::runtime_error("Unsupported Millennium Atari ST configuration source");
    }
    const auto payload = disk.read(*entry);
    config_entry_ = parse_millennium_atari_config_entry(payload);
    fread_config_load_address_boundary_ = parse_millennium_atari_fread_config_load_address_boundary(
        fread_config_transfer_, payload, config_entry_);
    fread_mapped_config_prelude_ = parse_millennium_atari_fread_mapped_config_prelude(
        fread_config_transfer_, payload, config_entry_);
    // One coordinator-owned Atari session is one compatibility generation.
    // RuntimeHost revokes the entire coordinator before any source switch.
    read_only_gemdos_.emplace(1, disk, trap_, fread_frame_prefix_,
        fread_config_transfer_);

    constexpr std::string_view post_config_sha256 =
        "e315b0ec01f2fe429fdce101765577b893d031389c540de1fbe43eca121d53e9";
    const auto* post_config_entry = disk.find(post_config_filename_.filename);
    if (!post_config_entry || post_config_entry->directory()
        || post_config_entry->size != 84720U) {
        throw std::runtime_error("Unsupported Millennium Atari ST post-config source");
    }
    post_config_payload_ = disk.read(*post_config_entry);
    if (post_config_payload_.size() != 84720U
        || to_hex(sha256(post_config_payload_)) != post_config_sha256) {
        throw std::runtime_error("Millennium Atari ST post-config source changed");
    }
    post_config_module_entry_ = parse_millennium_atari_post_config_module_entry(
        post_config_payload_);
}

NativeRuntimeEffectBatch MillenniumAtariBootstrapSession::make_post_config_fread_effect_batch(
    const std::int32_t returned_bytes, std::string id) const {
    if (id.empty() || post_config_payload_.size() != 84720U
        || returned_bytes > static_cast<std::int32_t>(post_config_payload_.size())
        || to_hex(sha256(post_config_payload_))
            != "e315b0ec01f2fe429fdce101765577b893d031389c540de1fbe43eca121d53e9") {
        throw std::runtime_error("Unsupported Millennium Atari ST post-config Fread result");
    }
    const auto byte_count = returned_bytes > 0
        ? static_cast<std::size_t>(returned_bytes) : 0U;
    NativeRuntimeEffectBatch batch{std::move(id), true, {}};
    batch.effects.reserve(byte_count);
    for (std::size_t index = 0; index < byte_count; ++index) {
        batch.effects.push_back({index + 1,
            {NativeRuntimeAddressSpace::linear, std::nullopt, 0x11e00ULL + index},
            MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian,
            post_config_payload_[index]});
    }
    return batch;
}

} // namespace eon
