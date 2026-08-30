#include "data/reference_trace.hpp"

#include "data/deuteros_atari_reference_trace.hpp"
#include "data/deuteros_amiga_reference_trace.hpp"
#include "data/deuteros_amiga_main_stage_reference_trace.hpp"
#include "data/deuteros_amiga_title_display_reference_trace.hpp"
#include "data/deuteros_amiga_title_bridge_reference_trace.hpp"
#include "data/millennium_amiga_reference_trace.hpp"
#include "data/millennium_dos_reference_trace.hpp"
#include "data/recovery_map.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <exception>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <string_view>
#include <system_error>
#include <utility>

namespace eon {
namespace {

constexpr std::uintmax_t maximum_manifest_size = 16U * 1024U * 1024U;
constexpr std::uintmax_t maximum_events_size = 256U * 1024U * 1024U;
constexpr std::size_t maximum_line_size = 4096;

// Keep capture admission diagnostics declarative.  The event parsers below
// remain deliberately adapter-specific, but this small map makes the
// relationship from a validated capture to hash-bound recovery evidence
// inspectable without turning the map into a hook or execution mechanism.
struct AdapterRecoveryMap {
    std::string_view adapter;
    std::array<std::string_view, 3> entry_ids;
    std::size_t entry_count;
};

constexpr std::array adapter_recovery_maps{
    AdapterRecoveryMap{"millennium-dos-en-startup-v1",
        {"millennium-dos-launcher", "millennium-dos-title-flow", "millennium-dos-game-flow"}, 3},
    AdapterRecoveryMap{"millennium-dos-en-gx-startup-v2",
        {"millennium-dos-game-flow", "millennium-dos-gx-overlay", ""}, 2},
    AdapterRecoveryMap{"millennium-dos-en-title-init-v2",
        {"millennium-dos-launcher", "millennium-dos-title-flow", ""}, 2},
    AdapterRecoveryMap{"deuteros-atari-st-boot-v1",
        {"deuteros-atari-protected-boot", "deuteros-atari-first-stage", ""}, 2},
    AdapterRecoveryMap{"millennium-amiga-en-defjam-bootstrap-v1",
        {"millennium-amiga-defjam-bootstrap", "millennium-amiga-shared-resident", ""}, 2},
    AdapterRecoveryMap{"deuteros-amiga-en-title-stage-v1",
        {"deuteros-amiga-main-stage", "deuteros-amiga-title-handoff", ""}, 2},
    AdapterRecoveryMap{"deuteros-amiga-en-main-copy-loop-v3",
        {"deuteros-amiga-main-stage", "", ""}, 1},
    AdapterRecoveryMap{"deuteros-amiga-en-title-bridge-v3",
        {"deuteros-amiga-main-stage", "deuteros-amiga-title-handoff", ""}, 2},
    AdapterRecoveryMap{"deuteros-amiga-en-title-display-v4",
        {"deuteros-amiga-main-stage", "deuteros-amiga-title-handoff", ""}, 2},
    AdapterRecoveryMap{"deuteros-amiga-en-title-display-artifacts-v5",
        {"deuteros-amiga-main-stage", "deuteros-amiga-title-handoff", ""}, 2},
};

const AdapterRecoveryMap* adapter_recovery_map(const std::string_view adapter) {
    const auto found = std::find_if(adapter_recovery_maps.begin(), adapter_recovery_maps.end(),
        [adapter](const auto& candidate) { return candidate.adapter == adapter; });
    return found == adapter_recovery_maps.end() ? nullptr : &*found;
}

bool trace_recovery_boundaries(const std::string_view adapter,
                               const std::string_view release_sha256,
                               std::vector<ReferenceTraceBoundary>& boundaries,
                               std::string& error) {
    boundaries.clear();
    if (adapter.empty()) return true;
    const auto* mapping = adapter_recovery_map(adapter);
    if (mapping == nullptr) {
        error = "Reference trace adapter has no declarative recovery-map binding";
        return false;
    }
    for (std::size_t index = 0; index < mapping->entry_count; ++index) {
        const auto entry_id = mapping->entry_ids[index];
        const auto found = std::find_if(recovery_map().begin(), recovery_map().end(),
            [release_sha256, entry_id](const auto& entry) {
                return entry.release_sha256 == release_sha256 && entry.id == entry_id;
            });
        if (found == recovery_map().end()
                || !release_has_recovery_map_entry(release_sha256, entry_id)) {
            error = "Reference trace adapter recovery-map binding is not admitted for its source release";
            return false;
        }
        boundaries.push_back({std::string(found->id), std::string(found->source_address),
            std::string(found->documentation_anchor)});
    }
    return true;
}

bool lowercase_sha256(const std::string_view value) {
    if (value.size() != 64) return false;
    for (const auto character : value) {
        if (!((character >= '0' && character <= '9')
                || (character >= 'a' && character <= 'f'))) return false;
    }
    return true;
}

bool decimal_u64(const std::string_view value, std::uint64_t& result) {
    if (value.empty()) return false;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
}

bool ascii_printable(const std::string_view value) {
    return !value.empty() && value.find_first_of("\r\n\t") == std::string_view::npos
        && std::all_of(value.begin(), value.end(), [](const unsigned char character) {
            return character >= 0x20U && character <= 0x7eU;
        });
}

bool basename_only(const std::string_view value) {
    if (!ascii_printable(value) || value == "." || value == "..") return false;
    if (value.find('/') != std::string_view::npos || value.find('\\') != std::string_view::npos) {
        return false;
    }
    const std::filesystem::path path(value);
    return path.filename() == path && !path.has_root_path();
}

bool utc_timestamp(const std::string_view value) {
    // Preserve a machine-readable capture boundary without accepting locale
    // dependent timestamps. Leap-second details are recorder evidence, not
    // replay semantics, so the lexical UTC form is intentionally sufficient.
    if (value.size() != 20 || value[4] != '-' || value[7] != '-'
        || value[10] != 'T' || value[13] != ':' || value[16] != ':' || value[19] != 'Z') {
        return false;
    }
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (index == 4 || index == 7 || index == 10 || index == 13 || index == 16 || index == 19) continue;
        if (value[index] < '0' || value[index] > '9') return false;
    }
    const auto number_at = [&value](const std::size_t offset, const std::size_t width) {
        unsigned result = 0;
        for (std::size_t index = 0; index < width; ++index) {
            result = result * 10U + static_cast<unsigned>(value[offset + index] - '0');
        }
        return result;
    };
    const unsigned year = number_at(0, 4);
    const unsigned month = number_at(5, 2);
    const unsigned day = number_at(8, 2);
    const unsigned hour = number_at(11, 2);
    const unsigned minute = number_at(14, 2);
    const unsigned second = number_at(17, 2);
    if (year == 0 || month == 0 || month > 12 || hour > 23 || minute > 59 || second > 59) {
        return false;
    }
    constexpr std::array<unsigned, 12> month_days{31, 28, 31, 30, 31, 30,
                                                   31, 31, 30, 31, 30, 31};
    const bool leap_year = year % 4U == 0U && (year % 100U != 0U || year % 400U == 0U);
    const unsigned maximum_day = month == 2 && leap_year ? 29U : month_days[month - 1U];
    return day != 0 && day <= maximum_day;
}

std::optional<Game> game_from_trace(const std::string_view value) {
    if (value == "millennium") return Game::millennium;
    if (value == "deuteros") return Game::deuteros;
    return std::nullopt;
}

std::optional<Platform> platform_from_trace(const std::string_view value) {
    if (value == "dos") return Platform::dos;
    if (value == "amiga") return Platform::amiga;
    if (value == "atari-st") return Platform::atari_st;
    return std::nullopt;
}

bool regular_file_size(const std::filesystem::path& path, const std::uintmax_t maximum,
                       std::uintmax_t& size, std::string& error) {
    std::error_code filesystem_error;
    const auto status = std::filesystem::symlink_status(path, filesystem_error);
    if (filesystem_error) {
        error = "Reference trace file is not a regular file: " + path.string();
        return false;
    }
    if (std::filesystem::is_symlink(status)) {
        error = "Reference trace file must not be a symbolic link: " + path.string();
        return false;
    }
    if (!std::filesystem::is_regular_file(path, filesystem_error) || filesystem_error) {
        error = "Reference trace file is not a regular file: " + path.string();
        return false;
    }
    size = std::filesystem::file_size(path, filesystem_error);
    if (filesystem_error) {
        error = "Unable to determine reference trace file size: " + path.string();
        return false;
    }
    if (size > maximum) {
        error = "Reference trace file exceeds its v1 size limit: " + path.string();
        return false;
    }
    return true;
}

bool parse_key_value_file(const std::filesystem::path& path, const std::uintmax_t maximum,
                          std::map<std::string, std::string>& fields, std::string& error) {
    std::uintmax_t size = 0;
    if (!regular_file_size(path, maximum, size, error)) return false;
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "Unable to read reference trace file: " + path.string();
        return false;
    }
    stream.seekg(-1, std::ios::end);
    char final_character = '\0';
    stream.get(final_character);
    if (final_character != '\n') {
        error = "Reference trace manifest must use LF-terminated records";
        return false;
    }
    stream.clear();
    stream.seekg(0);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.size() > maximum_line_size || line.empty() || line.back() == '\r') {
            error = "Reference trace manifest has an invalid line";
            return false;
        }
        const auto tab = line.find('\t');
        if (tab == std::string::npos || tab == 0 || tab != line.rfind('\t')) {
            error = "Reference trace manifest line is not key<TAB>value";
            return false;
        }
        const std::string key = line.substr(0, tab);
        const std::string value = line.substr(tab + 1);
        if (!ascii_printable(key) || !ascii_printable(value)
            || !std::all_of(key.begin(), key.end(), [](const unsigned char character) {
                // Versioned checksum field names (for example event_sha256)
                // are part of the fixed manifest contract. Keep the key
                // grammar bounded while admitting those decimal digits.
                return (character >= 'a' && character <= 'z')
                    || (character >= '0' && character <= '9') || character == '_';
            }) || !fields.emplace(key, value).second) {
            error = "Reference trace manifest has an invalid or duplicate field";
            return false;
        }
    }
    if (!stream.eof()) {
        error = "Unable to read reference trace manifest";
        return false;
    }
    if (fields.empty()) {
        error = "Reference trace manifest is empty";
        return false;
    }
    return true;
}

