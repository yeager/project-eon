#include "data/millennium_amiga_reference_trace.hpp"

#include <charconv>
#include <initializer_list>
#include <map>
#include <string_view>

namespace eon {
namespace {

bool decimal_u64(const std::string_view value, std::uint64_t& result) {
    if (value.empty()) return false;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
}

bool fields_equal(const std::map<std::string_view, std::string_view>& fields,
                  const std::initializer_list<std::pair<std::string_view, std::string_view>> expected) {
    if (fields.size() != expected.size()) return false;
    for (const auto& [key, value] : expected) {
        const auto found = fields.find(key);
        if (found == fields.end() || found->second != value) return false;
    }
    return true;
}

bool parse_fields(const std::string_view value, std::uint64_t& sequence, std::uint64_t& tick,
                  std::string_view& type, std::map<std::string_view, std::string_view>& fields) {
    const auto first_space = value.find(' ');
    const auto second_space = first_space == std::string_view::npos
        ? std::string_view::npos : value.find(' ', first_space + 1);
    if (first_space == std::string_view::npos || second_space == std::string_view::npos
        || !decimal_u64(value.substr(0, first_space), sequence)
        || !decimal_u64(value.substr(first_space + 1, second_space - first_space - 1), tick)) return false;
    const auto type_end = value.find(' ', second_space + 1);
    type = value.substr(second_space + 1, type_end == std::string_view::npos
        ? std::string_view::npos : type_end - second_space - 1);
    if (type.empty() || type_end == std::string_view::npos) return false;
    std::size_t cursor = type_end + 1;
    while (cursor < value.size()) {
        const auto end = value.find(' ', cursor);
        const auto field = value.substr(cursor, end == std::string_view::npos
            ? std::string_view::npos : end - cursor);
        const auto equals = field.find('=');
        if (equals == std::string_view::npos || equals == 0 || equals == field.size() - 1
            || equals != field.rfind('=')
            || !fields.emplace(field.substr(0, equals), field.substr(equals + 1)).second) return false;
        if (end == std::string_view::npos) break;
        cursor = end + 1;
    }
    return true;
}

bool schema_matches(const std::string_view type,
                    const std::map<std::string_view, std::string_view>& fields) {
    // Both records are literal instruction sites in the bootstrap which is
    // copied from ADF +0x400 to $70000.  The register values are the immediate
    // caller-side handoff values only; they say nothing about device I/O, the
    // opaque first stage, whether JSR returns, or the target's execution.
    return type == "cpu" && (
        fields_equal(fields, {{"image", "bootstrap-loader"}, {"pc", "0x702e4"},
                              {"op", "jsr-indirect"}, {"a3", "0x41000"}})
        || fields_equal(fields, {{"image", "bootstrap-loader"}, {"pc", "0x70320"},
                                 {"op", "jmp-indirect"}, {"a3", "0x68000"},
                                 {"d6", "0xa8d398fb"}}));
}

} // namespace

bool validate_millennium_amiga_english_reference_events(
    const std::string_view events, MillenniumAmigaReferenceTraceDiagnostics& diagnostics,
    std::string& error) {
    diagnostics = {};
    if (events.empty() || events.back() != '\n') {
        error = "Millennium Amiga reference events must use LF-terminated records";
        return false;
    }
    std::uint64_t previous_sequence = 0;
    std::uint64_t previous_tick = 0;
    bool first = true;
    std::size_t cursor = 0;
    while (cursor < events.size()) {
        const auto end = events.find('\n', cursor);
        const auto line = events.substr(cursor, end - cursor);
        cursor = end + 1;
        const auto tab = line.find('\t');
        if (line.size() > 4096 || line.empty() || line.find('\r') != std::string_view::npos
            || tab == std::string_view::npos || tab != line.rfind('\t')
            || line.substr(0, tab) != "event") {
            error = "Millennium Amiga reference event is not event<TAB>sequence tick type fields";
            return false;
        }
        std::uint64_t sequence = 0;
        std::uint64_t tick = 0;
        std::string_view type;
        std::map<std::string_view, std::string_view> fields;
        if (!parse_fields(line.substr(tab + 1), sequence, tick, type, fields)
            || (!first && (sequence <= previous_sequence || tick <= previous_tick))) {
            error = "Millennium Amiga reference event has invalid ordering or fields";
            return false;
        }
        if (!schema_matches(type, fields)) {
            error = "Millennium Amiga reference event is outside the documented bootstrap schema";
            return false;
        }
        first = false;
        previous_sequence = sequence;
        previous_tick = tick;
        ++diagnostics.event_count;
        ++diagnostics.cpu_count;
    }
    return diagnostics.event_count != 0;
}

} // namespace eon
