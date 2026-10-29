#pragma once

#include "engine/millennium_dos_native_process.hpp"
#include "engine/millennium_dos_video_function_31_session.hpp"
#include "engine/millennium_dos_post_overlay_loop_session.hpp"
#include "engine/millennium_dos_function_dispatch_admission.hpp"
#include "engine/millennium_dos_seventh_function_session.hpp"
#include "engine/millennium_dos_sixth_function_session.hpp"
#include "engine/millennium_dos_eighth_function_session.hpp"
#include "engine/millennium_dos_ninth_function_session.hpp"
#include "engine/millennium_dos_ninth_function_handoff_session.hpp"
#include "engine/millennium_dos_fourth_function_session.hpp"
#include "engine/millennium_dos_fifth_function_session.hpp"
#include "engine/millennium_dos_third_function_session.hpp"
#include "engine/millennium_dos_first_function_session.hpp"
#include "engine/millennium_dos_second_function_session.hpp"
#include "engine/millennium_dos_second_function_callback_session.hpp"
#include "engine/millennium_dos_bdf_service_session.hpp"
#include "engine/millennium_dos_bdf_mode_two_session.hpp"
#include "engine/millennium_dos_bdf_other_mode_session.hpp"
#include "engine/millennium_dos_shared_helper_session.hpp"
#include "engine/millennium_dos_game_session.hpp"
#include "engine/millennium_dos_gx_overlay_adapter_session.hpp"
#include "engine/millennium_dos_tenth_function_session.hpp"

#include <cstdint>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace eon {

enum class MillenniumDosNativeRecoveryEntry {
    startup,
    post_gx_loader,
};

// Copy-only diagnostic value for a prepared native continuation. It contains
// no source path, original bytes, mutable process, or claim of reachability.
struct MillenniumDosNativeProcessCheckpoint {
    MillenniumDosNativeRecoveryEntry recovery_entry =
        MillenniumDosNativeRecoveryEntry::startup;
    MillenniumDosNativeProcessState state =
        MillenniumDosNativeProcessState::startup_first_private_interrupt;
    MillenniumDosNativeBoundary boundary;
    std::string release_sha256;
    std::string game_executable_sha256;
    std::optional<std::string> gx_overlay_sha256;
    bool static_recovery_entry = true;
    // These fields remain empty until the title-to-game coordinator accepts
    // a matching DOS child-process transfer observation.
    bool observed_child_process_entry = false;
    std::uint64_t child_entry_sequence = 0;
    std::uint16_t child_code_segment = 0;
    std::uint64_t last_observation_sequence = 0;
    std::optional<MillenniumDosNativeBiosInterruptReturnObservation> startup_bios_return;
    std::size_t startup_ega_palette_write_count = 0;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation> startup_dos_return;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_allocator_second_dos_return;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_allocator_free_dos_return;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_allocator_third_dos_return;
    std::optional<MillenniumDosNativeStartupFileOpenRequest> startup_file_open_request;
    std::optional<MillenniumDosNativeStartupFileOpenRequest>
        startup_zero_path_file_open_request;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_zero_path_file_open_dos_return;
    std::optional<MillenniumDosNativeStartupFileRequest>
        startup_zero_path_file_read_request;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_zero_path_file_read_dos_return;
    std::optional<MillenniumDosNativeStartupFileRequest>
        startup_zero_path_file_close_request;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_zero_path_file_close_dos_return;
    std::optional<MillenniumDosNativeStartupFileRequest>
        startup_zero_path_vector_get_request;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_zero_path_vector_get_dos_return;
    std::optional<MillenniumDosNativeStartupFileRequest>
        startup_zero_path_vector_set_request;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_zero_path_vector_set_dos_return;
    std::optional<MillenniumDosNativeRuntimeByteObservation>
        startup_zero_path_continuation_byte;
    std::optional<std::uint8_t> startup_zero_path_selector_value;
    std::optional<MillenniumDosSibmFunctionZeroCheckpoint>
        startup_sibm_function_zero;
    std::optional<MillenniumDosNativeStartupFileRequest> startup_file_seek_request;
    std::optional<MillenniumDosNativeStartupFileRequest> startup_file_close_request;
    std::optional<MillenniumDosNativeStartupFileRequest> startup_allocator_fourth_request;
    std::optional<MillenniumDosNativeStartupFileRequest> startup_allocator_fifth_request;
    std::optional<MillenniumDosNativeStartupFileRequest> startup_allocator_seventh_request;
    std::optional<MillenniumDosNativeStartupFileRequest> startup_allocator_eighth_request;
    std::optional<MillenniumDosNativeStartupFileRequest> startup_allocator_ninth_request;
    std::optional<MillenniumDosNativeStartupFileRequest> startup_allocator_tenth_request;
    std::optional<MillenniumDosNativeStartupFileRequest> startup_allocator_eleventh_request;
    std::optional<std::uint16_t> startup_file_handle;
    std::optional<std::uint16_t> startup_file_size;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_allocator_fourth_dos_return;
    std::optional<std::uint16_t> startup_allocator_fourth_segment;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_allocator_fifth_dos_return;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_allocator_sixth_dos_return;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_allocator_seventh_dos_return;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_allocator_eighth_dos_return;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_allocator_ninth_dos_return;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_allocator_tenth_dos_return;
    std::optional<MillenniumDosNativeDosInterruptReturnObservation>
        startup_allocator_eleventh_dos_return;
    std::optional<std::uint16_t> startup_allocator_return_ax;
    std::optional<std::uint16_t> startup_allocator_stored_bx;
    std::optional<std::uint16_t> startup_allocator_es_segment;
};

