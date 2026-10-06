#pragma once

#include "data/atari_st_prg.hpp"
#include "data/fat12.hpp"
#include "engine/atari_st_prg_load_session.hpp"
#include "engine/millennium_atari_read_only_gemdos_session.hpp"

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace eon {

// Executes the strictly local Equinox Atari ST bootstrap in memory and stops
// at the first GEMDOS Fopen boundary. No host file handle, D0 result, config
// execution, or display state is invented.  The session also retains the
// separately read, hash-verified requested-config metadata and static address
// disagreement, but never presents those source bytes as a Fread result.
class MillenniumAtariBootstrapSession {
public:
    MillenniumAtariBootstrapSession(const Fat12Disk& disk,
        std::span<const std::uint8_t> program);

    [[nodiscard]] const MillenniumAtariBootstrap& bootstrap() const { return bootstrap_; }
    [[nodiscard]] const AtariStPrgLoadCheckpoint& native_prg_image() const {
        return prg_load_.checkpoint();
    }
    [[nodiscard]] const MillenniumAtariBssEntry& bss_entry() const { return bss_entry_; }
    [[nodiscard]] const MillenniumAtariBssSource& bss_source() const { return bss_source_; }
    [[nodiscard]] const MillenniumAtariMaterializedTarget& target() const { return target_; }
    [[nodiscard]] const MillenniumAtariBootstrapExecution& execution() const { return execution_; }
    [[nodiscard]] const MillenniumAtariPostConfigFilenameEvidence& post_config_filename() const {
        return post_config_filename_;
    }
    [[nodiscard]] const MillenniumAtariPostConfigModuleEntryEvidence& post_config_module_entry() const {
        return post_config_module_entry_;
    }
    [[nodiscard]] std::span<const std::uint8_t> post_config_payload() const {
        return post_config_payload_;
    }
    [[nodiscard]] const MillenniumAtariTrapEntry& fopen_boundary() const { return trap_; }
    [[nodiscard]] const MillenniumAtariFopenResultGateExecution& fopen_result_gate() const {
        return fopen_result_gate_;
    }
    [[nodiscard]] const MillenniumAtariFopenFallthrough& fopen_fallthrough() const {
        return fopen_fallthrough_;
    }
    [[nodiscard]] const MillenniumAtariFreadFramePrefixExecution& fread_frame_prefix() const {
        return fread_frame_prefix_;
    }
    [[nodiscard]] const MillenniumAtariFreadConfigTransferBoundary& fread_config_transfer() const {
        return fread_config_transfer_;
    }
    [[nodiscard]] const MillenniumAtariRootInventory& root_inventory() const {
        return root_inventory_;
    }
    [[nodiscard]] const MillenniumAtariConfigEvidence& config() const { return config_; }
    [[nodiscard]] const MillenniumAtariConfigEntry& config_entry() const {
        return config_entry_;
    }
    [[nodiscard]] const MillenniumAtariFreadConfigLoadAddressBoundary&
    fread_config_load_address_boundary() const {
        return fread_config_load_address_boundary_;
    }
    [[nodiscard]] const MillenniumAtariFreadMappedConfigPrelude&
    fread_mapped_config_prelude() const {
        return fread_mapped_config_prelude_;
    }
    [[nodiscard]] const MillenniumAtariReadOnlyGemdosSession& read_only_gemdos() const {
        return *read_only_gemdos_;
    }
    [[nodiscard]] NativeRuntimeEffectBatch make_post_config_fread_effect_batch(
        std::int32_t returned_bytes, std::string id) const;

private:
    MillenniumAtariPrgLoadSession prg_load_;
    MillenniumAtariBootstrap bootstrap_;
    MillenniumAtariBssEntry bss_entry_;
    MillenniumAtariBssSource bss_source_;
    MillenniumAtariMaterializedTarget target_;
    MillenniumAtariPostConfigFilenameEvidence post_config_filename_;
    MillenniumAtariPostConfigModuleEntryEvidence post_config_module_entry_;
    MillenniumAtariBootstrapExecution execution_;
    MillenniumAtariTrapEntry trap_;
    MillenniumAtariFopenResultGateExecution fopen_result_gate_;
    MillenniumAtariFopenFallthrough fopen_fallthrough_;
    MillenniumAtariFreadFramePrefixExecution fread_frame_prefix_;
    MillenniumAtariFreadConfigTransferBoundary fread_config_transfer_;
    MillenniumAtariRootInventory root_inventory_;
    MillenniumAtariConfigEvidence config_;
    MillenniumAtariConfigEntry config_entry_;
    MillenniumAtariFreadConfigLoadAddressBoundary fread_config_load_address_boundary_;
    MillenniumAtariFreadMappedConfigPrelude fread_mapped_config_prelude_;
    std::optional<MillenniumAtariReadOnlyGemdosSession> read_only_gemdos_;
    std::vector<std::uint8_t> post_config_payload_;
};

} // namespace eon
