#pragma once

#include "data/amiga_adf.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace eon {

struct AmigaLoadStage {
    std::uint32_t disk_offset = 0;
    std::uint32_t length = 0;
    std::uint32_t destination = 0;
    std::uint32_t entry_address = 0;
};

// One of the bootstrap routines selected through the six-entry table at
// $12a36.  These routines only provide read constants; unlike profile zero,
// a later profile is not assumed to begin with an absolute JMP.
struct DeuterosAmigaBootstrapProfile {
    std::uint32_t disk_offset = 0;
    std::uint32_t length = 0;
    std::uint32_t destination = 0;
};

// Facts recovered from the first straight-line and loop setup portion of the
// raw main stage.  These are addresses and literal values only: they do not
// assign gameplay meaning to the state cells or service calls.
struct DeuterosAmigaMainStageEntry {
    std::uint32_t entry_address = 0;
    std::uint32_t incoming_controller_cell = 0;
    std::uint32_t incoming_mode_cell = 0;
    std::uint32_t stack_address = 0;
    std::uint32_t memory_ceiling = 0;
    std::array<std::uint32_t, 2> initialization_calls{};
    std::uint32_t loop_address = 0;
    std::uint32_t loop_first_service_address = 0;
    std::uint32_t loop_scheduler_address = 0;
    std::uint32_t first_state_word_address = 0;
    std::uint32_t second_state_word_address = 0;
    std::uint32_t scheduler_enable_word_address = 0;
    std::uint16_t scheduler_enable_word_value = 0;
    std::uint32_t first_input_address = 0;
    std::uint8_t first_input_bit = 0;
    std::uint32_t second_input_address = 0;
    std::uint8_t second_input_bit = 0;
    // The > two route returns to $2181c, which enters the genuine 24-byte
    // channel scheduler at $21380.  These facts identify its state layout
    // and the four wait selectors proven by the raw code.  They are not a
    // host timing model and do not assign a gameplay meaning to a selector.
    std::uint32_t scheduler_state_base_address = 0;
    std::uint32_t scheduler_channel_count_address = 0;
    std::uint16_t scheduler_channel_stride = 0;
    std::uint16_t scheduler_active_program_offset = 0;
    std::uint16_t scheduler_wait_selector_offset = 0;
    std::uint16_t scheduler_wait_value_offset = 0;
    std::array<std::uint8_t, 4> scheduler_wait_selectors{};
    // After all channel slots, the original probes this custom register bit
    // and conditionally invokes a raw service.  Preserve that ordering as a
    // bounded scheduler-tail fact, without labelling the service.
    std::uint32_t scheduler_tail_probe_address = 0;
    std::uint8_t scheduler_tail_probe_bit = 0;
    std::uint32_t scheduler_tail_service_address = 0;
    // The first input-originated branch after the recurring loop.  The
    // original compares the word at input_dispatch_state_address with two;
    // values below two are written back as input_dispatch_clamped_value and
    // enter input_dispatch_service_address.  Two enters that same service;
    // greater values branch back to input_dispatch_continue_address.  These
    // are raw dispatch facts, not names for modes or menus.
    std::uint32_t input_dispatch_address = 0;
    std::uint32_t input_dispatch_state_address = 0;
    std::uint16_t input_dispatch_compare_value = 0;
    std::uint16_t input_dispatch_clamped_value = 0;
    std::uint32_t input_dispatch_service_address = 0;
    std::uint32_t input_dispatch_continue_address = 0;
    // The service entered by values <= two reads this same word, increments
    // it, and selects one of two fully verified exits.  The first sets a
    // bootstrap-profile cell and returns; the second enters a polling loop.
    // These remain control-flow facts, rather than names for a screen or a
    // game mode.
    std::uint32_t dispatch_service_state_address = 0;
    std::uint16_t dispatch_service_first_exit_value = 0;
    std::uint32_t dispatch_service_first_exit_address = 0;
    std::uint16_t dispatch_service_second_exit_value = 0;
    std::uint32_t dispatch_service_second_exit_address = 0;
    std::uint32_t first_exit_profile_cell_address = 0;
    std::uint32_t first_exit_profile_value = 0;
    std::uint32_t bootstrap_controller_return_cell = 0;
    std::uint32_t bootstrap_profile_return_cell = 0;
    std::uint32_t second_exit_profile_cell_address = 0;
    std::uint32_t second_exit_initial_profile_value = 0;
    std::uint32_t second_exit_service_address = 0;
    std::uint32_t second_exit_service_match_value = 0;
    std::uint32_t second_exit_matched_return_address = 0;
    // The raw resource loader at $21932 takes a D0 table index, scales it by
    // four, and obtains a disk offset from $21708. It first transfers four
    // bytes at that offset to resource_probe_address. If the recovered
    // big-endian longword is nonzero, it uses that literal as the next
    // transfer length and transfers from the same offset to
    // resource_payload_address. These are transfer facts only; neither the
    // resource format nor the destination-memory meaning is inferred.
    std::uint32_t resource_loader_address = 0;
    std::uint32_t resource_table_address = 0;
    std::uint8_t resource_index_scale_shift = 0;
    std::uint32_t resource_probe_address = 0;
    std::uint32_t resource_payload_address = 0;
    std::uint32_t resource_transfer_address = 0;
    std::uint32_t resource_transfer_chunk_length = 0;
    std::uint32_t resource_retry_probe_address = 0;
    std::uint8_t resource_retry_probe_bit = 0;
    std::uint32_t resource_retry_address = 0;
    // A separately verified consumer in the same main-stage image enters at
    // resource_consumer_address. It loads A4 from the exact payload
    // destination above, combines a 16-bit seed with the low word of a
    // counter, masks the result, reads one big-endian word, adds the literal
    // below, and adds that result back into the seed. The two dispatcher
    // sites recorded here reach it for literal command words; these are raw
    // control/data-flow facts, not a name for the bytes or their effect.
    std::uint32_t resource_consumer_address = 0;
    std::uint32_t resource_consumer_base_address = 0;
    std::uint32_t resource_consumer_seed_address = 0;
    std::uint32_t resource_consumer_counter_address = 0;
    std::uint16_t resource_consumer_index_mask = 0;
    std::uint16_t resource_consumer_word_addend = 0;
    std::array<std::uint16_t, 2> resource_consumer_command_words{};
    std::array<std::uint32_t, 2> resource_consumer_call_sites{};
    // Selector $fe does not use the regular indexed-bitmap route. The render
    // pass loads its byte-stream pointer from state +12 and calls this exact
    // original routine. Its global display state remains deliberately raw.
    std::uint32_t renderer_pass_address = 0;
    std::uint16_t alternate_renderer_selector = 0;
    std::uint16_t alternate_renderer_state_data_offset = 0;
    std::uint32_t alternate_renderer_address = 0;
    std::uint32_t regular_renderer_address = 0;
    // Command $10 is not itself a title-stage handoff. Its dispatcher arm
    // stores the literal below in this main-loop cell; the loop tests the
    // cell and branches to the recorded raw continuation.
    std::uint32_t channel_request_cell_address = 0;
    std::uint16_t channel_request_value = 0;
    std::uint32_t channel_request_loop_test_address = 0;
    std::uint32_t channel_request_loop_branch_address = 0;
    std::uint32_t channel_request_continuation_address = 0;
};

