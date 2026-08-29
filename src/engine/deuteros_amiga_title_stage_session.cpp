#include "engine/deuteros_amiga_title_stage_session.hpp"

#include "data/sha256.hpp"

#include <stdexcept>
#include <string_view>

namespace eon {

DeuterosAmigaTitleStageSession::DeuterosAmigaTitleStageSession(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    const std::uint16_t incoming_profile)
    : disk_(&disk), stage_(plan.title_stage), profile_(parse_deuteros_amiga_title_stage(disk, plan)) {
    if (stage_.length == 0 || stage_.disk_offset > AmigaAdf::standard_size
        || stage_.length > AmigaAdf::standard_size - stage_.disk_offset
        || stage_.entry_address < stage_.destination
        || stage_.entry_address - stage_.destination >= stage_.length) {
        throw std::runtime_error("Invalid Deuteros Amiga title-stage provenance");
    }
    original_sha256_ = to_hex(sha256(original_bytes()));
    constexpr std::string_view clean_title_stage_sha256 =
        "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03";
    if (original_sha256_ != clean_title_stage_sha256) {
        throw std::runtime_error("Unexpected Deuteros Amiga title-stage media");
    }
    // Keep the caller-proven bootstrap profile joined to the exact loaded
    // stage. The helper models only local writes and stops before Exec.
    entry_prefix_ = execute_deuteros_amiga_title_entry_prefix(disk, plan, incoming_profile);
}

std::span<const std::uint8_t> DeuterosAmigaTitleStageSession::original_bytes() const {
    if (!disk_) throw std::runtime_error("Missing Deuteros Amiga title-stage source disk");
    return disk_->bytes(stage_.disk_offset, stage_.length);
}

} // namespace eon
