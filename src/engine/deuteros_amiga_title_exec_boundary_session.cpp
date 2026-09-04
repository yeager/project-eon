#include "engine/deuteros_amiga_title_exec_boundary_session.hpp"

#include "data/sha256.hpp"

#include <array>
#include <stdexcept>

namespace eon {
namespace {

std::uint16_t big16(const std::span<const std::uint8_t> bytes, const std::size_t offset) {
    if (offset > bytes.size() || 2 > bytes.size() - offset) {
        throw std::runtime_error("Deuteros title Exec boundary word outside source");
    }
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(bytes[offset]) << 8U) | bytes[offset + 1]);
}

std::uint32_t big32(const std::span<const std::uint8_t> bytes, const std::size_t offset) {
    return (static_cast<std::uint32_t>(big16(bytes, offset)) << 16U)
        | big16(bytes, offset + 2U);
}

} // namespace

DeuterosAmigaTitleExecBoundarySession::DeuterosAmigaTitleExecBoundarySession(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    const DeuterosAmigaTitleExecPrelude& prelude,
    const DeuterosAmigaTitleStageProfile& profile) {
    constexpr std::uint32_t boundary_address = 0x40450;
    constexpr std::size_t boundary_length = 28;
    if (prelude.incoming_profile != 1 || prelude.entry_address != boundary_address
        || prelude.stack_pointer_value != profile.initialization_stack_address
        || prelude.stop_before_exec_base_read_address != 0x40456
        || profile.initialization_exec_base_address != 4
        || profile.initialization_exec_vectors
            != std::array<std::int16_t, 2>{-0x96, -0x9c}
        || boundary_address < plan.title_stage.destination
        || boundary_address - plan.title_stage.destination > plan.title_stage.length
        || boundary_length > plan.title_stage.length
            - (boundary_address - plan.title_stage.destination)) {
        throw std::runtime_error("Invalid Deuteros title Exec boundary provenance");
    }
    const auto bytes = disk.bytes(plan.title_stage.disk_offset
        + boundary_address - plan.title_stage.destination, boundary_length);
    constexpr std::string_view expected_sha256 =
        "24f5fb4f5019bf450f8b6931fe1c77747461704b139bbe14ec079b1008af1f49";
    if (big16(bytes, 0) != 0x2e7c || big32(bytes, 2) != prelude.stack_pointer_value
        || big16(bytes, 6) != 0x2c78 || big16(bytes, 8) != 4
        || big16(bytes, 10) != 0x4eae || big16(bytes, 12) != 0xff6a
        || big16(bytes, 14) != 0x203c
        || big32(bytes, 16) != profile.initialization_exec_allocation_size
        || big16(bytes, 20) != 0x2c78 || big16(bytes, 22) != 4
        || big16(bytes, 24) != 0x4eae || big16(bytes, 26) != 0xff64
        || to_hex(sha256(bytes)) != expected_sha256) {
        throw std::runtime_error("Unexpected Deuteros title Exec boundary bytes");
    }
    checkpoint_.stack_pointer_value = prelude.stack_pointer_value;
    checkpoint_.stop_before_address = prelude.stop_before_exec_base_read_address;
    checkpoint_.exec_base_source_address = profile.initialization_exec_base_address;
    checkpoint_.deferred_calls = {{
        {0x40456, 4, 0x4045a, -0x96, 0x4045e, std::nullopt},
        {0x40464, 4, 0x40468, -0x9c, 0x4046c,
            profile.initialization_exec_allocation_size},
    }};
    checkpoint_.boundary_sha256 = std::string(expected_sha256);
}

std::optional<DeuterosAmigaTitleExecBoundaryCheckpoint>
DeuterosAmigaTitleExecBoundarySession::enter_after_local_prefix(
    const std::uint32_t stack_pointer_value) {
    if (checkpoint_.state != DeuterosAmigaTitleExecBoundaryState::awaiting_local_prefix) {
        return std::nullopt;
    }
    if (stack_pointer_value != checkpoint_.stack_pointer_value) {
        throw std::runtime_error("Deuteros title Exec boundary stack provenance mismatch");
    }
    checkpoint_.state = DeuterosAmigaTitleExecBoundaryState::awaiting_exec_base_read;
    return checkpoint_;
}

std::optional<DeuterosAmigaTitleExecBoundaryCheckpoint>
DeuterosAmigaTitleExecBoundarySession::observe_exec_return(
    const DeuterosAmigaObservedExecReturn& observation) {
    std::size_t index = 0;
    if (checkpoint_.state == DeuterosAmigaTitleExecBoundaryState::awaiting_exec_base_read) {
        index = 0;
    } else if (checkpoint_.state
        == DeuterosAmigaTitleExecBoundaryState::awaiting_second_exec_return) {
        index = 1;
    } else {
        return std::nullopt;
    }
    const auto& expected = checkpoint_.deferred_calls[index];
    if (observation.trace_sequence == 0
        || (index != 0 && (!checkpoint_.observed_returns[0]
            || observation.trace_sequence
                <= checkpoint_.observed_returns[0]->trace_sequence))
        || observation.exec_base_source_address != expected.exec_base_source_address
        || observation.call_address != expected.call_address
        || observation.vector != expected.vector
        || observation.return_address != expected.return_address) {
        throw std::runtime_error("Deuteros title Exec return does not match the next boundary");
    }
    checkpoint_.observed_returns[index] = observation;
    if (index == 0) {
        // The intervening MOVE.L literal is wholly local, but the second
        // Exec-base read/call still requires its own genuine observation.
        checkpoint_.stop_before_address = expected.return_address + 6U;
        checkpoint_.state =
            DeuterosAmigaTitleExecBoundaryState::awaiting_second_exec_return;
    } else {
        checkpoint_.stop_before_address = expected.return_address;
        checkpoint_.state =
            DeuterosAmigaTitleExecBoundaryState::before_open_library_boundary;
    }
    return checkpoint_;
}

} // namespace eon
