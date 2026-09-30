#pragma once

#include "engine/millennium_dos_video_function_13_session.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace eon {

enum class MillenniumDosVideoFunction13InterruptState {
    awaiting_interrupt_request,
    awaiting_retrace_port_read,
    awaiting_mcga_postlude_byte,
    iret_boundary,
    callback_boundary,
    awaiting_mcga_callback_word,
    callback_local_boundary,
    returned,
};

struct MillenniumDosVideoFunction13InterruptBoundary {
    std::uint16_t instruction_address = 0;
    constexpr bool operator==(const MillenniumDosVideoFunction13InterruptBoundary&) const = default;
};

struct MillenniumDosVideoFunction13InterruptRequest {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0x0127;
    std::uint16_t ax = 0x0013;
    std::uint16_t return_ip = 0x0129;
    std::uint16_t return_cs = 0;
    std::uint16_t return_flags = 0;
};

struct MillenniumDosVideoFunction13McgaPostludeByte {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0x001a;
    std::uint16_t segment = 0;
    std::uint16_t offset = 0x01e5;
    std::uint8_t value = 0;
};

enum class MillenniumDosVideoFunction13McgaCallbackReadWidth : std::uint8_t {
    byte = 1,
    word = 2,
};

struct MillenniumDosVideoFunction13McgaCallbackRead {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    std::uint16_t segment = 0;
    std::uint16_t offset = 0;
    std::uint16_t value = 0;
    MillenniumDosVideoFunction13McgaCallbackReadWidth width =
        MillenniumDosVideoFunction13McgaCallbackReadWidth::byte;
    constexpr bool operator==(const MillenniumDosVideoFunction13McgaCallbackRead&) const = default;
};

enum class MillenniumDosVideoFunction13McgaCallbackRegister : std::uint8_t {
    al,
    ax,
    bp,
    bl,
    bx,
    cx,
    dx,
    ds,
    si,
    es,
    di,
};

struct MillenniumDosVideoFunction13McgaCallbackRegisterEffect {
    std::uint16_t instruction_address = 0;
    MillenniumDosVideoFunction13McgaCallbackRegister reg =
        MillenniumDosVideoFunction13McgaCallbackRegister::cx;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunction13McgaCallbackRegisterEffect&) const = default;
};

struct MillenniumDosVideoFunction13McgaCallbackPortWrite {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    std::uint16_t port_address = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunction13McgaCallbackPortWrite&) const = default;
};

struct MillenniumDosVideoFunction13McgaCallbackFarPointerRead {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0x0d49;
    std::uint16_t source_segment = 0;
    std::uint16_t source_offset = 0x0d1a;
    std::uint16_t value_segment = 0;
    std::uint16_t value_offset = 0;
    constexpr bool operator==(const MillenniumDosVideoFunction13McgaCallbackFarPointerRead&) const = default;
};

struct MillenniumDosVideoFunction13McgaCallbackIndirectWordRead {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0x0d4e;
    std::uint16_t segment = 0;
    std::uint16_t offset = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunction13McgaCallbackIndirectWordRead&) const = default;
};

struct MillenniumDosVideoFunction13McgaCallbackMemoryWordEffect {
    std::uint16_t instruction_address = 0;
    std::uint16_t segment = 0;
    std::uint16_t offset = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunction13McgaCallbackMemoryWordEffect&) const = default;
};

struct MillenniumDosVideoFunction13McgaCallbackMemoryByteEffect {
    std::uint16_t instruction_address = 0;
    std::uint16_t segment = 0;
    std::uint16_t offset = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunction13McgaCallbackMemoryByteEffect&) const = default;
};

struct MillenniumDosVideoFunction13DriverByteEffect {
    std::uint16_t instruction_address = 0;
    std::uint16_t segment = 0;
    std::uint16_t offset = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunction13DriverByteEffect&) const = default;
};

struct MillenniumDosVideoFunction13DriverWordEffect {
    std::uint16_t instruction_address = 0;
    std::uint16_t segment = 0;
    std::uint16_t offset = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunction13DriverWordEffect&) const = default;
};

struct MillenniumDosVideoFunction13McgaCallbackRegisterStackInput {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0x0d3b;
    std::uint16_t ss = 0;
    std::uint16_t sp = 0;
    std::uint16_t ds = 0;
    std::uint16_t es = 0;
    std::uint16_t di = 0;
    std::uint16_t si = 0;
    std::uint16_t dx = 0;
    std::uint16_t cx = 0;
    std::uint16_t ax = 0;
    std::uint16_t bx = 0;
    std::uint16_t bp = 0;
};

