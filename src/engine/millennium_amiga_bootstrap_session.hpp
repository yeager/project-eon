#pragma once

#include "data/millennium_amiga_loader.hpp"

#include <vector>

namespace eon {

// Validates Defjam's original raw Millennium Amiga load plan in memory. It
// records only the source ranges and resident entry, stopping before the
// first transformed stage and before any reconstructed Amiga OS behavior.
class MillenniumAmigaBootstrapSession {
public:
    explicit MillenniumAmigaBootstrapSession(std::vector<std::uint8_t> disk_image);

    [[nodiscard]] const MillenniumAmigaLoadPlan& plan() const { return plan_; }
    [[nodiscard]] const MillenniumAmigaSharedResidentLayout& shared_resident() const {
        return shared_resident_;
    }
    [[nodiscard]] const MillenniumAmigaResidentEntry& resident_entry() const {
        return resident_entry_;
    }

private:
    MillenniumAmigaLoadPlan plan_;
    MillenniumAmigaSharedResidentLayout shared_resident_;
    MillenniumAmigaResidentEntry resident_entry_;
};

} // namespace eon
