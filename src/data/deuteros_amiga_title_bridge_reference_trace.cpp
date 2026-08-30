#include "data/deuteros_amiga_title_bridge_reference_trace.hpp"

#include <charconv>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <string_view>
#include <vector>

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

bool lower_hex(const std::string_view value, const std::size_t digits) {
    if (value.size() != digits) return false;
    for (const auto character : value) {
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
    const auto type_end = second_space == std::string_view::npos
        ? std::string_view::npos : value.find(' ', second_space + 1U);
    if (first_space == std::string_view::npos || second_space == std::string_view::npos
        || type_end == std::string_view::npos
        || !decimal_u64(value.substr(0, first_space), sequence)
        || !decimal_u64(value.substr(first_space + 1U, second_space - first_space - 1U), tick)) return false;
    type = value.substr(second_space + 1U, type_end - second_space - 1U);
    if (type.empty()) return false;
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
    return true;
}

bool exec_return(const std::map<std::string_view, std::string_view>& fields) {
    return exact_keys(fields, {"site", "exec_base_address", "vector", "result_d0", "result_sr"})
        && fields.at("site") == "0x00040450" && fields.at("exec_base_address") == "0x00000004"
        && (fields.at("vector") == "-0x0096" || fields.at("vector") == "-0x009c") && raw_result(fields);
}

bool open_library_return(const std::map<std::string_view, std::string_view>& fields) {
    return exact_keys(fields, {"site", "name_address", "exec_base_address", "vector", "result_d0", "result_sr"})
        && fields.at("site") == "0x0001ed80" && fields.at("name_address") == "0x0001ed02"
        && fields.at("exec_base_address") == "0x00000004" && fields.at("vector") == "-0x0228"
        && raw_result(fields);
}

bool graphics(const std::map<std::string_view, std::string_view>& fields, const bool has_result) {
    if (has_result) {
        if (!exact_keys(fields, {"site", "graphics_base_address", "vector", "result_d0", "result_sr"})) return false;
    } else if (!exact_keys(fields, {"site", "graphics_base_address", "vector"})) return false;
    return fields.at("site") == "0x0004069a"
        && fields.at("graphics_base_address") == "0x00012fec" && fields.at("vector") == "-0x00c0"
        && (!has_result || raw_result(fields));
}

bool custom_register(const std::map<std::string_view, std::string_view>& fields, const bool has_result) {
    if (has_result) {
        if (!exact_keys(fields, {"site", "base", "offset", "value", "result_d0", "result_sr"})) return false;
    } else if (!exact_keys(fields, {"site", "base", "offset", "value"})) return false;
    if (fields.at("site") != "0x0004046c"
        || fields.at("base") != "0x00dff000" || !hex(fields.at("offset"), 4)
        || !hex(fields.at("value"), 4) || (has_result && !raw_result(fields))) return false;
    const auto offset = fields.at("offset");
    const auto value = fields.at("value");
    return (offset == "0x0040" && value == "0x7fff")
        || (offset == "0x0042" && value == "0x7fff")
        || (offset == "0x009a" && value == "0xc000")
        || (offset == "0x0096" && value == "0x87ff");
}

bool queue_snapshot(const std::map<std::string_view, std::string_view>& fields, const std::string_view phase) {
    return exact_keys(fields, {"phase", "queue_address", "queue_bytes", "pending_address", "pending_word",
                               "source_table_address", "source_table_size", "source_table_sha256"})
        && fields.at("phase") == phase && fields.at("queue_address") == "0x0001eec0"
        && lower_hex(fields.at("queue_bytes"), 40) && fields.at("pending_address") == "0x0001eed6"
        && hex(fields.at("pending_word"), 4) && fields.at("source_table_address") == "0x0001ee20"
        && fields.at("source_table_size") == "160"
        && fields.at("source_table_sha256") == "2f00ffdf05ab28379e97e91e98fa764e45769d7ea55363846543becf7552e265";
}

bool callback_entry(const std::map<std::string_view, std::string_view>& fields) {
    return exact_keys(fields, {"site", "incoming_a0", "frame_04_0d"})
        && fields.at("site") == "0x0001f056" && hex(fields.at("incoming_a0"), 8)
        && lower_hex(fields.at("frame_04_0d"), 20);
}

bool selector_entry(const std::map<std::string_view, std::string_view>& fields) {
    return exact_keys(fields, {"site", "incoming_d0"}) && fields.at("site") == "0x0001fe7a"
        && hex(fields.at("incoming_d0"), 8);
}

bool local_call(const std::map<std::string_view, std::string_view>& fields, const std::string_view call_site,
                const std::string_view return_pc, const bool has_result) {
    if (has_result) {
        if (!exact_keys(fields, {"call_site", "callee", "return_pc", "result_d0", "result_sr"})) return false;
    } else if (!exact_keys(fields, {"call_site", "callee", "return_pc"})) return false;
    return fields.at("call_site") == call_site
        && fields.at("callee") == "0x0001feaa" && fields.at("return_pc") == return_pc
        && (!has_result || raw_result(fields));
}

bool dispatch_snapshot(const std::map<std::string_view, std::string_view>& fields,
                       const std::string_view phase) {
    return exact_keys(fields, {"phase", "site", "cell_1f98c", "cell_1f98e", "cell_1f99c", "cell_1f974",
                               "cell_1f970", "cell_1f96c", "cell_1f994", "cell_1f998"})
        && fields.at("phase") == phase && fields.at("site") == "0x0001fbe6"
        && hex(fields.at("cell_1f98c"), 2) && hex(fields.at("cell_1f98e"), 2)
        && hex(fields.at("cell_1f99c"), 8) && hex(fields.at("cell_1f974"), 8)
        && hex(fields.at("cell_1f970"), 8) && hex(fields.at("cell_1f96c"), 8)
        && hex(fields.at("cell_1f994"), 8) && hex(fields.at("cell_1f998"), 8);
}

} // namespace

