#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace eon {

enum class MillenniumDosTitleInitializationState {
    awaiting_entry,
    private_interrupt_result_boundary,
    selected_local_call_boundary,
    selected_callee_private_interrupt_result_boundary,
    selected_followup_call_boundary,
    bios_palette_interrupt_boundary,
    title_main_allocation_call_boundary,
    dos_resize_result_boundary,
    dos_large_allocation_result_boundary,
    dos_free_result_boundary,
    dos_first_buffer_allocation_result_boundary,
    dos_second_buffer_allocation_result_boundary,
    dos_file_open_result_boundary,
    dos_file_seek_result_boundary,
    dos_file_close_result_boundary,
    dos_file_sized_allocation_result_boundary,
    dos_single_paragraph_allocation_result_boundary,
    dos_scratch_allocation_result_boundary,
    dos_scratch_free_result_boundary,
    dos_library_open_result_boundary,
    dos_library_read_result_boundary,
    dos_library_close_result_boundary,
    library_relocation_complete,
    library_palette_copy_boundary,
    post_library_setup_call_boundary,
    dos_get_vector_zero_result_boundary,
    dos_set_vector_zero_result_boundary,
    dos_get_vector_four_result_boundary,
    dos_set_vector_four_result_boundary,
    bios_int15_first_result_boundary,
    bios_int15_second_result_boundary,
    post_library_next_setup_call_boundary,
    post_library_followup_call_boundary,
    timer_vector_far_read_boundary,
    post_vector_hook_call_boundary,
    video_vector_far_read_boundary,
    post_video_hook_mode_call_boundary,
    post_video_setup_call_boundary,
    post_video_graphics_call_boundary,
    post_video_private_interrupt_result_boundary,
    post_video_followup_call_boundary,
    graphics_descriptor_far_read_boundary,
    graphics_record_word_read_boundary,
    graphics_record_second_word_read_boundary,
    graphics_record_third_word_read_boundary,
    graphics_record_byte_read_boundary,
    graphics_record_second_byte_read_boundary,
    graphics_record_private_interrupt_result_boundary,
    post_descriptor_private_interrupt_result_boundary,
    post_descriptor_first_loop_far_read_boundary,
    post_descriptor_first_loop_record_word_read_boundary,
    post_descriptor_first_loop_second_word_read_boundary,
    post_descriptor_first_loop_third_word_read_boundary,
    post_descriptor_first_loop_byte_read_boundary,
    post_descriptor_first_loop_second_byte_read_boundary,
    post_descriptor_first_loop_encoded_payload_byte_boundary,
    post_descriptor_first_loop_encoded_stream_byte_boundary,
    post_descriptor_first_loop_encoded_escape_word_boundary,
    post_descriptor_first_loop_encoded_mode_two_word_boundary,
    post_descriptor_first_loop_encoded_mode_two_escape_word_boundary,
    post_descriptor_first_loop_encoded_mode_two_second_escape_word_boundary,
    post_descriptor_first_loop_encoded_xlat_byte_boundary,
    post_descriptor_first_loop_encoded_high_nibble_byte_boundary,
    post_descriptor_first_loop_encoded_high_escape_word_boundary,
    post_descriptor_first_loop_encoded_high_mode_two_word_boundary,
    post_descriptor_first_loop_encoded_high_xlat_byte_boundary,
    post_descriptor_first_loop_encoded_record_complete,
    post_descriptor_second_loop_far_read_boundary,
    post_descriptor_second_loop_record_word_read_boundary,
    post_descriptor_second_loop_second_word_read_boundary,
    post_descriptor_second_loop_third_word_read_boundary,
    post_descriptor_second_loop_byte_read_boundary,
    dos_file_failure_boundary,
    allocation_failure_boundary,
};

enum class MillenniumDosTitleInitializationEffectWidth { byte, word };

struct MillenniumDosTitlePrivateInterruptResultObservation {
    std::uint64_t sequence = 0;
    std::uint16_t interrupt_address = 0;
    std::uint16_t return_address = 0;
    std::uint16_t ax = 0;
    std::uint16_t flags = 0;
    // Function $001a is admitted with the raw ten-byte request record that
    // occupies CS:$0fdf..$0fe8. Other private-ABI observations leave this
    // provenance empty.
    std::uint16_t record_segment = 0;
    std::uint16_t record_offset = 0;
    std::vector<std::uint8_t> record_bytes;
};

