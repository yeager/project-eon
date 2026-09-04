#include "engine/native_runtime_memory.hpp"
#include "engine/deuteros_amiga_title_planar_patch.hpp"
#include "engine/millennium_dos_external_transfer_admission.hpp"
#include "engine/release_runtime.hpp"

#include <array>
#include <cassert>
#include <cstdint>

int main() {
    eon::BoundedMemoryTransferSession transfer({
        0x38a28,0x26cc0,0x1c482,4,4,2,2,
        eon::MemoryTransferElementWidth::longword,0x1000000});
    assert(!eon::make_bounded_memory_transfer_batch(transfer.checkpoint(),"partial"));
    assert(transfer.observe_chunk({1,0x38a28,0,0x26cc0,0x1c482,
        {0x11223344,0xaabbccdd}}).accepted);
    const auto batch=eon::make_bounded_memory_transfer_batch(transfer.checkpoint(),"deuteros-copy");
    assert(batch && batch->fully_admitted && batch->effects.size()==2);

    eon::NativeRuntimeMemory memory;
    assert(memory.apply(*batch).accepted);
    assert(memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x1c482})==0x11);
    assert(memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x1c485})==0x44);
    assert(memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x1c486})==0xaa);
    assert(!memory.read_byte({eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x1c481}));
    assert(!memory.apply(*batch).accepted);
    const auto before_rejection=memory.checkpoint();

    eon::NativeRuntimeEffectBatch overlap{"overlap",true,{
        {1,{eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x400},
            eon::MemoryTransferElementWidth::word,eon::NativeRuntimeByteOrder::big_endian,0x1234},
        {2,{eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x401},
            eon::MemoryTransferElementWidth::byte,eon::NativeRuntimeByteOrder::big_endian,0x56},
    }};
    assert(!memory.apply(overlap).accepted);
    assert(memory.checkpoint().initialized_bytes==before_rejection.initialized_bytes);
    assert(memory.checkpoint().checksum==before_rejection.checksum);

    eon::NativeRuntimeEffectBatch gap{"gap",true,{{
        2,{eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x500},
        eon::MemoryTransferElementWidth::byte,eon::NativeRuntimeByteOrder::little_endian,1}}};
    assert(!memory.apply(gap).accepted);
    eon::NativeRuntimeEffectBatch invented{"invented",false,{{
        1,{eon::NativeRuntimeAddressSpace::linear,std::nullopt,0x500},
        eon::MemoryTransferElementWidth::byte,eon::NativeRuntimeByteOrder::little_endian,1}}};
    assert(!memory.apply(invented).accepted);

    eon::NativeRuntimeMemory same_memory;
    assert(same_memory.apply(*batch).accepted);
    assert(same_memory.diagnostics().checksum==memory.diagnostics().checksum);
    assert(same_memory.diagnostics().initialized_byte_count==8
        && same_memory.diagnostics().applied_batch_count==1);

    eon::MillenniumDosExternalTransferAdmission bdf_transfer(
        eon::MillenniumDosExternalTransferKind::bdf_mode_two_jump);
    assert(bdf_transfer.observe_entry({10,0x0c4b,0x11f7}).accepted);
    assert(bdf_transfer.observe_return({11,0x12cb,0xd40d}).accepted);
    eon::MillenniumDosBdfCheckpoint bdf;
    bdf.terminal_transfer=bdf_transfer.checkpoint();
    bdf.mode_two=eon::MillenniumDosBdfModeTwoCheckpoint{
        eon::MillenniumDosBdfModeTwoState::returned,{0x12cb,0,false,0},
        {{0x12af,0xa000,0x6100,0x1234}}, {}, {}};
    const auto bdf_batch=eon::make_millennium_dos_bdf_effect_batch(bdf,"bdf-zero-copy");
    assert(bdf_batch && bdf_batch->effects.size()==1);
    assert(memory.apply(*bdf_batch).accepted);
    assert(memory.read_byte({eon::NativeRuntimeAddressSpace::dos_segmented,0xa000,0x6100})==0x34);
    assert(memory.read_byte({eon::NativeRuntimeAddressSpace::dos_segmented,0xa000,0x6101})==0x12);
    bdf.terminal_transfer->returned.reset();
    assert(!eon::make_millennium_dos_bdf_effect_batch(bdf,"not-returned"));

    // Exact title-display geometry from the admitted Deuteros v4/v5 trace.
    // The RGB values are the first 16 original RGB4 words at `$1ed24`,
    // expanded nibble-for-nibble rather than replaced with a host palette.
    constexpr std::array<eon::RgbColor,16> title_palette{{
        {0x00,0x00,0x00},{0x99,0xaa,0x77},{0x77,0x88,0x55},{0x55,0x66,0x33},
        {0x33,0x33,0x00},{0xaa,0x00,0x00},{0xcc,0x22,0x00},{0x66,0x00,0x00},
        {0x00,0x22,0x88},{0x00,0xcc,0xff},{0x00,0x88,0x00},{0x88,0x66,0x00},
        {0xff,0xff,0x00},{0xff,0x00,0x00},{0x88,0x00,0x00},{0xff,0xff,0xff},
    }};
    eon::NativeRuntimeEffectBatch patch_batch{"deuteros-planar-patch",true,{}};
    constexpr std::uint32_t patch_base=0xb782;
    for(std::uint32_t row=0;row<8;++row){
        const auto glyph=static_cast<std::uint8_t>(row*0x11U);
        for(std::uint32_t plane=0;plane<4;++plane){
            const auto first=static_cast<std::uint8_t>(0xa0U+plane);
            const auto second=static_cast<std::uint8_t>(0x50U+plane);
            const auto value=static_cast<std::uint8_t>(
                (first&static_cast<std::uint8_t>(~glyph))|(second&glyph));
            patch_batch.effects.push_back({patch_batch.effects.size()+1,
                {eon::NativeRuntimeAddressSpace::linear,std::nullopt,
                    patch_base+row*0x28U+plane*0x1f40U},
                eon::MemoryTransferElementWidth::byte,
                eon::NativeRuntimeByteOrder::big_endian,value});
        }
    }
    eon::NativeRuntimeMemory patch_memory;
    assert(patch_memory.apply(patch_batch).accepted);
    const auto patch=eon::decode_deuteros_amiga_title_planar_patch(
        patch_memory.checkpoint(),patch_base,6,title_palette);
    assert(patch && patch->pixel_x==16 && patch->pixel_y==10
        && patch->color_indices[0]==15 && patch->color_indices[1]==0
        && patch->color_indices[6]==12 && patch->color_indices[7]==10
        && patch->rgba[0]==0xff && patch->rgba[1]==0xff
        && patch->rgba[2]==0xff && patch->rgba[3]==0xff
        && patch->rgba[4]==0 && patch->rgba[5]==0
        && patch->rgba[6]==0 && patch->rgba[7]==0xff);
    auto incomplete_patch_memory=patch_memory.checkpoint();
    incomplete_patch_memory.initialized_bytes.pop_back();
    assert(!eon::decode_deuteros_amiga_title_planar_patch(
        incomplete_patch_memory,patch_base,6,title_palette));
    assert(!eon::decode_deuteros_amiga_title_planar_patch(
        patch_memory.checkpoint(),0x60000,6,title_palette));

    // The native presentation backing retains every admitted patch rather
    // than exposing only the most recent glyph. Missing pixels remain
    // invalid/transparent, and a malformed next patch is atomic.
    std::array<std::uint32_t,32> first_addresses{};
    std::array<std::uint8_t,32> first_values{};
    for(std::size_t index=0;index<first_addresses.size();++index){
        first_addresses[index]=static_cast<std::uint32_t>(
            patch_batch.effects[index].location.offset);
        first_values[index]=static_cast<std::uint8_t>(patch_batch.effects[index].value);
    }
    eon::DeuterosAmigaTitlePlanarSurface surface;
    assert(surface.apply(first_addresses,first_values,6).accepted);
    auto surface_snapshot=surface.snapshot(title_palette,patch_memory.checkpoint().checksum);
    const auto first_pixel=static_cast<std::size_t>(10*320+16);
    assert(surface_snapshot && surface_snapshot->width==320
        && surface_snapshot->height==200
        && surface_snapshot->last_command_generation==6
        && surface_snapshot->applied_patch_count==1
        && surface_snapshot->initialized_plane_byte_count==32
        && surface_snapshot->decoded_pixel_count==64
        && surface_snapshot->valid_pixels.size()==320*200
        && surface_snapshot->color_indices.size()==320*200
        && surface_snapshot->rgba.size()==320*200*4
        && surface_snapshot->valid_pixels[first_pixel]==1
        && surface_snapshot->color_indices[first_pixel]==15
        && surface_snapshot->rgba[first_pixel*4+3]==0xff
        && surface_snapshot->valid_pixels[0]==0
        && surface_snapshot->rgba[3]==0);

    auto malformed_addresses=first_addresses;
    malformed_addresses[17]++;
    assert(!surface.apply(malformed_addresses,first_values,7).accepted);
    assert(surface.snapshot(title_palette,1)->applied_patch_count==1
        && surface.snapshot(title_palette,1)->last_command_generation==6);

    std::array<std::uint32_t,32> second_addresses{};
    std::array<std::uint8_t,32> second_values{};
    constexpr std::uint32_t second_base=0xb5f0+4;
    for(std::uint32_t row=0;row<8;++row){
        for(std::uint32_t plane=0;plane<4;++plane){
            const auto index=row*4U+plane;
            second_addresses[index]=second_base+row*0x28U+plane*0x1f40U;
            second_values[index]=plane==0?0xff:0;
        }
    }
    assert(surface.apply(second_addresses,second_values,8).accepted);
    surface_snapshot=surface.snapshot(title_palette,0x1234);
    assert(surface_snapshot && surface_snapshot->applied_patch_count==2
        && surface_snapshot->last_command_generation==8
        && surface_snapshot->initialized_plane_byte_count==64
        && surface_snapshot->decoded_pixel_count==128
        && surface_snapshot->runtime_memory_checksum==0x1234
        && surface_snapshot->valid_pixels[32]==1
        && surface_snapshot->color_indices[32]==1
        && surface_snapshot->rgba[32*4]==0x99
        && surface_snapshot->rgba[32*4+1]==0xaa
        && surface_snapshot->rgba[32*4+2]==0x77
        && surface_snapshot->rgba[32*4+3]==0xff);
    std::array<std::uint8_t,32> overwritten_values{};
    assert(surface.apply(first_addresses,overwritten_values,9).accepted);
    surface_snapshot=surface.snapshot(title_palette,0x5678);
    assert(surface_snapshot && surface_snapshot->applied_patch_count==3
        && surface_snapshot->initialized_plane_byte_count==64
        && surface_snapshot->decoded_pixel_count==128
        && surface_snapshot->color_indices[first_pixel]==0
        && surface_snapshot->rgba[first_pixel*4+3]==0xff);

    const auto diagnostics=memory.diagnostics();
    assert(diagnostics.initialized_byte_count==10 && diagnostics.applied_batch_count==2);
    return 0;
}
