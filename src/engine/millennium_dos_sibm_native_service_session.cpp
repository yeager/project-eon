#include "engine/millennium_dos_sibm_native_service_session.hpp"

#include "data/sha256.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace eon {

MillenniumDosSibmNativeServiceSession::MillenniumDosSibmNativeServiceSession(
    const std::uint64_t requested_generation,
    const std::uint64_t active_process_generation,
    const MillenniumDosNativeProcessCheckpoint& source_process,
    const std::span<const std::uint8_t> sibm_driver,
    const MillenniumDosSibmNativeVoiceBank& bank,
    const std::uint16_t native_driver_segment,
    const std::uint16_t arena_begin_segment,
    const std::uint16_t arena_end_segment_exclusive,
    const std::uint64_t arena_generation) {
    if (requested_generation == 0 || active_process_generation == 0
        || requested_generation != active_process_generation) {
        throw std::invalid_argument(
            "SIBM native service generation is detached from the active DOS process");
    }
    if (source_process.recovery_entry != MillenniumDosNativeRecoveryEntry::startup
        || source_process.release_sha256
            != "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123"
        || source_process.game_executable_sha256
            != "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57"
        || source_process.state
            != MillenniumDosNativeProcessState::startup_sibm_timer_vector_get_boundary
        || source_process.boundary.kind != MillenniumDosNativeBoundaryKind::dos_interrupt
        || source_process.boundary.address != 0x0233
        || source_process.boundary.interrupt != std::uint8_t{0x21}
        || source_process.boundary.service != std::uint8_t{0x35}
        || !source_process.observed_child_process_entry
        || source_process.child_entry_sequence == 0
        || source_process.child_code_segment == 0
        || !source_process.startup_sibm_function_zero) {
        throw std::invalid_argument(
            "SIBM native service requires the active AH=$35 function-zero boundary");
    }

    checkpoint_.generation = requested_generation;
    checkpoint_.source_process_generation = active_process_generation;
    checkpoint_.source_process_state = source_process.state;
    checkpoint_.source_process_boundary = source_process.boundary;
    // The exact first-call branch at SIBM `$04a8` returns AX=0 after the timer
    // helper returns. This is a native service result; it does not advance or
    // mutate the still-published guest process checkpoint at `$0233`.
    checkpoint_.native_function_zero_ax = 0;

    timer_.emplace(sibm_driver, *source_process.startup_sibm_function_zero);
    checkpoint_.timer = timer_->activate_native_callback();
    if (checkpoint_.timer.state != MillenniumDosSibmTimerState::running) {
        throw std::runtime_error(checkpoint_.timer.error.empty()
            ? "Eon-owned SIBM timer callback did not activate"
            : checkpoint_.timer.error);
    }
    checkpoint_.native_timer_callback_activated = true;

    if (native_driver_segment == 0) {
        throw std::invalid_argument("SIBM native service needs an owned driver segment");
    }
    checkpoint_.native_driver_segment = native_driver_segment;
    checkpoint_.allocation_arena_generation = arena_generation == 0
        ? requested_generation : arena_generation;
    // The segment is selected by the Eon compatibility process. The service
    // owns its contents separately from the guest's still-stopped checkpoint.
    startup_.emplace(requested_generation, native_driver_segment,
        arena_begin_segment, arena_end_segment_exclusive, arena_generation);
    if (!startup_->load_driver_image(sibm_driver)) {
        throw std::runtime_error(startup_->checkpoint().error.empty()
            ? "Hash-verified SIBM driver image could not be loaded into native memory"
            : startup_->checkpoint().error);
    }
    if (!startup_->apply_function_zero_effects(*source_process.startup_sibm_function_zero)) {
        throw std::runtime_error(startup_->checkpoint().error.empty()
            ? "Native SIBM function-zero effects were rejected"
            : startup_->checkpoint().error);
    }
    checkpoint_.startup = startup_->execute_after_timer_install(bank);
    if (checkpoint_.startup.state != MillenniumDosSibmStartupState::complete
        || checkpoint_.startup.function_twelve_ax != 0
        || checkpoint_.startup.last_function_eleven_ax != 0
        || checkpoint_.startup.loaded_voice_count != 14) {
        throw std::runtime_error(checkpoint_.startup.error.empty()
            ? "Native SIBM startup did not load all fourteen voices"
            : checkpoint_.startup.error);
    }
    checkpoint_.loaded_voice_count = checkpoint_.startup.loaded_voice_count;
    checkpoint_.state = MillenniumDosSibmNativeServiceState::complete;
}

