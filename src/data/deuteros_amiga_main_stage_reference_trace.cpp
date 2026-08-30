#include "data/deuteros_amiga_main_stage_reference_trace.hpp"

#include <charconv>
#include <cstdint>
#include <map>
#include <string_view>

namespace eon {
namespace {

bool decimal_u64(const std::string_view value, std::uint64_t& result) {
    if (value.empty()) return false;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
}

bool parse_fields(const std::string_view value, std::uint64_t& sequence, std::uint64_t& tick,
    std::string_view& type, std::map<std::string_view, std::string_view>& fields) {
    const auto first = value.find(' ');
    const auto second = first == std::string_view::npos ? std::string_view::npos
        : value.find(' ', first + 1U);
    const auto type_end = second == std::string_view::npos ? std::string_view::npos
        : value.find(' ', second + 1U);
    if (first == std::string_view::npos || second == std::string_view::npos
        || type_end == std::string_view::npos
        || !decimal_u64(value.substr(0, first), sequence)
        || !decimal_u64(value.substr(first + 1U, second - first - 1U), tick)) return false;
    type = value.substr(second + 1U, type_end - second - 1U);
    for (std::size_t cursor = type_end + 1U; cursor < value.size();) {
        const auto end = value.find(' ', cursor);
        const auto field = value.substr(cursor, end == std::string_view::npos
            ? std::string_view::npos : end - cursor);
        const auto equals = field.find('=');
        if (equals == std::string_view::npos || equals == 0U || equals == field.size() - 1U
            || equals != field.rfind('=')
            || !fields.emplace(field.substr(0, equals), field.substr(equals + 1U)).second) return false;
        if (end == std::string_view::npos) break;
        cursor = end + 1U;
    }
    return !type.empty();
}

bool copy_loop(const std::map<std::string_view, std::string_view>& fields) {
    return fields.size() == 2U && fields.contains("pc") && fields.contains("opcode")
        && fields.at("pc") == "0x000210d4" && fields.at("opcode") == "0x51c8";
}

} // namespace

bool validate_deuteros_amiga_main_stage_reference_events(const std::string_view events,
    DeuterosAmigaMainStageReferenceTraceDiagnostics& diagnostics, std::string& error) {
    diagnostics = {};
    if (events.empty() || events.back() != '\n') {
        error = "Deuteros Amiga main-stage events must use LF-terminated records";
        return false;
    }
    std::uint64_t previous_sequence = 0;
    std::uint64_t previous_tick = 0;
    bool first = true;
    for (std::size_t cursor = 0; cursor < events.size();) {
        const auto end = events.find('\n', cursor);
        const auto line = events.substr(cursor, end - cursor);
        cursor = end + 1U;
        const auto tab = line.find('\t');
        if (line.empty() || line.size() > 4096U || line.find('\r') != std::string_view::npos
            || tab == std::string_view::npos || tab != line.rfind('\t')
            || line.substr(0, tab) != "event") {
            error = "Deuteros Amiga main-stage event is not event<TAB>sequence tick type fields";
            return false;
        }
        std::uint64_t sequence = 0;
        std::uint64_t tick = 0;
        std::string_view type;
        std::map<std::string_view, std::string_view> fields;
        if (!parse_fields(line.substr(tab + 1U), sequence, tick, type, fields)
            || (!first && (sequence <= previous_sequence || tick <= previous_tick))
            || type != "main-copy-loop-pc" || !copy_loop(fields)) {
            error = "Deuteros Amiga main-stage event is outside the v3 raw copy-loop schema";
            return false;
        }
        ++diagnostics.event_count;
        ++diagnostics.main_copy_loop_pc_count;
        previous_sequence = sequence;
        previous_tick = tick;
        first = false;
    }
    if (diagnostics.event_count != 1U) {
        error = "Deuteros Amiga main-stage copy-loop trace requires exactly one observation";
        return false;
    }
    return true;
}

} // namespace eon