struct MillenniumDosNativeFunction31DispatchResult {
    bool accepted = false;
    std::string error;
    std::uint64_t sequence = 0;
    std::uint16_t target_address = 0;
    std::uint16_t local_state_address = 0;
    std::uint8_t local_state_value = 0;
    std::uint16_t returned_ax = 0;
};

struct MillenniumDosNativePrivateInterruptReturnObservation {
    std::uint64_t sequence = 0;
    std::uint16_t interrupt_return_address = 0;
    std::uint16_t ax = 0;
};

// Owns the private backing copies required by MillenniumDosNativeProcess's
// read-only spans. This object is intended to live inside the release runtime;
// its public surface is typed observations plus a value-only checkpoint.
// Construction requires the caller's exact English release identity and
// independently validates both leaf byte streams. Authentication of the
// outer release container remains the future coordinator's responsibility.
// It does not assert that title execution reached either recovery entry.
class MillenniumDosNativeProcessAdmission {
public:
    [[nodiscard]] static MillenniumDosNativeProcessAdmission startup(
        std::string_view release_sha256,
        std::span<const std::uint8_t> game_executable);
    [[nodiscard]] static MillenniumDosNativeProcessAdmission post_gx_loader(
        std::string_view release_sha256,
        std::span<const std::uint8_t> game_executable,
        std::span<const std::uint8_t> gx_overlay_executable);

    MillenniumDosNativeProcessAdmission() = default;
    MillenniumDosNativeProcessAdmission(MillenniumDosNativeProcessAdmission&&) = default;
    MillenniumDosNativeProcessAdmission& operator=(MillenniumDosNativeProcessAdmission&&) = default;
    MillenniumDosNativeProcessAdmission(const MillenniumDosNativeProcessAdmission&) = delete;
    MillenniumDosNativeProcessAdmission& operator=(
        const MillenniumDosNativeProcessAdmission&) = delete;
    ~MillenniumDosNativeProcessAdmission() = default;

    [[nodiscard]] bool admitted() const { return process_ != nullptr && error_.empty(); }
    [[nodiscard]] const std::string& error() const { return error_; }
    [[nodiscard]] std::optional<MillenniumDosNativeProcessCheckpoint> checkpoint() const;
    // Constructs a span-based continuation whose lifetime must remain nested
    // within this admission. The exact backing bytes never cross the API.
    [[nodiscard]] MillenniumDosPostOverlayLoopSession make_post_overlay_loop_session(
        std::uint8_t selected_mode_byte) const;
    [[nodiscard]] MillenniumDosTenthFunctionSession make_tenth_function_session() const;
    [[nodiscard]] MillenniumDosSeventhFunctionSession make_seventh_function_session() const;
    [[nodiscard]] MillenniumDosSixthFunctionSession make_sixth_function_session() const;
    [[nodiscard]] MillenniumDosFunctionDispatchAdmission admit_function_dispatch(
        const MillenniumDosPostOverlayLoopSession& loop,
        MillenniumDosFunctionDispatchObservation observation) const;
    [[nodiscard]] MillenniumDosEighthFunctionSession make_eighth_function_session() const;
    [[nodiscard]] MillenniumDosNinthFunctionSession make_ninth_function_session() const;
    [[nodiscard]] MillenniumDosNinthFunctionHandoffSession make_ninth_function_handoff_session() const;
    [[nodiscard]] MillenniumDosFourthFunctionSession make_fourth_function_session() const;
    [[nodiscard]] MillenniumDosFifthFunctionSession make_fifth_function_session() const;
    [[nodiscard]] MillenniumDosThirdFunctionSession make_third_function_session() const;
    [[nodiscard]] MillenniumDosFirstFunctionSession make_first_function_session() const;
    [[nodiscard]] MillenniumDosSecondFunctionSession make_second_function_session() const;
    [[nodiscard]] MillenniumDosSecondFunctionCallbackSession make_second_function_callback_session() const;
    [[nodiscard]] MillenniumDosBdfServiceSession make_bdf_service_session() const;
    [[nodiscard]] MillenniumDosBdfModeTwoSession make_bdf_mode_two_session(std::uint16_t entry_di) const;
    [[nodiscard]] MillenniumDosBdfOtherModeSession make_bdf_other_mode_session(std::uint8_t entry_dl,std::uint16_t entry_di) const;
    [[nodiscard]] MillenniumDosSharedHelperSession make_shared_helper_session(std::uint16_t caller_ax) const;
    [[nodiscard]] MillenniumDosGameSession make_game_session() const;
    [[nodiscard]] MillenniumDosGxOverlayAdapterSession make_gx_overlay_adapter_session(
        std::uint16_t caller_ax, std::uint16_t caller_return,
        std::uint16_t code_segment) const;