bool validate_events(const std::filesystem::path& path, const std::uintmax_t expected_size,
                     const std::string_view expected_sha256, std::size_t& count, std::string& error) {
    std::uintmax_t observed_size = 0;
    if (!regular_file_size(path, maximum_events_size, observed_size, error)) return false;
    if (observed_size != expected_size) {
        error = "Reference trace events size does not match its manifest";
        return false;
    }
    try {
        if (to_hex(sha256_file(path)) != expected_sha256) {
            error = "Reference trace events SHA-256 does not match its manifest";
            return false;
        }
    } catch (const std::exception&) {
        error = "Unable to hash reference trace events";
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "Unable to read reference trace events";
        return false;
    }
    stream.seekg(-1, std::ios::end);
    char final_character = '\0';
    stream.get(final_character);
    if (final_character != '\n') {
        error = "Reference trace events must use LF-terminated records";
        return false;
    }
    stream.clear();
    stream.seekg(0);
    std::uint64_t previous_sequence = 0;
    std::uint64_t previous_tick = 0;
    bool first = true;
    std::string line;
    while (std::getline(stream, line)) {
        if (line.size() > maximum_line_size || line.empty() || line.back() == '\r') {
            error = "Reference trace events has an invalid line";
            return false;
        }
        const auto tab = line.find('\t');
        if (tab == std::string::npos || tab != line.rfind('\t') || line.substr(0, tab) != "event") {
            error = "Reference trace event line is not event<TAB>sequence tick type";
            return false;
        }
        const auto value = std::string_view(line).substr(tab + 1);
        const auto first_space = value.find(' ');
        const auto second_space = first_space == std::string_view::npos
            ? std::string_view::npos : value.find(' ', first_space + 1);
        if (first_space == std::string_view::npos || second_space == std::string_view::npos
            || value.find(' ', second_space + 1) != std::string_view::npos) {
            error = "Reference trace event must contain sequence, tick, and type";
            return false;
        }
        std::uint64_t sequence = 0;
        std::uint64_t tick = 0;
        const auto type = value.substr(second_space + 1);
        if (!decimal_u64(value.substr(0, first_space), sequence)
            || !decimal_u64(value.substr(first_space + 1, second_space - first_space - 1), tick)
            || (type != "cpu" && type != "interrupt" && type != "file" && type != "memory"
                && type != "frame" && type != "audio")) {
            error = "Reference trace event has an invalid sequence, tick, or type";
            return false;
        }
        if (!first && (sequence <= previous_sequence || tick <= previous_tick)) {
            error = "Reference trace event sequence and tick must increase monotonically";
            return false;
        }
        first = false;
        previous_sequence = sequence;
        previous_tick = tick;
        if (count == std::numeric_limits<std::size_t>::max()) {
            error = "Reference trace event count overflow";
            return false;
        }
        ++count;
    }
    if (!stream.eof()) {
        error = "Unable to read reference trace events";
        return false;
    }
    if (count == 0) {
        error = "Reference trace events is empty";
        return false;
    }
    try {
        if (to_hex(sha256_file(path)) != expected_sha256) {
            error = "Reference trace events changed while it was being validated";
            return false;
        }
    } catch (const std::exception&) {
        error = "Unable to rehash reference trace events";
        return false;
    }
    return true;
}