bool validate_deuteros_amiga_title_bridge_reference_events(
    const std::string_view events, DeuterosAmigaTitleBridgeReferenceTraceDiagnostics& diagnostics,
    std::string& error) {
    diagnostics = {};
    if (events.empty() || events.back() != '\n') {
        error = "Deuteros Amiga title-bridge events must use LF-terminated records";
        return false;
    }
    enum class Segment { exec, open_library, abi_calls, queue_pre, callback, queue_post,
                         selector, local_first_call, local_first_return, local_second_call,
                         local_second_return, dispatch_pre, dispatch_post, complete };
    Segment segment = Segment::exec;
    // Calls may interleave by ABI type. Keep their exact nesting/order, not
    // merely independent per-type totals, so a return cannot be reassigned to
    // an earlier observed call.
    enum class PendingCall { graphics, custom_register };
    std::vector<PendingCall> pending_calls;
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
            error = "Deuteros Amiga title-bridge event is not event<TAB>sequence tick type fields";
            return false;
        }
        std::uint64_t sequence = 0;
        std::uint64_t tick = 0;
        std::string_view type;
        std::map<std::string_view, std::string_view> fields;
        if (!parse_fields(line.substr(tab + 1U), sequence, tick, type, fields)
            || (!first && (sequence <= previous_sequence || tick <= previous_tick))) {
            error = "Deuteros Amiga title-bridge event has malformed fields or non-increasing sequence/tick";
            return false;
        }
        bool accepted = false;
        switch (segment) {
        case Segment::exec:
            accepted = type == "exec-return" && exec_return(fields);
            if (accepted) {
                ++diagnostics.exec_return_count;
                if (diagnostics.exec_return_count == 2U) segment = Segment::open_library;
            }
            break;
        case Segment::open_library:
            accepted = type == "open-library-return" && open_library_return(fields);
            if (accepted) { ++diagnostics.open_library_return_count; segment = Segment::abi_calls; }
            break;
        case Segment::abi_calls:
            if (type == "graphics-call" && graphics(fields, false)) {
                pending_calls.push_back(PendingCall::graphics);
                ++diagnostics.graphics_call_count; accepted = true;
            } else if (type == "graphics-return" && graphics(fields, true)
                       && !pending_calls.empty() && pending_calls.back() == PendingCall::graphics) {
                pending_calls.pop_back(); ++diagnostics.graphics_return_count; accepted = true;
            } else if (type == "custom-register-call" && custom_register(fields, false)) {
                pending_calls.push_back(PendingCall::custom_register);
                ++diagnostics.custom_register_call_count; accepted = true;
            } else if (type == "custom-register-return" && custom_register(fields, true)
                       && !pending_calls.empty() && pending_calls.back() == PendingCall::custom_register) {
                pending_calls.pop_back(); ++diagnostics.custom_register_return_count; accepted = true;
            } else if (type == "callback-registration-return" && pending_calls.empty()
                       && diagnostics.graphics_call_count != 0U
                       && exact_keys(fields, {"site", "callback", "exec_base_address", "vector", "result_d0", "result_sr"})
                       && fields.at("site") == "0x0001ef74" && fields.at("callback") == "0x0001f056"
                       && fields.at("exec_base_address") == "0x00000004" && fields.at("vector") == "-0x01ce"
                       && raw_result(fields)) {
                ++diagnostics.callback_registration_return_count; accepted = true; segment = Segment::queue_pre;
            }
            break;
        case Segment::queue_pre:
            accepted = type == "queue-snapshot" && queue_snapshot(fields, "pre");
            if (accepted) { ++diagnostics.queue_snapshot_count; segment = Segment::callback; }
            break;
        case Segment::callback:
            accepted = type == "callback-entry" && callback_entry(fields);
            if (accepted) { ++diagnostics.callback_entry_count; segment = Segment::queue_post; }
            break;
        case Segment::queue_post:
            accepted = type == "queue-snapshot" && queue_snapshot(fields, "post");
            if (accepted) { ++diagnostics.queue_snapshot_count; segment = Segment::selector; }
            break;
        case Segment::selector:
            accepted = type == "selector-entry" && selector_entry(fields);
            if (accepted) { ++diagnostics.selector_entry_count; segment = Segment::local_first_call; }
            break;
        case Segment::local_first_call:
            accepted = type == "local-call" && local_call(fields, "0x0001fe84", "0x0001fe88", false);
            if (accepted) { ++diagnostics.local_call_count; segment = Segment::local_first_return; }
            break;
        case Segment::local_first_return:
            accepted = type == "local-return" && local_call(fields, "0x0001fe84", "0x0001fe88", true);
            if (accepted) { ++diagnostics.local_return_count; segment = Segment::local_second_call; }
            break;
        case Segment::local_second_call:
            accepted = type == "local-call" && local_call(fields, "0x0001fe92", "0x0001fe96", false);
            if (accepted) { ++diagnostics.local_call_count; segment = Segment::local_second_return; }
            break;
        case Segment::local_second_return:
            accepted = type == "local-return" && local_call(fields, "0x0001fe92", "0x0001fe96", true);
            if (accepted) { ++diagnostics.local_return_count; segment = Segment::dispatch_pre; }
            break;
        case Segment::dispatch_pre:
            accepted = type == "dispatch-snapshot" && dispatch_snapshot(fields, "pre");
            if (accepted) { ++diagnostics.dispatch_snapshot_count; segment = Segment::dispatch_post; }
            break;
        case Segment::dispatch_post:
            accepted = type == "dispatch-snapshot" && dispatch_snapshot(fields, "post");
            if (accepted) { ++diagnostics.dispatch_snapshot_count; segment = Segment::complete; }
            break;
        case Segment::complete: break;
        }
        if (!accepted) {
            error = "Deuteros Amiga title-bridge event is outside the v3 ordered raw-observation schema";
            return false;
        }
        first = false;
        previous_sequence = sequence;
        previous_tick = tick;
        diagnostics.last_sequence = sequence;
        diagnostics.last_tick = tick;
        ++diagnostics.event_count;
    }
    if (segment != Segment::complete) {
        error = "Deuteros Amiga title-bridge trace ends before the required bridge sequence";
        return false;
    }
    return true;
}

} // namespace eon
