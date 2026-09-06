#pragma once

#include "engine/bounded_memory_transfer.hpp"
#include "engine/deuteros_amiga_owned_commands.hpp"

#include <cstdint>
#include <stdexcept>

namespace eon {

struct DeuterosAmigaOuterInputRoute {
    std::uint32_t next_instruction=0;
    std::uint32_t d0=0;
    std::uint32_t caller_return=0;
    std::uint32_t fade_return=0;
    std::uint32_t library=0;
};

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
        write(0xbfe001,Width::byte,value);
        if((value&0x40U)==0)write(0x21720,Width::byte,1);
        if(read(0x2171e,1)!=0||read(0x21720,1)==0)return {0x21380,d0};
        if(counter_word()&1U)return {0x21380,d0};
        return execute_deuteros_amiga_owned_outer_transition(0x21982,d0,read,write);
    }
    throw std::runtime_error("Unsupported Deuteros outer input instruction");
}

} // namespace eon
