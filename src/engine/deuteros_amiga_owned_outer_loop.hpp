#pragma once

#include "engine/bounded_memory_transfer.hpp"

#include <cstdint>
#include <stdexcept>

namespace eon {

struct DeuterosAmigaOuterInputRoute {
    std::uint32_t next_instruction=0;
    std::uint32_t d0=0;
};

// The caller validates the ordered port observation before publishing this
// private transaction. Values are raw port bytes, not host button meanings.
template<class Read,class Write>
DeuterosAmigaOuterInputRoute execute_deuteros_amiga_owned_outer_input(
    std::uint32_t instruction,std::uint8_t value,std::uint32_t d0,Read read,Write write){
    using Width=MemoryTransferElementWidth;
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
            return {0x21850,d0};
        if(read(0x210f4,1)!=0)return {0x21892,d0};
        return {0x2185e,d0};
    }
    if(instruction==0x2185e){
        write(0xbfe001,Width::byte,value);
        if((value&0x40U)==0)write(0x21720,Width::byte,1);
        if(read(0x2171e,1)!=0||read(0x21720,1)==0)return {0x21380,d0};
        return {(counter_word()&1U)?0x21380U:0x21982U,d0};
    }
    throw std::runtime_error("Unsupported Deuteros outer input instruction");
}

} // namespace eon
