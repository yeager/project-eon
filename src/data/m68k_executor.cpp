#include "data/m68k_executor.hpp"

#include <algorithm>
#include <array>
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

void set_move_byte_flags(MachineState& state, const std::uint8_t value) {
    state.sr = static_cast<std::uint16_t>(state.sr & 0xfff0U);
    if ((value & 0x80U) != 0U) state.sr |= 0x0008U;
    if (value == 0U) state.sr |= 0x0004U;
}

void set_compare_word_flags(MachineState& state, const std::uint16_t left,
    const std::uint16_t right) {
    const auto value = static_cast<std::uint16_t>(left - right);
    auto flags = static_cast<std::uint16_t>(state.sr & 0xfff0U);
    if ((value & 0x8000U) != 0U) flags |= 0x0008U;
    if (value == 0U) flags |= 0x0004U;
    if (((left ^ right) & (left ^ value) & 0x8000U) != 0U) flags |= 0x0002U;
    if (left < right) flags |= 0x0001U;
    state.sr = flags;
}

void set_compare_long_flags(MachineState& state, const std::uint32_t left,
    const std::uint32_t right) {
    const auto value = left - right;
    auto flags = static_cast<std::uint16_t>(state.sr & 0xfff0U);
    if ((value & 0x80000000U) != 0U) flags |= 0x0008U;
    if (value == 0U) flags |= 0x0004U;
    if (((left ^ right) & (left ^ value) & 0x80000000U) != 0U) flags |= 0x0002U;
    if (left < right) flags |= 0x0001U;
    state.sr = flags;
}

void set_compare_byte_flags(MachineState& state, const std::uint8_t left,
    const std::uint8_t right) {
    const auto value = static_cast<std::uint8_t>(left - right);
    auto flags = static_cast<std::uint16_t>(state.sr & 0xfff0U);
    if ((value & 0x80U) != 0U) flags |= 0x0008U;
    if (value == 0U) flags |= 0x0004U;
    if (((left ^ right) & (left ^ value) & 0x80U) != 0U) flags |= 0x0002U;
    if (left < right) flags |= 0x0001U;
    state.sr = flags;
}

std::uint16_t add_quick_word_flags(const std::uint16_t left,
    const std::uint8_t right, MachineState& state) {
    const auto sum = static_cast<std::uint32_t>(left) + right;
    const auto value = static_cast<std::uint16_t>(sum);
    auto flags = static_cast<std::uint16_t>(state.sr & 0xffe0U);
    if ((value & 0x8000U) != 0U) flags |= 0x0008U;
    if (value == 0U) flags |= 0x0004U;
    if (((~(left ^ right) & (left ^ value)) & 0x8000U) != 0U)
        flags |= 0x0002U;
    if (sum > 0xffffU) flags |= 0x0011U; // C and X
    state.sr = flags;
    return value;
}

} // namespace

ExecutionResult execute(const std::span<const std::uint8_t> code,
    const std::uint32_t code_base, MachineState initial,
    const std::span<MemoryRange> memory, const std::uint64_t max_steps,
    const std::uint32_t stop_address) {
    const std::array<std::uint32_t, 1> stops{stop_address};
    return execute(code, code_base, initial, memory, max_steps, stops);
}