    void observe_private_interrupt_return(std::uint16_t address, std::uint16_t ax);
    void begin_startup_ega_palette(
        const MillenniumDosGameFunctionFourDispatchResult& function_four);
    void observe_startup_allocation_request(std::uint16_t interrupt_address);
    void observe_private_interrupt_return(
        const MillenniumDosNativePrivateInterruptReturnObservation& observation);
    void observe_bios_interrupt_return(
        const MillenniumDosNativeBiosInterruptReturnObservation& observation);
    void continue_startup_ega_caller(std::uint8_t selector_value,
        std::uint16_t first_return_word, std::uint16_t outer_return_word);
    void continue_startup_ega_allocator_request(std::uint16_t outer_return_word);
    void observe_dos_interrupt_return(
        const MillenniumDosNativeDosInterruptReturnObservation& observation);
    void observe_runtime_byte(std::uint64_t sequence, std::uint16_t instruction_address,
        std::uint16_t runtime_address, std::uint8_t value);
    void observe_startup_sibm_function_zero_prefix(
        const MillenniumDosSibmFunctionZeroCheckpoint& result);
    void observe_startup_sibm_timer_vector_get_return(
        const MillenniumDosNativeDosInterruptReturnObservation& observation,
        const MillenniumDosSibmFunctionZeroCheckpoint& result,
        std::uint16_t loaded_driver_segment);
    void observe_startup_sibm_timer_vector_set_return(
        const MillenniumDosNativeDosInterruptReturnObservation& observation,
        const MillenniumDosSibmFunctionZeroCheckpoint& result,
        std::uint16_t loaded_driver_segment);
    void observe_child_process_entry(std::uint64_t sequence, std::uint16_t code_segment);
    // Execute only the exact first 2200AD private INT 91h function ($1f),
    // and only from the admitted MCGA/EGA vector and its owned loaded image.
    // The function reads the current driver-local state byte; no return value
    // is supplied by the caller or inferred from the original file.
    [[nodiscard]] MillenniumDosNativeFunction31DispatchResult
    execute_startup_function_31_return(
        std::uint64_t sequence,
        std::span<const std::uint8_t> exact_driver_image,
        std::span<const std::uint8_t> owned_loaded_driver_image,
        MillenniumDosVideoDriverKind kind,
        std::uint16_t loaded_driver_segment,
        std::uint8_t vector_interrupt,
        std::uint16_t vector_offset,
        std::uint16_t vector_segment);
    void observe_runtime_byte(std::uint16_t instruction_address,
        std::uint16_t runtime_address, std::uint8_t value);
    void observe_native_call_return(std::uint16_t call_address,
        std::uint16_t return_address);

    // Revokes the process before releasing its backing source bytes. A stale
    // checkpoint remains a detached value and cannot advance the new state.
    void reset();

private:
    static constexpr std::string_view english_release_sha256 =
        "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123";
    static constexpr std::string_view game_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    static constexpr std::string_view gx_sha256 =
        "093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb";

    void require_admitted() const;

    // Declaration order is intentional: process_ is destroyed before spans'
    // backing buffers during ordinary destruction.
    std::vector<std::uint8_t> game_executable_;
    std::vector<std::uint8_t> gx_overlay_executable_;
    std::unique_ptr<MillenniumDosNativeProcess> process_;
    MillenniumDosNativeRecoveryEntry recovery_entry_ =
        MillenniumDosNativeRecoveryEntry::startup;
    std::string error_;
    bool observed_child_process_entry_ = false;
    std::uint64_t child_entry_sequence_ = 0;
    std::uint16_t child_code_segment_ = 0;
    std::uint64_t last_observation_sequence_ = 0;
};

} // namespace eon
