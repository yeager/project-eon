#pragma once

#include "data/amiga_adf.hpp"
#include "data/deuteros_amiga_bundle.hpp"
#include "data/deuteros_amiga_channel_vm.hpp"
#include "data/deuteros_amiga_audio.hpp"
#include "data/deuteros_amiga_alternate_renderer.hpp"
#include "data/deuteros_amiga_frame.hpp"
#include "data/deuteros_amiga_loader.hpp"
#include "engine/deuteros_amiga_title_stage_session.hpp"
#include "engine/deuteros_amiga_title_bootstrap_session.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace eon {

// A native self-consistency checkpoint for the already recovered opening
// only. It is not emulator output, a capture receipt, title-stage evidence or
// a parity claim. Hashes describe frames already composed from the exact ADFs.
struct DeuterosAmigaOpeningCheckpoint {
    std::uint64_t tick = 0;
    std::uint32_t vblank_counter = 0;
    bool input_gate = false;
    std::string indexed_frame_sha256;
    std::string rgba_frame_sha256;
};

// Live, original-data-backed opening sequence. It owns the ADF image and all
// VM state, advances the verified VBL source once per scheduler tick, and
// exposes an explicit handoff event rather than manufacturing a game screen.
class DeuterosAmigaOpening {
public:
    // Both original disks are required for a runtime session. The recovered
    // opening reads only disk 1's caller-proved ranges, but disk 2 remains
    // identity-bound instead of being silently omitted or substituted.
    DeuterosAmigaOpening(std::vector<std::uint8_t> system_adf,
        std::vector<std::uint8_t> data_adf);

