#include "data/deuteros_amiga_reference_trace.hpp"

#include <charconv>
#include <cstdint>
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

bool hex(const std::string_view value, const std::size_t digits) {
    if (value.size() != digits + 2U || value.substr(0, 2) != "0x") return false;
    for (const auto character : value.substr(2)) {
        if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f'))) return false;
    }
    return true;
}

bool exact_keys(const std::map<std::string_view, std::string_view>& fields,
                const std::initializer_list<std::string_view> expected) {
    if (fields.size() != expected.size()) return false;
    for (const auto key : expected) if (!fields.contains(key)) return false;
    return true;
}

bool raw_result(const std::map<std::string_view, std::string_view>& fields) {
    return hex(fields.at("result_d0"), 8) && hex(fields.at("result_sr"), 4);
}

bool parse_fields(const std::string_view value, std::uint64_t& sequence, std::uint64_t& tick,
                  std::string_view& type, std::map<std::string_view, std::string_view>& fields) {
    const auto first_space = value.find(' ');
    const auto second_space = first_space == std::string_view::npos
        ? std::string_view::npos : value.find(' ', first_space + 1U);
    if (first_space == std::string_view::npos || second_space == std::string_view::npos
        || !decimal_u64(value.substr(0, first_space), sequence)
        || !decimal_u64(value.substr(first_space + 1U, second_space - first_space - 1U), tick)) return false;
    const auto type_end = value.find(' ', second_space + 1U);
    if (type_end == std::string_view::npos) return false;
    type = value.substr(second_space + 1U, type_end - second_space - 1U);
    if (type.empty()) return false;
    for (std::size_t cursor = type_end + 1U; cursor < value.size();) {
        const auto end = value.find(' ', cursor);
        const auto field = value.substr(cursor, end == std::string_view::npos
            ? std::string_view::npos : end - cursor);
        const auto equals = field.find('=');
        if (equals == std::string_view::npos || equals == 0U || equals == field.size() - 1U
            || equals != field.rfind('=') || !fields.emplace(field.substr(0, equals),
                field.substr(equals + 1U)).second) return false;
        if (end == std::string_view::npos) break;
        cursor = end + 1U;
    }
    return true;
}

bool schema_matches(const std::string_view type,
                    const std::map<std::string_view, std::string_view>& fields) {
    if (type == "exec") {
        return exact_keys(fields, {"site", "exec_base_address", "vector", "result_d0", "result_sr"})
            && fields.at("site") == "0x00040450" && fields.at("exec_base_address") == "0x00000004"
            && (fields.at("vector") == "-0x0096" || fields.at("vector") == "-0x009c")
            && raw_result(fields);
    }
    if (type == "open-library") {
        return exact_keys(fields, {"site", "name_address", "exec_base_address", "vector", "result_d0", "result_sr"})
            && fields.at("site") == "0x0001ed80" && fields.at("name_address") == "0x0001ed02"
            && fields.at("exec_base_address") == "0x00000004" && fields.at("vector") == "-0x0228"
            && raw_result(fields);
    }
    if (type == "graphics") {
        return exact_keys(fields, {"site", "graphics_base_address", "vector", "result_d0", "result_sr"})
            && fields.at("site") == "0x0004069a" && fields.at("graphics_base_address") == "0x00012fec"
            && fields.at("vector") == "-0x00c0" && raw_result(fields);
    }
    if (type == "custom-register") {
        if (!exact_keys(fields, {"site", "base", "offset", "value", "result_d0", "result_sr"})
            || fields.at("site") != "0x0004046c" || fields.at("base") != "0x00dff000"
            || !hex(fields.at("offset"), 4) || !hex(fields.at("value"), 4) || !raw_result(fields)) return false;
        const auto offset = fields.at("offset");
        const auto value = fields.at("value");
        return (offset == "0x0040" && value == "0x7fff")
            || (offset == "0x0042" && value == "0x7fff")
            || (offset == "0x009a" && value == "0xc000")
            || (offset == "0x0096" && value == "0x87ff");
    }
    if (type == "callback") {
        const bool registration = exact_keys(fields,
            {"site", "callback", "exec_base_address", "vector", "result_d0", "result_sr"})
            && fields.at("site") == "0x0001ef74" && fields.at("callback") == "0x0001f056"
            && fields.at("exec_base_address") == "0x00000004" && fields.at("vector") == "-0x01ce"
            && raw_result(fields);
        const bool entry = exact_keys(fields, {"site", "incoming_a0", "result_d0", "result_sr"})
            && fields.at("site") == "0x0001f056" && hex(fields.at("incoming_a0"), 8) && raw_result(fields);
        return registration || entry;
    }
    return false;
}

} // namespace

bool validate_deuteros_amiga_title_reference_events(
    const std::string_view events, DeuterosAmigaReferenceTraceDiagnostics& diagnostics,
    std::string& error) {
    diagnostics = {};
    if (events.empty() || events.back() != '\n') {
        error = "Deuteros Amiga reference events must use LF-terminated records";
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
        if (line.size() > 4096U || line.empty() || line.find('\r') != std::string_view::npos
            || tab == std::string_view::npos || tab != line.rfind('\t') || line.substr(0, tab) != "event") {
            error = "Deuteros Amiga reference event is not event<TAB>sequence tick type fields";
            return false;
        }
        std::uint64_t sequence = 0;
        std::uint64_t tick = 0;
        std::string_view type;
        std::map<std::string_view, std::string_view> fields;
        if (!parse_fields(line.substr(tab + 1U), sequence, tick, type, fields)
            || (!first && (sequence <= previous_sequence || tick <= previous_tick))
            || !schema_matches(type, fields)) {
            error = "Deuteros Amiga reference event is outside the documented title-stage schema";
            return false;
        }
        first = false;
        previous_sequence = sequence;
        previous_tick = tick;
        ++diagnostics.event_count;
        if (type == "exec") ++diagnostics.exec_count;
        else if (type == "open-library") ++diagnostics.open_library_count;
        else if (type == "graphics") ++diagnostics.graphics_count;
        else if (type == "custom-register") ++diagnostics.custom_register_count;
        else ++diagnostics.callback_count;
    }
    return diagnostics.event_count != 0U;
}

} // namespace eon