struct MillenniumDosTitleSelectedCalleeResultObservation {
    std::uint64_t sequence = 0;
    std::uint16_t interrupt_address = 0;
    std::uint16_t wrapper_return_address = 0;
    std::uint16_t selected_callee_return_address = 0;
    std::uint16_t ax = 0;
    std::uint16_t flags = 0;
};

struct MillenniumDosTitleBiosInterruptBoundary {
    std::uint16_t selected_followup_call_address = 0;
    std::uint16_t selected_followup_call_target = 0;
    std::uint16_t interrupt_address = 0;
    std::uint8_t interrupt = 0;
    std::uint16_t ax = 0;
    std::uint16_t bx = 0;
    std::uint16_t cx = 0;
    std::uint16_t dx_known_mask = 0;
    std::uint16_t dx_known_value = 0;
    std::uint16_t source_address = 0;
    bool result_observed = false;
};

struct MillenniumDosTitleBiosResultObservation {
    std::uint64_t sequence = 0;
    std::uint16_t interrupt_address = 0;
    std::uint16_t return_address = 0;
    std::uint16_t ax = 0;
    std::uint16_t flags = 0;
};

struct MillenniumDosTitleBiosResultRecord {
    std::uint64_t sequence = 0;
    std::uint16_t interrupt_address = 0;
    std::uint16_t return_address = 0;
    std::uint16_t ax = 0;
    std::uint16_t flags = 0;
};

struct MillenniumDosTitleDosResultObservation {
    std::uint64_t sequence = 0;
    std::uint16_t interrupt_address = 0;
    std::uint16_t return_address = 0;
    bool carry = false;
    std::uint16_t ax = 0;
    std::uint16_t bx = 0;
    std::uint16_t flags = 0;
};

struct MillenniumDosTitleDosResultRecord {
    std::uint64_t sequence = 0;
    std::uint16_t interrupt_address = 0;
    std::uint16_t return_address = 0;
    bool carry = false;
    std::uint16_t ax = 0;
    std::uint16_t bx = 0;
    std::uint16_t flags = 0;
};

struct MillenniumDosTitleDosFileResultObservation {
    std::uint64_t sequence = 0;
    std::uint16_t interrupt_address = 0;
    std::uint16_t return_address = 0;
    bool carry = false;
    std::uint16_t ax = 0;
    std::uint16_t bx = 0;
    std::uint16_t cx = 0;
    std::uint16_t dx = 0;
    std::uint16_t flags = 0;
};

struct MillenniumDosTitleDosFileResultRecord {
    std::uint64_t sequence = 0;
    std::uint16_t interrupt_address = 0;
    std::uint16_t return_address = 0;
    bool carry = false;
    std::uint16_t ax = 0;
    std::uint16_t bx = 0;
    std::uint16_t cx = 0;
    std::uint16_t dx = 0;
    std::uint16_t flags = 0;
};

struct MillenniumDosTitleDosVectorResultObservation {
    std::uint64_t sequence = 0;
    std::uint16_t interrupt_address = 0;
    std::uint16_t return_address = 0;
    std::uint16_t ax = 0;
    std::uint16_t bx = 0;
    std::uint16_t es = 0;
    std::uint16_t flags = 0;
};

struct MillenniumDosTitleDosVectorResultRecord {
    std::uint64_t sequence = 0;
    std::uint16_t interrupt_address = 0;
    std::uint16_t return_address = 0;
    std::uint16_t ax = 0;
    std::uint16_t bx = 0;
    std::uint16_t es = 0;
    std::uint16_t flags = 0;
};

struct MillenniumDosTitleSetupBiosResultObservation {
    std::uint64_t sequence = 0;
    std::uint16_t interrupt_address = 0;
    std::uint16_t return_address = 0;
    std::uint16_t ax = 0;
    std::uint16_t bx = 0;
    std::uint16_t flags = 0;
};

