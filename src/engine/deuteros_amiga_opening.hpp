#pragma once

#include "data/amiga_adf.hpp"
#include "data/deuteros_amiga_bundle.hpp"
#include "data/deuteros_amiga_channel_vm.hpp"
#include "data/deuteros_amiga_frame.hpp"
#include "data/deuteros_amiga_loader.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace eon {

// Live, original-data-backed opening sequence. It owns the ADF image and all
// VM state, advances the verified VBL source once per scheduler tick, and
// exposes an explicit handoff event rather than manufacturing a game screen.
class DeuterosAmigaOpening {
public:
    explicit DeuterosAmigaOpening(std::vector<std::uint8_t> system_adf);

    [[nodiscard]] DeuterosAmigaVmEvents tick(bool input_pressed = false);
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> rgba_frame() const;
    [[nodiscard]] bool frame_composed_on_last_tick() const { return frame_composed_on_last_tick_; }
    [[nodiscard]] std::uint64_t ticks() const { return ticks_; }
    [[nodiscard]] const DeuterosAmigaBootstrapProfile& title_handoff_profile() const {
        return load_plan_.title_handoff_profile;
    }

private:
    AmigaAdf disk_;
    DeuterosAmigaLoadPlan load_plan_;
    DeuterosAmigaBundle bundle_;
    DeuterosAmigaIndexedBlob blob_;
    DeuterosAmigaChannelVm vm_;
    DeuterosAmigaRandom random_;
    std::optional<DeuterosAmigaFrame> last_frame_;
    bool frame_composed_on_last_tick_ = false;
    std::uint64_t ticks_ = 0;
};

} // namespace eon