struct MillenniumDosVideoFunction13CallbackStackWordEffect {
    std::uint16_t instruction_address = 0;
    std::uint16_t segment = 0;
    std::uint16_t offset = 0;
    std::uint16_t value = 0;
    constexpr bool operator==(const MillenniumDosVideoFunction13CallbackStackWordEffect&) const = default;
};

struct MillenniumDosVideoFunction13McgaCallbackEpilogueStackRead {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    std::uint16_t ss = 0;
    std::uint16_t sp = 0;
    std::uint16_t value = 0;
};

struct MillenniumDosVideoFunction13McgaCallbackReturnStackRead {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    std::uint16_t ss = 0;
    std::uint16_t sp = 0;
    std::uint16_t value = 0;
};

struct MillenniumDosVideoFunction13McgaCallbackPaletteStackRead {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    std::uint16_t ss = 0;
    std::uint16_t sp = 0;
    std::uint16_t value = 0;
};

struct MillenniumDosVideoFunction13InterruptOutcome {
    std::uint16_t ax = 0;
    std::uint16_t dx = 0x03da;
    std::uint16_t di = 0x0026;
    std::uint16_t ds = 0;
    std::uint16_t return_ip = 0;
    std::uint16_t return_cs = 0;
    std::uint16_t return_flags = 0;
    std::vector<MillenniumDosVideoFunction13PortRead> port_reads;
    std::vector<MillenniumDosVideoFunction13DriverByteEffect> memory_effects;
};

// Hash-bound standalone INT 91h -> function-$13 dispatch path. The supplied
// driver image is a scenario input only; this does not establish that the
// original IVT selected it or emulate DOS, hardware ports, or callback code
// beyond the explicitly represented local MCGA spans.
class MillenniumDosVideoFunction13InterruptSession {
public:
    MillenniumDosVideoFunction13InterruptSession(
        std::span<const std::uint8_t> english_driver,
        MillenniumDosVideoDriverKind kind,
        std::uint16_t driver_segment);

    [[nodiscard]] MillenniumDosVideoFunction13InterruptState state() const { return state_; }
    [[nodiscard]] std::uint64_t next_sequence() const { return last_sequence_ + 1; }
    [[nodiscard]] std::optional<MillenniumDosVideoFunction13InterruptBoundary> boundary() const;
    [[nodiscard]] const std::optional<MillenniumDosVideoFunction13Outcome>& retrace_outcome() const {
        return retrace_outcome_;
    }
    [[nodiscard]] std::optional<MillenniumDosVideoFunction13InterruptOutcome> outcome() const {
        return outcome_;
    }
    [[nodiscard]] const std::vector<MillenniumDosVideoFunction13McgaCallbackRead>& callback_reads() const {
        return callback_reads_;
    }
    [[nodiscard]] const std::vector<MillenniumDosVideoFunction13DriverByteEffect>&
    callback_driver_effects() const { return callback_driver_effects_; }
    [[nodiscard]] const std::vector<MillenniumDosVideoFunction13DriverWordEffect>&
    callback_driver_word_effects() const { return callback_driver_word_effects_; }
    [[nodiscard]] const std::vector<MillenniumDosVideoFunction13CallbackStackWordEffect>&
    callback_stack_effects() const { return callback_stack_effects_; }
    [[nodiscard]] const std::vector<MillenniumDosVideoFunction13McgaCallbackRegisterEffect>&
    callback_register_effects() const { return callback_register_effects_; }
    [[nodiscard]] const std::vector<MillenniumDosVideoFunction13McgaCallbackPortWrite>&
    callback_port_writes() const { return callback_port_writes_; }
    [[nodiscard]] const std::vector<MillenniumDosVideoFunction13PortRead>&
    callback_port_reads() const { return callback_port_reads_; }
    [[nodiscard]] const std::vector<MillenniumDosVideoFunction13McgaCallbackFarPointerRead>&
    callback_far_pointer_reads() const { return callback_far_pointer_reads_; }
    [[nodiscard]] const std::vector<MillenniumDosVideoFunction13McgaCallbackIndirectWordRead>&
    callback_indirect_word_reads() const { return callback_indirect_word_reads_; }
    [[nodiscard]] const std::vector<MillenniumDosVideoFunction13McgaCallbackMemoryWordEffect>&
    callback_memory_word_effects() const { return callback_memory_word_effects_; }
    [[nodiscard]] const std::vector<MillenniumDosVideoFunction13McgaCallbackMemoryByteEffect>&
    callback_memory_byte_effects() const { return callback_memory_byte_effects_; }

