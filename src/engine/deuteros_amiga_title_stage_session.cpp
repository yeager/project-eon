#include "engine/deuteros_amiga_title_stage_session.hpp"

#include "data/sha256.hpp"

#include <stdexcept>

namespace eon {

DeuterosAmigaTitleStageSession::DeuterosAmigaTitleStageSession(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan)
    : disk_(&disk), stage_(plan.title_stage), profile_(parse_deuteros_amiga_title_stage(disk, plan)) {
    if (stage_.length == 0 || stage_.disk_offset > AmigaAdf::standard_size
        || stage_.length > AmigaAdf::standard_size - stage_.disk_offset
        || stage_.entry_address < stage_.destination
        || stage_.entry_address - stage_.destination >= stage_.length) {
        throw std::runtime_error("Invalid Deuteros Amiga title-stage provenance");
    }
    original_sha256_ = to_hex(sha256(original_bytes()));
}

std::span<const std::uint8_t> DeuterosAmigaTitleStageSession::original_bytes() const {
    if (!disk_) throw std::runtime_error("Missing Deuteros Amiga title-stage source disk");
    return disk_->bytes(stage_.disk_offset, stage_.length);
}

} // namespace eon