struct MillenniumDosTitleSetupBiosBoundary {
    std::uint16_t interrupt_address = 0;
    std::uint16_t return_address = 0;
    std::uint8_t interrupt = 0;
    std::uint16_t ax_known_mask = 0;
    std::uint16_t ax_known_value = 0;
    std::uint16_t bx_known_mask = 0;
    std::uint16_t bx_known_value = 0;
    bool result_observed = false;
};

struct MillenniumDosTitleFarReadBoundary {
    std::uint16_t instruction_address = 0;
    std::uint16_t source_segment = 0;
    std::uint16_t source_offset = 0;
    std::uint16_t word_count = 0;
    std::uint16_t destination_segment = 0;
    std::uint16_t destination_offset = 0;
};

struct MillenniumDosTitleFarWordsObservation {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    std::uint16_t source_segment = 0;
    std::uint16_t source_offset = 0;
    std::uint16_t first_word = 0;
    std::uint16_t second_word = 0;
};
struct MillenniumDosTitleFarWordObservation {
    std::uint64_t sequence=0;
    std::uint16_t instruction_address=0;
    std::uint16_t source_segment=0;
    std::uint16_t source_offset=0;
    std::uint16_t word=0;
};
struct MillenniumDosTitleFarByteBoundary {
    std::uint16_t instruction_address=0;
    std::uint16_t source_segment=0;
    std::uint16_t source_offset=0;
    std::uint16_t destination_offset=0;
};
struct MillenniumDosTitleFarByteObservation {
    std::uint64_t sequence=0;
    std::uint16_t instruction_address=0;
    std::uint16_t source_segment=0;
    std::uint16_t source_offset=0;
    std::uint8_t byte=0;
};

struct MillenniumDosTitleDosBoundary {
    std::uint16_t interrupt_address = 0;
    std::uint16_t return_address = 0;
    std::uint8_t interrupt = 0;
    std::uint8_t service = 0;
    std::uint16_t ax_known_mask = 0;
    std::uint16_t ax_known_value = 0;
    std::uint16_t bx = 0;
    std::uint16_t segment = 0;
    std::uint16_t dx = 0;
    bool result_observed = false;
    std::uint16_t cx = 0;
    std::uint16_t source_address = 0;
    std::uint16_t source_size = 0;
};

struct MillenniumDosTitleInitializationMemoryEffect {
    std::uint16_t instruction_address = 0;
    std::uint16_t offset = 0;
    MillenniumDosTitleInitializationEffectWidth width =
        MillenniumDosTitleInitializationEffectWidth::byte;
    std::uint16_t value = 0;
    std::uint16_t segment = 0;
    bool explicit_segment = false;
};

struct MillenniumDosTitleInitializationRegisterEffect {
    std::uint16_t instruction_address = 0;
    std::string_view register_name;
    std::uint16_t value = 0;
};

struct MillenniumDosTitlePrivateInterruptBoundary {
    std::uint16_t call_address = 0;
    std::uint16_t wrapper_address = 0;
    std::uint16_t interrupt_address = 0;
    std::uint8_t interrupt = 0;
    std::uint16_t function = 0;
    std::uint16_t record_segment = 0;
    std::uint16_t record_offset = 0;
    bool result_observed = false;
    bool stack_storage_modeled = false;
};

