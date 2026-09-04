#include "engine/deuteros_amiga_title_bootstrap_session.hpp"

#include "data/sha256.hpp"

#include <stdexcept>

namespace eon {
namespace {

std::uint16_t big16(const std::span<const std::uint8_t> bytes, const std::size_t offset) {
    if (offset > bytes.size() || 2 > bytes.size() - offset) {
        throw std::runtime_error("Deuteros title bootstrap word outside source");
    }
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(bytes[offset]) << 8U) | bytes[offset + 1]);
}

std::uint32_t big32(const std::span<const std::uint8_t> bytes, const std::size_t offset) {
    return (static_cast<std::uint32_t>(big16(bytes, offset)) << 16U)
        | big16(bytes, offset + 2U);
}

} // namespace

DeuterosAmigaTitleBootstrapSession::DeuterosAmigaTitleBootstrapSession(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    const DeuterosAmigaTitleHandoffRoute& route)
    : disk_(&disk), plan_(&plan) {
    constexpr std::string_view clean_adf_sha256 =
        "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38";
    if (to_hex(sha256(disk.bytes(0, AmigaAdf::standard_size))) != clean_adf_sha256
        || route.bootstrap_profile_return_cell != plan.main_stage_entry.bootstrap_profile_return_cell
        || route.bootstrap_profile_return_cell != 0x12ffc
        || route.bootstrap_profile_value != 1) {
        throw std::runtime_error("Unexpected Deuteros title bootstrap provenance");
    }
    checkpoint_.profile_return_cell = route.bootstrap_profile_return_cell;
    checkpoint_.profile_value = route.bootstrap_profile_value;
    checkpoint_.profile_table_address = 0x12a36;
    checkpoint_.profile_routine_address = 0x12b30;
    checkpoint_.stage_disk_offset = plan.title_handoff_profile.disk_offset;
    checkpoint_.stage_length = plan.title_handoff_profile.length;
    checkpoint_.stage_destination = plan.title_handoff_profile.destination;
    checkpoint_.entry_address = plan.title_stage.entry_address;
}

bool DeuterosAmigaTitleBootstrapSession::advance() {
    if (!disk_ || !plan_ || complete()) return false;
    switch (checkpoint_.state) {
    case DeuterosAmigaTitleBootstrapState::profile_return_observed: {
        const auto table_offset = plan_->bootstrap_loader.disk_offset
            + checkpoint_.profile_table_address - plan_->bootstrap_loader.destination;
        const auto table = disk_->bytes(table_offset, 8);
        if (big32(table, 4) != checkpoint_.profile_routine_address) {
            throw std::runtime_error("Unexpected Deuteros title bootstrap table entry");
        }
        const auto routine_offset = plan_->bootstrap_loader.disk_offset
            + checkpoint_.profile_routine_address - plan_->bootstrap_loader.destination;
        const auto routine = disk_->bytes(routine_offset, 20);
        if (big16(routine, 0) != 0x223c
            || big32(routine, 2) != checkpoint_.stage_destination
            || big16(routine, 6) != 0x203c
            || big32(routine, 8) != checkpoint_.stage_length
            || big16(routine, 12) != 0x243c
            || big32(routine, 14) * (AmigaAdf::sector_size * AmigaAdf::sectors_per_track)
                != checkpoint_.stage_disk_offset
            || big16(routine, 18) != 0x4e75) {
            throw std::runtime_error("Unexpected Deuteros title bootstrap profile routine");
        }
        checkpoint_.state = DeuterosAmigaTitleBootstrapState::profile_selected;
        return true;
    }
    case DeuterosAmigaTitleBootstrapState::profile_selected: {
        const auto stage = disk_->bytes(checkpoint_.stage_disk_offset, checkpoint_.stage_length);
        checkpoint_.stage_sha256 = to_hex(sha256(stage));
        constexpr std::string_view expected_stage_sha256 =
            "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03";
        if (checkpoint_.stage_sha256 != expected_stage_sha256) {
            throw std::runtime_error("Unexpected Deuteros title bootstrap stage");
        }
        checkpoint_.state = DeuterosAmigaTitleBootstrapState::stage_transfer_validated;
        return true;
    }
    case DeuterosAmigaTitleBootstrapState::stage_transfer_validated: {
        const auto stage = disk_->bytes(checkpoint_.stage_disk_offset, checkpoint_.stage_length);
        if (big16(stage, 0) != 0x4ef9 || big32(stage, 2) != checkpoint_.entry_address
            || checkpoint_.entry_address < checkpoint_.stage_destination
            || checkpoint_.entry_address - checkpoint_.stage_destination
                >= checkpoint_.stage_length) {
            throw std::runtime_error("Unexpected Deuteros title bootstrap entry dispatch");
        }
        checkpoint_.state = DeuterosAmigaTitleBootstrapState::title_entry_dispatched;
        return true;
    }
    case DeuterosAmigaTitleBootstrapState::title_entry_dispatched:
        return false;
    }
    return false;
}

} // namespace eon
