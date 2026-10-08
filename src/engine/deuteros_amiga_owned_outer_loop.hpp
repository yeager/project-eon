#pragma once

#include "engine/bounded_memory_transfer.hpp"
#include "engine/deuteros_amiga_owned_commands.hpp"
#include "data/m68k_executor.hpp"
#include "data/sha256.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace eon {

struct DeuterosAmigaOuterInputRoute {
    std::uint32_t next_instruction=0;
    std::uint32_t d0=0;
    std::uint32_t caller_return=0;
    std::uint32_t fade_return=0;
    std::uint32_t library=0;
    std::uint16_t status_register=0;
    std::uint64_t instructions_executed=0;
    std::vector<m68k::BranchCheckpoint> branch_checkpoints;

    DeuterosAmigaOuterInputRoute() = default;
    DeuterosAmigaOuterInputRoute(std::uint32_t next, std::uint32_t register_zero,
        std::uint32_t caller=0, std::uint32_t fade=0, std::uint32_t library_base=0,
        std::uint16_t status=0, std::uint64_t instruction_count=0,
        std::vector<m68k::BranchCheckpoint> branches={})
        : next_instruction(next), d0(register_zero), caller_return(caller),
          fade_return(fade), library(library_base), status_register(status),
          instructions_executed(instruction_count), branch_checkpoints(std::move(branches)) {}
};

template<class Read,class Write>
DeuterosAmigaOuterInputRoute execute_deuteros_amiga_owned_outer_transition(
    std::uint32_t instruction,std::uint32_t d0,Read read,Write write);

template<class Read, class Write>
DeuterosAmigaOuterInputRoute execute_deuteros_amiga_owned_secondary_input(
    const std::span<const std::uint8_t> code, const std::uint8_t port_value,
    const std::uint32_t initial_d0, Read read, Write write) {
    constexpr std::uint32_t entry = 0x2185e;
    constexpr std::uint32_t branch_join = 0x21870;
    constexpr std::uint32_t scheduler = 0x2181c;
    constexpr std::uint32_t transition = 0x21982;
    if (code.size() != 0x34U
        || to_hex(sha256(code))
            != "82eaf3c25827471bcc4005c155722c63bd6913af294677ab592162b13904fd86")
        throw std::runtime_error("Deuteros secondary input source span is unavailable");

    std::array<std::uint8_t, 1> port{port_value};
    std::array<std::uint8_t, 1> latch{};
    std::array<m68k::MemoryRange, 2> first_memory{{
        {0xbfe001, port, false}, {0x21720, latch, true}}};
    m68k::MachineState state;
    state.pc = entry;
    state.data[0] = initial_d0;
    std::vector<m68k::BranchCheckpoint> branches;
    std::uint64_t instructions = 0;

    const auto run = [&](std::span<m68k::MemoryRange> memory,
                         const std::uint64_t limit,
                         const std::span<const std::uint32_t> stops) {
        const auto result = m68k::execute(code, entry, state, memory, limit, stops);
        if (result.reason != m68k::StopReason::requested_address)
            throw std::runtime_error("Deuteros secondary input instruction path is unsupported or incomplete");
        state = result.state;
        instructions += result.instructions_executed;
        branches.insert(branches.end(), result.branches.begin(), result.branches.end());
    };

    const std::array<std::uint32_t, 1> join_stop{branch_join};
    run(first_memory, 3U, join_stop);
    if (state.pc != branch_join || branches.size() != 1U
        || branches.front().instruction_address != 0x21866U)
        throw std::runtime_error("Deuteros secondary input did not reach the hash-bound branch join");
    const bool latch_store_executed = !branches.front().taken;

    std::array<std::uint8_t, 1> first_latch{static_cast<std::uint8_t>(read(0x2171e, 1))};
    std::array<m68k::MemoryRange, 1> first_flag_memory{{{0x2171e, first_latch, false}}};
    const std::array<std::uint32_t, 2> flag_stops{0x21878U, 0x21880U};
    run(first_flag_memory, 2U, flag_stops);

    if (state.pc == 0x21878U) {
        if (!latch_store_executed)
            latch[0] = static_cast<std::uint8_t>(read(0x21720, 1));
        std::array<m68k::MemoryRange, 1> latch_memory{{{0x21720, latch, false}}};
        const std::array<std::uint32_t, 2> latch_stops{0x21882U, 0x21880U};
        run(latch_memory, 2U, latch_stops);
    }

    if (state.pc == 0x21880U) {
        std::array<m68k::MemoryRange, 0> no_memory{};
        const std::array<std::uint32_t, 2> terminal_stops{scheduler, transition};
        run(no_memory, 1U, terminal_stops);
    } else if (state.pc == 0x21882U) {
        const auto counter = static_cast<std::uint16_t>(read(0x21696, 2));
        std::array<std::uint8_t, 2> counter_bytes{
            static_cast<std::uint8_t>(counter >> 8U), static_cast<std::uint8_t>(counter)};
        std::array<m68k::MemoryRange, 1> counter_memory{{{0x21696, counter_bytes, false}}};
        const std::array<std::uint32_t, 2> terminal_stops{scheduler, transition};
        run(counter_memory, 4U, terminal_stops);
    }

    if (state.pc != scheduler && state.pc != transition)
        throw std::runtime_error("Deuteros secondary input stopped at an unsupported instruction");
    if (branches.size() > 5U)
        throw std::runtime_error("Deuteros secondary input exceeded its bounded branch trace");
    const auto commit_input_writes = [&]() {
        write(0xbfe001, MemoryTransferElementWidth::byte, port_value);
        if (latch_store_executed)
            write(0x21720, MemoryTransferElementWidth::byte, latch[0]);
    };

    if(state.pc==transition){
        auto route=execute_deuteros_amiga_owned_outer_transition(
            transition,state.data[0],read,write);
        commit_input_writes();
        route.status_register=state.sr;
        route.instructions_executed=instructions;
        route.branch_checkpoints=std::move(branches);
        return route;
    }
    commit_input_writes();
    DeuterosAmigaOuterInputRoute route;
    route.next_instruction = state.pc;
    route.d0 = state.data[0];
    route.status_register = state.sr;
    route.instructions_executed = instructions;
    route.branch_checkpoints = std::move(branches);
    return route;
}

