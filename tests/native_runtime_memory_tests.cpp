#include "engine/native_runtime_memory.hpp"
#include "engine/millennium_dos_external_transfer_admission.hpp"
#include "engine/release_runtime.hpp"

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

    const auto diagnostics=memory.diagnostics();
    assert(diagnostics.initialized_byte_count==10 && diagnostics.applied_batch_count==2);
    return 0;
}
