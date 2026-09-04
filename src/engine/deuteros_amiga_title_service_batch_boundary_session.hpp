#pragma once

#include "data/amiga_adf.hpp"
#include "data/deuteros_amiga_loader.hpp"
#include "data/sha256.hpp"
#include "engine/bounded_memory_transfer.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace eon {

struct DeuterosAmigaObservedGraphicsVectorReturn {
    std::uint64_t trace_sequence = 0;
    std::uint32_t library_base_source_address = 0;
    std::uint32_t observed_library_base = 0;
    std::uint32_t call_address = 0;
    std::int16_t vector = 0;
    std::uint32_t return_address = 0;
    std::uint32_t result_d0 = 0;
    std::uint16_t result_sr = 0;
};

struct DeuterosAmigaTitleServiceBatchLocalPlan {
    DeuterosAmigaObservedGraphicsVectorReturn observed_return;
    std::uint32_t source_address = 0;
    std::uint32_t destination_address = 0;
    std::uint32_t count_d0 = 0;
    std::uint32_t graphics_helper_rts_address = 0;
    std::uint32_t second_call_address = 0;
    std::uint32_t second_call_target = 0;
    std::uint32_t zero_word_destination = 0;
    std::uint32_t literal_word_destination = 0;
    std::uint16_t literal_word_value = 0;
    std::uint32_t literal_long_destination = 0;
    std::uint32_t literal_long_value = 0;
    std::uint32_t unresolved_read_address = 0;
    std::uint32_t unresolved_read_source = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaObservedServiceWordRead {
    std::uint64_t trace_sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t source_address = 0;
    std::uint16_t observed_value = 0;
};

struct DeuterosAmigaTitlePostServiceWordLocalPlan {
    DeuterosAmigaObservedServiceWordRead observation;
    std::uint32_t destination_address = 0;
    std::uint16_t destination_value = 0;
    std::uint32_t routine_rts_address = 0;
    std::uint32_t batch_next_call_address = 0;
    std::uint32_t batch_next_call_target = 0;
    std::uint32_t nested_call_address = 0;
    std::uint32_t nested_call_target = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaTitleGraphicsServiceFirstLocalPlan {
    DeuterosAmigaObservedGraphicsVectorReturn observed_return;
    std::uint32_t local_d0_value = 0;
    std::uint32_t descriptor_a0_value = 0;
    std::uint32_t next_library_base_source_address = 0;
    std::uint32_t next_call_address = 0;
    std::int16_t next_vector = 0;
    std::uint32_t next_return_address = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaTitleGraphicsServiceSecondLocalPlan {
    DeuterosAmigaObservedGraphicsVectorReturn observed_return;
    std::uint32_t result_byte_destination = 0;
    std::uint8_t result_low_byte = 0;
    std::uint32_t pointer_destination = 0;
    std::uint32_t pointer_value = 0;
    std::uint32_t descriptor_address = 0;
    std::array<std::uint16_t, 3> descriptor_offsets{};
    std::array<std::uint16_t, 3> descriptor_values{};
    std::uint32_t next_a0_value = 0;
    std::uint32_t next_a1_value = 0;
    std::uint32_t next_a2_pointer_cell = 0;
    std::uint32_t next_library_base_source_address = 0;
    std::uint32_t next_call_address = 0;
    std::int16_t next_vector = 0;
    std::uint32_t next_return_address = 0;
    std::uint32_t stop_before_address = 0;
};

// The third graphics return completes `$20094`. The original caller then
// installs its literal local A6 value, tail-jumps to `$201d2`, and enters the
// first local setup wrapper. Keep pointer cells as addresses only: their
// runtime contents and the next graphics result are external observations.
struct DeuterosAmigaTitleGraphicsServiceThirdLocalPlan {
    DeuterosAmigaObservedGraphicsVectorReturn observed_return;
    std::uint32_t service_rts_address = 0;
    std::uint32_t dispatcher_return_address = 0;
    std::uint32_t dispatcher_a6_value = 0;
    std::uint32_t dispatcher_jump_address = 0;
    std::uint32_t tail_entry_address = 0;
    std::uint32_t first_call_address = 0;
    std::uint32_t first_call_target = 0;
    std::uint32_t next_a0_value = 0;
    std::uint32_t next_a1_value = 0;
    std::uint32_t next_a2_pointer_cell = 0;
    std::uint32_t next_library_base_source_address = 0;
    std::uint32_t next_call_address = 0;
    std::int16_t next_vector = 0;
    std::uint32_t next_return_address = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaTitleTailFirstGraphicsLocalPlan {
    DeuterosAmigaObservedGraphicsVectorReturn observed_return;
    std::uint32_t wrapper_rts_address = 0;
    std::uint32_t caller_resume_address = 0;
    std::uint32_t restored_a6_value = 0;
    std::uint32_t destination_base = 0;
    std::uint32_t source_address = 0;
    std::uint32_t copy_instruction_address = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaObservedTailCopyWords {
    std::uint64_t trace_sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t source_address = 0;
    std::array<std::uint16_t, 4> observed_words{};
};

struct DeuterosAmigaTitleTailCopyLocalPlan {
    DeuterosAmigaObservedTailCopyWords observation;
    std::array<std::uint32_t, 4> destination_addresses{};
    std::array<std::uint16_t, 4> destination_values{};
    std::array<std::uint32_t, 2> literal_destination_addresses{};
    std::array<std::uint16_t, 2> literal_values{};
    std::uint32_t next_call_address = 0;
    std::uint32_t next_call_target = 0;
    std::uint32_t unresolved_read_address = 0;
    std::uint32_t unresolved_read_source = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaObservedTailSelectionWords {
    std::uint64_t trace_sequence = 0;
    std::uint32_t instruction_address = 0;
    std::array<std::uint32_t, 8> source_addresses{};
    std::array<std::uint16_t, 8> observed_words{};
};

struct DeuterosAmigaTitleTailSelectionLocalPlan {
    DeuterosAmigaObservedTailSelectionWords observation;
    std::array<std::uint16_t, 2> selected_words{};
    std::array<std::uint32_t, 2> destination_addresses{};
    std::uint16_t graphics_d0 = 0;
    std::uint16_t graphics_d1 = 0;
    std::uint32_t a0_value = 0;
    std::uint32_t a1_value = 0;
    std::uint32_t library_base_source_address = 0;
    std::uint32_t next_call_address = 0;
    std::int16_t next_vector = 0;
    std::uint32_t next_return_address = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaTitleTailSecondGraphicsLocalPlan {
    DeuterosAmigaObservedGraphicsVectorReturn observed_return;
    std::uint32_t register_restore_address = 0;
    std::uint32_t wrapper_rts_address = 0;
    std::uint32_t caller_resume_address = 0;
    std::array<std::uint32_t, 2> literal_destination_addresses{};
    std::array<std::uint16_t, 2> literal_values{};
    std::uint32_t next_call_address = 0;
    std::uint32_t next_call_target = 0;
    std::uint32_t unresolved_read_address = 0;
    std::uint32_t unresolved_read_source = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaTitleTailRepeatedGraphicsLocalPlan {
    DeuterosAmigaObservedGraphicsVectorReturn observed_return;
    std::uint32_t register_restore_address = 0;
    std::uint32_t wrapper_rts_address = 0;
    std::uint32_t caller_resume_address = 0;
    std::uint32_t next_call_address = 0;
    std::uint32_t next_call_target = 0;
    std::uint32_t next_a0_value = 0;
    std::uint32_t next_a1_value = 0;
    std::uint32_t next_a2_pointer_cell = 0;
    std::uint32_t next_library_base_source_address = 0;
    std::uint32_t next_graphics_call_address = 0;
    std::int16_t next_graphics_vector = 0;
    std::uint32_t next_graphics_return_address = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaTitleTailRepeatedWrapperReturnPlan {
    DeuterosAmigaObservedGraphicsVectorReturn observed_return;
    std::uint32_t generation = 0;
    std::uint32_t wrapper_rts_address = 0;
    std::uint32_t tail_caller_resume_address = 0;
    std::uint32_t tail_rts_address = 0;
    std::uint32_t batch_third_return_address = 0;
    std::uint32_t batch_fourth_call_address = 0;
    std::uint32_t batch_fourth_call_target = 0;
    std::uint32_t batch_fourth_return_address = 0;
    std::uint32_t batch_rts_address = 0;
    std::uint32_t batch_caller_resume_address = 0;
    std::uint32_t unresolved_read_address = 0;
    std::uint32_t unresolved_read_source = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaObservedTailSourceTable {
    std::uint64_t trace_sequence = 0;
    std::uint32_t first_instruction_address = 0;
    std::array<std::uint32_t, 2> source_addresses{};
    std::array<std::uint32_t, 2> observed_longwords{};
};

struct DeuterosAmigaTitleTailSourceTableLocalPlan {
    DeuterosAmigaObservedTailSourceTable observation;
    std::array<std::uint32_t, 2> destination_addresses{};
    std::array<std::uint32_t, 2> destination_values{};
    std::uint32_t local_call_address = 0;
    std::uint32_t local_call_target = 0;
    std::uint32_t descriptor_address = 0;
    std::array<std::uint16_t, 4> descriptor_offsets{};
    std::array<std::uint32_t, 4> descriptor_values{};
    std::uint32_t exec_base_source_address = 0;
    std::uint32_t next_call_address = 0;
    std::int16_t next_vector = 0;
    std::uint32_t next_return_address = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaObservedTailExecReturn {
    std::uint64_t trace_sequence = 0;
    std::uint32_t exec_base_source_address = 0;
    std::uint32_t observed_exec_base = 0;
    std::uint32_t call_address = 0;
    std::int16_t vector = 0;
    std::uint32_t return_address = 0;
    std::uint32_t result_d0 = 0;
    std::uint16_t result_sr = 0;
};

struct DeuterosAmigaTitleTailExecReturnLocalPlan {
    DeuterosAmigaObservedTailExecReturn observation;
    std::uint32_t local_rts_address = 0;
    std::uint32_t caller_resume_address = 0;
    std::uint32_t next_call_address = 0;
    std::uint32_t next_call_target = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaObservedLocalCallReturn {
    std::uint64_t trace_sequence = 0;
    std::uint32_t call_address = 0;
    std::uint32_t call_target = 0;
    std::uint32_t return_address = 0;
    std::uint32_t result_d0 = 0;
    std::uint16_t result_sr = 0;
};

struct DeuterosAmigaTitleLoadServiceLocalPlan {
    DeuterosAmigaObservedLocalCallReturn observation;
    std::uint32_t d7_value = 0;
    std::uint32_t d1_value = 0;
    std::uint32_t d0_value = 0;
    std::uint32_t selector_read_address = 0;
    std::uint32_t selector_source_address = 0;
    std::uint32_t stop_before_address = 0;
};

enum class DeuterosAmigaTitleLoadServiceOutcome { zero_retry_boundary, one_exit, copy_boundary };

struct DeuterosAmigaObservedLoadSelector {
    std::uint64_t trace_sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t source_address = 0;
    std::uint16_t observed_value = 0;
};

struct DeuterosAmigaTitleLoadServiceSelectorPlan {
    DeuterosAmigaObservedLoadSelector observation;
    DeuterosAmigaTitleLoadServiceOutcome outcome =
        DeuterosAmigaTitleLoadServiceOutcome::zero_retry_boundary;
    std::uint32_t copy_source_address = 0;
    std::uint32_t copy_destination_address = 0;
    std::uint32_t copy_longword_count = 0;
    std::uint32_t next_address = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaObservedLoadCopyChunk {
    std::uint64_t trace_sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t source_address = 0;
    std::uint32_t destination_address = 0;
    std::uint32_t first_longword_index = 0;
    std::vector<std::uint32_t> observed_longwords;
};

struct DeuterosAmigaTitleLoadCopyChunkPlan {
    DeuterosAmigaObservedLoadCopyChunk observation;
    std::vector<std::uint32_t> destination_addresses;
    std::vector<std::uint32_t> destination_values;
    std::uint32_t completed_longwords = 0;
    std::uint32_t remaining_longwords = 0;
    std::uint32_t next_source_address = 0;
    std::uint32_t next_destination_address = 0;
    bool copy_complete = false;
    std::uint32_t loop_instruction_address = 0;
    std::uint32_t local_rts_address = 0;
    std::uint32_t caller_resume_address = 0;
    std::uint32_t next_call_address = 0;
    std::uint32_t next_call_target = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaObservedLoadDispatchTableBase {
    std::uint64_t trace_sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t source_address = 0;
    std::uint32_t observed_value = 0;
};

struct DeuterosAmigaTitleLoadDispatchTableBasePlan {
    DeuterosAmigaObservedLoadDispatchTableBase observation;
    std::uint32_t caller_address = 0;
    std::uint32_t call_address = 0;
    std::uint32_t call_target = 0;
    std::uint32_t index_value = 0;
    std::uint32_t table_word_address = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaObservedLoadDispatchTableWord {
    std::uint64_t trace_sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t source_address = 0;
    std::uint16_t observed_value = 0;
};

struct DeuterosAmigaTitleLoadDispatchLocalPlan {
    DeuterosAmigaObservedLoadDispatchTableWord observation;
    std::int16_t signed_offset = 0;
    std::uint32_t command_stream_address = 0;
    std::uint32_t nested_call_address = 0;
    std::uint32_t nested_call_target = 0;
    std::uint32_t byte_write_address = 0;
    std::uint8_t byte_write_value = 0;
    std::uint32_t stop_before_address = 0;
};

enum class DeuterosAmigaTitleCommandOpcodeOutcome {
    complete,
    operand_byte_boundary,
    pointer_copy_boundary,
    runtime_long_boundary,
    local_no_op,
    unresolved_boundary,
};

struct DeuterosAmigaObservedTitleCommandOpcode {
    std::uint64_t trace_sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t source_address = 0;
    std::uint8_t observed_value = 0;
};

struct DeuterosAmigaTitleCommandOpcodePlan {
    DeuterosAmigaObservedTitleCommandOpcode observation;
    DeuterosAmigaTitleCommandOpcodeOutcome outcome =
        DeuterosAmigaTitleCommandOpcodeOutcome::unresolved_boundary;
    std::uint32_t next_stream_address = 0;
    std::uint32_t next_observation_instruction_address = 0;
    std::uint32_t unresolved_call_address = 0;
    std::uint32_t unresolved_call_target = 0;
    std::uint32_t parser_return_address = 0;
    std::uint32_t dispatcher_return_address = 0;
    std::uint32_t caller_resume_address = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaObservedTitleCommandOperandByte {
    std::uint64_t trace_sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t source_address = 0;
    std::uint8_t observed_value = 0;
};

struct DeuterosAmigaTitleCommandOperandLocalPlan {
    DeuterosAmigaObservedTitleCommandOperandByte observation;
    std::uint32_t destination_address = 0;
    std::uint32_t destination_value = 0;
    std::uint32_t next_stream_address = 0;
    std::uint32_t next_opcode_read_address = 0;
};

struct DeuterosAmigaObservedTitleCommandPointerLong {
    std::uint64_t trace_sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t source_address = 0;
    std::uint32_t observed_value = 0;
};

struct DeuterosAmigaTitleCommandPointerCopyPlan {
    DeuterosAmigaObservedTitleCommandPointerLong observation;
    std::uint32_t destination_address = 0;
    std::uint32_t destination_value = 0;
    std::uint32_t next_stream_address = 0;
    std::uint32_t next_opcode_read_address = 0;
};

struct DeuterosAmigaObservedTitleCommandEightPointer {
    std::uint64_t trace_sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t source_address = 0;
    std::uint32_t observed_value = 0;
};

struct DeuterosAmigaTitleCommandEightPointerPlan {
    DeuterosAmigaObservedTitleCommandEightPointer observation;
    std::uint32_t mode_read_instruction_address = 0;
    std::uint32_t mode_source_address = 0;
};

struct DeuterosAmigaObservedTitleCommandEightMode {
    std::uint64_t trace_sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t source_address = 0;
    std::uint8_t observed_value = 0;
};

struct DeuterosAmigaTitleCommandEightModePlan {
    DeuterosAmigaObservedTitleCommandEightMode observation;
    bool scale_read_required = false;
    std::uint32_t scale_read_instruction_address = 0;
    std::uint32_t scale_source_address = 0;
    std::uint32_t destination_value = 0;
    std::array<std::uint32_t, 2> destination_addresses{};
    std::uint32_t next_stream_address = 0;
    std::uint32_t next_opcode_read_address = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaObservedTitleCommandEightScale {
    std::uint64_t trace_sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t source_address = 0;
    std::uint32_t observed_value = 0;
};

struct DeuterosAmigaTitleCommandEightScalePlan {
    DeuterosAmigaObservedTitleCommandEightScale observation;
    std::uint32_t word_shifted_scale = 0;
    std::uint32_t destination_value = 0;
    std::array<std::uint32_t, 2> destination_addresses{};
    std::uint32_t next_stream_address = 0;
    std::uint32_t next_opcode_read_address = 0;
};

struct DeuterosAmigaObservedTitleCommandCallReturn {
    std::uint64_t trace_sequence = 0;
    std::uint32_t call_address = 0;
    std::uint32_t call_target = 0;
    std::uint32_t return_address = 0;
    std::uint32_t result_a4 = 0;
    std::uint32_t result_d0 = 0;
    std::uint16_t result_sr = 0;
};

struct DeuterosAmigaTitleCommandCallReturnPlan {
    DeuterosAmigaObservedTitleCommandCallReturn observation;
    std::uint32_t next_stream_address = 0;
    std::uint32_t next_opcode_read_address = 0;
};

class DeuterosAmigaTitleServiceBatchBoundarySession {
public:
    DeuterosAmigaTitleServiceBatchBoundarySession(
        const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan) {
        const auto at = [&](std::uint32_t address, std::size_t length) {
            return disk.bytes(plan.title_stage.disk_offset
                + address - plan.title_stage.destination, length);
        };
        const auto batch = parse_deuteros_amiga_title_post_exec_service_batch_profile(
            disk, plan);
        third_service_ = parse_deuteros_amiga_title_post_exec_third_service_profile(
            disk, plan);
        tail_dispatch_ = parse_deuteros_amiga_title_post_exec_tail_dispatch_profile(
            disk, plan);
        tail_first_callee_ =
            parse_deuteros_amiga_title_post_exec_tail_first_callee_profile(disk, plan);
        tail_second_callee_ =
            parse_deuteros_amiga_title_post_exec_tail_second_callee_profile(disk, plan);
        tail_fourth_callee_ =
            parse_deuteros_amiga_title_post_exec_tail_fourth_callee_profile(disk, plan);
        fourth_service_ = parse_deuteros_amiga_title_post_exec_fourth_service_profile(
            disk, plan);
        tail_return_ = parse_deuteros_amiga_title_post_exec_tail_return_profile(disk, plan);
        load_service_ = parse_deuteros_amiga_title_post_exec_load_service_profile(disk, plan);
        load_dispatch_ = parse_deuteros_amiga_title_post_load_dispatch_profile(disk, plan);
        command_interpreter_ =
            parse_deuteros_amiga_title_command_interpreter_profile(disk, plan);
        if (to_hex(sha256(at(0x403c8, 30)))
                != "3f9cf2302a4078faddd0796fc05268386d46c4be64f294b8082ba085b9609f5f"
            || to_hex(sha256(at(0x20510, 38)))
                != "60ee2fcb4a18f62cd2066aba2429e760a64f14cd3f07f3cfe8467972030008bc"
            || to_hex(sha256(at(0x1f37a, 24)))
                != "9dd36cbd04608b7381526479275f1846284f87886b725bf581ade813f70d10f5"
            || batch.call_site_address != 0x404ce || batch.callee_address != 0x403f4
            || batch.direct_callee_addresses[0] != 0x403c8
            || batch.direct_callee_addresses[1] != 0x20510
            || batch.direct_callee_addresses[2] != 0x1f37a) {
            throw std::runtime_error("Unsupported Deuteros service batch prefix");
        }
    }

    void enter(std::uint64_t preceding_sequence, std::uint32_t graphics_library_base) {
        if (armed_ || preceding_sequence == 0 || graphics_library_base == 0) {
            throw std::runtime_error("Invalid Deuteros service batch entry");
        }
        armed_ = true;
        preceding_sequence_ = preceding_sequence;
        graphics_library_base_ = graphics_library_base;
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleServiceBatchLocalPlan>
    observe_graphics_return(const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        if (!armed_ || observed_) return std::nullopt;
        if (observation.trace_sequence <= preceding_sequence_
            || observation.library_base_source_address != 0x12fec
            || observation.observed_library_base != graphics_library_base_
            || observation.call_address != 0x403e0 || observation.vector != -0xc0
            || observation.return_address != 0x403e4) {
            throw std::runtime_error("Deuteros graphics return does not match service batch");
        }
        observed_ = observation;
        return DeuterosAmigaTitleServiceBatchLocalPlan{observation,
            0x1ed24, 0x12e12, 0x14, 0x403e4, 0x403fa, 0x20510,
            0x202c4, 0x2027e, 0xf690, 0x20280, 1,
            0x2052a, 0x20276, 0x2052a};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitlePostServiceWordLocalPlan>
    observe_runtime_word(const DeuterosAmigaObservedServiceWordRead& observation) {
        if (!observed_ || observed_word_) return std::nullopt;
        if (observation.trace_sequence <= observed_->trace_sequence
            || observation.instruction_address != 0x2052a
            || observation.source_address != 0x20276) {
            throw std::runtime_error("Deuteros service word observation does not match boundary");
        }
        observed_word_ = observation;
        return DeuterosAmigaTitlePostServiceWordLocalPlan{observation,
            0x2027c, observation.observed_value, 0x20534,
            0x40400, 0x1f37a, 0x1f37a, 0x20094, 0x1f37a};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleGraphicsServiceFirstLocalPlan>
    observe_graphics_service_first_return(
        const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        if (!observed_word_ || observed_graphics_service_first_) return std::nullopt;
        if (observation.trace_sequence <= observed_word_->trace_sequence
            || observation.library_base_source_address != third_service_.graphics_library_base_address
            || observation.observed_library_base != graphics_library_base_
            || observation.call_address != 0x2009c
            || observation.vector != third_service_.graphics_library_vectors[0]
            || observation.return_address != 0x200a0) {
            throw std::runtime_error("Deuteros first graphics-service return does not match boundary");
        }
        observed_graphics_service_first_ = observation;
        return DeuterosAmigaTitleGraphicsServiceFirstLocalPlan{observation,
            0xffffffff, third_service_.descriptor_address,
            third_service_.graphics_library_base_address,
            0x200b0, third_service_.graphics_library_vectors[1], 0x200b4, 0x200b0};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleGraphicsServiceSecondLocalPlan>
    observe_graphics_service_second_return(
        const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        if (!observed_graphics_service_first_ || observed_graphics_service_second_) {
            return std::nullopt;
        }
        if (observation.trace_sequence
                <= observed_graphics_service_first_->trace_sequence
            || observation.library_base_source_address
                != third_service_.graphics_library_base_address
            || observation.observed_library_base != graphics_library_base_
            || observation.call_address != 0x200b0
            || observation.vector != third_service_.graphics_library_vectors[1]
            || observation.return_address != 0x200b4) {
            throw std::runtime_error("Deuteros second graphics-service return does not match boundary");
        }
        observed_graphics_service_second_ = observation;
        return DeuterosAmigaTitleGraphicsServiceSecondLocalPlan{observation,
            third_service_.status_byte_address,
            static_cast<std::uint8_t>(observation.result_d0 & 0xffU),
            third_service_.destination_pointer_cell_address,
            third_service_.destination_pointer_literal, third_service_.descriptor_address,
            third_service_.descriptor_offsets, third_service_.descriptor_values,
            0x12e12, third_service_.descriptor_address,
            third_service_.destination_pointer_cell_address,
            third_service_.graphics_library_base_address,
            0x200f4, third_service_.graphics_library_vectors[2], 0x200f8, 0x200f4};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleGraphicsServiceThirdLocalPlan>
    observe_graphics_service_third_return(
        const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        if (!observed_graphics_service_second_ || observed_graphics_service_third_) {
            return std::nullopt;
        }
        if (observation.trace_sequence
                <= observed_graphics_service_second_->trace_sequence
            || observation.library_base_source_address
                != third_service_.graphics_library_base_address
            || observation.observed_library_base != graphics_library_base_
            || observation.call_address != 0x200f4
            || observation.vector != third_service_.graphics_library_vectors[2]
            || observation.return_address != 0x200f8) {
            throw std::runtime_error("Deuteros third graphics-service return does not match boundary");
        }
        observed_graphics_service_third_ = observation;
        return DeuterosAmigaTitleGraphicsServiceThirdLocalPlan{observation,
            0x200f8, 0x1f380, third_service_.dispatcher_a6_literal,
            third_service_.dispatcher_tail_jump_address, tail_dispatch_.entry_address,
            tail_first_callee_.caller_address, tail_first_callee_.entry_address,
            tail_first_callee_.a0_literal, tail_first_callee_.a1_literal,
            tail_first_callee_.a2_pointer_cell_address,
            tail_first_callee_.graphics_library_base_address,
            0x20112, tail_first_callee_.graphics_library_vector,
            tail_first_callee_.vector_return_address, 0x20112};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleTailFirstGraphicsLocalPlan>
    observe_tail_first_graphics_return(
        const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        if (!observed_graphics_service_third_ || observed_tail_first_graphics_) {
            return std::nullopt;
        }
        if (observation.trace_sequence <= observed_graphics_service_third_->trace_sequence
            || observation.library_base_source_address
                != tail_first_callee_.graphics_library_base_address
            || observation.observed_library_base != graphics_library_base_
            || observation.call_address != 0x20112
            || observation.vector != tail_first_callee_.graphics_library_vector
            || observation.return_address != tail_first_callee_.vector_return_address) {
            throw std::runtime_error("Deuteros tail first graphics return does not match boundary");
        }
        observed_tail_first_graphics_ = observation;
        return DeuterosAmigaTitleTailFirstGraphicsLocalPlan{observation,
            tail_first_callee_.vector_return_address, 0x201da,
            third_service_.dispatcher_a6_literal, 0x1ffc8, 0x1f372,
            0x201e4, 0x201e4};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleTailCopyLocalPlan>
    observe_tail_copy_words(const DeuterosAmigaObservedTailCopyWords& observation) {
        if (!observed_tail_first_graphics_ || observed_tail_copy_words_) {
            return std::nullopt;
        }
        if (observation.trace_sequence <= observed_tail_first_graphics_->trace_sequence
            || observation.instruction_address != 0x201e4
            || observation.source_address != third_service_.dispatcher_a6_literal) {
            throw std::runtime_error("Deuteros tail word-copy observation does not match boundary");
        }
        observed_tail_copy_words_ = observation;
        return DeuterosAmigaTitleTailCopyLocalPlan{observation,
            {{0x1ffca, 0x1ffcc, 0x1ffd0, 0x1ffd2}}, observation.observed_words,
            {{0x1ee12, 0x1ee10}}, {{0xffff, 0xffff}},
            tail_second_callee_.caller_address, tail_second_callee_.entry_address,
            tail_second_callee_.entry_address, 0x1ffc8, tail_second_callee_.entry_address};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleTailSelectionLocalPlan>
    observe_tail_selection_words(
        const DeuterosAmigaObservedTailSelectionWords& observation) {
        constexpr std::array<std::uint32_t, 8> sources{{
            0x1ffc8, 0x1ee10, 0x1ffca, 0x1ffcc,
            0x1ffce, 0x1ee12, 0x1ffd0, 0x1ffd2}};
        if (!observed_tail_copy_words_ || observed_tail_selection_words_) {
            return std::nullopt;
        }
        if (observation.trace_sequence <= observed_tail_copy_words_->trace_sequence
            || observation.instruction_address != tail_second_callee_.entry_address
            || observation.source_addresses != sources) {
            throw std::runtime_error("Deuteros tail selection observation does not match boundary");
        }
        observed_tail_selection_words_ = observation;
        const auto select = [](std::uint16_t current, std::uint16_t adjustment,
                               std::uint16_t lower, std::uint16_t upper) {
            const auto sum = static_cast<std::uint16_t>(current + adjustment);
            if (static_cast<std::int16_t>(adjustment) < 0) {
                return static_cast<std::int16_t>(sum) < 0 || sum < lower ? lower : sum;
            }
            return sum < upper ? sum : upper;
        };
        const auto first = select(observation.observed_words[0], observation.observed_words[1],
                                  observation.observed_words[2], observation.observed_words[3]);
        const auto second = select(observation.observed_words[4], observation.observed_words[5],
                                   observation.observed_words[6], observation.observed_words[7]);
        return DeuterosAmigaTitleTailSelectionLocalPlan{observation, {{first, second}},
            {{0x1ffc8, 0x1ffce}},
            static_cast<std::uint16_t>(static_cast<std::uint16_t>(first - 0x10U) >> 1U),
            static_cast<std::uint16_t>(second - 6U),
            tail_second_callee_.a0_literal, tail_second_callee_.a1_literal,
            tail_second_callee_.graphics_library_base_address,
            0x201b6, tail_second_callee_.graphics_library_vector,
            tail_second_callee_.vector_return_address, 0x201b6};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleTailSecondGraphicsLocalPlan>
    observe_tail_second_graphics_return(
        const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        if (!observed_tail_selection_words_ || observed_tail_second_graphics_) {
            return std::nullopt;
        }
        if (observation.trace_sequence <= observed_tail_selection_words_->trace_sequence
            || observation.library_base_source_address
                != tail_second_callee_.graphics_library_base_address
            || observation.observed_library_base != graphics_library_base_
            || observation.call_address != 0x201b6
            || observation.vector != tail_second_callee_.graphics_library_vector
            || observation.return_address != tail_second_callee_.vector_return_address) {
            throw std::runtime_error("Deuteros tail second graphics return does not match boundary");
        }
        observed_tail_second_graphics_ = observation;
        return DeuterosAmigaTitleTailSecondGraphicsLocalPlan{observation,
            0x201ba, 0x201be, tail_second_callee_.caller_continuation_address,
            {{0x1ee12, 0x1ee10}}, {{1, 1}}, 0x20212, 0x20118,
            0x20118, 0x1ffc8, 0x20118};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleTailSelectionLocalPlan>
    observe_tail_repeated_selection_words(
        const DeuterosAmigaObservedTailSelectionWords& observation) {
        constexpr std::array<std::uint32_t, 8> sources{{
            0x1ffc8, 0x1ee10, 0x1ffca, 0x1ffcc,
            0x1ffce, 0x1ee12, 0x1ffd0, 0x1ffd2}};
        if (!observed_tail_second_graphics_ || observed_tail_repeated_selection_words_) {
            return std::nullopt;
        }
        if (observation.trace_sequence <= observed_tail_second_graphics_->trace_sequence
            || observation.instruction_address != tail_second_callee_.entry_address
            || observation.source_addresses != sources) {
            throw std::runtime_error("Deuteros repeated tail selection does not match boundary");
        }
        observed_tail_repeated_selection_words_ = observation;
        const auto select = [](std::uint16_t current, std::uint16_t adjustment,
                               std::uint16_t lower, std::uint16_t upper) {
            const auto sum = static_cast<std::uint16_t>(current + adjustment);
            if (static_cast<std::int16_t>(adjustment) < 0) {
                return static_cast<std::int16_t>(sum) < 0 || sum < lower ? lower : sum;
            }
            return sum < upper ? sum : upper;
        };
        const auto first = select(observation.observed_words[0], observation.observed_words[1],
                                  observation.observed_words[2], observation.observed_words[3]);
        const auto second = select(observation.observed_words[4], observation.observed_words[5],
                                   observation.observed_words[6], observation.observed_words[7]);
        return DeuterosAmigaTitleTailSelectionLocalPlan{observation, {{first, second}},
            {{0x1ffc8, 0x1ffce}},
            static_cast<std::uint16_t>(static_cast<std::uint16_t>(first - 0x10U) >> 1U),
            static_cast<std::uint16_t>(second - 6U),
            tail_second_callee_.a0_literal, tail_second_callee_.a1_literal,
            tail_second_callee_.graphics_library_base_address,
            0x201b6, tail_second_callee_.graphics_library_vector,
            tail_second_callee_.vector_return_address, 0x201b6};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleTailRepeatedGraphicsLocalPlan>
    observe_tail_repeated_graphics_return(
        const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        if (!observed_tail_repeated_selection_words_ || observed_tail_repeated_graphics_) {
            return std::nullopt;
        }
        if (observation.trace_sequence
                <= observed_tail_repeated_selection_words_->trace_sequence
            || observation.library_base_source_address
                != tail_second_callee_.graphics_library_base_address
            || observation.observed_library_base != graphics_library_base_
            || observation.call_address != 0x201b6
            || observation.vector != tail_second_callee_.graphics_library_vector
            || observation.return_address != tail_second_callee_.vector_return_address) {
            throw std::runtime_error("Deuteros repeated tail graphics return does not match boundary");
        }
        observed_tail_repeated_graphics_ = observation;
        return DeuterosAmigaTitleTailRepeatedGraphicsLocalPlan{observation,
            0x201ba, 0x201be, 0x20216, tail_fourth_callee_.caller_address,
            tail_fourth_callee_.entry_address, tail_fourth_callee_.a0_literal,
            tail_fourth_callee_.a1_literal, tail_fourth_callee_.a2_pointer_cell_address,
            tail_fourth_callee_.graphics_library_base_address, 0x200f4,
            tail_fourth_callee_.graphics_library_vector,
            tail_fourth_callee_.vector_return_address, 0x200f4};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleTailRepeatedWrapperReturnPlan>
    observe_tail_repeated_wrapper_graphics_return(
        const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        if (!observed_tail_repeated_graphics_ || observed_tail_repeated_wrapper_graphics_) {
            return std::nullopt;
        }
        if (observation.trace_sequence <= observed_tail_repeated_graphics_->trace_sequence
            || observation.library_base_source_address
                != tail_fourth_callee_.graphics_library_base_address
            || observation.observed_library_base != graphics_library_base_
            || observation.call_address != 0x200f4
            || observation.vector != tail_fourth_callee_.graphics_library_vector
            || observation.return_address != tail_fourth_callee_.vector_return_address) {
            throw std::runtime_error("Deuteros repeated wrapper graphics return does not match boundary");
        }
        observed_tail_repeated_wrapper_graphics_ = observation;
        return DeuterosAmigaTitleTailRepeatedWrapperReturnPlan{observation, 2,
            tail_fourth_callee_.vector_return_address,
            tail_fourth_callee_.caller_continuation_address,
            tail_dispatch_.return_address - 2U, third_service_.caller_address + 6U,
            fourth_service_.caller_address, fourth_service_.callee_address,
            fourth_service_.caller_return_address, fourth_service_.caller_return_address,
            tail_return_.continuation_address, 0x404da,
            tail_return_.source_table_address, 0x404da};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleTailSourceTableLocalPlan>
    observe_tail_source_table(const DeuterosAmigaObservedTailSourceTable& observation) {
        constexpr std::array<std::uint32_t, 2> sources{{0x12ff4, 0x12ff8}};
        if (!observed_tail_repeated_wrapper_graphics_ || observed_tail_source_table_) {
            return std::nullopt;
        }
        if (observation.trace_sequence
                <= observed_tail_repeated_wrapper_graphics_->trace_sequence
            || observation.first_instruction_address != 0x404da
            || observation.source_addresses != sources) {
            throw std::runtime_error("Deuteros tail source-table observation does not match boundary");
        }
        observed_tail_source_table_ = observation;
        return DeuterosAmigaTitleTailSourceTableLocalPlan{observation,
            tail_return_.destination_addresses, observation.observed_longwords,
            tail_return_.local_service_call_address, tail_return_.local_service_address,
            tail_return_.service_a1_literal, tail_return_.service_a1_offsets,
            {{2, 0xc4, tail_return_.service_long_literals[0],
                tail_return_.service_long_literals[1]}},
            tail_return_.exec_base_address, 0x204f4, tail_return_.exec_vector,
            tail_return_.vector_return_address, 0x204f4};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleTailExecReturnLocalPlan>
    observe_tail_exec_return(const DeuterosAmigaObservedTailExecReturn& observation) {
        if (!observed_tail_source_table_ || observed_tail_exec_return_) {
            return std::nullopt;
        }
        if (observation.trace_sequence <= observed_tail_source_table_->trace_sequence
            || observation.exec_base_source_address != tail_return_.exec_base_address
            || observation.observed_exec_base == 0
            || observation.call_address != 0x204f4
            || observation.vector != tail_return_.exec_vector
            || observation.return_address != tail_return_.vector_return_address) {
            throw std::runtime_error("Deuteros tail Exec return does not match boundary");
        }
        observed_tail_exec_return_ = observation;
        return DeuterosAmigaTitleTailExecReturnLocalPlan{observation,
            tail_return_.vector_return_address, 0x404f0,
            0x404f0, 0x389e2, 0x404f0};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleLoadServiceLocalPlan>
    observe_load_service_return(const DeuterosAmigaObservedLocalCallReturn& observation) {
        if (!observed_tail_exec_return_ || observed_load_service_return_) return std::nullopt;
        if (observation.trace_sequence <= observed_tail_exec_return_->trace_sequence
            || observation.call_address != load_service_.nested_call_address
            || observation.call_target != load_service_.nested_call_target
            || observation.return_address != load_service_.nested_return_address) {
            throw std::runtime_error("Deuteros load-service return does not match boundary");
        }
        observed_load_service_return_ = observation;
        return DeuterosAmigaTitleLoadServiceLocalPlan{observation,
            load_service_.d7_value, load_service_.d1_value, load_service_.d0_value,
            0x389fa, load_service_.selector_word_address, 0x389fa};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleLoadServiceSelectorPlan>
    observe_load_selector(const DeuterosAmigaObservedLoadSelector& observation) {
        if (!observed_load_service_return_ || observed_load_selector_) return std::nullopt;
        if (observation.trace_sequence <= observed_load_service_return_->trace_sequence
            || observation.instruction_address != 0x389fa
            || observation.source_address != load_service_.selector_word_address) {
            throw std::runtime_error("Deuteros load selector does not match boundary");
        }
        observed_load_selector_ = observation;
        const auto selector = static_cast<std::uint8_t>(observation.observed_value & 0xffU);
        if (selector == 0) {
            return DeuterosAmigaTitleLoadServiceSelectorPlan{observation,
                DeuterosAmigaTitleLoadServiceOutcome::zero_retry_boundary,
                0, 0, 0, 0x389a6, 0x389aa};
        }
        if (selector == 1) {
            return DeuterosAmigaTitleLoadServiceSelectorPlan{observation,
                DeuterosAmigaTitleLoadServiceOutcome::one_exit,
                0, 0, 0, 0x404f6, 0x404f8};
        }
        const auto source = selector == 2 ? 0x26cc0U : 0x29540U;
        load_copy_transfer_.emplace(BoundedMemoryTransferContract{
            0x38a28, source, load_service_.copy_destination, 4, 4,
            load_service_.copy_longword_count, 256,
            MemoryTransferElementWidth::longword, 0x1000000});
        return DeuterosAmigaTitleLoadServiceSelectorPlan{observation,
            DeuterosAmigaTitleLoadServiceOutcome::copy_boundary,
            source, load_service_.copy_destination, load_service_.copy_longword_count,
            0x38a28, 0x38a28};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleLoadCopyChunkPlan>
    observe_load_copy_chunk(const DeuterosAmigaObservedLoadCopyChunk& observation) {
        if (!load_copy_transfer_) {
            return std::nullopt;
        }
        if (load_copy_transfer_->checkpoint().next_index == 0
            && observation.trace_sequence <= observed_load_selector_->trace_sequence) {
            throw std::runtime_error("Deuteros load copy precedes its selector generation");
        }
        const auto count = static_cast<std::uint32_t>(observation.observed_longwords.size());
        const auto admitted = load_copy_transfer_->observe_chunk({observation.trace_sequence,
            observation.instruction_address, observation.first_longword_index,
            observation.source_address, observation.destination_address,
            observation.observed_longwords});
        if (!admitted.accepted) {
            if (load_copy_transfer_->checkpoint().complete) return std::nullopt;
            throw std::runtime_error("Deuteros load copy chunk does not match boundary: "
                + admitted.error);
        }
        const auto checkpoint = load_copy_transfer_->checkpoint();
        std::vector<std::uint32_t> destinations;
        destinations.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index) {
            destinations.push_back(observation.destination_address + index * 4U);
        }
        const auto completed = static_cast<std::uint32_t>(checkpoint.next_index);
        const auto remaining = load_service_.copy_longword_count - completed;
        const bool complete = checkpoint.complete;
        return DeuterosAmigaTitleLoadCopyChunkPlan{observation, std::move(destinations),
            observation.observed_longwords, completed, remaining,
            static_cast<std::uint32_t>(checkpoint.contract.source_base + completed * 4U),
            static_cast<std::uint32_t>(checkpoint.contract.destination_base + completed * 4U),
            complete, 0x38a28, complete ? 0x38a2eU : 0U,
            complete ? 0x404f6U : 0U, complete ? 0x404f8U : 0U,
            complete ? 0x1fb9aU : 0U, complete ? 0x404f8U : 0x38a28U};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleLoadDispatchTableBasePlan>
    observe_load_dispatch_table_base(
        const DeuterosAmigaObservedLoadDispatchTableBase& observation) {
        if (!observed_load_selector_ || observed_load_dispatch_table_base_) {
            return std::nullopt;
        }
        const auto selector = static_cast<std::uint8_t>(
            observed_load_selector_->observed_value & 0xffU);
        const bool ready = selector == 1
            || (load_copy_transfer_ && load_copy_transfer_->checkpoint().complete);
        const auto preceding_sequence = selector == 1
            ? observed_load_selector_->trace_sequence
            : load_copy_transfer_->checkpoint().last_sequence;
        if (!ready) return std::nullopt;
        if (observation.trace_sequence <= preceding_sequence
            || observation.instruction_address != load_dispatch_.entry_address
            || observation.source_address != load_dispatch_.table_base_cell_address) {
            throw std::runtime_error("Deuteros load-dispatch table base does not match boundary");
        }
        if (observation.observed_value > std::numeric_limits<std::uint32_t>::max()
                - load_dispatch_.index_value * 2U) {
            throw std::runtime_error("Deuteros load-dispatch table address overflows");
        }
        observed_load_dispatch_table_base_ = observation;
        return DeuterosAmigaTitleLoadDispatchTableBasePlan{observation,
            load_dispatch_.caller_address, load_dispatch_.caller_address + 2U,
            load_dispatch_.entry_address, load_dispatch_.index_value,
            observation.observed_value + load_dispatch_.index_value * 2U,
            load_dispatch_.table_word_instruction_address};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleLoadDispatchLocalPlan>
    observe_load_dispatch_table_word(
        const DeuterosAmigaObservedLoadDispatchTableWord& observation) {
        if (!observed_load_dispatch_table_base_ || observed_load_dispatch_table_word_) {
            return std::nullopt;
        }
        const auto expected_source = observed_load_dispatch_table_base_->observed_value
            + load_dispatch_.index_value * 2U;
        if (observation.trace_sequence
                <= observed_load_dispatch_table_base_->trace_sequence
            || observation.instruction_address
                != load_dispatch_.table_word_instruction_address
            || observation.source_address != expected_source) {
            throw std::runtime_error("Deuteros load-dispatch table word does not match boundary");
        }
        const auto offset = static_cast<std::int16_t>(observation.observed_value);
        const auto target = static_cast<std::int64_t>(
            observed_load_dispatch_table_base_->observed_value) + offset;
        if (target < 0
            || target > std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error("Deuteros load-dispatch command address overflows");
        }
        observed_load_dispatch_table_word_ = observation;
        next_command_address_ = static_cast<std::uint32_t>(target);
        return DeuterosAmigaTitleLoadDispatchLocalPlan{observation, offset,
            static_cast<std::uint32_t>(target), load_dispatch_.nested_call_address,
            load_dispatch_.nested_call_target, load_dispatch_.parser_mode_byte_address,
            load_dispatch_.parser_mode_byte_value, load_dispatch_.first_command_read_address};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandOpcodePlan>
    observe_command_opcode(const DeuterosAmigaObservedTitleCommandOpcode& observation) {
        if (!observed_load_dispatch_table_word_ || command_halted_
            || pending_command_opcode_ || pending_command_call_) {
            return std::nullopt;
        }
        const auto preceding_sequence = last_command_sequence_ == 0
            ? observed_load_dispatch_table_word_->trace_sequence : last_command_sequence_;
        if (observation.trace_sequence <= preceding_sequence
            || observation.instruction_address != command_interpreter_.opcode_read_address
            || observation.source_address != next_command_address_) {
            throw std::runtime_error("Deuteros command opcode does not match boundary");
        }
        last_command_sequence_ = observation.trace_sequence;
        ++next_command_address_;
        const auto opcode = observation.observed_value;
        if (opcode == 0) {
            command_halted_ = true;
            return DeuterosAmigaTitleCommandOpcodePlan{observation,
                DeuterosAmigaTitleCommandOpcodeOutcome::complete,
                next_command_address_, 0, 0, 0, command_interpreter_.return_address,
                0x1fbae, 0x404fe, 0x404fe};
        }
        if (opcode == 0x10 || opcode == 0x11) {
            pending_command_opcode_ = opcode;
            return DeuterosAmigaTitleCommandOpcodePlan{observation,
                DeuterosAmigaTitleCommandOpcodeOutcome::operand_byte_boundary,
                next_command_address_, opcode == 0x10 ? 0x1fa2eU : 0x1fa3aU,
                0, 0, 0, 0, 0, opcode == 0x10 ? 0x1fa2eU : 0x1fa3aU};
        }
        if (opcode == 0x07) {
            pending_command_opcode_ = opcode;
            return DeuterosAmigaTitleCommandOpcodePlan{observation,
                DeuterosAmigaTitleCommandOpcodeOutcome::pointer_copy_boundary,
                next_command_address_, 0x1fa5e, 0, 0, 0, 0, 0, 0x1fa5e};
        }
        if (opcode == 0x08) {
            pending_command_opcode_ = opcode;
            return DeuterosAmigaTitleCommandOpcodePlan{observation,
                DeuterosAmigaTitleCommandOpcodeOutcome::runtime_long_boundary,
                next_command_address_, 0x1fa70, 0, 0, 0, 0, 0, 0x1fa70};
        }
        std::uint32_t call_address = 0;
        std::uint32_t call_target = 0;
        std::uint32_t boundary = 0;
        if (opcode == 0x16) { boundary = 0x1fb0c; }
        else if (opcode == 0x1a) { call_address = 0x1fa46; call_target = 0x1fde6; }
        else if (opcode == 0x12) { call_address = 0x1fa52; call_target = 0x1fe3c; }
        else if (opcode == 0x04) { call_address = 0x1faaa; call_target = 0x402ac; }
        else if (opcode < 0x20) {
            return DeuterosAmigaTitleCommandOpcodePlan{observation,
                DeuterosAmigaTitleCommandOpcodeOutcome::local_no_op,
                next_command_address_, command_interpreter_.opcode_read_address,
                command_interpreter_.no_op_call_address,
                command_interpreter_.no_op_target, 0, 0, 0,
                command_interpreter_.opcode_read_address};
        }
        else if (opcode < 0x90) { call_address = 0x1fac2; call_target = 0x1fbe6; }
        else { boundary = 0x1fada; }
        if (call_address != 0) {
            pending_command_call_ = PendingCommandCall{
                call_address, call_target, call_address + (call_address == 0x1faaa ? 6U : 4U)};
        } else {
            command_halted_ = true;
        }
        return DeuterosAmigaTitleCommandOpcodePlan{observation,
            DeuterosAmigaTitleCommandOpcodeOutcome::unresolved_boundary,
            next_command_address_, 0, call_address, call_target, 0, 0, 0,
            boundary != 0 ? boundary : call_address};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandCallReturnPlan>
    observe_command_call_return(
        const DeuterosAmigaObservedTitleCommandCallReturn& observation) {
        if (!pending_command_call_ || command_halted_) return std::nullopt;
        if (observation.trace_sequence <= last_command_sequence_
            || observation.call_address != pending_command_call_->address
            || observation.call_target != pending_command_call_->target
            || observation.return_address != pending_command_call_->return_address) {
            throw std::runtime_error("Deuteros command call return does not match boundary");
        }
        last_command_sequence_ = observation.trace_sequence;
        next_command_address_ = observation.result_a4;
        pending_command_call_.reset();
        return DeuterosAmigaTitleCommandCallReturnPlan{observation,
            next_command_address_, command_interpreter_.opcode_read_address};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandOperandLocalPlan>
    observe_command_operand_byte(
        const DeuterosAmigaObservedTitleCommandOperandByte& observation) {
        if (!pending_command_opcode_
            || (*pending_command_opcode_ != 0x10 && *pending_command_opcode_ != 0x11)) {
            return std::nullopt;
        }
        const auto instruction = *pending_command_opcode_ == 0x10 ? 0x1fa2eU : 0x1fa3aU;
        if (observation.trace_sequence <= last_command_sequence_
            || observation.instruction_address != instruction
            || observation.source_address != next_command_address_) {
            throw std::runtime_error("Deuteros command operand does not match boundary");
        }
        const auto helper_index = *pending_command_opcode_ == 0x10 ? 0U : 1U;
        ++next_command_address_;
        last_command_sequence_ = observation.trace_sequence;
        pending_command_opcode_.reset();
        return DeuterosAmigaTitleCommandOperandLocalPlan{observation,
            command_interpreter_.operand_helper_destinations[helper_index],
            command_interpreter_.operand_table_base
                + static_cast<std::uint32_t>(observation.observed_value & 0x0fU) * 8U,
            next_command_address_, command_interpreter_.opcode_read_address};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandPointerCopyPlan>
    observe_command_pointer_long(
        const DeuterosAmigaObservedTitleCommandPointerLong& observation) {
        if (!pending_command_opcode_ || *pending_command_opcode_ != 0x07) {
            return std::nullopt;
        }
        if (observation.trace_sequence <= last_command_sequence_
            || observation.instruction_address != 0x1fa5e
            || observation.source_address != command_interpreter_.pointer_copy_source_address) {
            throw std::runtime_error("Deuteros command pointer read does not match boundary");
        }
        last_command_sequence_ = observation.trace_sequence;
        pending_command_opcode_.reset();
        return DeuterosAmigaTitleCommandPointerCopyPlan{observation,
            command_interpreter_.pointer_copy_destination_address,
            observation.observed_value, next_command_address_,
            command_interpreter_.opcode_read_address};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandEightPointerPlan>
    observe_command_eight_pointer(
        const DeuterosAmigaObservedTitleCommandEightPointer& observation) {
        if (!pending_command_opcode_ || *pending_command_opcode_ != 0x08
            || command_eight_pointer_) return std::nullopt;
        if (observation.trace_sequence <= last_command_sequence_
            || observation.instruction_address != 0x1fa70
            || observation.source_address != 0x1f978) {
            throw std::runtime_error("Deuteros command-eight pointer does not match boundary");
        }
        last_command_sequence_ = observation.trace_sequence;
        command_eight_pointer_ = observation;
        return DeuterosAmigaTitleCommandEightPointerPlan{observation, 0x1fa76, 0x1f98e};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandEightModePlan>
    observe_command_eight_mode(
        const DeuterosAmigaObservedTitleCommandEightMode& observation) {
        if (!command_eight_pointer_ || command_eight_mode_) return std::nullopt;
        if (observation.trace_sequence <= last_command_sequence_
            || observation.instruction_address != 0x1fa76
            || observation.source_address != 0x1f98e) {
            throw std::runtime_error("Deuteros command-eight mode does not match boundary");
        }
        last_command_sequence_ = observation.trace_sequence;
        command_eight_mode_ = observation;
        if (observation.observed_value != 0) {
            return DeuterosAmigaTitleCommandEightModePlan{observation, true,
                0x1fa80, 0x1f994, 0, {{0, 0}}, next_command_address_, 0, 0x1fa80};
        }
        const auto value = command_eight_pointer_->observed_value + 0x140U;
        pending_command_opcode_.reset();
        command_eight_pointer_.reset();
        command_eight_mode_.reset();
        return DeuterosAmigaTitleCommandEightModePlan{observation, false,
            0, 0, value, {{0x1f978, 0x1f974}}, next_command_address_,
            command_interpreter_.opcode_read_address,
            command_interpreter_.opcode_read_address};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandEightScalePlan>
    observe_command_eight_scale(
        const DeuterosAmigaObservedTitleCommandEightScale& observation) {
        if (!command_eight_pointer_ || !command_eight_mode_
            || command_eight_mode_->observed_value == 0) return std::nullopt;
        if (observation.trace_sequence <= last_command_sequence_
            || observation.instruction_address != 0x1fa80
            || observation.source_address != 0x1f994) {
            throw std::runtime_error("Deuteros command-eight scale does not match boundary");
        }
        const auto shifted = (observation.observed_value & 0xffff0000U)
            | ((observation.observed_value << 3U) & 0x0000ffffU);
        const auto value = command_eight_pointer_->observed_value + shifted;
        last_command_sequence_ = observation.trace_sequence;
        pending_command_opcode_.reset();
        command_eight_pointer_.reset();
        command_eight_mode_.reset();
        return DeuterosAmigaTitleCommandEightScalePlan{observation, shifted, value,
            {{0x1f978, 0x1f974}}, next_command_address_,
            command_interpreter_.opcode_read_address};
    }

private:
    bool armed_ = false;
    std::uint64_t preceding_sequence_ = 0;
    std::uint32_t graphics_library_base_ = 0;
    std::optional<DeuterosAmigaObservedGraphicsVectorReturn> observed_;
    std::optional<DeuterosAmigaObservedServiceWordRead> observed_word_;
    DeuterosAmigaTitlePostExecThirdServiceProfile third_service_;
    DeuterosAmigaTitlePostExecTailDispatchProfile tail_dispatch_;
    DeuterosAmigaTitlePostExecTailFirstCalleeProfile tail_first_callee_;
    DeuterosAmigaTitlePostExecTailSecondCalleeProfile tail_second_callee_;
    DeuterosAmigaTitlePostExecTailFourthCalleeProfile tail_fourth_callee_;
    DeuterosAmigaTitlePostExecFourthServiceProfile fourth_service_;
    DeuterosAmigaTitlePostExecTailReturnProfile tail_return_;
    DeuterosAmigaTitlePostExecLoadServiceProfile load_service_;
    DeuterosAmigaTitlePostLoadDispatchProfile load_dispatch_;
    DeuterosAmigaTitleCommandInterpreterProfile command_interpreter_;
    std::optional<DeuterosAmigaObservedGraphicsVectorReturn>
        observed_graphics_service_first_;
    std::optional<DeuterosAmigaObservedGraphicsVectorReturn>
        observed_graphics_service_second_;
    std::optional<DeuterosAmigaObservedGraphicsVectorReturn>
        observed_graphics_service_third_;
    std::optional<DeuterosAmigaObservedGraphicsVectorReturn>
        observed_tail_first_graphics_;
    std::optional<DeuterosAmigaObservedTailCopyWords> observed_tail_copy_words_;
    std::optional<DeuterosAmigaObservedTailSelectionWords>
        observed_tail_selection_words_;
    std::optional<DeuterosAmigaObservedGraphicsVectorReturn>
        observed_tail_second_graphics_;
    std::optional<DeuterosAmigaObservedTailSelectionWords>
        observed_tail_repeated_selection_words_;
    std::optional<DeuterosAmigaObservedGraphicsVectorReturn>
        observed_tail_repeated_graphics_;
    std::optional<DeuterosAmigaObservedGraphicsVectorReturn>
        observed_tail_repeated_wrapper_graphics_;
    std::optional<DeuterosAmigaObservedTailSourceTable> observed_tail_source_table_;
    std::optional<DeuterosAmigaObservedTailExecReturn> observed_tail_exec_return_;
    std::optional<DeuterosAmigaObservedLocalCallReturn> observed_load_service_return_;
    std::optional<DeuterosAmigaObservedLoadSelector> observed_load_selector_;
    std::optional<BoundedMemoryTransferSession> load_copy_transfer_;
    std::optional<DeuterosAmigaObservedLoadDispatchTableBase>
        observed_load_dispatch_table_base_;
    std::optional<DeuterosAmigaObservedLoadDispatchTableWord>
        observed_load_dispatch_table_word_;
    std::uint32_t next_command_address_ = 0;
    std::uint64_t last_command_sequence_ = 0;
    std::optional<std::uint8_t> pending_command_opcode_;
    std::optional<DeuterosAmigaObservedTitleCommandEightPointer>
        command_eight_pointer_;
    std::optional<DeuterosAmigaObservedTitleCommandEightMode> command_eight_mode_;
    bool command_halted_ = false;
    struct PendingCommandCall {
        std::uint32_t address;
        std::uint32_t target;
        std::uint32_t return_address;
    };
    std::optional<PendingCommandCall> pending_command_call_;
};

} // namespace eon