template<class Read,class Write>
std::uint32_t restart_deuteros_amiga_owned_loop(Read read,Write write){
    using Width=MemoryTransferElementWidth;
    std::uint32_t d0=0;
    write(0x22a30,Width::byte,0);
    std::uint16_t channels=15;
    stage_deuteros_amiga_owned_sound(d0,channels,read,write);
    write(0x21720,Width::word,0);
    write(0x2171e,Width::word,0);
    write(0x210f2,Width::word,1);
    return d0;
}

// $224a2 restores the owned vector operand and emits raw audio-control
// writes; $22a5a then resets the software descriptors. These writes alone
// are not a host-audio acknowledgement or a hardware readback model.
template<class Read,class Write>
std::uint32_t execute_deuteros_amiga_owned_cleanup(Read read,Write write){
    using Width=MemoryTransferElementWidth;
    write(0x6c,Width::longword,read(0x224e6,4));
    for(std::uint32_t channel=0;channel<4;++channel)
        write(0xdff0a8+channel*16,Width::word,0);
    write(0xdff096,Width::word,15);
    write(0x22a30,Width::byte,0);
    std::uint32_t d0=0;
    std::uint16_t channels=15;
    stage_deuteros_amiga_owned_sound(d0,channels,read,write);
    return d0;
}

