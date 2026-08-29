#include "data/reference_trace.hpp"

#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <exception>
#include <fstream>
#include <limits>
#include <map>
#include <string_view>
#include <system_error>

namespace eon {
namespace {

constexpr std::uintmax_t maximum_manifest_size = 16U * 1024U * 1024U;
constexpr std::uintmax_t maximum_events_size = 256U * 1024U * 1024U;
constexpr std::size_t maximum_line_size = 4096;

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
    return true;
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
                return (character >= 'a' && character <= 'z') || character == '_';
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
    constexpr std::array required_fields{
        "format", "event_file", "event_size", "event_sha256", "game", "platform", "language",
        "source_release_sha256", "source_release_size", "capture_start_utc", "capture_end_utc",
        "emulator_name", "emulator_version", "emulator_sha256", "config_sha256",
        "command_tail_sha256", "input_timeline_sha256"};
    if (fields.size() != required_fields.size()) {
        return {{}, "Reference trace manifest has unknown or missing fields"};
    }
    for (const auto field : required_fields) {
        if (!fields.contains(field)) return {{}, "Reference trace manifest has unknown or missing fields"};
    }
    if (fields.at("format") != "project-eon-reference-trace-v1"
        || !basename_only(fields.at("event_file"))
        || !lowercase_sha256(fields.at("event_sha256"))
        || !lowercase_sha256(fields.at("source_release_sha256"))
        || !lowercase_sha256(fields.at("emulator_sha256"))
        || !lowercase_sha256(fields.at("config_sha256"))
        || !lowercase_sha256(fields.at("command_tail_sha256"))
        || !lowercase_sha256(fields.at("input_timeline_sha256"))
        || !utc_timestamp(fields.at("capture_start_utc"))
        || !utc_timestamp(fields.at("capture_end_utc"))
        || fields.at("capture_end_utc") < fields.at("capture_start_utc")) {
        return {{}, "Reference trace manifest has invalid v1 values"};
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
    if (!validate_events(events_path, event_size, fields.at("event_sha256"), event_count, error)) {
        return {{}, error};
    }
    return {ReferenceTrace{manifest_path, events_path, *source,
        fields.at("capture_start_utc"), fields.at("capture_end_utc"), fields.at("emulator_name"),
        fields.at("emulator_version"), fields.at("emulator_sha256"), event_count, event_size,
        fields.at("event_sha256")}, {}};
}

} // namespace eon