    void observe_interrupt_request(const MillenniumDosVideoFunction13InterruptRequest& request);
    void observe_port_read(const MillenniumDosVideoFunction13PortRead& read);
    void observe_mcga_postlude_byte(const MillenniumDosVideoFunction13McgaPostludeByte& read);
    void observe_mcga_callback_read(const MillenniumDosVideoFunction13McgaCallbackRead& read);
    void observe_mcga_callback_counter(const MillenniumDosVideoFunction13McgaCallbackRead& read);
    void execute_mcga_callback_alternate_flag(std::uint64_t sequence,
        std::uint16_t instruction_address);
    void execute_mcga_callback_register_saves(
        const MillenniumDosVideoFunction13McgaCallbackRegisterStackInput& input);
    void observe_mcga_callback_alternate_cx_read(
        const MillenniumDosVideoFunction13McgaCallbackRead& read);
    void observe_mcga_callback_alternate_pointer(
        const MillenniumDosVideoFunction13McgaCallbackFarPointerRead& read);
    void observe_mcga_callback_alternate_indirect_word(
        const MillenniumDosVideoFunction13McgaCallbackIndirectWordRead& read);
    void observe_mcga_callback_alternate_video_pointer(
        const MillenniumDosVideoFunction13McgaCallbackFarPointerRead& read);
    void observe_mcga_callback_alternate_selector_byte(
        const MillenniumDosVideoFunction13McgaCallbackRead& read);
    void execute_mcga_callback_loop_iteration(std::uint64_t sequence,
        std::uint16_t instruction_address);
    void execute_mcga_callback_copy_route(std::uint64_t sequence,
        std::uint16_t instruction_address);
    void observe_mcga_callback_copy_height(const MillenniumDosVideoFunction13McgaCallbackRead& read);
    void observe_mcga_callback_copy_width(const MillenniumDosVideoFunction13McgaCallbackRead& read);
    void observe_mcga_callback_copy_limit(const MillenniumDosVideoFunction13McgaCallbackRead& read);
    void observe_mcga_callback_copy_source_byte(const MillenniumDosVideoFunction13McgaCallbackRead& read);
    void observe_mcga_callback_copy_destination_word(const MillenniumDosVideoFunction13McgaCallbackRead& read);
    void observe_mcga_callback_copy_density_byte(const MillenniumDosVideoFunction13McgaCallbackRead& read);
    void observe_mcga_callback_copy_current_byte(const MillenniumDosVideoFunction13McgaCallbackRead& read);
    void execute_mcga_callback_copy_store(std::uint64_t sequence,
        std::uint16_t instruction_address);
    void observe_mcga_callback_epilogue_stack_read(
        const MillenniumDosVideoFunction13McgaCallbackEpilogueStackRead& read);
    void observe_mcga_callback_epilogue_descriptor(
        const MillenniumDosVideoFunction13McgaCallbackRead& read);
    void execute_mcga_callback_epilogue_clear(std::uint64_t sequence,
        std::uint16_t instruction_address);
    void execute_mcga_callback_epilogue_jump(std::uint64_t sequence,
        std::uint16_t instruction_address);
    void observe_mcga_callback_palette_descriptor_count(
        const MillenniumDosVideoFunction13McgaCallbackRead& read);
    void observe_mcga_callback_palette_descriptor_index(
        const MillenniumDosVideoFunction13McgaCallbackRead& read);
    void execute_mcga_callback_palette_descriptor_clear(std::uint64_t sequence,
        std::uint16_t instruction_address);
    void execute_mcga_callback_palette_prefix(std::uint64_t sequence,
        std::uint16_t instruction_address);
    void observe_mcga_callback_palette_source_index(
        const MillenniumDosVideoFunction13McgaCallbackRead& read);
    void execute_mcga_callback_palette_out(std::uint64_t sequence,
        std::uint16_t instruction_address);
    void observe_mcga_callback_palette_retrace(const MillenniumDosVideoFunction13PortRead& read);
    void observe_mcga_callback_palette_source_pointer(
        const MillenniumDosVideoFunction13McgaCallbackFarPointerRead& read);
    void observe_mcga_callback_palette_source_byte(const MillenniumDosVideoFunction13McgaCallbackRead& read);
    void execute_mcga_callback_palette_data_out(std::uint64_t sequence,
        std::uint16_t instruction_address);
    void execute_mcga_callback_palette_loop(std::uint64_t sequence,
        std::uint16_t instruction_address);
    void observe_mcga_callback_palette_stack_read(
        const MillenniumDosVideoFunction13McgaCallbackPaletteStackRead& read);
    void observe_mcga_callback_palette_descriptor_word(
        const MillenniumDosVideoFunction13McgaCallbackRead& read);
    void observe_mcga_callback_palette_loop_count_stack(
        const MillenniumDosVideoFunction13McgaCallbackPaletteStackRead& read);
    void execute_mcga_callback_palette_descriptor_loop(std::uint64_t sequence,
        std::uint16_t instruction_address);
    void observe_mcga_callback_return_stack_read(
        const MillenniumDosVideoFunction13McgaCallbackReturnStackRead& read);
    void execute_mcga_callback_return_flag_clear(std::uint64_t sequence,
        std::uint16_t instruction_address);
    void execute_mcga_callback_return(std::uint64_t sequence,
        std::uint16_t instruction_address);
    void execute_iret(std::uint64_t sequence, std::uint16_t instruction_address);

private:
    MillenniumDosVideoDriverKind kind_;
    std::uint16_t driver_segment_;
    MillenniumDosVideoFunction13Session retrace_;
    MillenniumDosVideoFunction13InterruptState state_ =
        MillenniumDosVideoFunction13InterruptState::awaiting_interrupt_request;
    std::uint64_t last_sequence_ = 0;
    std::optional<MillenniumDosVideoFunction13InterruptRequest> request_;
    std::optional<MillenniumDosVideoFunction13Outcome> retrace_outcome_;
    std::optional<MillenniumDosVideoFunction13InterruptOutcome> outcome_;
    std::vector<MillenniumDosVideoFunction13McgaCallbackRead> callback_reads_;
    std::vector<MillenniumDosVideoFunction13DriverByteEffect> callback_driver_effects_;
    std::vector<MillenniumDosVideoFunction13DriverWordEffect> callback_driver_word_effects_;
    std::vector<MillenniumDosVideoFunction13CallbackStackWordEffect> callback_stack_effects_;
    std::vector<MillenniumDosVideoFunction13McgaCallbackRegisterEffect> callback_register_effects_;
    std::vector<MillenniumDosVideoFunction13McgaCallbackPortWrite> callback_port_writes_;
    std::vector<MillenniumDosVideoFunction13PortRead> callback_port_reads_;
    std::vector<MillenniumDosVideoFunction13McgaCallbackFarPointerRead> callback_far_pointer_reads_;
    std::vector<MillenniumDosVideoFunction13McgaCallbackIndirectWordRead> callback_indirect_word_reads_;
    std::vector<MillenniumDosVideoFunction13McgaCallbackMemoryWordEffect> callback_memory_word_effects_;
    std::vector<MillenniumDosVideoFunction13McgaCallbackMemoryByteEffect> callback_memory_byte_effects_;
    std::optional<MillenniumDosVideoFunction13McgaCallbackFarPointerRead> callback_far_pointer_;
    std::optional<std::uint16_t> callback_cx_;
    std::optional<std::uint8_t> callback_copy_height_;
    std::optional<std::uint8_t> callback_copy_width_;
    std::optional<std::uint16_t> callback_copy_ax_;
    std::optional<std::uint16_t> callback_copy_cx_;
    std::optional<std::uint16_t> callback_copy_dx_;
    std::optional<std::uint16_t> callback_copy_si_;
    std::optional<std::uint16_t> callback_copy_di_;
    std::optional<std::uint16_t> callback_copy_segment_;
    std::optional<std::uint8_t> callback_copy_density_;
    std::optional<std::uint16_t> callback_epilogue_ss_;
    std::optional<std::uint16_t> callback_epilogue_sp_;
    std::optional<std::uint16_t> callback_epilogue_si_;
    std::optional<std::uint16_t> callback_epilogue_ds_;
    std::optional<std::uint16_t> callback_palette_source_si_;
    std::optional<std::uint16_t> callback_palette_source_ds_;
    std::uint8_t callback_palette_component_ = 0;
    std::optional<std::uint16_t> callback_palette_cx_;
    std::optional<std::uint16_t> callback_palette_ss_;
    std::optional<std::uint16_t> callback_palette_sp_;
    std::optional<std::uint16_t> callback_palette_si_;
    std::optional<std::uint16_t> callback_palette_ds_;
    std::optional<std::uint16_t> callback_palette_outer_cx_;
    std::uint8_t callback_return_pop_count_ = 0;
    bool callback_iret_pending_ = false;
    std::uint16_t callback_next_instruction_ = 0;
};

} // namespace eon
