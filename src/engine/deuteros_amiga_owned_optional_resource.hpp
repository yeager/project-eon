#pragma once

#include "engine/native_runtime_memory.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>

namespace eon {

struct DeuterosAmigaOptionalResourceInitResult {
    std::uint32_t d0=0,d1=0,d2=0,d7=0;
    std::uint32_t a0=0,a1=0,a2=0;
};

// Native execution of the hash-bound $22330..$224a0 optional-resource
// initializer. The caller supplies a read-through private write journal and
// publishes it only after the complete operation and session transition pass.
template<class Read,class Write>
DeuterosAmigaOptionalResourceInitResult initialize_deuteros_amiga_owned_optional_resource(
    const std::uint32_t expected_base,const std::uint8_t prior_cia,Read&& read,Write&& write){
    using Width=MemoryTransferElementWidth;
    const auto base=read(0x2126a,4);
    if(base==0||base!=expected_base||(base&1U)!=0||base>0xffffffU-0xf4U)
        throw std::runtime_error("Deuteros optional resource base is invalid");

    write(0xbfe001,Width::byte,static_cast<std::uint8_t>(prior_cia|2U));
    write(0x2229a,Width::word,0x100);
    write(0x22a2c,Width::longword,base);
    write(0x22a1c,Width::longword,base);
    write(0x22a18,Width::longword,base);
    write(0x22a1c,Width::longword,base+0xf2U);
    write(0x22a18,Width::longword,base+0x1bcU);
    write(0x22a16,Width::word,0x40);
    for(std::uint32_t channel=0;channel<4;++channel)
        write(0xdff0a8+channel*16U,Width::word,0);
    write(0xdff096,Width::word,0x0f);
    for(const auto address:std::array<std::uint32_t,3>{0x22a0a,0x22a0e,0x22a20})
        write(address,Width::longword,0);
    for(const auto address:std::array<std::uint32_t,10>{
            0x22a12,0x22a02,0x22a04,0x22a06,0x22a08,
            0x229ea,0x229ec,0x229ee,0x229f0,0x229e8})
        write(address,Width::word,0);
    for(const auto address:std::array<std::uint32_t,4>{0x229f2,0x229f6,0x229fa,0x229fe})
        write(address,Width::longword,0);
    write(0x22a14,Width::word,6);

    DeuterosAmigaOptionalResourceInitResult result;
    result.a0=base;result.a2=base+0xf4U;
    result.d1=read(base+0xf0U,4)-1U;
    const auto scan_count=static_cast<std::uint32_t>(result.d1&0xffffU)+1U;
    result.d0=0;
    for(std::uint32_t index=0;index<scan_count;++index){
        result.d2=read(result.a2,2);result.a2+=2U;
        if(static_cast<std::int16_t>(result.d0)<static_cast<std::int16_t>(result.d2))
            result.d0=(result.d0&0xffff0000U)|(result.d2&0xffffU);
        result.d1=(result.d1&0xffff0000U)|((result.d1-1U)&0xffffU);
    }
    result.d0+=0x400U;
    result.a1=base+0x1bcU+result.d0;
    result.d7=14;
    for(std::uint32_t index=0;index<15;++index){
        const auto record=result.a0;
        const auto old_start=read(record,4);
        if(old_start!=0){
            result.d0=old_start;
            result.d1=read(record+8U,4)-old_start;
            write(record,Width::longword,result.a1);
            result.d0=result.a1+result.d1;
            write(record+8U,Width::longword,result.d0);
            if(read(record+12U,2)==2)
                write(record+8U,Width::longword,0x22a24);
            result.d0=static_cast<std::uint32_t>(read(record+4U,2))<<1U;
            result.a1+=result.d0;
        }
        result.a0+=16U;
        result.d7=(result.d7&0xffff0000U)|((result.d7-1U)&0xffffU);
    }
    write(0x224e6,Width::longword,read(0x6c,4));
    write(0x6c,Width::longword,0x224cc);
    return result;
}

} // namespace eon