// Follow the local cleanup prefix, retaining the primary BSR's return site.
// Stop before fade service calls or the asynchronous counter read.
template<class Read,class Write>
DeuterosAmigaOuterInputRoute execute_deuteros_amiga_owned_outer_transition(
    std::uint32_t instruction,std::uint32_t d0,Read read,Write write){
    using Width=MemoryTransferElementWidth;
    const auto caller_return=instruction==0x21850?0x21854U:0U;
    if(instruction==0x21850)instruction=0x218cc;
    if(instruction==0x21982){
        d0=(d0&0xffff0000U)|read(0x21704,2);
        const auto selector=d0&0xffU; // Original CMP.B, not CMP.W.
        if(selector>2)return {0x21380,d0,0};
        if(selector<2)write(0x21704,Width::word,1);
        instruction=0x218cc;
    }
    if(instruction!=0x21892&&instruction!=0x218cc)
        throw std::runtime_error("Unsupported Deuteros outer transition instruction");
    const bool restart=instruction==0x21892;
    d0=read(0x2126a,4);
    if(d0!=0){
        write(0x2229a,Width::word,0x100);
        write(0x207ea,Width::byte,0);
        return {0x222ac,d0,caller_return,restart?0x2189eU:0x218d8U};
    }
    // $22a5a tail-calls the existing native $22ab8 software staging routine.
    d0=0;
    write(0x22a30,Width::byte,0);
    std::uint16_t channels=15;
    stage_deuteros_amiga_owned_sound(d0,channels,read,write);
    return {restart?0x218a8U:0x218e2U,d0,caller_return};
}

// The caller validates the ordered port observation before publishing this
// private transaction. Values are raw port bytes, not host button meanings.
template<class Read,class Write>
DeuterosAmigaOuterInputRoute execute_deuteros_amiga_owned_outer_input(
    std::uint32_t instruction,std::uint8_t value,std::uint32_t d0,Read read,Write write){
    using Width=MemoryTransferElementWidth;
    if(instruction==0x217e4){
        write(0xbfe001,Width::byte,value|2U);
        d0=(d0&0xffff0000U)|read(0x21704,2);
        return {0x21926,d0,0x217f6};
    }
    if(instruction==0x218be){
        write(0xbfe001,Width::byte,value);
        if((value&0x40U)==0)return {0x218be,d0};
        // $217f6 re-enters the loop without reloading the original resource.
        d0=restart_deuteros_amiga_owned_loop(read,write);
        return {0x21816,d0};
    }
    if(instruction==0x222ac){
        write(0xdff01f,Width::byte,value);
        if((value&0x20U)==0)return {0x222ac,d0};
        for(std::uint32_t index=0;index<16;++index){
            auto color=read(0x12ecc+index*2,2);
            if(color>=0x100U)color-=0x100U;
            if((color&0xffU)>=0x10U)color-=0x10U;
            if((color&15U)!=0)--color;
            write(0x12ecc+index*2,Width::word,color);
        }
        return {0x222fc,(d0&0xffff0000U)|16U,0,0,read(0x12fec,4)};
    }
    const auto counter_word=[&](){
        const auto counter=read(0x21696,2);
        d0=(d0&0xffff0000U)|counter;
        return counter;
    };
    if(instruction==0x21822){
        write(0xdff016,Width::byte,value);
        // BTST #10 against memory tests bit 10 modulo 8 in the byte.
        if((value&4U)==0)write(0x21721,Width::byte,1);
        if(read(0x2171e,1)==0&&read(0x21721,1)!=0&&(counter_word()&1U)==0)
            return execute_deuteros_amiga_owned_outer_transition(0x21850,d0,read,write);
        if(read(0x210f4,1)!=0)
            return execute_deuteros_amiga_owned_outer_transition(0x21892,d0,read,write);
        return {0x2185e,d0};
    }
    if(instruction==0x2185e){
        throw std::runtime_error("Deuteros secondary input requires its hash-bound instruction span");
    }
    throw std::runtime_error("Unsupported Deuteros outer input instruction");
}

template<class Read,class Write>
DeuterosAmigaOuterInputRoute execute_deuteros_amiga_owned_outer_input(
    std::uint32_t instruction,std::uint8_t value,std::uint32_t d0,Read read,Write write,
    const std::span<const std::uint8_t> secondary_input_code){
    if(instruction==0x2185e)
        return execute_deuteros_amiga_owned_secondary_input(secondary_input_code,value,d0,
            read,write);
    return execute_deuteros_amiga_owned_outer_input(instruction,value,d0,read,write);
}

} // namespace eon
