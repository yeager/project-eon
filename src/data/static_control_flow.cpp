#include "data/static_control_flow.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace eon {
namespace {

constexpr std::size_t max_input_bytes = 32U * 1024U * 1024U;
constexpr std::size_t max_json_depth = 32U;
constexpr std::size_t max_json_nodes = 1'000'000U;
constexpr std::size_t max_declared_ranges = 250'000U;
constexpr std::string_view classification = "static-candidate-unclassified";

struct JsonValue;
using JsonObject = std::map<std::string, JsonValue, std::less<>>;
using JsonArray = std::vector<JsonValue>;
struct JsonNumber { std::string raw; };
struct JsonValue : std::variant<std::nullptr_t, bool, JsonNumber, std::string, JsonArray, JsonObject> {
    using variant::variant;
};

class JsonParser {
public:
    explicit JsonParser(const std::string_view input) : input_(input) {}

    JsonValue parse() {
        auto value = parse_value(0);
        skip_space();
        if (position_ != input_.size()) fail("trailing data");
        return value;
    }

private:
    [[noreturn]] void fail(const std::string_view message) const {
        throw std::invalid_argument("Static control-flow sidecar JSON rejected: " + std::string(message));
    }

    void count_node() {
        if (++node_count_ > max_json_nodes) fail("too many JSON values");
    }

    void skip_space() {
        while (position_ < input_.size()
            && std::isspace(static_cast<unsigned char>(input_[position_])) != 0) ++position_;
    }

    char take() {
        if (position_ == input_.size()) fail("truncated value");
        return input_[position_++];
    }

    bool consume(const char expected) {
        skip_space();
        if (position_ < input_.size() && input_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    static unsigned hex_value(const char value) {
        if (value >= '0' && value <= '9') return static_cast<unsigned>(value - '0');
        if (value >= 'a' && value <= 'f') return static_cast<unsigned>(value - 'a' + 10);
        if (value >= 'A' && value <= 'F') return static_cast<unsigned>(value - 'A' + 10);
        return 16U;
    }

    unsigned take_hex_quad() {
        if (input_.size() - position_ < 4U) fail("truncated unicode escape");
        unsigned result = 0;
        for (unsigned count = 0; count < 4U; ++count) {
            const auto value = hex_value(input_[position_++]);
            if (value == 16U) fail("invalid unicode escape");
            result = (result << 4U) | value;
        }
        return result;
    }

    static void append_utf8(std::string& output, const unsigned value) {
        if (value <= 0x7fU) output.push_back(static_cast<char>(value));
        else if (value <= 0x7ffU) {
            output.push_back(static_cast<char>(0xc0U | (value >> 6U)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3fU)));
        } else {
            output.push_back(static_cast<char>(0xe0U | (value >> 12U)));
            output.push_back(static_cast<char>(0x80U | ((value >> 6U) & 0x3fU)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3fU)));
        }
    }

