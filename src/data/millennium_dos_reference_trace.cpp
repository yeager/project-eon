#include "data/millennium_dos_reference_trace.hpp"

#include <array>
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
            || equals != field.rfind('=') || !fields.emplace(field.substr(0, equals),
                field.substr(equals + 1)).second) return false;
        if (end == std::string_view::npos) break;
        cursor = end + 1;
    }
    return true;
}

bool schema_matches(const std::string_view type,
                    const std::map<std::string_view, std::string_view>& fields) {
    // These values are source addresses and literal operands from the exact
    // archive pinned by the v2 manifest. They validate a recorder's claim;
    // they do not assert call success, carry flags, returned registers, DOS
    // behaviour, vector installation, file contents, or child execution.
    if (type == "interrupt") {
        // 0x0209 is the schema's stable MOV AX,0x2591 setup-site identifier.
        // The recorder observes the following CD 21 instruction at 0x020c.
        return fields_equal(fields, {{"image", "mill.com"}, {"pc", "0x0209"},
                   {"int", "0x21"}, {"ax", "0x2591"}, {"dx", "0x0000"}})
            || fields_equal(fields, {{"image", "titles.exe"}, {"pc", "0x0127"},
                   {"int", "0x91"}, {"ax", "0x0000"}, {"es", "cs"}, {"bx", "0x1ac4"}})
            // 2200AD.EXE is a flat COM-style image. Its byte-locked entry
            // loads these registers immediately before its first call reaches
            // the same private wrapper at $0124. This still says nothing
            // about the interrupt's ABI, result, or whether it returns.
            || fields_equal(fields, {{"image", "2200ad.exe"}, {"pc", "0x0124"},
                   {"int", "0x91"}, {"ax", "0x001f"}, {"es", "cs"}, {"bx", "0xd19e"}})
            || fields_equal(fields, {{"image", "titles.exe"}, {"pc", "0x0d0a"},
                   {"int", "0x21"}, {"ah", "0x06"}, {"dl", "0xff"}})
            || fields_equal(fields, {{"image", "titles.exe"}, {"pc", "0x1a12"},
                   {"int", "0x21"}, {"ax", "0x4c00"}});
    }
    if (type == "file") {
        return fields_equal(fields, {{"image", "mill.com"}, {"pc", "0x02cf"},
                   {"op", "driver-load"}, {"path", "ega640.bin"}})
            || fields_equal(fields, {{"image", "mill.com"}, {"pc", "0x02cf"},
                   {"op", "driver-load"}, {"path", "mcga.bin"}});
    }
    if (type == "exec") {
        return fields_equal(fields, {{"image", "mill.com"}, {"pc", "0x0337"},
                   {"int", "0x21"}, {"ax", "0x4b00"}, {"path", "titles.exe"}})
            || fields_equal(fields, {{"image", "mill.com"}, {"pc", "0x0337"},
                   {"int", "0x21"}, {"ax", "0x4b00"}, {"path", "2200ad.exe"}});
    }
    return false;
}

bool fixed_lowercase_hex(const std::string_view value, const std::size_t digits) {
    if (value.size() != digits + 2 || !value.starts_with("0x")) return false;
    for (const auto character : value.substr(2)) {
        if (!((character >= '0' && character <= '9')
                || (character >= 'a' && character <= 'f'))) return false;
    }
    return true;
}

bool gx_schema_matches(const std::size_t index, const std::string_view type,
                       const std::map<std::string_view, std::string_view>& fields) {
    // Each PC/return site is an independently hash-locked instruction
    // boundary. AX and the two mode values deliberately remain observations,
    // not asserted behaviour or values supplied to the recovered runtime.
    if (index == 0) {
        const auto ax = fields.find("ax");
        return type == "private-return" && ax != fields.end() && fixed_lowercase_hex(ax->second, 4)
            && fields_equal(fields, {{"image", "2200ad.exe"}, {"pc", "0x0129"},
                {"int", "0x91"}, {"ax", ax->second}});
    }
    if (index == 1 || index == 9) {
        const auto value = fields.find("value");
        const auto pc = index == 1 ? "0xd349" : "0xd388";
        return value != fields.end() && fixed_lowercase_hex(value->second, 2)
            && type == "mode-read"
            && fields_equal(fields, {{"image", "2200ad.exe"}, {"pc", pc},
                {"address", "0xda05"}, {"value", value->second}});
    }
    if (index == 2) {
        return type == "adapter-return"
            && fields_equal(fields, {{"image", "2200gx.exe"}, {"pc", "0x00ed"},
                {"op", "retf"}, {"return_pc", "0xd376"}});
    }
    constexpr std::array<std::string_view, 6> call_pcs{
        "0xd376", "0xd379", "0xd37c", "0xd37f", "0xd382", "0xd385"};
    constexpr std::array<std::string_view, 6> return_pcs{
        "0xd379", "0xd37c", "0xd37f", "0xd382", "0xd385", "0xd388"};
    const auto call_index = index - 3;
    return call_index < call_pcs.size() && type == "local-return"
        && fields_equal(fields, {{"image", "2200ad.exe"}, {"call_pc", call_pcs[call_index]},
            {"return_pc", return_pcs[call_index]}});
}