struct MillenniumDosTitleInitializationCheckpoint {
    MillenniumDosTitleInitializationState state =
        MillenniumDosTitleInitializationState::awaiting_entry;
    std::uint64_t last_sequence = 0;
    std::uint16_t child_code_segment = 0;
    std::vector<MillenniumDosTitleInitializationRegisterEffect> register_effects;
    std::vector<MillenniumDosTitleInitializationMemoryEffect> memory_effects;
    MillenniumDosTitlePrivateInterruptBoundary boundary;
    std::uint16_t observed_ax = 0;
    std::uint16_t observed_flags = 0;
    std::uint8_t selected_mode = 0;
    std::uint16_t selected_call_address = 0;
    std::uint16_t selected_call_target = 0;
    MillenniumDosTitlePrivateInterruptBoundary selected_callee_boundary;
    std::uint16_t selected_callee_observed_ax = 0;
    std::uint16_t selected_callee_observed_flags = 0;
    std::uint16_t selected_followup_call_address = 0;
    std::uint16_t selected_followup_call_target = 0;
    MillenniumDosTitleBiosInterruptBoundary bios_boundary;
    std::vector<MillenniumDosTitleBiosResultRecord> bios_results;
    std::uint16_t title_main_call_address = 0;
    std::uint16_t title_main_call_target = 0;
    MillenniumDosTitleDosBoundary dos_boundary;
    std::vector<MillenniumDosTitleDosResultRecord> dos_results;
    std::vector<MillenniumDosTitleDosFileResultRecord> dos_file_results;
    std::vector<MillenniumDosTitleDosVectorResultRecord> dos_vector_results;
    MillenniumDosTitleSetupBiosBoundary setup_bios_boundary;
    std::vector<MillenniumDosTitleSetupBiosResultObservation> setup_bios_results;
    MillenniumDosTitleFarReadBoundary far_read_boundary;
    std::vector<MillenniumDosTitleFarWordsObservation> far_word_observations;
    std::vector<MillenniumDosTitleFarWordObservation> far_single_word_observations;
    MillenniumDosTitleFarByteBoundary far_byte_boundary;
    std::vector<MillenniumDosTitleFarByteObservation> far_byte_observations;
    std::uint16_t failure_address = 0;
    std::uint16_t continuation_address = 0;
    std::uint16_t post_video_observed_ax = 0;
    std::uint16_t post_video_observed_flags = 0;
    std::uint16_t graphics_record_observed_ax = 0;
    std::uint16_t graphics_record_observed_flags = 0;
    std::uint16_t post_descriptor_observed_ax = 0;
    std::uint16_t post_descriptor_observed_flags = 0;
    std::vector<std::uint8_t> post_descriptor_observed_record;
};

// Native execution of TITLES.EXE's deterministic $1b80 startup through the
// exact $0122 wrapper request. It records instruction-defined register state
// and stops before INT 91h can return. The wrapper's x86 stack storage is not
// synthesized because the narrow compatibility child allocation does not
// establish the original DOS process memory extent.
class MillenniumDosTitleInitializationSession {
public:
    MillenniumDosTitleInitializationSession(
        std::span<const std::uint8_t> titles_executable,
        std::uint16_t child_code_segment, std::uint64_t entry_sequence);

    void execute_exact_startup(std::uint64_t sequence,
        std::uint16_t entry_address, std::uint16_t call_address,
        std::uint16_t wrapper_address, std::uint8_t interrupt);
    void observe_private_interrupt_result(
        const MillenniumDosTitlePrivateInterruptResultObservation&);
    void execute_selected_callee_start(std::uint64_t sequence,
        std::uint16_t selected_call_address, std::uint16_t selected_call_target);
    void observe_selected_callee_private_interrupt_result(
        const MillenniumDosTitleSelectedCalleeResultObservation&);
    void execute_selected_followup_start(std::uint64_t sequence,
        std::uint16_t selected_followup_call_address,
        std::uint16_t selected_followup_call_target);
    void observe_bios_palette_result(
        const MillenniumDosTitleBiosResultObservation&,
        std::span<const std::uint8_t> titles_executable);
    void execute_title_main_allocation_start(std::uint64_t sequence,
        std::uint16_t call_address, std::uint16_t call_target);
    void observe_dos_memory_result(
        const MillenniumDosTitleDosResultObservation&);
    void observe_dos_file_result(
        const MillenniumDosTitleDosFileResultObservation&,
        std::span<const std::uint8_t> title_library = {});
    void execute_post_relocation(std::uint64_t sequence,
        std::span<const std::uint8_t> title_library);
    void execute_post_library_setup(std::uint64_t sequence,
        std::uint16_t call_address, std::uint16_t call_target);
    void observe_dos_vector_result(
        const MillenniumDosTitleDosVectorResultObservation&);
    void observe_setup_bios_result(
        const MillenniumDosTitleSetupBiosResultObservation&);
    void execute_next_setup(std::uint64_t sequence,
        std::uint16_t call_address, std::uint16_t call_target);
    void execute_followup_setup(std::uint64_t sequence,
        std::uint16_t call_address, std::uint16_t call_target);
    void observe_far_words(const MillenniumDosTitleFarWordsObservation&);
    void observe_far_word(const MillenniumDosTitleFarWordObservation&);
    void observe_far_byte(const MillenniumDosTitleFarByteObservation&);
    void execute_video_hook_setup(std::uint64_t sequence,
        std::uint16_t call_address, std::uint16_t call_target);
    void execute_post_video_mode_call(std::uint64_t sequence,
        std::uint16_t call_address, std::uint16_t call_target);
    void execute_post_video_setup(std::uint64_t sequence,
        std::uint16_t call_address, std::uint16_t call_target);
    void execute_post_video_graphics_call(std::uint64_t sequence,
        std::uint16_t call_address, std::uint16_t call_target);
    void execute_post_video_followup(std::uint64_t sequence,
        std::uint16_t call_address, std::uint16_t call_target);

