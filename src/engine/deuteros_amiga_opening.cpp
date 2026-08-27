#include "engine/deuteros_amiga_opening.hpp"

#include <stdexcept>
#include <string_view>

namespace eon {

DeuterosAmigaOpening::DeuterosAmigaOpening(std::vector<std::uint8_t> system_adf)
    : disk_(std::move(system_adf)),
      load_plan_(parse_deuteros_amiga_load_plan(disk_)),
      bundle_(parse_deuteros_amiga_bundle(disk_, load_plan_.resource_disk_offsets[0])),
      blob_(parse_deuteros_amiga_indexed_blob(disk_, bundle_)),
      vm_(disk_, bundle_),
      random_(disk_, bundle_) {}

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
    try {
        last_frame_ = compose_deuteros_amiga_frame(disk_, bundle_, blob_, vm_.channels());
        frame_composed_on_last_tick_ = true;
    } catch (const std::runtime_error& error) {
        // Stateful scanline save/restore is an accurately identified, separate
        // blitter path. Keep the last fully composed authentic frame until it
        // is implemented; never invent later pixels.
        if (std::string_view(error.what()).find("save/restore") == std::string_view::npos) throw;
    }
    return events;
}

std::optional<std::vector<std::uint8_t>> DeuterosAmigaOpening::rgba_frame() const {
    if (!last_frame_) return std::nullopt;
    const auto palette = decode_deuteros_amiga_palette(disk_, bundle_, vm_.palette_index());
    return colorize_deuteros_amiga_frame(*last_frame_, palette);
}

} // namespace eon
