#pragma once

#include "data/millennium_dos_game_flow.hpp"
#include "engine/millennium_dos_gx_startup_session.hpp"
#include "engine/millennium_dos_ega_startup_palette_session.hpp"
#include "engine/millennium_dos_sibm_function_zero_session.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>

namespace eon {

// One manually recompiled, hash-bound continuation of the English DOS game.
// It never decodes arbitrary x86 and cannot cross a native call by itself.
enum class MillenniumDosNativeProcessState {
    startup_first_private_interrupt,
    startup_selected_private_interrupt,
    startup_local_return_boundary,
    startup_palette_interrupt_boundary,
    startup_palette_interrupt_return_observed,
    startup_palette_loop_return_boundary,
    startup_ega_caller_return_boundary,
    startup_allocation_interrupt_boundary,
    startup_allocator_second_request_boundary,
    startup_allocator_free_request_boundary,
    startup_allocator_third_request_boundary,
    startup_allocator_file_open_request_boundary,
    startup_allocator_file_seek_request_boundary,
    startup_allocator_file_close_request_boundary,
    startup_allocator_fourth_request_boundary,
    startup_allocator_fifth_request_boundary,
    startup_allocator_sixth_request_boundary,
    startup_allocator_seventh_request_boundary,
    startup_allocator_eighth_request_boundary,
    startup_allocator_ninth_request_boundary,
    startup_allocator_tenth_request_boundary,
    startup_allocator_eleventh_request_boundary,
    startup_allocator_local_call_boundary,
    startup_zero_path_file_open_request_boundary,
    startup_zero_path_file_read_request_boundary,
    startup_zero_path_file_close_request_boundary,
    startup_zero_path_vector_get_request_boundary,
    startup_zero_path_vector_set_request_boundary,
    startup_zero_path_vector_set_return_observed,
    startup_zero_path_continuation_byte_boundary,
    startup_zero_path_selector_call_boundary,
    startup_sibm_timer_vector_get_boundary,
    startup_sibm_timer_vector_set_boundary,
    startup_sibm_timer_vector_set_return_observed,
    startup_allocator_failure_branch_boundary,
    startup_allocator_file_error_boundary,
    gx_private_interrupt,
    gx_mode_byte,
    gx_adapter_return,
    gx_post_overlay_call_return,
    gx_post_overlay_mode_byte,
    gx_post_overlay_private_interrupt,
};

enum class MillenniumDosNativeBoundaryKind {
    private_interrupt,
    bios_interrupt,
    observed_bios_interrupt_return,
    runtime_byte,
    native_call_return,
    local_return,
    dos_interrupt,
    observed_dos_interrupt_return,
    local_instruction,
};

struct MillenniumDosNativeBiosInterruptReturnObservation {
    std::uint64_t sequence = 0;
    std::uint16_t interrupt_instruction_address = 0;
    std::uint8_t interrupt_number = 0;
    std::uint16_t return_code_segment = 0;
    std::uint16_t return_instruction_pointer = 0;
    std::uint16_t ax = 0;
    std::uint16_t flags = 0;
    std::uint16_t bx = 0;
    constexpr bool operator==(const MillenniumDosNativeBiosInterruptReturnObservation&) const = default;
};

struct MillenniumDosNativeDosInterruptReturnObservation {
    std::uint64_t sequence = 0;
    std::uint16_t interrupt_instruction_address = 0;
    std::uint8_t interrupt_number = 0;
    std::uint8_t service = 0;
    std::uint16_t return_code_segment = 0;
    std::uint16_t return_instruction_pointer = 0;
    std::uint16_t ax = 0;
    std::uint16_t bx = 0;
    std::uint16_t cx = 0;
    std::uint16_t dx = 0;
    std::uint16_t flags = 0;
    std::uint16_t es = 0;
    constexpr bool operator==(const MillenniumDosNativeDosInterruptReturnObservation&) const = default;
};

struct MillenniumDosNativeRuntimeByteObservation {
    std::uint64_t sequence = 0;
    std::uint16_t instruction_address = 0;
    std::uint16_t runtime_address = 0;
    std::uint8_t value = 0;
    constexpr bool operator==(const MillenniumDosNativeRuntimeByteObservation&) const = default;
};

struct MillenniumDosNativeBoundary {
    MillenniumDosNativeBoundaryKind kind = MillenniumDosNativeBoundaryKind::private_interrupt;
    std::uint16_t address = 0;
    std::optional<std::uint8_t> interrupt;
    std::optional<std::uint16_t> runtime_byte_address;
    std::size_t ordinal = 0;
    std::optional<std::uint8_t> service;
    constexpr bool operator==(const MillenniumDosNativeBoundary&) const = default;
};

struct MillenniumDosNativeStartupFileOpenRequest {
    std::uint16_t instruction_address = 0;
    std::uint16_t return_instruction_pointer = 0;
    std::uint8_t interrupt_number = 0;
    std::uint8_t service = 0;
    std::uint16_t ax = 0;
    bool data_segment_is_code_segment = false;
    std::uint16_t dx_name_address = 0;
    constexpr bool operator==(const MillenniumDosNativeStartupFileOpenRequest&) const = default;
};

struct MillenniumDosNativeStartupFileRequest {
    std::uint16_t instruction_address = 0;
    std::uint16_t return_instruction_pointer = 0;
    std::uint8_t interrupt_number = 0;
    std::uint8_t service = 0;
    std::uint16_t ax = 0;
    std::uint16_t bx = 0;
    std::uint16_t cx = 0;
    std::uint16_t dx = 0;
    std::uint16_t ds = 0;
    constexpr bool operator==(const MillenniumDosNativeStartupFileRequest&) const = default;
};

// `startup()` begins only at 2200AD.EXE's independently verified entry.
// `post_gx_loader()` is a separate recovery entry and deliberately does not
// claim that the title/startup path reached it. The caller must keep both
// authenticated source spans alive for the lifetime of this object.
class MillenniumDosNativeProcess {
public:
    [[nodiscard]] static MillenniumDosNativeProcess startup(
        std::span<const std::uint8_t> game_executable);
    [[nodiscard]] static MillenniumDosNativeProcess post_gx_loader(
        std::span<const std::uint8_t> game_executable,
        std::span<const std::uint8_t> gx_overlay_executable);