    [[nodiscard]] DeuterosAmigaVmEvents tick(bool input_pressed = false);
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> rgba_frame() const;
    [[nodiscard]] std::optional<DeuterosAmigaOpeningCheckpoint> checkpoint() const;
    [[nodiscard]] bool frame_composed_on_last_tick() const { return frame_composed_on_last_tick_; }
    [[nodiscard]] std::uint64_t ticks() const { return ticks_; }
    [[nodiscard]] std::uint32_t vblank_counter() const { return random_.vblank_counter(); }
    // These are raw opening-VM observables used by the provenance overlay.
    // They are not title/gameplay labels or host controls.
    [[nodiscard]] bool input_gate() const { return vm_.input_gate(); }
    [[nodiscard]] std::uint16_t palette_index() const { return vm_.palette_index(); }
    [[nodiscard]] std::size_t active_channel_count() const;
    [[nodiscard]] bool title_handed_off() const { return title_handed_off_; }
    [[nodiscard]] const DeuterosAmigaBootstrapProfile& title_handoff_profile() const {
        return load_plan_.title_handoff_profile;
    }
    [[nodiscard]] const DeuterosAmigaTitleHandoffRoute& title_handoff_route() const {
        return title_handoff_route_;
    }
    [[nodiscard]] const DeuterosAmigaSoundBank& sound_bank() const { return sound_bank_; }
    [[nodiscard]] const std::optional<DeuterosAmigaAlternateRendererTrace>& alternate_renderer_trace() const {
        return alternate_renderer_trace_;
    }
    [[nodiscard]] const std::optional<DeuterosAmigaTitleStageSession>& title_stage_session() const {
        return title_stage_session_;
    }
    [[nodiscard]] std::optional<DeuterosAmigaTitleStageSession::LocalPrefixAdvance> advance_title_local_prefix() { return title_stage_session_ ? title_stage_session_->execute_local_prefix() : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleExecBoundaryCheckpoint> observe_title_exec_return(const DeuterosAmigaObservedExecReturn& o) { return title_stage_session_ ? title_stage_session_->observe_exec_return(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleOpenLibraryBoundaryCheckpoint> observe_title_open_library_return(const DeuterosAmigaObservedOpenLibraryReturn& o) { return title_stage_session_ ? title_stage_session_->observe_open_library_return(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostOpenLibraryLocalAdvance> advance_title_post_open_library_local_path() { return title_stage_session_ ? title_stage_session_->advance_post_open_library_local_path() : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleDisplayLocalAdvance> observe_title_display_base(const DeuterosAmigaObservedDisplayBaseRead& o) { return title_stage_session_ ? title_stage_session_->observe_display_base_and_advance(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCallbackRegistrationLocalPlan> observe_title_custom_chip_write(const DeuterosAmigaObservedCustomChipWrite& o) { return title_stage_session_ ? title_stage_session_->observe_custom_chip_write(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCallbackRegistrationAdvance> observe_title_callback_exec_return(const DeuterosAmigaObservedCallbackExecReturn& o) { return title_stage_session_ ? title_stage_session_->observe_callback_exec_return(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleServiceSetupLocalPlan> observe_title_service_setup_exec_return(const DeuterosAmigaObservedServiceSetupExecReturn& o) { return title_stage_session_ ? title_stage_session_->observe_service_setup_exec_return(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleSecondServiceLocalPlan> observe_title_second_service_exec_return(const DeuterosAmigaObservedServiceSetupExecReturn& o) { return title_stage_session_ ? title_stage_session_->observe_second_service_exec_return(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleThirdServiceLocalPlan> observe_title_third_service_exec_return(const DeuterosAmigaObservedServiceSetupExecReturn& o) { return title_stage_session_ ? title_stage_session_->observe_third_service_exec_return(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleFourthServiceLocalPlan> observe_title_fourth_service_exec_return(const DeuterosAmigaObservedServiceSetupExecReturn& o) { return title_stage_session_ ? title_stage_session_->observe_fourth_service_exec_return(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleFifthServiceLocalPlan> observe_title_fifth_service_exec_return(const DeuterosAmigaObservedServiceSetupExecReturn& o) { return title_stage_session_ ? title_stage_session_->observe_fifth_service_exec_return(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleControllerPointerSeedPlan> advance_title_controller_pointer_seed() { return title_stage_session_ ? title_stage_session_->advance_controller_pointer_seed() : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleServiceBatchLocalPlan> observe_title_service_batch_graphics_return(const DeuterosAmigaObservedGraphicsVectorReturn& o) { return title_stage_session_ ? title_stage_session_->observe_service_batch_graphics_return(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostServiceWordLocalPlan> observe_title_service_batch_runtime_word(const DeuterosAmigaObservedServiceWordRead& o) { return title_stage_session_ ? title_stage_session_->observe_service_batch_runtime_word(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleGraphicsServiceFirstLocalPlan> observe_title_graphics_service_first_return(const DeuterosAmigaObservedGraphicsVectorReturn& o) { return title_stage_session_ ? title_stage_session_->observe_graphics_service_first_return(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleGraphicsServiceSecondLocalPlan> observe_title_graphics_service_second_return(const DeuterosAmigaObservedGraphicsVectorReturn& o) { return title_stage_session_ ? title_stage_session_->observe_graphics_service_second_return(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleGraphicsServiceThirdLocalPlan> observe_title_graphics_service_third_return(const DeuterosAmigaObservedGraphicsVectorReturn& o) { return title_stage_session_ ? title_stage_session_->observe_graphics_service_third_return(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleTailFirstGraphicsLocalPlan> observe_title_tail_first_graphics_return(const DeuterosAmigaObservedGraphicsVectorReturn& o) { return title_stage_session_ ? title_stage_session_->observe_tail_first_graphics_return(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleTailCopyLocalPlan> observe_title_tail_copy_words(const DeuterosAmigaObservedTailCopyWords& o) { return title_stage_session_ ? title_stage_session_->observe_tail_copy_words(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleTailSelectionLocalPlan> observe_title_tail_selection_words(const DeuterosAmigaObservedTailSelectionWords& o) { return title_stage_session_ ? title_stage_session_->observe_tail_selection_words(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleTailSecondGraphicsLocalPlan> observe_title_tail_second_graphics_return(const DeuterosAmigaObservedGraphicsVectorReturn& o) { return title_stage_session_ ? title_stage_session_->observe_tail_second_graphics_return(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleTailSelectionLocalPlan> observe_title_tail_repeated_selection_words(const DeuterosAmigaObservedTailSelectionWords& o) { return title_stage_session_ ? title_stage_session_->observe_tail_repeated_selection_words(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleTailRepeatedGraphicsLocalPlan> observe_title_tail_repeated_graphics_return(const DeuterosAmigaObservedGraphicsVectorReturn& o) { return title_stage_session_ ? title_stage_session_->observe_tail_repeated_graphics_return(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleTailRepeatedWrapperReturnPlan> observe_title_tail_repeated_wrapper_graphics_return(const DeuterosAmigaObservedGraphicsVectorReturn& o) { return title_stage_session_ ? title_stage_session_->observe_tail_repeated_wrapper_graphics_return(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleTailSourceTableLocalPlan> observe_title_tail_source_table(const DeuterosAmigaObservedTailSourceTable& o) { return title_stage_session_ ? title_stage_session_->observe_tail_source_table(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleTailExecReturnLocalPlan> observe_title_tail_exec_return(const DeuterosAmigaObservedTailExecReturn& o) { return title_stage_session_ ? title_stage_session_->observe_tail_exec_return(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleLoadServiceLocalPlan> observe_title_load_service_return(const DeuterosAmigaObservedLocalCallReturn& o) { return title_stage_session_ ? title_stage_session_->observe_load_service_return(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleLoadServiceSelectorPlan> observe_title_load_selector(const DeuterosAmigaObservedLoadSelector& o) { return title_stage_session_ ? title_stage_session_->observe_load_selector(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleLoadCopyChunkPlan> observe_title_load_copy_chunk(const DeuterosAmigaObservedLoadCopyChunk& o) { return title_stage_session_ ? title_stage_session_->observe_load_copy_chunk(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleLoadDispatchTableBasePlan> observe_title_load_dispatch_table_base(const DeuterosAmigaObservedLoadDispatchTableBase& o) { return title_stage_session_ ? title_stage_session_->observe_load_dispatch_table_base(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleLoadDispatchLocalPlan> observe_title_load_dispatch_table_word(const DeuterosAmigaObservedLoadDispatchTableWord& o) { return title_stage_session_ ? title_stage_session_->observe_load_dispatch_table_word(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandOpcodePlan> observe_title_command_opcode(const DeuterosAmigaObservedTitleCommandOpcode& o) { return title_stage_session_ ? title_stage_session_->observe_command_opcode(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandOperandLocalPlan> observe_title_command_operand_byte(const DeuterosAmigaObservedTitleCommandOperandByte& o) { return title_stage_session_ ? title_stage_session_->observe_command_operand_byte(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandPointerCopyPlan> observe_title_command_pointer_long(const DeuterosAmigaObservedTitleCommandPointerLong& o) { return title_stage_session_ ? title_stage_session_->observe_command_pointer_long(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandEightPointerPlan> observe_title_command_eight_pointer(const DeuterosAmigaObservedTitleCommandEightPointer& o) { return title_stage_session_ ? title_stage_session_->observe_command_eight_pointer(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandEightModePlan> observe_title_command_eight_mode(const DeuterosAmigaObservedTitleCommandEightMode& o) { return title_stage_session_ ? title_stage_session_->observe_command_eight_mode(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandEightScalePlan> observe_title_command_eight_scale(const DeuterosAmigaObservedTitleCommandEightScale& o) { return title_stage_session_ ? title_stage_session_->observe_command_eight_scale(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandCallReturnPlan> observe_title_command_call_return(const DeuterosAmigaObservedTitleCommandCallReturn& o) { return title_stage_session_ ? title_stage_session_->observe_command_call_return(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandPlanarWritePlan> observe_title_command_planar_write(const DeuterosAmigaObservedTitleCommandPlanarWrite& o) { return title_stage_session_ ? title_stage_session_->observe_command_planar_write(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandPlanarVariantWritePlan> observe_title_command_planar_variant_write(const DeuterosAmigaObservedTitleCommandPlanarVariantWrite& o) { return title_stage_session_ ? title_stage_session_->observe_command_planar_variant_write(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleCommandNegativeServicePlan> observe_title_command_negative_service(const DeuterosAmigaObservedTitleCommandNegativeService& o) { return title_stage_session_ ? title_stage_session_->observe_command_negative_service(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandPointerRoutePlan> observe_title_post_command_pointer_route(const DeuterosAmigaObservedTitlePostCommandPointerRoute& o) { return title_stage_session_ ? title_stage_session_->observe_post_command_pointer_route(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandGraphicsReturnPlan> observe_title_post_command_graphics_return(const DeuterosAmigaObservedGraphicsVectorReturn& o) { return title_stage_session_ ? title_stage_session_->observe_post_command_graphics_return(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandFirstDispatchPlan> advance_title_post_command_first_dispatch() { return title_stage_session_ ? title_stage_session_->advance_post_command_first_dispatch() : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleFirstDispatchHeaderPlan> observe_title_post_command_first_dispatch_header(const DeuterosAmigaObservedTitleFirstDispatchHeader& o) { return title_stage_session_ ? title_stage_session_->observe_post_command_first_dispatch_header(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleFirstDispatchPacketPlan> advance_title_post_command_first_dispatch_packet() { return title_stage_session_ ? title_stage_session_->advance_post_command_first_dispatch_packet() : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleFirstDispatchDecodePlan> advance_title_post_command_first_dispatch_decode() { return title_stage_session_ ? title_stage_session_->advance_post_command_first_dispatch_decode() : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleFirstDispatchCallerTailPlan> advance_title_post_command_first_dispatch_caller_tail() { return title_stage_session_ ? title_stage_session_->advance_post_command_first_dispatch_caller_tail() : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleFirstDispatchMergePlan> observe_title_post_command_first_dispatch_destination_words(const DeuterosAmigaObservedTitleFirstDispatchDestinationWords& o) { return title_stage_session_ ? title_stage_session_->observe_post_command_first_dispatch_destination_words(o) : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitlePostCommandSecondDispatchPlan> advance_title_post_command_second_dispatch() { return title_stage_session_ ? title_stage_session_->advance_post_command_second_dispatch() : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleSecondDispatchDecodePlan> advance_title_post_command_second_dispatch_decode() { return title_stage_session_ ? title_stage_session_->advance_post_command_second_dispatch_decode() : std::nullopt; }
    [[nodiscard]] std::optional<DeuterosAmigaTitleSecondDispatchMergePlan> observe_title_post_command_second_dispatch_destination_words(const DeuterosAmigaObservedTitleSecondDispatchDestinationWords& o) { return title_stage_session_ ? title_stage_session_->observe_post_command_second_dispatch_destination_words(o) : std::nullopt; }
    [[nodiscard]] const std::optional<DeuterosAmigaTitleBootstrapSession>&
    title_bootstrap_session() const { return title_bootstrap_session_; }

private:
    AmigaAdf disk_;
    AmigaAdf data_disk_;
    DeuterosAmigaLoadPlan load_plan_;
    DeuterosAmigaTitleHandoffRoute title_handoff_route_;
    DeuterosAmigaMainResourceTransfer transferred_bundle_;
    DeuterosAmigaBundle bundle_;
    DeuterosAmigaSoundBank sound_bank_;
    DeuterosAmigaIndexedBlob blob_;
    DeuterosAmigaChannelVm vm_;
    DeuterosAmigaRandom random_;
    DeuterosAmigaCompositor compositor_;
    std::optional<DeuterosAmigaFrame> last_frame_;
    std::optional<DeuterosAmigaAlternateRendererTrace> alternate_renderer_trace_;
    std::optional<DeuterosAmigaTitleStageSession> title_stage_session_;
    std::optional<DeuterosAmigaTitleBootstrapSession> title_bootstrap_session_;
    bool title_handed_off_ = false;
    bool frame_composed_on_last_tick_ = false;
    std::uint64_t ticks_ = 0;
};

} // namespace eon
