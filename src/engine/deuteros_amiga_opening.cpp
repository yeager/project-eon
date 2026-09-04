#include "engine/deuteros_amiga_opening.hpp"

#include "data/deuteros_amiga_data_disk.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <stdexcept>

namespace eon {
namespace {

std::vector<std::uint8_t> require_clean_deuteros_amiga_system_adf(
    std::vector<std::uint8_t> system_adf) {
    constexpr std::string_view clean_system_adf_sha256 =
        "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38";
    if (to_hex(sha256(system_adf)) != clean_system_adf_sha256) {
        throw std::runtime_error("Unsupported Deuteros Amiga system ADF");
    }
    return system_adf;
}

std::vector<std::uint8_t> require_clean_deuteros_amiga_data_adf(
    std::vector<std::uint8_t> data_adf) {
    constexpr std::string_view clean_data_adf_sha256 =
        "99909db1e190be02e049084743af44f00e331be6bf2d97b4831ada5fe4c30b4a";
    if (to_hex(sha256(data_adf)) != clean_data_adf_sha256) {
        throw std::runtime_error("Unsupported Deuteros Amiga data ADF");
    }
    return data_adf;
}

DeuterosAmigaMainResourceTransfer require_opening_transfer(const AmigaAdf& disk,
    const DeuterosAmigaLoadPlan& plan) {
    const auto transfer = read_deuteros_amiga_main_resource(disk, plan, 0);
    if (!transfer) {
        throw std::runtime_error("Deuteros opening resource unexpectedly requests loader retry");
    }
    return *transfer;
}

} // namespace

DeuterosAmigaOpening::DeuterosAmigaOpening(std::vector<std::uint8_t> system_adf,
    std::vector<std::uint8_t> data_adf)
    : disk_(require_clean_deuteros_amiga_system_adf(std::move(system_adf))),
      data_disk_(require_clean_deuteros_amiga_data_adf(std::move(data_adf))),
      admitted_game_text_(admit_all_game_text_from_source(
          Game::deuteros, Platform::amiga,
          "Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 1 of 2).adf",
          disk_.bytes(0, AmigaAdf::standard_size))),
      load_plan_(parse_deuteros_amiga_load_plan(disk_)),
      title_handoff_route_(parse_deuteros_amiga_title_handoff_route(disk_, load_plan_)),
      transferred_bundle_(require_opening_transfer(disk_, load_plan_)),
      bundle_(parse_deuteros_amiga_bundle(disk_, load_plan_.resource_disk_offsets[0])),
      sound_bank_(parse_deuteros_amiga_sound_bank(disk_, bundle_)),
      blob_(parse_deuteros_amiga_indexed_blob(disk_, bundle_)),
      vm_(disk_, bundle_),
      random_(transferred_bundle_, load_plan_.main_stage_entry) {
    static_cast<void>(inspect_deuteros_amiga_data_disk_header(data_disk_));
}

DeuterosAmigaVmEvents DeuterosAmigaOpening::tick(bool input_pressed) {
    // The exact $0f,$0b38 handoff returns to the bootstrap loader, which
    // reads the separately hash-verified title stage. Continuing to tick the
    // opening VM after that original edge would create a mixed, invented
    // runtime. Keep its final original frame and stop at the title boundary.
    if (title_handed_off_) return {};
    DeuterosAmigaVmInputs inputs;
    inputs.input_pressed = input_pressed;
    inputs.random_word = [this] { return random_.next(); };
    auto events = vm_.tick(inputs);
    // The initial opening's accepted $14 input reaches $0f with this exact
    // raw operand. Only that verified route selects bootstrap profile one and
    // the original title-stage interval; another alternate resource must not
    // be presented as a title handoff.
    if (!title_stage_session_ && events.alternate_resources.size() == 1
        && events.alternate_resources.front().resource_relative_offset
            == title_handoff_route_.resource_relative_offset
        && events.alternate_resources.front().command_disk_offset
            == title_handoff_route_.resource_command_disk_offset) {
        // Bind only the exact raw $0f command to the caller-side profile
        // written through the verified bootstrap return cell.  Do not treat a
        // coincidentally equal resource operand from another bundle path as a
        // title stage.
        title_bootstrap_session_.emplace(disk_, load_plan_, title_handoff_route_);
        while (title_bootstrap_session_->advance()) {}
        if (!title_bootstrap_session_->complete()) {
            throw std::runtime_error("Deuteros title bootstrap did not reach its entry dispatch");
        }
        title_stage_session_.emplace(disk_, load_plan_, title_handoff_route_.bootstrap_profile_value);
        static_cast<void>(title_stage_session_->execute_local_prefix());
        events.title_handoff = true;
    }
    ++ticks_;
    // $207fe runs independently between scheduler calls, including after an
    // input-triggered title handoff.
    random_.advance_vblank();
    frame_composed_on_last_tick_ = false;
    static_cast<void>(compositor_.compose(disk_, bundle_, blob_, vm_.compositor_channels()));
    for (const auto& channel : vm_.channels()) {
        if (channel.active && channel.bitmap_selector == load_plan_.main_stage_entry.alternate_renderer_selector) {
            alternate_renderer_trace_ = trace_deuteros_amiga_alternate_renderer(
                transferred_bundle_, load_plan_.main_stage_entry, channel.mode_data);
            apply_deuteros_amiga_alternate_renderer(compositor_.global_video_frame(), disk_, load_plan_,
                *alternate_renderer_trace_);
            break;
        }
    }
    last_frame_ = compositor_.global_video_frame();
    frame_composed_on_last_tick_ = true;
    if (events.title_handoff) title_handed_off_ = true;
    return events;
}

std::optional<std::vector<std::uint8_t>> DeuterosAmigaOpening::rgba_frame() const {
    if (!last_frame_) return std::nullopt;
    const auto palette = decode_deuteros_amiga_palette(disk_, bundle_, vm_.palette_index());
    return colorize_deuteros_amiga_frame(*last_frame_, palette);
}

std::optional<DeuterosAmigaOpeningCheckpoint> DeuterosAmigaOpening::checkpoint() const {
    if (!last_frame_ || title_handed_off_) return std::nullopt;
    const auto rgba = rgba_frame();
    if (!rgba) return std::nullopt;
    return DeuterosAmigaOpeningCheckpoint{
        ticks_, random_.vblank_counter(), vm_.input_gate(),
        to_hex(sha256(last_frame_->color_indices)), to_hex(sha256(*rgba))};
}

std::size_t DeuterosAmigaOpening::active_channel_count() const {
    return static_cast<std::size_t>(std::count_if(vm_.channels().begin(), vm_.channels().end(),
        [](const DeuterosAmigaChannelState& channel) { return channel.active; }));
}

} // namespace eon
