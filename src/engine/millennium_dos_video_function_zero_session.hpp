#pragma once

#include "data/millennium_dos_video_driver.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace eon {

enum class MillenniumDosVideoFunctionZeroState {
    awaiting_cached_mode_query_result,
    awaiting_set_mode_result,
    awaiting_verify_mode_result,
    mode_match_continuation_boundary,
    mode_success_postlude_prefix_recorded,
    mode_mismatch_ret_boundary,
};

enum class MillenniumDosVideoFunctionZeroBoundaryKind {
    cached_mode_query,
    set_mode,
    verify_mode_query,
};

// INT 10h result as externally observed. AX/BX/CX/DX/FLAGS are captured raw
// fields; the session assigns no BIOS semantics to them.
struct MillenniumDosVideoFunctionZeroBiosResult {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    std::uint8_t interrupt_number = 0x10;
    std::uint16_t ax = 0;
    std::uint16_t bx = 0;
    std::uint16_t cx = 0;
    std::uint16_t dx = 0;
    std::uint16_t flags = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionZeroBiosResult&) const = default;
};

// Describes only registers proven immediately before an INT 10h boundary.
// For the two AH=$0f queries, AL is caller/BIOS state and can be unknown.
struct MillenniumDosVideoFunctionZeroBoundary {
    MillenniumDosVideoFunctionZeroBoundaryKind kind{};
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    std::uint8_t interrupt_number = 0x10;
    std::uint16_t ax = 0;
    std::uint16_t ax_known_mask = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionZeroBoundary&) const = default;
};

struct MillenniumDosVideoFunctionZeroLocalWrite {
    std::uint16_t instruction_address = 0;
    std::uint16_t driver_local_address = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionZeroLocalWrite&) const = default;
};

enum class MillenniumDosVideoFunctionZeroEndpoint {
    // This is a stop before executing the instruction at the match target.
    mode_match_continuation,
    // The mismatch code's XOR AX,AX has been modeled; stop at RET itself.
    mode_mismatch_ret_boundary,
};

enum class MillenniumDosVideoFunctionZeroPostludeEndpoint {
    // EGA640 stops before PUSH SI; no stack effect has been applied.
    ega_pre_push,
    // MCGA's DEC/JZ selects the single-count path; stop before its target.
    mcga_single_count_branch,
    // Stop before DOS-private INT 92h; no handler result is inferred.
    mcga_int92_request,
};

struct MillenniumDosVideoFunctionZeroPostludeOutcome {
    MillenniumDosVideoFunctionZeroPostludeEndpoint endpoint{};
    std::uint16_t instruction_address = 0;
    std::uint16_t ax = 0, bx = 0, cx = 0, dx = 0, si = 0;
    std::optional<std::uint16_t> di;
    std::optional<MillenniumDosVideoFunctionZeroLocalWrite> local_write;
    std::uint8_t interrupt_number = 0;
    constexpr bool operator==(const MillenniumDosVideoFunctionZeroPostludeOutcome&) const = default;
};

struct MillenniumDosVideoFunctionZeroOutcome {
    MillenniumDosVideoFunctionZeroEndpoint endpoint{};
    std::uint16_t instruction_address = 0;
    MillenniumDosVideoFunctionZeroBiosResult verify_result{};
    std::uint16_t ax_after_local_effect = 0;
    bool ax_is_routine_return = false;
    constexpr bool operator==(const MillenniumDosVideoFunctionZeroOutcome&) const = default;
};

// Standalone, hash-bound execution of English EGA640/MCGA function $00.
// It first stops at the successful mode-match continuation. A separate bounded
// step can advance only the verified local prefix, stopping before EGA's PUSH
// or MCGA's branch target/INT 92h. The caller supplies the driver-local cache
// byte as observed input and every INT 10h result explicitly.
class MillenniumDosVideoFunctionZeroSession {
public:
    MillenniumDosVideoFunctionZeroSession(std::span<const std::uint8_t> english_driver,
        MillenniumDosVideoDriverKind kind, std::uint8_t initial_cached_mode);

    [[nodiscard]] MillenniumDosVideoFunctionZeroState state() const { return state_; }
    [[nodiscard]] std::optional<MillenniumDosVideoFunctionZeroBoundary> boundary() const;
    [[nodiscard]] const MillenniumDosVideoDriverProfile& driver() const { return driver_; }
    [[nodiscard]] std::uint64_t next_sequence() const { return next_sequence_; }
    [[nodiscard]] const std::vector<MillenniumDosVideoFunctionZeroBiosResult>& bios_results() const {
        return bios_results_;
    }
    [[nodiscard]] const std::optional<MillenniumDosVideoFunctionZeroLocalWrite>& cache_write() const {
        return cache_write_;
    }
    [[nodiscard]] std::optional<MillenniumDosVideoFunctionZeroOutcome> outcome() const {
        return outcome_;
    }
    [[nodiscard]] std::optional<MillenniumDosVideoFunctionZeroPostludeOutcome>
    postlude_prefix_outcome() const { return postlude_prefix_outcome_; }

    void observe_bios_result(const MillenniumDosVideoFunctionZeroBiosResult& result);
    void advance_success_postlude_prefix();

private:
    MillenniumDosVideoDriverProfile driver_;
    MillenniumDosVideoFunctionZeroState state_{};
    std::uint64_t next_sequence_ = 1;
    std::optional<std::uint8_t> set_mode_result_al_;
    std::vector<MillenniumDosVideoFunctionZeroBiosResult> bios_results_;
    std::optional<MillenniumDosVideoFunctionZeroLocalWrite> cache_write_;
    std::optional<MillenniumDosVideoFunctionZeroOutcome> outcome_;
    std::optional<MillenniumDosVideoFunctionZeroPostludeOutcome> postlude_prefix_outcome_;
};

} // namespace eon
