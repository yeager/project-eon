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
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandRepeatBytesPlan>
    observe_command_repeat_bytes(
        const DeuterosAmigaObservedTitleCommandRepeatBytes& observation) {
        return service_batch_boundary_session_.observe_command_repeat_bytes(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandRepeatCallReturnPlan>
    observe_command_repeat_call_return(
        const DeuterosAmigaObservedTitleCommandRepeatCallReturn& observation) {
        return service_batch_boundary_session_.observe_command_repeat_call_return(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandHighTableBytePlan>
    observe_command_high_table_byte(
        const DeuterosAmigaObservedTitleCommandHighTableByte& observation) {
        return service_batch_boundary_session_.observe_command_high_table_byte(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandHighCallReturnPlan>
    observe_command_high_call_return(
        const DeuterosAmigaObservedTitleCommandHighCallReturn& observation) {
        return service_batch_boundary_session_.observe_command_high_call_return(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandPlanarWritePlan>
    observe_command_planar_write(
        const DeuterosAmigaObservedTitleCommandPlanarWrite& observation) {
        return service_batch_boundary_session_.observe_command_planar_write(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandPlanarVariantWritePlan>
    observe_command_planar_variant_write(
        const DeuterosAmigaObservedTitleCommandPlanarVariantWrite& observation) {
        return service_batch_boundary_session_.observe_command_planar_variant_write(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandNegativeServicePlan>
    observe_command_negative_service(
        const DeuterosAmigaObservedTitleCommandNegativeService& observation) {
        return service_batch_boundary_session_.observe_command_negative_service(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandPointerRoutePlan>
    observe_post_command_pointer_route(
        const DeuterosAmigaObservedTitlePostCommandPointerRoute& observation) {
        return service_batch_boundary_session_.observe_post_command_pointer_route(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandGraphicsReturnPlan>
    observe_post_command_graphics_return(
        const DeuterosAmigaObservedGraphicsVectorReturn& observation) {
        return service_batch_boundary_session_.observe_post_command_graphics_return(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandFirstDispatchPlan>
    advance_post_command_first_dispatch() {
        return service_batch_boundary_session_.advance_post_command_first_dispatch();
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleFirstDispatchHeaderPlan>
    observe_post_command_first_dispatch_header(
        const DeuterosAmigaObservedTitleFirstDispatchHeader& observation) {
        return service_batch_boundary_session_.observe_post_command_first_dispatch_header(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleFirstDispatchPacketPlan>
    advance_post_command_first_dispatch_packet() {
        return service_batch_boundary_session_.advance_post_command_first_dispatch_packet();
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleFirstDispatchDecodePlan>
    advance_post_command_first_dispatch_decode() {
        return service_batch_boundary_session_.advance_post_command_first_dispatch_decode();
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleFirstDispatchCallerTailPlan>
    advance_post_command_first_dispatch_caller_tail() {
        return service_batch_boundary_session_.advance_post_command_first_dispatch_caller_tail();
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleFirstDispatchMergePlan>
    observe_post_command_first_dispatch_destination_words(
        const DeuterosAmigaObservedTitleFirstDispatchDestinationWords& observation) {
        return service_batch_boundary_session_.observe_post_command_first_dispatch_destination_words(observation);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandSecondDispatchPlan>
    advance_post_command_second_dispatch() { return service_batch_boundary_session_.advance_post_command_second_dispatch(); }
    [[nodiscard]] std::optional<DeuterosAmigaTitleSecondDispatchDecodePlan>
    advance_post_command_second_dispatch_decode() { return service_batch_boundary_session_.advance_post_command_second_dispatch_decode(); }
    [[nodiscard]] std::optional<DeuterosAmigaTitleSecondDispatchMergePlan>
    observe_post_command_second_dispatch_destination_words(const DeuterosAmigaObservedTitleSecondDispatchDestinationWords& o) {
        return service_batch_boundary_session_.observe_post_command_second_dispatch_destination_words(o);
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandServiceRoutePrefixPlan>
    advance_post_command_service_route_prefix() { return service_batch_boundary_session_.advance_post_command_service_route_prefix(); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandServiceFirstReturnPlan>
    observe_post_command_service_first_return(const DeuterosAmigaObservedLocalCallReturn& o) { return service_batch_boundary_session_.observe_post_command_service_first_return(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandServiceSecondReturnPlan>
    observe_post_command_service_second_return(const DeuterosAmigaObservedLocalCallReturn& o) { return service_batch_boundary_session_.observe_post_command_service_second_return(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandServiceThirdReturnPlan> observe_post_command_service_third_return(const DeuterosAmigaObservedLocalCallReturn& o) { return service_batch_boundary_session_.observe_post_command_service_third_return(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandNestedWordsPlan> observe_post_command_nested_words(const DeuterosAmigaObservedTitlePostCommandNestedWords& o) { return service_batch_boundary_session_.observe_post_command_nested_words(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandNestedCallReturnPlan> observe_post_command_nested_call_return(const DeuterosAmigaObservedLocalCallReturn& o) { return service_batch_boundary_session_.observe_post_command_nested_call_return(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandNestedLoopAdvancePlan> advance_post_command_nested_loop() { return service_batch_boundary_session_.advance_post_command_nested_loop(); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandContinuationReturnPlan> observe_post_command_continuation_return(const DeuterosAmigaObservedLocalCallReturn& o) { return service_batch_boundary_session_.observe_post_command_continuation_return(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandPointerChainPlan> observe_post_command_pointer_chain(const DeuterosAmigaObservedTitlePostCommandPointerChain& o) { return service_batch_boundary_session_.observe_post_command_pointer_chain(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandDispatchSetupPlan> observe_post_command_dispatch_destination(const DeuterosAmigaObservedTitlePostCommandDispatchDestination& o) { return service_batch_boundary_session_.observe_post_command_dispatch_destination(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandSelectedStreamPlan> advance_post_command_selected_stream() { return service_batch_boundary_session_.advance_post_command_selected_stream(); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandDescriptorCallPlan> observe_post_command_descriptor_call_return(const DeuterosAmigaObservedLocalCallReturn& o) { return service_batch_boundary_session_.observe_post_command_descriptor_call_return(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandDescriptorLoopPlan> advance_post_command_descriptor_loop() { return service_batch_boundary_session_.advance_post_command_descriptor_loop(); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandDescriptorBytePlan> observe_post_command_descriptor_byte(const DeuterosAmigaObservedTitlePostCommandDescriptorByte& o) { return service_batch_boundary_session_.observe_post_command_descriptor_byte(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandAdjustedDispatchPlan> observe_post_command_adjusted_dispatch_destination(const DeuterosAmigaObservedTitlePostCommandAdjustedDispatchDestination& o) { return service_batch_boundary_session_.observe_post_command_adjusted_dispatch_destination(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjustedCallerPointerPlan> observe_post_adjusted_caller_pointer(const DeuterosAmigaObservedTitlePostAdjustedCallerPointer& o) { return service_batch_boundary_session_.observe_post_adjusted_caller_pointer(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjustedObjectGatePlan> observe_post_adjusted_object_gate(const DeuterosAmigaObservedTitlePostAdjustedObjectGate& o) { return service_batch_boundary_session_.observe_post_adjusted_object_gate(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjustedFirstHelperReturnPlan> observe_post_adjusted_first_helper_return(const DeuterosAmigaObservedLocalCallReturn& o) { return service_batch_boundary_session_.observe_post_adjusted_first_helper_return(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjustedSecondHelperReturnPlan> observe_post_adjusted_second_helper_return(const DeuterosAmigaObservedLocalCallReturn& o) { return service_batch_boundary_session_.observe_post_adjusted_second_helper_return(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjustedRtsFramePlan> observe_post_adjusted_rts_frame(const DeuterosAmigaObservedTitlePostAdjustedRtsFrame& o) { return service_batch_boundary_session_.observe_post_adjusted_rts_frame(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandNestedWordsPlan> observe_post_adjusted_repeated_nested_words(const DeuterosAmigaObservedTitlePostCommandNestedWords& o) { return service_batch_boundary_session_.observe_post_adjusted_repeated_nested_words(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandNestedCallReturnPlan> observe_post_adjusted_repeated_nested_call_return(const DeuterosAmigaObservedLocalCallReturn& o) { return service_batch_boundary_session_.observe_post_adjusted_repeated_nested_call_return(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandNestedLoopAdvancePlan> advance_post_adjusted_repeated_nested_loop() { return service_batch_boundary_session_.advance_post_adjusted_repeated_nested_loop(); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjustedCallerIndirectPlan> advance_post_adjusted_caller_indirect() { return service_batch_boundary_session_.advance_post_adjusted_caller_indirect(); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjustedIndirectReturnPlan> observe_post_adjusted_caller_indirect_return(const DeuterosAmigaObservedTitlePostAdjustedIndirectReturn& o) { return service_batch_boundary_session_.observe_post_adjusted_caller_indirect_return(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjusted37180ReturnPlan> observe_post_adjusted_caller_37180_return(const DeuterosAmigaObservedTitlePostAdjusted37180Return& o) { return service_batch_boundary_session_.observe_post_adjusted_caller_37180_return(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjustedModeReturnPlan> observe_post_adjusted_mode_return(const DeuterosAmigaObservedLocalCallReturn& o) { return service_batch_boundary_session_.observe_post_adjusted_mode_return(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjusted222c0ReturnPlan> observe_post_adjusted_222c0_return(const DeuterosAmigaObservedLocalCallReturn& o) { return service_batch_boundary_session_.observe_post_adjusted_222c0_return(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjustedTimerStatePlan> observe_post_adjusted_timer_state(const DeuterosAmigaObservedTitlePostAdjustedTimerState& o) { return service_batch_boundary_session_.observe_post_adjusted_timer_state(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjusted4069aReturnPlan> observe_post_adjusted_4069a_return(const DeuterosAmigaObservedLocalCallReturn& o) { return service_batch_boundary_session_.observe_post_adjusted_4069a_return(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjustedJoinBytePlan> observe_post_adjusted_join_byte(const DeuterosAmigaObservedTitlePostAdjustedJoinByte& o) { return service_batch_boundary_session_.observe_post_adjusted_join_byte(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjusted1f9a4ReturnPlan> observe_post_adjusted_1f9a4_return(const DeuterosAmigaObservedLocalCallReturn& o) { return service_batch_boundary_session_.observe_post_adjusted_1f9a4_return(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjusted1fe88ReturnPlan> observe_post_adjusted_1fe88_return(const DeuterosAmigaObservedTitlePostAdjusted1fe88Return& o) { return service_batch_boundary_session_.observe_post_adjusted_1fe88_return(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjustedFinalGatePlan> observe_post_adjusted_final_gate(const DeuterosAmigaObservedTitlePostAdjustedFinalGate& o) { return service_batch_boundary_session_.observe_post_adjusted_final_gate(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjustedInputReturnPlan> observe_post_adjusted_input_return(const DeuterosAmigaObservedTitlePostAdjustedInputReturn& o) { return service_batch_boundary_session_.observe_post_adjusted_input_return(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostAdjustedRepeatedInputReturnPlan> observe_post_adjusted_repeated_input_return(const DeuterosAmigaObservedLocalCallReturn& o) { return service_batch_boundary_session_.observe_post_adjusted_repeated_input_return(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitleTailCopyPlan> observe_title_tail_copy(const DeuterosAmigaObservedTitleTailCopy& o) { return service_batch_boundary_session_.observe_title_tail_copy(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitleTailSubroutinePlan> advance_title_tail_subroutine() { return service_batch_boundary_session_.advance_title_tail_subroutine(); }
    [[nodiscard]] std::optional<DeuterosAmigaTitleTailInitialServicePlan> observe_title_tail_initial_service_return(const DeuterosAmigaObservedLocalCallReturn& o) { return service_batch_boundary_session_.observe_title_tail_initial_service_return(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitleTailExecPlan> observe_title_tail_exec_return(const DeuterosAmigaObservedTitleTailExecReturn& o) { return service_batch_boundary_session_.observe_title_tail_exec_return(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitleTailComparePlan> observe_title_tail_compare_longs(const DeuterosAmigaObservedTitleTailCompareLongs& o) { return service_batch_boundary_session_.observe_title_tail_compare_longs(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitleTailBootstrapPlan> observe_title_tail_controller_long(const DeuterosAmigaObservedTitleTailControllerLong& o) { return service_batch_boundary_session_.observe_title_tail_controller_long(o); }
    [[nodiscard]] std::optional<DeuterosAmigaTitleProfileTwoBootstrapPlan> advance_title_profile_two_bootstrap() { return service_batch_boundary_session_.advance_title_profile_two_bootstrap(); }
    [[nodiscard]] std::optional<DeuterosAmigaMainStageReentryPrefixPlan> observe_main_stage_reentry_d0(const DeuterosAmigaObservedMainStageReentryD0& o) { return service_batch_boundary_session_.observe_main_stage_reentry_d0(o); }
    [[nodiscard]] std::optional<DeuterosAmigaMainStageFirstExecReturnPlan> observe_main_stage_first_exec_return(const DeuterosAmigaObservedMainStageExecReturn& o) { return service_batch_boundary_session_.observe_main_stage_first_exec_return(o); }

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
