#include "data/deuteros_atari_reference_trace.hpp"

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

bool lowercase_hex(const std::string_view value, const std::size_t digits) {
    if (value.size() != digits + 2U || value.substr(0, 2) != "0x") return false;
    for (const auto character : value.substr(2)) {
        if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f'))) return false;
    }
    return true;
}

bool raw_hex_frame(const std::string_view value) {
    // A raw XBIOS frame is evidence, not a decoded service ABI.  Keep it
    // bounded and byte-addressable while allowing the recorder to retain the
    // complete frame it observed.
    if (value.empty() || value.size() > 4096U || value.size() % 2U != 0U) return false;
    for (const auto character : value) {
        if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f'))) return false;
    }
    return true;
}

bool provenance_sha256(const std::string_view value) {
    return value.size() == 64U && [&] {
        for (const auto character : value) {
            if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f'))) return false;
        }
        return true;
    }();
}

bool exact_keys(const std::map<std::string_view, std::string_view>& fields,
                const std::initializer_list<std::string_view> expected) {
    if (fields.size() != expected.size()) return false;
    for (const auto key : expected) if (!fields.contains(key)) return false;
    return true;
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

bool is_u32(const std::map<std::string_view, std::string_view>& fields, const std::string_view key) {
    return lowercase_hex(fields.at(key), 8U);
}

bool is_u16(const std::map<std::string_view, std::string_view>& fields, const std::string_view key) {
    return lowercase_hex(fields.at(key), 4U);
}

bool schema_matches(const std::string_view type,
                    const std::map<std::string_view, std::string_view>& fields) {
    if (type == "trap") {
        return exact_keys(fields, {"pc", "incoming_a7", "incoming_sr", "selector", "callback",
                                   "return_pc", "return_a7", "return_sr", "return_d0"})
            && fields.at("pc") == "0x00001edc" && fields.at("selector") == "0x0026"
            && fields.at("callback") == "0x00001fa6" && is_u32(fields, "incoming_a7")
            && is_u16(fields, "incoming_sr") && is_u32(fields, "return_pc")
            && is_u32(fields, "return_a7") && is_u16(fields, "return_sr") && is_u32(fields, "return_d0");
    }
    if (type == "callback") {
        return exact_keys(fields, {"entry_pc", "incoming_a7", "stack_longword", "outgoing_a7",
                                   "return_pc", "return_a7", "return_sr", "return_d0"})
            && fields.at("entry_pc") == "0x00001fa6" && fields.at("outgoing_a7") == "0x0007b000"
            && is_u32(fields, "incoming_a7") && is_u32(fields, "stack_longword")
            && is_u32(fields, "return_pc") && is_u32(fields, "return_a7")
            && is_u16(fields, "return_sr") && is_u32(fields, "return_d0");
    }
    if (type == "frame") {
        return exact_keys(fields, {"site", "input_frame", "result_frame"})
            && fields.at("site") == "0x00001e9c" && raw_hex_frame(fields.at("input_frame"))
            && raw_hex_frame(fields.at("result_frame"));
    }
    if (type == "state") {
        return exact_keys(fields, {"ram_25f4", "ram_25f4_provenance", "ram_25fc",
                                   "ram_25fc_provenance", "branch_pc", "state_word"})
            && is_u32(fields, "ram_25f4") && provenance_sha256(fields.at("ram_25f4_provenance"))
            && is_u32(fields, "ram_25fc") && provenance_sha256(fields.at("ram_25fc_provenance"))
            && is_u32(fields, "branch_pc") && is_u16(fields, "state_word");
    }
    if (type == "table") {
        if (!exact_keys(fields, {"base", "shifted_index", "target_a1", "entry_pc", "return_pc",
                                 "return_d1", "return_d2"})
            || fields.at("base") != "0x00001eac" || !is_u16(fields, "shifted_index")
            || !is_u32(fields, "target_a1") || !is_u32(fields, "entry_pc")
            || !is_u32(fields, "return_pc") || !is_u32(fields, "return_d1") || !is_u32(fields, "return_d2")) {
            return false;
        }
        const auto target = fields.at("target_a1");
        return (target == "0x00001f1a" || target == "0x00001f2e" || target == "0x00001f50"
                || target == "0x00001f52") && fields.at("entry_pc") == target;
    }
    if (type == "raw-reader") {
        return exact_keys(fields, {"entry_pc", "trap_pc", "call_a7", "return_pc", "return_a7",
                                   "return_sr", "return_d0"})
            && fields.at("entry_pc") == "0x00001e60" && fields.at("trap_pc") == "0x00001e9c"
            && is_u32(fields, "call_a7") && is_u32(fields, "return_pc")
            && is_u32(fields, "return_a7") && is_u16(fields, "return_sr") && is_u32(fields, "return_d0");
    }
    return false;
}

} // namespace

bool validate_deuteros_atari_reference_events(
    const std::string_view events, DeuterosAtariReferenceTraceDiagnostics& diagnostics,
    std::string& error) {
    diagnostics = {};
    if (events.empty() || events.back() != '\n') {
        error = "Deuteros Atari reference events must use LF-terminated records";
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
            error = "Deuteros Atari reference event is not event<TAB>sequence tick type fields";
            return false;
        }
        std::uint64_t sequence = 0;
        std::uint64_t tick = 0;
        std::string_view type;
        std::map<std::string_view, std::string_view> fields;
        if (!parse_fields(line.substr(tab + 1U), sequence, tick, type, fields)
            || (!first && (sequence <= previous_sequence || tick <= previous_tick))
            || !schema_matches(type, fields)) {
            error = "Deuteros Atari reference event is outside the documented boot schema";
            return false;
        }
        first = false;
        previous_sequence = sequence;
        previous_tick = tick;
        ++diagnostics.event_count;
        if (type == "trap") ++diagnostics.trap_count;
        else if (type == "callback") ++diagnostics.callback_count;
        else if (type == "frame") ++diagnostics.frame_count;
        else if (type == "state") ++diagnostics.state_count;
        else if (type == "table") ++diagnostics.table_count;
        else if (type == "raw-reader") ++diagnostics.raw_reader_count;
    }
    return diagnostics.event_count != 0U;
}

} // namespace eon
