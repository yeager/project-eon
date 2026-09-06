#include "engine/millennium_amiga_bootstrap_relocator_session.hpp"

#include "data/amiga_adf.hpp"
#include "data/millennium_amiga_loader.hpp"
#include "data/sha256.hpp"

#include <stdexcept>

namespace eon {

MillenniumAmigaBootstrapRelocatorSession::MillenniumAmigaBootstrapRelocatorSession(
    const std::span<const std::uint8_t> disk_image) {
    constexpr auto disk_sha =
        "8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c";
    constexpr auto bootstrap_sha =
        "c31e59f83d6825a2da7a6fd5e3297a322993b0483105794fca449d97d3861e06";
    if (disk_image.size() != AmigaAdf::standard_size
        || to_hex(sha256(disk_image)) != disk_sha
        || to_hex(sha256(disk_image.subspan(0x400, 0x400))) != bootstrap_sha) {
        throw std::runtime_error("Unsupported Millennium Amiga bootstrap relocator media");
    }
    const AmigaAdf disk(std::vector<std::uint8_t>(disk_image.begin(), disk_image.end()));
    const auto plan = parse_millennium_amiga_load_plan(disk);
    const auto boundary = parse_millennium_amiga_bootstrap_relocation_boundary(disk, plan);
    if (boundary.copy_source_address != 0x70032
        || boundary.copy_destination_address != 0x66032
        || boundary.copy_byte_count != 0x3cf
        || boundary.copy_source_end_inclusive != 0x70400
        || boundary.relocated_continuation_address != 0x6629e) {
        throw std::runtime_error("Unsupported Millennium Amiga bootstrap relocation contract");
    }
    constexpr std::size_t first_stage_disk_offset = 0x6e000;
    constexpr std::size_t first_stage_byte_count = 0x24200;
    constexpr auto first_stage_sha =
        "df97c7f6cd622b16b9ffb57bc562906e349c18c56ed8abeb564c6f411e64891c";
    const auto first_stage = disk_image.subspan(first_stage_disk_offset,
        first_stage_byte_count);
    first_stage_sha256_ = to_hex(sha256(first_stage));
    if (first_stage_sha256_ != first_stage_sha
        || first_stage[0] != 0x60 || first_stage[1] != 0x00
        || first_stage[2] != 0x00 || first_stage[3] != 0xba) {
        throw std::runtime_error("Unsupported Millennium Amiga opaque first stage");
    }
    first_stage_bytes_.assign(first_stage.begin(), first_stage.end());

    custom_chip_effect_ = {0x70000, 0xdff104, 0x0024};
    copy_effects_.reserve(boundary.copy_byte_count);
    // $70032..$703ff is entirely inside the exact disk +$400 load. The final
    // $70400 byte is intentionally not read from the following disk byte.
    for (std::uint32_t i = 0; i < boundary.copy_byte_count - 1; ++i) {
        copy_effects_.push_back({0x70036, boundary.copy_source_address + i,
            boundary.copy_destination_address + i,
            disk_image[0x432 + i]});
    }
    final_a3_ = 0x66400;
    final_a5_ = 0x70400;
    final_d1_ = 0;
}

MillenniumAmigaBootstrapRelocatorBoundary
MillenniumAmigaBootstrapRelocatorSession::boundary() const {
    switch (state_) {
    case MillenniumAmigaBootstrapRelocatorState::awaiting_overread_byte:
        return {0x70036, 0x70400, 0x66400};
    case MillenniumAmigaBootstrapRelocatorState::awaiting_terminal_jump:
        return {0x7003c, 0, 0x6629e};
    case MillenniumAmigaBootstrapRelocatorState::awaiting_setup_call_return:
        return {0x662b2, 0, 0x66128};
    case MillenniumAmigaBootstrapRelocatorState::awaiting_first_read_return:
        return {0x662cc, 0, 0x661da};
    case MillenniumAmigaBootstrapRelocatorState::awaiting_opaque_first_stage:
        return {0x662e4, 0, 0x41000};
    case MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_illegal_exception:
        return {0x410de, 0x10, 0};
    case MillenniumAmigaBootstrapRelocatorState::awaiting_second_first_stage_illegal_exception:
        return {0x410fc, 0x10, 0};
    case MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_trace_exception:
        return {trace_branch_chain_execution_ ? 0x411d8U : 0x41110U, 0x24, 0x411ac};
    case MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_decrypted_instruction:
        return {0x41110, 0, 0};
    case MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_decrypted_memory_write:
        return {0x411ee,0,0xa183ec32};
    case MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_custom_chip_write:
        return {0x42504,0,0xdff180};
    case MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_exec_service:
        return {0x4251a,4,custom_chip_exec_prefix_execution_->pending_vector_address};
    case MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_setup_call:
        return {0x4252e,0,0x415ea};
    case MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_graphics_allocation:
        return {0x4164a,4,open_graphics_execution_->pending_vector_address};
    case MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_graphics_init:
        return {0x41666,0x41844,allocation_consumer_execution_->pending_vector_address};
    case MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_view_init:
        return {0x41780,0x41844,graphics_initialization_execution_->pending_vector_address};
    case MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_interrupt_control:
        return {0x42546,0,view_service_execution_->pending_custom_effect.address};
    }
    throw std::runtime_error("Invalid Millennium Amiga bootstrap relocator state");
}

MillenniumAmigaViewServiceExecution
MillenniumAmigaBootstrapRelocatorSession::execute_view_services(
    const MillenniumAmigaViewServiceObservation& o) {
    constexpr std::array<std::uint32_t,4> calls{0x41780,0x417b4,0x417c6,0x417d2};
    constexpr std::array<std::int16_t,4> vectors{-198,-216,-210,-222};
    constexpr std::array<std::uint32_t,4> returns{0x41784,0x417b8,0x417ca,0x417d6};
    const auto graphics_base=open_graphics_execution_?open_graphics_execution_->graphics_base_value:0;
    if(state_!=MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_view_init
        ||!graphics_initialization_execution_||o.graphics_base_address!=0x41844
        ||o.graphics_base_value!=graphics_base||graphics_base<=222)
        throw std::runtime_error("Detached Millennium Amiga view service observation");
    for(std::size_t i=0;i<o.returns.size();++i)
        if(o.returns[i].call_address!=calls[i]||o.returns[i].vector!=vectors[i]
            ||o.returns[i].return_address!=returns[i]||o.returns[i].result_a7<8
            ||o.returns[i].result_a7>0xfffffc||(o.returns[i].result_a7&1U)!=0)
            throw std::runtime_error("Detached Millennium Amiga view service return");
    const auto stack=o.returns[0].result_a7;
    if(o.returns[1].result_a7!=stack-8||o.returns[2].result_a7!=stack-4
        ||o.returns[3].result_a7!=stack)
        throw std::runtime_error("Contradictory Millennium Amiga view service stack");
    MillenniumAmigaViewServiceExecution r;r.observed=o;
    r.viewport_address=0x41900;r.bitmap_address=0x418cc;
    r.saved_stack_address=stack-8;r.saved_stack_value=0x41892;
    r.allocation_begin=allocation_consumer_execution_->allocation_begin;
    r.allocation_size=0x7d00;r.setup_flag_address=0x41ad6;r.setup_flag_value=0;
    r.pending_custom_effect={0x42546,0xdff09a,0xc000};
    view_service_execution_=r;
    state_=MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_interrupt_control;
    return r;
}

MillenniumAmigaGraphicsInitializationExecution
MillenniumAmigaBootstrapRelocatorSession::execute_graphics_initialization(
    const MillenniumAmigaGraphicsInitializationObservation& o) {
    constexpr std::array<std::uint32_t,3> calls{0x41666,0x41676,0x416b0};
    constexpr std::array<std::int16_t,3> vectors{-360,-204,-390};
    constexpr std::array<std::uint32_t,3> returns{0x4166a,0x4167a,0x416b4};
    const auto graphics_base=open_graphics_execution_?open_graphics_execution_->graphics_base_value:0;
    if(state_!=MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_graphics_init
        ||!allocation_consumer_execution_||o.graphics_base_address!=0x41844
        ||o.graphics_base_value!=graphics_base||graphics_base<=390)
        throw std::runtime_error("Detached Millennium Amiga graphics initialization observation");
    for(std::size_t i=0;i<o.returns.size();++i)
        if(o.returns[i].call_address!=calls[i]||o.returns[i].vector!=vectors[i]
            ||o.returns[i].return_address!=returns[i]||o.returns[i].result_a7<4
            ||o.returns[i].result_a7>0xfffffc||(o.returns[i].result_a7&1U)!=0)
            throw std::runtime_error("Detached Millennium Amiga graphics service return");
    MillenniumAmigaGraphicsInitializationExecution r;r.observed=o;
    r.allocation_begin=allocation_consumer_execution_->allocation_begin;
    r.allocation_end_exclusive=allocation_consumer_execution_->allocation_end_exclusive;
    const auto add=[&](std::uint32_t a,std::uint8_t w,std::uint32_t v){r.effects.push_back({a,v,w});};
    add(0x41892,4,0x418a4);add(0x418c2,2,0x000c);
    add(0x418f8,4,0x418cc);add(0x418fc,2,0);add(0x418fe,2,0);add(0x418f4,4,0);
    add(0x418bc,2,0x0140);add(0x418be,2,0x00c8);add(0x418c8,4,0x418f4);
    add(0x41958,2,0x000a);add(0x4195a,4,0x4195e);
    for(std::size_t i=0;i<20;++i){const auto p=0x86aU+i*2;add(0x4195eU+static_cast<std::uint32_t>(i*2),2,(std::uint32_t(first_stage_bytes_[p])<<8U)|first_stage_bytes_[p+1]);}
    add(0x418a8,4,0x41956);add(0x418c4,2,0x4000);
    constexpr std::uint32_t stride=0x1f40;
    for(std::size_t i=0;i<4;++i){const auto p=r.allocation_begin+static_cast<std::uint32_t>(i)*stride;r.plane_addresses[i]=p;add(0x418d4+static_cast<std::uint32_t>(i)*4,4,p);add(0x41866-static_cast<std::uint32_t>(i)*4,4,p);}
    add(0x41ae0,4,r.allocation_end_exclusive);
    r.pending_request_address=0x41900;r.pending_call_address=0x41780;
    r.pending_graphics_vector=-198;r.pending_vector_address=graphics_base-198;
    graphics_initialization_execution_=r;
    state_=MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_view_init;
    return r;
}

MillenniumAmigaAllocationConsumerExecution
MillenniumAmigaBootstrapRelocatorSession::execute_allocation_consumer(
    const MillenniumAmigaAllocationObservation& o) {
    constexpr std::uint32_t size=0x7d00;
    const auto graphics_base=open_graphics_execution_?open_graphics_execution_->graphics_base_value:0;
    if(state_!=MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_graphics_allocation
        ||!open_graphics_execution_||o.exec_base_source_address!=4
        ||o.exec_base_value!=open_graphics_execution_->observed.exec_base_value
        ||o.call_address!=0x4164a||o.vector!=-198||o.return_address!=0x4164e
        ||o.result_d0==0||(o.result_d0&1U)!=0||o.result_d0>0xffffff
        ||o.allocated_size!=size||!o.cleared||o.result_d0>0x1000000U-size
        ||o.result_a7<4||o.result_a7>0xfffffc||(o.result_a7&1U)!=0
        ||graphics_base<=360)
        throw std::runtime_error("Detached Millennium Amiga AllocMem observation");
    MillenniumAmigaAllocationConsumerExecution r;
    r.observed=o;r.allocation_begin=o.result_d0;
    r.allocation_end_exclusive=o.result_d0+size;
    r.primary_pointer_address=0x41ad2;r.secondary_pointer_address=0x41ace;
    r.graphics_request_address=0x41892;r.pending_call_address=0x41666;
    r.pending_graphics_vector=-360;r.pending_vector_address=graphics_base-360;
    allocation_consumer_execution_=r;
    state_=MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_graphics_init;
    return r;
}

MillenniumAmigaOpenGraphicsExecution
MillenniumAmigaBootstrapRelocatorSession::execute_open_graphics(
    const MillenniumAmigaOpenGraphicsObservation& o) {
    if(state_!=MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_setup_call
        ||!exec_transition_execution_||o.setup_call_address!=0x4252e
        ||o.setup_call_target!=0x415ea||o.exec_base_source_address!=4
        ||o.exec_base_value!=custom_chip_exec_prefix_execution_->resulting_a6
        ||o.exec_base_value<=552
        ||o.exec_call_address!=0x415f6||o.exec_vector!=-552
        ||o.exec_return_address!=0x415fa||o.result_d0<=552
        ||o.result_d0>0xffffff||(o.result_d0&1U)!=0||o.result_a7<4
        ||o.result_a7>0xfffffc||(o.result_a7&1U)!=0
        ||first_stage_bytes_[0x98a]!=0||first_stage_bytes_[0x98b]!=0)
        throw std::runtime_error("Detached Millennium Amiga OpenLibrary observation");
    MillenniumAmigaOpenGraphicsExecution r;
    r.observed=o;r.library_name_address=0x41848;r.requested_version=0;
    r.graphics_base_address=0x41844;r.graphics_base_value=o.result_d0;
    r.open_count_address=0x4198a;r.open_count_value=1;
    r.allocation_flags=0x10002;r.allocation_size=0x7d00;
    r.pending_call_address=0x4164a;r.pending_exec_vector=-198;
    r.pending_vector_address=o.exec_base_value-198;
    open_graphics_execution_=r;
    state_=MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_graphics_allocation;
    return r;
}

MillenniumAmigaExecTransitionExecution
MillenniumAmigaBootstrapRelocatorSession::execute_exec_transition(
    const MillenniumAmigaExecTransitionObservation& o) {
    if(state_!=MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_exec_service
        ||!custom_chip_exec_prefix_execution_||o.exec_base_source_address!=4
        ||o.exec_base_value!=custom_chip_exec_prefix_execution_->resulting_a6
        ||o.first_call_address!=0x4251a||o.first_vector!=-150||o.first_return_address!=0x4251e
        ||o.second_call_address!=0x42528||o.second_vector!=-156||o.second_return_address!=0x4252c
        ||o.second_result_a7<4||o.second_result_a7>0xfffffc||(o.second_result_a7&1U)!=0)
        throw std::runtime_error("Detached Millennium Amiga Exec transition observation");
    MillenniumAmigaExecTransitionExecution r;
    r.observed=o;r.user_stack_argument=0x7fff0;r.saved_a1_address=o.second_result_a7-4;
    r.saved_a1_value=bus_error_prefix_execution_->exception_frame_address;
    r.resulting_d0=o.second_result_d0;r.resulting_sr=o.second_result_sr;
    r.pending_call_address=0x4252e;r.pending_call_target=0x415ea;
    exec_transition_execution_=r;
    state_=MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_setup_call;
    return r;
}

MillenniumAmigaCustomChipExecPrefixExecution
MillenniumAmigaBootstrapRelocatorSession::execute_custom_chip_exec_prefix(
    const MillenniumAmigaCustomChipExecBaseObservation& o) {
    if(state_!=MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_custom_chip_write
        ||!bus_error_prefix_execution_||o.instruction_address!=0x42504
        ||o.custom_chip_base!=0xdff000||o.register_offset!=0x0180||o.value!=0x0f00
        ||o.exec_base_source_address!=4||o.exec_base_value<=552
        ||o.exec_base_value>0xffffff||(o.exec_base_value&1U)!=0)
        throw std::runtime_error("Detached Millennium Amiga custom-chip/ExecBase observation");
    MillenniumAmigaCustomChipExecPrefixExecution r;
    r.custom_chip_effect={0x42504,0xdff180,0x0f00};
    r.saved_d0_address=0x40ffc;r.saved_d0_value=trace_register_prefix_execution_->resulting_d0;
    r.resulting_stack_pointer=0x65134;r.resulting_a6=o.exec_base_value;
    r.exec_base_source_address=4;r.pending_call_address=0x4251a;
    r.pending_exec_vector=-150;r.pending_vector_address=o.exec_base_value-150;
    custom_chip_exec_prefix_execution_=r;
    state_=MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_exec_service;
    return r;
}

MillenniumAmigaBusErrorPrefixExecution
MillenniumAmigaBootstrapRelocatorSession::execute_bus_error_prefix(
    const MillenniumAmigaBusErrorObservation& o) {
    if(state_!=MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_decrypted_memory_write
        ||!trace_register_prefix_execution_||o.handler_entry_address!=0x415d6
        ||o.saved_status_register!=0xa700||o.saved_program_counter!=0x411f4
        ||o.instruction_register!=0x13c4||o.fault_address!=0x83ec32
        ||o.special_status_word!=0x0005||(o.exception_frame_address&1U)!=0
        ||o.exception_frame_address<14||o.exception_frame_address>0xfffff2)
        throw std::runtime_error("Detached Millennium Amiga bus-error observation");
    MillenniumAmigaBusErrorPrefixExecution r;
    r.exception_frame_address=o.exception_frame_address;
    r.logical_address=0xa183ec32;r.physical_address=0x83ec32;
    r.handler_jump_target=0x424f6;
    r.saved_a1_address=0x4198c;r.saved_a1_value=trace_register_prefix_execution_->resulting_a1;
    r.resulting_a0=0xdff000;r.resulting_a1=o.exception_frame_address;
    r.pending_instruction_address=0x42504;r.pending_destination_address=0xdff180;
    r.pending_value=0x0f00;
    bus_error_prefix_execution_=r;
    state_=MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_custom_chip_write;
    return r;
}

MillenniumAmigaTraceRegisterPrefixExecution
MillenniumAmigaBootstrapRelocatorSession::execute_trace_register_prefix(
    const MillenniumAmigaTraceRegisterPrefixObservation& observation){
    constexpr std::array pcs{0x411d8U,0x411dcU,0x411e2U,0x411e6U,
        0x411e8U,0x411eaU,0x411ecU,0x411eeU};
    constexpr std::array statuses{0xa700U,0xa700U,0xa700U,0xa700U,
        0xa700U,0xa704U,0xa700U,0xa700U};
    constexpr std::array ciphertexts{0xf076fce0U,0x40e60f89U,0xba0d0eaeU,
        0x81560ca9U,0x0ca9c14eU,0xc14eec3bU,0xec3b0000U,0x00009f32U};
    constexpr std::array keys{0x03014e73U,0xf076fce0U,0x0f890008U,
        0xba0d0eaeU,0x0eae8156U,0x81560ca9U,0x0ca9c14eU,0xc14eec3bU};
    constexpr std::array transformed{0x41fa001eU,0x43f90000U,0x45fafed8U,
        0x7007495bU,0x7200301fU,0x32189292U,0xd28af356U,0x13c4a183U};
    if(state_!=MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_trace_exception
        ||!trace_branch_chain_execution_||boundary().instruction_address!=0x411d8)
        throw std::runtime_error("Detached Millennium Amiga trace register prefix");
    MillenniumAmigaTraceRegisterPrefixExecution result;
    std::uint32_t cursor=0x41142,cipher=0x607ed5c0;
    for(std::size_t i=0;i<pcs.size();++i){const auto&o=observation.exceptions[i];
        if(o.handler_entry_address!=0x411ac||o.saved_program_counter!=pcs[i]
            ||o.saved_status_register!=statuses[i]||o.handler_status_register!=0x2700
            ||(o.exception_frame_address&1U)!=0||o.exception_frame_address<0x12
            ||o.exception_frame_address>0xfffffa
            ||(o.exception_frame_address-12<0x65200&&o.exception_frame_address+6>0x41000))
            throw std::runtime_error("Detached Millennium Amiga trace register observation");
        auto&e=result.decryptions[i];e.exception_frame_address=o.exception_frame_address;
        e.resulting_handler_status_register=0x2000;e.saved_status_register=o.saved_status_register;
        e.saved_program_counter=o.saved_program_counter;e.temporary_stack_address=o.exception_frame_address-12;
        e.temporary_stack={{first_stage_illegal_execution_->snapshot[0],first_stage_illegal_execution_->resulting_a0,first_stage_entry_execution_->snapshot[9]}};
        e.restored_address=cursor;e.restored_value=cipher;e.cursor_address=0x410b4;e.cursor_value=pcs[i];
        e.saved_ciphertext_address=0x410b8;e.saved_ciphertext_value=ciphertexts[i];
        e.key_source_address=pcs[i]-4;e.key_source_value=keys[i];const auto inv=~keys[i];e.xor_key=(inv<<16U)|(inv>>16U);
        e.transformed_address=pcs[i];e.transformed_value=transformed[i];e.resulting_stack_pointer=o.exception_frame_address+6;
        cursor=pcs[i];cipher=ciphertexts[i];}
    result.resulting_d0=7;result.resulting_d1=0x41656;result.resulting_a0=0x411fa;
    result.resulting_a1=8;result.resulting_a2=0x410bc;result.resulting_status_register=0xa700;
    result.pending_instruction_address=0x411ee;result.pending_destination_address=0xa183ec32;
    result.pending_value=static_cast<std::uint8_t>(first_stage_illegal_execution_->snapshot[4]);
    trace_register_prefix_execution_=result;
    state_=MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_decrypted_memory_write;
    return result;
}

MillenniumAmigaTraceBranchChainExecution
MillenniumAmigaBootstrapRelocatorSession::execute_trace_branch_chain(
    const MillenniumAmigaTraceBranchChainObservation& observation) {
    constexpr std::array pcs{0x41112U,0x4112eU,0x41166U,0x4115eU,0x41132U,
        0x41162U,0x41152U,0x41106U,0x4116aU,0x41142U};
    constexpr std::array ciphertexts{0x60762ad6U,0xd5670071U,0x4a17601fU,
        0xd5f9ffe9U,0x9f8e2ab6U,0x6016d5e8U,0x607ed5ccU,0xb5399f83U,
        0xffe04a3eU,0x607ed5c0U};
    constexpr std::array keys{0xd533ff89U,0xffb84a98U,0x6016d5e8U,
        0xffc44a06U,0xd5670071U,0xd5f9ffe9U,0xd581ff81U,0x601e2ac6U,
        0x4a17601fU,0x2aabff81U};
    constexpr std::array transformed{0x6000001aU,0x60000036U,0x6000fff6U,
        0x6000ffd2U,0x6000002eU,0x6000ffeeU,0x6000ffb2U,0x60000062U,
        0x6000ffd6U,0x60000094U};
    constexpr std::array targets{0x4112eU,0x41166U,0x4115eU,0x41132U,
        0x41162U,0x41152U,0x41106U,0x4116aU,0x41142U,0x411d8U};
    if(state_!=MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_decrypted_instruction
        || !first_stage_entry_execution_ || !first_stage_illegal_execution_
        || !first_trace_execution_ || first_trace_execution_->transformed_value!=0xd545d545)
        throw std::runtime_error("Detached Millennium Amiga traced ADDX chain");
    MillenniumAmigaTraceBranchChainExecution result;
    result.addx_instruction_address=0x41110;
    const auto source=static_cast<std::uint16_t>(first_stage_illegal_execution_->snapshot[5]);
    const auto old_d2=first_stage_illegal_execution_->snapshot[2];
    const auto destination=static_cast<std::uint16_t>(old_d2);
    const auto prior_sr=first_trace_execution_->saved_status_register;
    const auto sum=static_cast<std::uint32_t>(source)+destination+((prior_sr&0x10U)?1U:0U);
    const auto value=static_cast<std::uint16_t>(sum);
    std::uint16_t status=prior_sr&0xffe0U;
    if(sum>0xffffU)status|=0x11U;
    if(value&0x8000U)status|=0x08U;
    if(value==0 && (prior_sr&0x04U))status|=0x04U;
    if((~(source^destination)&(destination^value)&0x8000U)!=0)status|=0x02U;
    result.resulting_d2=(old_d2&0xffff0000U)|value;
    result.resulting_status_register=status;
    std::uint32_t cursor=0x41110, old_cipher=0xff896076;
    for(std::size_t i=0;i<pcs.size();++i){
        const auto& o=observation.exceptions[i];
        if(o.handler_entry_address!=0x411ac || o.saved_program_counter!=pcs[i]
            || o.saved_status_register!=status || (o.exception_frame_address&1U)!=0
            || o.exception_frame_address<0x12 || o.exception_frame_address>0xfffffa
            || (o.exception_frame_address-12<0x65200&&o.exception_frame_address+6>0x41000)
            || o.handler_status_register!=observation.exceptions[0].handler_status_register)
            throw std::runtime_error("Detached Millennium Amiga trace branch observation");
        auto& e=result.decryptions[i];
        e.exception_frame_address=o.exception_frame_address;
        e.resulting_handler_status_register=o.handler_status_register&0xf8ffU;
        e.saved_status_register=o.saved_status_register;e.saved_program_counter=o.saved_program_counter;
        e.temporary_stack_address=o.exception_frame_address-12;
        e.temporary_stack={{first_stage_illegal_execution_->snapshot[0],
            first_stage_illegal_execution_->resulting_a0,first_stage_entry_execution_->snapshot[9]}};
        e.restored_address=cursor;e.restored_value=old_cipher;
        e.cursor_address=0x410b4;e.cursor_value=pcs[i];e.saved_ciphertext_address=0x410b8;
        e.saved_ciphertext_value=ciphertexts[i];e.key_source_address=pcs[i]-4;
        e.key_source_value=keys[i];
        const auto inverted=~keys[i];e.xor_key=(inverted<<16U)|(inverted>>16U);
        e.transformed_address=pcs[i];e.transformed_value=transformed[i];
        e.resulting_stack_pointer=o.exception_frame_address+6;
        cursor=pcs[i];old_cipher=ciphertexts[i];
    }
    result.terminal_trace_program_counter=targets.back();
    trace_branch_chain_execution_=result;
    state_=MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_trace_exception;
    return result;
}

MillenniumAmigaFirstTraceExecution
MillenniumAmigaBootstrapRelocatorSession::execute_first_trace_handler(
    const MillenniumAmigaFirstTraceObservation& observation) {
    if (state_ != MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_trace_exception
        || !first_stage_entry_execution_ || !first_stage_illegal_execution_
        || !second_illegal_execution_
        || observation.handler_entry_address != 0x411ac
        || observation.saved_program_counter != 0x41110
        || (observation.exception_frame_address & 1U) != 0
        || observation.exception_frame_address < 0x12
        || observation.exception_frame_address > 0xfffffa
        || (observation.exception_frame_address - 12 < 0x65200
            && observation.exception_frame_address + 6 > 0x41000)
        || first_stage_bytes_.size() != 0x24200
        || first_stage_bytes_[0x10c] != 0x4a || first_stage_bytes_[0x10d] != 0xcc
        || first_stage_bytes_[0x10e] != 0xd5 || first_stage_bytes_[0x10f] != 0x33
        || first_stage_bytes_[0x110] != 0xff || first_stage_bytes_[0x111] != 0x89
        || first_stage_bytes_[0x112] != 0x60 || first_stage_bytes_[0x113] != 0x76
        || first_stage_bytes_[0x1ac] != 0x02 || first_stage_bytes_[0x1ad] != 0x7c
        || first_stage_bytes_[0x1ae] != 0xf8 || first_stage_bytes_[0x1af] != 0xff
        || first_stage_bytes_[0x1d0] != 0xb1 || first_stage_bytes_[0x1d1] != 0x90
        || first_stage_bytes_[0x1d6] != 0x4e || first_stage_bytes_[0x1d7] != 0x73) {
        throw std::runtime_error("Detached Millennium Amiga first trace handler");
    }

    MillenniumAmigaFirstTraceExecution result;
    result.exception_frame_address = observation.exception_frame_address;
    result.resulting_handler_status_register =
        static_cast<std::uint16_t>(observation.handler_status_register & 0xf8ffU);
    result.saved_status_register = observation.saved_status_register;
    result.saved_program_counter = observation.saved_program_counter;
    result.temporary_stack_address = observation.exception_frame_address - 12;
    result.temporary_stack = {{first_stage_illegal_execution_->snapshot[0],
        first_stage_illegal_execution_->resulting_a0,
        first_stage_entry_execution_->snapshot[9]}};
    result.restored_address = second_illegal_execution_->cursor_value;
    result.restored_value = second_illegal_execution_->saved_ciphertext_value;
    result.cursor_address = second_illegal_execution_->cursor_address;
    result.cursor_value = observation.saved_program_counter;
    result.saved_ciphertext_address = second_illegal_execution_->saved_ciphertext_address;
    result.saved_ciphertext_value = 0xff896076;
    result.key_source_address = observation.saved_program_counter - 4;
    result.key_source_value = 0x4accd533;
    const auto inverted = ~result.key_source_value;
    result.xor_key = static_cast<std::uint32_t>((inverted << 16U) | (inverted >> 16U));
    result.transformed_address = observation.saved_program_counter;
    result.transformed_value = result.saved_ciphertext_value ^ result.xor_key;
    result.resulting_stack_pointer = observation.exception_frame_address + 6;
    first_trace_execution_ = result;
    state_ = MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_decrypted_instruction;
    return result;
}

MillenniumAmigaSecondIllegalExecution
MillenniumAmigaBootstrapRelocatorSession::execute_second_illegal_handler(
    const MillenniumAmigaSecondIllegalObservation& observation) {
    if (state_ != MillenniumAmigaBootstrapRelocatorState::awaiting_second_first_stage_illegal_exception
        || !first_stage_entry_execution_ || !first_stage_illegal_execution_
        || observation.handler_entry_address != 0x41172
        || observation.saved_program_counter != 0x410fc
        || (observation.saved_status_register & 0x8000U) != 0
        || (observation.exception_frame_address & 1U) != 0
        || observation.exception_frame_address < 0x34
        || observation.exception_frame_address > 0xfffffa
        || (observation.exception_frame_address - 12 < 0x65200
            && observation.exception_frame_address + 6 > 0x41000)
        || first_stage_bytes_.size() != 0x24200
        || first_stage_bytes_[0x172] != 0x48 || first_stage_bytes_[0x173] != 0xe7
        || first_stage_bytes_[0x19e] != 0x43 || first_stage_bytes_[0x19f] != 0xfa
        || first_stage_bytes_[0x1d6] != 0x4e || first_stage_bytes_[0x1d7] != 0x73
        || first_stage_bytes_[0xfa] != 0x00 || first_stage_bytes_[0xfb] != 0x10
        || first_stage_bytes_[0xfe] != 0xd5 || first_stage_bytes_[0xff] != 0x03) {
        throw std::runtime_error("Detached Millennium Amiga second ILLEGAL handler");
    }
    MillenniumAmigaSecondIllegalExecution result;
    result.exception_frame_address = observation.exception_frame_address;
    result.resulting_saved_status_register =
        static_cast<std::uint16_t>((observation.saved_status_register | 0x0700U) ^ 0x8000U);
    result.resulting_saved_program_counter = 0x410fe;
    result.temporary_stack_address = observation.exception_frame_address - 12;
    result.temporary_stack = {{first_stage_illegal_execution_->snapshot[0],
        first_stage_illegal_execution_->resulting_a0,
        first_stage_entry_execution_->snapshot[9]}};
    result.vector_8_value = 0x415d6;
    result.vector_9_value = 0x411ac;
    result.cursor_address = 0x410b4;
    result.cursor_value = 0x410fe;
    result.saved_ciphertext_address = 0x410b8;
    result.saved_ciphertext_value = 0xd503ffe1;
    result.transformed_address = 0x410fe;
    const std::uint32_t key = 0xb503ffef; // SWAP(NOT.L($00104afc)).
    result.transformed_value = result.saved_ciphertext_value ^ key;
    result.resulting_stack_pointer = observation.exception_frame_address + 6;
    result.branch_target = 0x41110;
    result.trace_resume_address = 0x41110;
    second_illegal_execution_ = result;
    state_ = MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_trace_exception;
    return result;
}

MillenniumAmigaFirstStageIllegalExecution
MillenniumAmigaBootstrapRelocatorSession::execute_first_stage_illegal_handler(
    const MillenniumAmigaFirstStageIllegalObservation& observation) {
    if (state_ != MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_illegal_exception
        || !first_stage_entry_execution_
        || observation.handler_entry_address != 0x410e0
        || observation.saved_program_counter != 0x410de
        || (observation.exception_frame_address & 1U) != 0
        || observation.exception_frame_address > 0xfffffa
        || observation.exception_frame_address < 0x28
        || (observation.exception_frame_address < 0x410aa
            && observation.exception_frame_address + 6 > 0x4104a)
        || observation.vector_longs[2] != first_stage_entry_execution_->resulting_d0
        || first_stage_bytes_.size() != 0x24200
        || first_stage_bytes_[0xe0] != 0x23 || first_stage_bytes_[0xe1] != 0xc0
        || first_stage_bytes_[0xe6] != 0x4c || first_stage_bytes_[0xe7] != 0xf9
        || first_stage_bytes_[0xee] != 0x48 || first_stage_bytes_[0xef] != 0xd6
        || first_stage_bytes_[0xf2] != 0x41 || first_stage_bytes_[0xf3] != 0xfa
        || first_stage_bytes_[0xfc] != 0x4a || first_stage_bytes_[0xfd] != 0xfc) {
        throw std::runtime_error("Detached Millennium Amiga first-stage ILLEGAL handler");
    }
    MillenniumAmigaFirstStageIllegalExecution result;
    result.exception_frame_address = observation.exception_frame_address;
    result.saved_status_register = observation.saved_status_register;
    result.saved_program_counter = observation.saved_program_counter;
    result.restored_vector_value = first_stage_entry_execution_->resulting_d0;
    result.snapshot_address = 0x4108a;
    result.snapshot = observation.vector_longs;
    result.installed_vector_value = 0x41172;
    result.resulting_a0 = 0x41172;
    result.resulting_stack_pointer = observation.exception_frame_address;
    result.illegal_instruction_address = 0x410fc;
    first_stage_illegal_execution_ = result;
    state_ = MillenniumAmigaBootstrapRelocatorState::awaiting_second_first_stage_illegal_exception;
    return result;
}

MillenniumAmigaFirstStageEntryExecution
MillenniumAmigaBootstrapRelocatorSession::execute_first_stage_entry(
    const MillenniumAmigaFirstStageRegisterObservation& observation) {
    if (state_ != MillenniumAmigaBootstrapRelocatorState::awaiting_opaque_first_stage
        || observation.instruction_address != 0x41000
        || observation.stack_pointer < 4
        || (observation.stack_pointer - 4 < 0x14)
        || (observation.stack_pointer - 4 < 0x4108a
            && observation.stack_pointer > 0x4104a)
        || first_stage_bytes_.size() != 0x24200
        || first_stage_bytes_[0] != 0x60 || first_stage_bytes_[1] != 0x00
        || first_stage_bytes_[2] != 0x00 || first_stage_bytes_[3] != 0xba
        || first_stage_bytes_[0xbc] != 0x2f || first_stage_bytes_[0xbd] != 0x0e
        || first_stage_bytes_[0xde] != 0x4a || first_stage_bytes_[0xdf] != 0xfc) {
        throw std::runtime_error("Detached Millennium Amiga first-stage entry");
    }
    MillenniumAmigaFirstStageEntryExecution result;
    result.branch_target = 0x410bc;
    result.snapshot_address = 0x4104a;
    for (std::size_t i = 0; i < observation.data.size(); ++i) result.snapshot[i] = observation.data[i];
    for (std::size_t i = 0; i < observation.address.size(); ++i) result.snapshot[8+i] = observation.address[i];
    // MOVEM first stores the temporary A6=$4104a, then MOVE.L (A7)+,-8(A6)
    // restores the original A6 into that exact saved-register slot.
    result.snapshot[14] = observation.address[6];
    result.snapshot[15] = observation.stack_pointer - 4;
    result.transient_stack_address = observation.stack_pointer - 4;
    result.original_a6 = observation.address[6];
    result.installed_vector_address = 0x10;
    result.installed_vector_value = 0x410e0;
    result.resulting_d0 = observation.exception_vector_10;
    result.resulting_a6 = 0x4108a;
    result.resulting_stack_pointer = observation.stack_pointer;
    result.illegal_instruction_address = 0x410de;
    first_stage_entry_execution_ = result;
    state_ = MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_illegal_exception;
    return result;
}

void MillenniumAmigaBootstrapRelocatorSession::observe_overread_byte(
    const std::uint32_t instruction_address, const std::uint32_t source_address,
    const std::uint8_t value) {
    if (state_ != MillenniumAmigaBootstrapRelocatorState::awaiting_overread_byte
        || instruction_address != 0x70036 || source_address != 0x70400) {
        throw std::runtime_error("Detached Millennium Amiga bootstrap over-read");
    }
    copy_effects_.push_back({0x70036, 0x70400, 0x66400, value});
    final_a3_ = 0x66401;
    final_a5_ = 0x70401;
    final_d1_ = 0xffff;
    state_ = MillenniumAmigaBootstrapRelocatorState::awaiting_terminal_jump;
}

void MillenniumAmigaBootstrapRelocatorSession::observe_terminal_jump(
    const std::uint32_t instruction_address, const std::uint32_t target_address) {
    if (state_ != MillenniumAmigaBootstrapRelocatorState::awaiting_terminal_jump
        || instruction_address != 0x7003c || target_address != 0x6629e) {
        throw std::runtime_error("Detached Millennium Amiga bootstrap terminal jump");
    }
    state_ = MillenniumAmigaBootstrapRelocatorState::awaiting_setup_call_return;
}

void MillenniumAmigaBootstrapRelocatorSession::observe_setup_call_return(
    const std::uint32_t instruction_address, const std::uint32_t target_address) {
    if (state_ != MillenniumAmigaBootstrapRelocatorState::awaiting_setup_call_return
        || instruction_address != 0x662b2 || target_address != 0x66128) {
        throw std::runtime_error("Detached Millennium Amiga setup-call return");
    }
    state_ = MillenniumAmigaBootstrapRelocatorState::awaiting_first_read_return;
}

void MillenniumAmigaBootstrapRelocatorSession::observe_first_read_return(
    const std::uint32_t instruction_address, const std::uint32_t target_address,
    const std::uint8_t io_error) {
    if (state_ != MillenniumAmigaBootstrapRelocatorState::awaiting_first_read_return
        || instruction_address != 0x662cc || target_address != 0x661da
        || io_error != 0) {
        throw std::runtime_error("Detached Millennium Amiga first-read return");
    }
    state_ = MillenniumAmigaBootstrapRelocatorState::awaiting_opaque_first_stage;
}

} // namespace eon