    [[nodiscard]] MillenniumDosNativeProcessState state() const { return state_; }
    [[nodiscard]] MillenniumDosNativeBoundary boundary() const;

    // Observations are accepted only at the exact currently published
    // boundary. Address arguments prevent a result from being replayed at a
    // different INT, call, or native byte read.
    void observe_private_interrupt_return(std::uint16_t address, std::uint16_t ax);
    void observe_startup_allocation_request(std::uint16_t interrupt_address);
    void continue_startup_allocator_zero_path();
    void observe_bios_interrupt_return(
        const MillenniumDosNativeBiosInterruptReturnObservation& observation);
    void begin_startup_ega_palette(
        const MillenniumDosGameFunctionFourDispatchResult& function_four);
    void continue_startup_ega_caller(
        std::uint8_t selector_value, std::uint16_t first_return_word,
        std::uint16_t outer_return_word);
    void continue_startup_ega_allocator_request(std::uint16_t outer_return_word);
    void observe_dos_interrupt_return(
        const MillenniumDosNativeDosInterruptReturnObservation& observation);
    void observe_runtime_byte(std::uint16_t instruction_address,
        std::uint16_t runtime_address, std::uint8_t value,
        std::uint64_t sequence = 0);
    void observe_startup_sibm_function_zero_prefix(
        const MillenniumDosSibmFunctionZeroCheckpoint& result);
    void observe_startup_sibm_function_zero_vector_get(
        const MillenniumDosSibmFunctionZeroCheckpoint& result);
    void observe_startup_sibm_function_zero_vector_set_return(
        const MillenniumDosSibmFunctionZeroCheckpoint& result);
    void observe_native_call_return(std::uint16_t call_address,
        std::uint16_t return_address);

