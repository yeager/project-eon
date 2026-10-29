#include "data/m68k_executor.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <span>

namespace {

using eon::m68k::ExecutionResult;
using eon::m68k::MachineState;
using eon::m68k::MemoryRange;
using eon::m68k::StopReason;

ExecutionResult run(std::span<const std::uint8_t> code, MachineState state,
    std::span<MemoryRange> memory = {}, std::uint64_t steps = 16,
    std::uint32_t stop = 0xffffffffU, std::uint32_t base = 0x1000U) {
    return eon::m68k::execute(code, base, state, memory, steps, stop);
}

} // namespace

int main() {
    {
        const std::array<std::uint8_t, 6> code{0x22, 0x7c, 0x00, 0x07, 0x70, 0x00};
        const auto result = run(code, {.pc = 0x1000U});
        assert(result.reason == StopReason::memory_fault);
        assert(result.instructions_executed == 1 && result.state.pc == 0x1006U);
        assert(result.state.address[1] == 0x77000U);
    }
    {
        const std::array<std::uint8_t, 4> code{0x30, 0x3c, 0x01, 0x00};
        MachineState initial{.pc = 0x1000U};
        initial.data[0] = 0xabcdffffU;
        const auto result = run(code, initial);
        assert(result.state.data[0] == 0xabcd0100U && result.state.pc == 0x1004U);
    }
    {
        const std::array<std::uint8_t, 2> code{0x32, 0xd8};
        std::array<std::uint8_t, 2> source{0x12, 0x34};
        std::array<std::uint8_t, 2> destination{};
        std::array<MemoryRange, 2> memory{{{0x3000U, source, false},
            {0x4000U, destination, true}}};
        MachineState initial{.pc = 0x1000U};
        initial.address[0] = 0x3000U;
        initial.address[1] = 0x4000U;
        const auto result = run(code, initial, memory, 1);
        assert(result.instructions_executed == 1 && result.state.pc == 0x1002U);
        assert(result.state.address[0] == 0x3002U && result.state.address[1] == 0x4002U);
        assert((destination == std::array<std::uint8_t, 2>{0x12, 0x34}));
    }
    {
        const std::array<std::uint8_t, 4> code{0x51, 0xc8, 0xff, 0xfc};
        MachineState initial{.pc = 0x1000U};
        initial.data[0] = 1;
        const auto taken = run(code, initial);
        assert(taken.instructions_executed == 1 && taken.state.data[0] == 0);
        assert(taken.state.pc == 0x0ffeU);
        initial.data[0] = 0;
        const auto expired = run(code, initial);
        assert(expired.state.data[0] == 0xffffU && expired.state.pc == 0x1004U);
    }
    {
        const std::array<std::uint8_t, 6> code{0x4e, 0xf9, 0x00, 0x07, 0x70, 0x00};
        const auto result = run(code, {.pc = 0x1000U}, {}, 4, 0x77000U);
        assert(result.reason == StopReason::requested_address);
        assert(result.instructions_executed == 1 && result.state.pc == 0x77000U);
    }
    {
        // Reaching the stop address on the final permitted instruction is a
        // completed bounded execution, not an instruction-limit failure.
        const std::array<std::uint8_t, 6> code{0x4e, 0xf9, 0x00, 0x07, 0x70, 0x00};
        const auto result = run(code, {.pc = 0x1000U}, {}, 1, 0x77000U);
        assert(result.reason == StopReason::requested_address);
        assert(result.instructions_executed == 1 && result.state.pc == 0x77000U);
    }
    {
        const auto result = run({}, {.pc = 0x1000U}, {}, 0, 0x1000U);
        assert(result.reason == StopReason::requested_address);
        assert(result.instructions_executed == 0 && result.state.pc == 0x1000U);
    }
    {
        // The byte-set is the opening instruction of the hash-bound
        // Deuteros CIA prefix at $217e4 (PRESERVATION.md, SHA-256
        // cb9046ad20fffc0a52f43431949927b4d159ecc795fe5f62dbbd01accd0684c7).
        const std::array<std::uint8_t, 8> code{
            0x08, 0xf9, 0x00, 0x01, 0x00, 0xbf, 0xe0, 0x01};
        std::array<std::uint8_t, 1> port{0x40};
        std::array<MemoryRange, 1> memory{{{0xbfe001U, port, true}}};
        MachineState initial{.pc = 0x1000U, .sr = 0xa711U};
        const auto result = run(code, initial, memory, 1, 0x1008U);
        assert(result.reason == StopReason::requested_address);
        assert(result.instructions_executed == 1 && result.state.pc == 0x1008U);
        assert(port[0] == 0x42U);
        assert(result.state.sr == 0xa715U); // prior bit clear sets Z; X and upper SR survive

        port[0] = 0x42U;
        const auto already_set = run(code, initial, memory, 1, 0x1008U);
        assert(already_set.reason == StopReason::requested_address);
        assert(port[0] == 0x42U && already_set.state.sr == 0xa711U);
    }
    {
        const std::array<std::uint8_t, 6> code{
            0x30, 0x39, 0x00, 0x02, 0x17, 0x04}; // MOVE.W $21704,D0
        std::array<std::uint8_t, 2> source{0x80, 0x00};
        std::array<MemoryRange, 1> memory{{{0x21704U, source, false}}};
        MachineState initial{.pc = 0x1000U, .sr = 0x2717U};
        initial.data[0] = 0xabcd1234U;
        const auto result = run(code, initial, memory, 1, 0x1006U);
        assert(result.reason == StopReason::requested_address);
        assert(result.state.data[0] == 0xabcd8000U);
        assert(result.state.sr == 0x2718U); // MOVE.W sets N, clears Z/V/C, preserves X
    }
    {
        const std::array<std::uint8_t, 2> code{0x4e, 0x41}; // TRAP #1
        const auto result = run(code, {.pc = 0x1000U});
        assert(result.reason == StopReason::trap_instruction);
        assert(result.instructions_executed == 0 && result.stop_opcode == 0x4e41U);
    }
    {
        const std::array<std::uint8_t, 2> code{0x4e, 0x71}; // NOP is outside this slice
        const auto result = run(code, {.pc = 0x1000U});
        assert(result.reason == StopReason::unsupported_instruction);
        assert(result.instructions_executed == 0 && result.stop_opcode == 0x4e71U);
    }
    {
        // Exact 28-byte BSS-entry span; its SHA-256 is the existing anchor
        // bae3f526a7a7e42ca59d840ed80606f0f2b5a9f420fe221ddd30f04d9388e30b.
        const std::array<std::uint8_t, 28> code{
            0x22, 0x7c, 0x00, 0x07, 0x70, 0x00, // MOVEA.L #$77000,A1
            0x20, 0x7c, 0x00, 0x01, 0xd6, 0x52, // MOVEA.L #$1d652,A0
            0x30, 0x3c, 0x01, 0x00,             // MOVE.W #$100,D0
            0x32, 0xd8,                         // MOVE.W (A0)+,(A1)+
            0x51, 0xc8, 0xff, 0xfc,             // DBF D0,-4
            0x4e, 0xf9, 0x00, 0x07, 0x70, 0x00  // JMP $77000
        };
        std::array<std::uint8_t, 0x202> source{};
        std::array<std::uint8_t, 0x202> destination{};
        for (std::size_t i = 0; i < source.size(); ++i)
            source[i] = static_cast<std::uint8_t>((i * 37U + 11U) & 0xffU);
        const auto original_source = source;
        std::array<MemoryRange, 2> memory{{{0x1d652U, source, false},
            {0x77000U, destination, true}}};
        const auto result = eon::m68k::execute(code, 0x1d636U,
            {.pc = 0x1d636U}, memory, 600, 0x77000U);
        assert(result.reason == StopReason::requested_address);
        assert(result.instructions_executed == 518 && result.state.pc == 0x77000U);
        assert(result.state.address[0] == 0x1d854U && result.state.address[1] == 0x77202U);
        assert(result.state.data[0] == 0xffffU && destination == original_source);
        assert(source == original_source);
    }
    {
        const std::array<std::uint8_t, 2> code{0x32, 0xd8};
        std::array<std::uint8_t, 2> source{0x12, 0x34};
        std::array<std::uint8_t, 2> destination{};
        std::array<MemoryRange, 2> memory{{{0x3000U, source, false},
            {0x4000U, destination, false}}};
        MachineState initial{.pc = 0x1000U};
        initial.address[0] = 0x3000U;
        initial.address[1] = 0x4000U;
        const auto result = run(code, initial, memory, 1);
        assert(result.reason == StopReason::memory_fault);
        assert(result.instructions_executed == 0 && destination[0] == 0 && destination[1] == 0);
    }
    {
        const std::array<std::uint8_t, 2> code{0x32, 0xd8};
        std::array<std::uint8_t, 1> source_high{0x12};
        std::array<std::uint8_t, 1> source_low{0x34};
        std::array<std::uint8_t, 2> destination{};
        std::array<MemoryRange, 3> memory{{{0x3000U, source_high, false},
            {0x3001U, source_low, false}, {0x4000U, destination, true}}};
        MachineState initial{.pc = 0x1000U};
        initial.address[0] = 0x3000U;
        initial.address[1] = 0x4000U;
        const auto result = run(code, initial, memory, 1);
        assert(result.reason == StopReason::memory_fault);
        assert(result.instructions_executed == 0 && result.state.address[0] == 0x3000U);
        assert(destination[0] == 0 && destination[1] == 0);
    }
    {
        // A word read must not join an external byte with the first code byte.
        const std::array<std::uint8_t, 7> code{
            0x34, 0x30, 0x39, 0x00, 0x00, 0x30, 0x00};
        std::array<std::uint8_t, 1> source{0xab};
        std::array<MemoryRange, 1> memory{{{0x3000U, source, false}}};
        MachineState initial{.pc = 0x3002U};
        const auto result = eon::m68k::execute(code, 0x3001U, initial,
            memory, 1, 0xffffffffU);
        assert(result.reason == StopReason::memory_fault);
        assert(result.instructions_executed == 0 && result.state.data[0] == 0);
    }
    {
        // Executable bytes are not an implicit data-memory range.
        const std::array<std::uint8_t, 6> code{
            0x30, 0x39, 0x00, 0x00, 0x10, 0x00};
        MachineState initial{.pc = 0x1000U};
        const auto result = run(code, initial, {}, 1);
        assert(result.reason == StopReason::memory_fault);
        assert(result.instructions_executed == 0 && result.state.data[0] == 0);
    }
    {
        const std::array<std::uint8_t, 2> code{0x4e, 0x75};
        std::array<std::uint8_t, 2> return_high{0x00, 0x00};
        std::array<std::uint8_t, 2> return_low{0x10, 0x10};
        std::array<MemoryRange, 2> memory{{{0x5000U, return_high, false},
            {0x5002U, return_low, false}}};
        MachineState initial{.pc = 0x1000U};
        initial.address[7] = 0x5000U;
        const auto result = run(code, initial, memory, 1);
        assert(result.reason == StopReason::memory_fault);
        assert(result.instructions_executed == 0 && result.state.address[7] == 0x5000U);
    }
}
