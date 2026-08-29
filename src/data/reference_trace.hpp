#pragma once

#include "platform/game_data.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace eon {

// An external reference trace is preservation evidence, not a save state or
// an instruction stream for the runtime.  Version 1 deliberately retains
// only identity and event ordering: Project Eon validates it and reports its
// provenance, but never replays it or derives game behaviour from it.
struct ReferenceTrace {
    std::filesystem::path manifest_path;
    std::filesystem::path events_path;
    ReleaseArchive source_release;
    std::string capture_start_utc;
    std::string capture_end_utc;
    std::string emulator_name;
    std::string emulator_version;
    std::string emulator_sha256;
    std::string format;
    std::string adapter;
    std::size_t event_count = 0;
    std::uint64_t event_size = 0;
    std::string event_sha256;
    std::size_t adapter_interrupt_count = 0;
    std::size_t adapter_file_count = 0;
    std::size_t adapter_exec_count = 0;
};

struct ReferenceTraceValidation {
    std::optional<ReferenceTrace> trace;
    std::string error;
};

// Read and validate one user-supplied v1 or narrowly declared v2 manifest. The manifest names its
// sibling events file by basename only, pins it by size/SHA-256, and pins its
// source release to one of the scanner's recognised ReleaseArchive objects.
// Both files are bounded, ASCII/LF-only, and are left untouched.
[[nodiscard]] ReferenceTraceValidation validate_reference_trace(
    const std::filesystem::path& manifest_path,
    const std::vector<ReleaseArchive>& releases,
    Game requested_game,
    Platform requested_platform);

} // namespace eon