    [[nodiscard]] MillenniumDosTitleInitializationCheckpoint checkpoint() const;

private:
    MillenniumDosTitleInitializationState state_ =
        MillenniumDosTitleInitializationState::awaiting_entry;
    std::uint64_t last_sequence_ = 0;
    std::uint16_t child_code_segment_ = 0;
    std::vector<MillenniumDosTitleInitializationRegisterEffect> effects_;
    std::vector<MillenniumDosTitleInitializationMemoryEffect> memory_effects_;
    MillenniumDosTitlePrivateInterruptBoundary boundary_;
    MillenniumDosTitlePrivateInterruptBoundary selected_callee_boundary_;
    std::uint16_t observed_ax_ = 0;
    std::uint16_t observed_flags_ = 0;
    std::uint8_t selected_mode_ = 0;
    std::uint16_t selected_call_address_ = 0;
    std::uint16_t selected_call_target_ = 0;
    std::uint16_t selected_callee_observed_ax_ = 0;
    std::uint16_t selected_callee_observed_flags_ = 0;
    std::uint16_t selected_followup_call_address_ = 0;
    std::uint16_t selected_followup_call_target_ = 0;
    bool post_video_repeat_ = false;
    MillenniumDosTitleBiosInterruptBoundary bios_boundary_;
    std::vector<MillenniumDosTitleBiosResultRecord> bios_results_;
    std::uint16_t title_main_call_address_ = 0;
    std::uint16_t title_main_call_target_ = 0;
    MillenniumDosTitleDosBoundary dos_boundary_;
    std::vector<MillenniumDosTitleDosResultRecord> dos_results_;
    std::vector<MillenniumDosTitleDosFileResultRecord> dos_file_results_;
    std::uint16_t dos_file_handle_ = 0;
    std::uint16_t dos_file_length_low_ = 0;
    std::uint16_t title_library_segment_ = 0;
    std::uint16_t title_library_paragraphs_ = 0;
    std::uint16_t title_library_handle_ = 0;
    std::uint32_t title_library_cursor_ = 0;
    std::uint8_t title_library_read_index_ = 0;
    std::uint16_t title_library_first_read_count_ = 0;
    std::uint16_t failure_address_ = 0;
    std::uint16_t continuation_address_ = 0;
    std::vector<MillenniumDosTitleDosVectorResultRecord> dos_vector_results_;
    MillenniumDosTitleSetupBiosBoundary setup_bios_boundary_;
    std::vector<MillenniumDosTitleSetupBiosResultObservation> setup_bios_results_;
    MillenniumDosTitleFarReadBoundary far_read_boundary_;
    std::vector<MillenniumDosTitleFarWordsObservation> far_word_observations_;
    std::vector<MillenniumDosTitleFarWordObservation> far_single_word_observations_;
    MillenniumDosTitleFarByteBoundary far_byte_boundary_;
    std::vector<MillenniumDosTitleFarByteObservation> far_byte_observations_;
    std::uint16_t post_video_observed_ax_ = 0;
    std::uint16_t post_video_observed_flags_ = 0;
    std::uint16_t graphics_record_observed_ax_ = 0;
    std::uint16_t graphics_record_observed_flags_ = 0;
    std::uint16_t post_descriptor_observed_ax_ = 0;
    std::uint16_t post_descriptor_observed_flags_ = 0;
    std::vector<std::uint8_t> post_descriptor_observed_record_;
};

} // namespace eon