ExecutionResult execute(const std::span<const std::uint8_t> code,
    const std::uint32_t code_base, MachineState initial,
    const std::span<MemoryRange> memory, const std::uint64_t max_steps,
    const std::span<const std::uint32_t> stop_addresses) {
    ExecutionResult result{initial, StopReason::instruction_limit, 0, 0, {}};

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
    const auto read_long = [&](const std::uint32_t address, std::uint32_t& value) {
        std::uint8_t bytes[4]{};
        if ((address & 1U) != 0U || !read_bytes(address, bytes, 4U)) return false;
        value = (static_cast<std::uint32_t>(bytes[0]) << 24U)
            | (static_cast<std::uint32_t>(bytes[1]) << 16U)
            | (static_cast<std::uint32_t>(bytes[2]) << 8U)
            | static_cast<std::uint32_t>(bytes[3]);
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
        if (std::find(stop_addresses.begin(), stop_addresses.end(), result.state.pc)
            != stop_addresses.end()) {
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
        if ((opcode & 0xf100U) == 0x7000U) { // MOVEQ #imm, Dn
            const auto data_register = static_cast<std::size_t>((opcode >> 9U) & 7U);
            const auto signed_immediate = static_cast<std::int8_t>(opcode & 0x00ffU);
            const auto value = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(signed_immediate));
            result.state.data[data_register] = value;
            set_move_long_flags(result.state, value);
        } else if ((opcode & 0xf1f8U) == 0x2040U) { // MOVEA.L Dn,An
            const auto address_register = static_cast<std::size_t>((opcode >> 9U) & 7U);
            const auto data_register = static_cast<std::size_t>(opcode & 7U);
            result.state.address[address_register] = result.state.data[data_register];
        } else if ((opcode & 0xf1ffU) == 0x203cU) { // MOVE.L #imm,Dn
            std::uint32_t immediate = 0;
            if (!fetch_long(next_pc, immediate)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto data_register = static_cast<std::size_t>((opcode >> 9U) & 7U);
            result.state.data[data_register] = immediate;
            set_move_long_flags(result.state, immediate);
            next_pc += 4U;
        } else if ((opcode & 0xf1ffU) == 0x2078U
            || (opcode & 0xf1ffU) == 0x2079U) { // MOVEA.L (abs.w/abs.l),An
            std::uint16_t extension = 0;
            if (!fetch_word(next_pc, extension)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            std::uint32_t address = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(static_cast<std::int16_t>(extension)));
            next_pc += 2U;
            if ((opcode & 0xf1ffU) == 0x2079U) {
                std::uint16_t low = 0;
                if (!fetch_word(next_pc, low)) {
                    result.reason = StopReason::memory_fault;
                    return result;
                }
                address = (static_cast<std::uint32_t>(extension) << 16U) | low;
                next_pc += 2U;
            }
            std::uint32_t value = 0;
            if (!read_long(address, value)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto address_register = static_cast<std::size_t>((opcode >> 9U) & 7U);
            result.state.address[address_register] = value;
        } else if ((opcode & 0xfff8U) == 0x23c0U) { // MOVE.L Dn,(abs.l)
            std::uint32_t address = 0;
            if (!fetch_long(next_pc, address)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            auto* destination = writable_range(address, 4U);
            if (!destination) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto data_register = static_cast<std::size_t>(opcode & 7U);
            const auto value = result.state.data[data_register];
            const auto offset = static_cast<std::size_t>(address - destination->base);
            destination->bytes[offset] = static_cast<std::uint8_t>(value >> 24U);
            destination->bytes[offset + 1U] = static_cast<std::uint8_t>(value >> 16U);
            destination->bytes[offset + 2U] = static_cast<std::uint8_t>(value >> 8U);
            destination->bytes[offset + 3U] = static_cast<std::uint8_t>(value);
            set_move_long_flags(result.state, value);
            next_pc += 4U;
        } else if ((opcode & 0xf1ffU) == 0xb07cU) { // CMP.W #imm,Dn
            std::uint16_t immediate = 0;
            if (!fetch_word(next_pc, immediate)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto data_register = static_cast<std::size_t>((opcode >> 9U) & 7U);
            set_compare_word_flags(result.state,
                static_cast<std::uint16_t>(result.state.data[data_register]), immediate);
            next_pc += 2U;
        } else if ((opcode & 0xf1ffU) == 0xc039U) { // AND.B (abs.l),Dn
            std::uint32_t address = 0;
            if (!fetch_long(next_pc, address)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            std::uint8_t source = 0;
            if (!read_bytes(address, &source, 1U)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto data_register = static_cast<std::size_t>((opcode >> 9U) & 7U);
            const auto value = static_cast<std::uint8_t>(
                static_cast<std::uint8_t>(result.state.data[data_register]) & source);
            result.state.data[data_register] = (result.state.data[data_register] & 0xffffff00U)
                | value;
            set_move_byte_flags(result.state, value);
            next_pc += 4U;
        } else if (opcode == 0x0c39U) { // CMPI.B #imm,(abs.l)
            std::uint16_t immediate_word = 0;
            std::uint32_t address = 0;
            if (!fetch_word(next_pc, immediate_word)
                || !fetch_long(next_pc + 2U, address)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            std::uint8_t destination = 0;
            if (!read_bytes(address, &destination, 1U)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            set_compare_byte_flags(result.state, destination,
                static_cast<std::uint8_t>(immediate_word));
            next_pc += 6U;
        } else if ((opcode & 0xfff8U) == 0x0240U) { // ANDI.W #imm,Dn
            std::uint16_t immediate = 0;
            if (!fetch_word(next_pc, immediate)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto data_register = static_cast<std::size_t>(opcode & 7U);
            const auto value = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(result.state.data[data_register]) & immediate);
            result.state.data[data_register] =
                (result.state.data[data_register] & 0xffff0000U) | value;
            set_move_word_flags(result.state, value);
            next_pc += 2U;
        } else if (opcode == 0x13b0U) { // MOVE.B d8(A0,Dn.W),d8(A1,Dn.W)
            std::uint16_t source_extension = 0;
            std::uint16_t destination_extension = 0;
            if (!fetch_word(next_pc, source_extension)
                || !fetch_word(next_pc + 2U, destination_extension)
                || (source_extension & 0x0700U) != 0
                || (destination_extension & 0x0700U) != 0) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto indexed_address = [&](const std::uint32_t base,
                                             const std::uint16_t extension) {
                const auto index_register = static_cast<std::size_t>((extension >> 12U) & 7U);
                const auto index_value = (extension & 0x8000U) != 0
                    ? result.state.address[index_register]
                    : result.state.data[index_register];
                const auto index_low_word = static_cast<std::uint16_t>(index_value);
                const auto signed_index = (extension & 0x0800U) != 0
                    ? static_cast<std::int64_t>(index_value & 0x7fffffffU)
                        - ((index_value & 0x80000000U) != 0 ? 0x80000000LL : 0LL)
                    : static_cast<std::int64_t>(index_low_word & 0x7fffU)
                        - ((index_low_word & 0x8000U) != 0 ? 0x8000LL : 0LL);
                const auto displacement_low_byte = static_cast<std::uint8_t>(extension);
                const auto displacement = static_cast<std::int64_t>(
                    displacement_low_byte & 0x7fU)
                    - ((displacement_low_byte & 0x80U) != 0 ? 0x80LL : 0LL);
                return static_cast<std::uint32_t>(static_cast<std::int64_t>(base)
                    + signed_index + displacement);
            };
            const auto source_address = indexed_address(result.state.address[0], source_extension);
            const auto destination_address = indexed_address(result.state.address[1], destination_extension);
            std::uint8_t value = 0;
            if (!read_bytes(source_address, &value, 1U)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            auto* destination = writable_range(destination_address, 1U);
            if (!destination) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            destination->bytes[static_cast<std::size_t>(
                destination_address - destination->base)] = value;
            set_move_byte_flags(result.state, value);
            next_pc += 4U;
        } else if ((opcode & 0xf1ffU) == 0x5079U) { // ADDQ.W #n,(abs.l)
            std::uint32_t address = 0;
            std::uint16_t destination = 0;
            if (!fetch_long(next_pc, address) || !read_word(address, destination)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            auto* memory_destination = writable_range(address, 2U);
            if (!memory_destination) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            auto quick = static_cast<std::uint8_t>((opcode >> 9U) & 7U);
            if (quick == 0U) quick = 8U;
            const auto value = add_quick_word_flags(destination, quick, result.state);
            const auto offset = static_cast<std::size_t>(address - memory_destination->base);
            memory_destination->bytes[offset] = static_cast<std::uint8_t>(value >> 8U);
            memory_destination->bytes[offset + 1U] = static_cast<std::uint8_t>(value);
            next_pc += 4U;
        } else if (opcode == 0x207cU || opcode == 0x227cU) { // MOVEA.L #imm, A0/A1
            std::uint32_t immediate = 0;
            if (!fetch_long(next_pc, immediate)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto address_register = opcode == 0x207cU ? 0U : 1U;
            result.state.address[address_register] = immediate;
            next_pc += 4U;
        } else if (opcode == 0x43f9U) { // LEA (absolute long), A1
            std::uint32_t address = 0;
            if (!fetch_long(next_pc, address)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            result.state.address[1] = address;
            next_pc += 4U;
        } else if (opcode == 0x2f01U) { // MOVE.L D1,-(A7)
            const auto stack = result.state.address[7];
            if (stack < 4U || ((stack - 4U) & 1U) != 0U) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto address = stack - 4U;
            auto* destination = writable_range(address, 4U);
            if (!destination) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto value = result.state.data[1];
            const auto offset = static_cast<std::size_t>(address - destination->base);
            destination->bytes[offset] = static_cast<std::uint8_t>(value >> 24U);
            destination->bytes[offset + 1U] = static_cast<std::uint8_t>(value >> 16U);
            destination->bytes[offset + 2U] = static_cast<std::uint8_t>(value >> 8U);
            destination->bytes[offset + 3U] = static_cast<std::uint8_t>(value);
            result.state.address[7] = address;
            set_move_long_flags(result.state, value);
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
        } else if (opcode == 0x0839U) { // BTST #imm,(absolute long), byte
            std::uint16_t bit_number = 0;
            std::uint32_t address = 0;
            std::uint8_t value = 0;
            if (!fetch_word(next_pc, bit_number) || !fetch_long(next_pc + 2U, address)
                || !read_bytes(address, &value, 1U)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto bit = static_cast<std::uint8_t>(bit_number & 7U);
            result.state.sr = static_cast<std::uint16_t>(result.state.sr & 0xfffbU);
            if ((value & static_cast<std::uint8_t>(1U << bit)) == 0U)
                result.state.sr |= 0x0004U;
            next_pc += 6U;
        } else if (opcode == 0x0c28U) { // CMPI.W #imm, d16(A0)
            std::uint16_t immediate = 0;
            std::uint16_t displacement = 0;
            std::uint16_t destination = 0;
            if (!fetch_word(next_pc, immediate)
                || !fetch_word(next_pc + 2U, displacement)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto address = static_cast<std::uint32_t>(
                static_cast<std::int64_t>(result.state.address[0])
                    + static_cast<std::int16_t>(displacement));
            if (!read_word(address, destination)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto difference = static_cast<std::uint16_t>(destination - immediate);
            const bool overflow = ((destination ^ immediate) & (destination ^ difference)
                & 0x8000U) != 0U;
            const bool borrow = destination < immediate;
            result.state.sr = static_cast<std::uint16_t>(result.state.sr & 0xfff0U);
            if ((difference & 0x8000U) != 0U) result.state.sr |= 0x0008U;
            if (difference == 0U) result.state.sr |= 0x0004U;
            if (overflow) result.state.sr |= 0x0002U;
            if (borrow) result.state.sr |= 0x0001U;
            next_pc += 4U;
        } else if (opcode == 0x0c40U) { // CMPI.W #imm, D0
            std::uint16_t immediate = 0;
            if (!fetch_word(next_pc, immediate)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto destination = static_cast<std::uint16_t>(result.state.data[0]);
            const auto difference = static_cast<std::uint16_t>(destination - immediate);
            const bool overflow = ((destination ^ immediate) & (destination ^ difference)
                & 0x8000U) != 0U;
            const bool borrow = destination < immediate;
            result.state.sr = static_cast<std::uint16_t>(result.state.sr & 0xfff0U);
            if ((difference & 0x8000U) != 0U) result.state.sr |= 0x0008U;
            if (difference == 0U) result.state.sr |= 0x0004U;
            if (overflow) result.state.sr |= 0x0002U;
            if (borrow) result.state.sr |= 0x0001U;
            next_pc += 2U;
        } else if (opcode == 0x0240U) { // ANDI.W #imm,D0
            std::uint16_t immediate = 0;
            if (!fetch_word(next_pc, immediate)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto value = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(result.state.data[0]) & immediate);
            result.state.data[0] = (result.state.data[0] & 0xffff0000U) | value;
            set_move_word_flags(result.state, value);
            next_pc += 2U;
        } else if (opcode == 0xb03cU) { // CMP.B #imm,D0
            std::uint16_t immediate = 0;
            if (!fetch_word(next_pc, immediate)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto destination = static_cast<std::uint8_t>(result.state.data[0]);
            const auto source = static_cast<std::uint8_t>(immediate);
            const auto difference = static_cast<std::uint8_t>(destination - source);
            const bool overflow = ((destination ^ source) & (destination ^ difference)
                & 0x80U) != 0U;
            const bool borrow = destination < source;
            result.state.sr = static_cast<std::uint16_t>(result.state.sr & 0xfff0U);
            if ((difference & 0x80U) != 0U) result.state.sr |= 0x0008U;
            if (difference == 0U) result.state.sr |= 0x0004U;
            if (overflow) result.state.sr |= 0x0002U;
            if (borrow) result.state.sr |= 0x0001U;
            next_pc += 2U;
        } else if ((opcode & 0xf1ffU) == 0xb0bcU) { // CMP.L #imm,Dn
            std::uint32_t immediate = 0;
            if (!fetch_long(next_pc, immediate)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto data_register = static_cast<std::size_t>((opcode >> 9U) & 7U);
            set_compare_long_flags(result.state, result.state.data[data_register], immediate);
            next_pc += 4U;
        } else if (opcode == 0x3228U) { // MOVE.W d16(A0),D1
            std::uint16_t displacement = 0;
            std::uint16_t value = 0;
            if (!fetch_word(next_pc, displacement)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto address = static_cast<std::uint32_t>(
                static_cast<std::int64_t>(result.state.address[0])
                    + static_cast<std::int16_t>(displacement));
            if (!read_word(address, value)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            result.state.data[1] = (result.state.data[1] & 0xffff0000U) | value;
            set_move_word_flags(result.state, value);
            next_pc += 2U;
        } else if (opcode == 0x4240U) { // CLR.W D0
            result.state.data[0] &= 0xffff0000U;
            result.state.sr = static_cast<std::uint16_t>(result.state.sr & 0xfff0U);
            result.state.sr |= 0x0004U;
        } else if (opcode == 0xe549U || opcode == 0xe349U) { // LSL.W #2/#1,D1
            const auto count = static_cast<unsigned>(opcode == 0xe549U ? 2U : 1U);
            auto value = static_cast<std::uint16_t>(result.state.data[1]);
            bool carry = false;
            bool overflow = false;
            for (unsigned index = 0; index < count; ++index) {
                const bool prior_sign = (value & 0x8000U) != 0U;
                carry = prior_sign;
                value = static_cast<std::uint16_t>(value << 1U);
                overflow = overflow || (prior_sign != ((value & 0x8000U) != 0U));
            }
            result.state.data[1] = (result.state.data[1] & 0xffff0000U) | value;
            result.state.sr = static_cast<std::uint16_t>(result.state.sr & 0xffe0U);
            if ((value & 0x8000U) != 0U) result.state.sr |= 0x0008U;
            if (value == 0U) result.state.sr |= 0x0004U;
            if (overflow) result.state.sr |= 0x0002U;
            if (carry) result.state.sr |= 0x0001U | 0x0010U;
        } else if (opcode == 0x5200U || opcode == 0x5400U) { // ADDQ.B #1/#2,D0
            const auto source = static_cast<std::uint8_t>(opcode == 0x5200U ? 1U : 2U);
            const auto destination = static_cast<std::uint8_t>(result.state.data[0]);
            const auto result_byte = static_cast<std::uint8_t>(destination + source);
            const bool carry = static_cast<unsigned>(destination) + source > 0xffU;
            const bool overflow = ((~(destination ^ source) & (destination ^ result_byte))
                & 0x80U) != 0U;
            result.state.data[0] = (result.state.data[0] & 0xffffff00U) | result_byte;
            result.state.sr = static_cast<std::uint16_t>(result.state.sr & 0xfff0U);
            if ((result_byte & 0x80U) != 0U) result.state.sr |= 0x0008U;
            if (result_byte == 0U) result.state.sr |= 0x0004U;
            if (overflow) result.state.sr |= 0x0002U;
            if (carry) result.state.sr |= 0x0001U | 0x0010U;
        } else if (opcode == 0xb079U) { // CMP.W (absolute long),D0
            std::uint32_t address = 0;
            std::uint16_t source = 0;
            if (!fetch_long(next_pc, address) || !read_word(address, source)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto destination = static_cast<std::uint16_t>(result.state.data[0]);
            const auto difference = static_cast<std::uint16_t>(destination - source);
            const bool overflow = ((destination ^ source) & (destination ^ difference)
                & 0x8000U) != 0U;
            const bool borrow = destination < source;
            result.state.sr = static_cast<std::uint16_t>(result.state.sr & 0xfff0U);
            if ((difference & 0x8000U) != 0U) result.state.sr |= 0x0008U;
            if (difference == 0U) result.state.sr |= 0x0004U;
            if (overflow) result.state.sr |= 0x0002U;
            if (borrow) result.state.sr |= 0x0001U;
            next_pc += 4U;
        } else if (opcode == 0x0800U) { // BTST #imm,D0
            std::uint16_t bit_number = 0;
            if (!fetch_word(next_pc, bit_number)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto bit = static_cast<std::uint8_t>(bit_number & 31U);
            result.state.sr = static_cast<std::uint16_t>(result.state.sr & 0xfffbU);
            if ((result.state.data[0] & (std::uint32_t{1} << bit)) == 0U)
                result.state.sr |= 0x0004U;
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
        } else if (opcode == 0x13fcU) { // MOVE.B #imm,(absolute long)
            std::uint16_t immediate = 0;
            std::uint32_t address = 0;
            if (!fetch_word(next_pc, immediate) || !fetch_long(next_pc + 2U, address)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            auto* destination = writable_range(address, 1U);
            if (!destination) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto offset = static_cast<std::size_t>(address - destination->base);
            const auto value = static_cast<std::uint8_t>(immediate);
            destination->bytes[offset] = value;
            set_move_byte_flags(result.state, value);
            next_pc += 6U;
        } else if (opcode == 0x1368U) { // MOVE.B d16(A0),d16(A1)
            std::uint16_t source_displacement = 0;
            std::uint16_t destination_displacement = 0;
            if (!fetch_word(next_pc, source_displacement)
                || !fetch_word(next_pc + 2U, destination_displacement)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto source_address = static_cast<std::uint32_t>(
                static_cast<std::int64_t>(result.state.address[0])
                    + static_cast<std::int16_t>(source_displacement));
            const auto destination_address = static_cast<std::uint32_t>(
                static_cast<std::int64_t>(result.state.address[1])
                    + static_cast<std::int16_t>(destination_displacement));
            std::uint8_t value = 0;
            if (!read_bytes(source_address, &value, 1U)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            auto* destination = writable_range(destination_address, 1U);
            if (!destination) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            destination->bytes[static_cast<std::size_t>(destination_address - destination->base)] = value;
            set_move_byte_flags(result.state, value);
            next_pc += 4U;
        } else if (opcode == 0x33e8U) { // MOVE.W d16(A0),(absolute long)
            std::uint16_t source_displacement = 0;
            std::uint32_t destination_address = 0;
            std::uint16_t value = 0;
            if (!fetch_word(next_pc, source_displacement)
                || !fetch_long(next_pc + 2U, destination_address)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto source_address = static_cast<std::uint32_t>(
                static_cast<std::int64_t>(result.state.address[0])
                    + static_cast<std::int16_t>(source_displacement));
            if ((destination_address & 1U) != 0U
                || !read_word(source_address, value)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            auto* destination = writable_range(destination_address, 2U);
            if (!destination) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto offset = static_cast<std::size_t>(destination_address - destination->base);
            destination->bytes[offset] = static_cast<std::uint8_t>(value >> 8U);
            destination->bytes[offset + 1U] = static_cast<std::uint8_t>(value);
            set_move_word_flags(result.state, value);
            next_pc += 6U;
        } else if (opcode == 0x1028U) { // MOVE.B d16(A0),D0
            std::uint16_t displacement = 0;
            if (!fetch_word(next_pc, displacement)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto address = static_cast<std::uint32_t>(
                static_cast<std::int64_t>(result.state.address[0])
                    + static_cast<std::int16_t>(displacement));
            std::uint8_t value = 0;
            if (!read_bytes(address, &value, 1U)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            result.state.data[0] = (result.state.data[0] & 0xffffff00U) | value;
            set_move_byte_flags(result.state, value);
            next_pc += 2U;
        } else if (opcode == 0x4a39U) { // TST.B (absolute long)
            std::uint32_t address = 0;
            std::uint8_t value = 0;
            if (!fetch_long(next_pc, address) || !read_bytes(address, &value, 1U)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            result.state.sr = static_cast<std::uint16_t>(result.state.sr & 0xfff0U);
            if ((value & 0x80U) != 0U) result.state.sr |= 0x0008U;
            if (value == 0U) result.state.sr |= 0x0004U;
            next_pc += 4U;
        } else if (opcode == 0x4a00U) { // TST.B D0
            const auto value = static_cast<std::uint8_t>(result.state.data[0]);
            result.state.sr = static_cast<std::uint16_t>(result.state.sr & 0xfff0U);
            if ((value & 0x80U) != 0U) result.state.sr |= 0x0008U;
            if (value == 0U) result.state.sr |= 0x0004U;
        } else if ((opcode & 0xf1ffU) == 0x3039U) { // MOVE.W (absolute long),Dn
            std::uint32_t address = 0;
            std::uint16_t value = 0;
            if (!fetch_long(next_pc, address) || !read_word(address, value)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto data_register = static_cast<std::size_t>((opcode >> 9U) & 7U);
            result.state.data[data_register] =
                (result.state.data[data_register] & 0xffff0000U) | value;
            set_move_word_flags(result.state, value);
            next_pc += 4U;
        } else if (opcode == 0x3028U) { // MOVE.W d16(A0),D0
            std::uint16_t displacement = 0;
            std::uint16_t value = 0;
            if (!fetch_word(next_pc, displacement)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            const auto address = static_cast<std::uint32_t>(
                static_cast<std::int64_t>(result.state.address[0])
                    + static_cast<std::int16_t>(displacement));
            if (!read_word(address, value)) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            result.state.data[0] = (result.state.data[0] & 0xffff0000U) | value;
            set_move_word_flags(result.state, value);
            next_pc += 2U;
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
        } else if (opcode == 0x221fU) { // MOVE.L (A7)+,D1
            const auto stack = result.state.address[7];
            if ((stack & 1U) != 0U || stack > std::numeric_limits<std::uint32_t>::max() - 3U) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            std::uint8_t bytes[4]{};
            if (!read_bytes(stack, bytes, 4U)
                || stack > std::numeric_limits<std::uint32_t>::max() - 4U) {
                result.reason = StopReason::memory_fault;
                return result;
            }
            result.state.data[1] = (static_cast<std::uint32_t>(bytes[0]) << 24U)
                | (static_cast<std::uint32_t>(bytes[1]) << 16U)
                | (static_cast<std::uint32_t>(bytes[2]) << 8U) | bytes[3];
            result.state.address[7] += 4U;
            set_move_long_flags(result.state, result.state.data[1]);
        } else if (opcode == 0x4280U) { // CLR.L D0
            result.state.data[0] = 0;
            result.state.sr = static_cast<std::uint16_t>(result.state.sr & 0xfff0U);
            result.state.sr |= 0x0004U;
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
        } else if ((opcode & 0xff00U) == 0x6000U
            || (opcode & 0xff00U) == 0x6400U
            || (opcode & 0xff00U) == 0x6600U
            || (opcode & 0xff00U) == 0x6700U
            || (opcode & 0xff00U) == 0x6d00U) { // BRA/BNE/BEQ/BLT
            const auto condition = static_cast<std::uint8_t>((opcode >> 8U) & 0x0fU);
            const bool is_bra = condition == 0U;
            const bool is_bcc = condition == 4U;
            const bool is_bne = condition == 6U;
            const bool is_beq = condition == 7U;
            const bool is_blt = condition == 13U;
            if (!is_bra && !is_bcc && !is_bne && !is_beq && !is_blt) {
                result.reason = StopReason::unsupported_instruction;
                return result;
            }
            std::int32_t displacement = static_cast<std::int8_t>(opcode & 0xffU);
            if ((opcode & 0xffU) == 0U) {
                std::uint16_t extension = 0;
                if (!fetch_word(next_pc, extension)) {
                    result.reason = StopReason::memory_fault;
                    return result;
                }
                displacement = static_cast<std::int16_t>(extension);
                next_pc += 2U;
            }
            const auto target = static_cast<std::uint32_t>(
                static_cast<std::int64_t>(instruction_pc) + 2 + displacement);
            const bool taken = is_bra || (is_bcc && (result.state.sr & 0x0001U) == 0U)
                || (is_bne && (result.state.sr & 0x0004U) == 0U)
                || (is_beq && (result.state.sr & 0x0004U) != 0U)
                || (is_blt && ((result.state.sr & 0x0008U) != 0U)
                    != ((result.state.sr & 0x0002U) != 0U));
            result.branches.push_back({instruction_pc, result.state.sr, target, taken});
            if (taken) next_pc = target;
        } else {
            result.reason = StopReason::unsupported_instruction;
            return result;
        }

        result.state.pc = next_pc;
        ++result.instructions_executed;
    }

    result.reason = std::find(stop_addresses.begin(), stop_addresses.end(), result.state.pc)
            != stop_addresses.end()
        ? StopReason::requested_address
        : StopReason::instruction_limit;
    return result;
}

} // namespace eon::m68k
