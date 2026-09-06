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
void draw_deuteros_amiga_owned_bitmap(std::uint32_t record,Read read,Write write,
    bool scratch=false,bool low_byte_selector=false){
    using Width=MemoryTransferElementWidth;
    const auto table=read(0x2126e,4);
    const auto data=read(0x21272,4);
    const auto raw_selector=read(record,2);
    const auto selector=(scratch||low_byte_selector)?(raw_selector&0xffU):raw_selector;
    if(!scratch&&(selector&0xa000U)!=0)
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
    const auto x=read(scratch?0x20c70U:record+2,2),y=read(scratch?0x20c72U:record+4,2);
    const auto buffer=scratch?0x2ad24U:read(0x20128,4);
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

// $20cc6 caches a decoded sprite at $2ad24. $20fb2 then uses the OR of
// its four plane words as coverage; zero-colour pixels preserve the target.
template<class Read,class Write>
void draw_deuteros_amiga_owned_masked_bitmap(std::uint32_t record,Read read,Write write){
    using Width=MemoryTransferElementWidth;
    const auto selector=read(record,2);
    if((selector&0xe000U)!=0x8000U)
        throw std::runtime_error("Deuteros masked selector requires another renderer route");
    if(selector==read(0x20c8a,2)){
        write(0x20c10,Width::longword,read(0x20c18,4));
    }else{
        write(0x20c8a,Width::word,selector);
        draw_deuteros_amiga_owned_bitmap(record,read,write,true);
        write(0x20c18,Width::longword,read(0x20c10,4));
    }
    const auto x=read(record+2,2);
    const auto raw_y=read(record+4,2);
    const auto y=raw_y<0x8000U?static_cast<std::int32_t>(raw_y):static_cast<std::int32_t>(raw_y)-65536;
    const auto height=read(0x20c12,2)&0xffU;
    const auto groups=read(0x20c10,2)>>2U;
    const auto buffer=read(0x20128,4);
    if(y<0&&static_cast<std::int32_t>(height)+y<0)return; // Original BPL/RTS.
    if(groups==0||groups>20||x>20-groups||y>=200||height==0
        ||(buffer&1U)!=0||buffer>0x1000000U-0x7d00U)
        throw std::runtime_error("Deuteros masked bitmap geometry is unsupported");
    const auto skip=y<0?static_cast<std::uint32_t>(-y):0U;
    if(skip>=height)
        throw std::runtime_error("Deuteros masked bitmap reaches a zero-height loop");
    const auto origin=y<0?0U:static_cast<std::uint32_t>(y);
    const auto rows=std::min(height-skip,200-origin);
    for(std::uint32_t row=0;row<rows;++row){
        for(std::uint32_t group=0;group<groups;++group){
            const auto source=0x2ad24+(skip+row)*40+group*2;
            const auto target=buffer+(origin+row)*40+(x+group)*2;
            std::uint32_t coverage=0;
            for(std::uint32_t plane=0;plane<4;++plane)coverage|=read(source+plane*8000,2);
            const auto inverse=coverage^0xffffU;
            for(std::uint32_t plane=0;plane<4;++plane){
                const auto input=read(source+plane*8000,2);
                const auto destination=target+plane*8000;
                write(destination,Width::word,(read(destination,2)|coverage)&(input|inverse));
            }
        }
    }
}

template<class Read,class Write>
void save_deuteros_amiga_owned_scanlines(std::uint32_t record,Read read,Write write){
    using Width=MemoryTransferElementWidth;
    const auto rows=read(0x20c12,2)&0xffU;
    if(rows==0||rows>=200)
        throw std::runtime_error("Deuteros saved scanlines have unsupported row count");
    write(0x23024,Width::word,rows);
    const auto origin=read(record+4,2);
    write(0x23026,Width::word,origin);
    write(record,Width::word,0xffff);
    const auto buffer=read(0x20128,4);
    if(origin>=200||rows>200-origin||(buffer&1U)||buffer>0x1000000U-32000)
        throw std::runtime_error("Deuteros saved scanlines exceed the owned frame");
    for(std::uint32_t plane=0;plane<4;++plane)
        for(std::uint32_t row=0;row<rows;++row)
            for(std::uint32_t offset=0;offset<40;offset+=4)
                write(0x23028+plane*rows*40+row*40+offset,Width::longword,
                    read(buffer+plane*8000+(origin+row)*40+offset,4));
}

template<class Read,class Write>
void restore_deuteros_amiga_owned_scanlines(std::uint32_t record,Read read,Write write){
    using Width=MemoryTransferElementWidth;
    const auto stored_rows=read(0x23024,2);
    // The stored origin is read by the original but is not the destination.
    static_cast<void>(read(0x23026,2));
    const auto origin=read(record+4,2);
    if(stored_rows==0||stored_rows>=200||origin>=200)
        throw std::runtime_error("Deuteros scanline restore has unsupported geometry");
    const auto rows=std::min(stored_rows,200-origin);
    const auto buffer=read(0x20128,4);
    if((buffer&1U)||buffer>0x1000000U-32000)
        throw std::runtime_error("Deuteros scanline restore buffer is outside native memory");
    for(std::uint32_t plane=0;plane<4;++plane)
        for(std::uint32_t row=0;row<rows;++row)
            for(std::uint32_t offset=0;offset<40;offset+=4)
                write(buffer+plane*8000+(origin+row)*40+offset,Width::longword,
                    read(0x23028+plane*stored_rows*40+row*40+offset,4));
}

template<class Read,class Write>
void draw_deuteros_amiga_owned_sprite(std::uint32_t record,Read read,Write write){
    const auto selector=read(record,2);
    if((selector&0x2000U)!=0){
        restore_deuteros_amiga_owned_scanlines(record,read,write);return;
    }
    if((selector&0xc000U)==0xc000U){
        draw_deuteros_amiga_owned_bitmap(record,read,write,false,true);
        save_deuteros_amiga_owned_scanlines(record,read,write);return;
    }
    if((selector&0x8000U)!=0)draw_deuteros_amiga_owned_masked_bitmap(record,read,write);
    else draw_deuteros_amiga_owned_bitmap(record,read,write);
}

} // namespace eon
