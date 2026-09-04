#pragma once

#include "data/amiga_adf.hpp"
#include "data/deuteros_amiga_loader.hpp"
#include "data/sha256.hpp"
#include "engine/bounded_memory_transfer.hpp"

#include <algorithm>
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
    repeat_byte_boundary,
    fixed_table_byte_boundary,
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

struct DeuterosAmigaObservedTitleCommandTwoOperandMode {
    std::uint64_t trace_sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t source_address = 0;
    std::uint8_t observed_value = 0;
};

struct DeuterosAmigaTitleCommandTwoOperandModePlan {
    DeuterosAmigaObservedTitleCommandTwoOperandMode observation;
    std::array<std::uint32_t, 2> operand_source_addresses{};
    std::uint32_t first_operand_instruction_address = 0;
};

struct DeuterosAmigaObservedTitleCommandTwoOperands {
    std::uint64_t trace_sequence = 0;
    std::array<std::uint32_t, 2> instruction_addresses{};
    std::array<std::uint32_t, 2> source_addresses{};
    std::array<std::uint8_t, 2> observed_values{};
};

struct DeuterosAmigaTitleCommandTwoOperandsPlan {
    DeuterosAmigaObservedTitleCommandTwoOperands observation;
    std::uint32_t runtime_instruction_address = 0;
    std::uint32_t runtime_source_address = 0;
    std::uint32_t next_stream_address = 0;
};

struct DeuterosAmigaObservedTitleCommandTwoOperandRuntimeLong {
    std::uint64_t trace_sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t source_address = 0;
    std::uint32_t observed_value = 0;
};

struct DeuterosAmigaTitleCommandTwoOperandLocalPlan {
    DeuterosAmigaObservedTitleCommandTwoOperandRuntimeLong observation;
    std::uint32_t destination_address = 0;
    std::uint32_t destination_value = 0;
    std::uint32_t next_stream_address = 0;
    std::uint32_t next_opcode_read_address = 0;
};

struct DeuterosAmigaObservedTitleCommandRepeatBytes {
    std::uint64_t trace_sequence = 0;
    std::array<std::uint32_t, 2> instruction_addresses{};
    std::array<std::uint32_t, 2> source_addresses{};
    std::uint8_t count_value = 0;
    std::uint8_t repeated_value = 0;
};

