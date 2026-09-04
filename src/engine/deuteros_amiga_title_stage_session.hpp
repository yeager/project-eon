#pragma once

#include "data/amiga_adf.hpp"
#include "data/deuteros_amiga_bundle.hpp"
#include "data/deuteros_amiga_loader.hpp"
#include "data/deuteros_amiga_title_stage.hpp"
#include "engine/deuteros_amiga_title_exec_boundary_session.hpp"
#include "engine/deuteros_amiga_title_custom_chip_boundary_session.hpp"
#include "engine/deuteros_amiga_title_service_setup_boundary_session.hpp"
#include "engine/deuteros_amiga_title_service_batch_boundary_session.hpp"
#include "engine/deuteros_amiga_title_open_library_boundary_session.hpp"

#include <span>
#include <optional>
#include <string>

namespace eon {

// An explicit, read-only boundary after the verified opening input handoff.
// It proves which original stage is ready for execution and retains the
// caller-connected local entry-prefix result without inventing the
// register/global state or graphics-library calls that the stage requires.
class DeuterosAmigaTitleStageSession {
public:
    DeuterosAmigaTitleStageSession(const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
        std::uint16_t incoming_profile);

    [[nodiscard]] const AmigaLoadStage& stage() const noexcept { return stage_; }
    [[nodiscard]] const DeuterosAmigaTitleStageProfile& profile() const noexcept { return profile_; }
    // The literal local entry prefix selected by the live bootstrap handoff.
    // It stops before the original Exec ABI boundary.
    [[nodiscard]] const DeuterosAmigaTitleEntryPrefix& entry_prefix() const noexcept {
        return entry_prefix_;
    }
    // These two writes are the complete caller-proven title RAM effect before
    // the first unresolved Exec vector. They are sparse records only; no
    // synthetic address space is allocated or exposed.
    [[nodiscard]] const DeuterosAmigaTitleEntryPrefixState& entry_prefix_state() const noexcept {
        return entry_prefix_state_;
    }
    // The next literal instruction initializes A7 and then stops before the
    // original reads the unknown Exec base. This is a register fact only.
    [[nodiscard]] const DeuterosAmigaTitleExecPrelude& exec_prelude() const noexcept {
        return exec_prelude_;
    }
    // These caller-connected routines immediately follow the pre-Exec
    // startup sequence in the hash-locked title stage.  They remain
    // provenance profiles: the external display base and every Exec or
    // graphics-library result are intentionally absent.
    [[nodiscard]] const DeuterosAmigaTitleGraphicsSetupProfile& graphics_setup() const noexcept {
        return graphics_setup_;
    }
    [[nodiscard]] const DeuterosAmigaTitleDisplayClearProfile& display_clear() const noexcept {
        return display_clear_;
    }
    [[nodiscard]] std::span<const std::uint8_t> original_bytes() const;
    // The first sixteen RGB4 words at the hash-validated title transition's
    // source address, exposed only as original palette evidence. This is not
    // a decoded title screen and does not cross graphics.library.
    [[nodiscard]] std::array<RgbColor, 16> transition_palette_evidence() const;
    // All twenty RGB4 words copied by the separately verified local graphics
    // setup routine. They are source-data evidence only, never a screen.
    [[nodiscard]] std::array<RgbColor, 20> graphics_setup_palette_evidence() const;
    [[nodiscard]] const std::string& original_sha256() const noexcept { return original_sha256_; }

