#pragma once

#include "data/amiga_adf.hpp"
#include "data/deuteros_amiga_bundle.hpp"
#include "data/deuteros_amiga_channel_vm.hpp"
#include "data/deuteros_amiga_audio.hpp"
#include "data/deuteros_amiga_alternate_renderer.hpp"
#include "data/deuteros_amiga_frame.hpp"
#include "data/deuteros_amiga_loader.hpp"
#include "engine/deuteros_amiga_title_stage_session.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace eon {

// A native self-consistency checkpoint for the already recovered opening
// only. It is not emulator output, a capture receipt, title-stage evidence or
// a parity claim. Hashes describe frames already composed from the exact ADFs.
struct DeuterosAmigaOpeningCheckpoint {
    std::uint64_t tick = 0;
    std::uint32_t vblank_counter = 0;
    bool input_gate = false;
    std::string indexed_frame_sha256;
    std::string rgba_frame_sha256;
};

// Live, original-data-backed opening sequence. It owns the ADF image and all
// VM state, advances the verified VBL source once per scheduler tick, and
// exposes an explicit handoff event rather than manufacturing a game screen.
class DeuterosAmigaOpening {
public:
    // Both original disks are required for a runtime session. The recovered
    // opening reads only disk 1's caller-proved ranges, but disk 2 remains
    // identity-bound instead of being silently omitted or substituted.
    DeuterosAmigaOpening(std::vector<std::uint8_t> system_adf,
        std::vector<std::uint8_t> data_adf);

    [[nodiscard]] DeuterosAmigaVmEvents tick(bool input_pressed = false);
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> rgba_frame() const;
    [[nodiscard]] std::optional<DeuterosAmigaOpeningCheckpoint> checkpoint() const;
    [[nodiscard]] bool frame_composed_on_last_tick() const { return frame_composed_on_last_tick_; }
    [[nodiscard]] std::uint64_t ticks() const { return ticks_; }
    [[nodiscard]] std::uint32_t vblank_counter() const { return random_.vblank_counter(); }
    // These are raw opening-VM observables used by the provenance overlay.
    // They are not title/gameplay labels or host controls.
    [[nodiscard]] bool input_gate() const { return vm_.input_gate(); }
    [[nodiscard]] std::uint16_t palette_index() const { return vm_.palette_index(); }
    [[nodiscard]] std::size_t active_channel_count() const;
    [[nodiscard]] bool title_handed_off() const { return title_handed_off_; }
    [[nodiscard]] const DeuterosAmigaBootstrapProfile& title_handoff_profile() const {
        return load_plan_.title_handoff_profile;
    }
    [[nodiscard]] const DeuterosAmigaTitleHandoffRoute& title_handoff_route() const {
        return title_handoff_route_;
    }
    [[nodiscard]] const DeuterosAmigaSoundBank& sound_bank() const { return sound_bank_; }
    [[nodiscard]] const std::optional<DeuterosAmigaAlternateRendererTrace>& alternate_renderer_trace() const {
        return alternate_renderer_trace_;
    }
    [[nodiscard]] const std::optional<DeuterosAmigaTitleStageSession>& title_stage_session() const {
        return title_stage_session_;
    }

private:
    AmigaAdf disk_;
    AmigaAdf data_disk_;
    DeuterosAmigaLoadPlan load_plan_;
    DeuterosAmigaTitleHandoffRoute title_handoff_route_;
    DeuterosAmigaMainResourceTransfer transferred_bundle_;
    DeuterosAmigaBundle bundle_;
    DeuterosAmigaSoundBank sound_bank_;
    DeuterosAmigaIndexedBlob blob_;
    DeuterosAmigaChannelVm vm_;
    DeuterosAmigaRandom random_;
    DeuterosAmigaCompositor compositor_;
    std::optional<DeuterosAmigaFrame> last_frame_;
    std::optional<DeuterosAmigaAlternateRendererTrace> alternate_renderer_trace_;
    std::optional<DeuterosAmigaTitleStageSession> title_stage_session_;
    bool title_handed_off_ = false;
    bool frame_composed_on_last_tick_ = false;
    std::uint64_t ticks_ = 0;
};

} // namespace eon