struct DeuterosAmigaTitleCommandRepeatBytesPlan {
    DeuterosAmigaObservedTitleCommandRepeatBytes observation;
    std::uint16_t iteration_count = 0;
    std::uint32_t call_address = 0;
    std::uint32_t call_target = 0;
    std::uint32_t return_address = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaObservedTitleCommandRepeatCallReturn {
    std::uint64_t trace_sequence = 0;
    std::uint32_t call_address = 0;
    std::uint32_t call_target = 0;
    std::uint32_t return_address = 0;
    std::uint32_t result_a4 = 0;
    std::uint32_t result_d0 = 0;
    std::uint16_t result_sr = 0;
};

struct DeuterosAmigaTitleCommandRepeatCallReturnPlan {
    DeuterosAmigaObservedTitleCommandRepeatCallReturn observation;
    std::uint16_t completed_iterations = 0;
    std::uint16_t remaining_iterations = 0;
    std::uint32_t next_call_address = 0;
    std::uint32_t next_stream_address = 0;
    std::uint32_t next_opcode_read_address = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaObservedTitleCommandHighTableByte {
    std::uint64_t trace_sequence = 0;
    std::uint32_t instruction_address = 0;
    std::uint32_t source_address = 0;
    std::uint8_t observed_value = 0;
};

struct DeuterosAmigaTitleCommandHighTableBytePlan {
    DeuterosAmigaObservedTitleCommandHighTableByte observation;
    std::uint8_t byte_index = 0;
    std::uint32_t call_input_d0 = 0;
    std::uint32_t call_address = 0;
    std::uint32_t call_target = 0;
    std::uint32_t return_address = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaObservedTitleCommandHighCallReturn {
    std::uint64_t trace_sequence = 0;
    std::uint32_t call_address = 0;
    std::uint32_t call_target = 0;
    std::uint32_t return_address = 0;
    std::uint32_t result_a4 = 0;
    std::uint32_t result_d0 = 0;
    std::uint16_t result_sr = 0;
};

struct DeuterosAmigaTitleCommandHighCallReturnPlan {
    DeuterosAmigaObservedTitleCommandHighCallReturn observation;
    std::uint8_t completed_calls = 0;
    std::uint32_t next_table_read_instruction_address = 0;
    std::uint32_t next_table_source_address = 0;
    std::uint32_t next_stream_address = 0;
    std::uint32_t next_opcode_read_address = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaObservedTitleCommandPlanarWrite {
    std::uint64_t trace_sequence = 0;
    std::array<std::uint32_t, 2> mode_instruction_addresses{};
    std::array<std::uint32_t, 2> mode_source_addresses{};
    std::array<std::uint8_t, 2> observed_mode_values{};
    std::array<std::uint32_t, 5> pointer_source_addresses{};
    std::array<std::uint32_t, 5> observed_pointer_values{};
    std::array<std::uint32_t, 8> glyph_source_addresses{};
    std::array<std::uint8_t, 8> observed_glyph_values{};
    std::array<std::uint32_t, 32> first_word_source_addresses{};
    std::array<std::uint16_t, 32> observed_first_words{};
    std::array<std::uint32_t, 32> second_word_source_addresses{};
    std::array<std::uint16_t, 32> observed_second_words{};
};

struct DeuterosAmigaTitleCommandPlanarWritePlan {
    DeuterosAmigaObservedTitleCommandPlanarWrite observation;
    std::array<std::uint32_t, 32> destination_addresses{};
    std::array<std::uint8_t, 32> destination_values{};
    std::uint32_t destination_pointer_cell_address = 0;
    std::uint32_t destination_pointer_value = 0;
    std::uint32_t routine_return_address = 0;
    std::uint32_t command_return_address = 0;
    std::uint32_t next_stream_address = 0;
    std::uint32_t next_opcode_read_address = 0;
};

enum class DeuterosAmigaTitlePlanarVariant {
    positive_clear,
    zero_set,
    positive_set,
};

// Exact runtime reads for the three remaining non-negative `$1fbe6`
// bit-combine routes. The vectors are route-sized because the original does
// not read unused stride/source cells. `observed_base_values` contains a full
// word on zero-mode routes and one byte (0..255) on positive-mode routes.
struct DeuterosAmigaObservedTitleCommandPlanarVariantWrite {
    std::uint64_t trace_sequence = 0;
    std::array<std::uint32_t, 2> mode_instruction_addresses{};
    std::array<std::uint32_t, 2> mode_source_addresses{};
    std::array<std::uint8_t, 2> observed_mode_values{};
    std::vector<std::uint32_t> pointer_source_addresses;
    std::vector<std::uint32_t> observed_pointer_values;
    std::array<std::uint32_t, 8> glyph_source_addresses{};
    std::array<std::uint8_t, 8> observed_glyph_values{};
    std::array<std::uint32_t, 32> base_source_addresses{};
    std::array<std::uint16_t, 32> observed_base_values{};
    std::array<std::uint32_t, 32> blend_word_source_addresses{};
    std::array<std::uint16_t, 32> observed_blend_words{};
};

struct DeuterosAmigaTitleCommandPlanarVariantWritePlan {
    DeuterosAmigaObservedTitleCommandPlanarVariantWrite observation;
    DeuterosAmigaTitlePlanarVariant variant =
        DeuterosAmigaTitlePlanarVariant::positive_clear;
    std::array<std::uint32_t, 32> destination_addresses{};
    std::array<std::uint8_t, 32> destination_values{};
    std::uint32_t destination_pointer_cell_address = 0;
    std::uint32_t destination_pointer_value = 0;
        // The positive-clear route uses literal advances $28/$1f40.  The
        // sibling routes replace these with their ordered pointer-cell
        // observations below.
        std::uint32_t row_stride = 0x28;
        std::uint32_t plane_stride = 0x1f40;
    bool recovered_title_surface_layout = false;
    std::uint32_t routine_return_address = 0;
    std::uint32_t command_return_address = 0;
    std::uint32_t next_stream_address = 0;
    std::uint32_t next_opcode_read_address = 0;
};

// Completion evidence for the signed-negative `$1fbe6` dispatch. The nested
// service is deliberately retained as an opaque call boundary; only its
// exact call ABI and return are admitted here.
struct DeuterosAmigaObservedTitleCommandNegativeService {
    std::uint64_t trace_sequence = 0;
    std::uint32_t mode_instruction_address = 0;
    std::uint32_t mode_source_address = 0;
    std::uint8_t observed_mode_value = 0;
    bool service_called = false;
    std::uint32_t service_call_address = 0;
    std::uint32_t service_target = 0;
    std::uint32_t service_return_address = 0;
    std::uint32_t service_d0 = 0;
    std::uint32_t service_d1 = 0;
};

struct DeuterosAmigaTitleCommandNegativeServicePlan {
    DeuterosAmigaObservedTitleCommandNegativeService observation;
    bool service_suppressed = false;
    std::uint32_t delay_iterations = 0;
    std::uint32_t routine_return_address = 0;
    std::uint32_t command_return_address = 0;
    std::uint32_t next_stream_address = 0;
    std::uint32_t next_opcode_read_address = 0;
};

struct DeuterosAmigaObservedTitlePostCommandPointerRoute {
    std::uint64_t trace_sequence = 0;
    std::uint32_t preceding_call_address = 0;
    std::uint32_t preceding_call_target = 0;
    std::uint32_t preceding_return_address = 0;
    std::uint32_t pointer_call_address = 0;
    std::uint32_t pointer_call_target = 0;
    std::uint32_t flag_instruction_address = 0;
    std::uint32_t flag_source_address = 0;
    std::uint8_t observed_flag_value = 0;
};

struct DeuterosAmigaTitlePostCommandPointerRoutePlan {
    DeuterosAmigaObservedTitlePostCommandPointerRoute observation;
    std::array<std::uint32_t, 3> destination_addresses{};
    std::array<std::uint32_t, 3> destination_values{};
    std::array<MemoryTransferElementWidth, 3> destination_widths{};
    std::uint8_t effect_count = 0;
    bool pointer_call_completed = false;
    std::uint32_t nested_boundary_address = 0;
    std::uint32_t caller_continuation_address = 0;
};

struct DeuterosAmigaTitlePostCommandGraphicsReturnPlan {
    DeuterosAmigaObservedGraphicsVectorReturn observation;
    std::uint32_t cleared_byte_address = 0;
    std::uint8_t cleared_byte_value = 0;
    std::uint32_t local_return_address = 0;
    std::uint32_t pointer_caller_return_address = 0;
    std::uint16_t next_d0_literal = 0;
    std::uint32_t next_call_address = 0;
    std::uint32_t next_call_target = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaTitlePostCommandFirstDispatchPlan {
    std::uint16_t caller_d0 = 0;
    std::uint32_t outer_entry_address = 0;
    std::uint16_t outer_descriptor_word = 0;
    std::uint16_t nested_index = 0;
    std::uint32_t nested_pointer_cell_address = 0;
    std::uint32_t nested_pointer_offset = 0;
    std::uint32_t compressed_source_address = 0;
    std::uint32_t destination_address = 0;
    std::uint32_t destination_wrap_address = 0;
    std::uint32_t destination_wrap_subtract = 0;
    std::uint32_t row_advance = 0;
    std::uint32_t first_source_read_address = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaObservedTitleFirstDispatchHeader {
    std::uint64_t trace_sequence = 0;
    std::array<std::uint32_t,2> instruction_addresses{};
    std::array<std::uint32_t,2> source_addresses{};
    std::array<std::uint16_t,2> observed_words{};
};

struct DeuterosAmigaTitleFirstDispatchHeaderPlan {
    DeuterosAmigaObservedTitleFirstDispatchHeader observation;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint16_t width_counter = 0;
    std::array<std::uint32_t,2> destination_addresses{};
    std::array<std::uint16_t,2> destination_values{};
    bool low_height_decoder = false;
    std::uint32_t next_source_address = 0;
    std::uint32_t next_instruction_address = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaTitleFirstDispatchPacketPlan {
    std::uint8_t control = 0;
    std::array<std::uint8_t,2> literal_bytes{};
    std::array<std::uint32_t,4> destination_addresses{};
    std::array<std::uint32_t,4> destination_values{};
    std::array<MemoryTransferElementWidth,4> destination_widths{};
    std::uint32_t next_source_address = 0;
    std::uint32_t next_destination_address = 0;
    std::uint16_t remaining_group_columns = 0;
    std::uint16_t remaining_rows = 0;
    std::uint16_t remaining_planes = 0;
    std::uint32_t next_instruction_address = 0;
};

struct DeuterosAmigaTitleFirstDispatchDecodePlan {
    std::vector<std::uint32_t> destination_addresses;
    std::vector<std::uint8_t> destination_values;
    std::uint32_t source_begin = 0;
    std::uint32_t source_end = 0;
    std::uint32_t packet_count = 0;
    std::array<std::uint32_t,4> packet_family_counts{};
    std::uint32_t decoded_pair_count = 0;
    std::uint32_t routine_return_address = 0;
    std::string source_sha256;
};

struct DeuterosAmigaTitleFirstDispatchCallerTailPlan {
    std::uint32_t caller_resume_address = 0;
    std::uint32_t destination_base = 0;
    std::uint32_t mask_base = 0;
    std::uint16_t outer_iterations = 0;
    std::uint16_t words_per_iteration = 0;
    std::array<std::uint32_t,4> mask_source_addresses{};
    std::array<std::uint16_t,4> mask_source_words{};
    std::uint16_t combined_mask = 0;
    std::uint32_t first_destination_read_address = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaObservedTitleFirstDispatchDestinationWords {
    std::uint64_t trace_sequence = 0;
    std::uint32_t first_instruction_address = 0;
    std::vector<std::uint32_t> source_addresses;
    std::vector<std::uint16_t> observed_words;
    std::vector<std::uint32_t> mask_source_addresses;
    std::vector<std::uint16_t> observed_mask_words;
};

struct DeuterosAmigaTitleFirstDispatchMergePlan {
    DeuterosAmigaObservedTitleFirstDispatchDestinationWords observation;
    std::vector<std::uint32_t> destination_addresses;
    std::vector<std::uint16_t> destination_values;
    std::uint32_t executed_word_writes = 0;
    std::uint32_t unique_word_writes = 0;
    std::uint32_t routine_return_address = 0;
    std::uint32_t caller_return_address = 0;
    std::uint16_t next_d0_literal = 0;
    std::uint32_t next_call_address = 0;
    std::uint32_t next_call_target = 0;
};

struct DeuterosAmigaTitlePostCommandSecondDispatchPlan {
    std::uint16_t caller_d0 = 0;
    std::uint32_t outer_entry_address = 0;
    std::uint32_t outer_table_base = 0;
    std::uint16_t outer_descriptor_word = 0;
    std::uint32_t fixed_descriptor_address = 0;
    std::uint16_t nested_index = 0;
    std::uint32_t nested_pointer_cell_address = 0;
    std::uint32_t nested_pointer_offset = 0;
    std::uint32_t compressed_source_address = 0;
    std::uint32_t destination_address = 0;
    std::uint32_t destination_wrap_address = 0;
    std::uint32_t row_advance = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaTitleSecondDispatchDecodePlan {
    std::vector<std::uint32_t> destination_addresses;
    std::vector<std::uint8_t> destination_values;
    std::uint32_t source_begin = 0;
    std::uint32_t source_end = 0;
    std::uint32_t packet_count = 0;
    std::array<std::uint32_t,4> packet_family_counts{};
    std::uint32_t decoded_pair_count = 0;
    std::uint32_t caller_resume_address = 0;
    std::string source_sha256;
};

struct DeuterosAmigaObservedTitleSecondDispatchDestinationWords {
    std::uint64_t trace_sequence = 0;
    std::uint32_t first_instruction_address = 0;
    std::array<std::uint32_t,64> source_addresses{};
    std::array<std::uint16_t,64> observed_words{};
};

struct DeuterosAmigaTitleSecondDispatchMergePlan {
    DeuterosAmigaObservedTitleSecondDispatchDestinationWords observation;
    std::vector<std::uint32_t> destination_addresses;
    std::vector<std::uint16_t> destination_values;
    std::uint32_t executed_word_writes = 0;
    std::uint32_t unique_word_writes = 0;
    std::uint32_t routine_return_address = 0;
    std::uint32_t caller_return_address = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaTitlePostCommandServiceRoutePrefixPlan {
    std::array<std::uint32_t,7> destination_addresses{};
    std::array<std::uint32_t,7> destination_values{};
    std::array<MemoryTransferElementWidth,7> destination_widths{};
    std::uint16_t selected_table_word = 0;
    std::uint32_t caller_address = 0, callee_address = 0;
    std::uint32_t external_call_address = 0, external_call_target = 0;
    std::uint16_t external_call_d0 = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaTitlePostCommandServiceFirstReturnPlan {
    DeuterosAmigaObservedLocalCallReturn observation;
    std::uint16_t restored_d0 = 0;
    std::uint16_t adjusted_d0 = 0;
    std::uint32_t next_call_address = 0;
    std::uint32_t next_call_target = 0;
    std::uint32_t stop_before_address = 0;
};

struct DeuterosAmigaTitlePostCommandServiceSecondReturnPlan {
    DeuterosAmigaObservedLocalCallReturn observation;
    std::uint16_t restored_d0 = 0;
    std::uint16_t scaled_index = 0;
    std::uint32_t table_address = 0;
    std::uint32_t selected_value = 0;
    std::uint32_t destination_address = 0;
    std::uint32_t next_call_address = 0;
    std::uint32_t next_call_target = 0;
    std::uint32_t stop_before_address = 0;
};
struct DeuterosAmigaTitlePostCommandServiceThirdReturnPlan {
    DeuterosAmigaObservedLocalCallReturn observation;
    std::uint32_t local_call_address=0,local_call_target=0;
    std::uint32_t pointer_cell_address=0,pointer_value=0;
    std::uint32_t stop_before_address=0;
};
struct DeuterosAmigaObservedTitlePostCommandNestedWords {
    std::uint64_t trace_sequence=0;
    std::array<std::uint32_t,2> instruction_addresses{};
    std::array<std::uint32_t,2> source_addresses{};
    std::array<std::uint16_t,2> observed_words{};
};
struct DeuterosAmigaTitlePostCommandNestedWordsPlan {
    DeuterosAmigaObservedTitlePostCommandNestedWords observation;
    std::uint16_t shifted_d7=0,decremented_d5=0;
    bool carry_branch=false,writes_counter=false;
    std::uint32_t counter_destination=0;
    std::uint16_t counter_value=0,call_d0=0,call_d1=0;
    std::uint32_t call_address=0,call_target=0,stop_before_address=0;
};
struct DeuterosAmigaTitlePostCommandNestedCallReturnPlan {
    DeuterosAmigaObservedLocalCallReturn observation;
    std::uint16_t restored_d7=0;
    std::uint16_t loop_d6=0;
    std::uint32_t loop_boundary_address=0;
};
struct DeuterosAmigaTitlePostCommandNestedLoopAdvancePlan {
    std::uint16_t iteration=0,shifted_d7=0,decremented_d5=0,loop_d6=0;
    bool carry_branch=false,writes_counter=false,completed=false;
    std::uint32_t counter_destination=0;std::uint16_t counter_value=0;
    std::uint16_t call_d0=0,call_d1=0;
    std::uint32_t call_address=0,call_target=0,stop_before_address=0;
};
struct DeuterosAmigaTitlePostCommandContinuationReturnPlan {
    DeuterosAmigaObservedLocalCallReturn observation;
    std::uint32_t pointer_cell_address=0,pointer_cell_value=0;
    std::uint32_t first_pointer_read_address=0,stop_before_address=0;
};
struct DeuterosAmigaObservedTitlePostCommandPointerChain {
    std::uint64_t trace_sequence=0;
    std::array<std::uint32_t,3> instruction_addresses{};
    std::array<std::uint32_t,3> source_addresses{};
    std::array<std::uint32_t,2> observed_pointer_values{};
    std::uint16_t observed_word=0;
};
struct DeuterosAmigaTitlePostCommandPointerChainPlan {
    DeuterosAmigaObservedTitlePostCommandPointerChain observation;
    std::uint16_t descriptor_word=0;
    std::uint32_t descriptor_destination=0;
    std::uint16_t next_d0=0;
    std::uint32_t next_call_address=0,next_call_target=0,stop_before_address=0;
};
struct DeuterosAmigaObservedTitlePostCommandDispatchDestination {
    std::uint64_t trace_sequence=0;
    std::uint32_t instruction_address=0,source_address=0,observed_pointer=0;
};
struct DeuterosAmigaTitlePostCommandDispatchSetupPlan {
    DeuterosAmigaObservedTitlePostCommandDispatchDestination observation;
    std::uint16_t selector=0,descriptor=0;
    std::uint32_t outer_entry_address=0,resource_pointer_cell=0,resource_offset=0,source_header=0;
    std::uint16_t x_word=0,y_word=0,wrap_word=0,row_stride=0;
    std::uint32_t destination=0,first_source_read=0,stop_before_address=0;
};
struct DeuterosAmigaTitlePostCommandSelectedStreamPlan {
    std::uint16_t descriptor=0,width=0,height=0;
    std::uint32_t source_header=0,payload_start=0,payload_end=0;
    std::vector<std::uint32_t> destination_addresses;
    std::vector<std::uint8_t> destination_values;
    std::uint32_t packet_count=0;
    std::array<std::uint32_t,4> packet_family_counts{};
    std::uint32_t decoded_pairs=0,return_address=0,caller_continuation=0;
    std::uint16_t caller_d5=0,caller_d6=0;
    std::uint32_t stop_before_address=0;
    std::string payload_sha256;
};
struct DeuterosAmigaTitlePostCommandDescriptorCallPlan {
    DeuterosAmigaObservedLocalCallReturn observation;
    std::uint16_t iteration=0,remaining_d6=0,shifted_d5=0;
};
struct DeuterosAmigaTitlePostCommandDescriptorLoopPlan {
    bool completed=false;
    std::uint16_t iteration=0,remaining_d6=0,shifted_d5=0,d0=0,d1=0;
    std::uint32_t call_address=0,call_target=0,descriptor_destination=0;
    std::uint16_t descriptor_value=0;
    std::uint32_t stop_before_address=0;
};
struct DeuterosAmigaObservedTitlePostCommandDescriptorByte {
    std::uint64_t trace_sequence=0;
    std::uint32_t instruction_address=0,source_address=0;
    std::uint8_t observed_value=0;
};
struct DeuterosAmigaTitlePostCommandDescriptorBytePlan {
    DeuterosAmigaObservedTitlePostCommandDescriptorByte observation;
    std::uint16_t base_descriptor=0,result_descriptor=0,selector=0;
    std::uint32_t descriptor_destination=0,call_address=0,call_target=0,stop_before_address=0;
};
struct DeuterosAmigaObservedTitlePostCommandAdjustedDispatchDestination {
    std::uint64_t trace_sequence=0;
    std::uint32_t instruction_address=0,source_address=0,observed_pointer=0;
};
struct DeuterosAmigaTitlePostCommandAdjustedDispatchPlan {
    DeuterosAmigaObservedTitlePostCommandAdjustedDispatchDestination observation;
    std::uint16_t descriptor=0,width=0,height=0,x_word=0,y_word=0,row_advance=0;
    std::uint32_t descriptor_address=0,source_header=0,payload_start=0,payload_end=0;
    std::vector<std::uint32_t> destination_addresses;
    std::vector<std::uint8_t> destination_values;
    std::uint32_t packet_count=0;
    std::array<std::uint32_t,4> packet_family_counts{};
    std::uint32_t return_address=0,stop_before_address=0;
    std::string descriptor_suffix_sha256,header_sha256,payload_sha256;
};
struct DeuterosAmigaObservedTitlePostAdjustedCallerPointer {
    std::uint64_t trace_sequence=0;
    std::uint32_t instruction_address=0,source_address=0,observed_pointer=0;
};
struct DeuterosAmigaTitlePostAdjustedCallerPointerPlan {
    DeuterosAmigaObservedTitlePostAdjustedCallerPointer observation;
    bool pointer_is_zero=false;
    std::uint32_t branch_address=0,branch_target=0,next_instruction=0;
    std::string prefix_sha256;
};
struct DeuterosAmigaObservedTitlePostAdjustedObjectGate {
    std::uint64_t trace_sequence=0;
    std::uint32_t instruction_address=0;
    std::uint32_t first_source_address=0; std::uint8_t first_value=0;
    std::uint32_t second_source_address=0; std::uint8_t second_value=0;
};
struct DeuterosAmigaTitlePostAdjustedObjectGatePlan {
    DeuterosAmigaObservedTitlePostAdjustedObjectGate observation;
    std::uint32_t table_address=0;
    std::uint16_t first_d0=0,first_d1=0;
    std::uint32_t call_address=0,call_target=0,return_address=0;
    std::string path_sha256,table_prefix_sha256;
};
struct DeuterosAmigaTitlePostAdjustedFirstHelperReturnPlan {
    DeuterosAmigaObservedLocalCallReturn observation;
    std::uint32_t table_address=0;
    std::uint16_t second_d0=0,second_d1=0;
    std::uint32_t next_call_address=0,next_call_target=0,next_return_address=0;
    std::string continuation_sha256;
};
struct DeuterosAmigaTitlePostAdjustedSecondHelperReturnPlan {
    DeuterosAmigaObservedLocalCallReturn observation;
    std::uint32_t local_rts_address=0,stop_before_address=0;
    std::string call_and_rts_sha256;
};
struct DeuterosAmigaObservedTitlePostAdjustedRtsFrame {
    std::uint64_t trace_sequence=0;
    std::uint32_t instruction_address=0,frame_address=0,return_address=0;
};
struct DeuterosAmigaTitlePostAdjustedRtsFramePlan {
    DeuterosAmigaObservedTitlePostAdjustedRtsFrame observation;
    std::uint32_t caller_call_address=0,caller_call_target=0,caller_return_address=0;
    std::string caller_prefix_sha256;
};
struct DeuterosAmigaTitlePostAdjustedCallerIndirectPlan {
    std::uint32_t entry_address=0,pointer_literal=0,call_address=0,call_target=0,return_address=0;
    std::string prefix_sha256;
};
struct DeuterosAmigaObservedTitlePostAdjustedIndirectReturn {
    std::uint64_t trace_sequence=0;
    std::uint32_t call_address=0,call_target=0,return_address=0;
    std::uint32_t source_address=0,source_long=0,opaque_d0=0;
    std::uint16_t opaque_sr=0;
};
struct DeuterosAmigaTitlePostAdjustedIndirectReturnPlan {
    DeuterosAmigaObservedTitlePostAdjustedIndirectReturn observation;
    std::uint32_t shifted_source=0,destination_address=0;
    std::uint16_t destination_word=0;
    std::uint32_t next_call_address=0,next_call_target=0,next_return_address=0;
    std::string continuation_sha256;
};
struct DeuterosAmigaObservedTitlePostAdjusted37180Return { std::uint64_t trace_sequence=0; std::uint32_t call_address=0,call_target=0,return_address=0,source_address=0,source_long=0,mode_address=0; std::uint16_t mode_word=0,opaque_sr=0; std::uint32_t opaque_d0=0; };
struct DeuterosAmigaTitlePostAdjusted37180ReturnPlan { DeuterosAmigaObservedTitlePostAdjusted37180Return observation; std::uint32_t destination_address=0,destination_long=0,branch_address=0,selected_call_address=0,selected_call_target=0,selected_return_address=0; bool mode_is_five=false; std::string continuation_sha256; };
struct DeuterosAmigaTitlePostAdjustedModeReturnPlan { DeuterosAmigaObservedLocalCallReturn observation; std::uint32_t join_address=0,next_call_address=0,next_call_target=0,next_return_address=0; bool branch_to_join=false; };

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
        post_command_continuation_ =
            parse_deuteros_amiga_title_post_exec_tail_return_continuation_profile(disk, plan);
        post_command_pointer_route_ =
            parse_deuteros_amiga_title_post_exec_pointer_route_profile(disk, plan);
        post_command_service_route_ =
            parse_deuteros_amiga_title_post_exec_service_route_profile(disk, plan);
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
        if (to_hex(sha256(at(0x1fca6, 100)))
                != "806ad8916bbcdd2b6e01806f56cde2905cd8f9d2af63c877c2242371e2659141"
            || to_hex(sha256(at(0x1fd0a, 104)))
                != "13e86b16e732da32a9cbdcd1b0b387c042b6a6bedd9b46cb5664d0a4a121318a"
            || to_hex(sha256(at(0x1fd7a, 102)))
                != "bd6bfbd42d3b6471a8166e14228fe177f5afe3a7ff3c8372cf291b0c37c44f82") {
            throw std::runtime_error("Unsupported Deuteros planar variant routines");
        }
        if (to_hex(sha256(at(0x1fbe6, 60)))
                != "cbddd93eb43c498079e7e2175f8f7d6178c357aa6b5241631e717f9037cff414") {
            throw std::runtime_error("Unsupported Deuteros negative title service route");
        }
        if (to_hex(sha256(at(0x416d0,14)))
                != "dd1ef0e747524f3b48c3cca3e81f9601b7f19587cfdb841064e4072324e8c3d7"
            || to_hex(sha256(at(0x4128e,12)))
                != "f1e6aa704e116355d7475b93167fb52fb8b9ae8310db046c230e74cf83aa5da5"
            || to_hex(sha256(at(0x422ae,4)))
                != "08512d1c9cc7d8b5d700c917e2b750cddc6816bd7946ef45cc44c486ec2919ff") {
            throw std::runtime_error("Unsupported Deuteros first post-command dispatch tables");
        }
        if (to_hex(sha256(at(0x78aa8,7)))
                != "96c277c906c4179741d1de3383fa161bc415b93771dad9b8377f9d9190491ce9") {
            throw std::runtime_error("Unsupported Deuteros first post-command compressed prefix");
        }
        const auto compressed=at(0x78aac,453);
        if (to_hex(sha256(compressed)) != "684e83c90a64bd8829ad01f3b7615b5d686d4d054e25c7483d29b7b50046fb1d")
            throw std::runtime_error("Unsupported Deuteros first compressed stream");
        std::size_t source=0;
        std::uint32_t plane_base=0x256dc,row_base=plane_base,destination=row_base;
        std::uint32_t groups=17,rows=16,planes=4;
        auto emit=[&](std::uint8_t first,std::uint8_t second){
            if (planes == 0 || destination > std::numeric_limits<std::uint32_t>::max() - 1U)
                throw std::runtime_error("Deuteros compressed output exceeds boundary");
            first_dispatch_decode_addresses_.push_back(destination);
            first_dispatch_decode_values_.push_back(first);
            first_dispatch_decode_addresses_.push_back(destination+1U);
            first_dispatch_decode_values_.push_back(second);
            destination+=2U;
            if (--groups == 0) {
                if (--rows == 0) {
                    if (--planes == 0) return;
                    plane_base+=0x1a40U; row_base=plane_base; rows=16;
                } else {
                    row_base += 0x38U;
                }
                destination=row_base;groups=17;
            }
        };
        while (planes != 0) {
            if (source >= compressed.size())
                throw std::runtime_error("Truncated Deuteros compressed stream");
            const auto control=compressed[source++];const auto family=control>>6U;
            ++first_dispatch_family_counts_[family];++first_dispatch_packet_count_;
            std::uint32_t count=control&0x3fU;
            std::uint8_t first=0,second=0;
            if (family == 0) {
                count = control == 0 ? 256U : control;
                for (std::uint32_t i=0;i<count&&planes;++i) {
                    if (source + 2U > compressed.size())
                        throw std::runtime_error("Truncated Deuteros literal packet");
                    first=compressed[source++]; second=compressed[source++]; emit(first,second);
                }
                continue;
            }
            if (family == 1) {
                count=count==0?256U:count;
                if (source >= compressed.size())
                    throw std::runtime_error("Truncated Deuteros fill packet");
                first=second=compressed[source++];
            } else {
                if (family == 2) {
                    if (source >= compressed.size())
                        throw std::runtime_error("Truncated Deuteros extended count");
                    count=(count<<8U)|compressed[source++]; if(count==0)count=65536U;
                } else if(count==0) count=256U;
                if (source + 2U > compressed.size())
                    throw std::runtime_error("Truncated Deuteros swapped packet");
                second=compressed[source++]; first=compressed[source++];
            }
            for(std::uint32_t i=0;i<count&&planes;++i)emit(first,second);
        }
        if(source!=compressed.size()||first_dispatch_decode_values_.size()!=2176U)
            throw std::runtime_error("Deuteros compressed stream completion mismatch");

        if (to_hex(sha256(at(0x416de,14)))
                != "32638511c8219c7a132dcc35a59b40c2a1fa028e48c0d4c6891a383d08e79162"
            || to_hex(sha256(at(0x422b2,4)))
                != "acf20393858e975c2b8e9674523830117781a9c0b12ae6b40469418a95be9cf1"
            || to_hex(sha256(at(0x78c72,7)))
                != "4a7c8a66510bb9364ca1247c92f42dee95835bf0cabce6d474be90f0a09f4a16")
            throw std::runtime_error("Unsupported Deuteros second post-command dispatch tables");
        const auto second_compressed=at(0x78c76,229);
        if(to_hex(sha256(second_compressed))
                != "dabcb6ee4feb0022f3232bcab1ffccb6657448e8602c39ef248da996e57a5666")
            throw std::runtime_error("Unsupported Deuteros second compressed stream");
        source=0;plane_base=0x256dc;row_base=plane_base;destination=row_base;
        groups=5;rows=16;planes=4;
        auto emit_second=[&](std::uint8_t first,std::uint8_t second){
            second_dispatch_decode_addresses_.push_back(destination);
            second_dispatch_decode_values_.push_back(first);
            second_dispatch_decode_addresses_.push_back(destination+1U);
            second_dispatch_decode_values_.push_back(second);
            destination+=2U;
            if(--groups==0){
                if(--rows==0){if(--planes==0)return;plane_base+=0x1a40U;row_base=plane_base;rows=16;}
                else row_base+=0x38U;
                destination=row_base;groups=5;
            }
        };
        while(planes!=0){
            if(source>=second_compressed.size())throw std::runtime_error("Truncated Deuteros second compressed stream");
            const auto control=second_compressed[source++];const auto family=control>>6U;
            ++second_dispatch_family_counts_[family];++second_dispatch_packet_count_;
            std::uint32_t count=control&0x3fU;std::uint8_t first=0,second=0;
            if(family==0){count=control==0?256U:control;for(std::uint32_t i=0;i<count&&planes;++i){
                if(source+2U>second_compressed.size())throw std::runtime_error("Truncated Deuteros second literal packet");
                first=second_compressed[source++];second=second_compressed[source++];emit_second(first,second);}continue;}
            if(family==1){count=count==0?256U:count;if(source>=second_compressed.size())throw std::runtime_error("Truncated Deuteros second fill packet");first=second=second_compressed[source++];}
            else {if(family==2){if(source>=second_compressed.size())throw std::runtime_error("Truncated Deuteros second extended count");count=(count<<8U)|second_compressed[source++];if(count==0)count=65536U;}
                else if(count==0)count=256U;
                if(source+2U>second_compressed.size())throw std::runtime_error("Truncated Deuteros second swapped packet");
                second=second_compressed[source++];first=second_compressed[source++];}
            for(std::uint32_t i=0;i<count&&planes;++i)emit_second(first,second);
        }
        if(source!=second_compressed.size()||second_dispatch_decode_values_.size()!=640U
            ||second_dispatch_family_counts_!=std::array<std::uint32_t,4>{{33,32,1,0}})
            throw std::runtime_error("Deuteros second compressed stream completion mismatch");

        const auto decode_selected=[&](std::uint16_t descriptor,std::uint32_t header_address,
            std::size_t payload_length,std::uint32_t groups,std::string_view header_hash,
            std::string_view payload_hash,std::vector<std::uint32_t>& addresses,
            std::vector<std::uint8_t>& values,std::uint32_t& packets,
            std::array<std::uint32_t,4>& families){
            if(to_hex(sha256(at(header_address,4)))!=header_hash)
                throw std::runtime_error("Unsupported Deuteros selected dispatch header");
            const auto payload=at(header_address+4U,payload_length);
            if(to_hex(sha256(payload))!=payload_hash)
                throw std::runtime_error("Unsupported Deuteros selected dispatch payload");
            std::size_t cursor=0;std::uint32_t plane=0,row=0,group=0;
            const auto pair_total=groups*16U*4U;
            auto emit_pair=[&](std::uint8_t first,std::uint8_t second){
                const auto address=plane*0x1a40U+row*0x28U+group*2U;
                addresses.push_back(address);values.push_back(first);
                addresses.push_back(address+1U);values.push_back(second);
                if(++group==groups){group=0;if(++row==16U){row=0;++plane;}}
            };
            while(values.size()/2U<pair_total){
                if(cursor>=payload.size())throw std::runtime_error("Truncated Deuteros selected dispatch payload");
                const auto control=payload[cursor++];const auto family=control>>6U;
                ++families[family];++packets;std::uint32_t count=control&0x3fU;
                if(family==0){count=control==0?256U:count;for(std::uint32_t i=0;i<count&&values.size()/2U<pair_total;++i){
                    if(cursor+2U>payload.size())throw std::runtime_error("Truncated Deuteros selected literal packet");
                    emit_pair(payload[cursor],payload[cursor+1U]);cursor+=2U;}continue;}
                std::uint8_t first=0,second=0;
                if(family==1){count=count==0?256U:count;if(cursor>=payload.size())throw std::runtime_error("Truncated Deuteros selected fill packet");first=second=payload[cursor++];}
                else {
                    if(family==2) {
                        if(cursor>=payload.size())
                            throw std::runtime_error("Truncated Deuteros selected extended count");
                        count=(count<<8U)|payload[cursor++];
                        if(count==0)count=65536U;
                    } else if(count==0) {
                        count=256U;
                    }
                    if(cursor+2U>payload.size())
                        throw std::runtime_error("Truncated Deuteros selected swapped packet");
                    second=payload[cursor++];
                    first=payload[cursor++];
                }
                for(std::uint32_t i=0;i<count&&values.size()/2U<pair_total;++i)emit_pair(first,second);
            }
            if(cursor!=payload.size()||values.size()!=pair_total*2U)
                throw std::runtime_error("Deuteros selected dispatch completion mismatch");
            (void)descriptor;
        };
        decode_selected(0x00b0,0x74576,1320,12,
            "5b5f874b3e3dcaf3ab874493a0483ce5be66dbf1ce24f1a3b850594eaa93d61d",
            "67251004cede98024d69fff3b1bac02f7df956aca5422086f00a88825ad1366c",
            selected_b0_addresses_,selected_b0_values_,selected_b0_packet_count_,selected_b0_family_counts_);
        decode_selected(0x00bd,0x76e24,4,3,
            "3c388d6dfefe53e270955f50b2cfe452bb072e0c0025c2bfef2cb4518d97f054",
            "8dca78516efa8b24c5a195cd4427fe196b4e15759c00882aa1a229ae99edd173",
            selected_bd_addresses_,selected_bd_values_,selected_bd_packet_count_,selected_bd_family_counts_);

        constexpr std::uint32_t adjusted_header=0x770a0;
        const auto adjusted_descriptor_suffix=at(0x416b6,12);
        const auto adjusted_header_bytes=at(adjusted_header,4);
        if(to_hex(sha256(adjusted_descriptor_suffix))!="d36e52f85876496cb0123b9c5d4c0ad2bce2b7ecfad168c1c1fe247507060c2d"
            ||to_hex(sha256(adjusted_header_bytes))!="fbaaa7db8117a83718be8cea03749e455893d2e0feb5e72c74d1158749bc7095")
            throw std::runtime_error("Unsupported Deuteros adjusted descriptor profile");
        const auto adjusted_payload=at(adjusted_header+4U,6659);
        if(to_hex(sha256(adjusted_payload))!="8b535aadc3aaa48055ffaf3ce03339455ebcee3951e737ddedfb62d8b690b840")
            throw std::runtime_error("Unsupported Deuteros adjusted descriptor payload");
        std::size_t adjusted_cursor=0;
        const auto adjusted_pairs=68U*168U;
        const auto adjusted_emit=[&](const std::uint8_t first,const std::uint8_t second){
            adjusted_c0_values_.push_back(first);adjusted_c0_values_.push_back(second);
        };
        while(adjusted_c0_values_.size()/2U<adjusted_pairs){
            if(adjusted_cursor>=adjusted_payload.size())throw std::runtime_error("Truncated Deuteros adjusted payload");
            const auto control=adjusted_payload[adjusted_cursor++];const auto family=control>>6U;
            ++adjusted_c0_families_[family];++adjusted_c0_packets_;std::uint32_t count=control&0x3fU;
            if(family==0){count=control==0?256U:control;for(std::uint32_t i=0;i<count&&adjusted_c0_values_.size()/2U<adjusted_pairs;++i){
                if(adjusted_cursor+2U>adjusted_payload.size())throw std::runtime_error("Truncated Deuteros adjusted literal packet");
                adjusted_emit(adjusted_payload[adjusted_cursor],adjusted_payload[adjusted_cursor+1U]);adjusted_cursor+=2U;}continue;}
            std::uint8_t first=0,second=0;
            if(family==1){count=count==0?256U:count;if(adjusted_cursor>=adjusted_payload.size())throw std::runtime_error("Truncated Deuteros adjusted fill packet");first=second=adjusted_payload[adjusted_cursor++];}
            else {
                if(family==2){
                    if(adjusted_cursor>=adjusted_payload.size())throw std::runtime_error("Truncated Deuteros adjusted count");
                    count=(count<<8U)|adjusted_payload[adjusted_cursor++];
                    if(count==0)count=65536U;
                } else if(count==0) {
                    count=256U;
                }
                if(adjusted_cursor+2U>adjusted_payload.size())throw std::runtime_error("Truncated Deuteros adjusted swapped packet");
                second=adjusted_payload[adjusted_cursor++];
                first=adjusted_payload[adjusted_cursor++];
            }
            for(std::uint32_t i=0;i<count&&adjusted_c0_values_.size()/2U<adjusted_pairs;++i)adjusted_emit(first,second);
        }
        if(adjusted_cursor!=adjusted_payload.size()||adjusted_c0_values_.size()!=22848U
            ||adjusted_c0_families_!=std::array<std::uint32_t,4>{{576,484,2,209}})
            throw std::runtime_error("Deuteros adjusted payload completion mismatch");
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
        if (next_command_address_ == std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error("Deuteros command stream address overflows");
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
        if (opcode == 0x16) {
            pending_command_opcode_ = opcode;
            return DeuterosAmigaTitleCommandOpcodePlan{observation,
                DeuterosAmigaTitleCommandOpcodeOutcome::unresolved_boundary,
                next_command_address_, 0x1fb00, 0x1fa1c,
                command_interpreter_.two_operand_target, 0, 0, 0, 0x1fb00};
        }
        if (opcode == 0x12) {
            pending_command_opcode_ = opcode;
            return DeuterosAmigaTitleCommandOpcodePlan{observation,
                DeuterosAmigaTitleCommandOpcodeOutcome::repeat_byte_boundary,
                next_command_address_, 0x1fe40, 0x1fa52, 0x1fe3c,
                0, 0, 0, 0x1fe40};
        }
        else if (opcode == 0x1a) { call_address = 0x1fa46; call_target = 0x1fde6; }
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
        else {
            pending_command_opcode_ = opcode;
            command_high_table_address_ = command_interpreter_.high_opcode_table_address
                + static_cast<std::uint32_t>(opcode & 0x0fU) * 2U;
            return DeuterosAmigaTitleCommandOpcodePlan{observation,
                DeuterosAmigaTitleCommandOpcodeOutcome::fixed_table_byte_boundary,
                next_command_address_, 0x1fada, 0, 0, 0, 0, 0, 0x1fada};
        }
        if (call_address != 0) {
            pending_command_call_ = PendingCommandCall{
                call_address, call_target, call_address + (call_address == 0x1faaa ? 6U : 4U),
                opcode};
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
        // `$1fbe6` has observable, route-dependent memory effects. A plain
        // register return must never bypass any recovered dispatch route.
        // Every mode now has a dedicated typed observation.
        if (pending_command_call_->address == 0x1fac2
            && pending_command_call_->target == 0x1fbe6
            && pending_command_call_->opcode >= 0x20
            && pending_command_call_->opcode < 0x90) {
            throw std::runtime_error(
                "Deuteros planar command requires a typed dispatch-route observation");
        }
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

    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandTwoOperandModePlan>
    observe_command_two_operand_mode(
        const DeuterosAmigaObservedTitleCommandTwoOperandMode& observation) {
        if (!pending_command_opcode_ || *pending_command_opcode_ != 0x16
            || command_two_operand_mode_) return std::nullopt;
        if (observation.trace_sequence <= last_command_sequence_
            || observation.instruction_address != 0x1fb00
            || observation.source_address != 0x1f98e) {
            throw std::runtime_error("Deuteros two-operand mode does not match boundary");
        }
        if (next_command_address_ == std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error("Deuteros two-operand stream address overflows");
        }
        last_command_sequence_ = observation.trace_sequence;
        command_two_operand_mode_ = observation;
        return DeuterosAmigaTitleCommandTwoOperandModePlan{observation,
            {{next_command_address_, next_command_address_ + 1U}}, 0x1fb0c};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandTwoOperandsPlan>
    observe_command_two_operands(
        const DeuterosAmigaObservedTitleCommandTwoOperands& observation) {
        if (!command_two_operand_mode_ || command_two_operands_) return std::nullopt;
        if (observation.trace_sequence <= last_command_sequence_
            || observation.instruction_addresses
                != std::array<std::uint32_t, 2>{{0x1fb0c, 0x1fb10}}
            || observation.source_addresses
                != std::array<std::uint32_t, 2>{{next_command_address_,
                    next_command_address_ + 1U}}) {
            throw std::runtime_error("Deuteros two command operands do not match boundary");
        }
        last_command_sequence_ = observation.trace_sequence;
        next_command_address_ += 2U;
        command_two_operands_ = observation;
        const bool alternate = command_two_operand_mode_->observed_value != 0;
        return DeuterosAmigaTitleCommandTwoOperandsPlan{observation,
            alternate ? 0x1fb46U : 0x1fb28U,
            alternate ? 0x1f994U : 0x1f168U, next_command_address_};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandTwoOperandLocalPlan>
    observe_command_two_operand_runtime_long(
        const DeuterosAmigaObservedTitleCommandTwoOperandRuntimeLong& observation) {
        if (!command_two_operand_mode_ || !command_two_operands_) return std::nullopt;
        const bool alternate = command_two_operand_mode_->observed_value != 0;
        const auto instruction = alternate ? 0x1fb46U : 0x1fb28U;
        const auto source = alternate ? 0x1f994U : 0x1f168U;
        if (observation.trace_sequence <= last_command_sequence_
            || observation.instruction_address != instruction
            || observation.source_address != source) {
            throw std::runtime_error("Deuteros two-operand runtime long does not match boundary");
        }
        const auto first = command_two_operands_->observed_values[0];
        const auto second = command_two_operands_->observed_values[1];
        std::uint32_t value = 0;
        if (!alternate) {
            const auto clamped = std::min<std::uint32_t>(second, 0x30U);
            const auto delta = clamped * 4U * 0x28U + first;
            value = (observation.observed_value & 0xffff0000U)
                | ((observation.observed_value + delta) & 0xffffU);
        } else {
            auto delta = static_cast<std::uint32_t>(second) * 4U
                * static_cast<std::uint32_t>(observation.observed_value & 0xffffU);
            delta = (delta & 0xffff0000U) | ((delta + first) & 0xffffU);
            value = 0x256c0U + delta;
        }
        last_command_sequence_ = observation.trace_sequence;
        pending_command_opcode_.reset();
        command_two_operand_mode_.reset();
        command_two_operands_.reset();
        return DeuterosAmigaTitleCommandTwoOperandLocalPlan{observation,
            0x1f974, value, next_command_address_,
            command_interpreter_.opcode_read_address};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandRepeatBytesPlan>
    observe_command_repeat_bytes(
        const DeuterosAmigaObservedTitleCommandRepeatBytes& observation) {
        if (!pending_command_opcode_ || *pending_command_opcode_ != 0x12
            || command_repeat_iterations_ != 0) return std::nullopt;
        if (next_command_address_ > std::numeric_limits<std::uint32_t>::max() - 2U
            || observation.trace_sequence <= last_command_sequence_
            || observation.instruction_addresses
                != std::array<std::uint32_t, 2>{{0x1fe40, 0x1fe44}}
            || observation.source_addresses
                != std::array<std::uint32_t, 2>{{next_command_address_,
                    next_command_address_ + 1U}}) {
            throw std::runtime_error("Deuteros repeated command bytes do not match boundary");
        }
        last_command_sequence_ = observation.trace_sequence;
        command_repeat_character_address_ = next_command_address_ + 1U;
        command_repeat_iterations_ = observation.count_value == 0
            ? 256U : observation.count_value;
        command_repeat_initial_iterations_ = command_repeat_iterations_;
        return DeuterosAmigaTitleCommandRepeatBytesPlan{observation,
            command_repeat_iterations_, 0x1fe46, 0x1fbe6, 0x1fe4a, 0x1fe46};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandRepeatCallReturnPlan>
    observe_command_repeat_call_return(
        const DeuterosAmigaObservedTitleCommandRepeatCallReturn& observation) {
        if (!pending_command_opcode_ || *pending_command_opcode_ != 0x12
            || command_repeat_iterations_ == 0) return std::nullopt;
        if (observation.trace_sequence <= last_command_sequence_
            || observation.call_address != 0x1fe46
            || observation.call_target != 0x1fbe6
            || observation.return_address != 0x1fe4a
            || observation.result_a4 != command_repeat_character_address_) {
            throw std::runtime_error("Deuteros repeated command call return does not match boundary");
        }
        last_command_sequence_ = observation.trace_sequence;
        --command_repeat_iterations_;
        const auto completed = static_cast<std::uint16_t>(
            command_repeat_initial_iterations_ - command_repeat_iterations_);
        if (command_repeat_iterations_ != 0) {
            return DeuterosAmigaTitleCommandRepeatCallReturnPlan{observation,
                completed, command_repeat_iterations_, 0x1fe46, 0, 0, 0x1fe46};
        }
        next_command_address_ = command_repeat_character_address_ + 1U;
        pending_command_opcode_.reset();
        return DeuterosAmigaTitleCommandRepeatCallReturnPlan{observation,
            completed, 0, 0, next_command_address_,
            command_interpreter_.opcode_read_address,
            command_interpreter_.opcode_read_address};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandHighTableBytePlan>
    observe_command_high_table_byte(
        const DeuterosAmigaObservedTitleCommandHighTableByte& observation) {
        if (!pending_command_opcode_ || *pending_command_opcode_ < 0x90
            || (command_high_phase_ != 0 && command_high_phase_ != 2)) {
            return std::nullopt;
        }
        const bool second = command_high_phase_ == 2;
        const auto instruction = second ? 0x1fae0U : 0x1fadaU;
        const auto source = command_high_table_address_ + (second ? 1U : 0U);
        if (observation.trace_sequence <= last_command_sequence_
            || observation.instruction_address != instruction
            || observation.source_address != source) {
            throw std::runtime_error("Deuteros high-opcode table byte does not match boundary");
        }
        last_command_sequence_ = observation.trace_sequence;
        command_high_phase_ = second ? 3U : 1U;
        return DeuterosAmigaTitleCommandHighTableBytePlan{observation,
            static_cast<std::uint8_t>(second ? 1 : 0), observation.observed_value,
            second ? 0x1fae2U : 0x1fadcU, 0x1fbe6,
            second ? 0x1fae6U : 0x1fae0U,
            second ? 0x1fae2U : 0x1fadcU};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandHighCallReturnPlan>
    observe_command_high_call_return(
        const DeuterosAmigaObservedTitleCommandHighCallReturn& observation) {
        if (!pending_command_opcode_ || *pending_command_opcode_ < 0x90
            || (command_high_phase_ != 1 && command_high_phase_ != 3)) {
            return std::nullopt;
        }
        const bool second = command_high_phase_ == 3;
        const auto call = second ? 0x1fae2U : 0x1fadcU;
        const auto ret = second ? 0x1fae6U : 0x1fae0U;
        if (observation.trace_sequence <= last_command_sequence_
            || observation.call_address != call
            || observation.call_target != 0x1fbe6
            || observation.return_address != ret
            || observation.result_a4 != command_high_table_address_ + 1U) {
            throw std::runtime_error("Deuteros high-opcode call return does not match boundary");
        }
        last_command_sequence_ = observation.trace_sequence;
        if (!second) {
            command_high_phase_ = 2;
            return DeuterosAmigaTitleCommandHighCallReturnPlan{observation, 1,
                0x1fae0, command_high_table_address_ + 1U, 0, 0, 0x1fae0};
        }
        command_high_phase_ = 0;
        pending_command_opcode_.reset();
        return DeuterosAmigaTitleCommandHighCallReturnPlan{observation, 2,
            0, 0, next_command_address_, command_interpreter_.opcode_read_address,
            command_interpreter_.opcode_read_address};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandPlanarWritePlan>
    observe_command_planar_write(
        const DeuterosAmigaObservedTitleCommandPlanarWrite& observation) {
        if (!pending_command_call_ || pending_command_call_->address != 0x1fac2
            || pending_command_call_->target != 0x1fbe6
            || pending_command_call_->opcode < 0x20
            || pending_command_call_->opcode >= 0x90) return std::nullopt;
        constexpr std::array<std::uint32_t, 2> mode_instructions{{0x1fbe6, 0x1fc22}};
        constexpr std::array<std::uint32_t, 2> mode_sources{{0x1f98c, 0x1f98e}};
        constexpr std::array<std::uint32_t, 5> pointer_sources{{
            0x1f99c, 0x1f974, 0x1f96c, 0x1f970, 0x1f9a0}};
        if (observation.trace_sequence <= last_command_sequence_
            || observation.mode_instruction_addresses != mode_instructions
            || observation.mode_source_addresses != mode_sources
            || observation.observed_mode_values != std::array<std::uint8_t, 2>{{0, 0}}
            || observation.pointer_source_addresses != pointer_sources) {
            throw std::runtime_error("Deuteros planar-write gates do not match boundary");
        }
        const auto glyph_base = observation.observed_pointer_values[0]
            + static_cast<std::uint32_t>(pending_command_call_->opcode - 0x20U) * 8U;
        const auto destination_base = observation.observed_pointer_values[1];
        const auto first_words_base = observation.observed_pointer_values[3];
        const auto second_words_base = observation.observed_pointer_values[2];
        std::array<std::uint32_t, 32> destinations{};
        std::array<std::uint8_t, 32> values{};
        for (std::uint32_t row = 0; row < 8; ++row) {
            if (observation.glyph_source_addresses[row] != glyph_base + row) {
                throw std::runtime_error("Deuteros planar-write glyph order does not match boundary");
            }
            const auto glyph = observation.observed_glyph_values[row];
            for (std::uint32_t plane = 0; plane < 4; ++plane) {
                const auto index = row * 4U + plane;
                const auto first_address = first_words_base + plane * 2U;
                const auto second_address = second_words_base + plane * 2U;
                if (observation.first_word_source_addresses[index] != first_address
                    || observation.second_word_source_addresses[index] != second_address) {
                    throw std::runtime_error("Deuteros planar-write word order does not match boundary");
                }
                const auto first_low = static_cast<std::uint8_t>(
                    observation.observed_first_words[index] & 0xffU);
                const auto second_low = static_cast<std::uint8_t>(
                    observation.observed_second_words[index] & 0xffU);
                values[index] = static_cast<std::uint8_t>(
                    (first_low & static_cast<std::uint8_t>(~glyph))
                    | (second_low & glyph));
                destinations[index] = destination_base + row * 0x28U + plane * 0x1f40U;
            }
        }
        last_command_sequence_ = observation.trace_sequence;
        pending_command_call_.reset();
        return DeuterosAmigaTitleCommandPlanarWritePlan{observation,
            destinations, values, 0x1f974,
            destination_base + observation.observed_pointer_values[4],
            0x1fc9a, 0x1fac6, next_command_address_,
            command_interpreter_.opcode_read_address};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandPlanarVariantWritePlan>
    observe_command_planar_variant_write(
        const DeuterosAmigaObservedTitleCommandPlanarVariantWrite& observation) {
        if (!pending_command_call_ || pending_command_call_->address != 0x1fac2
            || pending_command_call_->target != 0x1fbe6
            || pending_command_call_->opcode < 0x20
            || pending_command_call_->opcode >= 0x90) return std::nullopt;
        constexpr std::array<std::uint32_t, 2> mode_sources{{0x1f98c, 0x1f98e}};
        if (observation.trace_sequence <= last_command_sequence_
            || observation.mode_source_addresses != mode_sources) {
            throw std::runtime_error("Deuteros planar variant gates do not match boundary");
        }
        const auto primary_mode = observation.observed_mode_values[0];
        const bool positive = primary_mode != 0 && primary_mode < 0x80;
        if (primary_mode >= 0x80) {
            throw std::runtime_error("Deuteros negative planar dispatch is not a write route");
        }
        const bool set = observation.observed_mode_values[1] != 0;
        if (!positive && !set) {
            throw std::runtime_error("Deuteros zero/zero planar route requires its original typed observation");
        }
        const std::array<std::uint32_t, 2> expected_mode_instructions{{
            0x1fbe6, positive ? 0x1fc9cU : 0x1fc22U}};
        if (observation.mode_instruction_addresses != expected_mode_instructions
            || observation.pointer_source_addresses.size()
                != observation.observed_pointer_values.size()) {
            throw std::runtime_error("Deuteros planar variant route does not match boundary");
        }

        DeuterosAmigaTitlePlanarVariant variant;
        std::vector<std::uint32_t> expected_pointers;
        std::uint32_t row_stride = 0x28;
        std::uint32_t plane_stride = 0x1f40;
        std::uint32_t glyph_pointer = 0;
        std::uint32_t destination = 0;
        std::uint32_t base_words = 0;
        std::uint32_t blend_words = 0;
        std::uint32_t pointer_advance = 1;
        std::uint32_t routine_return = 0;
        if (!positive) {
            variant = DeuterosAmigaTitlePlanarVariant::zero_set;
            expected_pointers = {0x1f994, 0x1f998, 0x1f99c, 0x1f974,
                0x1f96c, 0x1f970};
            if (observation.observed_pointer_values.size() == expected_pointers.size()) {
                row_stride = observation.observed_pointer_values[0];
                plane_stride = observation.observed_pointer_values[1];
                glyph_pointer = observation.observed_pointer_values[2];
                destination = observation.observed_pointer_values[3];
                blend_words = observation.observed_pointer_values[4];
                base_words = observation.observed_pointer_values[5];
            }
            routine_return = 0x1fd78;
        } else if (!set) {
            variant = DeuterosAmigaTitlePlanarVariant::positive_clear;
            expected_pointers = {0x1f99c, 0x1f974, 0x1f96c};
            if (observation.observed_pointer_values.size() == expected_pointers.size()) {
                glyph_pointer = observation.observed_pointer_values[0];
                destination = observation.observed_pointer_values[1];
                blend_words = observation.observed_pointer_values[2];
            }
            routine_return = 0x1fd08;
        } else {
            variant = DeuterosAmigaTitlePlanarVariant::positive_set;
            expected_pointers = {0x1f994, 0x1f998, 0x1f99c, 0x1f974, 0x1f96c};
            if (observation.observed_pointer_values.size() == expected_pointers.size()) {
                row_stride = observation.observed_pointer_values[0];
                plane_stride = observation.observed_pointer_values[1];
                glyph_pointer = observation.observed_pointer_values[2];
                destination = observation.observed_pointer_values[3];
                blend_words = observation.observed_pointer_values[4];
            }
            routine_return = 0x1fde2;
        }
        if (observation.pointer_source_addresses != expected_pointers
            || row_stride == 0 || plane_stride == 0) {
            throw std::runtime_error("Deuteros planar variant pointer reads do not match boundary");
        }

        const auto glyph_base = glyph_pointer
            + static_cast<std::uint32_t>(pending_command_call_->opcode - 0x20U) * 8U;
        std::array<std::uint32_t, 32> destinations{};
        std::array<std::uint8_t, 32> values{};
        for (std::uint32_t row = 0; row < 8U; ++row) {
            if (observation.glyph_source_addresses[row] != glyph_base + row) {
                throw std::runtime_error("Deuteros planar variant glyph order does not match boundary");
            }
            const auto glyph = observation.observed_glyph_values[row];
            for (std::uint32_t plane = 0; plane < 4U; ++plane) {
                const auto index = row * 4U + plane;
                const auto address64 = static_cast<std::uint64_t>(destination)
                    + static_cast<std::uint64_t>(row) * row_stride
                    + static_cast<std::uint64_t>(plane) * plane_stride;
                if (address64 > std::numeric_limits<std::uint32_t>::max()) {
                    throw std::runtime_error("Deuteros planar variant destination overflows");
                }
                destinations[index] = static_cast<std::uint32_t>(address64);
                const auto expected_base = positive
                    ? destinations[index] : base_words + plane * 2U;
                if (observation.base_source_addresses[index] != expected_base
                    || observation.blend_word_source_addresses[index]
                        != blend_words + plane * 2U
                    || (positive && observation.observed_base_values[index] > 0xffU)) {
                    throw std::runtime_error("Deuteros planar variant source order does not match boundary at element "
                        + std::to_string(index) + " (base $"
                        + std::to_string(observation.base_source_addresses[index])
                        + "/$" + std::to_string(expected_base) + ", blend $"
                        + std::to_string(observation.blend_word_source_addresses[index])
                        + "/$" + std::to_string(blend_words + plane * 2U)
                        + ", observed base "
                        + std::to_string(observation.observed_base_values[index]) + ")");
                }
                const auto base = static_cast<std::uint8_t>(
                    observation.observed_base_values[index] & 0xffU);
                const auto blend = static_cast<std::uint8_t>(
                    observation.observed_blend_words[index] & 0xffU);
                values[index] = static_cast<std::uint8_t>(
                    (base & static_cast<std::uint8_t>(~glyph)) | (blend & glyph));
            }
        }
        last_command_sequence_ = observation.trace_sequence;
        pending_command_call_.reset();
        return DeuterosAmigaTitleCommandPlanarVariantWritePlan{observation,
            variant, destinations, values, 0x1f974,
            destination + pointer_advance, row_stride, plane_stride,
            row_stride == 0x28 && plane_stride == 0x1f40,
            routine_return, 0x1fac6, next_command_address_,
            command_interpreter_.opcode_read_address};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandNegativeServicePlan>
    observe_command_negative_service(
        const DeuterosAmigaObservedTitleCommandNegativeService& observation) {
        if (!pending_command_call_ || pending_command_call_->address != 0x1fac2
            || pending_command_call_->target != 0x1fbe6
            || pending_command_call_->opcode < 0x20
            || pending_command_call_->opcode >= 0x90) return std::nullopt;
        if (observation.trace_sequence <= last_command_sequence_
            || observation.mode_instruction_address != 0x1fbe6
            || observation.mode_source_address != 0x1f98c
            || observation.observed_mode_value < 0x80) {
            throw std::runtime_error("Deuteros negative service gate does not match boundary");
        }
        const bool suppressed = pending_command_call_->opcode == 0x20;
        if (suppressed) {
            if (observation.service_called || observation.service_call_address != 0
                || observation.service_target != 0
                || observation.service_return_address != 0
                || observation.service_d0 != 0 || observation.service_d1 != 0) {
                throw std::runtime_error("Deuteros space command must suppress the negative service");
            }
        } else if (!observation.service_called
            || observation.service_call_address != 0x1fc08
            || observation.service_target != 0x3fbf8
            || observation.service_return_address != 0x1fc0e
            || observation.service_d0 != 0x13
            || observation.service_d1 != 0x0c) {
            throw std::runtime_error("Deuteros negative nested service does not match boundary");
        }
        last_command_sequence_ = observation.trace_sequence;
        pending_command_call_.reset();
        return DeuterosAmigaTitleCommandNegativeServicePlan{observation,
            suppressed, 0x4e20, 0x1fc20, 0x1fac6, next_command_address_,
            command_interpreter_.opcode_read_address};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandPointerRoutePlan>
    observe_post_command_pointer_route(
        const DeuterosAmigaObservedTitlePostCommandPointerRoute& observation) {
        if (!command_halted_ || observed_post_command_pointer_route_) return std::nullopt;
        if (observation.trace_sequence <= last_command_sequence_
            || observation.preceding_call_address != 0x404fe
            || observation.preceding_call_target
                != post_command_continuation_.direct_call_addresses[2]
            || observation.preceding_return_address != 0x40504
            || observation.pointer_call_address != post_command_pointer_route_.caller_address
            || observation.pointer_call_target != post_command_pointer_route_.entry_address
            || observation.flag_instruction_address != 0x20238
            || observation.flag_source_address
                != post_command_pointer_route_.selected_flag_address) {
            throw std::runtime_error("Deuteros post-command pointer route does not match boundary");
        }
        DeuterosAmigaTitlePostCommandPointerRoutePlan plan;
        plan.observation = observation;
        if (observation.observed_flag_value == 0) {
            plan.destination_addresses = {{
                post_command_pointer_route_.selected_pointer_cell_address,
                post_command_pointer_route_.selected_flag_address, 0}};
            plan.destination_values = {{
                post_command_pointer_route_.selected_pointer_literal,
                post_command_pointer_route_.selected_flag_value, 0}};
            plan.destination_widths = {{MemoryTransferElementWidth::longword,
                MemoryTransferElementWidth::byte, MemoryTransferElementWidth::byte}};
            plan.effect_count = 2;
            plan.nested_boundary_address = post_command_pointer_route_.selected_branch_target;
        } else {
            plan.destination_addresses[0] = post_command_pointer_route_.entry_clear_flag_address;
            plan.destination_values[0] = 0;
            plan.destination_widths[0] = MemoryTransferElementWidth::byte;
            plan.effect_count = 1;
            plan.pointer_call_completed = true;
            plan.caller_continuation_address =
                post_command_pointer_route_.caller_continuation_address;
        }
        observed_post_command_pointer_route_ = observation;
        last_command_sequence_ = observation.trace_sequence;
        return plan;
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandGraphicsReturnPlan>
    observe_post_command_graphics_return(
        const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        if (!observed_post_command_pointer_route_
            || observed_post_command_pointer_route_->observed_flag_value != 0
            || observed_post_command_graphics_return_) return std::nullopt;
        if (observation.trace_sequence <= last_command_sequence_
            || observation.library_base_source_address != 0x12fec
            || observation.observed_library_base != graphics_library_base_
            || observation.call_address != 0x200f4
            || observation.vector != -0x1a4
            || observation.return_address != 0x200f8) {
            throw std::runtime_error("Deuteros post-command graphics return does not match boundary");
        }
        observed_post_command_graphics_return_ = observation;
        last_command_sequence_ = observation.trace_sequence;
        return DeuterosAmigaTitlePostCommandGraphicsReturnPlan{observation,
            post_command_pointer_route_.entry_clear_flag_address,0,
            0x200f8,post_command_pointer_route_.caller_continuation_address,
            0x4d,0x4050e,0x41bb4,0x4050e};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandFirstDispatchPlan>
    advance_post_command_first_dispatch() {
        if (!observed_post_command_graphics_return_ || post_command_first_dispatch_advanced_)
            return std::nullopt;
        post_command_first_dispatch_advanced_=true;
        // `$004d * $000e` selects `$416d0`; its signed/high-bit descriptor
        // redirects with low byte `$c1` through the fixed `$4128e` descriptor.
        return DeuterosAmigaTitlePostCommandFirstDispatchPlan{
            0x4d,0x41bb4,0x00c1,0x00c1,0x422ae,0x000367ae,0x00078aa8,
            0x000256dc,0x0002bfc0,0x000068fe,0x38,0x41c72,0x41c72};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleFirstDispatchHeaderPlan>
    observe_post_command_first_dispatch_header(
        const DeuterosAmigaObservedTitleFirstDispatchHeader& observation) {
        if (!post_command_first_dispatch_advanced_ || observed_first_dispatch_header_)
            return std::nullopt;
        constexpr std::array<std::uint32_t,2> instructions{{0x41c72,0x41c7e}};
        constexpr std::array<std::uint32_t,2> sources{{0x78aa8,0x78aaa}};
        if (observation.trace_sequence <= last_command_sequence_
            || observation.instruction_addresses != instructions
            || observation.source_addresses != sources
            || observation.observed_words != std::array<std::uint16_t,2>{{0x44,0x8010}}) {
            throw std::runtime_error("Deuteros first dispatch header does not match boundary");
        }
        observed_first_dispatch_header_=observation;
        last_command_sequence_=observation.trace_sequence;
        const auto width=observation.observed_words[0];
        const auto height=observation.observed_words[1];
        const bool low=height < 0xc8;
        return DeuterosAmigaTitleFirstDispatchHeaderPlan{observation,width,height,
            static_cast<std::uint16_t>(width-1U),{{0x410c8,0x410ca}},{{width,height}},
            low,0x78aac,low?0x41c98U:0x41d44U,low?0x41c98U:0x41d44U};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleFirstDispatchPacketPlan>
    advance_post_command_first_dispatch_packet() {
        if (!observed_first_dispatch_header_ || first_dispatch_packet_advanced_
            || observed_first_dispatch_header_->observed_words[1] < 0xc8)
            return std::nullopt;
        first_dispatch_packet_advanced_=true;
        return DeuterosAmigaTitleFirstDispatchPacketPlan{0x01,{{0x0f,0xff}},
            {{0x410cc,0x410ce,0x256dc,0x256dd}},{{0x11,0x10,0x0f,0xff}},
            {{MemoryTransferElementWidth::word,MemoryTransferElementWidth::word,
              MemoryTransferElementWidth::byte,MemoryTransferElementWidth::byte}},
            0x78aaf,0x256de,0x10,0x10,4,0x41d60};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleFirstDispatchDecodePlan>
    advance_post_command_first_dispatch_decode() {
        if(!first_dispatch_packet_advanced_||first_dispatch_decode_advanced_)return std::nullopt;
        first_dispatch_decode_advanced_=true;
        return DeuterosAmigaTitleFirstDispatchDecodePlan{
            {first_dispatch_decode_addresses_.begin()+2,first_dispatch_decode_addresses_.end()},
            {first_dispatch_decode_values_.begin()+2,first_dispatch_decode_values_.end()},
            0x78aac,0x78c71,first_dispatch_packet_count_,first_dispatch_family_counts_,
            1088,0x41e40,"684e83c90a64bd8829ad01f3b7615b5d686d4d054e25c7483d29b7b50046fb1d"};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleFirstDispatchCallerTailPlan>
    advance_post_command_first_dispatch_caller_tail() {
        if(!first_dispatch_decode_advanced_||first_dispatch_caller_tail_advanced_)return std::nullopt;
        const std::array<std::uint32_t,4> addresses{{0x256dc,0x2711c,0x28b5c,0x2a59c}};
        std::array<std::uint16_t,4> words{};
        for(std::size_t i=0;i<addresses.size();++i){
            const auto it=std::lower_bound(first_dispatch_decode_addresses_.begin(),
                first_dispatch_decode_addresses_.end(),addresses[i]);
            if(it==first_dispatch_decode_addresses_.end()||*it!=addresses[i])
                throw std::runtime_error("Deuteros decoded mask address is absent");
            const auto index=static_cast<std::size_t>(it-first_dispatch_decode_addresses_.begin());
            if(index+1>=first_dispatch_decode_values_.size()
                ||first_dispatch_decode_addresses_[index+1]!=addresses[i]+1U)
                throw std::runtime_error("Deuteros decoded mask word is truncated");
            words[i]=static_cast<std::uint16_t>(first_dispatch_decode_values_[index]<<8U)
                |first_dispatch_decode_values_[index+1];
        }
        const auto combined=static_cast<std::uint16_t>(words[0]|words[1]|words[2]|words[3]);
        first_dispatch_caller_tail_advanced_=true;
        return DeuterosAmigaTitleFirstDispatchCallerTailPlan{0x41be6,0x27f06,0x256dc,
            3,0x0b,addresses,words,combined,0x27f06,0x41ed8};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleFirstDispatchMergePlan>
    observe_post_command_first_dispatch_destination_words(
        const DeuterosAmigaObservedTitleFirstDispatchDestinationWords& observation) {
        if(!first_dispatch_caller_tail_advanced_||first_dispatch_merge_observed_)return std::nullopt;
        std::vector<std::uint32_t> required;
        required.reserve(960);
        for(std::uint32_t outer=0;outer<3;++outer)for(std::uint32_t word=0;word<184;++word)
            for(std::uint32_t plane=0;plane<4;++plane){
                const auto address=0x27f06U+outer*0x38U+word*2U+plane*0x1a40U;
                if(std::find(required.begin(),required.end(),address)==required.end())required.push_back(address);
            }
        if(observation.trace_sequence<=last_command_sequence_
            ||observation.first_instruction_address!=0x41ed8
            ||observation.source_addresses!=required
            ||observation.observed_words.size()!=required.size())
            throw std::runtime_error("Deuteros first merge destination observations do not match boundary");
        std::vector<std::uint32_t> mask_gaps;
        for(std::uint32_t outer=0;outer<3;++outer)for(std::uint32_t word=0;word<184;++word)
            for(std::uint32_t plane=0;plane<4;++plane){const auto address=0x256dcU+outer*0x38U+word*2U+plane*0x1a40U;
                if(std::binary_search(first_dispatch_decode_addresses_.begin(),first_dispatch_decode_addresses_.end(),address))continue;
                if(std::find(mask_gaps.begin(),mask_gaps.end(),address)==mask_gaps.end())mask_gaps.push_back(address);}
        if(observation.mask_source_addresses!=mask_gaps
            ||observation.observed_mask_words.size()!=mask_gaps.size())
            throw std::runtime_error("Deuteros first merge mask-gap observations do not match boundary");
        auto values=observation.observed_words;
        const auto decoded_word=[&](std::uint32_t address){
            const auto it=std::lower_bound(first_dispatch_decode_addresses_.begin(),
                first_dispatch_decode_addresses_.end(),address);
            if(it==first_dispatch_decode_addresses_.end()||*it!=address){
                const auto gap=std::find(mask_gaps.begin(),mask_gaps.end(),address);
                if(gap==mask_gaps.end())throw std::runtime_error("Deuteros merge mask lies outside typed bytes");
                return observation.observed_mask_words[static_cast<std::size_t>(gap-mask_gaps.begin())];}
            const auto index=static_cast<std::size_t>(it-first_dispatch_decode_addresses_.begin());
            if(index+1>=first_dispatch_decode_values_.size()
                ||first_dispatch_decode_addresses_[index+1]!=address+1U)
                throw std::runtime_error("Deuteros merge mask word is truncated");
            return static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(first_dispatch_decode_values_[index]) << 8U)
                | static_cast<std::uint16_t>(first_dispatch_decode_values_[index + 1U]));
        };
        for(std::uint32_t outer=0;outer<3;++outer)for(std::uint32_t word=0;word<184;++word){
            std::array<std::uint16_t,4> masks{};std::uint16_t combined=0;
            for(std::uint32_t plane=0;plane<4;++plane){masks[plane]=decoded_word(
                0x256dcU+outer*0x38U+word*2U+plane*0x1a40U);combined|=masks[plane];}
            for(std::uint32_t plane=0;plane<4;++plane){
                const auto address=0x27f06U+outer*0x38U+word*2U+plane*0x1a40U;
                const auto it=std::find(required.begin(),required.end(),address);
                const auto index=static_cast<std::size_t>(it-required.begin());
                values[index]=static_cast<std::uint16_t>((values[index]|combined)
                    &(masks[plane]|static_cast<std::uint16_t>(~combined)));
            }
        }
        first_dispatch_merge_observed_=true;last_command_sequence_=observation.trace_sequence;
        return DeuterosAmigaTitleFirstDispatchMergePlan{observation,std::move(required),
            std::move(values),2208,960,0x41f30,0x40514,0x4e,0x40518,0x41bb4};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandSecondDispatchPlan>
    advance_post_command_second_dispatch() {
        if(!first_dispatch_merge_observed_||second_dispatch_advanced_)return std::nullopt;
        second_dispatch_advanced_=true;
        // The outer lookup uses `$4129a`, but the high-bit path deliberately
        // resets A5 to the fixed descriptor at `$4128e` before indexing `$c2`.
        return DeuterosAmigaTitlePostCommandSecondDispatchPlan{0x4e,0x416de,0x4129a,
            0x00c2,0x4128e,0x00c2,0x422b2,0x00036978,0x78c72,0x256dc,
            0x2bfc0,0x38,0x14,0x8010,0x41c72};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleSecondDispatchDecodePlan>
    advance_post_command_second_dispatch_decode() {
        if(!second_dispatch_advanced_||second_dispatch_decode_advanced_)return std::nullopt;
        second_dispatch_decode_advanced_=true;
        return DeuterosAmigaTitleSecondDispatchDecodePlan{second_dispatch_decode_addresses_,
            second_dispatch_decode_values_,0x78c76,0x78d5a,second_dispatch_packet_count_,
            second_dispatch_family_counts_,320,0x41be6,
            "dabcb6ee4feb0022f3232bcab1ffccb6657448e8602c39ef248da996e57a5666"};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitleSecondDispatchMergePlan>
    observe_post_command_second_dispatch_destination_words(
        const DeuterosAmigaObservedTitleSecondDispatchDestinationWords& observation) {
        if(!second_dispatch_decode_advanced_||second_dispatch_merge_observed_)return std::nullopt;
        std::array<std::uint32_t,64> required{};std::size_t required_index=0;
        for(std::uint32_t row=0;row<16;++row)for(std::uint32_t plane=0;plane<4;++plane)
            required[required_index++]=0x256e6U+row*0x38U+plane*0x1a40U;
        if(observation.trace_sequence<=last_command_sequence_
            ||observation.first_instruction_address!=0x41ed8
            ||observation.source_addresses!=required)
            throw std::runtime_error("Deuteros second merge destination observations do not match boundary");
        const auto decoded_word=[&](std::uint32_t address){
            const auto it=std::lower_bound(second_dispatch_decode_addresses_.begin(),
                second_dispatch_decode_addresses_.end(),address);
            if(it==second_dispatch_decode_addresses_.end()||*it!=address)
                throw std::runtime_error("Deuteros second merge mask lies outside decoded bytes");
            const auto index=static_cast<std::size_t>(it-second_dispatch_decode_addresses_.begin());
            return static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(second_dispatch_decode_values_[index])<<8U
                |second_dispatch_decode_values_[index+1U]);
        };
        std::vector<std::uint32_t> addresses;std::vector<std::uint16_t> values;
        addresses.reserve(320);values.reserve(320);required_index=0;
        for(std::uint32_t row=0;row<16;++row)for(std::uint32_t word=0;word<5;++word){
            std::array<std::uint16_t,4> masks{};std::uint16_t combined=0;
            for(std::uint32_t plane=0;plane<4;++plane){masks[plane]=decoded_word(
                0x256dcU+row*0x38U+word*2U+plane*0x1a40U);combined|=masks[plane];}
            for(std::uint32_t plane=0;plane<4;++plane){
                const auto address=0x256deU+row*0x38U+word*2U+plane*0x1a40U;
                const auto prior=word==4?observation.observed_words[row*4U+plane]:decoded_word(address);
                addresses.push_back(address);values.push_back(static_cast<std::uint16_t>(
                    (prior|combined)&(masks[plane]|static_cast<std::uint16_t>(~combined))));
            }
        }
        second_dispatch_merge_observed_=true;last_command_sequence_=observation.trace_sequence;
        return DeuterosAmigaTitleSecondDispatchMergePlan{observation,std::move(addresses),
            std::move(values),320,320,0x41f30,0x4051e,0x4051e};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandServiceRoutePrefixPlan>
    advance_post_command_service_route_prefix() {
        if(!second_dispatch_merge_observed_||post_command_service_route_prefix_advanced_)
            return std::nullopt;
        post_command_service_route_prefix_advanced_=true;
        return DeuterosAmigaTitlePostCommandServiceRoutePrefixPlan{
            {{0x222ae,0x13006,0x13052,0x1301a,0x13036,0x13126,0x1304c}},
            {{0x2151a,0,0x13050,0x13008,0x13024,0,0x1303a}},
            {{MemoryTransferElementWidth::longword,MemoryTransferElementWidth::word,
              MemoryTransferElementWidth::longword,MemoryTransferElementWidth::longword,
              MemoryTransferElementWidth::longword,MemoryTransferElementWidth::word,
              MemoryTransferElementWidth::longword}},
            0,0x4051e,post_command_service_route_.entry_address,0x20e6a,
            post_command_service_route_.external_call_targets[0],0xf9,0x20e6a};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandServiceFirstReturnPlan>
    observe_post_command_service_first_return(
        const DeuterosAmigaObservedLocalCallReturn& observation) {
        if(!post_command_service_route_prefix_advanced_||post_command_service_first_return_)
            return std::nullopt;
        if(observation.trace_sequence<=last_command_sequence_
            ||observation.call_address!=0x20e6a
            ||observation.call_target!=post_command_service_route_.external_call_targets[0]
            ||observation.return_address!=0x20e70)
            throw std::runtime_error("Deuteros post-command first service return does not match boundary");
        post_command_service_first_return_=observation;
        last_command_sequence_=observation.trace_sequence;
        return DeuterosAmigaTitlePostCommandServiceFirstReturnPlan{observation,0,0x00a0,
            0x20e7a,post_command_service_route_.external_call_targets[1],0x20e7a};
    }

    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandServiceSecondReturnPlan>
    observe_post_command_service_second_return(
        const DeuterosAmigaObservedLocalCallReturn& observation) {
        if(!post_command_service_first_return_||post_command_service_second_return_)
            return std::nullopt;
        if(observation.trace_sequence<=last_command_sequence_
            ||observation.call_address!=0x20e7a
            ||observation.call_target!=post_command_service_route_.external_call_targets[1]
            ||observation.return_address!=0x20e80)
            throw std::runtime_error("Deuteros post-command second service return does not match boundary");
        post_command_service_second_return_=observation;
        last_command_sequence_=observation.trace_sequence;
        return DeuterosAmigaTitlePostCommandServiceSecondReturnPlan{observation,0,0,
            0x13792,0x127a3980,0x1378e,0x20e96,
            post_command_service_route_.external_call_targets[2],0x20e96};
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandServiceThirdReturnPlan>
    observe_post_command_service_third_return(const DeuterosAmigaObservedLocalCallReturn& o){
        if(!post_command_service_second_return_||post_command_service_third_return_)return std::nullopt;
        if(o.trace_sequence<=last_command_sequence_||o.call_address!=0x20e96
            ||o.call_target!=post_command_service_route_.external_call_targets[2]
            ||o.return_address!=0x20e9c)throw std::runtime_error("Deuteros post-command third service return does not match boundary");
        post_command_service_third_return_=o;last_command_sequence_=o.trace_sequence;
        return DeuterosAmigaTitlePostCommandServiceThirdReturnPlan{o,0x20e9c,
            post_command_service_route_.nested_entry_address,0x1301a,0x13008,0x20bae};
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandNestedWordsPlan>
    observe_post_command_nested_words(const DeuterosAmigaObservedTitlePostCommandNestedWords& o){
        if(!post_command_service_third_return_||post_command_nested_words_)return std::nullopt;
        if(o.trace_sequence<=last_command_sequence_
            ||o.instruction_addresses!=std::array<std::uint32_t,2>{{0x20bae,0x20bb6}}
            ||o.source_addresses!=std::array<std::uint32_t,2>{{0x13008,0x202bc}})
            throw std::runtime_error("Deuteros post-command nested words do not match boundary");
        const auto shifted=static_cast<std::uint16_t>((o.observed_words[0]&0xff00U)|((o.observed_words[0]&0xffU)>>1U));
        const bool carry=(o.observed_words[0]&1U)!=0;
        const auto decremented=static_cast<std::uint16_t>((o.observed_words[1]&0xff00U)|((o.observed_words[1]-1U)&0xffU));
        post_command_nested_words_=o;last_command_sequence_=o.trace_sequence;
        nested_d7_=shifted;nested_d5_=decremented;nested_d6_=7;nested_iteration_=0;
        nested_call_pending_=!(carry&&static_cast<std::uint8_t>(decremented)==0);
        nested_call_address_=carry?0x20be4U:0x20bd6U;
        if(carry&&static_cast<std::uint8_t>(decremented)==0)
            return DeuterosAmigaTitlePostCommandNestedWordsPlan{o,shifted,decremented,true,false,0,0,
                0,0,0,0,0x20bea};
        return DeuterosAmigaTitlePostCommandNestedWordsPlan{o,shifted,decremented,carry,!carry&&static_cast<std::uint8_t>(decremented)==0,
            0x202bc,decremented,static_cast<std::uint16_t>(carry?0x0008:0x0048),0x0010,
            carry?0x20be4U:0x20bd6U,post_command_service_route_.nested_branch_target,
            carry?0x20be4U:0x20bd6U};
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandNestedCallReturnPlan>
    observe_post_command_nested_call_return(const DeuterosAmigaObservedLocalCallReturn& o){
        if(!post_command_nested_words_||!nested_call_pending_)return std::nullopt;
        const auto return_address=nested_call_address_==0x20bd6U?0x20bdcU:0x20beaU;
        if(o.trace_sequence<=last_command_sequence_||o.call_address!=nested_call_address_
            ||o.call_target!=post_command_service_route_.nested_branch_target
            ||o.return_address!=return_address)
            throw std::runtime_error("Deuteros post-command nested call return does not match boundary");
        nested_call_pending_=false;last_command_sequence_=o.trace_sequence;
        return DeuterosAmigaTitlePostCommandNestedCallReturnPlan{o,nested_d7_,nested_d6_,0x20bea};
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandNestedLoopAdvancePlan>
    advance_post_command_nested_loop(){
        if(!post_command_nested_words_||nested_call_pending_||nested_loop_completed_)return std::nullopt;
        if(nested_d6_==0){nested_d6_=0xffff;nested_loop_completed_=true;
            return DeuterosAmigaTitlePostCommandNestedLoopAdvancePlan{8,nested_d7_,nested_d5_,nested_d6_,false,false,true,0,0,0x000f,0,0x20bf4,0x1f9b8,0x20bf4};}
        --nested_d6_;++nested_iteration_;
        const std::array<std::uint16_t,16> table{{0x0008,0x0010,0x0011,0x0310,0x001a,0x0020,0x0023,0x0320,0x002c,0x0030,0x0035,0x0330,0x003e,0x0040,0x0047,0x0340}};
        const auto table_index=static_cast<std::size_t>(nested_iteration_)*2U;
        const bool carry=(nested_d7_&1U)!=0;nested_d7_=static_cast<std::uint16_t>((nested_d7_&0xff00U)|((nested_d7_&0xffU)>>1U));
        nested_d5_=static_cast<std::uint16_t>((nested_d5_&0xff00U)|((nested_d5_-1U)&0xffU));
        const bool skip=carry&&static_cast<std::uint8_t>(nested_d5_)==0;
        nested_call_pending_=!skip;nested_call_address_=carry?0x20be4U:0x20bd6U;
        const bool writes=!carry&&static_cast<std::uint8_t>(nested_d5_)==0;
        return DeuterosAmigaTitlePostCommandNestedLoopAdvancePlan{nested_iteration_,nested_d7_,nested_d5_,nested_d6_,carry,writes,false,
            0x202bc,nested_d5_,static_cast<std::uint16_t>(carry?table[table_index]:0x4fU-nested_d6_),table[table_index+1U],
            skip?0U:nested_call_address_,post_command_service_route_.nested_branch_target,skip?0x20beaU:nested_call_address_};
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandContinuationReturnPlan>
    observe_post_command_continuation_return(const DeuterosAmigaObservedLocalCallReturn& o){
        if(!nested_loop_completed_||post_command_continuation_return_)return std::nullopt;
        if(o.trace_sequence<=last_command_sequence_||o.call_address!=0x20bf4
            ||o.call_target!=0x1f9b8||o.return_address!=0x20bfa)
            throw std::runtime_error("Deuteros post-command continuation return does not match boundary");
        post_command_continuation_return_=o;last_command_sequence_=o.trace_sequence;
        return DeuterosAmigaTitlePostCommandContinuationReturnPlan{o,0x222ae,0x2151a,0x2151a,0x20c00};
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandPointerChainPlan>
    observe_post_command_pointer_chain(const DeuterosAmigaObservedTitlePostCommandPointerChain& o){
        if(!post_command_continuation_return_||post_command_pointer_chain_)return std::nullopt;
        if(o.trace_sequence<=last_command_sequence_
            ||o.instruction_addresses!=std::array<std::uint32_t,3>{{0x20c00,0x20c02,0x20c04}}
            ||o.source_addresses[0]!=0x2151a
            ||o.source_addresses[1]!=o.observed_pointer_values[0]
            ||o.source_addresses[2]!=o.observed_pointer_values[1]
            ||o.observed_pointer_values[0]==0||o.observed_pointer_values[1]==0)
            throw std::runtime_error("Deuteros post-command pointer chain does not match boundary");
        const auto low=static_cast<std::uint8_t>(o.observed_word);
        const auto descriptor=static_cast<std::uint16_t>(low!=0&&(low&0x40U)==0?0x00b0:0x00bd);
        post_command_pointer_chain_=o;last_command_sequence_=o.trace_sequence;
        return DeuterosAmigaTitlePostCommandPointerChainPlan{o,descriptor,0x417a2,0x005c,
            0x20c26,0x41bb4,0x20c26};
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandDispatchSetupPlan>
    observe_post_command_dispatch_destination(const DeuterosAmigaObservedTitlePostCommandDispatchDestination& o){
        if(!post_command_pointer_chain_||post_command_dispatch_destination_)return std::nullopt;
        if(o.trace_sequence<=last_command_sequence_||o.instruction_address!=0x41c48
            ||o.source_address!=0x1f168||o.observed_pointer==0)
            throw std::runtime_error("Deuteros post-command dispatch destination does not match boundary");
        const auto low=static_cast<std::uint8_t>(post_command_pointer_chain_->observed_word);
        const auto descriptor=static_cast<std::uint16_t>(low!=0&&(low&0x40U)==0?0x00b0:0x00bd);
        const auto cell=static_cast<std::uint32_t>(0x41faaU+descriptor*4U);
        const auto offset=descriptor==0x00b0?0x0003227cU:0x00034b2aU;
        post_command_dispatch_destination_=o;last_command_sequence_=o.trace_sequence;
        return DeuterosAmigaTitlePostCommandDispatchSetupPlan{o,0x005c,descriptor,0x417a2,
            cell,offset,0x422faU+offset,0,0x00b8,0x1f3e,0x0028,o.observed_pointer,0x41c72,0x41c72};
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandSelectedStreamPlan>
    advance_post_command_selected_stream(){
        if(!post_command_dispatch_destination_||post_command_selected_stream_advanced_)return std::nullopt;
        const auto low=static_cast<std::uint8_t>(post_command_pointer_chain_->observed_word);
        const auto descriptor=static_cast<std::uint16_t>(low!=0&&(low&0x40U)==0?0x00b0:0x00bd);
        const auto is_b0=descriptor==0x00b0;
        auto addresses=is_b0?selected_b0_addresses_:selected_bd_addresses_;
        for(auto& address:addresses)address+=post_command_dispatch_destination_->observed_pointer;
        post_command_selected_stream_advanced_=true;
        descriptor_loop_d5_=descriptor;descriptor_loop_d6_=0x000b;descriptor_loop_iteration_=0;
        // Enter the loop through its first reachable call. `$00bd` has carry
        // set on iteration zero, so it deterministically skips to iteration one.
        descriptor_loop_d5_>>=1U;
        if(descriptor==0x00bd){--descriptor_loop_d6_;++descriptor_loop_iteration_;descriptor_loop_d5_>>=1U;}
        descriptor_call_pending_=true;
        return DeuterosAmigaTitlePostCommandSelectedStreamPlan{descriptor,0x000c,
            static_cast<std::uint16_t>(is_b0?0x0010:0x8010),
            is_b0?0x74576U:0x76e24U,is_b0?0x7457aU:0x76e28U,
            is_b0?0x74aa1U:0x76e2bU,std::move(addresses),
            is_b0?selected_b0_values_:selected_bd_values_,
            is_b0?selected_b0_packet_count_:selected_bd_packet_count_,
            is_b0?selected_b0_family_counts_:selected_bd_family_counts_,
            is_b0?768U:192U,0x41be6,0x20c2c,descriptor,0x000b,0x20c4c,
            is_b0?"67251004cede98024d69fff3b1bac02f7df956aca5422086f00a88825ad1366c":
                "8dca78516efa8b24c5a195cd4427fe196b4e15759c00882aa1a229ae99edd173"};
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandDescriptorCallPlan>
    observe_post_command_descriptor_call_return(const DeuterosAmigaObservedLocalCallReturn& o){
        if(!descriptor_call_pending_||descriptor_call_return_)return std::nullopt;
        if(o.trace_sequence<=last_command_sequence_||o.call_address!=0x20c4c
            ||o.call_target!=0x41ad2||o.return_address!=0x20c52)
            throw std::runtime_error("Deuteros descriptor-loop call return does not match boundary");
        descriptor_call_pending_=false;descriptor_call_return_=o;last_command_sequence_=o.trace_sequence;
        return DeuterosAmigaTitlePostCommandDescriptorCallPlan{o,descriptor_loop_iteration_,
            descriptor_loop_d6_,descriptor_loop_d5_};
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandDescriptorLoopPlan>
    advance_post_command_descriptor_loop(){
        if(!descriptor_call_return_||descriptor_call_pending_||descriptor_loop_completed_)return std::nullopt;
        descriptor_call_return_.reset();
        constexpr std::array<std::uint16_t,24> table{{0x0000,0x0058,0x0002,0x0358,0x0003,0x0068,0x0004,0x0368,0x0005,0x0078,0x0007,0x0378,0x0006,0x0088,0x0001,0x0388,0x0003,0x0098,0x0008,0x0398,0x0026,0x00a8,0x0002,0x03a8}};
        while(descriptor_loop_d6_!=0){--descriptor_loop_d6_;++descriptor_loop_iteration_;
            const auto carry=(descriptor_loop_d5_&1U)!=0;descriptor_loop_d5_>>=1U;
            if(!carry&&descriptor_loop_d6_!=4U&&descriptor_loop_d6_!=5U){descriptor_call_pending_=true;
                return DeuterosAmigaTitlePostCommandDescriptorLoopPlan{false,descriptor_loop_iteration_,descriptor_loop_d6_,descriptor_loop_d5_,0x26,table[descriptor_loop_iteration_*2U+1U],0x20c4c,0x41ad2,0,0,0x20c4c};}
        }
        descriptor_loop_completed_=true;
        return DeuterosAmigaTitlePostCommandDescriptorLoopPlan{true,descriptor_loop_iteration_,0,
            descriptor_loop_d5_,0,0,0,0,0x416b4,0x00bd,0x20c6c};
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandDescriptorBytePlan>
    observe_post_command_descriptor_byte(const DeuterosAmigaObservedTitlePostCommandDescriptorByte& o){
        if(!descriptor_loop_completed_||post_command_descriptor_byte_)return std::nullopt;
        if(o.trace_sequence<=last_command_sequence_||o.instruction_address!=0x20c6c
            ||o.source_address!=0x20a10)
            throw std::runtime_error("Deuteros post-command descriptor byte does not match boundary");
        const auto result=static_cast<std::uint16_t>((0x00bdU+o.observed_value)&0x00ffU);
        post_command_descriptor_byte_=o;last_command_sequence_=o.trace_sequence;
        return DeuterosAmigaTitlePostCommandDescriptorBytePlan{o,0x00bd,result,0x004b,
            0x416b4,0x20c7a,0x41bb4,0x20c7a};
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandAdjustedDispatchPlan>
    observe_post_command_adjusted_dispatch_destination(
        const DeuterosAmigaObservedTitlePostCommandAdjustedDispatchDestination& o){
        if(!post_command_descriptor_byte_||adjusted_dispatch_destination_)return std::nullopt;
        const auto descriptor=static_cast<std::uint16_t>((0x00bdU+post_command_descriptor_byte_->observed_value)&0x00ffU);
        if(descriptor!=0x00c0||o.trace_sequence<=last_command_sequence_||o.instruction_address!=0x41c48
            ||o.source_address!=0x1f168||o.observed_pointer==0||o.observed_pointer>0xffff82ffU)
            throw std::runtime_error("Deuteros adjusted dispatch destination does not match boundary");
        std::vector<std::uint32_t> addresses;addresses.reserve(adjusted_c0_values_.size());
        constexpr std::uint32_t x=0,y=0x88,row_advance=0x28,wrap=0x1f3e;
        const auto threshold=o.observed_pointer+(wrap+2U)*4U;
        const auto subtract=wrap*4U+2U;
        for(std::uint32_t row=0;row<168U;++row){
            auto destination=o.observed_pointer+y*row_advance+x*2U+row*row_advance;
            for(std::uint32_t column=0;column<68U;++column){
                addresses.push_back(destination);addresses.push_back(destination+1U);
                destination+=2U+wrap;if(destination>=threshold)destination-=subtract;
            }
        }
        adjusted_dispatch_destination_=o;last_command_sequence_=o.trace_sequence;
        return DeuterosAmigaTitlePostCommandAdjustedDispatchPlan{o,descriptor,68,168,x,y,row_advance,
            0x416b4,0x770a0,0x770a4,0x78aa7,std::move(addresses),adjusted_c0_values_,
            adjusted_c0_packets_,adjusted_c0_families_,0x20c80,0x20c80,
            "d36e52f85876496cb0123b9c5d4c0ad2bce2b7ecfad168c1c1fe247507060c2d",
            "fbaaa7db8117a83718be8cea03749e455893d2e0feb5e72c74d1158749bc7095",
            "8b535aadc3aaa48055ffaf3ce03339455ebcee3951e737ddedfb62d8b690b840"};
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjustedCallerPointerPlan>
    observe_post_adjusted_caller_pointer(const DeuterosAmigaObservedTitlePostAdjustedCallerPointer&o){
        if(!adjusted_dispatch_destination_||post_adjusted_caller_pointer_)return std::nullopt;
        if(o.trace_sequence<=last_command_sequence_||o.instruction_address!=0x20c80
            ||o.source_address!=0x19d1e||o.observed_pointer>0xfffffeffU)
            throw std::runtime_error("Deuteros post-adjusted caller pointer does not match boundary");
        post_adjusted_caller_pointer_=o;last_command_sequence_=o.trace_sequence;
        const auto zero=o.observed_pointer==0;
        return DeuterosAmigaTitlePostAdjustedCallerPointerPlan{o,zero,0x20c86,0x20cb8,
            zero?0x20cb8U:0x20c88U,
            "457462f38e994a97b0d37b21cbead532d6bfdb685fc3a8c8784cea654422357d"};
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjustedObjectGatePlan>
    observe_post_adjusted_object_gate(const DeuterosAmigaObservedTitlePostAdjustedObjectGate&o){
        if(!post_adjusted_caller_pointer_||post_adjusted_caller_pointer_->observed_pointer==0
            ||post_adjusted_object_gate_)return std::nullopt;
        const auto pointer=post_adjusted_caller_pointer_->observed_pointer;
        if(o.trace_sequence<=last_command_sequence_||o.instruction_address!=0x20c8a
            ||o.first_source_address!=pointer+0xeeU||o.second_source_address!=pointer+0xf0U
            ||o.first_value<8U||o.second_value==0U||o.second_value>=3U)
            throw std::runtime_error("Deuteros post-adjusted object gate does not match qualifying path");
        post_adjusted_object_gate_=o;last_command_sequence_=o.trace_sequence;
        return DeuterosAmigaTitlePostAdjustedObjectGatePlan{o,0x20a6c,0x0009,0x0398,
            0x20ca8,0x41ad2,0x20cae,
            "ee46bb6621a91b5c5055ed0c93b775b7015f12807b939d6f78a8203f558b3195",
            "f366ff0abe5ea96505ad1c30bf834e5da3753159f02fc6892bc39d0f5c1dbc3c"};
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjustedFirstHelperReturnPlan>
    observe_post_adjusted_first_helper_return(const DeuterosAmigaObservedLocalCallReturn&o){
        if(!post_adjusted_object_gate_||post_adjusted_first_helper_return_)return std::nullopt;
        if(o.trace_sequence<=last_command_sequence_||o.call_address!=0x20ca8
            ||o.call_target!=0x41ad2||o.return_address!=0x20cae)
            throw std::runtime_error("Deuteros post-adjusted first helper return does not match boundary");
        post_adjusted_first_helper_return_=o;last_command_sequence_=o.trace_sequence;
        return DeuterosAmigaTitlePostAdjustedFirstHelperReturnPlan{o,0x20a70,0x0017,0x03a8,
            0x20cb2,0x41ad2,0x20cb8,
            "405d4903b7cc8571505f6ed5c89f6f2a7d5fc9ed28d249f73d314932a8758db8"};
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjustedSecondHelperReturnPlan>
    observe_post_adjusted_second_helper_return(const DeuterosAmigaObservedLocalCallReturn&o){
        if(!post_adjusted_first_helper_return_||post_adjusted_second_helper_return_)return std::nullopt;
        if(o.trace_sequence<=last_command_sequence_||o.call_address!=0x20cb2
            ||o.call_target!=0x41ad2||o.return_address!=0x20cb8)
            throw std::runtime_error("Deuteros post-adjusted second helper return does not match boundary");
        post_adjusted_second_helper_return_=o;last_command_sequence_=o.trace_sequence;
        return DeuterosAmigaTitlePostAdjustedSecondHelperReturnPlan{o,0x20cb8,0x20cb8,
            "889c758fbfd514bc3633787bc2736b39a95aa712af79d4dc8f119eee6bbb65ab"};
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjustedRtsFramePlan>
    observe_post_adjusted_rts_frame(const DeuterosAmigaObservedTitlePostAdjustedRtsFrame&o){
        if(!post_adjusted_second_helper_return_||post_adjusted_rts_frame_)return std::nullopt;
        if(o.trace_sequence<=last_command_sequence_||o.instruction_address!=0x20cb8
            ||o.frame_address>0xfffffffbU||o.return_address!=0x40530)
            throw std::runtime_error("Deuteros post-adjusted RTS frame does not match caller provenance");
        post_adjusted_rts_frame_=o;last_command_sequence_=o.trace_sequence;
        return DeuterosAmigaTitlePostAdjustedRtsFramePlan{o,0x40530,0x20ba8,0x40536,
            "8a7c8b9593ae8d101806072aafa8cc8aa91a34dd4802e67af4fd16f3dc56c362"};
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandNestedWordsPlan>
    observe_post_adjusted_repeated_nested_words(
        const DeuterosAmigaObservedTitlePostCommandNestedWords&o){
        if(!post_adjusted_rts_frame_||repeated_nested_words_)return std::nullopt;
        if(o.trace_sequence<=last_command_sequence_
            ||o.instruction_addresses!=std::array<std::uint32_t,2>{{0x20bae,0x20bb6}}
            ||o.source_addresses!=std::array<std::uint32_t,2>{{0x13008,0x202bc}})
            throw std::runtime_error("Deuteros repeated local-service words do not match boundary");
        const auto shifted=static_cast<std::uint16_t>((o.observed_words[0]&0xff00U)
            |((o.observed_words[0]&0xffU)>>1U));
        const bool carry=(o.observed_words[0]&1U)!=0;
        const auto decremented=static_cast<std::uint16_t>((o.observed_words[1]&0xff00U)
            |((o.observed_words[1]-1U)&0xffU));
        repeated_nested_words_=o;last_command_sequence_=o.trace_sequence;
        repeated_d7_=shifted;repeated_d5_=decremented;repeated_d6_=7;
        repeated_iteration_=0;
        repeated_call_pending_=!(carry&&static_cast<std::uint8_t>(decremented)==0);
        repeated_call_address_=carry?0x20be4U:0x20bd6U;
        if(carry&&static_cast<std::uint8_t>(decremented)==0)
            return DeuterosAmigaTitlePostCommandNestedWordsPlan{o,shifted,decremented,
                true,false,0,0,0,0,0,0,0x20bea};
        return DeuterosAmigaTitlePostCommandNestedWordsPlan{o,shifted,decremented,
            carry,!carry&&static_cast<std::uint8_t>(decremented)==0,
            0x202bc,decremented,static_cast<std::uint16_t>(carry?0x0008:0x0048),
            0x0010,repeated_call_address_,post_command_service_route_.nested_branch_target,
            repeated_call_address_};
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandNestedCallReturnPlan>
    observe_post_adjusted_repeated_nested_call_return(
        const DeuterosAmigaObservedLocalCallReturn&o){
        if(!repeated_nested_words_||!repeated_call_pending_)return std::nullopt;
        const auto return_address=repeated_call_address_==0x20bd6U?0x20bdcU:0x20beaU;
        if(o.trace_sequence<=last_command_sequence_||o.call_address!=repeated_call_address_
            ||o.call_target!=post_command_service_route_.nested_branch_target
            ||o.return_address!=return_address)
            throw std::runtime_error("Deuteros repeated local-service call return does not match boundary");
        repeated_call_pending_=false;last_command_sequence_=o.trace_sequence;
        return DeuterosAmigaTitlePostCommandNestedCallReturnPlan{o,repeated_d7_,
            repeated_d6_,0x20bea};
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandNestedLoopAdvancePlan>
    advance_post_adjusted_repeated_nested_loop(){
        if(!repeated_nested_words_||repeated_call_pending_||repeated_loop_completed_)
            return std::nullopt;
        if(repeated_d6_==0){repeated_d6_=0xffff;repeated_loop_completed_=true;
            return DeuterosAmigaTitlePostCommandNestedLoopAdvancePlan{8,repeated_d7_,
                repeated_d5_,repeated_d6_,false,false,true,0,0,0,0,0,0,0x40536};}
        --repeated_d6_;++repeated_iteration_;
        const std::array<std::uint16_t,16> table{{0x0008,0x0010,0x0011,0x0310,
            0x001a,0x0020,0x0023,0x0320,0x002c,0x0030,0x0035,0x0330,
            0x003e,0x0040,0x0047,0x0340}};
        const auto table_index=static_cast<std::size_t>(repeated_iteration_)*2U;
        const bool carry=(repeated_d7_&1U)!=0;
        repeated_d7_=static_cast<std::uint16_t>((repeated_d7_&0xff00U)
            |((repeated_d7_&0xffU)>>1U));
        repeated_d5_=static_cast<std::uint16_t>((repeated_d5_&0xff00U)
            |((repeated_d5_-1U)&0xffU));
        const bool skip=carry&&static_cast<std::uint8_t>(repeated_d5_)==0;
        repeated_call_pending_=!skip;
        repeated_call_address_=carry?0x20be4U:0x20bd6U;
        const bool writes=!carry&&static_cast<std::uint8_t>(repeated_d5_)==0;
        return DeuterosAmigaTitlePostCommandNestedLoopAdvancePlan{repeated_iteration_,
            repeated_d7_,repeated_d5_,repeated_d6_,carry,writes,false,0x202bc,
            repeated_d5_,static_cast<std::uint16_t>(carry?table[table_index]:0x4fU-repeated_d6_),
            table[table_index+1U],skip?0U:repeated_call_address_,
            post_command_service_route_.nested_branch_target,
            skip?0x20beaU:repeated_call_address_};
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjustedCallerIndirectPlan>
    advance_post_adjusted_caller_indirect(){
        if(!repeated_loop_completed_||post_adjusted_caller_indirect_advanced_)return std::nullopt;
        post_adjusted_caller_indirect_advanced_=true;
        return DeuterosAmigaTitlePostAdjustedCallerIndirectPlan{0x40536,0x20cfe,0x4053c,
            0x20cfe,0x4053e,"58b17754e42e00bee2c320083fbe09c0fe79b0bda626b71f98fc043598033752"};
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjustedIndirectReturnPlan>
    observe_post_adjusted_caller_indirect_return(
        const DeuterosAmigaObservedTitlePostAdjustedIndirectReturn&o){
        if(!post_adjusted_caller_indirect_advanced_||post_adjusted_indirect_return_)return std::nullopt;
        if(o.trace_sequence<=last_command_sequence_||o.call_address!=0x4053c
            ||o.call_target!=0x20cfe||o.return_address!=0x4053e
            ||o.source_address!=0x12fe4)
            throw std::runtime_error("Deuteros post-adjusted indirect return does not match boundary");
        post_adjusted_indirect_return_=o;last_command_sequence_=o.trace_sequence;
        const auto shifted=o.source_long>>3U;
        return DeuterosAmigaTitlePostAdjustedIndirectReturnPlan{o,shifted,0x1f42a,
            static_cast<std::uint16_t>(shifted),0x4054c,0x37180,0x40552,
            "58b17754e42e00bee2c320083fbe09c0fe79b0bda626b71f98fc043598033752"};
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjusted37180ReturnPlan>
    observe_post_adjusted_caller_37180_return(const DeuterosAmigaObservedTitlePostAdjusted37180Return&o){
        if(!post_adjusted_indirect_return_||post_adjusted_37180_return_)return std::nullopt;
        if(o.trace_sequence<=last_command_sequence_||o.call_address!=0x4054c||o.call_target!=0x37180
            ||o.return_address!=0x40552||o.source_address!=0x1378e||o.mode_address!=0x4040e)
            throw std::runtime_error("Deuteros $37180 return does not match caller boundary");
        post_adjusted_37180_return_=o;last_command_sequence_=o.trace_sequence;
        const bool five=o.mode_word==5;
        return DeuterosAmigaTitlePostAdjusted37180ReturnPlan{o,0x1c26c,o.source_long,0x40564,
            five?0x40566U:0x4056eU,five?0x36a8cU:0x1fb9aU,five?0x4056cU:0x40574U,five,
            "a208f64d43c08c1363f67586f924a5a7ae8143a8b40e691097ef3c29503666c3"};
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjustedModeReturnPlan> observe_post_adjusted_mode_return(const DeuterosAmigaObservedLocalCallReturn&o){
        if(!post_adjusted_37180_return_||post_adjusted_mode_return_)return std::nullopt;
        const bool five=post_adjusted_37180_return_->mode_word==5;
        if(o.trace_sequence<=last_command_sequence_||o.call_address!=(five?0x40566U:0x4056eU)
            ||o.call_target!=(five?0x36a8cU:0x1fb9aU)||o.return_address!=(five?0x4056cU:0x40574U))
            throw std::runtime_error("Deuteros selected mode return does not match boundary");
        post_adjusted_mode_return_=o;last_command_sequence_=o.trace_sequence;
        return DeuterosAmigaTitlePostAdjustedModeReturnPlan{o,0x40574,0x40574,0x222c0,0x4057a,five};
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
    DeuterosAmigaTitlePostExecTailReturnContinuationProfile post_command_continuation_;
    DeuterosAmigaTitlePostExecPointerRouteProfile post_command_pointer_route_;
    DeuterosAmigaTitlePostExecServiceRouteProfile post_command_service_route_;
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
    std::optional<DeuterosAmigaObservedTitlePostCommandPointerRoute>
        observed_post_command_pointer_route_;
    std::optional<DeuterosAmigaObservedGraphicsVectorReturn>
        observed_post_command_graphics_return_;
    bool post_command_first_dispatch_advanced_ = false;
    std::optional<DeuterosAmigaObservedTitleFirstDispatchHeader>
        observed_first_dispatch_header_;
    bool first_dispatch_packet_advanced_ = false;
    bool first_dispatch_decode_advanced_ = false;
    bool first_dispatch_caller_tail_advanced_ = false;
    bool first_dispatch_merge_observed_ = false;
    std::vector<std::uint32_t> first_dispatch_decode_addresses_;
    std::vector<std::uint8_t> first_dispatch_decode_values_;
    std::uint32_t first_dispatch_packet_count_=0;
    std::array<std::uint32_t,4> first_dispatch_family_counts_{};
    bool second_dispatch_advanced_ = false;
    bool second_dispatch_decode_advanced_ = false;
    bool second_dispatch_merge_observed_ = false;
    std::vector<std::uint32_t> second_dispatch_decode_addresses_;
    std::vector<std::uint8_t> second_dispatch_decode_values_;
    std::uint32_t second_dispatch_packet_count_=0;
    std::array<std::uint32_t,4> second_dispatch_family_counts_{};
    bool post_command_service_route_prefix_advanced_ = false;
    std::optional<DeuterosAmigaObservedLocalCallReturn> post_command_service_first_return_;
    std::optional<DeuterosAmigaObservedLocalCallReturn> post_command_service_second_return_;
    std::optional<DeuterosAmigaObservedLocalCallReturn> post_command_service_third_return_;
    std::optional<DeuterosAmigaObservedTitlePostCommandNestedWords> post_command_nested_words_;
    std::uint16_t nested_d7_=0,nested_d5_=0,nested_d6_=0,nested_iteration_=0;
    bool nested_call_pending_=false,nested_loop_completed_=false;
    std::uint32_t nested_call_address_=0;
    std::optional<DeuterosAmigaObservedLocalCallReturn> post_command_continuation_return_;
    std::optional<DeuterosAmigaObservedTitlePostCommandPointerChain> post_command_pointer_chain_;
    std::optional<DeuterosAmigaObservedTitlePostCommandDispatchDestination> post_command_dispatch_destination_;
    bool post_command_selected_stream_advanced_=false;
    std::vector<std::uint32_t> selected_b0_addresses_,selected_bd_addresses_;
    std::vector<std::uint8_t> selected_b0_values_,selected_bd_values_;
    std::uint32_t selected_b0_packet_count_=0,selected_bd_packet_count_=0;
    std::array<std::uint32_t,4> selected_b0_family_counts_{},selected_bd_family_counts_{};
    std::uint16_t descriptor_loop_d5_=0,descriptor_loop_d6_=0,descriptor_loop_iteration_=0;
    bool descriptor_call_pending_=false,descriptor_loop_completed_=false;
    std::optional<DeuterosAmigaObservedLocalCallReturn> descriptor_call_return_;
    std::optional<DeuterosAmigaObservedTitlePostCommandDescriptorByte> post_command_descriptor_byte_;
    std::optional<DeuterosAmigaObservedTitlePostCommandAdjustedDispatchDestination> adjusted_dispatch_destination_;
    std::optional<DeuterosAmigaObservedTitlePostAdjustedCallerPointer> post_adjusted_caller_pointer_;
    std::optional<DeuterosAmigaObservedTitlePostAdjustedObjectGate> post_adjusted_object_gate_;
    std::optional<DeuterosAmigaObservedLocalCallReturn> post_adjusted_first_helper_return_;
    std::optional<DeuterosAmigaObservedLocalCallReturn> post_adjusted_second_helper_return_;
    std::optional<DeuterosAmigaObservedTitlePostAdjustedRtsFrame> post_adjusted_rts_frame_;
    std::optional<DeuterosAmigaObservedTitlePostCommandNestedWords> repeated_nested_words_;
    std::uint16_t repeated_d7_=0,repeated_d5_=0,repeated_d6_=0,repeated_iteration_=0;
    bool repeated_call_pending_=false,repeated_loop_completed_=false;
    std::uint32_t repeated_call_address_=0;
    bool post_adjusted_caller_indirect_advanced_=false;
    std::optional<DeuterosAmigaObservedTitlePostAdjustedIndirectReturn> post_adjusted_indirect_return_;
    std::optional<DeuterosAmigaObservedTitlePostAdjusted37180Return> post_adjusted_37180_return_;
    std::optional<DeuterosAmigaObservedLocalCallReturn> post_adjusted_mode_return_;
    std::vector<std::uint8_t> adjusted_c0_values_;
    std::uint32_t adjusted_c0_packets_=0;
    std::array<std::uint32_t,4> adjusted_c0_families_{};
    struct PendingCommandCall {
        std::uint32_t address;
        std::uint32_t target;
        std::uint32_t return_address;
        std::uint8_t opcode;
    };
    std::optional<PendingCommandCall> pending_command_call_;
    std::optional<DeuterosAmigaObservedTitleCommandTwoOperandMode>
        command_two_operand_mode_;
    std::optional<DeuterosAmigaObservedTitleCommandTwoOperands>
        command_two_operands_;
    std::uint32_t command_repeat_character_address_ = 0;
    std::uint16_t command_repeat_iterations_ = 0;
    std::uint16_t command_repeat_initial_iterations_ = 0;
    std::uint32_t command_high_table_address_ = 0;
    std::uint8_t command_high_phase_ = 0;
};

} // namespace eon
