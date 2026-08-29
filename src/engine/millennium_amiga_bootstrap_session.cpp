#include "engine/millennium_amiga_bootstrap_session.hpp"

#include <stdexcept>

namespace eon {

MillenniumAmigaBootstrapSession::MillenniumAmigaBootstrapSession(
    std::vector<std::uint8_t> disk_image) {
    const AmigaAdf disk(std::move(disk_image));
    plan_ = parse_millennium_amiga_load_plan(disk);
    shared_resident_ = parse_millennium_amiga_shared_resident_layout(disk.bytes(0,
        AmigaAdf::standard_size));
    if (plan_.resident_stage.disk_offset != shared_resident_.disk_offset
        || plan_.resident_stage.length != shared_resident_.length
        || plan_.resident_stage.destination != shared_resident_.destination
        || plan_.resident_stage.raw_sha256 != shared_resident_.raw_sha256) {
        throw std::runtime_error("Millennium Amiga raw resident plan differs from shared evidence");
    }
    resident_entry_ = parse_millennium_amiga_resident_entry(disk, plan_);
    opaque_invocation_boundary_ =
        parse_millennium_amiga_bootstrap_opaque_invocation_boundary(disk, plan_);
    first_stage_source_anchors_ =
        parse_millennium_amiga_first_stage_source_anchor_boundary(disk, plan_);

    // Keep the diagnostic boundary joined to the same session-owned plan.
    // This guards against a future caller validating one plan but reporting
    // a continuation/anchor record derived from another image or range.
    if (opaque_invocation_boundary_.first_stage_target != plan_.first_stage.destination
        || opaque_invocation_boundary_.resident_stage_target != plan_.resident_entry
        || first_stage_source_anchors_.raw_disk_offset != plan_.first_stage.disk_offset
        || first_stage_source_anchors_.byte_count != plan_.first_stage.length
        || first_stage_source_anchors_.sha256 != plan_.first_stage.raw_sha256) {
        throw std::runtime_error("Millennium Amiga bootstrap evidence is detached from load plan");
    }
}

} // namespace eon
