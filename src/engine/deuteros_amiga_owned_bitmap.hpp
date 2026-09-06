#pragma once

#include "engine/bounded_memory_transfer.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace eon {

// Native opaque $20c8c->$20d8e decoder. The caller supplies a private
// read-through write overlay so source/destination overlap stays sequential.
// Masked sprites and the global saved-scanline path are separate routines.
template<class Read,class Write>
void draw_deuteros_amiga_owned_bitmap(std::uint32_t record,Read read,Write write){
    using Width=MemoryTransferElementWidth;
    const auto table=read(0x2126e,4);
    const auto data=read(0x21272,4);
    const auto selector=read(record,2);
    if((selector&0xa000U)!=0)
        throw std::runtime_error("Deuteros sprite requires masked or saved-scanline continuation");
    const auto displacement=(selector<<2U)&0xffffU;
    const auto entry=table+(displacement<0x8000U?displacement:displacement-0x10000U);
    const auto relative=read(entry,4);
    // The next source offset is a preservation bound, not an emulated read.
    const auto next_relative=read(entry+4,4);
    if(next_relative<=relative||data>0xffffffU||next_relative>0x1000000U-data)
        throw std::runtime_error("Deuteros sprite record bounds are invalid");
    auto source=data+relative;
    const auto end=data+next_relative;
    if((source&1U)!=0||end-source<4)
        throw std::runtime_error("Deuteros sprite header is unaligned or truncated");
    const auto x=read(record+2,2),y=read(record+4,2);
    const auto buffer=read(0x20128,4);
    if((buffer&1U)!=0||buffer>0x1000000U-0x7d00U)
        throw std::runtime_error("Deuteros sprite buffer is outside native memory");
    const auto words=read(source,2);source+=2;
    write(0x20c10,Width::word,words);
    const auto encoded_height=read(source,2);source+=2;
    if(words==0||words>80||(words&3U)!=0||x>20-words/4||y>=200)
        throw std::runtime_error("Deuteros sprite geometry requires an unsupported decoder route");
    const auto lower_extent=(encoded_height&0xffU)+y;
    const auto adjusted_height=(encoded_height-(lower_extent>=200?lower_extent-200:0))&0xffffU;
    write(0x20c12,Width::word,adjusted_height);
    const bool sequential=adjusted_height>=200;
    const auto height=sequential?(adjusted_height&0xffU):adjusted_height;
    if(height==0||height>=200||(sequential&&(encoded_height&0xff00U)!=0x8000U))
        throw std::runtime_error("Deuteros sprite has unsupported height encoding");
    const auto groups=words/4;
    if(sequential){
        write(0x20c14,Width::word,groups);
        write(0x20c16,Width::word,height);
    }
    const auto total=words*height;
    std::uint32_t emitted=0;
    const auto byte=[&](){
        if(source>=end)throw std::runtime_error("Truncated owned Deuteros bitmap stream");
        return read(source++,1);
    };
    const auto destination=[&](){
        const auto plane=sequential?emitted/(groups*height):emitted%4;
        const auto row=sequential?(emitted%(groups*height))/groups:emitted/words;
        const auto group=sequential?emitted%groups:(emitted%words)/4;
        return buffer+plane*8000+(y+row)*40+(x+group)*2;
    };
    while(emitted<total){
        const auto control=byte();
        const auto kind=control&0xc0U;
        auto count=control&0x3fU;
        if(kind==0x80U)count=(count<<8U)|byte();
        if(count==0)throw std::runtime_error("Zero-count owned Deuteros bitmap run");
        const auto fill=kind==0x40U?byte():0U;
        if(kind>=0x80U&&end-source<2)
            throw std::runtime_error("Truncated owned Deuteros bitmap pair");
        const auto run=std::min(count,total-emitted);
        for(std::uint32_t i=0;i<run;++i){
            const auto target=destination();
            if(kind==0){
                write(target,Width::byte,byte());
                write(target+1,Width::byte,byte());
            }else if(kind==0x40U){
                write(target,Width::word,fill*0x101U);
            }else{
                // Pair runs intentionally reverse their two source bytes.
                write(target,Width::byte,read(source+1,1));
                write(target+1,Width::byte,read(source,1));
            }
            ++emitted;
        }
        if(kind>=0x80U&&emitted<total)source+=2;
    }
}

} // namespace eon
