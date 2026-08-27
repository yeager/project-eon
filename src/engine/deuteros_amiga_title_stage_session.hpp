#pragma once

#include "data/amiga_adf.hpp"
#include "data/deuteros_amiga_loader.hpp"
#include "data/deuteros_amiga_title_stage.hpp"

#include <span>
#include <string>

namespace eon {

// An explicit, read-only boundary after the verified opening input handoff.
// It proves which original stage is ready for execution without inventing the
// register/global state or graphics-library calls that the stage requires.
class DeuterosAmigaTitleStageSession {
public:
    DeuterosAmigaTitleStageSession(const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

    [[nodiscard]] const AmigaLoadStage& stage() const noexcept { return stage_; }
    [[nodiscard]] const DeuterosAmigaTitleStageProfile& profile() const noexcept { return profile_; }
    [[nodiscard]] std::span<const std::uint8_t> original_bytes() const;
    [[nodiscard]] const std::string& original_sha256() const noexcept { return original_sha256_; }

private:
    const AmigaAdf* disk_ = nullptr;
    AmigaLoadStage stage_;
    DeuterosAmigaTitleStageProfile profile_;
    std::string original_sha256_;
};

} // namespace eon
