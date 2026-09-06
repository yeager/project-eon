#pragma once

#include "engine/deuteros_amiga_owned_alternate_renderer.hpp"
#include "engine/native_runtime_memory.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>

namespace eon {

enum class DeuterosAmigaOwnedDiskTransitionState {
    matched_boot_disk,
    awaiting_palette_return,
    awaiting_left_button,
    retry_disk_check,
};

struct DeuterosAmigaOwnedDiskTransitionPlan {
    DeuterosAmigaOwnedDiskTransitionState state =
        DeuterosAmigaOwnedDiskTransitionState::matched_boot_disk;
    std::uint32_t signature = 0;
    std::uint32_t buffer_address = 0;
    std::uint32_t a0_value = 0;
    std::uint32_t a1_value = 0;
    std::uint32_t a4_value = 0;
    std::uint32_t a6_value = 0;
    std::uint32_t d0_value = 0;
    std::uint32_t next_instruction_address = 0;
    std::uint32_t next_call_address = 0;
    std::uint32_t next_return_address = 0;
    std::int16_t next_vector = 0;
    std::uint32_t pending_read_instruction = 0;
    std::uint32_t pending_read_address = 0;
    std::uint8_t pending_read_bit = 0;
};

struct DeuterosAmigaObservedDiskTransitionPaletteReturn {
    std::uint32_t call_address = 0;
    std::int16_t vector = 0;
    std::uint32_t return_address = 0;
    std::uint32_t result_d0 = 0;
    std::uint32_t library = 0;
};

struct DeuterosAmigaObservedDiskTransitionInput {
    std::uint32_t instruction_address = 0;
    std::uint32_t port_address = 0;
    std::uint8_t bit = 0;
    std::uint8_t value = 0;
};

// Native execution of the hash-bound $219f8 disk check through either the
// shared $21a56 success tail or the first asynchronous palette boundary.
// MediaRead is a bounded, read-only big-endian callback over the currently
// selected original disk. The complete 1024-byte boot-block read is staged
// before any write, allowing a caller-owned journal to publish atomically.
template<class Read, class Write, class MediaRead>
DeuterosAmigaOwnedDiskTransitionPlan begin_deuteros_amiga_owned_disk_transition(
    const std::uint64_t media_size, Read&& read, Write&& write, MediaRead&& media_read) {
    using Width = MemoryTransferElementWidth;
    constexpr std::uint32_t boot_block_bytes = 1024;
    constexpr std::uint32_t expected_signature = 0x4452f018;

    if (media_size < boot_block_bytes)
        throw std::runtime_error("Deuteros disk-check media is shorter than its boot block");
    const auto buffer = read(0x2097a, 4);
    if ((buffer & 1U) != 0 || buffer > 0x1000000U - boot_block_bytes)
        throw std::runtime_error("Deuteros disk-check buffer is invalid");

    std::array<std::uint8_t, boot_block_bytes> boot{};
    for (std::uint32_t offset = 0; offset < boot_block_bytes; ++offset)
        boot[offset] = static_cast<std::uint8_t>(media_read(offset, 1));
    const auto signature = (static_cast<std::uint32_t>(boot[1020]) << 24U)
        | (static_cast<std::uint32_t>(boot[1021]) << 16U)
        | (static_cast<std::uint32_t>(boot[1022]) << 8U)
        | static_cast<std::uint32_t>(boot[1023]);

    // The mismatch branch calls $21aac unconditionally. Conversely, the
    // matching branch jumps directly to $21a56 and must not inspect either
    // $21706 or the display-buffer cell before that shared tail does so.
    const auto display = signature == expected_signature ? 0U : read(0x12ff4, 4);
    if (signature != expected_signature
        && ((display & 1U) != 0 || display > 0x1000000U - 32000U))
        throw std::runtime_error("Deuteros disk-message buffer is invalid");

    write(0x219f4, Width::longword, 5);
    for (std::uint32_t offset = 0; offset < boot_block_bytes; ++offset)
        write(buffer + offset, Width::byte, boot[offset]);
    write(0x2097e, Width::longword, signature);

    DeuterosAmigaOwnedDiskTransitionPlan plan;
    plan.signature = signature;
    plan.buffer_address = buffer;
    plan.d0_value = signature;
    if (signature == expected_signature) {
        plan.state = DeuterosAmigaOwnedDiskTransitionState::matched_boot_disk;
        plan.next_instruction_address = 0x21a56;
        return plan;
    }

    {
        for (std::uint32_t offset = 0; offset < 32000U; offset += 4U)
            write(display + offset, Width::longword, 0);
        plan.a0_value = display + 32000U;
    }
    plan.state = DeuterosAmigaOwnedDiskTransitionState::awaiting_palette_return;
    plan.a0_value = 0x12e12;
    plan.a1_value = read(0x21266, 4) + 0x200U;
    plan.a6_value = read(0x12fec, 4);
    plan.d0_value = 16;
    plan.next_call_address = 0x21a30;
    plan.next_return_address = 0x21a34;
    plan.next_vector = -0xc0;
    return plan;
}

// Continue only after the exact graphics.library return. Rendering consumes
// the original command stream already owned at $219a2; no host font or copied
// media is introduced.
template<class Read, class Write>
DeuterosAmigaOwnedDiskTransitionPlan resume_deuteros_amiga_owned_disk_transition_palette(
    DeuterosAmigaOwnedDiskTransitionPlan plan,
    const DeuterosAmigaObservedDiskTransitionPaletteReturn& observed,
    Read&& read, Write&& write) {
    if (plan.state != DeuterosAmigaOwnedDiskTransitionState::awaiting_palette_return
        || plan.next_call_address != 0x21a30 || plan.next_return_address != 0x21a34
        || plan.next_vector != -0xc0 || observed.call_address != plan.next_call_address
        || observed.return_address != plan.next_return_address
        || observed.vector != plan.next_vector || observed.library != plan.a6_value)
        throw std::runtime_error("Deuteros disk-message palette return does not match boundary");

    apply_deuteros_amiga_owned_alternate_renderer(0x219a2, read, write);
    plan.state = DeuterosAmigaOwnedDiskTransitionState::awaiting_left_button;
    plan.d0_value = observed.result_d0;
    plan.a4_value = 0x219a2;
    plan.next_call_address = 0;
    plan.next_return_address = 0;
    plan.next_vector = 0;
    plan.pending_read_instruction = 0x21a40;
    plan.pending_read_address = 0xbfe001;
    plan.pending_read_bit = 6;
    return plan;
}

inline DeuterosAmigaOwnedDiskTransitionPlan resume_deuteros_amiga_owned_disk_transition_input(
    DeuterosAmigaOwnedDiskTransitionPlan plan,
    const DeuterosAmigaObservedDiskTransitionInput& observed) {
    if (plan.state != DeuterosAmigaOwnedDiskTransitionState::awaiting_left_button
        || observed.instruction_address != 0x21a40 || observed.port_address != 0xbfe001
        || observed.bit != 6)
        throw std::runtime_error("Deuteros disk-message input does not match boundary");
    if ((observed.value & 0x40U) != 0)
        return plan;
    plan.state = DeuterosAmigaOwnedDiskTransitionState::retry_disk_check;
    plan.pending_read_instruction = 0;
    plan.pending_read_address = 0;
    plan.pending_read_bit = 0;
    plan.next_instruction_address = 0x21a02;
    return plan;
}

} // namespace eon
