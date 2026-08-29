#pragma once

#include "data/amiga_adf.hpp"
#include "data/deuteros_amiga_loader.hpp"
#include "data/deuteros_amiga_title_stage.hpp"

#include <span>
#include <string>

namespace eon {

// An explicit, read-only boundary after the verified opening input handoff.
// It proves which original stage is ready for execution and retains the
// caller-connected local entry-prefix result without inventing the
// register/global state or graphics-library calls that the stage requires.
class DeuterosAmigaTitleStageSession {
public:
    DeuterosAmigaTitleStageSession(const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
        std::uint16_t incoming_profile);

    [[nodiscard]] const AmigaLoadStage& stage() const noexcept { return stage_; }
    [[nodiscard]] const DeuterosAmigaTitleStageProfile& profile() const noexcept { return profile_; }
    // The literal local entry prefix selected by the live bootstrap handoff.
    // It stops before the original Exec ABI boundary.
    [[nodiscard]] const DeuterosAmigaTitleEntryPrefix& entry_prefix() const noexcept {
        return entry_prefix_;
    }
    // These two writes are the complete caller-proven title RAM effect before
    // the first unresolved Exec vector. They are sparse records only; no
    // synthetic address space is allocated or exposed.
    [[nodiscard]] const DeuterosAmigaTitleEntryPrefixState& entry_prefix_state() const noexcept {
        return entry_prefix_state_;
    }
    // The next literal instruction initializes A7 and then stops before the
    // original reads the unknown Exec base. This is a register fact only.
    [[nodiscard]] const DeuterosAmigaTitleExecPrelude& exec_prelude() const noexcept {
        return exec_prelude_;
    }
    [[nodiscard]] std::span<const std::uint8_t> original_bytes() const;
    [[nodiscard]] const std::string& original_sha256() const noexcept { return original_sha256_; }

private:
    const AmigaAdf* disk_ = nullptr;
    AmigaLoadStage stage_;
    DeuterosAmigaTitleStageProfile profile_;
    DeuterosAmigaTitleEntryPrefix entry_prefix_;
    DeuterosAmigaTitleEntryPrefixState entry_prefix_state_;
    DeuterosAmigaTitleExecPrelude exec_prelude_;
    std::string original_sha256_;
};

} // namespace eon