    std::string parse_string() {
        if (take() != '"') fail("expected string");
        std::string result;
        while (position_ < input_.size()) {
            const auto value = take();
            if (value == '"') return result;
            if (static_cast<unsigned char>(value) < 0x20U) fail("control character in string");
            if (value != '\\') {
                result.push_back(value);
                continue;
            }
            const auto escaped = take();
            switch (escaped) {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            case 'u': {
                auto codepoint = take_hex_quad();
                if (codepoint >= 0xd800U && codepoint <= 0xdbffU) {
                    if (input_.size() - position_ < 6U || input_[position_++] != '\\' || input_[position_++] != 'u') {
                        fail("unpaired unicode surrogate");
                    }
                    const auto low = take_hex_quad();
                    if (low < 0xdc00U || low > 0xdfffU) fail("invalid unicode surrogate pair");
                    codepoint = 0x10000U + ((codepoint - 0xd800U) << 10U) + (low - 0xdc00U);
                } else if (codepoint >= 0xdc00U && codepoint <= 0xdfffU) {
                    fail("unpaired unicode surrogate");
                }
                if (codepoint > 0xffffU) {
                    result.push_back(static_cast<char>(0xf0U | (codepoint >> 18U)));
                    result.push_back(static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3fU)));
                    result.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3fU)));
                    result.push_back(static_cast<char>(0x80U | (codepoint & 0x3fU)));
                } else append_utf8(result, codepoint);
                break;
            }
            default: fail("invalid string escape");
            }
        }
        fail("unterminated string");
    }

    JsonValue parse_number() {
        const auto start = position_;
        if (input_[position_] == '-') ++position_;
        if (position_ == input_.size()) fail("truncated number");
        if (input_[position_] == '0') ++position_;
        else {
            if (input_[position_] < '1' || input_[position_] > '9') fail("invalid number");
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
        }
        if (position_ < input_.size() && input_[position_] == '.') {
            ++position_;
            const auto fraction = position_;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
            if (fraction == position_) fail("invalid fraction");
        }
        if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
            ++position_;
            if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-')) ++position_;
            const auto exponent = position_;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
            if (exponent == position_) fail("invalid exponent");
        }
        count_node();
        return JsonNumber{std::string(input_.substr(start, position_ - start))};
    }

    JsonValue parse_array(const std::size_t depth) {
        take();
        JsonArray values;
        skip_space();
        if (consume(']')) { count_node(); return values; }
        while (true) {
            values.push_back(parse_value(depth + 1U));
            skip_space();
            if (consume(']')) break;
            if (!consume(',')) fail("expected array separator");
        }
        count_node();
        return values;
    }

    JsonValue parse_object(const std::size_t depth) {
        take();
        JsonObject values;
        skip_space();
        if (consume('}')) { count_node(); return values; }
        while (true) {
            skip_space();
            if (position_ == input_.size() || input_[position_] != '"') fail("expected object key");
            auto key = parse_string();
            if (!consume(':')) fail("expected key separator");
            auto [iterator, inserted] = values.emplace(std::move(key), parse_value(depth + 1U));
            static_cast<void>(iterator);
            if (!inserted) fail("duplicate object key");
            skip_space();
            if (consume('}')) break;
            if (!consume(',')) fail("expected object separator");
        }
        count_node();
        return values;
    }

    JsonValue parse_value(const std::size_t depth) {
        if (depth > max_json_depth) fail("JSON nesting limit exceeded");
        skip_space();
        if (position_ == input_.size()) fail("missing value");
        switch (input_[position_]) {
        case '{': return parse_object(depth);
        case '[': return parse_array(depth);
        case '"': { auto value = parse_string(); count_node(); return value; }
        case 't': if (input_.substr(position_, 4) == "true") { position_ += 4; count_node(); return true; } break;
        case 'f': if (input_.substr(position_, 5) == "false") { position_ += 5; count_node(); return false; } break;
        case 'n': if (input_.substr(position_, 4) == "null") { position_ += 4; count_node(); return nullptr; } break;
        default:
            if (input_[position_] == '-' || (input_[position_] >= '0' && input_[position_] <= '9')) return parse_number();
        }
        fail("invalid value");
    }

    std::string_view input_;
    std::size_t position_ = 0;
    std::size_t node_count_ = 0;
};

[[noreturn]] void reject(const std::string_view message) {
    throw std::invalid_argument("Static control-flow sidecar rejected: " + std::string(message));
}

const JsonObject& object(const JsonValue& value, const std::string_view name) {
    if (const auto* result = std::get_if<JsonObject>(&value)) return *result;
    reject(std::string(name) + " must be an object");
}

const JsonArray& array(const JsonValue& value, const std::string_view name) {
    if (const auto* result = std::get_if<JsonArray>(&value)) return *result;
    reject(std::string(name) + " must be an array");
}

const std::string& string(const JsonValue& value, const std::string_view name) {
    if (const auto* result = std::get_if<std::string>(&value)) return *result;
    reject(std::string(name) + " must be a string");
}

