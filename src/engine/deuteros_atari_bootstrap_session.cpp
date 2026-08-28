#include "engine/deuteros_atari_bootstrap_session.hpp"

#include "data/sha256.hpp"

#include <stdexcept>

namespace eon {

DeuterosAtariBootstrapSession::DeuterosAtariBootstrapSession(
    std::vector<std::uint8_t> disk_image) {
    DeuterosAtariDisk disk(std::move(disk_image));
    boot_ = disk.boot_profile();
    if (!boot_.has_recovered_first_stage) {
        throw std::runtime_error("Unsupported Deuteros Atari ST boot-stage path");
    }
    const auto first_stage_bytes = disk.read_sectors(boot_.first_stage_track, boot_.first_stage_side,
        boot_.first_stage_sector, boot_.first_stage_sector_count);
    first_stage_sha256_ = to_hex(sha256(first_stage_bytes));
    constexpr std::string_view expected_first_stage_sha256 =
        "dad3594c53375bd8285ef33e2d685bd38a5b38d930f2ea1305d117d63667f168";
    if (first_stage_sha256_ != expected_first_stage_sha256) {
        throw std::runtime_error("Unsupported Deuteros Atari ST first raw stage");
    }
    first_stage_ = parse_deuteros_atari_first_stage(first_stage_bytes);
    if (calculate_deuteros_atari_first_stage_checksum(first_stage_bytes, first_stage_)
        != first_stage_.checksum_expected) {
        throw std::runtime_error("Invalid Deuteros Atari ST first raw-stage checksum");
    }
    const auto second_stage_bytes = disk.read_sectors(first_stage_.next_track, first_stage_.next_side,
        first_stage_.next_sector, first_stage_.next_sector_count);
    second_stage_sha256_ = to_hex(sha256(second_stage_bytes));
    constexpr std::string_view expected_second_stage_sha256 =
        "2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7";
    if (second_stage_sha256_ != expected_second_stage_sha256) {
        throw std::runtime_error("Unsupported Deuteros Atari ST second raw stage");
    }
    second_stage_ = parse_deuteros_atari_second_stage(second_stage_bytes);
    dispatch_ = parse_deuteros_atari_dispatch(second_stage_bytes);
    // These are byte-validated static argument paths reached after the
    // second-stage hand-off. Retaining them in the live session prevents the
    // launch path from being less evidence-backed than --inspect, while still
    // not selecting a runtime vector, issuing a raw read, or calling XBIOS.
    state0_raw_load_plan_ = build_deuteros_atari_state0_raw_load_plan(second_stage_, dispatch_);
    state1_raw_load_plan_ = build_deuteros_atari_state1_raw_load_plan(second_stage_, dispatch_);
    state5_raw_load_plan_ = build_deuteros_atari_state5_raw_load_plan(second_stage_, dispatch_);
    state5_return_ = parse_deuteros_atari_state5_return(second_stage_bytes, second_stage_, dispatch_);
    supervisor_callback_ = parse_deuteros_atari_supervisor_callback(second_stage_bytes, second_stage_);
}

} // namespace eon
