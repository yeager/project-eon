#include "engine/millennium_atari_bootstrap_session.hpp"

#include <stdexcept>

namespace eon {

MillenniumAtariBootstrapSession::MillenniumAtariBootstrapSession(
    const Fat12Disk& disk, const std::span<const std::uint8_t> program) {
    const auto prg = parse_atari_st_prg(program);
    bootstrap_ = parse_millennium_atari_bootstrap(program, prg);
    bss_entry_ = parse_millennium_atari_bss_entry(program, prg, bootstrap_);
    bss_source_ = materialize_millennium_atari_bss_source(program, prg, bootstrap_, bss_entry_);
    target_ = materialize_millennium_atari_target(bss_source_, bss_entry_);
    trap_ = parse_millennium_atari_trap_entry(bss_source_, target_);
    fopen_fallthrough_ = parse_millennium_atari_fopen_fallthrough(target_, trap_);
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
}

} // namespace eon