bool parse_gx_event_line(const std::string_view line, const std::size_t index,
                         std::uint64_t& sequence, std::uint64_t& tick,
                         std::string& error) {
    const auto tab = line.find('\t');
    if (line.size() > 4096 || line.empty() || line.find('\r') != std::string_view::npos
        || tab == std::string_view::npos || tab != line.rfind('\t') || line.substr(0, tab) != "event") {
        error = "Millennium DOS GX reference event is not event<TAB>sequence tick type fields";
        return false;
    }
    std::string_view type;
    std::map<std::string_view, std::string_view> fields;
    if (!parse_fields(line.substr(tab + 1), sequence, tick, type, fields)
            || !gx_schema_matches(index, type, fields)) {
        error = "Millennium DOS GX reference event is outside the documented startup schema";
        return false;
    }
    return true;
}

} // namespace

bool validate_millennium_dos_english_reference_events(
    const std::string_view events, MillenniumDosEnglishReferenceTraceDiagnostics& diagnostics,
    std::string& error) {
    diagnostics = {};
    if (events.empty() || events.back() != '\n') {
        error = "Millennium DOS reference events must use LF-terminated records";
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
        if (line.size() > 4096 || line.empty() || line.find('\r') != std::string_view::npos || tab == std::string_view::npos
            || tab != line.rfind('\t') || line.substr(0, tab) != "event") {
            error = "Millennium DOS reference event is not event<TAB>sequence tick type fields";
            return false;
        }
        std::uint64_t sequence = 0;
        std::uint64_t tick = 0;
        std::string_view type;
        std::map<std::string_view, std::string_view> fields;
        if (!parse_fields(line.substr(tab + 1), sequence, tick, type, fields)
            || (!first && (sequence <= previous_sequence || tick <= previous_tick))) {
            error = "Millennium DOS reference event has invalid ordering or fields";
            return false;
        }
        if (!schema_matches(type, fields)) {
            error = "Millennium DOS reference event is outside the documented startup schema";
            return false;
        }
        first = false;
        previous_sequence = sequence;
        previous_tick = tick;
        ++diagnostics.event_count;
        if (type == "interrupt") ++diagnostics.interrupt_count;
        if (type == "file") ++diagnostics.file_count;
        if (type == "exec") ++diagnostics.exec_count;
    }
    return diagnostics.event_count != 0;
}

bool validate_millennium_dos_gx_startup_reference_events(
    const std::string_view events, MillenniumDosGxStartupReferenceTraceDiagnostics& diagnostics,
    std::string& error) {
    diagnostics = {};
    if (events.empty() || events.back() != '\n') {
        error = "Millennium DOS GX reference events must use LF-terminated records";
        return false;
    }
    std::uint64_t previous_sequence = 0;
    std::uint64_t previous_tick = 0;
    std::size_t cursor = 0;
    for (std::size_t index = 0; index < 10; ++index) {
        const auto end = events.find('\n', cursor);
        if (end == std::string_view::npos) {
            error = "Millennium DOS GX reference trace ended before its documented boundary";
            return false;
        }
        std::uint64_t sequence = 0;
        std::uint64_t tick = 0;
        if (!parse_gx_event_line(events.substr(cursor, end - cursor), index, sequence, tick, error)
                || (index != 0 && (sequence <= previous_sequence || tick <= previous_tick))) {
            if (error.empty()) error = "Millennium DOS GX reference events have invalid ordering";
            return false;
        }
        previous_sequence = sequence;
        previous_tick = tick;
        cursor = end + 1;
    }
    if (cursor != events.size()) {
        error = "Millennium DOS GX reference trace must contain exactly its documented boundary records";
        return false;
    }
    diagnostics.event_count = 10;
    diagnostics.private_return_count = 1;
    diagnostics.mode_read_count = 2;
    diagnostics.adapter_return_count = 1;
    diagnostics.local_return_count = 6;
    return true;
}

} // namespace eon