struct DeuterosAmigaLoadPlan {
    AmigaLoadStage bootstrap_loader;
    AmigaLoadStage main_stage;
    DeuterosAmigaMainStageEntry main_stage_entry;
    std::array<std::uint32_t, 5> resource_disk_offsets{};
    // The title's accepted-input path writes profile one to $12ffc before it
    // returns to this bootstrap. Retain the real load constants so callers
    // can hand off without guessing an unpacked game executable.
    DeuterosAmigaBootstrapProfile title_handoff_profile;
    // Profile one is itself a raw, relocatable 68000 stage.  Its first word
    // is an absolute JMP, so this supplies the real entry address without
    // treating the disk bytes as an unpacked host-side executable.
    AmigaLoadStage title_stage;
};

// Static facts from the nonzero arm of the main loop's verified channel-request
// edge. This never chooses an arm, invokes a service, or simulates its input
// port; it preserves only byte-addressed control-flow evidence.
struct DeuterosAmigaChannelRequestContinuation {
    std::uint32_t entry_address = 0;
    std::uint32_t first_longword_address = 0;
    std::uint32_t first_zero_branch_address = 0;
    std::uint32_t first_zero_branch_target = 0;
    std::array<std::uint32_t, 2> local_call_addresses{};
    std::array<std::uint32_t, 2> local_call_targets{};
    std::uint32_t following_call_address = 0;
    std::uint32_t following_call_target = 0;
    std::uint32_t repeated_longword_address = 0;
    std::uint32_t equal_branch_address = 0;
    std::uint32_t equal_branch_target = 0;
    std::uint32_t later_call_address = 0;
    std::uint32_t later_call_target = 0;
    std::uint32_t input_test_address = 0;
    std::uint8_t input_test_bit = 0;
    std::uint32_t input_zero_branch_address = 0;
    std::uint32_t input_zero_branch_target = 0;
    std::uint32_t final_branch_address = 0;
    std::uint32_t final_branch_target = 0;
    std::string raw_sha256;
};