    // Executes the complete caller-proven local prefix once: its two sparse
    // writes followed by the A7 literal. It deliberately stops before the
    // first Exec-base read and never creates host memory for those addresses.
    struct LocalPrefixAdvance {
        std::array<DeuterosAmigaTitleEntryWrite, 2> writes{};
        std::uint32_t stack_pointer_value = 0;
        std::uint32_t exec_boundary_address = 0;
    };
    [[nodiscard]] std::optional<LocalPrefixAdvance> execute_local_prefix();
    [[nodiscard]] bool local_prefix_executed() const noexcept { return local_prefix_executed_; }
    [[nodiscard]] const DeuterosAmigaTitleExecBoundaryCheckpoint& exec_boundary() const noexcept {
        return exec_boundary_session_.checkpoint();
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleExecBoundaryCheckpoint>
    observe_exec_return(const DeuterosAmigaObservedExecReturn& observation);
    [[nodiscard]] const DeuterosAmigaTitleOpenLibraryBoundaryCheckpoint*
    open_library_boundary() const noexcept {
        return open_library_boundary_session_
            ? &open_library_boundary_session_->checkpoint() : nullptr;
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleOpenLibraryBoundaryCheckpoint>
    observe_open_library_return(
        const DeuterosAmigaObservedOpenLibraryReturn& observation) {
        if (!open_library_boundary_session_) return std::nullopt;
        return open_library_boundary_session_->observe_return(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostOpenLibraryLocalAdvance>
    advance_post_open_library_local_path() {
        if (!open_library_boundary_session_) return std::nullopt;
        return open_library_boundary_session_->advance_nonzero_local_path();
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleDisplayLocalAdvance>
    observe_display_base_and_advance(
        const DeuterosAmigaObservedDisplayBaseRead& observation);
    [[nodiscard]] const DeuterosAmigaTitleCustomChipBoundarySession*
    custom_chip_boundary() const noexcept {
        return custom_chip_boundary_session_ ? &*custom_chip_boundary_session_ : nullptr;
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCallbackRegistrationLocalPlan>
    observe_custom_chip_write(const DeuterosAmigaObservedCustomChipWrite& observation) {
        if (!custom_chip_boundary_session_) return std::nullopt;
        return custom_chip_boundary_session_->observe_write(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCallbackRegistrationAdvance>
    observe_callback_exec_return(
        const DeuterosAmigaObservedCallbackExecReturn& observation);
    [[nodiscard]] std::optional<DeuterosAmigaTitleServiceSetupLocalPlan>
    observe_service_setup_exec_return(
        const DeuterosAmigaObservedServiceSetupExecReturn& observation) {
        return service_setup_boundary_session_.observe_exec_return(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleSecondServiceLocalPlan>
    observe_second_service_exec_return(
        const DeuterosAmigaObservedServiceSetupExecReturn& observation) {
        return service_setup_boundary_session_.observe_second_exec_return(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleThirdServiceLocalPlan>
    observe_third_service_exec_return(
        const DeuterosAmigaObservedServiceSetupExecReturn& observation) {
        return service_setup_boundary_session_.observe_third_exec_return(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleFourthServiceLocalPlan>
    observe_fourth_service_exec_return(
        const DeuterosAmigaObservedServiceSetupExecReturn& observation) {
        return service_setup_boundary_session_.observe_fourth_exec_return(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleFifthServiceLocalPlan>
    observe_fifth_service_exec_return(
        const DeuterosAmigaObservedServiceSetupExecReturn& observation) {
        return service_setup_boundary_session_.observe_fifth_exec_return(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleControllerPointerSeedPlan>
    advance_controller_pointer_seed();
    [[nodiscard]] std::optional<DeuterosAmigaTitleServiceBatchLocalPlan>
    observe_service_batch_graphics_return(
        const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        return service_batch_boundary_session_.observe_graphics_return(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostServiceWordLocalPlan>
    observe_service_batch_runtime_word(
        const DeuterosAmigaObservedServiceWordRead& observation) {
        return service_batch_boundary_session_.observe_runtime_word(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleGraphicsServiceFirstLocalPlan>
    observe_graphics_service_first_return(
        const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        return service_batch_boundary_session_.observe_graphics_service_first_return(
            observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleGraphicsServiceSecondLocalPlan>
    observe_graphics_service_second_return(
        const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        return service_batch_boundary_session_.observe_graphics_service_second_return(
            observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleGraphicsServiceThirdLocalPlan>
    observe_graphics_service_third_return(
        const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        return service_batch_boundary_session_.observe_graphics_service_third_return(
            observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleTailFirstGraphicsLocalPlan>
    observe_tail_first_graphics_return(
        const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        return service_batch_boundary_session_.observe_tail_first_graphics_return(
            observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleTailCopyLocalPlan>
    observe_tail_copy_words(const DeuterosAmigaObservedTailCopyWords& observation) {
        return service_batch_boundary_session_.observe_tail_copy_words(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleTailSelectionLocalPlan>
    observe_tail_selection_words(
        const DeuterosAmigaObservedTailSelectionWords& observation) {
        return service_batch_boundary_session_.observe_tail_selection_words(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleTailSecondGraphicsLocalPlan>
    observe_tail_second_graphics_return(
        const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        return service_batch_boundary_session_.observe_tail_second_graphics_return(
            observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleTailSelectionLocalPlan>
    observe_tail_repeated_selection_words(
        const DeuterosAmigaObservedTailSelectionWords& observation) {
        return service_batch_boundary_session_.observe_tail_repeated_selection_words(
            observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleTailRepeatedGraphicsLocalPlan>
    observe_tail_repeated_graphics_return(
        const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        return service_batch_boundary_session_.observe_tail_repeated_graphics_return(
            observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleTailRepeatedWrapperReturnPlan>
    observe_tail_repeated_wrapper_graphics_return(
        const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        return service_batch_boundary_session_.observe_tail_repeated_wrapper_graphics_return(
            observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleTailSourceTableLocalPlan>
    observe_tail_source_table(const DeuterosAmigaObservedTailSourceTable& observation) {
        return service_batch_boundary_session_.observe_tail_source_table(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleTailExecReturnLocalPlan>
    observe_tail_exec_return(const DeuterosAmigaObservedTailExecReturn& observation) {
        return service_batch_boundary_session_.observe_tail_exec_return(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleLoadServiceLocalPlan>
    observe_load_service_return(const DeuterosAmigaObservedLocalCallReturn& observation) {
        return service_batch_boundary_session_.observe_load_service_return(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleLoadServiceSelectorPlan>
    observe_load_selector(const DeuterosAmigaObservedLoadSelector& observation) {
        return service_batch_boundary_session_.observe_load_selector(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleLoadCopyChunkPlan>
    observe_load_copy_chunk(const DeuterosAmigaObservedLoadCopyChunk& observation) {
        return service_batch_boundary_session_.observe_load_copy_chunk(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleLoadDispatchTableBasePlan>
    observe_load_dispatch_table_base(
        const DeuterosAmigaObservedLoadDispatchTableBase& observation) {
        return service_batch_boundary_session_.observe_load_dispatch_table_base(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleLoadDispatchLocalPlan>
    observe_load_dispatch_table_word(
        const DeuterosAmigaObservedLoadDispatchTableWord& observation) {
        return service_batch_boundary_session_.observe_load_dispatch_table_word(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandOpcodePlan>
    observe_command_opcode(const DeuterosAmigaObservedTitleCommandOpcode& observation) {
        return service_batch_boundary_session_.observe_command_opcode(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandOperandLocalPlan>
    observe_command_operand_byte(
        const DeuterosAmigaObservedTitleCommandOperandByte& observation) {
        return service_batch_boundary_session_.observe_command_operand_byte(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandPointerCopyPlan>
    observe_command_pointer_long(
        const DeuterosAmigaObservedTitleCommandPointerLong& observation) {
        return service_batch_boundary_session_.observe_command_pointer_long(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandEightPointerPlan>
    observe_command_eight_pointer(
        const DeuterosAmigaObservedTitleCommandEightPointer& observation) {
        return service_batch_boundary_session_.observe_command_eight_pointer(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandEightModePlan>
    observe_command_eight_mode(
        const DeuterosAmigaObservedTitleCommandEightMode& observation) {
        return service_batch_boundary_session_.observe_command_eight_mode(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandEightScalePlan>
    observe_command_eight_scale(
        const DeuterosAmigaObservedTitleCommandEightScale& observation) {
        return service_batch_boundary_session_.observe_command_eight_scale(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandCallReturnPlan>
    observe_command_call_return(
        const DeuterosAmigaObservedTitleCommandCallReturn& observation) {
        return service_batch_boundary_session_.observe_command_call_return(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandTwoOperandModePlan>
    observe_command_two_operand_mode(
        const DeuterosAmigaObservedTitleCommandTwoOperandMode& observation) {
        return service_batch_boundary_session_.observe_command_two_operand_mode(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandTwoOperandsPlan>
    observe_command_two_operands(
        const DeuterosAmigaObservedTitleCommandTwoOperands& observation) {
        return service_batch_boundary_session_.observe_command_two_operands(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandTwoOperandLocalPlan>
    observe_command_two_operand_runtime_long(
        const DeuterosAmigaObservedTitleCommandTwoOperandRuntimeLong& observation) {
        return service_batch_boundary_session_.observe_command_two_operand_runtime_long(
            observation);
    }

private:
    const AmigaAdf* disk_ = nullptr;
    AmigaLoadStage stage_;
    DeuterosAmigaTitleStageProfile profile_;
    DeuterosAmigaTitleEntryPrefix entry_prefix_;
    DeuterosAmigaTitleEntryPrefixState entry_prefix_state_;
    DeuterosAmigaTitleExecPrelude exec_prelude_;
    DeuterosAmigaTitleGraphicsSetupProfile graphics_setup_;
    DeuterosAmigaTitleDisplayClearProfile display_clear_;
    DeuterosAmigaTitleCallbackRegistrationProfile callback_registration_;
    DeuterosAmigaTitleExecBoundarySession exec_boundary_session_;
    std::optional<DeuterosAmigaTitleOpenLibraryBoundarySession>
        open_library_boundary_session_;
    std::optional<DeuterosAmigaTitleCustomChipBoundarySession>
        custom_chip_boundary_session_;
    DeuterosAmigaTitleServiceSetupBoundarySession service_setup_boundary_session_;
    DeuterosAmigaTitleServiceBatchBoundarySession service_batch_boundary_session_;
    std::string original_sha256_;
    bool local_prefix_executed_ = false;
};

} // namespace eon