const JsonValue& required(const JsonObject& value, const std::string_view key) {
    const auto found = value.find(key);
    if (found == value.end()) reject(std::string("missing ") + std::string(key));
    return found->second;
}

void require_keys(const JsonObject& value, std::initializer_list<std::string_view> required_keys,
    std::initializer_list<std::string_view> optional_keys = {}) {
    for (const auto key : required_keys) static_cast<void>(required(value, key));
    for (const auto& [key, ignored] : value) {
        static_cast<void>(ignored);
        const auto allowed = std::find(required_keys.begin(), required_keys.end(), key) != required_keys.end()
            || std::find(optional_keys.begin(), optional_keys.end(), key) != optional_keys.end();
        if (!allowed) reject(std::string("unknown field ") + key);
    }
}

bool lower_hex_digest(const std::string_view value) {
    return value.size() == 64U && std::all_of(value.begin(), value.end(), [](const unsigned char character) {
        return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
    });
}

void require_digest(const JsonObject& value, const std::string_view key) {
    if (!lower_hex_digest(string(required(value, key), key))) reject(std::string(key) + " must be lower-case SHA-256");
}

std::uint64_t unsigned_number(const JsonValue& value, const std::string_view name) {
    const auto* number = std::get_if<JsonNumber>(&value);
    if (number == nullptr || number->raw.empty() || number->raw.front() == '-'
        || number->raw.find_first_of(".eE") != std::string::npos) reject(std::string(name) + " must be an unsigned integer");
    std::uint64_t result = 0;
    const auto [end, error] = std::from_chars(number->raw.data(), number->raw.data() + number->raw.size(), result);
    if (error != std::errc{} || end != number->raw.data() + number->raw.size()) reject(std::string(name) + " is out of range");
    return result;
}

void add_bytes(StaticControlFlowSummary& summary, const std::uint64_t length) {
    if (length > std::numeric_limits<std::uint64_t>::max() - summary.declared_byte_count) reject("declared byte count overflow");
    summary.declared_byte_count += length;
}

struct DeclaredRange { std::uint64_t address; std::uint64_t length; };

bool target_in_ranges(const std::vector<DeclaredRange>& ranges, const std::uint64_t target) {
    return std::any_of(ranges.begin(), ranges.end(), [&](const auto& range) {
        return target >= range.address && target - range.address < range.length;
    });
}

void parse_edge(const JsonObject& edge, const std::string_view address_key,
    const std::uint64_t range_offset, const std::uint64_t range_length,
    const std::vector<DeclaredRange>& ranges, StaticControlFlowSummary& summary) {
    require_keys(edge, {"source_offset", "instruction_size", "kind", "classification", address_key},
        {"target", "interrupt_vector", "trap_vector", "target_runtime_address",
         "target_image_relative_address", "target_scope"});
    const auto offset = unsigned_number(required(edge, "source_offset"), "edge source_offset");
    const auto size = unsigned_number(required(edge, "instruction_size"), "edge instruction_size");
    if (size == 0U || offset < range_offset || offset - range_offset >= range_length || size > range_length - (offset - range_offset)) {
        reject("edge is outside its declared range");
    }
    static_cast<void>(unsigned_number(required(edge, address_key), address_key));
    const auto& kind = string(required(edge, "kind"), "edge kind");
    if (kind != "return" && kind != "call" && kind != "jump" && kind != "conditional-jump"
        && kind != "interrupt" && kind != "trap") reject("unsupported edge kind");
    if (string(required(edge, "classification"), "edge classification") != classification) reject("edge classification is not static-candidate-unclassified");

    const auto has_target = edge.contains("target_runtime_address") || edge.contains("target_image_relative_address");
    const auto has_scope = edge.contains("target_scope");
    if (edge.contains("target_runtime_address") && edge.contains("target_image_relative_address")) reject("edge has two target address spaces");
    if (kind == "return") {
        if (!edge.contains("target") || string(required(edge, "target"), "return target") != "return-address-unproven"
            || has_target || has_scope || edge.contains("interrupt_vector") || edge.contains("trap_vector")) reject("invalid return edge");
    } else if (kind == "interrupt" || kind == "trap") {
        const auto vector_key = kind == "interrupt" ? "interrupt_vector" : "trap_vector";
        if (!edge.contains(vector_key) || has_target || has_scope || edge.contains("target")
            || edge.contains(kind == "interrupt" ? "trap_vector" : "interrupt_vector")) reject("invalid vector edge");
        static_cast<void>(unsigned_number(required(edge, vector_key), vector_key));
    } else {
        const auto expected_target_key = address_key == "runtime_address" ? "target_runtime_address" : "target_image_relative_address";
        if (!edge.contains(expected_target_key) || !has_scope || edge.contains("target")
            || edge.contains("interrupt_vector") || edge.contains("trap_vector")) reject("invalid direct edge");
        const auto target = unsigned_number(required(edge, expected_target_key), expected_target_key);
        const auto& scope = string(required(edge, "target_scope"), "target_scope");
        const auto expected_scope = target_in_ranges(ranges, target) ? "within-declared-range" : "outside-declared-range";
        if (scope != expected_scope) reject("direct edge target_scope disagrees with declared ranges");
        ++summary.target_scope_counts[scope];
    }
    ++summary.edge_count;
    ++summary.edge_kind_counts[kind];
}