// First static callee reached by the channel-request continuation. Its custom
// register polling and library-vector calls are retained as byte facts only.
struct DeuterosAmigaChannelRequestFirstCallee {
    std::uint32_t entry_address = 0;
    std::uint32_t first_word_address = 0;
    std::uint16_t first_word_value = 0;
    std::uint32_t cleared_byte_address = 0;
    std::uint32_t input_test_address = 0;
    std::uint8_t input_test_bit = 0;
    std::uint32_t input_zero_branch_address = 0;
    std::uint32_t input_zero_branch_target = 0;
    std::uint16_t loop_initial_counter = 0;
    std::uint32_t loop_branch_address = 0;
    std::uint32_t loop_branch_target = 0;
    std::uint32_t vector_base_address = 0;
    std::array<std::uint32_t, 2> vector_call_addresses{};
    std::array<std::uint32_t, 2> vector_a0_addresses{};
    std::uint32_t final_word_address = 0;
    std::uint16_t final_subtract_value = 0;
    std::uint32_t final_nonzero_branch_address = 0;
    std::uint32_t final_nonzero_branch_target = 0;
    std::array<std::uint32_t, 2> final_service_call_addresses{};
    std::uint32_t final_service_target = 0;
    std::uint32_t return_address = 0;
    std::string raw_sha256;
};

// Second static callee reached by the channel-request continuation. All of
// its instructions encode low-memory or custom-register writes, so these are
// provenance facts only and are never applied by Project Eon.
struct DeuterosAmigaChannelRequestSecondCallee {
    std::uint32_t entry_address = 0;
    std::uint32_t copied_longword_source_address = 0;
    std::uint32_t copied_longword_destination_address = 0;
    std::array<std::uint32_t, 4> cleared_word_addresses{};
    std::uint16_t final_word_value = 0;
    std::uint32_t final_word_address = 0;
    std::uint32_t return_address = 0;
    std::string raw_sha256;
};

// Static, return-bounded service reached after both channel-request callees.
// It contains embedded descriptor data and writes runtime cells; Project Eon
// retains the encodings and terminal RTS without applying any write.
struct DeuterosAmigaChannelRequestFollowingService {
    std::uint32_t entry_address = 0;
    std::uint32_t initialized_byte_address = 0;
    std::uint8_t initialized_byte_value = 0;
    std::uint16_t initial_mask_value = 0;
    std::uint32_t execution_entry_address = 0;
    std::uint32_t embedded_table_address = 0;
    std::uint32_t descriptor_base_address = 0;
    std::uint16_t descriptor_stride = 0;
    std::uint32_t source_record_address = 0;
    std::uint32_t source_payload_addend = 0;
    std::uint32_t flag_cell_address = 0;
    std::array<std::uint8_t, 4> flag_write_values{};
    std::uint32_t return_address = 0;
    std::string raw_sha256;
};

// Adjacent caller-state-dependent entry after the following service's RTS.
// Its pointer reads and conditional copies are raw encodings only.
struct DeuterosAmigaChannelRequestAdjacentEntry {
    std::uint32_t entry_address = 0;
    std::uint32_t tested_byte_address = 0;
    std::uint32_t zero_branch_address = 0;
    std::uint32_t zero_branch_target = 0;
    std::uint32_t early_return_address = 0;
    std::uint16_t multiply_immediate = 0;
    std::uint32_t pointer_cell_address = 0;
    std::uint32_t negative_branch_address = 0;
    std::uint32_t negative_branch_target = 0;
    std::uint32_t descriptor_base_address = 0;
    std::uint16_t copy_field_offset = 0;
    std::uint16_t descriptor_stride = 0;
    std::array<std::uint32_t, 4> shift_addresses{};
    std::array<std::uint32_t, 4> carry_clear_branch_addresses{};
    std::array<std::uint32_t, 4> carry_clear_branch_targets{};
    std::array<std::uint32_t, 4> copy_instruction_addresses{};
    std::uint32_t final_return_address = 0;
    std::string raw_sha256;
};

