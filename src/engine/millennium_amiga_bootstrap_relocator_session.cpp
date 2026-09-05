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
    }
    throw std::runtime_error("Invalid Millennium Amiga bootstrap relocator state");
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