bool validate_millennium_dos_english_events(const std::filesystem::path& path,
                                             const std::uintmax_t expected_size,
                                             const std::string_view expected_sha256,
                                             MillenniumDosEnglishReferenceTraceDiagnostics& diagnostics,
                                             std::string& error) {
    std::uintmax_t observed_size = 0;
    if (!regular_file_size(path, maximum_events_size, observed_size, error)) return false;
    if (observed_size != expected_size) {
        error = "Reference trace events size does not match its manifest";
        return false;
    }
    try {
        if (to_hex(sha256_file(path)) != expected_sha256) {
            error = "Reference trace events SHA-256 does not match its manifest";
            return false;
        }
    } catch (const std::exception&) {
        error = "Unable to hash reference trace events";
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "Unable to read reference trace events";
        return false;
    }
    std::string contents((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (stream.bad()) {
        error = "Unable to read reference trace events";
        return false;
    }
    if (!validate_millennium_dos_english_reference_events(contents, diagnostics, error)) return false;
    try {
        if (to_hex(sha256_file(path)) != expected_sha256) {
            error = "Reference trace events changed while it was being validated";
            return false;
        }
    } catch (const std::exception&) {
        error = "Unable to rehash reference trace events";
        return false;
    }
    return true;
}

bool validate_millennium_dos_gx_startup_events(const std::filesystem::path& path,
                                                const std::uintmax_t expected_size,
                                                const std::string_view expected_sha256,
                                                MillenniumDosGxStartupReferenceTraceDiagnostics& diagnostics,
                                                std::string& error) {
    std::uintmax_t observed_size = 0;
    if (!regular_file_size(path, maximum_events_size, observed_size, error)) return false;
    if (observed_size != expected_size) {
        error = "Reference trace events size does not match its manifest";
        return false;
    }
    try {
        if (to_hex(sha256_file(path)) != expected_sha256) {
            error = "Reference trace events SHA-256 does not match its manifest";
            return false;
        }
    } catch (const std::exception&) {
        error = "Unable to hash reference trace events";
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) { error = "Unable to read reference trace events"; return false; }
    std::string contents((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (stream.bad() || !validate_millennium_dos_gx_startup_reference_events(contents, diagnostics, error)) {
        if (stream.bad()) error = "Unable to read reference trace events";
        return false;
    }
    try {
        if (to_hex(sha256_file(path)) != expected_sha256) {
            error = "Reference trace events changed while it was being validated";
            return false;
        }
    } catch (const std::exception&) {
        error = "Unable to rehash reference trace events";
        return false;
    }
    return true;
}

bool validate_millennium_dos_title_init_events(const std::filesystem::path& path,
                                               const std::uintmax_t expected_size,
                                               const std::string_view expected_sha256,
                                               MillenniumDosTitleInitReferenceTraceDiagnostics& diagnostics,
                                               std::string& error) {
    std::uintmax_t observed_size = 0;
    if (!regular_file_size(path, maximum_events_size, observed_size, error)) return false;
    if (observed_size != expected_size) {
        error = "Reference trace events size does not match its manifest";
        return false;
    }
    try {
        if (to_hex(sha256_file(path)) != expected_sha256) {
            error = "Reference trace events SHA-256 does not match its manifest";
            return false;
        }
    } catch (const std::exception&) {
        error = "Unable to hash reference trace events";
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) { error = "Unable to read reference trace events"; return false; }
    std::string contents((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (stream.bad() || !validate_millennium_dos_title_init_reference_events(contents, diagnostics, error)) {
        if (stream.bad()) error = "Unable to read reference trace events";
        return false;
    }
    try {
        if (to_hex(sha256_file(path)) != expected_sha256) {
            error = "Reference trace events changed while it was being validated";
            return false;
        }
    } catch (const std::exception&) {
        error = "Unable to rehash reference trace events";
        return false;
    }
    return true;
}

bool validate_deuteros_atari_events(const std::filesystem::path& path,
                                    const std::uintmax_t expected_size,
                                    const std::string_view expected_sha256,
                                    DeuterosAtariReferenceTraceDiagnostics& diagnostics,
                                    std::string& error) {
    std::uintmax_t observed_size = 0;
    if (!regular_file_size(path, maximum_events_size, observed_size, error)) return false;
    if (observed_size != expected_size) {
        error = "Reference trace events size does not match its manifest";
        return false;
    }
    try {
        if (to_hex(sha256_file(path)) != expected_sha256) {
            error = "Reference trace events SHA-256 does not match its manifest";
            return false;
        }
    } catch (const std::exception&) {
        error = "Unable to hash reference trace events";
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "Unable to read reference trace events";
        return false;
    }
    const std::string contents((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (stream.bad()) {
        error = "Unable to read reference trace events";
        return false;
    }
    if (!validate_deuteros_atari_reference_events(contents, diagnostics, error)) return false;
    try {
        if (to_hex(sha256_file(path)) != expected_sha256) {
            error = "Reference trace events changed while it was being validated";
            return false;
        }
    } catch (const std::exception&) {
        error = "Unable to rehash reference trace events";
        return false;
    }
    return true;
}

bool validate_deuteros_amiga_events(const std::filesystem::path& path,
                                    const std::uintmax_t expected_size,
                                    const std::string_view expected_sha256,
                                    DeuterosAmigaReferenceTraceDiagnostics& diagnostics,
                                    std::string& error) {
    std::uintmax_t observed_size = 0;
    if (!regular_file_size(path, maximum_events_size, observed_size, error)) return false;
    if (observed_size != expected_size) {
        error = "Reference trace events size does not match its manifest";
        return false;
    }
    try {
        if (to_hex(sha256_file(path)) != expected_sha256) {
            error = "Reference trace events SHA-256 does not match its manifest";
            return false;
        }
    } catch (const std::exception&) {
        error = "Unable to hash reference trace events";
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "Unable to read reference trace events";
        return false;
    }
    const std::string contents((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    // Reading through istreambuf_iterator may finish with a clean stream on
    // some standard-library implementations, so only a real I/O failure
    // rejects this already bounded and hashed file.
    if (stream.bad()) {
        error = "Unable to read reference trace events";
        return false;
    }
    if (!validate_deuteros_amiga_title_reference_events(contents, diagnostics, error)) return false;
    try {
        if (to_hex(sha256_file(path)) != expected_sha256) {
            error = "Reference trace events changed while it was being validated";
            return false;
        }
    } catch (const std::exception&) {
        error = "Unable to rehash reference trace events";
        return false;
    }
    return true;
}

bool validate_deuteros_amiga_title_bridge_events(
    const std::filesystem::path& path, const std::uintmax_t expected_size,
    const std::string_view expected_sha256,
    DeuterosAmigaTitleBridgeReferenceTraceDiagnostics& diagnostics, std::string& error) {
    std::uintmax_t observed_size = 0;
    if (!regular_file_size(path, maximum_events_size, observed_size, error)) return false;
    if (observed_size != expected_size) {
        error = "Reference trace events size does not match its manifest";
        return false;
    }
    try {
        if (to_hex(sha256_file(path)) != expected_sha256) {
            error = "Reference trace events SHA-256 does not match its manifest";
            return false;
        }
    } catch (const std::exception&) {
        error = "Unable to hash reference trace events";
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "Unable to read reference trace events";
        return false;
    }
    const std::string contents((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (stream.bad()) {
        error = "Unable to read reference trace events";
        return false;
    }
    if (!validate_deuteros_amiga_title_bridge_reference_events(contents, diagnostics, error)) return false;
    try {
        if (to_hex(sha256_file(path)) != expected_sha256) {
            error = "Reference trace events changed while it was being validated";
            return false;
        }
    } catch (const std::exception&) {
        error = "Unable to rehash reference trace events";
        return false;
    }
    return true;
}

bool validate_deuteros_amiga_title_display_events(
    const std::filesystem::path& path, const std::uintmax_t expected_size,
    const std::string_view expected_sha256,
    const std::string_view expected_input_timeline_sha256,
    DeuterosAmigaTitleDisplayReferenceTraceDiagnostics& diagnostics, std::string& error) {
    std::uintmax_t observed_size = 0;
    if (!regular_file_size(path, maximum_events_size, observed_size, error)) return false;
    if (observed_size != expected_size) {
        error = "Reference trace events size does not match its manifest";
        return false;
    }
    try {
        if (to_hex(sha256_file(path)) != expected_sha256) {
            error = "Reference trace events SHA-256 does not match its manifest";
            return false;
        }
    } catch (const std::exception&) {
        error = "Unable to hash reference trace events";
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "Unable to read reference trace events";
        return false;
    }
    const std::string contents((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (stream.bad()
        || !validate_deuteros_amiga_title_display_reference_events(
            contents, diagnostics, error, expected_input_timeline_sha256)) {
        if (stream.bad()) error = "Unable to read reference trace events";
        return false;
    }
    try {
        if (to_hex(sha256_file(path)) != expected_sha256) {
            error = "Reference trace events changed while it was being validated";
            return false;
        }
    } catch (const std::exception&) {
        error = "Unable to rehash reference trace events";
        return false;
    }
    return true;
}

bool validate_deuteros_amiga_main_stage_events(
    const std::filesystem::path& path, const std::uintmax_t expected_size,
    const std::string_view expected_sha256,
    DeuterosAmigaMainStageReferenceTraceDiagnostics& diagnostics, std::string& error) {
    std::uintmax_t observed_size = 0;
    if (!regular_file_size(path, maximum_events_size, observed_size, error)) return false;
    if (observed_size != expected_size) {
        error = "Reference trace events size does not match its manifest";
        return false;
    }
    try {
        if (to_hex(sha256_file(path)) != expected_sha256) {
            error = "Reference trace events SHA-256 does not match its manifest";
            return false;
        }
    } catch (const std::exception&) {
        error = "Unable to hash reference trace events";
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "Unable to read reference trace events";
        return false;
    }
    const std::string contents((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (stream.bad()
        || !validate_deuteros_amiga_main_stage_reference_events(contents, diagnostics, error)) {
        if (stream.bad()) error = "Unable to read reference trace events";
        return false;
    }
    try {
        if (to_hex(sha256_file(path)) != expected_sha256) {
            error = "Reference trace events changed while it was being validated";
            return false;
        }
    } catch (const std::exception&) {
        error = "Unable to rehash reference trace events";
        return false;
    }
    return true;
}

bool validate_millennium_amiga_events(const std::filesystem::path& path,
                                      const std::uintmax_t expected_size,
                                      const std::string_view expected_sha256,
                                      MillenniumAmigaReferenceTraceDiagnostics& diagnostics,
                                      std::string& error) {
    std::uintmax_t observed_size = 0;
    if (!regular_file_size(path, maximum_events_size, observed_size, error)) return false;
    if (observed_size != expected_size) {
        error = "Reference trace events size does not match its manifest";
        return false;
    }
    try {
        if (to_hex(sha256_file(path)) != expected_sha256) {
            error = "Reference trace events SHA-256 does not match its manifest";
            return false;
        }
    } catch (const std::exception&) {
        error = "Unable to hash reference trace events";
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "Unable to read reference trace events";
        return false;
    }
    const std::string contents((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    // See the corresponding DOS adapter reader above.
    if (stream.bad()) {
        error = "Unable to read reference trace events";
        return false;
    }
    if (!validate_millennium_amiga_english_reference_events(contents, diagnostics, error)) return false;
    try {
        if (to_hex(sha256_file(path)) != expected_sha256) {
            error = "Reference trace events changed while it was being validated";
            return false;
        }
    } catch (const std::exception&) {
        error = "Unable to rehash reference trace events";
        return false;
    }
    return true;
}

bool hexadecimal_u64(const std::string_view value, std::uint64_t& result) {
    if (value.size() < 3 || value.substr(0, 2) != "0x") return false;
    const auto parsed = std::from_chars(value.data() + 2, value.data() + value.size(), result, 16);
    return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
}

bool validate_title_display_artifact(const std::filesystem::path& manifest_path,
    const std::map<std::string, std::string>& fields, const std::string_view key,
    const std::string_view expected_filename, const std::uint64_t expected_size,
    const std::string_view expected_sha256, const std::string_view format,
    const std::uintmax_t maximum_size, std::set<std::filesystem::path>& claimed_paths,
    std::vector<ReferenceTraceArtifact>& artifacts, std::string& error) {
    const std::string field_prefix(key);
    const auto& filename = fields.at(field_prefix + "_file");
    const auto& declared_size = fields.at(field_prefix + "_size");
    const auto& declared_sha256 = key == "input_timeline"
        ? fields.at("input_timeline_sha256") : fields.at(field_prefix + "_sha256");
    std::uint64_t size = 0;
    if (filename != expected_filename || !basename_only(filename)
        || !decimal_u64(declared_size, size) || size != expected_size
        || !lowercase_sha256(declared_sha256) || declared_sha256 != expected_sha256) {
        error = "Deuteros title-display v5 artifact manifest fields are invalid";
        return false;
    }
    const auto path = (manifest_path.parent_path() / filename).lexically_normal();
    if (!claimed_paths.insert(path).second) {
        error = "Deuteros title-display v5 artifacts must have distinct sibling paths";
        return false;
    }
    std::uintmax_t observed_size = 0;
    if (!regular_file_size(path, maximum_size, observed_size, error)) return false;
    if (observed_size != size) {
        error = "Deuteros title-display v5 artifact size does not match its manifest";
        return false;
    }
    try {
        if (to_hex(sha256_file(path)) != declared_sha256) {
            error = "Deuteros title-display v5 artifact SHA-256 does not match its manifest";
            return false;
        }
    } catch (const std::exception&) {
        error = "Unable to hash Deuteros title-display v5 artifact";
        return false;
    }
    artifacts.push_back({std::string(key), path, size, declared_sha256, std::string(format)});
    return true;
}

bool validate_deuteros_amiga_title_display_artifacts_v5(const std::filesystem::path& manifest_path,
    const std::map<std::string, std::string>& fields,
    const DeuterosAmigaTitleDisplayReferenceTraceDiagnostics& diagnostics,
    std::vector<ReferenceTraceArtifact>& artifacts, std::string& error) {
    // The title-display event parser has already bound its fixed v4 prefix.
    // This v5 layer only proves that capture-side bytes actually exist and
    // match those opaque checkpoints; it never opens them in the runtime.
    std::uint64_t input_size = 0;
    std::uint64_t pcm_size = 0;
    std::uint64_t channels = 0;
    std::uint64_t sample_frames = 0;
    if (!decimal_u64(fields.at("input_timeline_size"), input_size) || input_size == 0
        || input_size > 1024U * 1024U
        || !decimal_u64(fields.at("pcm_size"), pcm_size) || pcm_size == 0
        || pcm_size > 8U * 1024U * 1024U
        || !hexadecimal_u64(diagnostics.audio_channels, channels)
        || !hexadecimal_u64(diagnostics.audio_sample_frames, sample_frames)
        || channels == 0 || sample_frames == 0
        || channels > std::numeric_limits<std::uint64_t>::max() / sample_frames
        || channels * sample_frames > std::numeric_limits<std::uint64_t>::max() / 2U
        || pcm_size != channels * sample_frames * 2U) {
        error = "Deuteros title-display v5 audio or input artifact dimensions are invalid";
        return false;
    }
    const bool cross_bound = fields.at("copper_list_sha256") == diagnostics.copper_list_sha256
        && fields.at("rgb4_palette_sha256") == diagnostics.rgb4_palette_sha256
        && fields.at("bitplanes_sha256") == diagnostics.bitplanes_sha256
        && fields.at("rgba_palette_sha256") == diagnostics.rgba_palette_sha256
        && fields.at("rgba_frame_sha256") == diagnostics.rgba_sha256
        && fields.at("pcm_sha256") == diagnostics.pcm_sha256;
    if (!cross_bound) {
        error = "Deuteros title-display v5 artifacts do not match their event checkpoints";
        return false;
    }
    std::set<std::filesystem::path> claimed_paths;
    claimed_paths.insert(manifest_path.lexically_normal());
    const auto events_path = (manifest_path.parent_path() / fields.at("event_file")).lexically_normal();
    claimed_paths.insert(events_path);
    artifacts.clear();
    return validate_title_display_artifact(manifest_path, fields, "input_timeline", "input-timeline.txt",
               input_size, fields.at("input_timeline_sha256"), "host-input-timeline", 1024U * 1024U,
               claimed_paths, artifacts, error)
        && validate_title_display_artifact(manifest_path, fields, "copper_list", "copper-list.bin", 88,
               diagnostics.copper_list_sha256, "amiga-copper-words", 88, claimed_paths, artifacts, error)
        && validate_title_display_artifact(manifest_path, fields, "rgb4_palette", "palette-rgb4.bin", 40,
               diagnostics.rgb4_palette_sha256, "amiga-rgb4-words", 40, claimed_paths, artifacts, error)
        && validate_title_display_artifact(manifest_path, fields, "bitplanes", "bitplanes.bin", 32000,
               diagnostics.bitplanes_sha256, "amiga-4-plane-320x200", 32000, claimed_paths, artifacts, error)
        && validate_title_display_artifact(manifest_path, fields, "rgba_palette", "palette-rgba8888.bin", 80,
               diagnostics.rgba_palette_sha256, "rgba8888-rgb4-expanded-nibbles", 80, claimed_paths, artifacts, error)
        && validate_title_display_artifact(manifest_path, fields, "rgba_frame", "frame-rgba8888.bin", 256000,
               diagnostics.rgba_sha256, "rgba8888-row-major-320x200", 256000, claimed_paths, artifacts, error)
        && validate_title_display_artifact(manifest_path, fields, "pcm", "audio-s16le.bin", pcm_size,
               diagnostics.pcm_sha256, "s16le-interleaved", 8U * 1024U * 1024U, claimed_paths, artifacts, error);
}

} // namespace

ReferenceTraceValidation validate_reference_trace(
    const std::filesystem::path& manifest_path,
    const std::vector<ReleaseArchive>& releases,
    const Game requested_game,
    const Platform requested_platform) {
    if (manifest_path.extension() != ".eontrace") {
        return {{}, "Reference trace manifest must use the .eontrace extension"};
    }
    std::map<std::string, std::string> fields;
    std::string error;
    if (!parse_key_value_file(manifest_path, maximum_manifest_size, fields, error)) return {{}, error};
    constexpr std::array v1_required_fields{
        "format", "event_file", "event_size", "event_sha256", "game", "platform", "language",
        "source_release_sha256", "source_release_size", "capture_start_utc", "capture_end_utc",
        "emulator_name", "emulator_version", "emulator_sha256", "config_sha256",
        "command_tail_sha256", "input_timeline_sha256"};
    constexpr std::array v2_required_fields{
        "format", "adapter", "event_file", "event_size", "event_sha256", "game", "platform", "language",
        "source_release_sha256", "source_release_size", "capture_start_utc", "capture_end_utc",
        "emulator_name", "emulator_version", "emulator_sha256", "config_sha256",
        "command_tail_sha256", "input_timeline_sha256"};
    constexpr std::array deuteros_atari_v2_required_fields{
        "format", "adapter", "event_file", "event_size", "event_sha256", "game", "platform", "language",
        "source_release_sha256", "source_release_size", "source_media_sha256", "source_stage_sha256",
        "capture_start_utc", "capture_end_utc", "emulator_name", "emulator_version", "emulator_sha256",
        "config_sha256", "command_tail_sha256", "input_timeline_sha256"};
    constexpr std::array deuteros_amiga_v2_required_fields{
        "format", "adapter", "event_file", "event_size", "event_sha256", "game", "platform", "language",
        "source_release_sha256", "source_release_size", "source_media_sha256", "source_stage_sha256",
        "capture_start_utc", "capture_end_utc", "emulator_name", "emulator_version", "emulator_sha256",
        "config_sha256", "command_tail_sha256", "input_timeline_sha256"};
    constexpr std::array deuteros_amiga_title_display_v5_required_fields{
        "format", "adapter", "event_file", "event_size", "event_sha256", "game", "platform", "language",
        "source_release_sha256", "source_release_size", "source_media_sha256", "source_stage_sha256",
        "capture_start_utc", "capture_end_utc", "emulator_name", "emulator_version", "emulator_sha256",
        "config_sha256", "command_tail_sha256", "input_timeline_sha256", "input_timeline_file", "input_timeline_size",
        "copper_list_file", "copper_list_size", "copper_list_sha256",
        "rgb4_palette_file", "rgb4_palette_size", "rgb4_palette_sha256",
        "bitplanes_file", "bitplanes_size", "bitplanes_sha256",
        "rgba_palette_file", "rgba_palette_size", "rgba_palette_sha256",
        "rgba_frame_file", "rgba_frame_size", "rgba_frame_sha256",
        "pcm_file", "pcm_size", "pcm_sha256"};
    const bool v1 = fields.contains("format") && fields.at("format") == "project-eon-reference-trace-v1";
    const bool millennium_dos_v2 = fields.contains("format")
        && fields.at("format") == "project-eon-reference-trace-v2"
        && fields.contains("adapter") && fields.at("adapter") == "millennium-dos-en-startup-v1";
    const bool millennium_dos_gx_v2 = fields.contains("format")
        && fields.at("format") == "project-eon-reference-trace-v2"
        && fields.contains("adapter") && fields.at("adapter") == "millennium-dos-en-gx-startup-v2";
    const bool millennium_dos_title_init_v2 = fields.contains("format")
        && fields.at("format") == "project-eon-reference-trace-v2"
        && fields.contains("adapter") && fields.at("adapter") == "millennium-dos-en-title-init-v2";
    const bool deuteros_atari_v2 = fields.contains("format")
        && fields.at("format") == "project-eon-reference-trace-v2"
        && fields.contains("adapter") && fields.at("adapter") == "deuteros-atari-st-boot-v1";
    const bool millennium_amiga_v2 = fields.contains("format")
        && fields.at("format") == "project-eon-reference-trace-v2"
        && fields.contains("adapter") && fields.at("adapter") == "millennium-amiga-en-defjam-bootstrap-v1";
    const bool deuteros_amiga_v2 = fields.contains("format")
        && fields.at("format") == "project-eon-reference-trace-v2"
        && fields.contains("adapter") && fields.at("adapter") == "deuteros-amiga-en-title-stage-v1";
    const bool deuteros_amiga_title_bridge_v3 = fields.contains("format")
        && fields.at("format") == "project-eon-reference-trace-v3"
        && fields.contains("adapter") && fields.at("adapter") == "deuteros-amiga-en-title-bridge-v3";
    const bool deuteros_amiga_main_stage_v3 = fields.contains("format")
        && fields.at("format") == "project-eon-reference-trace-v3"
        && fields.contains("adapter") && fields.at("adapter") == "deuteros-amiga-en-main-copy-loop-v3";
    const bool deuteros_amiga_title_display_v4 = fields.contains("format")
        && fields.at("format") == "project-eon-reference-trace-v4"
        && fields.contains("adapter") && fields.at("adapter") == "deuteros-amiga-en-title-display-v4";
    const bool deuteros_amiga_title_display_v5 = fields.contains("format")
        && fields.at("format") == "project-eon-reference-trace-v5"
        && fields.contains("adapter") && fields.at("adapter") == "deuteros-amiga-en-title-display-artifacts-v5";
    const auto manifest_has_exact_fields = [&fields](const auto& required) {
        if (fields.size() != required.size()) return false;
        return std::all_of(required.begin(), required.end(), [&fields](const auto field) {
            return fields.contains(field);
        });
    };
    if (!(v1 ? manifest_has_exact_fields(v1_required_fields)
             : (millennium_dos_v2 || millennium_dos_gx_v2 || millennium_dos_title_init_v2)
                 ? manifest_has_exact_fields(v2_required_fields)
             : deuteros_atari_v2 ? manifest_has_exact_fields(deuteros_atari_v2_required_fields)
             : deuteros_amiga_v2 ? manifest_has_exact_fields(deuteros_amiga_v2_required_fields)
             : (deuteros_amiga_title_bridge_v3 || deuteros_amiga_main_stage_v3
                 || deuteros_amiga_title_display_v4) ? manifest_has_exact_fields(deuteros_amiga_v2_required_fields)
             : deuteros_amiga_title_display_v5
                 ? manifest_has_exact_fields(deuteros_amiga_title_display_v5_required_fields)
             : millennium_amiga_v2 ? manifest_has_exact_fields(v2_required_fields)
             : false)) {
        return {{}, "Reference trace manifest has unknown or missing fields"};
    }
    if ((!v1 && fields.at("format") != "project-eon-reference-trace-v2"
            && fields.at("format") != "project-eon-reference-trace-v3"
            && fields.at("format") != "project-eon-reference-trace-v4"
            && fields.at("format") != "project-eon-reference-trace-v5")
        || !basename_only(fields.at("event_file"))
        || !lowercase_sha256(fields.at("event_sha256"))
        || !lowercase_sha256(fields.at("source_release_sha256"))
        || !lowercase_sha256(fields.at("emulator_sha256"))
        || !lowercase_sha256(fields.at("config_sha256"))
        || !lowercase_sha256(fields.at("command_tail_sha256"))
        || !lowercase_sha256(fields.at("input_timeline_sha256"))
        || ((deuteros_atari_v2 || deuteros_amiga_v2 || deuteros_amiga_title_bridge_v3
             || deuteros_amiga_main_stage_v3 || deuteros_amiga_title_display_v4
             || deuteros_amiga_title_display_v5)
            && (!lowercase_sha256(fields.at("source_media_sha256"))
            || !lowercase_sha256(fields.at("source_stage_sha256"))))
        || !utc_timestamp(fields.at("capture_start_utc"))
        || !utc_timestamp(fields.at("capture_end_utc"))
        || fields.at("capture_end_utc") < fields.at("capture_start_utc")) {
        return {{}, "Reference trace manifest has invalid versioned values"};
    }
    const auto game = game_from_trace(fields.at("game"));
    const auto platform = platform_from_trace(fields.at("platform"));
    std::uint64_t event_size = 0;
    std::uint64_t release_size = 0;
    if (!game || !platform || !ascii_printable(fields.at("language"))
        || !decimal_u64(fields.at("event_size"), event_size)
        || !decimal_u64(fields.at("source_release_size"), release_size)
        || event_size > maximum_events_size) {
        return {{}, "Reference trace manifest has invalid release or event identity"};
    }
    if (*game != requested_game || *platform != requested_platform) {
        return {{}, "Reference trace does not match the requested game and platform"};
    }
    const auto source = std::find_if(releases.begin(), releases.end(), [&](const auto& release) {
        std::error_code filesystem_error;
        const auto size = std::filesystem::file_size(release.path, filesystem_error);
        return !filesystem_error && release.game == *game && release.platform == *platform
            && release.language == fields.at("language")
            && release.sha256 == fields.at("source_release_sha256") && size == release_size;
    });
    if (source == releases.end()) {
        return {{}, "Reference trace source release is not a recognised supplied archive"};
    }
    if (millennium_dos_v2 && (source->game != Game::millennium || source->platform != Platform::dos
            || source->language != "en"
            || source->sha256 != "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123")) {
        return {{}, "Reference trace adapter does not match the clean English Millennium DOS release"};
    }
    if (millennium_dos_gx_v2 && (source->game != Game::millennium || source->platform != Platform::dos
            || source->language != "en"
            || source->sha256 != "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123")) {
        return {{}, "Reference trace GX adapter does not match the clean English Millennium DOS release"};
    }
    if (millennium_dos_title_init_v2 && (source->game != Game::millennium || source->platform != Platform::dos
            || source->language != "en"
            || source->sha256 != "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123")) {
        return {{}, "Reference trace title-init adapter does not match the clean English Millennium DOS release"};
    }
    if (deuteros_atari_v2 && (source->game != Game::deuteros || source->platform != Platform::atari_st
            || source->language != "en"
            || source->sha256 != "c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653"
            || fields.at("source_media_sha256") != "aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee"
            || fields.at("source_stage_sha256") != "2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7")) {
        return {{}, "Reference trace adapter does not match the exact Deuteros Atari ST boot media"};
    }
    if (millennium_amiga_v2 && (source->game != Game::millennium || source->platform != Platform::amiga
            || source->language != "en"
            || source->sha256 != "2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400")) {
        return {{}, "Reference trace adapter does not match the clean English Millennium Amiga release"};
    }
    if (deuteros_amiga_v2 && (source->game != Game::deuteros || source->platform != Platform::amiga
            || source->language != "en"
            || source->sha256 != "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04"
            || fields.at("source_media_sha256") != "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38"
            || fields.at("source_stage_sha256") != "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03")) {
        return {{}, "Reference trace adapter does not match the exact Deuteros Amiga title-stage media"};
    }
    if (deuteros_amiga_title_bridge_v3
            && (source->game != Game::deuteros || source->platform != Platform::amiga
                || source->language != "en"
                || source->sha256 != "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04"
                || fields.at("source_media_sha256") != "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38"
                || fields.at("source_stage_sha256") != "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03")) {
        return {{}, "Reference trace adapter does not match the exact Deuteros Amiga title-bridge media"};
    }
    if (deuteros_amiga_main_stage_v3
            && (source->game != Game::deuteros || source->platform != Platform::amiga
                || source->language != "en"
                || source->sha256 != "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04"
                || fields.at("source_media_sha256") != "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38"
                || fields.at("source_stage_sha256") != "a82c0d6a12e156e0832d632a6c40dd58713a00b611dbcba7289aa16b0969a0a6")) {
        return {{}, "Reference trace adapter does not match the exact Deuteros Amiga main-stage media"};
    }
    if ((deuteros_amiga_title_display_v4 || deuteros_amiga_title_display_v5)
            && (source->game != Game::deuteros || source->platform != Platform::amiga
                || source->language != "en"
                || source->sha256 != "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04"
                || fields.at("source_media_sha256") != "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38"
                || fields.at("source_stage_sha256") != "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03")) {
        return {{}, "Reference trace adapter does not match the exact Deuteros Amiga title-display media"};
    }
    try {
        if (to_hex(sha256_file(source->path)) != source->sha256) {
            return {{}, "Reference trace source release changed after the provenance scan"};
        }
    } catch (const std::exception&) {
        return {{}, "Unable to rehash reference trace source release"};
    }
    const auto events_path = manifest_path.parent_path() / fields.at("event_file");
    if (events_path.extension() != ".eontrace" || events_path.lexically_normal()
            == manifest_path.lexically_normal()) {
        return {{}, "Reference trace events must be a distinct .eontrace sibling"};
    }
    std::size_t event_count = 0;
    MillenniumDosEnglishReferenceTraceDiagnostics diagnostics;
    MillenniumDosGxStartupReferenceTraceDiagnostics millennium_dos_gx_diagnostics;
    MillenniumDosTitleInitReferenceTraceDiagnostics millennium_dos_title_init_diagnostics;
    DeuterosAtariReferenceTraceDiagnostics deuteros_diagnostics;
    MillenniumAmigaReferenceTraceDiagnostics amiga_diagnostics;
    DeuterosAmigaReferenceTraceDiagnostics deuteros_amiga_diagnostics;
    DeuterosAmigaTitleBridgeReferenceTraceDiagnostics deuteros_amiga_title_bridge_diagnostics;
    DeuterosAmigaMainStageReferenceTraceDiagnostics deuteros_amiga_main_stage_diagnostics;
    DeuterosAmigaTitleDisplayReferenceTraceDiagnostics deuteros_amiga_title_display_diagnostics;
    const bool events_valid = v1
        ? validate_events(events_path, event_size, fields.at("event_sha256"), event_count, error)
        : millennium_dos_v2
            ? validate_millennium_dos_english_events(events_path, event_size, fields.at("event_sha256"),
                diagnostics, error)
            : millennium_dos_gx_v2
                ? validate_millennium_dos_gx_startup_events(events_path, event_size, fields.at("event_sha256"),
                    millennium_dos_gx_diagnostics, error)
            : millennium_dos_title_init_v2
                ? validate_millennium_dos_title_init_events(events_path, event_size, fields.at("event_sha256"),
                    millennium_dos_title_init_diagnostics, error)
            : deuteros_atari_v2
                ? validate_deuteros_atari_events(events_path, event_size, fields.at("event_sha256"),
                    deuteros_diagnostics, error)
                : deuteros_amiga_v2
                    ? validate_deuteros_amiga_events(events_path, event_size, fields.at("event_sha256"),
                        deuteros_amiga_diagnostics, error)
                    : (deuteros_amiga_title_display_v4 || deuteros_amiga_title_display_v5)
                        ? validate_deuteros_amiga_title_display_events(events_path, event_size,
                            fields.at("event_sha256"), fields.at("input_timeline_sha256"),
                            deuteros_amiga_title_display_diagnostics, error)
                    : deuteros_amiga_title_bridge_v3
                        ? validate_deuteros_amiga_title_bridge_events(events_path, event_size,
                            fields.at("event_sha256"), deuteros_amiga_title_bridge_diagnostics, error)
                        : deuteros_amiga_main_stage_v3
                            ? validate_deuteros_amiga_main_stage_events(events_path, event_size,
                                fields.at("event_sha256"), deuteros_amiga_main_stage_diagnostics, error)
                        : validate_millennium_amiga_events(events_path, event_size, fields.at("event_sha256"),
                            amiga_diagnostics, error);
    if (!events_valid) {
        return {{}, error};
    }
    std::vector<ReferenceTraceArtifact> artifacts;
    if (deuteros_amiga_title_display_v5
        && !validate_deuteros_amiga_title_display_artifacts_v5(manifest_path, fields,
            deuteros_amiga_title_display_diagnostics, artifacts, error)) {
        return {{}, error};
    }
    std::vector<ReferenceTraceBoundary> recovery_boundaries;
    if (!trace_recovery_boundaries(v1 ? std::string_view{} : std::string_view(fields.at("adapter")),
            source->sha256, recovery_boundaries, error)) {
        return {{}, error};
    }
    if (millennium_dos_v2) event_count = diagnostics.event_count;
    if (millennium_dos_gx_v2) event_count = millennium_dos_gx_diagnostics.event_count;
    if (millennium_dos_title_init_v2) event_count = millennium_dos_title_init_diagnostics.event_count;
    if (deuteros_atari_v2) event_count = deuteros_diagnostics.event_count;
    if (millennium_amiga_v2) event_count = amiga_diagnostics.event_count;
    if (deuteros_amiga_v2) event_count = deuteros_amiga_diagnostics.event_count;
    if (deuteros_amiga_title_bridge_v3) event_count = deuteros_amiga_title_bridge_diagnostics.event_count;
    if (deuteros_amiga_main_stage_v3) event_count = deuteros_amiga_main_stage_diagnostics.event_count;
    if (deuteros_amiga_title_display_v4 || deuteros_amiga_title_display_v5) {
        event_count = deuteros_amiga_title_display_diagnostics.event_count;
    }
    return {ReferenceTrace{manifest_path, events_path, *source,
        fields.at("capture_start_utc"), fields.at("capture_end_utc"), fields.at("emulator_name"),
        fields.at("emulator_version"), fields.at("emulator_sha256"), fields.at("config_sha256"),
        fields.at("command_tail_sha256"), fields.at("input_timeline_sha256"), fields.at("format"),
        v1 ? "" : fields.at("adapter"), event_count, event_size, fields.at("event_sha256"),
        (deuteros_atari_v2 || deuteros_amiga_v2 || deuteros_amiga_title_bridge_v3
            || deuteros_amiga_main_stage_v3 || deuteros_amiga_title_display_v4
            || deuteros_amiga_title_display_v5)
            ? fields.at("source_media_sha256") : "",
        (deuteros_atari_v2 || deuteros_amiga_v2 || deuteros_amiga_title_bridge_v3
            || deuteros_amiga_main_stage_v3 || deuteros_amiga_title_display_v4
            || deuteros_amiga_title_display_v5)
            ? fields.at("source_stage_sha256") : "",
        std::move(recovery_boundaries),
        millennium_dos_title_init_v2 ? millennium_dos_title_init_diagnostics.interrupt_count
            : diagnostics.interrupt_count,
        millennium_dos_title_init_v2 ? millennium_dos_title_init_diagnostics.file_count
            : diagnostics.file_count,
        millennium_dos_v2 ? diagnostics.exec_count : deuteros_amiga_diagnostics.exec_count,
        millennium_dos_title_init_v2 ? millennium_dos_title_init_diagnostics.private_return_count
            : millennium_dos_gx_v2 ? millennium_dos_gx_diagnostics.private_return_count : 0,
        deuteros_diagnostics.trap_count,
        deuteros_atari_v2 ? deuteros_diagnostics.callback_count : deuteros_amiga_diagnostics.callback_count,
        deuteros_diagnostics.frame_count, deuteros_diagnostics.state_count,
        deuteros_diagnostics.table_count, deuteros_diagnostics.raw_reader_count,
        amiga_diagnostics.cpu_count, deuteros_amiga_diagnostics.open_library_count,
        deuteros_amiga_diagnostics.graphics_count, deuteros_amiga_diagnostics.custom_register_count,
        deuteros_amiga_title_bridge_diagnostics.exec_return_count,
        deuteros_amiga_title_bridge_diagnostics.open_library_return_count,
        deuteros_amiga_title_bridge_diagnostics.graphics_call_count,
        deuteros_amiga_title_bridge_diagnostics.graphics_return_count,
        deuteros_amiga_title_bridge_diagnostics.custom_register_call_count,
        deuteros_amiga_title_bridge_diagnostics.custom_register_return_count,
        deuteros_amiga_title_bridge_diagnostics.callback_registration_return_count,
        deuteros_amiga_title_bridge_diagnostics.queue_snapshot_count,
        deuteros_amiga_title_bridge_diagnostics.callback_entry_count,
        deuteros_amiga_title_bridge_diagnostics.selector_entry_count,
        deuteros_amiga_title_bridge_diagnostics.local_call_count,
        millennium_dos_gx_v2 ? millennium_dos_gx_diagnostics.local_return_count
            : deuteros_amiga_title_bridge_diagnostics.local_return_count,
        deuteros_amiga_title_bridge_diagnostics.dispatch_snapshot_count,
        millennium_dos_gx_v2 ? millennium_dos_gx_diagnostics.mode_read_count : 0,
        millennium_dos_gx_v2 ? millennium_dos_gx_diagnostics.adapter_return_count : 0,
        (deuteros_amiga_title_display_v4 || deuteros_amiga_title_display_v5) ? deuteros_amiga_title_display_diagnostics.display_layout_count : 0,
        (deuteros_amiga_title_display_v4 || deuteros_amiga_title_display_v5) ? deuteros_amiga_title_display_diagnostics.bitplane_layout_count : 0,
        (deuteros_amiga_title_display_v4 || deuteros_amiga_title_display_v5) ? deuteros_amiga_title_display_diagnostics.palette_checkpoint_count : 0,
        (deuteros_amiga_title_display_v4 || deuteros_amiga_title_display_v5) ? deuteros_amiga_title_display_diagnostics.input_checkpoint_count : 0,
        (deuteros_amiga_title_display_v4 || deuteros_amiga_title_display_v5) ? deuteros_amiga_title_display_diagnostics.frame_checkpoint_count : 0,
        (deuteros_amiga_title_display_v4 || deuteros_amiga_title_display_v5) ? deuteros_amiga_title_display_diagnostics.audio_checkpoint_count : 0,
        std::move(artifacts)}, {}};
}

} // namespace eon
