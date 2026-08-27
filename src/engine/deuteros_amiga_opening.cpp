#include "engine/deuteros_amiga_opening.hpp"

#include <stdexcept>

namespace eon {
namespace {

DeuterosAmigaMainResourceTransfer require_opening_transfer(const AmigaAdf& disk,
    const DeuterosAmigaLoadPlan& plan) {
    const auto transfer = read_deuteros_amiga_main_resource(disk, plan, 0);
    if (!transfer) {
        throw std::runtime_error("Deuteros opening resource unexpectedly requests loader retry");
    }
    return *transfer;
}

} // namespace

DeuterosAmigaOpening::DeuterosAmigaOpening(std::vector<std::uint8_t> system_adf)
    : disk_(std::move(system_adf)),
      load_plan_(parse_deuteros_amiga_load_plan(disk_)),
      transferred_bundle_(require_opening_transfer(disk_, load_plan_)),
      bundle_(parse_deuteros_amiga_bundle(disk_, load_plan_.resource_disk_offsets[0])),
      sound_bank_(parse_deuteros_amiga_sound_bank(disk_, bundle_)),
      blob_(parse_deuteros_amiga_indexed_blob(disk_, bundle_)),
      vm_(disk_, bundle_),
      random_(transferred_bundle_, load_plan_.main_stage_entry) {}

DeuterosAmigaVmEvents DeuterosAmigaOpening::tick(bool input_pressed) {
    DeuterosAmigaVmInputs inputs;
    inputs.input_pressed = input_pressed;
    inputs.random_word = [this] { return random_.next(); };
    auto events = vm_.tick(inputs);
    ++ticks_;
    // $207fe runs independently between scheduler calls, including after an
    // input-triggered title handoff.
    random_.advance_vblank();
    frame_composed_on_last_tick_ = false;
    last_frame_ = compositor_.compose(disk_, bundle_, blob_, vm_.compositor_channels());
    for (const auto& channel : vm_.channels()) {
        if (channel.active && channel.bitmap_selector == load_plan_.main_stage_entry.alternate_renderer_selector) {
            alternate_renderer_trace_ = trace_deuteros_amiga_alternate_renderer(
                transferred_bundle_, load_plan_.main_stage_entry, channel.mode_data);
            break;
        }
    }
    frame_composed_on_last_tick_ = true;
    return events;
}

std::optional<std::vector<std::uint8_t>> DeuterosAmigaOpening::rgba_frame() const {
    if (!last_frame_) return std::nullopt;
    const auto palette = decode_deuteros_amiga_palette(disk_, bundle_, vm_.palette_index());
    return colorize_deuteros_amiga_frame(*last_frame_, palette);
}

} // namespace eon
