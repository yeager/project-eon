#pragma once

#include "engine/bounded_memory_transfer.hpp"

#include <cstddef>
#include <cstdint>
#include <array>
#include <stdexcept>

namespace eon {

// Registers needed to resume the local $214aa interpreter. A nonzero stop
// is before an unexecuted instruction, not evidence of a completed service.
struct DeuterosAmigaOwnedCommandStop {
    std::uint32_t instruction = 0;
    std::uint32_t record = 0, cursor = 0, d0 = 0;
    std::uint16_t d1_word = 0;
    std::uint32_t scheduler_index = 0;
    std::uint32_t scheduler_iterations = 0;
    std::size_t scheduler_commands_remaining = 0;
};

// $22ab8 stages four 14-byte software descriptors. It does not write AUDx
// hardware or start host audio; the later $22bea consumer owns that work.
template<class Read, class Write>
void stage_deuteros_amiga_owned_sound(std::uint32_t& d0, std::uint16_t& d1_word,
    Read read, Write write) {
    using Width = MemoryTransferElementWidth;
    std::uint32_t descriptor=0x22aaa;
    if ((d0&0xffU)!=0) {
        d0=(d0&0xffffU)*14U;
        const auto displacement=d0&0xffffU;
        descriptor=read(0x22aa6,4)+(displacement<0x8000U?displacement:displacement-0x10000U);
    }
    for (std::uint32_t channel=0;channel<4;++channel) {
        const bool selected=(d1_word&1U)!=0;
        d1_word=static_cast<std::uint16_t>((d1_word&0xff00U)|((d1_word&0xffU)>>1U));
        if (!selected) continue;
        const auto destination=0x22a6e + channel*14U;
        write(0x22a6c,Width::word,read(0x22a6c,2)|(1U<<channel));
        d0=read(descriptor,4)+0x32a24;
        write(destination,Width::longword,d0);
        write(destination+4,Width::longword,read(descriptor+4,4));
        write(destination+8,Width::longword,read(descriptor+8,4));
        write(destination+12,Width::word,read(descriptor+12,2));
    }
}

// Ordered DMA write intents are retained separately: a final raw register
// value is not a model of Amiga set/clear semantics or proof of host playback.
struct DeuterosAmigaOwnedAudioResult {
    std::array<std::uint16_t,2> dma_writes{};
};

// Native $22bea consumer. Read/write must use an unpublished transaction;
// the ROM-dependent random modes reject if their original byte is not owned.
template<class Read,class Write>
DeuterosAmigaOwnedAudioResult consume_deuteros_amiga_owned_audio(Read read,Write write){
    using Width=MemoryTransferElementWidth;
    const auto mask=static_cast<std::uint16_t>(read(0x22a6c,2));
    DeuterosAmigaOwnedAudioResult result{{mask,static_cast<std::uint16_t>(mask^0x800fU)}};
    for(const auto value:result.dma_writes)write(0xdff096,Width::word,value);
    const auto random=[&](){
        auto value=read(0x22a32,2)&0x7fffU;
        value=(value&0xff00U)|read(0xff0000+value,1);
        value+=read(0x2079e,4);
        write(0x22a32,Width::word,(read(0x22a32,2)+value)&0xffffU);
        return value&0xffU;
    };
    for(std::uint32_t channel=0;channel<4;++channel){
        const auto descriptor=0x22a6e + 14*channel;
        const auto hardware=0xdff0a0+16*channel;
        auto control=read(descriptor+10,2);
        auto count=control&0xffU;
        if((control&0x400U)&&count){
            --count;
            write(descriptor+10,Width::word,count?((control&0xff00U)|count):0);
            write(hardware,Width::longword,0x22a6a);
            write(hardware+4,Width::word,1);write(hardware+8,Width::word,0);
            continue;
        }
        write(hardware,Width::longword,read(descriptor,4));
        write(hardware+4,Width::longword,read(descriptor+4,4));
        write(hardware+8,Width::word,read(descriptor+8,2));
        if(control==0)continue;
        if(control&0x100U){
            const auto delta=count<128?count:count+0xff00U;
            const auto sum=read(descriptor+6,2)+delta;
            auto period=sum&0xffffU;
            const auto limit=read(descriptor+12,2);
            if(sum>0xffffU){if(period<limit)period=limit;}
            else if(period>=limit)period=limit;
            write(descriptor+6,Width::word,period);
            continue;
        }
        if(!(control&0x200U)){
            if(control&0x800U){
                const auto shift=channel<2?5U:channel==2?4U:3U;
                write(descriptor+6,Width::word,(((random()&count)<<shift)+read(descriptor+12,2))&0xffffU);
                continue;
            }
            if(!(control&0x1000U)&&(control&0x2000U)){
                if(!(read(0x22a6c,2)&(1U<<channel))){
                    const auto length=read(descriptor+4,2);
                    const auto tail=read(descriptor+12,2);
                    const auto displacement=(((length<<1)&0xffffU)-tail)&0xffffU;
                    write(descriptor+4,Width::word,tail>>1);
                    write(descriptor,Width::longword,read(descriptor,4)+displacement);
                    write(descriptor+10,Width::word,0);
                }
                continue;
            }
            count=((random()&count)+read(descriptor+12,2))&0xffffU;
            write(hardware+8,Width::word,count);write(descriptor+8,Width::word,count);
            write(descriptor+10,Width::word,0x202);
        }
        write(descriptor+10,Width::word,(read(descriptor+10,2)-1U)&0xffffU);
        if((count&0xffU)==0){
            write(descriptor,Width::longword,read(0x22aaa,4));
            write(descriptor+4,Width::word,read(0x22aae,2));
            write(descriptor+10,Width::word,0);
        }
    }
    write(0x22a6c,Width::word,0);
    return result;
}

// $2016a reads the owned original seed/counter and original resource word.
// The caller supplies the full D0 because ADD.L preserves more than its low
// word. No host clock or host pseudo-random generator participates.
template<class Read, class Write>
std::uint32_t next_deuteros_amiga_owned_random(std::uint32_t d0, Read read, Write write) {
    d0=(d0&0xffff0000U)|read(0x20168,2);
    d0+=read(0x2079e,4);
    d0=(d0&0xffff0000U)|(d0&0x3ffeU);
    d0=(d0&0xffff0000U)|read(0x32a24+(d0&0xffffU),2);
    d0=(d0&0xffff0000U)|((d0+14U)&0xffffU);
    write(0x20168,MemoryTransferElementWidth::word,(read(0x20168,2)+d0)&0xffffU);
    return d0;
}

// Read/write operate on the caller's private owned-memory transaction.
// This is the recovered game's command language, not a CPU emulator.
template<class Read, class Write>
DeuterosAmigaOwnedCommandStop execute_deuteros_amiga_owned_commands(
    std::uint32_t record, std::uint32_t cursor, std::uint32_t initial_d0, Read read, Write write,
    std::size_t& remaining_commands) {
    using Width = MemoryTransferElementWidth;
    DeuterosAmigaOwnedCommandStop state{0,record,cursor,initial_d0,0,0};
    const auto word = [&]() {
        const auto value=read(state.cursor,2); state.cursor+=2; return value;
    };
    const auto longword = [&]() {
        const auto value=read(state.cursor,4); state.cursor+=4; return value;
    };
    const auto save_cursor = [&]() { write(record+16,Width::longword,state.cursor); };
    for (;;) {
        if (remaining_commands==0)
            throw std::runtime_error("Deuteros owned command budget exhausted");
        --remaining_commands;
        // MOVE.W preserves the upper word; CMP.B dispatches on the low byte.
        state.d0=(state.d0&0xffff0000U)|word();
        if ((state.d0&0xffffU)==0) {
            write(record+6,Width::word,0); return state;
        }
        switch (state.d0&0xffU) {
        case 1:
            state.d0=(state.d0&0xffff0000U)|word();
            write(record,Width::word,state.d0&0xffffU); break;
        case 2:
            state.d0=longword(); write(record+2,Width::longword,state.d0); break;
        case 3: case 0x14:
            write(record+6,Width::word,state.d0&0xffffU);
            state.d0=(state.d0&0xffff0000U)|word();
            write(record+8,Width::word,state.d0&0xffffU); save_cursor(); return state;
        case 4:
            state.d0=(state.d0&0xffff0000U)|word();
            state.instruction=0x214ee; return state; // Before MOVEM stack save.
        case 5: case 6: {
            const auto mode=state.d0&0xffU;
            write(record+6,Width::word,state.d0&0xffffU);
            state.d0=longword(); write(record+8,Width::longword,state.d0);
            if (mode==6) { state.d0=longword(); write(record+12,Width::longword,state.d0); }
            save_cursor(); return state;
        }
        case 7: case 8: {
            const auto destination=record+((state.d0&0xffU)==7?2U:4U);
            state.d0=(state.d0&0xffff0000U)|word();
            write(destination,Width::word,(read(destination,2)+state.d0)&0xffffU); break;
        }
        case 9:
            state.d0=read(state.cursor,4);
            state.cursor-=state.d0; break; // SUBA.L uses the operand address.
        case 0x0a:
            state.d0=next_deuteros_amiga_owned_random(state.d0,read,write);
            state.d1_word=static_cast<std::uint16_t>(word());
            state.d0=(state.d0&0xffff0000U)|((state.d0&state.d1_word)&0xffffU);
            write(record+6,Width::word,3);
            write(record+8,Width::word,state.d0&0xffffU); save_cursor(); return state;
        case 0x0b:
            state.d0=(state.d0&0xffff0000U)|word();
            state.d1_word=static_cast<std::uint16_t>(word());
            stage_deuteros_amiga_owned_sound(state.d0,state.d1_word,read,write); break;
        case 0x0c:
            state.d0=longword(); write(record+20,Width::longword,state.cursor);
            state.d0+=0x32a24; state.cursor=state.d0; break;
        case 0x0d:
            state.cursor=read(record+20,4); break;
        case 0x0e:
            state.d0=(state.d0&0xffff0000U)|word();
            write(0x207ea,Width::byte,state.d0&0xffU); break;
        case 0x0f:
            state.d0=longword()+0x32a24;
            write(record+12,Width::longword,state.d0);
            write(record,Width::word,0xfe); break;
        case 0x10:
            write(0x210f4,Width::word,0xffff);
            state.d0=0; write(record+6,Width::word,0); return state;
        case 0x11: {
            state.d0=next_deuteros_amiga_owned_random(state.d0,read,write);
            state.d0=(state.d0&0xffff0000U)|(state.d0&0xfU);
            state.d1_word=static_cast<std::uint16_t>(word());
            if ((state.d0&0xffU)>=(state.d1_word&0xffU))
                state.d0=(state.d0&0xffffff00U)|((state.d0-state.d1_word)&0xffU);
            state.d1_word=static_cast<std::uint16_t>(word());
            state.d0=(state.d0&0xffffU)*state.d1_word;
            const auto displacement=state.d0&0xffffU;
            state.cursor+=displacement<0x8000U?displacement:displacement-0x10000U;
            break;
        }
        case 0x12: case 0x13:
            write(0x2171e,Width::byte,(state.d0&0xffU)==0x13?1U:0U); break;
        default:
            state.d0=0; write(record+6,Width::word,0); return state;
        }
    }
}

} // namespace eon
