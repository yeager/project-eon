#include "engine/millennium_amiga_bootstrap_session.hpp"

#include "data/sha256.hpp"

#include <stdexcept>

namespace eon {
namespace {

std::vector<std::uint8_t> require_defjam_millennium_adf(
    std::vector<std::uint8_t> disk_image) {
    constexpr std::string_view defjam_millennium_adf_sha256 =
        "8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c";
    if (to_hex(sha256(disk_image)) != defjam_millennium_adf_sha256) {
        throw std::runtime_error("Unsupported Millennium Amiga Defjam ADF");
    }
    return disk_image;
}

} // namespace

MillenniumAmigaBootstrapSession::MillenniumAmigaBootstrapSession(
    std::vector<std::uint8_t> disk_image) {
    const AmigaAdf disk(require_defjam_millennium_adf(std::move(disk_image)));
    plan_ = parse_millennium_amiga_load_plan(disk);
    opaque_invocation_boundary_ =
        parse_millennium_amiga_bootstrap_opaque_invocation_boundary(disk, plan_);
    first_stage_entry_boundary_ =
        parse_millennium_amiga_first_stage_entry_boundary(disk, plan_);
    relocation_boundary_ = parse_millennium_amiga_bootstrap_relocation_boundary(disk, plan_);
    if (opaque_invocation_boundary_.first_stage_target != plan_.first_stage.destination
        || opaque_invocation_boundary_.resident_stage_target != plan_.resident_entry
        || first_stage_entry_boundary_.destination != plan_.first_stage.destination
        || relocation_boundary_.raw_continuation_source_address
            != opaque_invocation_boundary_.entry_address) {
        throw std::runtime_error("Millennium Amiga bootstrap evidence is detached from load plan");
    }
}

} // namespace eon