MillenniumDosSibmNativeServiceCheckpoint
MillenniumDosSibmNativeServiceSession::checkpoint() const {
    return checkpoint_;
}

MillenniumDosSibmNativeMemoryCommitResult commit_millennium_dos_sibm_native_memory(
    NativeRuntimeMemory& target, const MillenniumDosSibmNativeServiceCheckpoint& service,
    const std::span<const std::uint8_t> source_driver) {
    constexpr std::string_view expected_driver_sha256 =
        "f3224caa43c1149907f852fa98816ed68c489b70f1ba795592d684d4e51f31b1";
    if (service.state != MillenniumDosSibmNativeServiceState::complete
        || service.generation == 0 || service.native_driver_segment == 0
        || source_driver.size() != 2'871U
        || to_hex(sha256(source_driver)) != expected_driver_sha256
        || service.startup.state != MillenniumDosSibmStartupState::complete) {
        return {false, "Native SIBM memory commit requires the complete hash-bound service"};
    }
    const auto& arena = service.startup.paragraph_arena;
    if (arena.generation != service.allocation_arena_generation || arena.begin_segment == 0
        || arena.end_segment_exclusive <= arena.begin_segment
        || arena.next_segment != arena.end_segment_exclusive) {
        return {false, "Native SIBM memory commit has no exact reserved paragraph range"};
    }
    std::vector<bool> driver_bytes_seen(source_driver.size(), false);
    auto next = target;
    NativeRuntimeEffectBatch batch{
        "millennium-dos-sibm-native-memory-" + std::to_string(service.generation), true, {}};
    const auto& cells = service.startup.memory.initialized_bytes;
    batch.effects.reserve(cells.size());
    for (const auto& cell : cells) {
        if (cell.location.address_space != NativeRuntimeAddressSpace::dos_segmented
            || !cell.location.segment) {
            return {false, "Native SIBM service produced a nonsegmented memory cell"};
        }
        const auto segment = *cell.location.segment;
        const auto prior = next.read_byte(cell.location);
        if (segment == service.native_driver_segment) {
            if (cell.location.offset >= source_driver.size() || !prior
                || *prior != source_driver[static_cast<std::size_t>(cell.location.offset)]) {
                return {false, "Loaded SIBM segment differs from its admitted source preimage"};
            }
            driver_bytes_seen[static_cast<std::size_t>(cell.location.offset)] = true;
        } else {
            if (segment < arena.begin_segment || segment >= arena.end_segment_exclusive) {
                return {false, "Native SIBM allocation escaped its reserved DOS paragraph range"};
            }
            if (prior) {
                return {false, "Reserved native SIBM startup range already contains guest bytes"};
            }
        }
        batch.effects.push_back({batch.effects.size() + 1, cell.location,
            MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::little_endian,
            cell.value});
    }
    if (std::ranges::any_of(driver_bytes_seen, [](const bool seen) { return !seen; })) {
        return {false, "Native SIBM memory snapshot is missing driver-image bytes"};
    }
    const auto applied = next.apply(batch);
    if (!applied.accepted) {
        return {false, "Native SIBM memory effects were rejected: " + applied.error};
    }
    target = std::move(next);
    return {true, {}};
}

} // namespace eon