void parse_document(const JsonObject& document, StaticControlFlowSummary& summary) {
    require_keys(document, {"schema", "cpu", "archive_sha256", "source", "source_kind", "source_sha256",
                            "classification", "ranges"},
                 {"address_space", "container_sha256", "carrier_archive_sha256", "direct_media_set_sha256"});
    if (string(required(document, "schema"), "document schema") != "project-eon.static-control-flow/v1") reject("unsupported document schema");
    const auto& cpu = string(required(document, "cpu"), "cpu");
    if (cpu != "i8086" && cpu != "m68000") reject("unsupported CPU");
    require_digest(document, "archive_sha256");
    require_digest(document, "source_sha256");
    for (const auto key : {"container_sha256", "carrier_archive_sha256", "direct_media_set_sha256"}) {
        if (document.contains(key) && !lower_hex_digest(string(required(document, key), key))) reject(std::string(key) + " must be lower-case SHA-256");
    }
    if (string(required(document, "source"), "source").empty()) reject("source provenance must not be empty");
    const auto& source_kind = string(required(document, "source_kind"), "source_kind");
    const auto has_container = document.contains("container_sha256");
    const auto has_carrier = document.contains("carrier_archive_sha256");
    const auto has_direct_set = document.contains("direct_media_set_sha256");
    const auto simple_i8086 = source_kind == "archive-member" || source_kind == "fat12-root-member"
        || source_kind == "verified-direct-media-member";
    const auto direct_media = source_kind == "verified-direct-media-member";
    const auto nested_m68k = source_kind == "nested-disk-range";
    const auto embedded_m68k = source_kind == "embedded-release-nested-disk-range";
    const auto direct_prg = source_kind == "nested-fat12-root-prg-text-data";
    const auto embedded_prg = source_kind == "embedded-release-nested-fat12-prg-text-data";
    if (!simple_i8086 && !nested_m68k && !embedded_m68k && !direct_prg && !embedded_prg) reject("unsupported source kind");
    if ((simple_i8086 && (cpu != "i8086" || has_container || has_carrier))
        || (direct_media != has_direct_set)
        || ((nested_m68k || direct_prg) && (cpu != "m68000" || has_carrier))
        || (nested_m68k && has_container)
        || (direct_prg && !has_container)
        || ((embedded_m68k || embedded_prg) && (cpu != "m68000" || !has_container || !has_carrier))) {
        reject("source-kind provenance fields are inconsistent");
    }
    if (string(required(document, "classification"), "document classification") != classification) reject("document classification is not static-candidate-unclassified");
    // Earlier extractor v1 documents omitted the redundant runtime spelling.
    // Retain that real-media grammar as a strict default, while an explicit
    // non-runtime address space must still name the one reviewed alternative.
    const auto address_space = document.contains("address_space")
        ? string(required(document, "address_space"), "address_space") : "runtime";
    if (address_space != "runtime" && address_space != "image-relative-unrelocated") reject("unsupported address space");
    const auto address_key = address_space == "runtime" ? "runtime_address" : "image_relative_address";
    const auto& ranges = array(required(document, "ranges"), "ranges");
    if (ranges.empty()) reject("document has no ranges");

    std::vector<DeclaredRange> declared;
    declared.reserve(ranges.size());
    const auto& release_identity = document.contains("carrier_archive_sha256")
        ? string(required(document, "carrier_archive_sha256"), "carrier_archive_sha256")
        : string(required(document, "archive_sha256"), "archive_sha256");
    const auto document_index = summary.documents.size();
    if (document_index == std::numeric_limits<std::size_t>::max()) reject("too many sidecar documents");
    summary.documents.push_back({release_identity, std::string(cpu), std::string(address_space),
        has_direct_set ? std::optional<std::string>(string(
            required(document, "direct_media_set_sha256"), "direct_media_set_sha256")) : std::nullopt});
    for (const auto& value : ranges) {
        const auto& range = object(value, "range");
        require_keys(range, {"source_offset", "length", address_key, "sha256", "edges"});
        const auto offset = unsigned_number(required(range, "source_offset"), "range source_offset");
        const auto length = unsigned_number(required(range, "length"), "range length");
        const auto address = unsigned_number(required(range, address_key), address_key);
        if (length == 0U || offset > std::numeric_limits<std::uint64_t>::max() - length
            || address > std::numeric_limits<std::uint64_t>::max() - length) reject("range overflows address space");
        require_digest(range, "sha256");
        for (const auto& prior : declared) {
            if (address < prior.address + prior.length && prior.address < address + length) reject("declared ranges overlap");
        }
        declared.push_back({address, length});
        if (summary.declared_ranges.size() == max_declared_ranges) reject("too many declared ranges");
        summary.declared_ranges.push_back({document_index,
            std::string(string(required(range, "sha256"), "sha256")), address, length});
        add_bytes(summary, length);
    }
    for (const auto& value : ranges) {
        const auto& range = object(value, "range");
        const auto offset = unsigned_number(required(range, "source_offset"), "range source_offset");
        const auto length = unsigned_number(required(range, "length"), "range length");
        for (const auto& edge : array(required(range, "edges"), "edges")) {
            parse_edge(object(edge, "edge"), address_key, offset, length, declared, summary);
        }
        ++summary.range_count;
    }
    ++summary.document_count;
    ++summary.cpu_counts[cpu];
    ++summary.archive_document_counts[string(required(document, "archive_sha256"), "archive_sha256")];
    ++summary.release_document_counts[release_identity];
}

} // namespace

StaticControlFlowSummary parse_static_control_flow_sidecar(const std::string_view json) {
    if (json.empty() || json.size() > max_input_bytes) reject("sidecar input exceeds bounded parser limit");
    const auto root_value = JsonParser(json).parse();
    const auto& root = object(root_value, "sidecar root");
    require_keys(root, {"schema", "classification", "documents"});
    if (string(required(root, "schema"), "sidecar schema") != "project-eon.static-control-flow-set/v1") reject("unsupported sidecar schema");
    if (string(required(root, "classification"), "sidecar classification") != classification) reject("sidecar classification is not static-candidate-unclassified");
    const auto& documents = array(required(root, "documents"), "documents");
    if (documents.empty()) reject("sidecar has no documents");
    StaticControlFlowSummary summary;
    for (const auto& document : documents) parse_document(object(document, "document"), summary);
    return summary;
}

} // namespace eon
