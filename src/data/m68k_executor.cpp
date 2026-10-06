#include "data/m68k_executor.hpp"

#include <limits>

namespace eon::m68k {
namespace {

bool contains(const std::uint32_t base, const std::size_t size,
    const std::uint32_t address, const std::size_t width) {
    const auto begin = static_cast<std::uint64_t>(address);
    const auto range_begin = static_cast<std::uint64_t>(base);
    const auto range_end = range_begin + size;
    return begin >= range_begin && begin <= range_end
        && width <= range_end - begin;
}

void set_move_word_flags(MachineState& state, const std::uint16_t value) {
    // MOVE updates N/Z and clears V/C, while preserving X and the upper SR.
    state.sr = static_cast<std::uint16_t>(state.sr & 0xfff0U);
    if ((value & 0x8000U) != 0) state.sr |= 0x0008U;
    if (value == 0) state.sr |= 0x0004U;
}

void set_move_long_flags(MachineState& state, const std::uint32_t value) {
    state.sr = static_cast<std::uint16_t>(state.sr & 0xfff0U);
    if ((value & 0x80000000U) != 0) state.sr |= 0x0008U;
    if (value == 0) state.sr |= 0x0004U;
}

} // namespace

ExecutionResult execute(const std::span<const std::uint8_t> code,
    const std::uint32_t code_base, MachineState initial,
    const std::span<MemoryRange> memory, const std::uint64_t max_steps,
    const std::uint32_t stop_address) {
    ExecutionResult result{initial, StopReason::instruction_limit, 0, 0};

    const auto read_bytes = [&](const std::uint32_t address, std::uint8_t* output,
        const std::size_t width) {
        for (const auto& range : memory) {
            if (contains(range.base, range.bytes.size(), address, width)) {
                const auto offset = static_cast<std::size_t>(address - range.base);
                for (std::size_t index = 0; index < width; ++index) {
                    output[index] = range.bytes[offset + index];
                }
                return true;
            }
        }
        return false;
    };
    const auto read_word = [&](const std::uint32_t address, std::uint16_t& value) {
        std::uint8_t bytes[2]{};
        if ((address & 1U) != 0U || !read_bytes(address, bytes, 2U)) return false;
        value = static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[0]) << 8U)
            | bytes[1]);
        return true;
    };
    const auto writable_range = [&](const std::uint32_t address, const std::size_t width)
        -> MemoryRange* {
        for (auto& range : memory) {
            if (range.writable && contains(range.base, range.bytes.size(), address, width)) {
                return &range;
            }
        }
        return nullptr;
    };

    const auto fetch_word = [&](const std::uint32_t address, std::uint16_t& value) {
        if ((address & 1U) != 0U || !contains(code_base, code.size(), address, 2)) return false;
        const auto offset = static_cast<std::size_t>(address - code_base);
        value = static_cast<std::uint16_t>((static_cast<std::uint16_t>(code[offset]) << 8U)
            | code[offset + 1U]);
        return true;
    };
    const auto fetch_long = [&](const std::uint32_t address, std::uint32_t& value) {
        std::uint16_t high = 0;
        std::uint16_t low = 0;
        if (address > std::numeric_limits<std::uint32_t>::max() - 3U
            || !fetch_word(address, high) || !fetch_word(address + 2U, low)) return false;
        value = (static_cast<std::uint32_t>(high) << 16U) | low;
        return true;
    };

    while (result.instructions_executed < max_steps) {
        if (result.state.pc == stop_address) {
            result.reason = StopReason::requested_address;
            return result;
        }

        const auto instruction_pc = result.state.pc;
        std::uint16_t opcode = 0;
        if (!fetch_word(instruction_pc, opcode)) {
            result.reason = StopReason::memory_fault;
            return result;
        }
        result.stop_opcode = opcode;
        if ((opcode & 0xfff0U) == 0x4e40U) {
            result.reason = StopReason::trap_instruction;
            return result;
        }

        std::uint32_t next_pc = instruction_pc + 2U;
        if (opcode == 0x207cU || opcode == 0x227cU) { // MOVEA.L #imm, A0/A1
            std::uint32_t immediate = 0;
            if (!fetch_long(next_pc, immediate)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto address_register = opcode == 0x207cU ? 0U : 1U;
            result.state.address[address_register] = immediate;
            next_pc += 4U;
        } else if (opcode == 0x2e3cU) { // MOVE.L #imm, D7
            std::uint32_t immediate = 0;
            if (!fetch_long(next_pc, immediate)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            result.state.data[7] = immediate;
            set_move_long_flags(result.state, immediate);
            next_pc += 4U;
        } else if (opcode == 0x2f07U) { // MOVE.L D7, -(A7)
            const auto stack = result.state.address[7];
            if (stack < 4U || ((stack - 4U) & 1U) != 0U) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto destination_address = stack - 4U;
            auto* destination = writable_range(destination_address, 4);
            if (!destination) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto value = result.state.data[7];
            const auto offset = static_cast<std::size_t>(destination_address - destination->base);
            destination->bytes[offset] = static_cast<std::uint8_t>(value >> 24U);
            destination->bytes[offset + 1U] = static_cast<std::uint8_t>(value >> 16U);
            destination->bytes[offset + 2U] = static_cast<std::uint8_t>(value >> 8U);
            destination->bytes[offset + 3U] = static_cast<std::uint8_t>(value);
            result.state.address[7] = destination_address;
            set_move_long_flags(result.state, value);
        } else if (opcode == 0x3f3cU) { // MOVE.W #imm, -(A7)
            std::uint16_t immediate = 0;
            const auto stack = result.state.address[7];
            if (!fetch_word(next_pc, immediate) || stack < 2U
                || ((stack - 2U) & 1U) != 0U) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto destination_address = stack - 2U;
            auto* destination = writable_range(destination_address, 2);
            if (!destination) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto offset = static_cast<std::size_t>(destination_address - destination->base);
            destination->bytes[offset] = static_cast<std::uint8_t>(immediate >> 8U);
            destination->bytes[offset + 1U] = static_cast<std::uint8_t>(immediate);
            result.state.address[7] = destination_address;
            set_move_word_flags(result.state, immediate);
            next_pc += 2U;
        } else if (opcode == 0x08f9U) { // BSET #imm,(absolute long), byte
            std::uint16_t bit_number = 0;
            std::uint32_t address = 0;
            if (!fetch_word(next_pc, bit_number)
                || !fetch_long(next_pc + 2U, address)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            auto* destination = writable_range(address, 1);
            if (!destination) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto offset = static_cast<std::size_t>(address - destination->base);
            const auto bit = static_cast<std::uint8_t>(bit_number & 7U);
            const auto prior = destination->bytes[offset];
            result.state.sr = static_cast<std::uint16_t>(result.state.sr & 0xfffbU);
            if ((prior & static_cast<std::uint8_t>(1U << bit)) == 0) {
                result.state.sr |= 0x0004U;
            }
            destination->bytes[offset] = static_cast<std::uint8_t>(
                prior | static_cast<std::uint8_t>(1U << bit));
            next_pc += 6U;
        } else if (opcode == 0x3039U) { // MOVE.W (absolute long),D0
            std::uint32_t address = 0;
            std::uint16_t value = 0;
            if (!fetch_long(next_pc, address) || !read_word(address, value)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            result.state.data[0] = (result.state.data[0] & 0xffff0000U) | value;
            result.state.sr = static_cast<std::uint16_t>(result.state.sr & 0xfff0U);
            if ((value & 0x8000U) != 0) result.state.sr |= 0x0008U;
            if (value == 0) result.state.sr |= 0x0004U;
            next_pc += 4U;
        } else if (opcode == 0x303cU) { // MOVE.W #imm, D0
            std::uint16_t immediate = 0;
            if (!fetch_word(next_pc, immediate)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            result.state.data[0] = (result.state.data[0] & 0xffff0000U) | immediate;
            set_move_word_flags(result.state, immediate);
            next_pc += 2U;
        } else if (opcode == 0x32d8U) { // MOVE.W (A0)+, (A1)+
            const auto source_address = result.state.address[0];
            const auto destination_address = result.state.address[1];
            std::uint16_t value = 0;
            auto* destination = writable_range(destination_address, 2);
            if ((destination_address & 1U) != 0U
                || !destination || !read_word(source_address, value)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            // The post-increment arithmetic is part of this instruction. Check
            // it before writing so a fault cannot leave a partial side effect.
            if (source_address > std::numeric_limits<std::uint32_t>::max() - 2U
                || destination_address > std::numeric_limits<std::uint32_t>::max() - 2U) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto offset = static_cast<std::size_t>(destination_address - destination->base);
            destination->bytes[offset] = static_cast<std::uint8_t>(value >> 8U);
            destination->bytes[offset + 1U] = static_cast<std::uint8_t>(value);
            result.state.address[0] += 2U;
            result.state.address[1] += 2U;
            set_move_word_flags(result.state, value);
        } else if (opcode == 0x51c8U) { // DBF D0, displacement
            std::uint16_t displacement = 0;
            if (!fetch_word(next_pc, displacement)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto decremented = static_cast<std::uint16_t>(result.state.data[0] - 1U);
            result.state.data[0] = (result.state.data[0] & 0xffff0000U) | decremented;
            next_pc += 2U;
            if (decremented != 0xffffU) {
                const auto extension_address = instruction_pc + 2U;
                const auto signed_displacement = static_cast<std::int16_t>(displacement);
                next_pc = static_cast<std::uint32_t>(
                    static_cast<std::int64_t>(extension_address) + signed_displacement);
            }
        } else if (opcode == 0x4ef9U) { // JMP (absolute long)
            std::uint32_t address = 0;
            if (!fetch_long(next_pc, address)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            next_pc = address;
        } else if (opcode == 0x508fU) { // ADDQ.L #8,A7
            if (result.state.address[7] > std::numeric_limits<std::uint32_t>::max() - 8U) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            result.state.address[7] += 8U;
        } else if (opcode == 0x33c0U) { // MOVE.W D0,(absolute long)
            std::uint32_t address = 0;
            if (!fetch_long(next_pc, address)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            if ((address & 1U) != 0U) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            auto* destination = writable_range(address, 2);
            if (!destination) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto value = static_cast<std::uint16_t>(result.state.data[0]);
            const auto offset = static_cast<std::size_t>(address - destination->base);
            destination->bytes[offset] = static_cast<std::uint8_t>(value >> 8U);
            destination->bytes[offset + 1U] = static_cast<std::uint8_t>(value);
            result.state.sr = static_cast<std::uint16_t>(result.state.sr & 0xfff0U);
            if ((value & 0x8000U) != 0) result.state.sr |= 0x0008U;
            if (value == 0) result.state.sr |= 0x0004U;
            next_pc += 4U;
        } else if (opcode == 0x4a80U) { // TST.L D0
            result.state.sr = static_cast<std::uint16_t>(result.state.sr & 0xfff0U);
            if ((result.state.data[0] & 0x80000000U) != 0) result.state.sr |= 0x0008U;
            if (result.state.data[0] == 0) result.state.sr |= 0x0004U;
        } else if (opcode == 0x4e75U) { // RTS
            const auto stack = result.state.address[7];
            std::uint32_t address = 0;
            if ((stack & 1U) != 0U
                || stack > std::numeric_limits<std::uint32_t>::max() - 3U) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            std::uint8_t bytes[4]{};
            if (!read_bytes(stack, bytes, 4U)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            address = (static_cast<std::uint32_t>(bytes[0]) << 24U)
                | (static_cast<std::uint32_t>(bytes[1]) << 16U)
                | (static_cast<std::uint32_t>(bytes[2]) << 8U)
                | bytes[3];
            if (stack > std::numeric_limits<std::uint32_t>::max() - 4U) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            result.state.address[7] += 4U;
            next_pc = address;
        } else {
            result.reason = StopReason::unsupported_instruction;
            return result;
        }

        result.state.pc = next_pc;
        ++result.instructions_executed;
    }

    result.reason = result.state.pc == stop_address
        ? StopReason::requested_address
        : StopReason::instruction_limit;
    return result;
}

} // namespace eon::m68k
