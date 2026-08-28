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
}

} // namespace eon
