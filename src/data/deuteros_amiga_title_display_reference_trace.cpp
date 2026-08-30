#include "data/deuteros_amiga_title_display_reference_trace.hpp"

#include "data/deuteros_amiga_title_bridge_reference_trace.hpp"

#include <charconv>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <string_view>

namespace eon {
namespace {

bool hex(const std::string_view value, const std::size_t digits) {
    if (value.size() != digits + 2U || value.substr(0, 2U) != "0x") return false;
    for (const auto c : value.substr(2U)) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}
bool sha(const std::string_view value) {
    if (value.size() != 64U) return false;
    for (const auto c : value) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}
bool u64(const std::string_view value, std::uint64_t& result) {
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    return !value.empty() && parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
}
bool fields_for(const std::string_view input, std::uint64_t& sequence,
                std::uint64_t& tick, std::string_view& type,
                std::map<std::string_view, std::string_view>& fields) {
    const auto first = input.find(' '), second = first == std::string_view::npos ? first : input.find(' ', first + 1U);
    const auto third = second == std::string_view::npos ? second : input.find(' ', second + 1U);
    if (first == std::string_view::npos || second == std::string_view::npos || third == std::string_view::npos
        || !u64(input.substr(0, first), sequence) || !u64(input.substr(first + 1U, second - first - 1U), tick)) return false;
    type = input.substr(second + 1U, third - second - 1U);
    for (std::size_t at = third + 1U; at < input.size();) {
        const auto end = input.find(' ', at); const auto part = input.substr(at, end == std::string_view::npos ? end : end - at);
        const auto equal = part.find('=');
        if (equal == std::string_view::npos || equal == 0U || equal + 1U == part.size()
            || !fields.emplace(part.substr(0, equal), part.substr(equal + 1U)).second) return false;
        if (end == std::string_view::npos) break;
        at = end + 1U;
    }
    return !type.empty();
}
bool exact(const std::map<std::string_view, std::string_view>& fields,
           std::initializer_list<std::string_view> names) {
    if (fields.size() != names.size()) return false;
    for (const auto name : names) if (!fields.contains(name)) return false;
    return true;
}
}

bool validate_deuteros_amiga_title_display_reference_events(
    const std::string_view events, DeuterosAmigaTitleDisplayReferenceTraceDiagnostics& diagnostics,
    std::string& error) {
    diagnostics = {};
    if (events.empty() || events.back() != '\n') { error = "Deuteros title-display events must be LF terminated"; return false; }
    // The v3 prefix remains independently validated, including callback/input ordering.
    std::size_t display_start = std::string_view::npos;
    for (std::size_t at = 0; at < events.size();) {
        const auto end = events.find('\n', at); const auto line = events.substr(at, end - at);
        if (line.find(" display-layout ") != std::string_view::npos) { display_start = at; break; }
        at = end + 1U;
    }
    if (display_start == std::string_view::npos) { error = "Deuteros title-display trace has no display layout"; return false; }
    DeuterosAmigaTitleBridgeReferenceTraceDiagnostics bridge;
    if (!validate_deuteros_amiga_title_bridge_reference_events(events.substr(0, display_start), bridge, error)) return false;
    diagnostics.bridge_event_count = bridge.event_count;
    diagnostics.event_count = bridge.event_count;
    enum class Step { display, planes, palette, input, frame, audio, complete } step = Step::display;
    std::uint64_t previous_sequence = 0;
    std::uint64_t previous_tick = 0;
    bool first = true;
    for (std::size_t at = display_start; at < events.size();) {
        const auto end = events.find('\n', at); const auto line = events.substr(at, end - at); at = end + 1U;
        if (line.empty() || line.size() > 4096U || line.find('\r') != std::string_view::npos
            || !line.starts_with("event\t")) { error = "Malformed Deuteros title-display event"; return false; }
        std::string_view type; std::map<std::string_view, std::string_view> fields;
        std::uint64_t sequence = 0;
        std::uint64_t tick = 0;
        if (!fields_for(line.substr(6U), sequence, tick, type, fields)
            || (!first && (sequence <= previous_sequence || tick <= previous_tick))) {
            error = "Malformed Deuteros title-display fields";
            return false;
        }
        bool accepted = false;
        if (step == Step::display && type == "display-layout" && exact(fields, {"site", "base_source_address", "base_destination_a", "base_destination_b", "display_base", "display_list", "copper_list_sha256"})
            && fields.at("site") == "0x0001eda6" && fields.at("base_source_address") == "0x00012ff4" && fields.at("base_destination_a") == "0x0001f168" && fields.at("base_destination_b") == "0x0001f164" && hex(fields.at("display_base"), 8) && hex(fields.at("display_list"), 8) && sha(fields.at("copper_list_sha256"))) { accepted = true; step = Step::planes; ++diagnostics.display_layout_count; }
        else if (step == Step::planes && type == "bitplane-layout" && exact(fields, {"site", "base_pointer_address", "bitplane_count", "plane0", "plane1", "plane2", "plane3", "width_pixels", "height_lines", "bytes_per_row", "modulo"})
            && fields.at("site") == "0x0001f182" && fields.at("base_pointer_address") == "0x0001f168" && fields.at("bitplane_count") == "0x0004" && hex(fields.at("plane0"), 8) && hex(fields.at("plane1"), 8) && hex(fields.at("plane2"), 8) && hex(fields.at("plane3"), 8) && hex(fields.at("width_pixels"), 4) && hex(fields.at("height_lines"), 4) && hex(fields.at("bytes_per_row"), 4) && hex(fields.at("modulo"), 4)) { accepted = true; step = Step::palette; ++diagnostics.bitplane_layout_count; }
        else if (step == Step::palette && type == "palette-checkpoint" && exact(fields, {"site", "source_address", "destination_address", "word_count", "rgb4_sha256", "rgba_palette_sha256"})
            && fields.at("site") == "0x0001eda6" && fields.at("source_address") == "0x0001ed24" && fields.at("destination_address") == "0x00012ecc" && fields.at("word_count") == "0x0014" && sha(fields.at("rgb4_sha256")) && sha(fields.at("rgba_palette_sha256"))) { accepted = true; step = Step::input; ++diagnostics.palette_checkpoint_count; }
        else if (step == Step::input && type == "input-checkpoint" && exact(fields, {"callback_site", "selector_site", "queue_sha256", "input_timeline_sha256"})
            && fields.at("callback_site") == "0x0001f056" && fields.at("selector_site") == "0x0001fe7a" && sha(fields.at("queue_sha256")) && sha(fields.at("input_timeline_sha256"))) { accepted = true; step = Step::frame; ++diagnostics.input_checkpoint_count; }
        else if (step == Step::frame && type == "frame-checkpoint" && exact(fields, {"display_base", "rgba_width", "rgba_height", "bitplanes_sha256", "rgba_sha256"})
            && hex(fields.at("display_base"), 8) && hex(fields.at("rgba_width"), 4) && hex(fields.at("rgba_height"), 4) && sha(fields.at("bitplanes_sha256")) && sha(fields.at("rgba_sha256"))) { accepted = true; step = Step::audio; ++diagnostics.frame_checkpoint_count; }
        else if (step == Step::audio && type == "audio-checkpoint" && exact(fields, {"sample_rate", "channels", "sample_frames", "pcm_sha256"})
            && hex(fields.at("sample_rate"), 8) && hex(fields.at("channels"), 2) && hex(fields.at("sample_frames"), 8) && sha(fields.at("pcm_sha256"))) { accepted = true; step = Step::complete; ++diagnostics.audio_checkpoint_count; }
        if (!accepted) { error = "Deuteros title-display event is outside the v4 ordered raw-observation schema"; return false; }
        ++diagnostics.event_count;
        previous_sequence = sequence;
        previous_tick = tick;
        first = false;
    }
    if (step != Step::complete) { error = "Deuteros title-display trace ends before all display checkpoints"; return false; }
    return true;
}

} // namespace eon
