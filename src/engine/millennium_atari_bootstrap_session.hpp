#pragma once

#include "data/atari_st_prg.hpp"
#include "data/fat12.hpp"

#include <span>

namespace eon {

// Executes the strictly local Equinox Atari ST bootstrap in memory and stops
// at the first GEMDOS Fopen boundary. No host file handle, D0 result, config
// execution, or display state is invented.
class MillenniumAtariBootstrapSession {
public:
    MillenniumAtariBootstrapSession(const Fat12Disk& disk,
        std::span<const std::uint8_t> program);

    [[nodiscard]] const MillenniumAtariBootstrap& bootstrap() const { return bootstrap_; }
    [[nodiscard]] const MillenniumAtariBssEntry& bss_entry() const { return bss_entry_; }
    [[nodiscard]] const MillenniumAtariBssSource& bss_source() const { return bss_source_; }
    [[nodiscard]] const MillenniumAtariMaterializedTarget& target() const { return target_; }
    [[nodiscard]] const MillenniumAtariTrapEntry& fopen_boundary() const { return trap_; }
    [[nodiscard]] const MillenniumAtariFopenFallthrough& fopen_fallthrough() const {
        return fopen_fallthrough_;
    }
    [[nodiscard]] const MillenniumAtariFreadConfigTransferBoundary& fread_config_transfer() const {
        return fread_config_transfer_;
    }
    [[nodiscard]] const MillenniumAtariRootInventory& root_inventory() const {
        return root_inventory_;
    }
    [[nodiscard]] const MillenniumAtariConfigEvidence& config() const { return config_; }

private:
    MillenniumAtariBootstrap bootstrap_;
    MillenniumAtariBssEntry bss_entry_;
    MillenniumAtariBssSource bss_source_;
    MillenniumAtariMaterializedTarget target_;
    MillenniumAtariTrapEntry trap_;
    MillenniumAtariFopenFallthrough fopen_fallthrough_;
    MillenniumAtariFreadConfigTransferBoundary fread_config_transfer_;
    MillenniumAtariRootInventory root_inventory_;
    MillenniumAtariConfigEvidence config_;
};

} // namespace eon