// A read-only representation of one completed pass through the main stage's
// resource loader at $21932. `payload` is copied only into this host-memory
// value: neither the supplied ADF nor the user's data directory is modified.
// The bytes deliberately include the four-byte length word because the
// original second transfer starts at the same source offset as its probe.
struct DeuterosAmigaMainResourceTransfer {
    std::uint16_t resource_index = 0;
    std::uint32_t source_disk_offset = 0;
    std::uint32_t probe_destination_address = 0;
    std::uint32_t payload_destination_address = 0;
    std::uint32_t payload_length = 0;
    std::vector<std::uint8_t> payload;
};

// One strictly read-only execution of the word lookup in $2016a against a
// completed $21932 transfer. `table_offset` is a byte offset from the exact
// payload destination ($32a24), not a host-side record number. The original
// mask $3ffe limits it to the first 16 KiB and makes every access aligned.
struct DeuterosAmigaResourceConsumerSample {
    std::uint16_t seed_before = 0;
    std::uint32_t counter = 0;
    std::uint16_t table_offset = 0;
    std::uint16_t sampled_word = 0;
    std::uint16_t addend_result = 0;
    std::uint16_t seed_after = 0;
};

// Decode load constants from the genuine 68000 instructions. Every expected
// opcode is checked before its immediate value is accepted.
[[nodiscard]] DeuterosAmigaLoadPlan parse_deuteros_amiga_load_plan(const AmigaAdf& disk);

// Hash-validates the raw main-stage continuation reached by the channel-request
// branch. It reports static bytes only and never executes the recorded calls.
[[nodiscard]] DeuterosAmigaChannelRequestContinuation
parse_deuteros_amiga_channel_request_continuation(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan);

// Validates the first static channel-request callee and records no effects,
// vector invocation, input-port result, or service return.
[[nodiscard]] DeuterosAmigaChannelRequestFirstCallee
parse_deuteros_amiga_channel_request_first_callee(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    const DeuterosAmigaChannelRequestContinuation& continuation);

// Validates the second static channel-request callee. It reports encodings
// only; no low-memory/custom-register effects are simulated.
[[nodiscard]] DeuterosAmigaChannelRequestSecondCallee
parse_deuteros_amiga_channel_request_second_callee(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    const DeuterosAmigaChannelRequestContinuation& continuation);

// Validates the returned following service through its terminal RTS. The
// embedded data, branching, descriptor copies and flags remain static facts.
[[nodiscard]] DeuterosAmigaChannelRequestFollowingService
parse_deuteros_amiga_channel_request_following_service(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    const DeuterosAmigaChannelRequestContinuation& continuation);

[[nodiscard]] DeuterosAmigaChannelRequestAdjacentEntry
parse_deuteros_amiga_channel_request_adjacent_entry(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    const DeuterosAmigaChannelRequestFollowingService& service);

// Models one exact, successful loader pass in memory. A zero probe is the
// original retry path, so it returns std::nullopt instead of inventing a
// payload. A nonzero length must fit in the physical original ADF; malformed
// media is rejected before any host-memory representation is returned.
[[nodiscard]] std::optional<DeuterosAmigaMainResourceTransfer>
read_deuteros_amiga_main_resource(const AmigaAdf& disk,
    const DeuterosAmigaLoadPlan& plan, std::uint16_t resource_index);

// Reproduce $2016a's bounded arithmetic using only a transfer already read
// from original media. It validates ownership, the leading length word, and
// the exact two-byte source range. It never writes source media or saves.
[[nodiscard]] DeuterosAmigaResourceConsumerSample
sample_deuteros_amiga_main_resource_consumer(
    const DeuterosAmigaMainResourceTransfer& transfer,
    const DeuterosAmigaMainStageEntry& entry,
    std::uint16_t seed, std::uint32_t counter);

} // namespace eon