    [[nodiscard]] std::optional<std::uint8_t> runtime_byte(std::uint16_t address) const;
    [[nodiscard]] std::optional<std::uint8_t> gx_overlay_byte(std::uint16_t offset) const;
    [[nodiscard]] const std::optional<MillenniumDosNativeBiosInterruptReturnObservation>&
    startup_bios_return() const { return startup_bios_return_; }
    [[nodiscard]] const std::optional<MillenniumDosEgaStartupPaletteSession>&
    startup_ega_palette_session() const noexcept { return startup_ega_palette_session_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeDosInterruptReturnObservation>&
    startup_dos_return() const { return startup_dos_return_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeDosInterruptReturnObservation>&
    startup_allocator_second_dos_return() const { return startup_allocator_second_dos_return_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeDosInterruptReturnObservation>&
    startup_allocator_free_dos_return() const { return startup_allocator_free_dos_return_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeDosInterruptReturnObservation>&
    startup_allocator_third_dos_return() const { return startup_allocator_third_dos_return_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeStartupFileOpenRequest>&
    startup_file_open_request() const { return startup_file_open_request_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeStartupFileOpenRequest>&
    startup_zero_path_file_open_request() const { return startup_zero_path_file_open_request_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeDosInterruptReturnObservation>&
    startup_zero_path_file_open_dos_return() const { return startup_zero_path_file_open_dos_return_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeStartupFileRequest>&
    startup_zero_path_file_read_request() const { return startup_zero_path_file_read_request_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeDosInterruptReturnObservation>&
    startup_zero_path_file_read_dos_return() const { return startup_zero_path_file_read_dos_return_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeStartupFileRequest>&
    startup_zero_path_file_close_request() const { return startup_zero_path_file_close_request_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeDosInterruptReturnObservation>&
    startup_zero_path_file_close_dos_return() const { return startup_zero_path_file_close_dos_return_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeStartupFileRequest>&
    startup_zero_path_vector_get_request() const { return startup_zero_path_vector_get_request_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeDosInterruptReturnObservation>&
    startup_zero_path_vector_get_dos_return() const { return startup_zero_path_vector_get_dos_return_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeStartupFileRequest>&
    startup_zero_path_vector_set_request() const { return startup_zero_path_vector_set_request_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeDosInterruptReturnObservation>&
    startup_zero_path_vector_set_dos_return() const { return startup_zero_path_vector_set_dos_return_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeRuntimeByteObservation>&
    startup_zero_path_continuation_byte() const { return startup_zero_path_continuation_byte_; }
    [[nodiscard]] const std::optional<MillenniumDosSibmFunctionZeroCheckpoint>&
    startup_sibm_function_zero() const { return startup_sibm_function_zero_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeStartupFileRequest>&
    startup_allocator_seventh_request() const { return startup_allocator_seventh_request_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeStartupFileRequest>&
    startup_allocator_eighth_request() const { return startup_allocator_eighth_request_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeStartupFileRequest>&
    startup_allocator_ninth_request() const { return startup_allocator_ninth_request_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeStartupFileRequest>&
    startup_allocator_tenth_request() const { return startup_allocator_tenth_request_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeStartupFileRequest>&
    startup_allocator_eleventh_request() const { return startup_allocator_eleventh_request_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeStartupFileRequest>&
    startup_file_seek_request() const { return startup_file_seek_request_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeStartupFileRequest>&
    startup_file_close_request() const { return startup_file_close_request_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeStartupFileRequest>&
    startup_allocator_fourth_request() const { return startup_allocator_fourth_request_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeStartupFileRequest>&
    startup_allocator_fifth_request() const { return startup_allocator_fifth_request_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeDosInterruptReturnObservation>&
    startup_allocator_fourth_dos_return() const { return startup_allocator_fourth_dos_return_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeDosInterruptReturnObservation>&
    startup_allocator_fifth_dos_return() const { return startup_allocator_fifth_dos_return_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeDosInterruptReturnObservation>&
    startup_allocator_sixth_dos_return() const { return startup_allocator_sixth_dos_return_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeDosInterruptReturnObservation>&
    startup_allocator_seventh_dos_return() const { return startup_allocator_seventh_dos_return_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeDosInterruptReturnObservation>&
    startup_allocator_eighth_dos_return() const { return startup_allocator_eighth_dos_return_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeDosInterruptReturnObservation>&
    startup_allocator_ninth_dos_return() const { return startup_allocator_ninth_dos_return_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeDosInterruptReturnObservation>&
    startup_allocator_tenth_dos_return() const { return startup_allocator_tenth_dos_return_; }
    [[nodiscard]] const std::optional<MillenniumDosNativeDosInterruptReturnObservation>&
    startup_allocator_eleventh_dos_return() const { return startup_allocator_eleventh_dos_return_; }
    [[nodiscard]] const std::optional<std::uint16_t>&
    startup_allocator_return_ax() const { return startup_allocator_return_ax_; }
    [[nodiscard]] const std::optional<std::uint16_t>&
    startup_allocator_fourth_segment() const { return startup_allocator_fourth_segment_; }
    [[nodiscard]] const std::optional<std::uint16_t>&
    startup_file_handle() const { return startup_file_handle_; }
    [[nodiscard]] const std::optional<std::uint16_t>&
    startup_file_size() const { return startup_file_size_; }
    [[nodiscard]] const std::optional<std::uint16_t>&
    startup_allocator_stored_bx() const { return startup_allocator_stored_bx_; }
    [[nodiscard]] const std::optional<std::uint16_t>&
    startup_allocator_es_segment() const { return startup_allocator_es_segment_; }

private:
    explicit MillenniumDosNativeProcess(std::span<const std::uint8_t> game_executable);
    MillenniumDosNativeProcess(std::span<const std::uint8_t> game_executable,
        std::span<const std::uint8_t> gx_overlay_executable);
    void apply_startup_writes(const MillenniumDosEnglishStartupPrefix& prefix);

    std::span<const std::uint8_t> game_executable_;
    MillenniumDosNativeProcessState state_ =
        MillenniumDosNativeProcessState::startup_first_private_interrupt;
    std::optional<std::uint16_t> startup_first_ax_;
    std::optional<MillenniumDosEnglishStartupPrefix> startup_prefix_;
    std::optional<MillenniumDosNativeBiosInterruptReturnObservation> startup_bios_return_;
    std::optional<MillenniumDosEgaStartupPaletteSession> startup_ega_palette_session_;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation> startup_dos_return_;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_allocator_second_dos_return_;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_allocator_free_dos_return_;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_allocator_third_dos_return_;
    std::optional<MillenniumDosNativeStartupFileOpenRequest> startup_file_open_request_;
    std::optional<MillenniumDosNativeStartupFileOpenRequest> startup_zero_path_file_open_request_;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_zero_path_file_open_dos_return_;
    std::optional<MillenniumDosNativeStartupFileRequest> startup_zero_path_file_read_request_;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_zero_path_file_read_dos_return_;
    std::optional<MillenniumDosNativeStartupFileRequest>
        startup_zero_path_file_close_request_;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_zero_path_file_close_dos_return_;
    std::optional<MillenniumDosNativeStartupFileRequest>
        startup_zero_path_vector_get_request_;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_zero_path_vector_get_dos_return_;
    std::optional<MillenniumDosNativeStartupFileRequest>
        startup_zero_path_vector_set_request_;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_zero_path_vector_set_dos_return_;
    std::optional<MillenniumDosNativeRuntimeByteObservation>
        startup_zero_path_continuation_byte_;
    std::optional<MillenniumDosSibmFunctionZeroCheckpoint>
        startup_sibm_function_zero_;
    std::optional<MillenniumDosNativeStartupFileRequest> startup_file_seek_request_;
    std::optional<MillenniumDosNativeStartupFileRequest> startup_file_close_request_;
    std::optional<MillenniumDosNativeStartupFileRequest> startup_allocator_fourth_request_;
    std::optional<MillenniumDosNativeStartupFileRequest> startup_allocator_fifth_request_;
    std::optional<MillenniumDosNativeStartupFileRequest> startup_allocator_seventh_request_;
    std::optional<MillenniumDosNativeStartupFileRequest> startup_allocator_eighth_request_;
    std::optional<MillenniumDosNativeStartupFileRequest> startup_allocator_ninth_request_;
    std::optional<MillenniumDosNativeStartupFileRequest> startup_allocator_tenth_request_;
    std::optional<MillenniumDosNativeStartupFileRequest> startup_allocator_eleventh_request_;
    std::optional<std::uint16_t> startup_file_handle_;
    std::optional<std::uint16_t> startup_file_size_;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_allocator_fourth_dos_return_;
    std::optional<std::uint16_t> startup_allocator_fourth_segment_;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_allocator_fifth_dos_return_;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_allocator_sixth_dos_return_;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_allocator_seventh_dos_return_;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_allocator_eighth_dos_return_;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_allocator_ninth_dos_return_;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_allocator_tenth_dos_return_;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_allocator_eleventh_dos_return_;
    std::optional<std::uint16_t> startup_allocator_return_ax_;
    std::uint8_t startup_file_helper_invocation_ = 0;
    std::optional<std::uint16_t> startup_allocator_stored_bx_;
    std::optional<std::uint16_t> startup_allocator_es_segment_;
    std::unique_ptr<MillenniumDosGxStartupSession> gx_session_;
    std::map<std::uint16_t, std::uint8_t> runtime_bytes_;
};

} // namespace eon
