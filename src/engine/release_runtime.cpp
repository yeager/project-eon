#include "engine/release_runtime.hpp"
#include "engine/release_runtime_capability.hpp"

#include "platform/game_data.hpp"
#include "data/reference_trace.hpp"
#include "data/reference_trace_registry.hpp"
#include "data/sha256.hpp"
#include "data/fat12.hpp"
#include "data/function_map.hpp"
#include "data/native_code_image_admission.hpp"
#include "data/millennium_dos_bitmap.hpp"
#include "data/millennium_dos_gameplay_screen.hpp"
#include "data/millennium_dos_lib.hpp"
#include "data/millennium_dos_title_presentation.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace eon {

std::string_view release_runtime_admission_label(const ReleaseRuntimeAdmission admission) {
    switch (admission) {
    case ReleaseRuntimeAdmission::unselected: return "NOT SELECTED";
    case ReleaseRuntimeAdmission::active: return "READY";
    case ReleaseRuntimeAdmission::identity_rejected: return "REJECTED: IDENTITY";
    case ReleaseRuntimeAdmission::archive_rejected: return "REJECTED: ARCHIVE HASH";
    case ReleaseRuntimeAdmission::adapter_rejected: return "REJECTED: ADAPTER";
    }
    return "REJECTED: ADAPTER";
}

std::string_view release_runtime_rejection_label(const ReleaseRuntimeRejection rejection) {
    switch (rejection) {
    case ReleaseRuntimeRejection::none: return "NONE";
    case ReleaseRuntimeRejection::launch_identity: return "LAUNCH IDENTITY";
    case ReleaseRuntimeRejection::original_media: return "ORIGINAL MEDIA";
    case ReleaseRuntimeRejection::runtime_capability: return "RUNTIME CAPABILITY";
    case ReleaseRuntimeRejection::adapter_construction: return "ADAPTER CONSTRUCTION";
    case ReleaseRuntimeRejection::input_contract: return "INPUT CONTRACT";
    case ReleaseRuntimeRejection::child_session: return "CHILD SESSION";
    case ReleaseRuntimeRejection::lifecycle_transition: return "LIFECYCLE TRANSITION";
    }
    return "ADAPTER CONSTRUCTION";
}

bool ReleaseRuntimeCoordinator::acquire(const ResolvedLaunchRequest& launch) {
    reset();
    // A launcher card can produce this object only through exact hash
    // resolution, but make that invariant explicit at the runtime boundary
    // too. A stale or forged DTO may never retain a previous source.
    if (!launch.request.game || !launch.request.platform || !launch.request.release_sha256
        || !launch.request.release_language
        || *launch.request.game != launch.release.game
        || *launch.request.platform != launch.release.platform
        || *launch.request.release_sha256 != launch.release.sha256
        || *launch.request.release_language != launch.release.language) {
        admission_ = ReleaseRuntimeAdmission::identity_rejected;
        rejection_ = ReleaseRuntimeRejection::launch_identity;
        return false;
    }
    if (!release_runtime_capability_manifest_is_valid()) {
        admission_ = ReleaseRuntimeAdmission::adapter_rejected;
        rejection_ = ReleaseRuntimeRejection::runtime_capability;
        return false;
    }
    std::optional<VerifiedReleaseMedia> media;
    try {
        media = VerifiedReleaseMedia::open(launch.release);
    } catch (...) {
        admission_ = ReleaseRuntimeAdmission::archive_rejected;
        rejection_ = ReleaseRuntimeRejection::original_media;
        return false;
    }
    if (!verified_release_media_has_declared_profile_ranges(*media)) {
        // A whole-container hash is necessary but not sufficient for an
        // adapter: every parser interval it could report must still name an
        // exact available leaf within the just-admitted media snapshot.
        admission_ = ReleaseRuntimeAdmission::adapter_rejected;
        rejection_ = ReleaseRuntimeRejection::runtime_capability;
        return false;
    }
    if (!function_map_entries_are_attested_by_media(*media)) {
        // The F10/CLI function map is only meaningful when every displayed
        // source-span digest is recomputed from this exact admitted media
        // snapshot. Do not let diagnostics silently outlive a changed parser
        // range or a detached original leaf.
        admission_ = ReleaseRuntimeAdmission::adapter_rejected;
        rejection_ = ReleaseRuntimeRejection::runtime_capability;
        return false;
    }
    const auto capability = release_runtime_capability_for(launch.release);
    if (!capability) {
        admission_ = ReleaseRuntimeAdmission::identity_rejected;
        rejection_ = ReleaseRuntimeRejection::runtime_capability;
        return false;
    }
    // Construct one typed adapter into local storage before publishing the
    // new identity. A failed leaf/parser admission must not leave a previous
    // adapter or a half-built replacement observable to SDL.
    std::optional<MillenniumDosRuntimeAssets> millennium_dos;
    std::unique_ptr<MillenniumDosSoundSelectionSession> millennium_dos_sound_selection;
    std::unique_ptr<MillenniumDosTitleSession> millennium_dos_title;
    std::optional<MillenniumDosNativeProcessAdmission> millennium_dos_native_process;
    std::unique_ptr<MillenniumAmigaBootstrapSession> millennium_amiga;
    std::optional<MillenniumAmigaBootstrapRelocatorSession> millennium_amiga_relocator;
    NativeRuntimeMemory runtime_memory;
    std::unique_ptr<MillenniumAtariBootstrapSession> millennium_atari;
    std::optional<MillenniumAtariConfigConsumerSession> millennium_atari_config_consumer;
    std::unique_ptr<DeuterosAmigaOpening> deuteros_amiga;
    std::unique_ptr<DeuterosAmigaPaulaMixer> deuteros_amiga_paula;
    std::unique_ptr<DeuterosAtariBootstrapSession> deuteros_atari;
    std::optional<RuntimeSessionSnapshot> session_snapshot;
    switch (capability->adapter) {
    case ReleaseRuntimeAdapter::millennium_dos:
            millennium_dos = load_millennium_dos_runtime(*media);
            break;
    case ReleaseRuntimeAdapter::millennium_amiga:
            millennium_amiga = load_millennium_amiga_runtime(*media);
            break;
    case ReleaseRuntimeAdapter::millennium_atari:
            millennium_atari = load_millennium_atari_runtime(*media);
            break;
    case ReleaseRuntimeAdapter::deuteros_amiga:
            deuteros_amiga = load_deuteros_amiga_runtime(*media);
            break;
    case ReleaseRuntimeAdapter::deuteros_atari:
            deuteros_atari = load_deuteros_atari_runtime(*media);
            break;
    }
    if (millennium_dos || millennium_amiga || millennium_atari || deuteros_amiga || deuteros_atari) {
        session_snapshot = make_runtime_session_snapshot(launch, capability->initial_kind);
        // The table is the declared admission contract; a changed generic
        // kind helper must not silently broaden this release's boundary.
        session_snapshot->boundary = capability->initial_boundary;
        session_snapshot->capabilities = capability->initial_capabilities;
    }
    if (millennium_dos && launch.release.game == Game::millennium
        && launch.release.platform == Platform::dos && launch.release.language == "en") {
        const auto game = admit_native_code_image(*media,
            "millennium-dos-2200ad-exe-linear", "millennium-dos-game-flow");
        if (!game.accepted()) {
            admission_ = ReleaseRuntimeAdmission::adapter_rejected;
            rejection_ = ReleaseRuntimeRejection::adapter_construction;
            return false;
        }
        auto prepared = MillenniumDosNativeProcessAdmission::startup(
            launch.release.sha256, game.view->bytes);
        const auto checkpoint = prepared.checkpoint();
        if (!checkpoint || !checkpoint->static_recovery_entry
            || checkpoint->recovery_entry != MillenniumDosNativeRecoveryEntry::startup
            || checkpoint->state
                != MillenniumDosNativeProcessState::startup_first_private_interrupt
            || checkpoint->boundary.kind != MillenniumDosNativeBoundaryKind::private_interrupt
            || checkpoint->boundary.address != 0x0129
            || checkpoint->boundary.interrupt != std::optional<std::uint8_t>{0x91}) {
            admission_ = ReleaseRuntimeAdmission::adapter_rejected;
            rejection_ = ReleaseRuntimeRejection::adapter_construction;
            return false;
        }
        millennium_dos_native_process.emplace(std::move(prepared));
    }
    if (!session_snapshot || (!millennium_dos && !millennium_amiga && !millennium_atari
        && !deuteros_amiga && !deuteros_atari)) {
        admission_ = ReleaseRuntimeAdmission::adapter_rejected;
        rejection_ = ReleaseRuntimeRejection::adapter_construction;
        return false;
    }
    if (deuteros_amiga) {
        try {
            deuteros_amiga_paula = std::make_unique<DeuterosAmigaPaulaMixer>(
                deuteros_amiga->sound_bank());
        } catch (...) {
            admission_ = ReleaseRuntimeAdmission::adapter_rejected;
            rejection_ = ReleaseRuntimeRejection::adapter_construction;
            return false;
        }
    }
    // The release table may narrow presentation/audio facts, but it must
    // never disagree with the separately declared input envelope.
    if (session_snapshot->capabilities.admitted_input
            != runtime_input_contract_admits_host_observation(session_snapshot->input_contract)
        || session_snapshot->input_contract
            != runtime_input_contract_for_session(session_snapshot->kind)) {
        admission_ = ReleaseRuntimeAdmission::adapter_rejected;
        rejection_ = ReleaseRuntimeRejection::input_contract;
        return false;
    }
    try {
        // These two objects stay inside the release-bound coordinator, rather
        // than allowing SDL to manufacture or retain a DOS input state. Their
        // constructors validate the exact parser evidence again.
        if (millennium_dos && millennium_dos->sound_selection && millennium_dos->sound_selection_prompt) {
            millennium_dos_sound_selection = std::make_unique<MillenniumDosSoundSelectionSession>(
                *millennium_dos->sound_selection, millennium_dos->sound_blaster_driver,
                millennium_dos->covox_driver);
        } else if (millennium_dos && millennium_dos->title_flow) {
            millennium_dos_title = std::make_unique<MillenniumDosTitleSession>(*millennium_dos->title_flow);
        } else if (millennium_dos && millennium_dos->spanish_title_boundary) {
            millennium_dos_title = std::make_unique<MillenniumDosTitleSession>(
                *millennium_dos->spanish_title_boundary);
        }
    } catch (...) {
        reset();
        admission_ = ReleaseRuntimeAdmission::adapter_rejected;
        rejection_ = ReleaseRuntimeRejection::child_session;
        return false;
    }
    try {
        constexpr std::string_view direct_defjam_release=
            "ec0424445d494809d2661492e289af71b056a429dde13b053a472ccc8347d4dd";
        constexpr std::string_view defjam_adf=
            "8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c";
        if(millennium_amiga&&launch.release.sha256==direct_defjam_release){
            const auto disk=media->extract(defjam_adf);
            if(!disk)throw std::runtime_error("Direct Defjam relocator image is unavailable");
            millennium_amiga_relocator.emplace(*disk);
            NativeRuntimeEffectBatch batch{"millennium-amiga-bootstrap-relocator-1-prefix",true,{}};
            batch.effects.reserve(millennium_amiga_relocator->copy_effects().size());
            std::size_t order=1;
            for(const auto& effect:millennium_amiga_relocator->copy_effects())batch.effects.push_back({order++,{NativeRuntimeAddressSpace::linear,std::nullopt,effect.destination_address},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,effect.value});
            const auto applied=runtime_memory.apply(batch);
            if(!applied.accepted)throw std::runtime_error(applied.error);
        }
    } catch (...) {
        reset(); admission_=ReleaseRuntimeAdmission::adapter_rejected;
        rejection_=ReleaseRuntimeRejection::child_session; return false;
    }
    try {
        if (millennium_atari) {
            // Both batches are applied to the unpublished acquisition-local
            // memory. A failure in either leaves no partial Atari generation
            // reachable through the coordinator or RuntimeHost.
            const auto image_applied = runtime_memory.apply(
                make_atari_st_prg_load_effect_batch(
                    millennium_atari->native_prg_image(), "millennium-atari-1-prg"));
            if (!image_applied.accepted) throw std::runtime_error(image_applied.error);
            const auto config_applied = runtime_memory.apply(
                millennium_atari->read_only_gemdos().make_fread_effect_batch(
                    "millennium-atari-1-config"));
            if (!config_applied.accepted) throw std::runtime_error(config_applied.error);
            millennium_atari_config_consumer.emplace(1, runtime_memory,
                millennium_atari->read_only_gemdos().checkpoint(),
                millennium_atari->fread_config_load_address_boundary(),
                millennium_atari->fread_mapped_config_prelude());
        }
    } catch (...) {
        reset(); admission_ = ReleaseRuntimeAdmission::adapter_rejected;
        rejection_ = ReleaseRuntimeRejection::child_session; return false;
    }
    millennium_dos_ = std::move(millennium_dos);
    millennium_dos_sound_selection_ = std::move(millennium_dos_sound_selection);
    millennium_dos_title_ = std::move(millennium_dos_title);
    millennium_dos_native_process_ = std::move(millennium_dos_native_process);
    millennium_amiga_ = std::move(millennium_amiga);
    millennium_amiga_relocator_=std::move(millennium_amiga_relocator);
    millennium_amiga_relocator_generation_=millennium_amiga_relocator_?1:0;
    millennium_atari_ = std::move(millennium_atari);
    millennium_atari_config_consumer_ = std::move(millennium_atari_config_consumer);
    deuteros_amiga_ = std::move(deuteros_amiga);
    deuteros_amiga_paula_ = std::move(deuteros_amiga_paula);
    deuteros_atari_ = std::move(deuteros_atari);
    session_snapshot_ = std::move(session_snapshot);
    active_ = launch;
    native_runtime_memory_=std::move(runtime_memory);
    admission_ = ReleaseRuntimeAdmission::active;
    rejection_ = ReleaseRuntimeRejection::none;
    return true;
}

void ReleaseRuntimeCoordinator::reset() {
    native_runtime_memory_.reset();
    millennium_atari_config_consumer_.reset();
    millennium_dos_title_to_game_.reset();
    millennium_dos_title_to_game_generation_ = 0;
    millennium_dos_title_to_game_last_sequence_ = 0;
    millennium_amiga_relocator_.reset();
    millennium_amiga_relocator_generation_=0;
    millennium_amiga_relocator_overread_sequence_.reset();
    millennium_amiga_relocator_terminal_sequence_.reset();
    deuteros_amiga_title_load_copy_.reset();
    deuteros_amiga_title_load_copy_generation_ = 0;
    deuteros_amiga_title_command_generation_ = 0;
    deuteros_amiga_title_planar_base_.reset();
    deuteros_amiga_title_planar_generation_ = 0;
    deuteros_amiga_title_planar_surface_.reset();
    millennium_dos_handler_completion_.reset();
    millennium_dos_tenth_function_.reset();
    millennium_dos_seventh_function_.reset();
    millennium_dos_sixth_function_.reset();
    millennium_dos_eighth_function_.reset();
    millennium_dos_ninth_function_.reset();
    millennium_dos_ninth_handoff_.reset();
    millennium_dos_fourth_function_.reset();
    millennium_dos_fifth_function_.reset();
    millennium_dos_third_function_.reset();
    millennium_dos_first_function_.reset();
    millennium_dos_second_function_.reset();
    millennium_dos_second_function_callback_.reset();
    millennium_dos_bdf_terminal_transfer_.reset();
    millennium_dos_bdf_mode_two_.reset();
    millennium_dos_bdf_other_mode_.reset();
    millennium_dos_shared_helper_.reset();
    millennium_dos_shared_helper_entry_.reset();
    millennium_dos_shared_helper_return_.reset();
    millennium_dos_special_action_.reset();
    millennium_dos_special_action_return_.reset();
    millennium_dos_second_special_action_return_.reset();
    millennium_dos_gx_adapter_return_.reset();
    millennium_dos_gx_overlay_return_.reset();
    millennium_dos_gx_adapter_entry_.reset();
    millennium_dos_gx_adapter_transfer_.reset();
    millennium_dos_gx_adapter_segment_.reset();
    millennium_dos_gx_adapter_.reset();
    millennium_dos_bdf_service_.reset();
    millennium_dos_second_function_callback_transfer_.reset();
    millennium_dos_gx_startup_.reset();
    millennium_dos_post_overlay_loop_.reset();
    millennium_dos_native_process_.reset();
    millennium_dos_sound_driver_load_.reset();
    millennium_dos_compatibility_runner_.reset();
    millennium_dos_title_exec_entry_.reset();
    millennium_dos_title_child_compatibility_.reset();
    millennium_dos_title_initialization_.reset();
    millennium_dos_sound_driver_load_generation_ = 0;
    millennium_dos_sound_driver_load_last_sequence_ = 0;
    millennium_dos_sound_selection_.reset();
    millennium_dos_title_.reset();
    millennium_dos_.reset();
    millennium_amiga_.reset();
    millennium_atari_.reset();
    deuteros_amiga_paula_.reset();
    deuteros_amiga_title_display_trace_.reset();
    deuteros_amiga_title_fifth_service_plan_.reset();
    deuteros_amiga_title_fourth_service_plan_.reset();
    deuteros_amiga_title_third_service_plan_.reset();
    deuteros_amiga_title_second_service_plan_.reset();
    deuteros_amiga_title_service_setup_plan_.reset();
    deuteros_amiga_.reset();
    deuteros_amiga_opening_input_held_ = false;
    deuteros_atari_.reset();
    session_snapshot_.reset();
    active_.reset();
    admission_ = ReleaseRuntimeAdmission::unselected;
    rejection_ = ReleaseRuntimeRejection::none;
}

MillenniumDosPostOverlayObservationResult ReleaseRuntimeCoordinator::complete_millennium_dos_handler(
    const MillenniumDosHandlerCompletionObservation observation) {
    MillenniumDosPostOverlayObservationResult result;
    if (!active_ || !session_snapshot_ || !millennium_dos_post_overlay_loop_
        || observation.function_key_index >= 10 || observation.dispatch_call_address != 0xd40a
        || observation.return_address != 0xd40d) {
        result.error = "Handler completion requires an active owned dispatch and exact return"; return result;
    }
    bool terminal=false;
    switch(observation.function_key_index) {
    case 0: terminal=millennium_dos_first_function_ && session_snapshot_->kind==RuntimeSessionKind::millennium_dos_first_function && millennium_dos_first_function_->boundary().kind==MillenniumDosFirstFunctionBoundaryKind::local_return && millennium_dos_first_function_->boundary().instruction_address==observation.terminal_instruction_address; break;
    case 1: terminal=millennium_dos_second_function_ && session_snapshot_->kind==RuntimeSessionKind::millennium_dos_second_function && millennium_dos_second_function_->boundary().kind==MillenniumDosSecondFunctionBoundaryKind::local_return && millennium_dos_second_function_->boundary().instruction_address==observation.terminal_instruction_address; break;
    case 2: terminal=millennium_dos_third_function_ && session_snapshot_->kind==RuntimeSessionKind::millennium_dos_third_function && millennium_dos_third_function_->boundary().kind==MillenniumDosThirdFunctionBoundaryKind::local_return && millennium_dos_third_function_->boundary().instruction_address==observation.terminal_instruction_address; break;
    case 3: terminal=millennium_dos_fourth_function_ && session_snapshot_->kind==RuntimeSessionKind::millennium_dos_fourth_function && millennium_dos_fourth_function_->boundary().kind==MillenniumDosFourthFunctionBoundaryKind::local_return && millennium_dos_fourth_function_->boundary().instruction_address==observation.terminal_instruction_address; break;
    case 4: terminal=millennium_dos_fifth_function_ && session_snapshot_->kind==RuntimeSessionKind::millennium_dos_fifth_function && millennium_dos_fifth_function_->boundary().kind==MillenniumDosFifthFunctionBoundaryKind::local_return && millennium_dos_fifth_function_->boundary().instruction_address==observation.terminal_instruction_address; break;
    case 5: terminal=millennium_dos_sixth_function_ && session_snapshot_->kind==RuntimeSessionKind::millennium_dos_sixth_function && millennium_dos_sixth_function_->boundary().kind==MillenniumDosSixthFunctionBoundaryKind::local_return && millennium_dos_sixth_function_->boundary().instruction_address==observation.terminal_instruction_address; break;
    case 6: terminal=millennium_dos_seventh_function_ && session_snapshot_->kind==RuntimeSessionKind::millennium_dos_seventh_function && millennium_dos_seventh_function_->boundary().kind==MillenniumDosSeventhFunctionBoundaryKind::local_return && millennium_dos_seventh_function_->boundary().instruction_address==observation.terminal_instruction_address; break;
    case 7: terminal=millennium_dos_eighth_function_ && session_snapshot_->kind==RuntimeSessionKind::millennium_dos_eighth_function && millennium_dos_eighth_function_->boundary().kind==MillenniumDosEighthFunctionBoundaryKind::local_return && millennium_dos_eighth_function_->boundary().instruction_address==observation.terminal_instruction_address; break;
    case 8: terminal=millennium_dos_ninth_function_ && session_snapshot_->kind==RuntimeSessionKind::millennium_dos_ninth_function && millennium_dos_ninth_function_->boundary().kind==MillenniumDosNinthFunctionBoundaryKind::local_return && millennium_dos_ninth_function_->boundary().instruction_address==observation.terminal_instruction_address; break;
    case 9: terminal=millennium_dos_tenth_function_ && session_snapshot_->kind==RuntimeSessionKind::millennium_dos_tenth_function && millennium_dos_tenth_function_->boundary().kind==MillenniumDosTenthFunctionBoundaryKind::local_return && millennium_dos_tenth_function_->boundary().instruction_address==observation.terminal_instruction_address; break;
    }
    if(!terminal){result.error="Handler completion is not at a proven local RET boundary";return result;}
    try { millennium_dos_post_overlay_loop_->observe_dispatch_return(observation.dispatch_call_address,observation.return_address); }
    catch(const std::exception&e){result.error=e.what();return result;}
    constexpr std::array<std::uint16_t,10> handlers{0x6f9a,0x71ca,0x6faa,0x72f9,0x7597,0x7415,0x7521,0x7306,0x7339,0x7384};
    millennium_dos_handler_completion_=MillenniumDosHandlerCompletionCheckpoint{observation.function_key_index,handlers[observation.function_key_index],observation.terminal_instruction_address,observation.return_address};
    millennium_dos_first_function_.reset(); millennium_dos_second_function_.reset(); millennium_dos_third_function_.reset(); millennium_dos_fourth_function_.reset(); millennium_dos_fifth_function_.reset(); millennium_dos_sixth_function_.reset(); millennium_dos_seventh_function_.reset(); millennium_dos_eighth_function_.reset(); millennium_dos_ninth_function_.reset(); millennium_dos_tenth_function_.reset();
    session_snapshot_=make_runtime_session_snapshot(*active_,RuntimeSessionKind::millennium_dos_post_overlay_loop); result.accepted=true; return result;
}

std::optional<MillenniumDosHandlerCompletionCheckpoint> ReleaseRuntimeCoordinator::millennium_dos_handler_completion_checkpoint() const { if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_post_overlay_loop)return std::nullopt;return millennium_dos_handler_completion_; }

std::optional<MillenniumDosPresentationSnapshot>
ReleaseRuntimeCoordinator::millennium_dos_presentation() const {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title
        || !millennium_dos_) return std::nullopt;
    return MillenniumDosPresentationSnapshot{*millennium_dos_};
}

std::optional<MillenniumDosStartupInputSnapshot>
ReleaseRuntimeCoordinator::millennium_dos_startup_input() const {
    if (!session_snapshot_ || (session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title
            && session_snapshot_->kind != RuntimeSessionKind::millennium_dos_sound_driver_boundary)
        || (!millennium_dos_sound_selection_ && !millennium_dos_title_)) return std::nullopt;
    MillenniumDosStartupInputSnapshot snapshot;
    if (millennium_dos_sound_selection_) {
        snapshot.sound_selection_active = true;
        snapshot.sound_selection_awaiting_choice = millennium_dos_sound_selection_->awaiting_choice();
        if (!snapshot.sound_selection_awaiting_choice) {
            snapshot.selected_original_filename =
                millennium_dos_sound_selection_->selected_original_filename();
            snapshot.selected_driver_is_admitted =
                millennium_dos_sound_selection_->selected_driver_is_admitted();
        }
    }
    if (millennium_dos_title_) {
        snapshot.title_active = true;
        snapshot.title_handed_off = millennium_dos_title_->handed_off();
    }
    return snapshot;
}

MillenniumDosSoundDriverLoadObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_sound_driver_load(
    const MillenniumDosSoundDriverLoadObservation observation) {
    MillenniumDosSoundDriverLoadObservationResult rejected;
    if (!active_ || !session_snapshot_ || !millennium_dos_ || !native_runtime_memory_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_sound_driver_boundary
        || !millennium_dos_sound_selection_) {
        rejected.error = "Sound-driver loading requires the active selected-driver boundary";
        return rejected;
    }
    const auto sequence = std::visit([](const auto& value) { return value.sequence; }, observation);
    if (sequence == 0 || sequence <= millennium_dos_sound_driver_load_last_sequence_) {
        rejected.error = "Sound-driver observation sequence is stale or duplicated";
        return rejected;
    }
    if (const auto* entry = std::get_if<MillenniumDosSoundDriverLoadEntryObservation>(&observation)) {
        if (sequence != 1 || millennium_dos_sound_driver_load_
            || !millennium_dos_sound_selection_->selected_driver_is_admitted()) {
            rejected.error = "Sound-driver entry was already admitted or has no exact selected leaf";
            return rejected;
        }
        try {
            const auto selected = millennium_dos_sound_selection_->selected_driver();
            if (!selected) throw std::runtime_error("Selected driver identity is unavailable");
            const auto media = VerifiedReleaseMedia::open(active_->release);
            const auto launcher = admit_native_code_image(media,
                "millennium-dos-mill-com-linear", "millennium-dos-launcher");
            const auto driver = media.borrow(selected->sha256);
            if (!launcher.accepted() || !driver) throw std::runtime_error("Selected driver bytes are unavailable");
            const char selected_character = selected->kind
                    == MillenniumDosSoundDriverKind::sound_blaster ? '1' : '2';
            MillenniumDosSoundDriverLoadSession next(launcher.view->bytes, *driver,
                selected_character, entry->code_segment);
            NativeRuntimeEffectBatch batch{
                "millennium-dos-sound-driver-" + std::to_string(millennium_dos_sound_driver_load_generation_ + 1) + "-selection",
                true, {}};
            for (std::size_t i=0; i<next.runtime_byte_effects().size(); ++i) {
                const auto& effect=next.runtime_byte_effects()[i];
                batch.effects.push_back({i+1,{NativeRuntimeAddressSpace::linear,std::nullopt,effect.address},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::little_endian,effect.value});
            }
            auto memory=*native_runtime_memory_;
            const auto applied=memory.apply(batch);
            if(!applied.accepted) throw std::runtime_error(applied.error);
            millennium_dos_sound_driver_load_=std::move(next);
            *native_runtime_memory_=std::move(memory);
            ++millennium_dos_sound_driver_load_generation_;
            millennium_dos_sound_driver_load_last_sequence_=sequence;
            millennium_dos_compatibility_runner_.emplace(
                millennium_dos_sound_driver_load_generation_, sequence, entry->code_segment);
            return {true,{}};
        } catch(const std::exception& e) { rejected.error=e.what(); return rejected; }
    }
    if (!millennium_dos_sound_driver_load_ || !millennium_dos_compatibility_runner_) {
        rejected.error="Sound-driver entry observation is required first"; return rejected;
    }
    if (!millennium_dos_compatibility_runner_->accepts(sequence)) {
        rejected.error="Compatibility runner requires the exact next observation sequence";
        return rejected;
    }
    if (std::holds_alternative<MillenniumDosSoundDriverOpenObservation>(observation)
        || std::holds_alternative<MillenniumDosSoundDriverSeekObservation>(observation)
        || std::holds_alternative<MillenniumDosSoundDriverAllocationObservation>(observation)
        || std::holds_alternative<MillenniumDosSoundDriverReadObservation>(observation)
        || std::holds_alternative<MillenniumDosSoundDriverCloseObservation>(observation)) {
        rejected.error="Native compatibility service owns deterministic driver file and arena operations";
        return rejected;
    }
    auto next=*millennium_dos_sound_driver_load_;
    auto memory=*native_runtime_memory_;
    auto runner=*millennium_dos_compatibility_runner_;
    const auto before_memory=next.memory_effects().size();
    const auto before_words=next.runtime_word_effects().size();
    try {
        std::visit([&](const auto& value) {
            using T=std::decay_t<decltype(value)>;
            if constexpr(std::is_same_v<T,MillenniumDosSoundDriverOpenObservation>) next.observe_open_result(value.instruction,value.carry,value.ax);
            else if constexpr(std::is_same_v<T,MillenniumDosSoundDriverSeekObservation>) {
                if(next.state()==MillenniumDosSoundDriverLoadState::awaiting_seek_end_result) next.observe_seek_end_result(value.instruction,value.carry,value.bx,value.ax,value.dx);
                else next.observe_seek_start_result(value.instruction,value.carry,value.bx,value.ax,value.dx);
            } else if constexpr(std::is_same_v<T,MillenniumDosSoundDriverAllocationObservation>) next.observe_allocation_result(value.instruction,value.carry,value.ax);
            else if constexpr(std::is_same_v<T,MillenniumDosSoundDriverReadObservation>) next.observe_read_result(value.instruction,value.carry,value.bx,value.ax);
            else if constexpr(std::is_same_v<T,MillenniumDosSoundDriverCloseObservation>) next.observe_close_result(value.instruction,value.carry,value.bx);
            else if constexpr(std::is_same_v<T,MillenniumDosSoundDriverVectorObservation>) next.observe_vector_install(value.instruction,value.ax,value.dx);
            else if constexpr(std::is_same_v<T,MillenniumDosSoundDriverStackObservation>) next.observe_parent_stack(value.instruction,value.address,value.value);
            else if constexpr(std::is_same_v<T,MillenniumDosSoundDriverTitleExecObservation>) next.observe_title_exec_request(value.instruction,value.ax,value.dx,value.parameter_block);
            else throw std::runtime_error("Sound-driver entry observation is valid only before admission");
        },observation);
        if(next.memory_effects().size()!=before_memory){
            NativeRuntimeEffectBatch batch{"millennium-dos-sound-driver-"+std::to_string(millennium_dos_sound_driver_load_generation_)+"-image",true,{}};
            batch.effects.reserve(next.memory_effects().size());
            for(std::size_t i=0;i<next.memory_effects().size();++i){const auto&e=next.memory_effects()[i];batch.effects.push_back({i+1,{NativeRuntimeAddressSpace::dos_segmented,e.segment,e.offset},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::little_endian,e.value});}
            const auto applied=memory.apply(batch);if(!applied.accepted)throw std::runtime_error(applied.error);
        }
        if(next.runtime_word_effects().size()!=before_words){
            NativeRuntimeEffectBatch batch{"millennium-dos-sound-driver-"+std::to_string(millennium_dos_sound_driver_load_generation_)+"-parent",true,{}};
            for(std::size_t i=0;i<next.runtime_word_effects().size();++i){const auto&e=next.runtime_word_effects()[i];batch.effects.push_back({i+1,{NativeRuntimeAddressSpace::linear,std::nullopt,e.address},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::little_endian,e.value});}
            const auto applied=memory.apply(batch);if(!applied.accepted)throw std::runtime_error(applied.error);
        }
        if(next.state()==MillenniumDosSoundDriverLoadState::title_exec_requested){
            if(!millennium_dos_->title_flow)throw std::runtime_error("English title flow is unavailable");
            constexpr std::string_view mill_sha =
                "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e";
            constexpr std::string_view titles_sha =
                "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
            const auto media=VerifiedReleaseMedia::open(active_->release);
            const auto mill=media.borrow(mill_sha);
            const auto titles=media.borrow(titles_sha);
            if(!mill||!titles)throw std::runtime_error("Exact TITLES.EXE process media is unavailable");
            millennium_dos_title_exec_entry_.emplace(*mill,*titles);
            millennium_dos_title_=std::make_unique<MillenniumDosTitleSession>(*millennium_dos_->title_flow);
        }
    } catch(const std::exception&e){rejected.error=e.what();return rejected;}
    runner.commit(sequence);
    millennium_dos_sound_driver_load_=std::move(next);*native_runtime_memory_=std::move(memory);
    millennium_dos_compatibility_runner_=std::move(runner);
    millennium_dos_sound_driver_load_last_sequence_=sequence;return {true,{}};
}

std::optional<MillenniumDosSoundDriverLoadCheckpoint>
ReleaseRuntimeCoordinator::millennium_dos_sound_driver_load_checkpoint()const{
    if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_sound_driver_boundary||!millennium_dos_sound_driver_load_)return std::nullopt;
    const auto&s=*millennium_dos_sound_driver_load_;
    return MillenniumDosSoundDriverLoadCheckpoint{millennium_dos_sound_driver_load_generation_,millennium_dos_sound_driver_load_last_sequence_,s.state(),s.boundary(),s.driver().kind,s.memory_effects().size(),s.file_handle(),s.load_segment(),s.runtime_word_effects(),s.runtime_byte_effects()};
}

std::optional<MillenniumDosCompatibilityRunnerCheckpoint>
ReleaseRuntimeCoordinator::tick_millennium_dos_compatibility_runner(){
    if(!active_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_sound_driver_boundary
        ||!millennium_dos_sound_driver_load_||!millennium_dos_compatibility_runner_
        ||!native_runtime_memory_)return std::nullopt;
    auto next=*millennium_dos_sound_driver_load_;
    auto runner=*millennium_dos_compatibility_runner_;
    auto memory=*native_runtime_memory_;
    try {
        // These results are properties of the admitted immutable leaf and of
        // Eon's private one-leaf compatibility service. No guest DOS state is
        // inferred. Run until the next operation needs external process state.
        while (true) {
            switch (next.state()) {
            case MillenniumDosSoundDriverLoadState::awaiting_open_result:
                next.observe_open_result(0x02d2,false,runner.compatibility_file_handle());
                runner.record_automatic_operation();
                continue;
            case MillenniumDosSoundDriverLoadState::awaiting_seek_end_result:
                next.observe_seek_end_result(0x02eb,false,
                    runner.compatibility_file_handle(),
                    static_cast<std::uint16_t>(next.driver().byte_size),0);
                runner.record_automatic_operation();
                continue;
            case MillenniumDosSoundDriverLoadState::awaiting_allocation_result: {
                const auto paragraphs=static_cast<std::uint32_t>(
                    (next.driver().byte_size+15U)/16U);
                const auto allocated=runner.allocate_paragraphs(paragraphs);
                if(!allocated.allocation)throw std::runtime_error(allocated.error);
                next.observe_allocation_result(0x02fa,false,
                    allocated.allocation->segment);
                continue;
            }
            case MillenniumDosSoundDriverLoadState::awaiting_seek_start_result:
                next.observe_seek_start_result(0x0309,false,
                    runner.compatibility_file_handle(),0,0);
                runner.record_automatic_operation();
                continue;
            case MillenniumDosSoundDriverLoadState::awaiting_read_result: {
                next.observe_read_result(0x0313,false,
                    runner.compatibility_file_handle(),
                    static_cast<std::uint16_t>(next.driver().byte_size));
                NativeRuntimeEffectBatch batch{
                    "millennium-dos-sound-driver-"
                        + std::to_string(millennium_dos_sound_driver_load_generation_)
                        + "-image",true,{}};
                batch.effects.reserve(next.memory_effects().size());
                for(std::size_t i=0;i<next.memory_effects().size();++i){
                    const auto&e=next.memory_effects()[i];
                    batch.effects.push_back({i+1,
                        {NativeRuntimeAddressSpace::dos_segmented,e.segment,e.offset},
                        MemoryTransferElementWidth::byte,
                        NativeRuntimeByteOrder::little_endian,e.value});
                }
                const auto applied=memory.apply(batch);
                if(!applied.accepted)throw std::runtime_error(applied.error);
                runner.record_automatic_operation();
                continue;
            }
            case MillenniumDosSoundDriverLoadState::awaiting_close_result:
                next.observe_close_result(0x0319,false,
                    runner.compatibility_file_handle());
                runner.record_automatic_operation();
                continue;
            default:
                if(next.state()==MillenniumDosSoundDriverLoadState::title_exec_requested
                    &&millennium_dos_title_exec_entry_
                    &&millennium_dos_title_exec_entry_->state()
                        ==MillenniumDosTitleExecEntryState::awaiting_child_process_entry) {
                    constexpr std::string_view titles_sha =
                        "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
                    const auto media=VerifiedReleaseMedia::open(active_->release);
                    const auto titles=media.borrow(titles_sha);
                    if(!titles)throw std::runtime_error(
                        "Exact TITLES.EXE compatibility child leaf is unavailable");
                    const auto allocated=runner.allocate_paragraphs(
                        MillenniumDosTitleChildCompatibilityService::required_paragraphs());
                    if(!allocated.allocation)throw std::runtime_error(allocated.error);
                    MillenniumDosTitleChildCompatibilityService child(
                        *titles,*allocated.allocation);
                    NativeRuntimeEffectBatch batch{
                        "millennium-dos-title-child-"
                            +std::to_string(millennium_dos_sound_driver_load_generation_)
                            +"-image",true,{}};
                    batch.effects.reserve(child.image_effects().size());
                    for(const auto& effect:child.image_effects()) {
                        batch.effects.push_back({batch.effects.size()+1,
                            {NativeRuntimeAddressSpace::dos_segmented,
                                allocated.allocation->segment,effect.offset},
                            MemoryTransferElementWidth::byte,
                            NativeRuntimeByteOrder::little_endian,effect.value});
                    }
                    const auto applied=memory.apply(batch);
                    if(!applied.accepted)throw std::runtime_error(applied.error);
                    auto entry=*millennium_dos_title_exec_entry_;
                    const auto child_entry_sequence=runner.next_sequence();
                    entry.observe_child_process_entry({child_entry_sequence,
                        0x0336,0x4b00,0x068f,0x067a,0x0100,
                        allocated.allocation->segment,
                        MillenniumDosTitleExecEntryProvenance::
                            eon_dos_compatibility_service});
                    runner.record_automatic_operation();
                    const auto prefix_sequence=runner.next_sequence();
                    entry.execute_exact_entry_prefix(prefix_sequence,
                        0x0100,0x0104,0x1b80);
                    runner.record_automatic_operation();
                    MillenniumDosTitleInitializationSession initialization(
                        *titles,allocated.allocation->segment,prefix_sequence);
                    const auto initialization_sequence=runner.next_sequence();
                    initialization.execute_exact_startup(initialization_sequence,
                        0x1b80,0x1b95,0x0122,0x91);
                    runner.record_automatic_operation();
                    millennium_dos_title_exec_entry_=std::move(entry);
                    millennium_dos_title_child_compatibility_.emplace(std::move(child));
                    millennium_dos_title_initialization_.emplace(
                        std::move(initialization));
                    session_snapshot_=make_runtime_session_snapshot(
                        *active_,RuntimeSessionKind::millennium_dos_title);
                }
                millennium_dos_sound_driver_load_=std::move(next);
                millennium_dos_compatibility_runner_=std::move(runner);
                *native_runtime_memory_=std::move(memory);
                auto checkpoint=millennium_dos_compatibility_runner_->checkpoint(
                    millennium_dos_sound_driver_load_->state(),
                    millennium_dos_sound_driver_load_->boundary());
                if(millennium_dos_title_initialization_) {
                    checkpoint.external_result_required=true;
                }
                millennium_dos_sound_driver_load_last_sequence_=
                    checkpoint.last_sequence;
                return checkpoint;
            }
        }
    } catch(const std::exception& e) {
        return millennium_dos_compatibility_runner_->checkpoint(
            millennium_dos_sound_driver_load_->state(),
            millennium_dos_sound_driver_load_->boundary(),e.what());
    }
}

MillenniumDosTitleExecEntryObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_title_child_process_entry(
    const MillenniumDosTitleExecProcessEntry observation) {
    MillenniumDosTitleExecEntryObservationResult result;
    if(!active_||!session_snapshot_
        ||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_sound_driver_boundary
        ||!millennium_dos_sound_driver_load_||!millennium_dos_title_exec_entry_
        ||millennium_dos_sound_driver_load_->state()
            !=MillenniumDosSoundDriverLoadState::title_exec_requested){
        result.error="TITLES.EXE child entry requires the exact preceding EXEC request";
        return result;
    }
    auto next=*millennium_dos_title_exec_entry_;
    try { next.observe_child_process_entry(observation); }
    catch(const std::exception& e){result.error=e.what();return result;}
    millennium_dos_title_exec_entry_=std::move(next);
    result.accepted=true;
    return result;
}

MillenniumDosTitleExecEntryObservationResult
ReleaseRuntimeCoordinator::advance_millennium_dos_title_entry_prefix(
    const MillenniumDosTitleExecPrefixObservation observation) {
    MillenniumDosTitleExecEntryObservationResult result;
    if(!active_||!session_snapshot_
        ||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_sound_driver_boundary
        ||!millennium_dos_title_exec_entry_
        ||millennium_dos_title_exec_entry_->state()
            !=MillenniumDosTitleExecEntryState::entry_prefix_boundary){
        result.error="TITLES.EXE local prefix requires an explicit child-process entry";
        return result;
    }
    auto next=*millennium_dos_title_exec_entry_;
    try { next.execute_exact_entry_prefix(observation.sequence,
        observation.prefix_address,observation.jump_address,
        observation.jump_destination); }
    catch(const std::exception& e){result.error=e.what();return result;}
    millennium_dos_title_exec_entry_=std::move(next);
    session_snapshot_=make_runtime_session_snapshot(*active_,RuntimeSessionKind::millennium_dos_title);
    result.accepted=true;
    return result;
}

std::optional<MillenniumDosTitleExecEntryRuntimeCheckpoint>
ReleaseRuntimeCoordinator::millennium_dos_title_exec_entry_checkpoint() const {
    if(!session_snapshot_||!millennium_dos_title_exec_entry_
        ||(session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_sound_driver_boundary
            &&session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_title))return std::nullopt;
    return MillenniumDosTitleExecEntryRuntimeCheckpoint{
        millennium_dos_sound_driver_load_generation_,
        millennium_dos_title_exec_entry_->checkpoint(),
        millennium_dos_title_child_compatibility_
            ?std::optional{millennium_dos_title_child_compatibility_->checkpoint()}
            :std::nullopt,
        millennium_dos_title_initialization_
            ?std::optional{millennium_dos_title_initialization_->checkpoint()}
            :std::nullopt};
}

MillenniumDosTitleInitializationObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_title_private_interrupt_result(
    const MillenniumDosTitlePrivateInterruptResultObservation observation) {
    MillenniumDosTitleInitializationObservationResult result;
    if(!session_snapshot_
        ||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_title
        ||!millennium_dos_title_initialization_||!native_runtime_memory_){
        result.error="Title private-interrupt result requires the active native title boundary";
        return result;
    }
    auto next=*millennium_dos_title_initialization_;
    auto memory=*native_runtime_memory_;
    const auto initial_state=next.checkpoint().state;
    const auto prior_effect_count=next.checkpoint().memory_effects.size();
    try {
        next.observe_private_interrupt_result(observation);
        if(initial_state==MillenniumDosTitleInitializationState::private_interrupt_result_boundary){
            const auto selected=next.checkpoint();
            next.execute_selected_callee_start(selected.last_sequence+1,
                selected.selected_call_address,selected.selected_call_target);
        }
        if(next.checkpoint().state==MillenniumDosTitleInitializationState::post_video_followup_call_boundary){
            const auto reached=next.checkpoint();
            next.execute_post_video_followup(reached.last_sequence+1,0x1c17,0x1725);
        }
    }
    catch(const std::exception& e){result.error=e.what();return result;}
    const auto checkpoint=next.checkpoint();
    NativeRuntimeEffectBatch batch{
        "millennium-dos-title-initialization-"
            +std::to_string(millennium_dos_sound_driver_load_generation_)
            +"-"+std::to_string(observation.sequence),true,{}};
    batch.effects.reserve(checkpoint.memory_effects.size()-prior_effect_count);
    for(std::size_t i=prior_effect_count;i<checkpoint.memory_effects.size();++i){
        const auto& effect=checkpoint.memory_effects[i];
        batch.effects.push_back({batch.effects.size()+1,
            {NativeRuntimeAddressSpace::dos_segmented,
                checkpoint.child_code_segment,effect.offset},
            effect.width==MillenniumDosTitleInitializationEffectWidth::byte
                ?MemoryTransferElementWidth::byte:MemoryTransferElementWidth::word,
            NativeRuntimeByteOrder::little_endian,effect.value});
    }
    if(!batch.effects.empty()){
        const auto applied=memory.apply(batch);
        if(!applied.accepted){result.error=applied.error;return result;}
    }
    millennium_dos_title_initialization_=std::move(next);
    *native_runtime_memory_=std::move(memory);
    result.accepted=true;
    return result;
}

MillenniumDosTitleInitializationObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_title_selected_callee_result(
    const MillenniumDosTitleSelectedCalleeResultObservation observation) {
    MillenniumDosTitleInitializationObservationResult result;
    if(!session_snapshot_
        ||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_title
        ||!millennium_dos_title_initialization_||!native_runtime_memory_){
        result.error="Selected title-callee result requires the active native title boundary";
        return result;
    }
    auto next=*millennium_dos_title_initialization_;
    auto memory=*native_runtime_memory_;
    const auto prior_effect_count=next.checkpoint().memory_effects.size();
    try {
        next.observe_selected_callee_private_interrupt_result(observation);
        const auto selected=next.checkpoint();
        next.execute_selected_followup_start(selected.last_sequence+1,
            selected.selected_followup_call_address,
            selected.selected_followup_call_target);
    } catch(const std::exception& e){result.error=e.what();return result;}
    const auto checkpoint=next.checkpoint();
    if(checkpoint.memory_effects.size()>prior_effect_count){
        NativeRuntimeEffectBatch batch{
            "millennium-dos-title-followup-"
                +std::to_string(millennium_dos_sound_driver_load_generation_)
                +"-"+std::to_string(observation.sequence),true,{}};
        for(std::size_t i=prior_effect_count;i<checkpoint.memory_effects.size();++i){
            const auto& effect=checkpoint.memory_effects[i];
            batch.effects.push_back({batch.effects.size()+1,
                {NativeRuntimeAddressSpace::dos_segmented,
                    checkpoint.child_code_segment,effect.offset},
                effect.width==MillenniumDosTitleInitializationEffectWidth::byte
                    ?MemoryTransferElementWidth::byte:MemoryTransferElementWidth::word,
                NativeRuntimeByteOrder::little_endian,effect.value});
        }
        const auto applied=memory.apply(batch);
        if(!applied.accepted){result.error=applied.error;return result;}
    }
    millennium_dos_title_initialization_=std::move(next);
    *native_runtime_memory_=std::move(memory);
    result.accepted=true;
    return result;
}

MillenniumDosTitleInitializationObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_title_bios_result(
    const MillenniumDosTitleBiosResultObservation observation) {
    MillenniumDosTitleInitializationObservationResult result;
    if(!active_||!session_snapshot_
        ||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_title
        ||!millennium_dos_title_initialization_||!native_runtime_memory_){
        result.error="Title BIOS result requires the active native title boundary";
        return result;
    }
    constexpr std::string_view titles_sha=
        "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
    try {
        const auto media=VerifiedReleaseMedia::open(active_->release);
        const auto titles=media.borrow(titles_sha);
        if(!titles)throw std::runtime_error("Exact TITLES.EXE BIOS source is unavailable");
        auto next=*millennium_dos_title_initialization_;
        auto memory=*native_runtime_memory_;
        const auto prior_effect_count=next.checkpoint().memory_effects.size();
        next.observe_bios_palette_result(observation,*titles);
        if(next.checkpoint().state
            ==MillenniumDosTitleInitializationState::title_main_allocation_call_boundary){
            const auto reached=next.checkpoint();
            next.execute_title_main_allocation_start(reached.last_sequence+1,
                reached.title_main_call_address,reached.title_main_call_target);
        }
        if(next.checkpoint().state
            ==MillenniumDosTitleInitializationState::post_video_setup_call_boundary){
            const auto reached=next.checkpoint();
            next.execute_post_video_setup(reached.last_sequence+1,0x1c0e,0x135e);
        }
        if(next.checkpoint().state
            ==MillenniumDosTitleInitializationState::post_video_graphics_call_boundary){
            const auto reached=next.checkpoint();
            next.execute_post_video_graphics_call(reached.last_sequence+1,0x1c11,0x0ff3);
        }
        const auto checkpoint=next.checkpoint();
        if(checkpoint.memory_effects.size()>prior_effect_count){
            NativeRuntimeEffectBatch batch{
                "millennium-dos-title-bios-"
                    +std::to_string(millennium_dos_sound_driver_load_generation_)
                    +"-"+std::to_string(observation.sequence),true,{}};
            for(std::size_t i=prior_effect_count;i<checkpoint.memory_effects.size();++i){
                const auto& effect=checkpoint.memory_effects[i];
                batch.effects.push_back({batch.effects.size()+1,
                    {NativeRuntimeAddressSpace::dos_segmented,
                        checkpoint.child_code_segment,effect.offset},
                    effect.width==MillenniumDosTitleInitializationEffectWidth::byte
                        ?MemoryTransferElementWidth::byte:MemoryTransferElementWidth::word,
                    NativeRuntimeByteOrder::little_endian,effect.value});
            }
            const auto applied=memory.apply(batch);
            if(!applied.accepted){result.error=applied.error;return result;}
        }
        millennium_dos_title_initialization_=std::move(next);
        *native_runtime_memory_=std::move(memory);
        result.accepted=true;
    } catch(const std::exception& e){result.error=e.what();}
    return result;
}

MillenniumDosTitleInitializationObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_title_dos_memory_result(
    const MillenniumDosTitleDosResultObservation observation){
    MillenniumDosTitleInitializationObservationResult result;
    if(!session_snapshot_
        ||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_title
        ||!millennium_dos_title_initialization_||!native_runtime_memory_){
        result.error="Title DOS-memory result requires the active native title boundary";
        return result;
    }
    auto next=*millennium_dos_title_initialization_;
    auto memory=*native_runtime_memory_;
    const auto prior_effect_count=next.checkpoint().memory_effects.size();
    try {next.observe_dos_memory_result(observation);}
    catch(const std::exception& e){result.error=e.what();return result;}
    const auto checkpoint=next.checkpoint();
    if(checkpoint.memory_effects.size()>prior_effect_count){
        NativeRuntimeEffectBatch batch{
            "millennium-dos-title-memory-service-"
                +std::to_string(millennium_dos_sound_driver_load_generation_)
                +"-"+std::to_string(observation.sequence),true,{}};
        for(std::size_t i=prior_effect_count;i<checkpoint.memory_effects.size();++i){
            const auto& effect=checkpoint.memory_effects[i];
            batch.effects.push_back({batch.effects.size()+1,
                {NativeRuntimeAddressSpace::dos_segmented,
                    checkpoint.child_code_segment,effect.offset},
                effect.width==MillenniumDosTitleInitializationEffectWidth::byte
                    ?MemoryTransferElementWidth::byte:MemoryTransferElementWidth::word,
                NativeRuntimeByteOrder::little_endian,effect.value});
        }
        const auto applied=memory.apply(batch);
        if(!applied.accepted){result.error=applied.error;return result;}
    }
    millennium_dos_title_initialization_=std::move(next);
    *native_runtime_memory_=std::move(memory);
    result.accepted=true;
    return result;
}

MillenniumDosTitleInitializationObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_title_dos_file_result(
    const MillenniumDosTitleDosFileResultObservation observation){
    MillenniumDosTitleInitializationObservationResult result;
    if(!active_||!session_snapshot_
        ||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_title
        ||!millennium_dos_title_initialization_||!native_runtime_memory_){
        result.error="Title DOS-file result requires the active native title boundary";
        return result;
    }
    auto next=*millennium_dos_title_initialization_;
    auto memory=*native_runtime_memory_;
    const auto prior_effect_count=next.checkpoint().memory_effects.size();
    try {
        constexpr std::string_view title_library_sha=
            "6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678";
        const auto media=VerifiedReleaseMedia::open(active_->release);
        const auto title_library=media.borrow(title_library_sha);
        if(!title_library)
            throw std::runtime_error("Exact TITLE.LIB file source is unavailable");
        next.observe_dos_file_result(observation,*title_library);
        const auto reached=next.checkpoint();
        if(reached.state==MillenniumDosTitleInitializationState::library_relocation_complete)
            next.execute_post_relocation(reached.last_sequence+1,*title_library);
        const auto post_relocation=next.checkpoint();
        if(post_relocation.state
            ==MillenniumDosTitleInitializationState::post_library_setup_call_boundary)
            next.execute_post_library_setup(post_relocation.last_sequence+1,0x1bef,0x1aac);
    }
    catch(const std::exception& e){result.error=e.what();return result;}
    const auto checkpoint=next.checkpoint();
    if(checkpoint.memory_effects.size()>prior_effect_count){
        NativeRuntimeEffectBatch batch{
            "millennium-dos-title-file-service-"
                +std::to_string(millennium_dos_sound_driver_load_generation_)
                +"-"+std::to_string(observation.sequence),true,{}};
        for(std::size_t i=prior_effect_count;i<checkpoint.memory_effects.size();++i){
            const auto& effect=checkpoint.memory_effects[i];
            batch.effects.push_back({batch.effects.size()+1,
                {NativeRuntimeAddressSpace::dos_segmented,
                    effect.segment==0?checkpoint.child_code_segment:effect.segment,
                    effect.offset},
                effect.width==MillenniumDosTitleInitializationEffectWidth::byte
                    ?MemoryTransferElementWidth::byte:MemoryTransferElementWidth::word,
                NativeRuntimeByteOrder::little_endian,effect.value});
        }
        const auto applied=memory.apply(batch);
        if(!applied.accepted){result.error=applied.error;return result;}
    }
    millennium_dos_title_initialization_=std::move(next);
    *native_runtime_memory_=std::move(memory);
    result.accepted=true;
    return result;
}

MillenniumDosTitleInitializationObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_title_dos_vector_result(const MillenniumDosTitleDosVectorResultObservation observation){
    MillenniumDosTitleInitializationObservationResult result;
    if(!active_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_title||!millennium_dos_title_initialization_||!native_runtime_memory_){result.error="Title DOS-vector result requires the active native title boundary";return result;}
    auto next=*millennium_dos_title_initialization_;auto memory=*native_runtime_memory_;
    const auto prior=next.checkpoint().memory_effects.size();
    try{next.observe_dos_vector_result(observation);}catch(const std::exception& e){result.error=e.what();return result;}
    const auto checkpoint=next.checkpoint();
    if(checkpoint.memory_effects.size()>prior){
        NativeRuntimeEffectBatch batch{"millennium-dos-title-vector-"+std::to_string(observation.sequence),true,{}};
        for(std::size_t i=prior;i<checkpoint.memory_effects.size();++i){const auto& effect=checkpoint.memory_effects[i];batch.effects.push_back({batch.effects.size()+1,{NativeRuntimeAddressSpace::dos_segmented,checkpoint.child_code_segment,effect.offset},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::little_endian,effect.value});}
        const auto applied=memory.apply(batch);if(!applied.accepted){result.error=applied.error;return result;}
    }
    millennium_dos_title_initialization_=std::move(next);*native_runtime_memory_=std::move(memory);result.accepted=true;return result;
}
MillenniumDosTitleInitializationObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_title_setup_bios_result(const MillenniumDosTitleSetupBiosResultObservation observation){
    MillenniumDosTitleInitializationObservationResult result;
    if(!active_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_title||!millennium_dos_title_initialization_||!native_runtime_memory_){result.error="Title setup BIOS result requires the active native title boundary";return result;}
    auto next=*millennium_dos_title_initialization_;
    auto memory=*native_runtime_memory_;const auto prior=next.checkpoint().memory_effects.size();
    try{next.observe_setup_bios_result(observation);const auto reached=next.checkpoint();if(reached.state==MillenniumDosTitleInitializationState::post_library_next_setup_call_boundary)next.execute_next_setup(reached.last_sequence+1,0x1bf2,0x11a7);const auto followup=next.checkpoint();if(followup.state==MillenniumDosTitleInitializationState::post_library_followup_call_boundary)next.execute_followup_setup(followup.last_sequence+1,0x1bf5,0x114e);}catch(const std::exception& e){result.error=e.what();return result;}
    const auto checkpoint=next.checkpoint();if(checkpoint.memory_effects.size()>prior){NativeRuntimeEffectBatch batch{"millennium-dos-title-next-setup-"+std::to_string(observation.sequence),true,{}};for(std::size_t i=prior;i<checkpoint.memory_effects.size();++i){const auto& effect=checkpoint.memory_effects[i];batch.effects.push_back({batch.effects.size()+1,{NativeRuntimeAddressSpace::dos_segmented,checkpoint.child_code_segment,effect.offset},effect.width==MillenniumDosTitleInitializationEffectWidth::byte?MemoryTransferElementWidth::byte:MemoryTransferElementWidth::word,NativeRuntimeByteOrder::little_endian,effect.value});}const auto applied=memory.apply(batch);if(!applied.accepted){result.error=applied.error;return result;}}
    millennium_dos_title_initialization_=std::move(next);*native_runtime_memory_=std::move(memory);result.accepted=true;return result;
}
MillenniumDosTitleInitializationObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_title_far_words(const MillenniumDosTitleFarWordsObservation observation){MillenniumDosTitleInitializationObservationResult result;if(!active_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_title||!millennium_dos_title_initialization_||!native_runtime_memory_){result.error="Title far words require the active native title boundary";return result;}auto next=*millennium_dos_title_initialization_;auto memory=*native_runtime_memory_;const auto prior=next.checkpoint().memory_effects.size();try{next.observe_far_words(observation);auto reached=next.checkpoint();if(reached.state==MillenniumDosTitleInitializationState::post_vector_hook_call_boundary){next.execute_video_hook_setup(reached.last_sequence+1,0x1bf8,0x12a0);reached=next.checkpoint();}if(reached.state==MillenniumDosTitleInitializationState::post_video_hook_mode_call_boundary)next.execute_post_video_mode_call(reached.last_sequence+1,0x1c02,0x1ada);}catch(const std::exception& e){result.error=e.what();return result;}const auto checkpoint=next.checkpoint();NativeRuntimeEffectBatch batch{"millennium-dos-title-vector-hook-"+std::to_string(observation.sequence),true,{}};for(std::size_t i=prior;i<checkpoint.memory_effects.size();++i){const auto& effect=checkpoint.memory_effects[i];batch.effects.push_back({batch.effects.size()+1,{NativeRuntimeAddressSpace::dos_segmented,effect.explicit_segment?effect.segment:checkpoint.child_code_segment,effect.offset},effect.width==MillenniumDosTitleInitializationEffectWidth::byte?MemoryTransferElementWidth::byte:MemoryTransferElementWidth::word,NativeRuntimeByteOrder::little_endian,effect.value});}const auto applied=memory.apply(batch);if(!applied.accepted){result.error=applied.error;return result;}millennium_dos_title_initialization_=std::move(next);*native_runtime_memory_=std::move(memory);result.accepted=true;return result;}

namespace {
MillenniumDosTitleToGameObservationResult title_to_game_rejected(std::string error) {
    return {false, std::move(error)};
}
}

MillenniumDosTitleToGameObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_title_to_game_call_return(
    const MillenniumDosTitleToGameCallReturnObservation observation) {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title_handoff_boundary
        || !millennium_dos_title_to_game_ || !native_runtime_memory_
        || observation.sequence <= millennium_dos_title_to_game_last_sequence_)
        return title_to_game_rejected("Title-to-game call return requires the active boundary and a later sequence");
    auto next = *millennium_dos_title_to_game_;
    auto next_memory = *native_runtime_memory_;
    const auto old_effect_count = next.effects().size();
    try { next.observe_call_return(observation.call_address, observation.return_address); }
    catch (const std::exception& e) { return title_to_game_rejected(e.what()); }
    if (next.effects().size() != old_effect_count) {
        if (next.effects().size() != old_effect_count + 1) {
            return title_to_game_rejected("Title-to-game call produced an unexpected effect count");
        }
        const auto& effect = next.effects().back();
        NativeRuntimeEffectBatch batch{
            "millennium-dos-title-to-game-"
                + std::to_string(millennium_dos_title_to_game_generation_) + "-title-flag",
            true,
            {{1,
                {NativeRuntimeAddressSpace::linear, std::nullopt, effect.address},
                MemoryTransferElementWidth::byte,
                NativeRuntimeByteOrder::little_endian,
                effect.value}}};
        const auto applied = next_memory.apply(batch);
        if (!applied.accepted) {
            return title_to_game_rejected("Runtime-memory application rejected: " + applied.error);
        }
    }
    millennium_dos_title_to_game_ = std::move(next);
    *native_runtime_memory_ = std::move(next_memory);
    millennium_dos_title_to_game_last_sequence_ = observation.sequence;
    return {true, {}};
}

MillenniumDosTitleToGameObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_title_to_game_stack_word(
    const MillenniumDosTitleToGameStackWordObservation observation) {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title_handoff_boundary
        || !millennium_dos_title_to_game_ || observation.sequence <= millennium_dos_title_to_game_last_sequence_)
        return title_to_game_rejected("Title-to-game stack word requires the active boundary and a later sequence");
    auto next = *millennium_dos_title_to_game_;
    try { next.observe_stack_word(observation.instruction_address, observation.address, observation.value); }
    catch (const std::exception& e) { return title_to_game_rejected(e.what()); }
    millennium_dos_title_to_game_ = std::move(next);
    millennium_dos_title_to_game_last_sequence_ = observation.sequence;
    return {true, {}};
}

MillenniumDosTitleToGameObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_title_to_game_title_termination(
    const MillenniumDosTitleToGameInterruptObservation observation) {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title_handoff_boundary
        || !millennium_dos_title_to_game_ || observation.sequence <= millennium_dos_title_to_game_last_sequence_)
        return title_to_game_rejected("Title termination requires the active title-to-game boundary and a later sequence");
    auto next = *millennium_dos_title_to_game_;
    try { next.observe_title_termination(observation.interrupt_address, observation.ax); }
    catch (const std::exception& e) { return title_to_game_rejected(e.what()); }
    millennium_dos_title_to_game_ = std::move(next); millennium_dos_title_to_game_last_sequence_ = observation.sequence;
    return {true, {}};
}

MillenniumDosTitleToGameObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_title_to_game_parent_exec_return(
    const MillenniumDosTitleToGameInterruptObservation observation) {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title_handoff_boundary
        || !millennium_dos_title_to_game_ || observation.sequence <= millennium_dos_title_to_game_last_sequence_)
        return title_to_game_rejected("Parent EXEC return requires the active title-to-game boundary and a later sequence");
    auto next = *millennium_dos_title_to_game_;
    try { next.observe_parent_exec_return(observation.interrupt_address, observation.carry); }
    catch (const std::exception& e) { return title_to_game_rejected(e.what()); }
    millennium_dos_title_to_game_ = std::move(next); millennium_dos_title_to_game_last_sequence_ = observation.sequence;
    return {true, {}};
}

MillenniumDosTitleToGameObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_title_to_game_child_status(
    const MillenniumDosTitleToGameInterruptObservation observation) {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title_handoff_boundary
        || !millennium_dos_title_to_game_ || observation.sequence <= millennium_dos_title_to_game_last_sequence_)
        return title_to_game_rejected("Child status requires the active title-to-game boundary and a later sequence");
    auto next = *millennium_dos_title_to_game_;
    try { next.observe_child_status(observation.interrupt_address, observation.al, observation.carry); }
    catch (const std::exception& e) { return title_to_game_rejected(e.what()); }
    millennium_dos_title_to_game_ = std::move(next); millennium_dos_title_to_game_last_sequence_ = observation.sequence;
    return {true, {}};
}

std::optional<MillenniumDosTitleToGameCheckpoint>
ReleaseRuntimeCoordinator::millennium_dos_title_to_game_checkpoint() const {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title_handoff_boundary
        || !millennium_dos_title_to_game_) return std::nullopt;
    return MillenniumDosTitleToGameCheckpoint{
        millennium_dos_title_to_game_generation_, millennium_dos_title_to_game_last_sequence_,
        millennium_dos_title_to_game_->state(), millennium_dos_title_to_game_->boundary(),
        millennium_dos_title_to_game_->effects(), millennium_dos_title_to_game_->restored_stack_pointer(),
        millennium_dos_title_to_game_->child_status_al()};
}

std::optional<MillenniumDosStaticDispatchDiagnostics>
ReleaseRuntimeCoordinator::millennium_dos_static_dispatch_diagnostics() const {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title
        || !millennium_dos_ || !millennium_dos_->game_flow) return std::nullopt;
    const auto& flow = *millennium_dos_->game_flow;
    // The parser admits precisely ten records. Keep the public DTO fixed-size
    // so diagnostic consumers cannot mistake an unbounded byte range for an
    // executable game-control mapping.
    if (flow.function_key_count != 10) return std::nullopt;
    constexpr std::string_view release_sha256 =
        "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123";
    constexpr std::array<std::string_view, 10> handler_ids{{
        "millennium-dos-en-f1-handler", "millennium-dos-en-f2-handler",
        "millennium-dos-en-f3-handler", "millennium-dos-en-f4-handler",
        "millennium-dos-en-f5-handler", "millennium-dos-en-f6-handler",
        "millennium-dos-en-f7-handler", "millennium-dos-en-f8-prefix",
        "millennium-dos-en-f9-handler", "millennium-dos-en-f10-handler",
    }};
    const std::array<std::uint16_t, 10> handler_addresses{{
        flow.first_function_key.handler_address, flow.second_function_key.handler_address,
        flow.third_function_key.handler_address, flow.fourth_function_key.handler_address,
        flow.fifth_function_key.handler_address, flow.sixth_function_key.handler_address,
        flow.seventh_function_key.handler_address, flow.eighth_function_key.handler_address,
        flow.ninth_function_key.handler_address, flow.tenth_function_key.handler_address,
    }};
    for (std::size_t index = 0; index < handler_ids.size(); ++index) {
        const auto mapped = function_map_runtime_address_for(release_sha256, handler_ids[index]);
        if (!mapped || *mapped != handler_addresses[index]) return std::nullopt;
    }
    std::array<MillenniumDosStaticDispatchEntry, 10> handlers;
    for (std::size_t index = 0; index < handlers.size(); ++index) {
        handlers[index] = {
            .function_id = std::string(handler_ids[index]),
            .action = static_cast<std::uint8_t>(flow.function_key_first_action + index),
            .handler_address = handler_addresses[index],
        };
    }
    return MillenniumDosStaticDispatchDiagnostics{
        .action_poll_address = flow.action_poll_address,
        .first_action = flow.function_key_first_action,
        .action_count = flow.function_key_count,
        .table_address = flow.function_key_table_address,
        .table_stride = flow.function_key_table_stride,
        .dispatch_address = flow.function_key_dispatch_address,
        .handler_addresses = handler_addresses,
        .handlers = std::move(handlers),
    };
}

std::optional<MillenniumDosNativeProcessCheckpoint>
ReleaseRuntimeCoordinator::millennium_dos_native_process_checkpoint() const {
    if (admission_ != ReleaseRuntimeAdmission::active || !active_ || !session_snapshot_
        || active_->release.game != Game::millennium
        || active_->release.platform != Platform::dos || active_->release.language != "en"
        || active_->release.sha256
            != "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123"
        || !millennium_dos_native_process_) return std::nullopt;
    const auto checkpoint = millennium_dos_native_process_->checkpoint();
    if (!checkpoint || !checkpoint->static_recovery_entry
        || checkpoint->recovery_entry != MillenniumDosNativeRecoveryEntry::startup
        || checkpoint->state
            != MillenniumDosNativeProcessState::startup_first_private_interrupt) {
        return std::nullopt;
    }
    return checkpoint;
}

MillenniumDosGxStartupTraceAdmission
ReleaseRuntimeCoordinator::admit_millennium_dos_gx_startup_reference_trace(
    const ReferenceTrace& trace) const {
    MillenniumDosGxStartupTraceAdmission rejected;
    constexpr std::string_view game_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr std::string_view gx_overlay_sha256 =
        "093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb";
    const auto* descriptor = reference_trace_adapter_descriptor(trace.adapter);
    if (!descriptor
        || descriptor->runtime_policy != ReferenceTraceRuntimePolicy::transient_call_free_gx_startup
        || trace.source_release.game != descriptor->game
        || trace.source_release.platform != descriptor->platform
        || trace.source_release.language != descriptor->language
        || trace.source_release.sha256 != descriptor->release_sha256) {
        rejected.error = "Reference trace does not name the exact Millennium DOS GX boundary";
        return rejected;
    }
    std::error_code filesystem_error;
    const auto event_size = std::filesystem::file_size(trace.events_path, filesystem_error);
    if (filesystem_error || event_size != trace.event_size
        || event_size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        rejected.error = "Reference trace events changed after validation";
        return rejected;
    }
    try {
        std::ifstream stream(trace.events_path, std::ios::binary);
        std::string events(static_cast<std::size_t>(event_size), '\0');
        stream.read(events.data(), static_cast<std::streamsize>(events.size()));
        if (!stream || static_cast<std::size_t>(stream.gcount()) != events.size()) {
            rejected.error = "Reference trace events changed after validation";
            return rejected;
        }
        const std::vector<std::uint8_t> event_bytes(events.begin(), events.end());
        if (to_hex(sha256(event_bytes)) != trace.event_sha256) {
            rejected.error = "Reference trace events changed after validation";
            return rejected;
        }
        const auto media = VerifiedReleaseMedia::open(trace.source_release);
        const auto game = media.extract(game_sha256);
        const auto overlay = media.extract(gx_overlay_sha256);
        if (!game || !overlay) {
            rejected.error = "Verified GX startup leaves are unavailable";
            return rejected;
        }
        return admit_millennium_dos_gx_startup_trace(*game, *overlay, events);
    } catch (...) {
        rejected.error = "Unable to admit the Millennium DOS GX startup boundary";
        return rejected;
    }
}

MillenniumDosGxActiveTraceAdmission
ReleaseRuntimeCoordinator::admit_active_millennium_dos_gx_startup_reference_trace(
    const ReferenceTrace& trace) {
    MillenniumDosGxActiveTraceAdmission result;
    constexpr std::string_view release_sha256 =
        "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123";
    if (admission_ != ReleaseRuntimeAdmission::active || !active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title_handoff_boundary
        || active_->release.game != Game::millennium || active_->release.platform != Platform::dos
        || active_->release.language != "en" || active_->release.sha256 != release_sha256
        || trace.source_release.game != active_->release.game
        || trace.source_release.platform != active_->release.platform
        || trace.source_release.language != active_->release.language
        || trace.source_release.sha256 != active_->release.sha256
        || trace.source_release.path != active_->release.path
        || trace.source_release.layout != active_->release.layout
        || trace.source_release.containers != active_->release.containers) {
        result.error = "GX startup trace requires the exact active English DOS title-handoff boundary";
        return result;
    }
    auto admitted = admit_millennium_dos_gx_startup_reference_trace(trace);
    if (!admitted.session) {
        result.error = admitted.error;
        return result;
    }
    if (admitted.session->state()
            != MillenniumDosGxStartupSessionState::post_overlay_private_interrupt_boundary
        || !admitted.session->evaluation() || !admitted.session->post_overlay_evaluation()) {
        result.error = "GX startup trace did not produce its exact terminal checkpoint";
        return result;
    }
    // Publish only after every source/event rehash and the complete ordered
    // replay succeeded. A rejection leaves the title-handoff state intact.
    millennium_dos_gx_startup_.emplace(std::move(admitted));
    session_snapshot_ = make_runtime_session_snapshot(
        *active_, RuntimeSessionKind::millennium_dos_gx_startup_boundary);
    result.accepted = true;
    return result;
}

std::optional<MillenniumDosGxStartupCheckpoint>
ReleaseRuntimeCoordinator::millennium_dos_gx_startup_checkpoint() const {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_gx_startup_boundary
        || !millennium_dos_gx_startup_ || !millennium_dos_gx_startup_->session) {
        return std::nullopt;
    }
    const auto& session = *millennium_dos_gx_startup_->session;
    const auto& overlay = session.evaluation();
    const auto& continuation = session.post_overlay_evaluation();
    if (!overlay || !continuation
        || session.state()
            != MillenniumDosGxStartupSessionState::post_overlay_private_interrupt_boundary
        || overlay->outcome != MillenniumDosGxOverlayStartupOutcome::overlay_return
        || continuation->outcome
            != MillenniumDosPostOverlayContinuationOutcome::private_interrupt_boundary) {
        return std::nullopt;
    }
    return MillenniumDosGxStartupCheckpoint{
        session.state(), session.observed_post_overlay_call_return_count(),
        overlay->boundary_address, continuation->boundary_address, overlay->overlay_writes};
}

MillenniumDosPostOverlayObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_post_overlay_private_interrupt_return(
    const MillenniumDosPostOverlayPrivateInterruptReturnObservation observation) {
    MillenniumDosPostOverlayObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_gx_startup_boundary
        || !millennium_dos_gx_startup_ || !millennium_dos_gx_startup_->session
        || !millennium_dos_native_process_) {
        result.error = "Post-overlay INT 91h return requires the active GX startup boundary";
        return result;
    }
    const auto& gx_session = *millennium_dos_gx_startup_->session;
    const auto& continuation = gx_session.post_overlay_evaluation();
    if (!continuation || continuation->outcome
            != MillenniumDosPostOverlayContinuationOutcome::private_interrupt_boundary
        || continuation->boundary_address != observation.interrupt_address
        || !continuation->observed_mode_byte) {
        result.error = "Post-overlay INT 91h return is detached from the admitted GX boundary";
        return result;
    }
    try {
        auto loop = millennium_dos_native_process_->make_post_overlay_loop_session(
            *continuation->observed_mode_byte);
        loop.observe_private_interrupt_return(observation.interrupt_address, observation.ax);
        millennium_dos_post_overlay_loop_.emplace(std::move(loop));
        session_snapshot_ = make_runtime_session_snapshot(
            *active_, RuntimeSessionKind::millennium_dos_post_overlay_loop);
        result.accepted = true;
    } catch (const std::exception& exception) {
        result.error = std::string("Post-overlay INT 91h return rejected: ") + exception.what();
    }
    return result;
}

MillenniumDosPostOverlayObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_post_overlay_call_return(
    const MillenniumDosPostOverlayCallReturnObservation observation) {
    MillenniumDosPostOverlayObservationResult result;
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_post_overlay_loop
        || !millennium_dos_post_overlay_loop_) {
        result.error = "Call return requires the active post-overlay loop";
        return result;
    }
    try {
        millennium_dos_post_overlay_loop_->observe_call_return(
            observation.call_address, observation.return_address);
        result.accepted = true;
    } catch (const std::exception& exception) {
        result.error = std::string("Post-overlay call return rejected: ") + exception.what();
    }
    return result;
}

MillenniumDosPostOverlayObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_post_overlay_al(
    const MillenniumDosPostOverlayAlObservation observation) {
    MillenniumDosPostOverlayObservationResult result;
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_post_overlay_loop
        || !millennium_dos_post_overlay_loop_) {
        result.error = "AL observation requires the active post-overlay loop";
        return result;
    }
    try {
        millennium_dos_post_overlay_loop_->observe_al(observation.test_address, observation.value);
        result.accepted = true;
    } catch (const std::exception& exception) {
        result.error = std::string("Post-overlay AL observation rejected: ") + exception.what();
    }
    return result;
}

MillenniumDosPostOverlayObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_post_overlay_runtime_byte(
    const MillenniumDosPostOverlayRuntimeByteObservation observation) {
    MillenniumDosPostOverlayObservationResult result;
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_post_overlay_loop
        || !millennium_dos_post_overlay_loop_) {
        result.error = "Runtime-byte observation requires the active post-overlay loop";
        return result;
    }
    try {
        millennium_dos_post_overlay_loop_->observe_runtime_byte(
            observation.load_address, observation.runtime_address, observation.value);
        result.accepted = true;
    } catch (const std::exception& exception) {
        result.error = std::string("Post-overlay runtime-byte observation rejected: ")
            + exception.what();
    }
    return result;
}

std::optional<MillenniumDosPostOverlayLoopCheckpoint>
ReleaseRuntimeCoordinator::millennium_dos_post_overlay_loop_checkpoint() const {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_post_overlay_loop
        || !millennium_dos_post_overlay_loop_) return std::nullopt;
    const auto& loop = *millennium_dos_post_overlay_loop_;
    return MillenniumDosPostOverlayLoopCheckpoint{
        loop.state(), loop.boundary(), loop.completed_call_return_count(),
        loop.action_poll_count(), loop.dispatch_generation(), loop.observed_private_interrupt_ax(),
        loop.observed_action(), loop.function_key_index(), loop.runtime_effects()};
}

MillenniumDosTenthFunctionObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_tenth_function_dispatch(
    const MillenniumDosTenthFunctionDispatchObservation observation) {
    MillenniumDosTenthFunctionObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_post_overlay_loop
        || !millennium_dos_post_overlay_loop_ || !millennium_dos_native_process_) {
        result.error = "Tenth-function dispatch requires the active post-overlay loop";
        return result;
    }
    const auto dispatch = millennium_dos_native_process_->admit_function_dispatch(
        *millennium_dos_post_overlay_loop_, {observation.scaled_call_address,
            observation.dispatcher_address, observation.function_key_index,
            observation.handler_address});
    if (observation.function_key_index != 9 || observation.handler_address != 0x7384
        || !dispatch.accepted) {
        result.error = "Tenth-function handler observation is detached from scaled dispatch index 9";
        return result;
    }
    try {
        millennium_dos_tenth_function_.emplace(
            millennium_dos_native_process_->make_tenth_function_session());
        session_snapshot_ = make_runtime_session_snapshot(
            *active_, RuntimeSessionKind::millennium_dos_tenth_function);
        result.accepted = true;
    } catch (const std::exception& exception) {
        result.error = std::string("Tenth-function dispatch rejected: ") + exception.what();
    }
    return result;
}

#define EON_TENTH_FORWARD(method_name, observation_type, call_expression, label) \
MillenniumDosTenthFunctionObservationResult ReleaseRuntimeCoordinator::method_name( \
    const observation_type observation) { \
    MillenniumDosTenthFunctionObservationResult result; \
    if (!session_snapshot_ \
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_tenth_function \
        || !millennium_dos_tenth_function_) { \
        result.error = label " requires the active tenth-function session"; \
        return result; \
    } \
    try { \
        millennium_dos_tenth_function_->call_expression; \
        result.accepted = true; \
    } catch (const std::exception& exception) { \
        result.error = std::string(label " rejected: ") + exception.what(); \
    } \
    return result; \
}

EON_TENTH_FORWARD(observe_millennium_dos_tenth_function_word,
    MillenniumDosTenthFunctionWordObservation,
    observe_runtime_word(observation.instruction_address, observation.runtime_address,
        observation.value), "Tenth-function word observation")
EON_TENTH_FORWARD(observe_millennium_dos_tenth_function_byte,
    MillenniumDosTenthFunctionByteObservation,
    observe_runtime_byte(observation.instruction_address, observation.runtime_address,
        observation.value), "Tenth-function byte observation")
EON_TENTH_FORWARD(observe_millennium_dos_tenth_function_call_return,
    MillenniumDosTenthFunctionCallReturnObservation,
    observe_call_return(observation.call_address, observation.return_address),
    "Tenth-function call return")
EON_TENTH_FORWARD(observe_millennium_dos_tenth_function_zero_flag,
    MillenniumDosTenthFunctionZeroFlagObservation,
    observe_zero_flag(observation.branch_address, observation.set),
    "Tenth-function zero flag")
EON_TENTH_FORWARD(observe_millennium_dos_tenth_function_bl,
    MillenniumDosTenthFunctionBlObservation,
    observe_bl(observation.shift_address, observation.value),
    "Tenth-function BL observation")
#undef EON_TENTH_FORWARD

std::optional<MillenniumDosTenthFunctionCheckpoint>
ReleaseRuntimeCoordinator::millennium_dos_tenth_function_checkpoint() const {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_tenth_function
        || !millennium_dos_tenth_function_) return std::nullopt;
    const auto& session = *millennium_dos_tenth_function_;
    return MillenniumDosTenthFunctionCheckpoint{session.state(), session.boundary(),
        session.limit_loop_count(), session.wait_loop_count(), session.runtime_effects()};
}

MillenniumDosSeventhFunctionObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_seventh_function_dispatch(
    const MillenniumDosSeventhFunctionDispatchObservation observation) {
    MillenniumDosSeventhFunctionObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_post_overlay_loop
        || !millennium_dos_post_overlay_loop_ || !millennium_dos_native_process_) {
        result.error = "Seventh-function dispatch requires the active post-overlay loop";
        return result;
    }
    const auto dispatch = millennium_dos_native_process_->admit_function_dispatch(
        *millennium_dos_post_overlay_loop_, {observation.scaled_call_address,
            observation.dispatcher_address, observation.function_key_index,
            observation.handler_address});
    if (observation.function_key_index != 6 || observation.handler_address != 0x7521
        || !dispatch.accepted) {
        result.error = "Seventh-function handler observation is detached from scaled dispatch index 6";
        return result;
    }
    try {
        millennium_dos_seventh_function_.emplace(
            millennium_dos_native_process_->make_seventh_function_session());
        session_snapshot_ = make_runtime_session_snapshot(
            *active_, RuntimeSessionKind::millennium_dos_seventh_function);
        result.accepted = true;
    } catch (const std::exception& exception) {
        result.error = std::string("Seventh-function dispatch rejected: ") + exception.what();
    }
    return result;
}

#define EON_SEVENTH_FORWARD(method_name, observation_type, call_expression, label) \
MillenniumDosSeventhFunctionObservationResult ReleaseRuntimeCoordinator::method_name( \
    const observation_type observation) { \
    MillenniumDosSeventhFunctionObservationResult result; \
    if (!session_snapshot_ \
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_seventh_function \
        || !millennium_dos_seventh_function_) { \
        result.error = label " requires the active seventh-function session"; \
        return result; \
    } \
    try { \
        millennium_dos_seventh_function_->call_expression; \
        result.accepted = true; \
    } catch (const std::exception& exception) { \
        result.error = std::string(label " rejected: ") + exception.what(); \
    } \
    return result; \
}
EON_SEVENTH_FORWARD(observe_millennium_dos_seventh_function_word,
    MillenniumDosSeventhFunctionWordObservation,
    observe_runtime_word(observation.instruction_address, observation.runtime_address,
        observation.value), "Seventh-function word observation")
EON_SEVENTH_FORWARD(observe_millennium_dos_seventh_function_byte,
    MillenniumDosSeventhFunctionByteObservation,
    observe_runtime_byte(observation.instruction_address, observation.runtime_address,
        observation.value), "Seventh-function byte observation")
EON_SEVENTH_FORWARD(observe_millennium_dos_seventh_function_call_return,
    MillenniumDosSeventhFunctionCallReturnObservation,
    observe_call_return(observation.call_address, observation.return_address),
    "Seventh-function call return")
EON_SEVENTH_FORWARD(observe_millennium_dos_seventh_function_returned_bx,
    MillenniumDosSeventhFunctionReturnedBxObservation,
    observe_returned_bx(observation.store_address, observation.value),
    "Seventh-function returned BX")
#undef EON_SEVENTH_FORWARD

std::optional<MillenniumDosSeventhFunctionCheckpoint>
ReleaseRuntimeCoordinator::millennium_dos_seventh_function_checkpoint() const {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_seventh_function
        || !millennium_dos_seventh_function_) return std::nullopt;
    const auto& session = *millennium_dos_seventh_function_;
    return MillenniumDosSeventhFunctionCheckpoint{session.boundary(), session.returned(),
        session.returned_by_guard(), session.runtime_effects()};
}

MillenniumDosSharedHelperObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_shared_helper_entry(const MillenniumDosSharedHelperEntryObservation o){MillenniumDosSharedHelperObservationResult r;if(!session_snapshot_||(session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_seventh_function&&session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_post_overlay_loop)||!millennium_dos_seventh_function_||!millennium_dos_native_process_||millennium_dos_shared_helper_){r.error="Shared helper entry requires one active F7 call boundary";return r;}const auto b=millennium_dos_seventh_function_->boundary();if(o.sequence==0||o.call_instruction!=0x7537||o.target_address!=0x0666||o.caller_ax!=0x012a||b.kind!=MillenniumDosSeventhFunctionBoundaryKind::call_return||b.instruction_address!=o.call_instruction||b.call_target!=o.target_address||b.known_ax!=o.caller_ax){r.error="Shared helper entry does not match the exact F7 call";return r;}try{millennium_dos_shared_helper_.emplace(millennium_dos_native_process_->make_shared_helper_session(o.caller_ax));millennium_dos_shared_helper_entry_=o;r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
MillenniumDosSharedHelperObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_special_action_helper_entry(const MillenniumDosSpecialActionHelperEntryObservation o){MillenniumDosSharedHelperObservationResult r;if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_post_overlay_loop||!millennium_dos_post_overlay_loop_||!millennium_dos_native_process_||millennium_dos_shared_helper_){r.error="Special-action helper requires its active dispatch boundary";return r;}const auto b=millennium_dos_post_overlay_loop_->boundary();if(o.sequence==0||o.dispatch_call!=0xd40e||o.dispatch_target!=0x11a4||o.runtime_address!=0x07f9||o.helper_call!=0x11b7||o.helper_target!=0x0666||b.kind!=MillenniumDosPostOverlayLoopBoundaryKind::dispatch_call||b.instruction_address!=o.dispatch_call||b.call_target!=o.dispatch_target){r.error="Special-action helper entry does not match exact dispatch";return r;}try{auto game=millennium_dos_native_process_->make_game_session();const auto action=game.observe_action({0x0f05,0x0b});const auto prefix=game.observe_first_special_action({o.runtime_address,o.runtime_value});if(prefix.selected_ax_value!=o.caller_ax)throw std::runtime_error("Explicit AX differs from prefix");const auto helper_prefix=game.observe_first_special_action_shared_helper_prefix();(void)action;(void)helper_prefix;millennium_dos_shared_helper_.emplace(millennium_dos_native_process_->make_shared_helper_session(o.caller_ax));millennium_dos_shared_helper_entry_=MillenniumDosSharedHelperEntryObservation{o.helper_call,o.helper_target,o.caller_ax,o.sequence};millennium_dos_special_action_=std::move(game);r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
#define EON_SHARED_HELPER_FORWARD(name,type,body) MillenniumDosSharedHelperObservationResult ReleaseRuntimeCoordinator::name(const type o){MillenniumDosSharedHelperObservationResult r;if(!session_snapshot_||(session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_seventh_function&&session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_post_overlay_loop)||!millennium_dos_shared_helper_||millennium_dos_shared_helper_return_){r.error="No active shared helper";return r;}try{millennium_dos_shared_helper_->body;r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
EON_SHARED_HELPER_FORWARD(observe_millennium_dos_shared_helper_word,MillenniumDosSharedHelperWordObservation,observe_runtime_word(o.instruction_address,o.address,o.value))
EON_SHARED_HELPER_FORWARD(observe_millennium_dos_shared_helper_far_word,MillenniumDosSharedHelperFarWordObservation,observe_far_word(o.instruction_address,o.segment,o.offset,o.value))
EON_SHARED_HELPER_FORWARD(observe_millennium_dos_shared_helper_call_return,MillenniumDosSharedHelperCallReturnObservation,observe_call_return(o.call_address,o.return_address))
MillenniumDosSharedHelperObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_shared_helper_external_return(const MillenniumDosSharedHelperExternalReturnObservation o){MillenniumDosSharedHelperObservationResult r;if(!session_snapshot_||!millennium_dos_shared_helper_||!millennium_dos_shared_helper_entry_||millennium_dos_shared_helper_return_||millennium_dos_shared_helper_->state()!=MillenniumDosSharedHelperState::returned||o.sequence<=millennium_dos_shared_helper_entry_->sequence||o.return_instruction!=0x0681){r.error="Shared helper return does not match entered call";return r;}const bool special=millennium_dos_shared_helper_entry_->call_instruction==0x11b7;const auto destination=std::uint16_t(special?0x11ba:0x753a);if(o.returned_to!=destination){r.error="Shared helper return destination is not exact";return r;}try{if(!special){if(!millennium_dos_seventh_function_)throw std::runtime_error("Missing F7 parent");millennium_dos_seventh_function_->observe_call_return(0x7537,0x753a);}millennium_dos_shared_helper_return_=o;r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
MillenniumDosSharedHelperObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_special_action_external_return(const MillenniumDosSharedHelperExternalReturnObservation o){MillenniumDosSharedHelperObservationResult r;if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_post_overlay_loop||!millennium_dos_post_overlay_loop_||!millennium_dos_special_action_||!millennium_dos_shared_helper_return_||millennium_dos_special_action_return_||o.sequence<=millennium_dos_shared_helper_return_->sequence||o.return_instruction!=0x11c1||o.returned_to!=0xd411){r.error="Special-action return does not match exact parent RET";return r;}try{millennium_dos_post_overlay_loop_->observe_dispatch_return(0xd40e,0xd411);millennium_dos_special_action_return_=o;r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}

MillenniumDosGxAdapterObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_second_special_action_adapter_entry(const MillenniumDosGxAdapterEntryObservation o){MillenniumDosGxAdapterObservationResult r;if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_post_overlay_loop||!millennium_dos_post_overlay_loop_||!millennium_dos_native_process_||millennium_dos_gx_adapter_){r.error="GX adapter requires active post-overlay dispatch";return r;}const auto b=millennium_dos_post_overlay_loop_->boundary();if(o.sequence==0||o.dispatch_call!=0xd3f4||o.dispatch_target!=0xd570||o.runtime_address!=0xda3a||o.runtime_value!=0||o.helper_call!=0xd573||o.helper_target!=0x6c52||o.caller_ax!=0x000d||o.code_segment==0||b.kind!=MillenniumDosPostOverlayLoopBoundaryKind::dispatch_call||b.instruction_address!=o.dispatch_call||b.call_target!=o.dispatch_target){r.error="GX adapter entry does not match exact action-$0c route";return r;}try{auto game=millennium_dos_native_process_->make_game_session();static_cast<void>(game.observe_action({0x0f05,0x0c}));const auto prefix=game.observe_second_special_action({o.runtime_address,o.runtime_value});if(prefix.selected_ax_value!=o.caller_ax||prefix.helper_call_address!=o.helper_call||prefix.helper_address!=o.helper_target)throw std::runtime_error("GX adapter entry differs from verified prefix");millennium_dos_gx_adapter_.emplace(millennium_dos_native_process_->make_gx_overlay_adapter_session(o.caller_ax,0xd576,o.code_segment));millennium_dos_gx_adapter_entry_=o;r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
MillenniumDosGxAdapterObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_gx_adapter_segment(const MillenniumDosGxAdapterWordObservation o){MillenniumDosGxAdapterObservationResult r;if(!millennium_dos_gx_adapter_||!millennium_dos_gx_adapter_entry_||millennium_dos_gx_adapter_segment_||o.sequence<=millennium_dos_gx_adapter_entry_->sequence){r.error="No active GX adapter segment boundary";return r;}try{millennium_dos_gx_adapter_->observe_segment(o.instruction_address,o.runtime_address,o.value);millennium_dos_gx_adapter_segment_=o;r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
MillenniumDosGxAdapterObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_gx_adapter_transfer(const MillenniumDosGxAdapterTransferObservation o){MillenniumDosGxAdapterObservationResult r;if(!millennium_dos_gx_adapter_||!millennium_dos_gx_adapter_segment_||millennium_dos_gx_adapter_transfer_||o.sequence<=millennium_dos_gx_adapter_segment_->sequence){r.error="No active GX adapter transfer boundary";return r;}try{millennium_dos_gx_adapter_->observe_far_transfer(o.instruction_address,o.segment,o.offset);millennium_dos_gx_adapter_transfer_=o;r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
MillenniumDosGxAdapterObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_gx_adapter_overlay_return(const MillenniumDosGxAdapterReturnObservation o){MillenniumDosGxAdapterObservationResult r;if(!millennium_dos_gx_adapter_||!millennium_dos_gx_adapter_transfer_||millennium_dos_gx_overlay_return_||o.sequence<=millennium_dos_gx_adapter_transfer_->sequence){r.error="No active GX overlay return boundary";return r;}try{millennium_dos_gx_adapter_->observe_overlay_return(o.instruction_address,o.segment,o.offset);millennium_dos_gx_overlay_return_=o;r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
MillenniumDosGxAdapterObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_gx_adapter_return(const MillenniumDosGxAdapterReturnObservation o){MillenniumDosGxAdapterObservationResult r;if(!millennium_dos_gx_adapter_||!millennium_dos_gx_overlay_return_||millennium_dos_gx_adapter_return_||o.sequence<=millennium_dos_gx_overlay_return_->sequence){r.error="No active GX adapter caller-return boundary";return r;}try{millennium_dos_gx_adapter_->observe_caller_return(o.instruction_address,o.segment,o.offset);millennium_dos_gx_adapter_return_=o;r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
MillenniumDosGxAdapterObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_second_special_action_return(const MillenniumDosGxAdapterReturnObservation o){MillenniumDosGxAdapterObservationResult r;if(!millennium_dos_post_overlay_loop_||!millennium_dos_gx_adapter_||millennium_dos_gx_adapter_->state()!=MillenniumDosGxOverlayAdapterState::returned||!millennium_dos_gx_adapter_entry_||!millennium_dos_gx_adapter_return_||millennium_dos_second_special_action_return_||o.sequence<=millennium_dos_gx_adapter_return_->sequence||o.instruction_address!=0xd576||o.segment!=millennium_dos_gx_adapter_entry_->code_segment||o.offset!=0xd3f7){r.error="Second special-action return does not match exact parent RET";return r;}try{millennium_dos_post_overlay_loop_->observe_dispatch_return(0xd3f4,0xd3f7);millennium_dos_second_special_action_return_=o;r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
std::optional<MillenniumDosGxAdapterCheckpoint> ReleaseRuntimeCoordinator::millennium_dos_gx_adapter_checkpoint()const{if(!millennium_dos_gx_adapter_||!millennium_dos_gx_adapter_entry_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_post_overlay_loop)return std::nullopt;return MillenniumDosGxAdapterCheckpoint{millennium_dos_gx_adapter_->state(),millennium_dos_gx_adapter_->boundary(),*millennium_dos_gx_adapter_entry_,millennium_dos_gx_adapter_segment_,millennium_dos_gx_adapter_transfer_,millennium_dos_gx_overlay_return_,millennium_dos_gx_adapter_return_,millennium_dos_second_special_action_return_};}
#undef EON_SHARED_HELPER_FORWARD
std::optional<MillenniumDosSharedHelperCheckpoint>ReleaseRuntimeCoordinator::millennium_dos_shared_helper_checkpoint()const{if(!session_snapshot_||(session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_seventh_function&&session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_post_overlay_loop)||!millennium_dos_shared_helper_||!millennium_dos_shared_helper_entry_)return std::nullopt;const auto&s=*millennium_dos_shared_helper_;return MillenniumDosSharedHelperCheckpoint{s.state(),s.boundary(),s.effects(),s.selected_offset(),*millennium_dos_shared_helper_entry_,millennium_dos_shared_helper_return_,millennium_dos_special_action_return_};}

MillenniumDosSixthFunctionObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_sixth_function_dispatch(
    const MillenniumDosSixthFunctionDispatchObservation observation) {
    MillenniumDosSixthFunctionObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_post_overlay_loop
        || !millennium_dos_post_overlay_loop_ || !millennium_dos_native_process_) {
        result.error = "Sixth-function dispatch requires the active post-overlay loop";
        return result;
    }
    const auto dispatch = millennium_dos_native_process_->admit_function_dispatch(
        *millennium_dos_post_overlay_loop_, {observation.scaled_call_address,
            observation.dispatcher_address, observation.function_key_index,
            observation.handler_address});
    if (observation.function_key_index != 5 || observation.handler_address != 0x7415
        || !dispatch.accepted) {
        result.error = "Sixth-function handler observation is detached from scaled dispatch index 5";
        return result;
    }
    try {
        millennium_dos_sixth_function_.emplace(
            millennium_dos_native_process_->make_sixth_function_session());
        session_snapshot_ = make_runtime_session_snapshot(
            *active_, RuntimeSessionKind::millennium_dos_sixth_function);
        result.accepted = true;
    } catch (const std::exception& exception) {
        result.error = std::string("Sixth-function dispatch rejected: ") + exception.what();
    }
    return result;
}

#define EON_SIXTH_FORWARD(method_name, observation_type, call_expression, label) \
MillenniumDosSixthFunctionObservationResult ReleaseRuntimeCoordinator::method_name( \
    const observation_type observation) { \
    MillenniumDosSixthFunctionObservationResult result; \
    if (!session_snapshot_ \
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_sixth_function \
        || !millennium_dos_sixth_function_) { \
        result.error = label " requires the active sixth-function session"; \
        return result; \
    } \
    try { \
        millennium_dos_sixth_function_->call_expression; \
        result.accepted = true; \
    } catch (const std::exception& exception) { \
        result.error = std::string(label " rejected: ") + exception.what(); \
    } \
    return result; \
}
EON_SIXTH_FORWARD(observe_millennium_dos_sixth_function_word,
    MillenniumDosSixthFunctionWordObservation,
    observe_runtime_word(observation.instruction_address, observation.runtime_address,
        observation.value), "Sixth-function word observation")
EON_SIXTH_FORWARD(observe_millennium_dos_sixth_function_byte,
    MillenniumDosSixthFunctionByteObservation,
    observe_runtime_byte(observation.instruction_address, observation.runtime_address,
        observation.value), "Sixth-function byte observation")
EON_SIXTH_FORWARD(observe_millennium_dos_sixth_function_call_return,
    MillenniumDosSixthFunctionCallReturnObservation,
    observe_call_return(observation.call_address, observation.return_address),
    "Sixth-function call return")
EON_SIXTH_FORWARD(observe_millennium_dos_sixth_function_bl,
    MillenniumDosSixthFunctionBlObservation,
    observe_bl(observation.shift_address, observation.value),
    "Sixth-function BL observation")
#undef EON_SIXTH_FORWARD

std::optional<MillenniumDosSixthFunctionCheckpoint>
ReleaseRuntimeCoordinator::millennium_dos_sixth_function_checkpoint() const {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_sixth_function
        || !millennium_dos_sixth_function_) return std::nullopt;
    const auto& session = *millennium_dos_sixth_function_;
    return MillenniumDosSixthFunctionCheckpoint{session.state(), session.boundary(),
        session.effects(), session.shifted_bl_values()};
}

MillenniumDosEighthFunctionObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_eighth_function_dispatch(
    const MillenniumDosEighthFunctionDispatchObservation observation) {
    MillenniumDosEighthFunctionObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_post_overlay_loop
        || !millennium_dos_post_overlay_loop_ || !millennium_dos_native_process_) {
        result.error = "Eighth-function dispatch requires the active post-overlay loop";
        return result;
    }
    const auto dispatch = millennium_dos_native_process_->admit_function_dispatch(
        *millennium_dos_post_overlay_loop_, {observation.scaled_call_address,
            observation.dispatcher_address, observation.function_key_index,
            observation.handler_address});
    if (observation.function_key_index != 7 || observation.handler_address != 0x7306
        || !dispatch.accepted) {
        result.error = "Eighth-function handler observation is detached from scaled dispatch index 7";
        return result;
    }
    try {
        millennium_dos_eighth_function_.emplace(
            millennium_dos_native_process_->make_eighth_function_session());
        session_snapshot_ = make_runtime_session_snapshot(
            *active_, RuntimeSessionKind::millennium_dos_eighth_function);
        result.accepted = true;
    } catch (const std::exception& exception) {
        result.error = std::string("Eighth-function dispatch rejected: ") + exception.what();
    }
    return result;
}

#define EON_EIGHTH_FORWARD(method_name, observation_type, expression, label) \
MillenniumDosEighthFunctionObservationResult ReleaseRuntimeCoordinator::method_name( \
    const observation_type observation) { \
    MillenniumDosEighthFunctionObservationResult result; \
    if (!session_snapshot_ \
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_eighth_function \
        || !millennium_dos_eighth_function_) { \
        result.error = label " requires the active eighth-function session"; \
        return result; \
    } \
    try { millennium_dos_eighth_function_->expression; result.accepted = true; } \
    catch (const std::exception& exception) { \
        result.error = std::string(label " rejected: ") + exception.what(); \
    } \
    return result; \
}
EON_EIGHTH_FORWARD(observe_millennium_dos_eighth_function_call_return,
    MillenniumDosEighthFunctionCallReturnObservation,
    observe_call_return(observation.call_address, observation.return_address),
    "Eighth-function call return")
EON_EIGHTH_FORWARD(observe_millennium_dos_eighth_function_bl,
    MillenniumDosEighthFunctionBlObservation,
    observe_bl(observation.shift_address, observation.value),
    "Eighth-function BL observation")
#undef EON_EIGHTH_FORWARD

std::optional<MillenniumDosEighthFunctionCheckpoint>
ReleaseRuntimeCoordinator::millennium_dos_eighth_function_checkpoint() const {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_eighth_function
        || !millennium_dos_eighth_function_) return std::nullopt;
    const auto& session = *millennium_dos_eighth_function_;
    return MillenniumDosEighthFunctionCheckpoint{session.state(), session.boundary(),
        session.effects(), session.shifted_bl_values()};
}

MillenniumDosNinthFunctionObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_ninth_function_dispatch(const MillenniumDosNinthFunctionDispatchObservation o) {
    MillenniumDosNinthFunctionObservationResult r;
    if(!active_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_post_overlay_loop||!millennium_dos_post_overlay_loop_||!millennium_dos_native_process_) { r.error="Ninth-function dispatch requires the active post-overlay loop"; return r; }
    const auto b=millennium_dos_post_overlay_loop_->boundary();
    if(millennium_dos_post_overlay_loop_->state()!=MillenniumDosPostOverlayLoopState::dispatch_call_boundary||b.kind!=MillenniumDosPostOverlayLoopBoundaryKind::dispatch_call||b.instruction_address!=0xd40a||b.call_target!=std::optional<std::uint16_t>{0x76f1}||millennium_dos_post_overlay_loop_->function_key_index()!=std::optional<std::size_t>{8}||o.scaled_call_address!=b.instruction_address||o.dispatcher_address!=*b.call_target||o.function_key_index!=8||o.handler_address!=0x7339) { r.error="Ninth-function handler observation is detached from scaled dispatch index 8"; return r; }
    try { millennium_dos_ninth_function_.emplace(millennium_dos_native_process_->make_ninth_function_session()); session_snapshot_=make_runtime_session_snapshot(*active_,RuntimeSessionKind::millennium_dos_ninth_function); r.accepted=true; } catch(const std::exception& e) { r.error=std::string("Ninth-function dispatch rejected: ")+e.what(); } return r;
}
#define EON_NINTH_FORWARD(name,type,expr,label) MillenniumDosNinthFunctionObservationResult ReleaseRuntimeCoordinator::name(const type o) { MillenniumDosNinthFunctionObservationResult r; if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_ninth_function||!millennium_dos_ninth_function_) { r.error=label " requires the active ninth-function session"; return r; } try { millennium_dos_ninth_function_->expr; r.accepted=true; } catch(const std::exception& e) { r.error=std::string(label " rejected: ")+e.what(); } return r; }
EON_NINTH_FORWARD(observe_millennium_dos_ninth_function_word,MillenniumDosNinthFunctionWordObservation,observe_runtime_word(o.instruction_address,o.runtime_address,o.value),"Ninth-function word observation")
EON_NINTH_FORWARD(observe_millennium_dos_ninth_function_byte,MillenniumDosNinthFunctionByteObservation,observe_runtime_byte(o.instruction_address,o.runtime_address,o.value),"Ninth-function byte observation")
EON_NINTH_FORWARD(observe_millennium_dos_ninth_function_call_return,MillenniumDosNinthFunctionCallReturnObservation,observe_call_return(o.call_address,o.return_address),"Ninth-function call return")
#undef EON_NINTH_FORWARD
std::optional<MillenniumDosNinthFunctionCheckpoint> ReleaseRuntimeCoordinator::millennium_dos_ninth_function_checkpoint() const { if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_ninth_function||!millennium_dos_ninth_function_) return std::nullopt; const auto& s=*millennium_dos_ninth_function_; return MillenniumDosNinthFunctionCheckpoint{s.state(),s.boundary(),s.loop_count(),s.effects()}; }
MillenniumDosNinthHandoffObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_ninth_handoff_entry(const MillenniumDosNinthHandoffEntryObservation o){MillenniumDosNinthHandoffObservationResult r;if(!active_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_ninth_function||!millennium_dos_ninth_function_||!millennium_dos_native_process_||millennium_dos_ninth_function_->boundary().kind!=MillenniumDosNinthFunctionBoundaryKind::jump_handoff){r.error="F9 continuation requires exact active $7381 -> $73cc handoff";return r;}MillenniumDosExternalTransferAdmission transfer(MillenniumDosExternalTransferKind::f9_short_return);if(!transfer.observe_entry({o.sequence,o.instruction_address,o.target_address}).accepted){r.error="F9 continuation entry rejected by external-transfer contract";return r;}try{millennium_dos_ninth_handoff_.emplace(millennium_dos_native_process_->make_ninth_function_handoff_session());session_snapshot_=make_runtime_session_snapshot(*active_,RuntimeSessionKind::millennium_dos_ninth_function_handoff);r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
#define EON_F9_HANDOFF(name,type,body) MillenniumDosNinthHandoffObservationResult ReleaseRuntimeCoordinator::name(const type o){MillenniumDosNinthHandoffObservationResult r;if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_ninth_function_handoff||!millennium_dos_ninth_handoff_){r.error="No active F9 handoff session";return r;}try{millennium_dos_ninth_handoff_->body;r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
EON_F9_HANDOFF(observe_millennium_dos_ninth_handoff_byte,MillenniumDosNinthHandoffByteObservation,observe_runtime_byte(o.instruction_address,o.runtime_address,o.value)) EON_F9_HANDOFF(observe_millennium_dos_ninth_handoff_word,MillenniumDosNinthHandoffWordObservation,observe_runtime_word(o.instruction_address,o.runtime_address,o.value)) EON_F9_HANDOFF(observe_millennium_dos_ninth_handoff_call_return,MillenniumDosNinthHandoffCallReturnObservation,observe_call_return(o.call_address,o.return_address)) EON_F9_HANDOFF(observe_millennium_dos_ninth_handoff_zero_flag,MillenniumDosNinthHandoffZeroFlagObservation,observe_zero_flag(o.instruction_address,o.set)) EON_F9_HANDOFF(observe_millennium_dos_ninth_handoff_bl,MillenniumDosNinthHandoffBlObservation,observe_bl(o.instruction_address,o.value))
#undef EON_F9_HANDOFF
std::optional<MillenniumDosNinthHandoffCheckpoint> ReleaseRuntimeCoordinator::millennium_dos_ninth_handoff_checkpoint()const{if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_ninth_function_handoff||!millennium_dos_ninth_handoff_)return std::nullopt;const auto&s=*millennium_dos_ninth_handoff_;return MillenniumDosNinthHandoffCheckpoint{s.state(),s.boundary(),s.effects()};}
MillenniumDosFourthFunctionObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_fourth_function_dispatch(const MillenniumDosFourthFunctionDispatchObservation o){MillenniumDosFourthFunctionObservationResult r;if(!active_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_post_overlay_loop||!millennium_dos_post_overlay_loop_||!millennium_dos_native_process_){r.error="Fourth-function dispatch requires the active post-overlay loop";return r;}auto d=millennium_dos_native_process_->admit_function_dispatch(*millennium_dos_post_overlay_loop_,{o.scaled_call_address,o.dispatcher_address,o.function_key_index,o.handler_address});if(!d.accepted||o.function_key_index!=3||o.handler_address!=0x72f9){r.error="Fourth-function observation is detached from dispatch index 3";return r;}try{millennium_dos_fourth_function_.emplace(millennium_dos_native_process_->make_fourth_function_session());session_snapshot_=make_runtime_session_snapshot(*active_,RuntimeSessionKind::millennium_dos_fourth_function);r.accepted=true;}catch(const std::exception& e){r.error=e.what();}return r;}
#define EON_FOURTH_FORWARD(name,type,expr) MillenniumDosFourthFunctionObservationResult ReleaseRuntimeCoordinator::name(const type o){MillenniumDosFourthFunctionObservationResult r;if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_fourth_function||!millennium_dos_fourth_function_){r.error="Observation requires active fourth-function session";return r;}try{millennium_dos_fourth_function_->expr;r.accepted=true;}catch(const std::exception& e){r.error=e.what();}return r;}
EON_FOURTH_FORWARD(observe_millennium_dos_fourth_function_word,MillenniumDosFourthFunctionWordObservation,observe_runtime_word(o.instruction_address,o.runtime_address,o.value))
EON_FOURTH_FORWARD(observe_millennium_dos_fourth_function_call_return,MillenniumDosFourthFunctionCallReturnObservation,observe_call_return(o.call_address,o.return_address))
#undef EON_FOURTH_FORWARD
std::optional<MillenniumDosFourthFunctionCheckpoint> ReleaseRuntimeCoordinator::millennium_dos_fourth_function_checkpoint() const{if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_fourth_function||!millennium_dos_fourth_function_)return std::nullopt;auto&s=*millennium_dos_fourth_function_;return MillenniumDosFourthFunctionCheckpoint{s.state(),s.boundary(),s.effects()};}
MillenniumDosFifthFunctionObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_fifth_function_dispatch(MillenniumDosFifthFunctionDispatchObservation o){MillenniumDosFifthFunctionObservationResult r;if(!active_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_post_overlay_loop||!millennium_dos_post_overlay_loop_||!millennium_dos_native_process_){r.error="Fifth-function dispatch requires post-overlay loop";return r;}auto d=millennium_dos_native_process_->admit_function_dispatch(*millennium_dos_post_overlay_loop_,{o.scaled_call_address,o.dispatcher_address,o.function_key_index,o.handler_address});if(!d.accepted||o.function_key_index!=4||o.handler_address!=0x7597){r.error="Detached fifth-function dispatch";return r;}millennium_dos_fifth_function_.emplace(millennium_dos_native_process_->make_fifth_function_session());session_snapshot_=make_runtime_session_snapshot(*active_,RuntimeSessionKind::millennium_dos_fifth_function);r.accepted=true;return r;} MillenniumDosFifthFunctionObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_fifth_function_call_return(MillenniumDosFifthFunctionCallReturnObservation o){MillenniumDosFifthFunctionObservationResult r;if(!millennium_dos_fifth_function_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_fifth_function){r.error="No fifth-function session";return r;}try{millennium_dos_fifth_function_->observe_call_return(o.call_address,o.return_address);r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;} std::optional<MillenniumDosFifthFunctionCheckpoint> ReleaseRuntimeCoordinator::millennium_dos_fifth_function_checkpoint()const{if(!millennium_dos_fifth_function_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_fifth_function)return std::nullopt;return MillenniumDosFifthFunctionCheckpoint{millennium_dos_fifth_function_->state(),millennium_dos_fifth_function_->boundary()};}

MillenniumDosThirdFunctionObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_third_function_dispatch(
    const MillenniumDosThirdFunctionDispatchObservation observation) {
    MillenniumDosThirdFunctionObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_post_overlay_loop
        || !millennium_dos_post_overlay_loop_ || !millennium_dos_native_process_) {
        result.error = "Third-function dispatch requires the active post-overlay loop";
        return result;
    }
    const auto admission = millennium_dos_native_process_->admit_function_dispatch(
        *millennium_dos_post_overlay_loop_, {observation.scaled_call_address,
            observation.dispatcher_address, observation.function_key_index,
            observation.handler_address});
    if (!admission.accepted || observation.function_key_index != 2
        || observation.handler_address != 0x6faa) {
        result.error = "Third-function observation is detached from dispatch index 2";
        return result;
    }
    try {
        millennium_dos_third_function_.emplace(
            millennium_dos_native_process_->make_third_function_session());
        session_snapshot_ = make_runtime_session_snapshot(
            *active_, RuntimeSessionKind::millennium_dos_third_function);
        result.accepted = true;
    } catch (const std::exception& exception) { result.error = exception.what(); }
    return result;
}

#define EON_THIRD_FORWARD(name, type, expression) \
MillenniumDosThirdFunctionObservationResult ReleaseRuntimeCoordinator::name( \
    const type observation) { \
    MillenniumDosThirdFunctionObservationResult result; \
    if (!session_snapshot_ \
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_third_function \
        || !millennium_dos_third_function_) { \
        result.error = "Observation requires active third-function session"; return result; \
    } \
    try { millennium_dos_third_function_->expression; result.accepted = true; } \
    catch (const std::exception& exception) { result.error = exception.what(); } \
    return result; \
}
EON_THIRD_FORWARD(observe_millennium_dos_third_function_word,
    MillenniumDosThirdFunctionWordObservation,
    observe_runtime_word(observation.instruction_address, observation.runtime_address,
        observation.value))
EON_THIRD_FORWARD(observe_millennium_dos_third_function_call_return,
    MillenniumDosThirdFunctionCallReturnObservation,
    observe_call_return(observation.call_address, observation.return_address))
EON_THIRD_FORWARD(observe_millennium_dos_third_function_bl,
    MillenniumDosThirdFunctionBlObservation,
    observe_bl(observation.instruction_address, observation.value))
#undef EON_THIRD_FORWARD

std::optional<MillenniumDosThirdFunctionCheckpoint>
ReleaseRuntimeCoordinator::millennium_dos_third_function_checkpoint() const {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_third_function
        || !millennium_dos_third_function_) return std::nullopt;
    const auto& session = *millennium_dos_third_function_;
    return MillenniumDosThirdFunctionCheckpoint{
        session.state(), session.boundary(), session.effects()};
}

MillenniumDosFirstFunctionObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_first_function_dispatch(
    const MillenniumDosFirstFunctionDispatchObservation observation) {
    MillenniumDosFirstFunctionObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_post_overlay_loop
        || !millennium_dos_post_overlay_loop_ || !millennium_dos_native_process_) {
        result.error = "First-function dispatch requires the active post-overlay loop";
        return result;
    }
    const auto admission = millennium_dos_native_process_->admit_function_dispatch(
        *millennium_dos_post_overlay_loop_, {observation.scaled_call_address,
            observation.dispatcher_address, observation.function_key_index,
            observation.handler_address});
    if (!admission.accepted || observation.function_key_index != 0
        || observation.handler_address != 0x6f9a) {
        result.error = "First-function observation is detached from dispatch index 0";
        return result;
    }
    try {
        millennium_dos_first_function_.emplace(
            millennium_dos_native_process_->make_first_function_session());
        session_snapshot_ = make_runtime_session_snapshot(
            *active_, RuntimeSessionKind::millennium_dos_first_function);
        result.accepted = true;
    } catch (const std::exception& exception) { result.error = exception.what(); }
    return result;
}

MillenniumDosFirstFunctionObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_first_function_call_return(
    const MillenniumDosFirstFunctionCallReturnObservation observation) {
    MillenniumDosFirstFunctionObservationResult result;
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_first_function
        || !millennium_dos_first_function_) { result.error = "No active first-function session"; return result; }
    try { millennium_dos_first_function_->observe_call_return(
        observation.call_address, observation.return_address); result.accepted = true; }
    catch (const std::exception& exception) { result.error = exception.what(); }
    return result;
}

MillenniumDosFirstFunctionObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_first_function_bl(
    const MillenniumDosFirstFunctionBlObservation observation) {
    MillenniumDosFirstFunctionObservationResult result;
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_first_function
        || !millennium_dos_first_function_) { result.error = "No active first-function session"; return result; }
    try { millennium_dos_first_function_->observe_bl(
        observation.instruction_address, observation.value); result.accepted = true; }
    catch (const std::exception& exception) { result.error = exception.what(); }
    return result;
}

std::optional<MillenniumDosFirstFunctionCheckpoint>
ReleaseRuntimeCoordinator::millennium_dos_first_function_checkpoint() const {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_first_function
        || !millennium_dos_first_function_) return std::nullopt;
    const auto& session = *millennium_dos_first_function_;
    return MillenniumDosFirstFunctionCheckpoint{session.state(), session.boundary(), session.effects()};
}

MillenniumDosSecondFunctionObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_second_function_dispatch(const MillenniumDosSecondFunctionDispatchObservation o){MillenniumDosSecondFunctionObservationResult r;if(!active_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_post_overlay_loop||!millennium_dos_post_overlay_loop_||!millennium_dos_native_process_){r.error="Second-function dispatch requires the active post-overlay loop";return r;}const auto a=millennium_dos_native_process_->admit_function_dispatch(*millennium_dos_post_overlay_loop_,{o.scaled_call_address,o.dispatcher_address,o.function_key_index,o.handler_address});if(!a.accepted||o.function_key_index!=1||o.handler_address!=0x71ca){r.error="Second-function observation is detached from dispatch index 1";return r;}try{millennium_dos_second_function_.emplace(millennium_dos_native_process_->make_second_function_session());session_snapshot_=make_runtime_session_snapshot(*active_,RuntimeSessionKind::millennium_dos_second_function);r.accepted=true;}catch(const std::exception& e){r.error=e.what();}return r;}
#define EON_SECOND_FORWARD(name,type,body) MillenniumDosSecondFunctionObservationResult ReleaseRuntimeCoordinator::name(const type o){MillenniumDosSecondFunctionObservationResult r;if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_second_function||!millennium_dos_second_function_){r.error="No active second-function session";return r;}try{millennium_dos_second_function_->body;r.accepted=true;}catch(const std::exception& e){r.error=e.what();}return r;}
EON_SECOND_FORWARD(observe_millennium_dos_second_function_runtime_byte,MillenniumDosSecondFunctionRuntimeByteObservation,observe_runtime_byte(o.instruction_address,o.runtime_address,o.value))
EON_SECOND_FORWARD(observe_millennium_dos_second_function_call_return,MillenniumDosSecondFunctionCallReturnObservation,observe_call_return(o.call_address,o.return_address))
EON_SECOND_FORWARD(observe_millennium_dos_second_function_bl,MillenniumDosSecondFunctionBlObservation,observe_bl(o.instruction_address,o.value))
#undef EON_SECOND_FORWARD
std::optional<MillenniumDosSecondFunctionCheckpoint> ReleaseRuntimeCoordinator::millennium_dos_second_function_checkpoint()const{if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_second_function||!millennium_dos_second_function_)return std::nullopt;const auto& s=*millennium_dos_second_function_;return MillenniumDosSecondFunctionCheckpoint{s.state(),s.boundary(),s.effects()};}
MillenniumDosSecondFunctionCallbackObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_second_function_callback_entry(const MillenniumDosSecondFunctionCallbackEntryObservation o){MillenniumDosSecondFunctionCallbackObservationResult r;if(!active_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_post_overlay_loop||!millennium_dos_native_process_||!millennium_dos_handler_completion_||millennium_dos_handler_completion_->function_key_index!=1||millennium_dos_handler_completion_->handler_address!=0x71ca||millennium_dos_handler_completion_->terminal_instruction_address!=0x7220||millennium_dos_handler_completion_->return_address!=0xd40d||o.entry_address!=0x7221){r.error="F2 callback entry requires exact prior F2 completion and observed $7221 entry";return r;}try{millennium_dos_second_function_callback_.emplace(millennium_dos_native_process_->make_second_function_callback_session());session_snapshot_=make_runtime_session_snapshot(*active_,RuntimeSessionKind::millennium_dos_second_function_callback);r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
#define EON_F2_CALLBACK_FORWARD(name,type,body) MillenniumDosSecondFunctionCallbackObservationResult ReleaseRuntimeCoordinator::name(const type o){MillenniumDosSecondFunctionCallbackObservationResult r;if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_second_function_callback||!millennium_dos_second_function_callback_){r.error="No active F2 callback session";return r;}try{millennium_dos_second_function_callback_->body;r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
EON_F2_CALLBACK_FORWARD(observe_millennium_dos_second_function_callback_runtime_byte,MillenniumDosSecondFunctionCallbackRuntimeByteObservation,observe_runtime_byte(o.instruction_address,o.runtime_address,o.value))
EON_F2_CALLBACK_FORWARD(observe_millennium_dos_second_function_callback_runtime_word,MillenniumDosSecondFunctionCallbackRuntimeWordObservation,observe_runtime_word(o.instruction_address,o.runtime_address,o.value))
EON_F2_CALLBACK_FORWARD(observe_millennium_dos_second_function_callback_call_return,MillenniumDosSecondFunctionCallbackCallReturnObservation,observe_call_return(o.call_address,o.return_address))
EON_F2_CALLBACK_FORWARD(observe_millennium_dos_second_function_callback_bl,MillenniumDosSecondFunctionCallbackBlObservation,observe_bl(o.instruction_address,o.value))
#undef EON_F2_CALLBACK_FORWARD
MillenniumDosSecondFunctionCallbackObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_second_function_callback_jump_entry(const MillenniumDosSecondFunctionCallbackJumpEntryObservation o){MillenniumDosSecondFunctionCallbackObservationResult r;if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_second_function_callback||!millennium_dos_second_function_callback_||millennium_dos_second_function_callback_transfer_){r.error="No active unentered F2 callback transfer";return r;}const bool tail=millennium_dos_second_function_callback_->boundary().kind==MillenniumDosSecondFunctionCallbackBoundaryKind::external_jump&&millennium_dos_second_function_callback_->boundary().instruction_address==0x7253;MillenniumDosExternalTransferAdmission transfer(tail?MillenniumDosExternalTransferKind::f2_tail_active_return:MillenniumDosExternalTransferKind::f2_reset_wrap_return);const auto admitted=transfer.observe_entry({o.sequence,o.instruction_address,o.target_address});if(!admitted.accepted){r.error=admitted.error;return r;}try{if(tail)millennium_dos_bdf_service_.emplace(millennium_dos_native_process_->make_bdf_service_session());else millennium_dos_second_function_callback_->observe_external_jump_entry(o.instruction_address,o.target_address);millennium_dos_second_function_callback_transfer_=std::move(transfer);r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
MillenniumDosSecondFunctionCallbackObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_second_function_callback_external_return(const MillenniumDosSecondFunctionCallbackExternalReturnObservation o){MillenniumDosSecondFunctionCallbackObservationResult r;if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_second_function_callback||!millennium_dos_second_function_callback_||!millennium_dos_second_function_callback_transfer_||millennium_dos_second_function_callback_->boundary().kind!=MillenniumDosSecondFunctionCallbackBoundaryKind::local_return){r.error="F2 external return requires the active reset-wrap RET boundary";return r;}const auto admitted=millennium_dos_second_function_callback_transfer_->observe_return({o.sequence,o.return_instruction,o.returned_to});r.accepted=admitted.accepted;r.error=admitted.error;return r;}
std::optional<MillenniumDosSecondFunctionCallbackCheckpoint> ReleaseRuntimeCoordinator::millennium_dos_second_function_callback_checkpoint()const{if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_second_function_callback||!millennium_dos_second_function_callback_)return std::nullopt;const auto&s=*millennium_dos_second_function_callback_;return MillenniumDosSecondFunctionCallbackCheckpoint{s.state(),s.boundary(),s.effects(),millennium_dos_second_function_callback_transfer_?std::optional{millennium_dos_second_function_callback_transfer_->checkpoint()}:std::nullopt};}
#define EON_BDF_FORWARD(name,type,body) MillenniumDosBdfObservationResult ReleaseRuntimeCoordinator::name(const type o){MillenniumDosBdfObservationResult r;if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_second_function_callback||!millennium_dos_bdf_service_){r.error="No active $0bdf continuation";return r;}try{millennium_dos_bdf_service_->body;r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
EON_BDF_FORWARD(observe_millennium_dos_bdf_byte,MillenniumDosBdfByteObservation,observe_runtime_byte(o.instruction_address,o.runtime_address,o.value)) EON_BDF_FORWARD(observe_millennium_dos_bdf_word,MillenniumDosBdfWordObservation,observe_runtime_word(o.instruction_address,o.runtime_address,o.value)) EON_BDF_FORWARD(observe_millennium_dos_bdf_poll_return,MillenniumDosBdfPollReturnObservation,observe_poll_return(o.call_address,o.return_address,o.cx,o.dx))
EON_BDF_FORWARD(observe_millennium_dos_bdf_mapping_return,MillenniumDosBdfMappingReturnObservation,observe_mapping_return(o.call_address,o.return_address,o.ax))
EON_BDF_FORWARD(observe_millennium_dos_bdf_far_byte,MillenniumDosBdfFarByteObservation,observe_far_memory_byte(o.instruction_address,o.segment,o.offset,o.value))
#undef EON_BDF_FORWARD
MillenniumDosBdfObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_bdf_external_return(const MillenniumDosBdfExternalReturnObservation o){MillenniumDosBdfObservationResult r;if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_second_function_callback||!millennium_dos_bdf_service_||!millennium_dos_second_function_callback_transfer_||millennium_dos_bdf_service_->boundary().kind!=MillenniumDosBdfServiceBoundaryKind::local_return){r.error="$0bdf external return requires its active local RET boundary";return r;}const auto admitted=millennium_dos_second_function_callback_transfer_->observe_return({o.sequence,o.return_instruction,o.returned_to});r.accepted=admitted.accepted;r.error=admitted.error;return r;}
MillenniumDosBdfObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_bdf_terminal_jump(const MillenniumDosBdfTerminalJumpObservation o){MillenniumDosBdfObservationResult r;if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_second_function_callback||!millennium_dos_bdf_service_||millennium_dos_bdf_terminal_transfer_){r.error="$0bdf terminal jump requires one active unentered boundary";return r;}const auto boundary=millennium_dos_bdf_service_->boundary();MillenniumDosExternalTransferKind kind;if(boundary.kind!=MillenniumDosBdfServiceBoundaryKind::external_jump){r.error="$0bdf terminal jump requires an external-jump boundary";return r;}if(boundary.instruction_address==0x0c4b&&boundary.call_target==0x11f7)kind=MillenniumDosExternalTransferKind::bdf_mode_two_jump;else if(boundary.instruction_address==0x0c4e&&boundary.call_target==0x0caa)kind=MillenniumDosExternalTransferKind::bdf_other_mode_jump;else{r.error="$0bdf terminal jump is not an owned transfer";return r;}if(!o.entry_di||(kind==MillenniumDosExternalTransferKind::bdf_other_mode_jump&&!o.entry_dl)){r.error="BDF terminal entry requires explicit registers";return r;}MillenniumDosExternalTransferAdmission transfer(kind);const auto admitted=transfer.observe_entry({o.sequence,o.instruction_address,o.target_address});if(!admitted.accepted){r.error=admitted.error;return r;}if(kind==MillenniumDosExternalTransferKind::bdf_mode_two_jump)millennium_dos_bdf_mode_two_.emplace(millennium_dos_native_process_->make_bdf_mode_two_session(*o.entry_di));else millennium_dos_bdf_other_mode_.emplace(millennium_dos_native_process_->make_bdf_other_mode_session(*o.entry_dl,*o.entry_di));millennium_dos_bdf_terminal_transfer_=std::move(transfer);r.accepted=true;return r;}
#define EON_BDF_MODE_TWO_FORWARD(name,type,body) MillenniumDosBdfObservationResult ReleaseRuntimeCoordinator::name(const type o){MillenniumDosBdfObservationResult r;if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_second_function_callback||!millennium_dos_bdf_terminal_transfer_||!millennium_dos_bdf_mode_two_){r.error="No active $11f7 continuation";return r;}try{millennium_dos_bdf_mode_two_->body;r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
EON_BDF_MODE_TWO_FORWARD(observe_millennium_dos_bdf_mode_two_byte,MillenniumDosBdfByteObservation,observe_runtime_byte(o.instruction_address,o.runtime_address,o.value))
EON_BDF_MODE_TWO_FORWARD(observe_millennium_dos_bdf_mode_two_word,MillenniumDosBdfWordObservation,observe_runtime_word(o.instruction_address,o.runtime_address,o.value))
EON_BDF_MODE_TWO_FORWARD(observe_millennium_dos_bdf_mode_two_far_word,MillenniumDosBdfModeTwoFarWordObservation,observe_far_word(o.instruction_address,o.segment,o.offset,o.value))
EON_BDF_MODE_TWO_FORWARD(observe_millennium_dos_bdf_mode_two_far_byte,MillenniumDosBdfModeTwoFarByteObservation,observe_far_byte(o.instruction_address,o.segment,o.offset,o.value))
#undef EON_BDF_MODE_TWO_FORWARD
MillenniumDosBdfObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_bdf_mode_two_external_return(
    const MillenniumDosBdfExternalReturnObservation o) {
    MillenniumDosBdfObservationResult result;
    if (!session_snapshot_
        || session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_second_function_callback
        || !millennium_dos_bdf_terminal_transfer_ || !millennium_dos_bdf_mode_two_
        || !native_runtime_memory_
        || millennium_dos_bdf_mode_two_->state()!=MillenniumDosBdfModeTwoState::returned
        || millennium_dos_bdf_mode_two_->boundary().instruction_address!=o.return_instruction) {
        result.error="$11f7 external return requires its active exact RET boundary";
        return result;
    }
    auto next_transfer=*millennium_dos_bdf_terminal_transfer_;
    const auto admitted=next_transfer.observe_return({o.sequence,o.return_instruction,o.returned_to});
    if (!admitted.accepted) { result.error=admitted.error; return result; }

    const auto& session=*millennium_dos_bdf_mode_two_;
    MillenniumDosBdfCheckpoint checkpoint;
    checkpoint.terminal_transfer=next_transfer.checkpoint();
    checkpoint.mode_two=MillenniumDosBdfModeTwoCheckpoint{session.state(),session.boundary(),
        session.far_effects(),session.far_byte_effects(),session.runtime_effects()};
    const auto batch=make_millennium_dos_bdf_effect_batch(checkpoint,
        "millennium-dos-bdf-mode-two-"+std::to_string(
            next_transfer.checkpoint().entry->sequence));
    if (!batch) { result.error="$11f7 returned effects are not fully admitted"; return result; }
    auto next_memory=*native_runtime_memory_;
    const auto applied=next_memory.apply(*batch);
    if (!applied.accepted) { result.error="Runtime-memory application rejected: "+applied.error; return result; }
    *millennium_dos_bdf_terminal_transfer_=std::move(next_transfer);
    *native_runtime_memory_=std::move(next_memory);
    result.accepted=true;
    return result;
}
#define EON_BDF_OTHER_FORWARD(name,type,body) MillenniumDosBdfObservationResult ReleaseRuntimeCoordinator::name(const type o){MillenniumDosBdfObservationResult r;if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_second_function_callback||!millennium_dos_bdf_terminal_transfer_||!millennium_dos_bdf_other_mode_){r.error="No active $0caa continuation";return r;}try{millennium_dos_bdf_other_mode_->body;r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
EON_BDF_OTHER_FORWARD(observe_millennium_dos_bdf_other_mode_byte,MillenniumDosBdfByteObservation,observe_runtime_byte(o.instruction_address,o.runtime_address,o.value))
EON_BDF_OTHER_FORWARD(observe_millennium_dos_bdf_other_mode_word,MillenniumDosBdfWordObservation,observe_runtime_word(o.instruction_address,o.runtime_address,o.value))
EON_BDF_OTHER_FORWARD(observe_millennium_dos_bdf_other_mode_far_word,MillenniumDosBdfModeTwoFarWordObservation,observe_far_word(o.instruction_address,o.segment,o.offset,o.value))
EON_BDF_OTHER_FORWARD(observe_millennium_dos_bdf_other_mode_far_byte,MillenniumDosBdfModeTwoFarByteObservation,observe_far_byte(o.instruction_address,o.segment,o.offset,o.value))
#undef EON_BDF_OTHER_FORWARD
MillenniumDosBdfObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_bdf_other_mode_external_return(const MillenniumDosBdfExternalReturnObservation o){MillenniumDosBdfObservationResult r;if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_second_function_callback||!millennium_dos_bdf_terminal_transfer_||!millennium_dos_bdf_other_mode_||millennium_dos_bdf_other_mode_->state()!=MillenniumDosBdfOtherModeState::returned||millennium_dos_bdf_other_mode_->boundary().instruction_address!=o.return_instruction){r.error="$0caa external return requires its active exact RET boundary";return r;}const auto admitted=millennium_dos_bdf_terminal_transfer_->observe_return({o.sequence,o.return_instruction,o.returned_to});r.accepted=admitted.accepted;r.error=admitted.error;return r;}
std::optional<MillenniumDosBdfCheckpoint>ReleaseRuntimeCoordinator::millennium_dos_bdf_checkpoint()const{if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_second_function_callback||!millennium_dos_bdf_service_||!millennium_dos_second_function_callback_transfer_)return std::nullopt;const auto&s=*millennium_dos_bdf_service_;std::optional<MillenniumDosBdfModeTwoCheckpoint>m;if(millennium_dos_bdf_mode_two_){const auto&x=*millennium_dos_bdf_mode_two_;m=MillenniumDosBdfModeTwoCheckpoint{x.state(),x.boundary(),x.far_effects(),x.far_byte_effects(),x.runtime_effects()};}std::optional<MillenniumDosBdfOtherModeCheckpoint>other;if(millennium_dos_bdf_other_mode_){const auto&x=*millennium_dos_bdf_other_mode_;other=MillenniumDosBdfOtherModeCheckpoint{x.state(),x.boundary(),x.far_effects(),x.far_byte_effects(),x.port_effects(),x.runtime_effects(),x.runtime_byte_effects()};}return MillenniumDosBdfCheckpoint{s.state(),s.boundary(),s.effects(),s.far_memory_effects(),millennium_dos_second_function_callback_transfer_->checkpoint(),millennium_dos_bdf_terminal_transfer_?std::optional{millennium_dos_bdf_terminal_transfer_->checkpoint()}:std::nullopt,m,other};}
std::optional<NativeRuntimeMemoryCheckpoint>
ReleaseRuntimeCoordinator::native_runtime_memory_checkpoint() const {
    if (!active_ || !session_snapshot_ || !native_runtime_memory_) return std::nullopt;
    return native_runtime_memory_->checkpoint();
}
std::optional<NativeRuntimeMemoryDiagnostics>
ReleaseRuntimeCoordinator::native_runtime_memory_diagnostics() const {
    if (!active_ || !session_snapshot_ || !native_runtime_memory_) return std::nullopt;
    return native_runtime_memory_->diagnostics();
}

std::optional<MillenniumDosOwnedFunctionDiagnostics>
ReleaseRuntimeCoordinator::millennium_dos_owned_function_diagnostics() const {
    if (!session_snapshot_) return std::nullopt;
    constexpr std::string_view game_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    MillenniumDosOwnedFunctionDiagnosticInput input{
        .session = *session_snapshot_,
        .game_executable_sha256 = std::string(game_sha256),
        .function_key_index = 0,
        .handler_address = 0,
        .boundary = {},
    };
    const auto assign = [&input](const auto& boundary,
                            const MillenniumDosOwnedFunctionBoundaryKind kind) {
        input.boundary.kind = kind;
        input.boundary.instruction_address = boundary.instruction_address;
        input.boundary.runtime_address = boundary.runtime_address;
        if (boundary.call_target) input.boundary.call_target = *boundary.call_target;
    };
    switch (session_snapshot_->kind) {
    case RuntimeSessionKind::millennium_dos_second_function_callback: {
        if(!millennium_dos_second_function_callback_)return std::nullopt;
        input.function_key_index=1;input.handler_address=0x71ca;
        const auto boundary=millennium_dos_second_function_callback_->boundary();
        auto kind=MillenniumDosOwnedFunctionBoundaryKind::register_value;
        if(boundary.kind==MillenniumDosSecondFunctionCallbackBoundaryKind::runtime_byte)kind=MillenniumDosOwnedFunctionBoundaryKind::runtime_byte;
        else if(boundary.kind==MillenniumDosSecondFunctionCallbackBoundaryKind::runtime_word)kind=MillenniumDosOwnedFunctionBoundaryKind::runtime_word;
        else if(boundary.kind==MillenniumDosSecondFunctionCallbackBoundaryKind::call_return)kind=MillenniumDosOwnedFunctionBoundaryKind::call_return;
        else if(boundary.kind==MillenniumDosSecondFunctionCallbackBoundaryKind::local_return)kind=MillenniumDosOwnedFunctionBoundaryKind::local_return;
        input.boundary.kind=kind;input.boundary.instruction_address=boundary.instruction_address;input.boundary.runtime_address=boundary.runtime_address;if(boundary.target)input.boundary.call_target=*boundary.target;break;
    }
    case RuntimeSessionKind::millennium_dos_second_function: {
        if (!millennium_dos_second_function_) return std::nullopt;
        input.function_key_index=1; input.handler_address=0x71ca;
        const auto boundary=millennium_dos_second_function_->boundary();
        auto kind=MillenniumDosOwnedFunctionBoundaryKind::local_return;
        if(boundary.kind==MillenniumDosSecondFunctionBoundaryKind::runtime_byte)kind=MillenniumDosOwnedFunctionBoundaryKind::runtime_byte;
        else if(boundary.kind==MillenniumDosSecondFunctionBoundaryKind::call_return)kind=MillenniumDosOwnedFunctionBoundaryKind::call_return;
        else if(boundary.kind==MillenniumDosSecondFunctionBoundaryKind::register_bl)kind=MillenniumDosOwnedFunctionBoundaryKind::register_value;
        assign(boundary,kind);break;
    }
    case RuntimeSessionKind::millennium_dos_first_function: {
        if (!millennium_dos_first_function_) return std::nullopt;
        input.function_key_index = 0; input.handler_address = 0x6f9a;
        const auto boundary = millennium_dos_first_function_->boundary();
        input.boundary.kind = boundary.kind == MillenniumDosFirstFunctionBoundaryKind::call_return
            ? MillenniumDosOwnedFunctionBoundaryKind::call_return
            : boundary.kind == MillenniumDosFirstFunctionBoundaryKind::register_bl
                ? MillenniumDosOwnedFunctionBoundaryKind::register_value
                : MillenniumDosOwnedFunctionBoundaryKind::local_return;
        input.boundary.instruction_address = boundary.instruction_address;
        if (boundary.call_target) input.boundary.call_target = *boundary.call_target;
        break;
    }
    case RuntimeSessionKind::millennium_dos_third_function: {
        if (!millennium_dos_third_function_) return std::nullopt;
        input.function_key_index = 2; input.handler_address = 0x6faa;
        const auto boundary = millennium_dos_third_function_->boundary();
        auto kind = MillenniumDosOwnedFunctionBoundaryKind::local_return;
        if (boundary.kind == MillenniumDosThirdFunctionBoundaryKind::runtime_word)
            kind = MillenniumDosOwnedFunctionBoundaryKind::runtime_word;
        else if (boundary.kind == MillenniumDosThirdFunctionBoundaryKind::call_return)
            kind = MillenniumDosOwnedFunctionBoundaryKind::call_return;
        else if (boundary.kind == MillenniumDosThirdFunctionBoundaryKind::register_bl
            || boundary.kind == MillenniumDosThirdFunctionBoundaryKind::far_pointer)
            kind = MillenniumDosOwnedFunctionBoundaryKind::register_value;
        assign(boundary, kind); break;
    }
    case RuntimeSessionKind::millennium_dos_fifth_function: {
        if (!millennium_dos_fifth_function_) return std::nullopt;
        input.function_key_index = 4; input.handler_address = 0x7597;
        const auto boundary = millennium_dos_fifth_function_->boundary();
        input.boundary.kind = boundary.kind == MillenniumDosFifthFunctionBoundaryKind::call_return
            ? MillenniumDosOwnedFunctionBoundaryKind::call_return
            : MillenniumDosOwnedFunctionBoundaryKind::local_return;
        input.boundary.instruction_address = boundary.instruction_address;
        if (boundary.call_target) input.boundary.call_target = *boundary.call_target;
        break;
    }
    case RuntimeSessionKind::millennium_dos_fourth_function: {
        if (!millennium_dos_fourth_function_) return std::nullopt;
        input.function_key_index = 3; input.handler_address = 0x72f9;
        const auto boundary = millennium_dos_fourth_function_->boundary();
        auto kind = MillenniumDosOwnedFunctionBoundaryKind::local_return;
        if (boundary.kind == MillenniumDosFourthFunctionBoundaryKind::runtime_word)
            kind = MillenniumDosOwnedFunctionBoundaryKind::runtime_word;
        else if (boundary.kind == MillenniumDosFourthFunctionBoundaryKind::call_return)
            kind = MillenniumDosOwnedFunctionBoundaryKind::call_return;
        assign(boundary, kind); break;
    }
    case RuntimeSessionKind::millennium_dos_sixth_function: {
        if (!millennium_dos_sixth_function_) return std::nullopt;
        input.function_key_index = 5; input.handler_address = 0x7415;
        const auto boundary = millennium_dos_sixth_function_->boundary();
        MillenniumDosOwnedFunctionBoundaryKind kind;
        switch (boundary.kind) {
        case MillenniumDosSixthFunctionBoundaryKind::runtime_word: kind=MillenniumDosOwnedFunctionBoundaryKind::runtime_word; break;
        case MillenniumDosSixthFunctionBoundaryKind::runtime_byte: kind=MillenniumDosOwnedFunctionBoundaryKind::runtime_byte; break;
        case MillenniumDosSixthFunctionBoundaryKind::call_return: kind=MillenniumDosOwnedFunctionBoundaryKind::call_return; break;
        case MillenniumDosSixthFunctionBoundaryKind::register_bl: kind=MillenniumDosOwnedFunctionBoundaryKind::register_value; break;
        case MillenniumDosSixthFunctionBoundaryKind::local_return: kind=MillenniumDosOwnedFunctionBoundaryKind::local_return; break;
        }
        assign(boundary, kind); break;
    }
    case RuntimeSessionKind::millennium_dos_seventh_function: {
        if (!millennium_dos_seventh_function_) return std::nullopt;
        input.function_key_index = 6; input.handler_address = 0x7521;
        const auto boundary = millennium_dos_seventh_function_->boundary();
        auto kind = MillenniumDosOwnedFunctionBoundaryKind::register_value;
        if (boundary.kind == MillenniumDosSeventhFunctionBoundaryKind::runtime_word) kind=MillenniumDosOwnedFunctionBoundaryKind::runtime_word;
        else if (boundary.kind == MillenniumDosSeventhFunctionBoundaryKind::runtime_byte) kind=MillenniumDosOwnedFunctionBoundaryKind::runtime_byte;
        else if (boundary.kind == MillenniumDosSeventhFunctionBoundaryKind::call_return) kind=MillenniumDosOwnedFunctionBoundaryKind::call_return;
        else if (boundary.kind == MillenniumDosSeventhFunctionBoundaryKind::local_return) kind=MillenniumDosOwnedFunctionBoundaryKind::local_return;
        assign(boundary, kind); break;
    }
    case RuntimeSessionKind::millennium_dos_eighth_function: {
        if (!millennium_dos_eighth_function_) return std::nullopt;
        input.function_key_index = 7; input.handler_address = 0x7306;
        const auto boundary = millennium_dos_eighth_function_->boundary();
        const auto kind = boundary.kind == MillenniumDosEighthFunctionBoundaryKind::call_return
            ? MillenniumDosOwnedFunctionBoundaryKind::call_return
            : boundary.kind == MillenniumDosEighthFunctionBoundaryKind::local_return
                ? MillenniumDosOwnedFunctionBoundaryKind::local_return
                : MillenniumDosOwnedFunctionBoundaryKind::register_value;
        input.boundary.kind=kind; input.boundary.instruction_address=boundary.instruction_address;
        if(boundary.call_target) input.boundary.call_target=*boundary.call_target;
        break;
    }
    case RuntimeSessionKind::millennium_dos_ninth_function: {
        if (!millennium_dos_ninth_function_) return std::nullopt;
        input.function_key_index = 8; input.handler_address = 0x7339;
        const auto boundary = millennium_dos_ninth_function_->boundary();
        auto kind=MillenniumDosOwnedFunctionBoundaryKind::local_return;
        if(boundary.kind==MillenniumDosNinthFunctionBoundaryKind::runtime_word) kind=MillenniumDosOwnedFunctionBoundaryKind::runtime_word;
        else if(boundary.kind==MillenniumDosNinthFunctionBoundaryKind::runtime_byte) kind=MillenniumDosOwnedFunctionBoundaryKind::runtime_byte;
        else if(boundary.kind==MillenniumDosNinthFunctionBoundaryKind::call_return) kind=MillenniumDosOwnedFunctionBoundaryKind::call_return;
        assign(boundary,kind); break;
    }
    case RuntimeSessionKind::millennium_dos_ninth_function_handoff: {
        if (!millennium_dos_ninth_handoff_) return std::nullopt;
        input.function_key_index = 8;
        input.handler_address = 0x7339;
        const auto boundary = millennium_dos_ninth_handoff_->boundary();
        auto kind = MillenniumDosOwnedFunctionBoundaryKind::register_value;
        if (boundary.kind == MillenniumDosNinthHandoffBoundaryKind::runtime_byte) {
            kind = MillenniumDosOwnedFunctionBoundaryKind::runtime_byte;
        } else if (boundary.kind == MillenniumDosNinthHandoffBoundaryKind::runtime_word) {
            kind = MillenniumDosOwnedFunctionBoundaryKind::runtime_word;
        } else if (boundary.kind == MillenniumDosNinthHandoffBoundaryKind::call_return) {
            kind = MillenniumDosOwnedFunctionBoundaryKind::call_return;
        } else if (boundary.kind == MillenniumDosNinthHandoffBoundaryKind::local_return) {
            kind = MillenniumDosOwnedFunctionBoundaryKind::local_return;
        }
        input.boundary.kind = kind;
        input.boundary.instruction_address = boundary.instruction_address;
        input.boundary.runtime_address = boundary.runtime_address;
        if (boundary.call_target) input.boundary.call_target = *boundary.call_target;
        break;
    }
    case RuntimeSessionKind::millennium_dos_tenth_function: {
        if (!millennium_dos_tenth_function_) return std::nullopt;
        input.function_key_index=9; input.handler_address=0x7384;
        const auto boundary=millennium_dos_tenth_function_->boundary();
        auto kind=MillenniumDosOwnedFunctionBoundaryKind::register_value;
        if(boundary.kind==MillenniumDosTenthFunctionBoundaryKind::runtime_word) kind=MillenniumDosOwnedFunctionBoundaryKind::runtime_word;
        else if(boundary.kind==MillenniumDosTenthFunctionBoundaryKind::runtime_byte) kind=MillenniumDosOwnedFunctionBoundaryKind::runtime_byte;
        else if(boundary.kind==MillenniumDosTenthFunctionBoundaryKind::call_return) kind=MillenniumDosOwnedFunctionBoundaryKind::call_return;
        else if(boundary.kind==MillenniumDosTenthFunctionBoundaryKind::local_return) kind=MillenniumDosOwnedFunctionBoundaryKind::local_return;
        assign(boundary,kind); break;
    }
    default: return std::nullopt;
    }
    return make_millennium_dos_owned_function_diagnostics(input);
}

RuntimeInputDisposition ReleaseRuntimeCoordinator::observe_input(
    const RuntimeInputObservation& observation) {
    // Do not accept a generic input event just because a session is active.
    // First gate its value shape through the immutable session contract. This
    // makes the contract used by the CLI/F10 diagnostic authoritative here,
    // while the branches below retain their stricter release/session checks.
    if (!session_snapshot_ || session_snapshot_->input_contract
            != runtime_input_contract_for_session(session_snapshot_->kind)
        || !runtime_input_contract_accepts_observation(session_snapshot_->input_contract,
            observation.kind)) {
        return RuntimeInputDisposition::rejected;
    }
    switch (session_snapshot_->input_contract) {
    case RuntimeInputContract::deuteros_amiga_opening_held_signal:
        if (session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_opening) {
            return RuntimeInputDisposition::rejected;
        }
        deuteros_amiga_opening_input_held_ = observation.ascii_character != '\0';
        return RuntimeInputDisposition::observed;
    case RuntimeInputContract::millennium_dos_startup_observation:
        break;
    case RuntimeInputContract::none:
        return RuntimeInputDisposition::rejected;
    }
    if (session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title) {
        return RuntimeInputDisposition::rejected;
    }
    if (observation.kind == RuntimeInputObservationKind::ascii_character) {
        if (!millennium_dos_sound_selection_) return RuntimeInputDisposition::rejected;
        if (!millennium_dos_sound_selection_->accept_ascii_character(observation.ascii_character)) {
            return RuntimeInputDisposition::ignored;
        }
        if (!active_) return RuntimeInputDisposition::rejected;
        session_snapshot_ = make_runtime_session_snapshot(*active_,
            RuntimeSessionKind::millennium_dos_sound_driver_boundary);
        return RuntimeInputDisposition::boundary_reached;
    }
    if (!millennium_dos_title_) return RuntimeInputDisposition::rejected;
    if (!millennium_dos_title_->poll_console(true)) return RuntimeInputDisposition::ignored;
    if (!active_) return RuntimeInputDisposition::rejected;
    if (active_->release.language == "en"
        && !prepare_millennium_dos_title_to_game_after_handoff()) {
        return RuntimeInputDisposition::rejected;
    }
    session_snapshot_ = make_runtime_session_snapshot(*active_,
        RuntimeSessionKind::millennium_dos_title_handoff_boundary);
    return RuntimeInputDisposition::boundary_reached;
}

bool ReleaseRuntimeCoordinator::prepare_millennium_dos_title_to_game_after_handoff() {
    constexpr std::string_view english_release =
        "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123";
    if (!active_ || !millennium_dos_title_ || !millennium_dos_title_->handed_off()
        || active_->release.game != Game::millennium
        || active_->release.platform != Platform::dos
        || active_->release.language != "en"
        || active_->release.sha256 != english_release
        || millennium_dos_title_to_game_) return false;
    try {
        const auto media = VerifiedReleaseMedia::open(active_->release);
        const auto launcher = admit_native_code_image(media,
            "millennium-dos-mill-com-linear", "millennium-dos-launcher");
        const auto titles = admit_native_code_image(media,
            "millennium-dos-titles-exe-linear", "millennium-dos-title-flow");
        if (!launcher.accepted() || !titles.accepted()) return false;
        MillenniumDosTitleToGameSession prepared(launcher.view->bytes, titles.view->bytes);
        millennium_dos_title_to_game_ = std::move(prepared);
        ++millennium_dos_title_to_game_generation_;
        millennium_dos_title_to_game_last_sequence_ = 0;
        return true;
    } catch (...) {
        return false;
    }
}

std::optional<DeuterosAmigaVmEvents> ReleaseRuntimeCoordinator::tick_deuteros_amiga_opening() {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_opening
        || !deuteros_amiga_ || deuteros_amiga_->title_handed_off()) return std::nullopt;
    auto events = deuteros_amiga_->tick(deuteros_amiga_opening_input_held_);
    if (deuteros_amiga_paula_) {
        for (const auto& sound : events.sounds) {
            static_cast<void>(deuteros_amiga_paula_->submit(sound));
        }
    }
    if (events.title_handoff) {
        // The opening object remains owner of the original ADF/title-stage
        // evidence, but the live session has crossed its last recovered VBL
        // frame. Publish the narrower state atomically so diagnostics and
        // input routing cannot describe or tick a completed opening.
        if (!active_ || !deuteros_amiga_->title_stage_session()) return std::nullopt;
        session_snapshot_ = make_runtime_session_snapshot(*active_,
            RuntimeSessionKind::deuteros_amiga_title_stage);
        deuteros_amiga_opening_input_held_ = false;
        // The title stage has no admitted audio capability.  Drop every
        // opening DMA channel before its boundary becomes visible to SDL.
        deuteros_amiga_paula_.reset();
    }
    return events;
}

std::optional<std::vector<float>>
ReleaseRuntimeCoordinator::render_deuteros_amiga_opening_audio(const std::size_t frames) {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_opening
        || !deuteros_amiga_paula_ || !deuteros_amiga_paula_->has_active_channels()) {
        return std::nullopt;
    }
    return deuteros_amiga_paula_->render(frames);
}

std::optional<DeuterosAmigaOpeningCheckpoint>
ReleaseRuntimeCoordinator::deuteros_amiga_opening_checkpoint() const {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_opening
        || !deuteros_amiga_) return std::nullopt;
    return deuteros_amiga_->checkpoint();
}

std::optional<DeuterosAmigaOpeningPresentationSnapshot>
ReleaseRuntimeCoordinator::deuteros_amiga_opening_presentation() const {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_opening
        || !deuteros_amiga_) return std::nullopt;
    const auto checkpoint = deuteros_amiga_->checkpoint();
    if (!checkpoint) return std::nullopt;
    return DeuterosAmigaOpeningPresentationSnapshot{
        *checkpoint,
        deuteros_amiga_->palette_index(),
        deuteros_amiga_->active_channel_count(),
        deuteros_amiga_->frame_composed_on_last_tick(),
        deuteros_amiga_->rgba_frame(),
    };
}

std::optional<DeuterosAmigaTitleStageBoundarySnapshot>
ReleaseRuntimeCoordinator::deuteros_amiga_title_stage_boundary() const {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !deuteros_amiga_->title_stage_session()) return std::nullopt;
    const auto& title_stage = *deuteros_amiga_->title_stage_session();
    return DeuterosAmigaTitleStageBoundarySnapshot{
        title_stage.stage(),
        title_stage.original_sha256(),
        title_stage.entry_prefix_state(),
        title_stage.exec_prelude(),
        title_stage.local_prefix_executed(),
        title_stage.graphics_setup_palette_evidence(),
        deuteros_amiga_->alternate_renderer_trace(),
    };
}

std::optional<DeuterosAmigaTitleDependencyChainCheckpoint>
ReleaseRuntimeCoordinator::deuteros_amiga_title_dependency_chain_checkpoint() const {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !deuteros_amiga_->title_stage_session()) return std::nullopt;
    const auto& title = *deuteros_amiga_->title_stage_session();
    DeuterosAmigaTitleDependencyChainCheckpoint result;
    result.title_stage_sha256 = title.original_sha256();
    result.exec = title.exec_boundary();
    if (const auto* open = title.open_library_boundary()) result.open_library = *open;
    if (const auto* custom = title.custom_chip_boundary()) {
        result.custom_chip_boundary_present = true;
        result.observed_custom_chip_write_count = custom->observed_write_count();
        result.custom_chip_complete = custom->complete();
        result.callback_exec_return_observed = custom->observed_exec_return().has_value();
        result.stop_before_address = custom->stop_before_address();
    } else if (result.open_library) {
        result.stop_before_address = result.open_library->stop_before_address;
    } else {
        result.stop_before_address = result.exec.stop_before_address;
    }
    result.service_setup_boundary_armed = result.callback_exec_return_observed;
    result.service_setup_local_plan = deuteros_amiga_title_service_setup_plan_;
    result.second_service_local_plan = deuteros_amiga_title_second_service_plan_;
    result.third_service_local_plan = deuteros_amiga_title_third_service_plan_;
    result.fourth_service_local_plan = deuteros_amiga_title_fourth_service_plan_;
    result.fifth_service_local_plan = deuteros_amiga_title_fifth_service_plan_;
    if (result.fifth_service_local_plan) result.stop_before_address=result.fifth_service_local_plan->stop_before_address;
    else if (result.fourth_service_local_plan) result.stop_before_address=result.fourth_service_local_plan->stop_before_address;
    else if (result.third_service_local_plan) {
        result.stop_before_address = result.third_service_local_plan->stop_before_address;
    } else if (result.second_service_local_plan) {
        result.stop_before_address = result.second_service_local_plan->stop_before_address;
    } else if (result.service_setup_local_plan) {
        result.stop_before_address = result.service_setup_local_plan->stop_before_address;
    } else if (result.service_setup_boundary_armed) {
        result.stop_before_address = 0x206d4;
    }
    return result;
}

#define EON_DEUTEROS_TITLE_ADVANCE(name, expression) \
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::name { \
    DeuterosAmigaTitleDependencyObservationResult result; \
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage || !deuteros_amiga_) { result.error="Deuteros title observation requires the active title stage"; return result; } \
    try { if (!(expression)) { result.error="Deuteros title observation did not match the next owned boundary"; return result; } result.accepted=true; } \
    catch(const std::exception& e) { result.error=std::string("Deuteros title observation rejected: ")+e.what(); } return result; }
EON_DEUTEROS_TITLE_ADVANCE(advance_deuteros_amiga_title_local_prefix(), deuteros_amiga_->advance_title_local_prefix())
EON_DEUTEROS_TITLE_ADVANCE(observe_deuteros_amiga_title_exec_return(const DeuterosAmigaObservedExecReturn o), deuteros_amiga_->observe_title_exec_return(o))
EON_DEUTEROS_TITLE_ADVANCE(observe_deuteros_amiga_title_open_library_return(const DeuterosAmigaObservedOpenLibraryReturn o), deuteros_amiga_->observe_title_open_library_return(o))
EON_DEUTEROS_TITLE_ADVANCE(advance_deuteros_amiga_title_post_open_library_local_path(), deuteros_amiga_->advance_title_post_open_library_local_path())
EON_DEUTEROS_TITLE_ADVANCE(observe_deuteros_amiga_title_display_base(const DeuterosAmigaObservedDisplayBaseRead o), deuteros_amiga_->observe_title_display_base(o))
EON_DEUTEROS_TITLE_ADVANCE(observe_deuteros_amiga_title_callback_exec_return(const DeuterosAmigaObservedCallbackExecReturn o), deuteros_amiga_->observe_title_callback_exec_return(o))
#undef EON_DEUTEROS_TITLE_ADVANCE

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_service_setup_exec_return(
    const DeuterosAmigaObservedServiceSetupExecReturn observation) {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || deuteros_amiga_title_service_setup_plan_) {
        result.error = "Deuteros service-setup observation requires the active title-stage boundary";
        return result;
    }
    try {
        auto plan = deuteros_amiga_->observe_title_service_setup_exec_return(observation);
        if (!plan) { result.error = "Deuteros service-setup observation did not match the next owned boundary"; return result; }
        deuteros_amiga_title_service_setup_plan_ = std::move(*plan);
        result.accepted = true;
    } catch (const std::exception& e) { result.error = std::string("Deuteros service-setup observation rejected: ") + e.what(); }
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_second_service_exec_return(
    const DeuterosAmigaObservedServiceSetupExecReturn observation) {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !deuteros_amiga_title_service_setup_plan_
        || deuteros_amiga_title_second_service_plan_) {
        result.error = "Deuteros second-service observation requires the active service-setup boundary";
        return result;
    }
    try {
        auto plan = deuteros_amiga_->observe_title_second_service_exec_return(observation);
        if (!plan) { result.error = "Deuteros second-service observation did not match the next owned boundary"; return result; }
        deuteros_amiga_title_second_service_plan_ = std::move(*plan);
        result.accepted = true;
    } catch (const std::exception& e) { result.error = std::string("Deuteros second-service observation rejected: ") + e.what(); }
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_third_service_exec_return(
    const DeuterosAmigaObservedServiceSetupExecReturn observation) {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !deuteros_amiga_title_second_service_plan_
        || deuteros_amiga_title_third_service_plan_) {
        result.error = "Deuteros third-service observation requires the active second-service boundary";
        return result;
    }
    try {
        auto plan = deuteros_amiga_->observe_title_third_service_exec_return(observation);
        if (!plan) { result.error = "Deuteros third-service observation did not match the next owned boundary"; return result; }
        deuteros_amiga_title_third_service_plan_ = std::move(*plan);
        result.accepted = true;
    } catch (const std::exception& e) { result.error = std::string("Deuteros third-service observation rejected: ") + e.what(); }
    return result;
}

#define EON_DEUTEROS_LATE_SERVICE(method, prior, stored, opening_method, label) \
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::method(const DeuterosAmigaObservedServiceSetupExecReturn observation){DeuterosAmigaTitleDependencyObservationResult result;if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::deuteros_amiga_title_stage||!deuteros_amiga_||!prior||stored){result.error="Deuteros " label " observation requires its active preceding boundary";return result;}try{auto plan=deuteros_amiga_->opening_method(observation);if(!plan){result.error="Deuteros " label " observation did not match the next owned boundary";return result;}stored=std::move(*plan);result.accepted=true;}catch(const std::exception&e){result.error=std::string("Deuteros " label " observation rejected: ")+e.what();}return result;}
EON_DEUTEROS_LATE_SERVICE(observe_deuteros_amiga_title_fourth_service_exec_return,deuteros_amiga_title_third_service_plan_,deuteros_amiga_title_fourth_service_plan_,observe_title_fourth_service_exec_return,"fourth-service")
EON_DEUTEROS_LATE_SERVICE(observe_deuteros_amiga_title_fifth_service_exec_return,deuteros_amiga_title_fourth_service_plan_,deuteros_amiga_title_fifth_service_plan_,observe_title_fifth_service_exec_return,"fifth-service")
#undef EON_DEUTEROS_LATE_SERVICE

#define EON_DEUTEROS_BATCH_FORWARD(method, opening_method, type) \
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::method(const type observation){DeuterosAmigaTitleDependencyObservationResult result;if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::deuteros_amiga_title_stage||!deuteros_amiga_||!deuteros_amiga_title_fifth_service_plan_){result.error="Deuteros service-batch observation requires the active fifth-service boundary";return result;}try{if(!deuteros_amiga_->opening_method(observation)){result.error="Deuteros service-batch observation did not match the next owned boundary";return result;}result.accepted=true;}catch(const std::exception&e){result.error=std::string("Deuteros service-batch observation rejected: ")+e.what();}return result;}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_title_controller_pointer_seed(){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::deuteros_amiga_title_stage||!deuteros_amiga_||!deuteros_amiga_title_fifth_service_plan_){result.error="Deuteros controller seed requires the active fifth-service boundary";return result;}
    try{if(!deuteros_amiga_->advance_title_controller_pointer_seed()){result.error="Deuteros controller seed did not match the next owned boundary";return result;}result.accepted=true;}catch(const std::exception&e){result.error=std::string("Deuteros controller seed rejected: ")+e.what();}return result;
}
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_service_batch_graphics_return,observe_title_service_batch_graphics_return,DeuterosAmigaObservedGraphicsVectorReturn)
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_service_batch_runtime_word,observe_title_service_batch_runtime_word,DeuterosAmigaObservedServiceWordRead)
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_graphics_service_first_return,observe_title_graphics_service_first_return,DeuterosAmigaObservedGraphicsVectorReturn)
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_graphics_service_second_return,observe_title_graphics_service_second_return,DeuterosAmigaObservedGraphicsVectorReturn)
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_graphics_service_third_return,observe_title_graphics_service_third_return,DeuterosAmigaObservedGraphicsVectorReturn)
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_tail_first_graphics_return,observe_title_tail_first_graphics_return,DeuterosAmigaObservedGraphicsVectorReturn)
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_tail_copy_words,observe_title_tail_copy_words,DeuterosAmigaObservedTailCopyWords)
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_tail_selection_words,observe_title_tail_selection_words,DeuterosAmigaObservedTailSelectionWords)
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_tail_second_graphics_return,observe_title_tail_second_graphics_return,DeuterosAmigaObservedGraphicsVectorReturn)
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_tail_repeated_selection_words,observe_title_tail_repeated_selection_words,DeuterosAmigaObservedTailSelectionWords)
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_tail_repeated_graphics_return,observe_title_tail_repeated_graphics_return,DeuterosAmigaObservedGraphicsVectorReturn)
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_tail_repeated_wrapper_graphics_return,observe_title_tail_repeated_wrapper_graphics_return,DeuterosAmigaObservedGraphicsVectorReturn)
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_tail_source_table,observe_title_tail_source_table,DeuterosAmigaObservedTailSourceTable)
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_tail_exec_return,observe_title_tail_exec_return,DeuterosAmigaObservedTailExecReturn)
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_load_service_return,observe_title_load_service_return,DeuterosAmigaObservedLocalCallReturn)
#undef EON_DEUTEROS_BATCH_FORWARD

DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_load_selector(const DeuterosAmigaObservedLoadSelector observation){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::deuteros_amiga_title_stage||!deuteros_amiga_||deuteros_amiga_title_load_copy_){result.error="Deuteros load selector requires its active service boundary";return result;}
    try{auto plan=deuteros_amiga_->observe_title_load_selector(observation);if(!plan){result.error="Deuteros load selector did not match the next owned boundary";return result;}if(plan->outcome==DeuterosAmigaTitleLoadServiceOutcome::copy_boundary){deuteros_amiga_title_load_copy_.emplace(BoundedMemoryTransferContract{0x38a28,plan->copy_source_address,plan->copy_destination_address,4,4,plan->copy_longword_count,256,MemoryTransferElementWidth::longword,0x1000000});++deuteros_amiga_title_load_copy_generation_;}result.accepted=true;}catch(const std::exception&e){result.error=std::string("Deuteros load selector rejected: ")+e.what();}return result;
}

DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_load_copy_chunk(const DeuterosAmigaObservedLoadCopyChunk observation){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::deuteros_amiga_title_stage||!deuteros_amiga_||!deuteros_amiga_title_load_copy_||!native_runtime_memory_){result.error="Deuteros load-copy chunk requires its active owned copy boundary";return result;}
    try{
        auto next_transfer=*deuteros_amiga_title_load_copy_;
        const auto admitted=next_transfer.observe_chunk({observation.trace_sequence,observation.instruction_address,observation.first_longword_index,observation.source_address,observation.destination_address,observation.observed_longwords});
        if(!admitted.accepted){result.error=admitted.error;return result;}
        auto next_memory=*native_runtime_memory_;
        if(next_transfer.checkpoint().complete){const auto batch=make_bounded_memory_transfer_batch(next_transfer.checkpoint(),"deuteros-amiga-title-load-copy-"+std::to_string(deuteros_amiga_title_load_copy_generation_));if(!batch){result.error="Completed Deuteros load copy did not produce an admitted batch";return result;}const auto applied=next_memory.apply(*batch);if(!applied.accepted){result.error="Runtime-memory application rejected: "+applied.error;return result;}}
        if(!deuteros_amiga_->observe_title_load_copy_chunk(observation)){result.error="Deuteros load-copy chunk did not match the next owned boundary";return result;}
        *deuteros_amiga_title_load_copy_=std::move(next_transfer);
        *native_runtime_memory_=std::move(next_memory);
        result.accepted=true;
    }catch(const std::exception&e){result.error=std::string("Deuteros load-copy chunk rejected: ")+e.what();}
    return result;
}

DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_load_dispatch_table_base(const DeuterosAmigaObservedLoadDispatchTableBase observation){
    DeuterosAmigaTitleDependencyObservationResult result;if(!deuteros_amiga_||!deuteros_amiga_title_load_copy_||!deuteros_amiga_title_load_copy_->checkpoint().complete){result.error="Deuteros dispatch base requires the completed owned load copy";return result;}try{if(!deuteros_amiga_->observe_title_load_dispatch_table_base(observation)){result.error="Deuteros dispatch base did not match the next owned boundary";return result;}result.accepted=true;}catch(const std::exception&e){result.error=std::string("Deuteros dispatch base rejected: ")+e.what();}return result;
}

DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_load_dispatch_table_word(const DeuterosAmigaObservedLoadDispatchTableWord observation){
    DeuterosAmigaTitleDependencyObservationResult result;if(!deuteros_amiga_||!native_runtime_memory_){result.error="Deuteros dispatch word requires the active owned title runtime";return result;}try{auto plan=deuteros_amiga_->observe_title_load_dispatch_table_word(observation);if(!plan){result.error="Deuteros dispatch word did not match the next owned boundary";return result;}NativeRuntimeEffectBatch batch{"deuteros-amiga-title-dispatch-"+std::to_string(deuteros_amiga_title_load_copy_generation_),true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->byte_write_address},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,plan->byte_write_value}}};auto next=*native_runtime_memory_;const auto applied=next.apply(batch);if(!applied.accepted){result.error="Runtime-memory application rejected: "+applied.error;return result;}*native_runtime_memory_=std::move(next);result.accepted=true;}catch(const std::exception&e){result.error=std::string("Deuteros dispatch word rejected: ")+e.what();}return result;
}

DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_command_opcode(const DeuterosAmigaObservedTitleCommandOpcode observation){
    DeuterosAmigaTitleDependencyObservationResult result;if(!deuteros_amiga_){result.error="Deuteros command opcode requires the active owned title runtime";return result;}try{if(!deuteros_amiga_->observe_title_command_opcode(observation)){result.error="Deuteros command opcode did not match the next owned boundary";return result;}++deuteros_amiga_title_command_generation_;result.accepted=true;}catch(const std::exception&e){result.error=std::string("Deuteros command opcode rejected: ")+e.what();}return result;
}

#define EON_DEUTEROS_COMMAND_WRITE(method, opening_method, type, width_expr, effects_expr) \
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::method(const type observation){DeuterosAmigaTitleDependencyObservationResult result;if(!deuteros_amiga_||!native_runtime_memory_||deuteros_amiga_title_command_generation_==0){result.error="Deuteros command write requires an active owned command generation";return result;}try{auto plan=deuteros_amiga_->opening_method(observation);if(!plan){result.error="Deuteros command write did not match its owned boundary";return result;}NativeRuntimeEffectBatch batch{"deuteros-amiga-title-command-"+std::to_string(deuteros_amiga_title_command_generation_),true,effects_expr};auto next=*native_runtime_memory_;const auto applied=next.apply(batch);if(!applied.accepted){result.error="Runtime-memory application rejected: "+applied.error;return result;}*native_runtime_memory_=std::move(next);result.accepted=true;}catch(const std::exception&e){result.error=std::string("Deuteros command write rejected: ")+e.what();}return result;}
EON_DEUTEROS_COMMAND_WRITE(observe_deuteros_amiga_title_command_operand_byte,observe_title_command_operand_byte,DeuterosAmigaObservedTitleCommandOperandByte,MemoryTransferElementWidth::longword,(std::vector<NativeRuntimeWriteEffect>{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->destination_address},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,plan->destination_value}}))
EON_DEUTEROS_COMMAND_WRITE(observe_deuteros_amiga_title_command_pointer_long,observe_title_command_pointer_long,DeuterosAmigaObservedTitleCommandPointerLong,MemoryTransferElementWidth::longword,(std::vector<NativeRuntimeWriteEffect>{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->destination_address},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,plan->destination_value}}))
#undef EON_DEUTEROS_COMMAND_WRITE

DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_command_eight_pointer(const DeuterosAmigaObservedTitleCommandEightPointer observation){DeuterosAmigaTitleDependencyObservationResult result;if(!deuteros_amiga_){result.error="Deuteros command-eight pointer requires active title runtime";return result;}try{if(!deuteros_amiga_->observe_title_command_eight_pointer(observation)){result.error="Deuteros command-eight pointer did not match boundary";return result;}result.accepted=true;}catch(const std::exception&e){result.error=e.what();}return result;}

#define EON_DEUTEROS_COMMAND_EIGHT_WRITE(method, opening_method, type) \
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::method(const type observation){DeuterosAmigaTitleDependencyObservationResult result;if(!deuteros_amiga_||!native_runtime_memory_||deuteros_amiga_title_command_generation_==0){result.error="Deuteros command-eight write requires active generation";return result;}try{auto plan=deuteros_amiga_->opening_method(observation);if(!plan){result.error="Deuteros command-eight write did not match boundary";return result;}if(plan->destination_addresses[0]!=0){NativeRuntimeEffectBatch batch{"deuteros-amiga-title-command-"+std::to_string(deuteros_amiga_title_command_generation_),true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->destination_addresses[0]},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,plan->destination_value},{2,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->destination_addresses[1]},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,plan->destination_value}}};auto next=*native_runtime_memory_;const auto applied=next.apply(batch);if(!applied.accepted){result.error="Runtime-memory application rejected: "+applied.error;return result;}*native_runtime_memory_=std::move(next);}result.accepted=true;}catch(const std::exception&e){result.error=std::string("Deuteros command-eight write rejected: ")+e.what();}return result;}
EON_DEUTEROS_COMMAND_EIGHT_WRITE(observe_deuteros_amiga_title_command_eight_mode,observe_title_command_eight_mode,DeuterosAmigaObservedTitleCommandEightMode)
EON_DEUTEROS_COMMAND_EIGHT_WRITE(observe_deuteros_amiga_title_command_eight_scale,observe_title_command_eight_scale,DeuterosAmigaObservedTitleCommandEightScale)
#undef EON_DEUTEROS_COMMAND_EIGHT_WRITE

DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_command_call_return(const DeuterosAmigaObservedTitleCommandCallReturn observation){DeuterosAmigaTitleDependencyObservationResult result;if(!deuteros_amiga_){result.error="Deuteros command call return requires active title runtime";return result;}try{if(!deuteros_amiga_->observe_title_command_call_return(observation)){result.error="Deuteros command call return did not match boundary";return result;}result.accepted=true;}catch(const std::exception&e){result.error=e.what();}return result;}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_command_planar_write(
    const DeuterosAmigaObservedTitleCommandPlanarWrite observation) {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !native_runtime_memory_
        || deuteros_amiga_title_command_generation_ == 0) {
        result.error = "Deuteros planar command requires an active owned command generation";
        return result;
    }
    try {
        // Validate and derive all effects against a private title-session
        // copy first. The live command state is advanced only after the
        // complete memory copy passes overlap/order/bounds admission.
        if (!deuteros_amiga_->title_stage_session()) {
            result.error = "Deuteros planar command requires the owned title-stage session";
            return result;
        }
        auto preview_session = *deuteros_amiga_->title_stage_session();
        const auto plan = preview_session.observe_command_planar_write(observation);
        if (!plan) {
            result.error = "Deuteros planar command did not match its owned boundary";
            return result;
        }
        NativeRuntimeEffectBatch batch{
            "deuteros-amiga-title-command-planar-"
                + std::to_string(deuteros_amiga_title_command_generation_),
            true,
            {}};
        batch.effects.reserve(plan->destination_addresses.size() + 1U);
        for (std::size_t index = 0; index < plan->destination_addresses.size(); ++index) {
            batch.effects.push_back({index + 1U,
                {NativeRuntimeAddressSpace::linear, std::nullopt,
                    plan->destination_addresses[index]},
                MemoryTransferElementWidth::byte,
                NativeRuntimeByteOrder::big_endian,
                plan->destination_values[index]});
        }
        batch.effects.push_back({batch.effects.size() + 1U,
            {NativeRuntimeAddressSpace::linear, std::nullopt,
                plan->destination_pointer_cell_address},
            MemoryTransferElementWidth::longword,
            NativeRuntimeByteOrder::big_endian,
            plan->destination_pointer_value});
        auto next_memory = *native_runtime_memory_;
        const auto applied = next_memory.apply(batch);
        if (!applied.accepted) {
            result.error = "Runtime-memory application rejected: " + applied.error;
            return result;
        }
        auto next_surface = deuteros_amiga_title_planar_surface_.value_or(
            DeuterosAmigaTitlePlanarSurface{});
        const auto surface_applied = next_surface.apply(plan->destination_addresses,
            plan->destination_values, deuteros_amiga_title_command_generation_);
        if (!surface_applied.accepted) {
            result.error = "Native planar-surface application rejected: "
                + surface_applied.error;
            return result;
        }
        // The same observation was fully validated without touching live
        // state. Committing it now is deterministic and allocation-free.
        if (!deuteros_amiga_->observe_title_command_planar_write(observation)) {
            result.error = "Deuteros planar command disappeared before commit";
            return result;
        }
        *native_runtime_memory_ = std::move(next_memory);
        deuteros_amiga_title_planar_surface_ = std::move(next_surface);
        deuteros_amiga_title_planar_base_ = plan->observation.observed_pointer_values[1];
        deuteros_amiga_title_planar_generation_ =
            deuteros_amiga_title_command_generation_;
        result.accepted = true;
    } catch (const std::exception& e) {
        result.error = std::string("Deuteros planar command rejected: ") + e.what();
    }
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_command_planar_variant_write(
    const DeuterosAmigaObservedTitleCommandPlanarVariantWrite observation) {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !native_runtime_memory_
        || deuteros_amiga_title_command_generation_ == 0) {
        result.error = "Deuteros planar variant requires an active owned command generation";
        return result;
    }
    try {
        if (!deuteros_amiga_->title_stage_session()) {
            result.error = "Deuteros planar variant requires the owned title-stage session";
            return result;
        }
        auto preview_session = *deuteros_amiga_->title_stage_session();
        const auto plan = preview_session.observe_command_planar_variant_write(observation);
        if (!plan) {
            result.error = "Deuteros planar variant did not match its owned boundary";
            return result;
        }
        auto next_memory = *native_runtime_memory_;
        if (plan->variant == DeuterosAmigaTitlePlanarVariant::positive_clear
            || plan->variant == DeuterosAmigaTitlePlanarVariant::positive_set) {
            for (std::size_t index = 0; index < plan->destination_addresses.size(); ++index) {
                const NativeRuntimeLocation location{NativeRuntimeAddressSpace::linear,
                    std::nullopt, plan->destination_addresses[index]};
                const auto known = next_memory.read_byte(location);
                if (known && *known != static_cast<std::uint8_t>(
                        observation.observed_base_values[index])) {
                    result.error = "Deuteros planar destination read contradicts owned runtime memory";
                    return result;
                }
            }
        }
        NativeRuntimeEffectBatch batch{
            "deuteros-amiga-title-command-planar-variant-"
                + std::to_string(deuteros_amiga_title_command_generation_),
            true, {}};
        batch.effects.reserve(plan->destination_addresses.size() + 1U);
        for (std::size_t index = 0; index < plan->destination_addresses.size(); ++index) {
            batch.effects.push_back({index + 1U,
                {NativeRuntimeAddressSpace::linear, std::nullopt,
                    plan->destination_addresses[index]},
                MemoryTransferElementWidth::byte,
                NativeRuntimeByteOrder::big_endian,
                plan->destination_values[index]});
        }
        batch.effects.push_back({batch.effects.size() + 1U,
            {NativeRuntimeAddressSpace::linear, std::nullopt,
                plan->destination_pointer_cell_address},
            MemoryTransferElementWidth::longword,
            NativeRuntimeByteOrder::big_endian,
            plan->destination_pointer_value});
        const auto applied = next_memory.apply(batch);
        if (!applied.accepted) {
            result.error = "Runtime-memory application rejected: " + applied.error;
            return result;
        }

        auto next_surface = deuteros_amiga_title_planar_surface_;
        if (plan->recovered_title_surface_layout) {
            auto candidate = next_surface.value_or(DeuterosAmigaTitlePlanarSurface{});
            const auto surface_applied = candidate.apply(plan->destination_addresses,
                plan->destination_values, deuteros_amiga_title_command_generation_);
            if (!surface_applied.accepted) {
                result.error = "Native planar-surface application rejected: "
                    + surface_applied.error;
                return result;
            }
            next_surface = std::move(candidate);
        }
        if (!deuteros_amiga_->observe_title_command_planar_variant_write(observation)) {
            result.error = "Deuteros planar variant disappeared before commit";
            return result;
        }
        *native_runtime_memory_ = std::move(next_memory);
        deuteros_amiga_title_planar_surface_ = std::move(next_surface);
        if (plan->recovered_title_surface_layout) {
            deuteros_amiga_title_planar_base_ = plan->destination_addresses[0];
            deuteros_amiga_title_planar_generation_ =
                deuteros_amiga_title_command_generation_;
        }
        result.accepted = true;
    } catch (const std::exception& e) {
        result.error = std::string("Deuteros planar variant rejected: ") + e.what();
    }
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_command_negative_service(
    const DeuterosAmigaObservedTitleCommandNegativeService observation) {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || deuteros_amiga_title_command_generation_ == 0) {
        result.error = "Deuteros negative service requires an active owned command generation";
        return result;
    }
    try {
        if (!deuteros_amiga_->title_stage_session()) {
            result.error = "Deuteros negative service requires the owned title-stage session";
            return result;
        }
        auto preview_session = *deuteros_amiga_->title_stage_session();
        const auto plan = preview_session.observe_command_negative_service(observation);
        if (!plan) {
            result.error = "Deuteros negative service did not match its owned boundary";
            return result;
        }
        if (!deuteros_amiga_->observe_title_command_negative_service(observation)) {
            result.error = "Deuteros negative service disappeared before commit";
            return result;
        }
        result.accepted = true;
    } catch (const std::exception& e) {
        result.error = std::string("Deuteros negative service rejected: ") + e.what();
    }
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_command_pointer_route(
    const DeuterosAmigaObservedTitlePostCommandPointerRoute observation) {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !native_runtime_memory_) {
        result.error = "Deuteros post-command route requires an active owned title session";
        return result;
    }
    try {
        if (!deuteros_amiga_->title_stage_session()) {
            result.error = "Deuteros post-command route requires the owned title-stage session";
            return result;
        }
        auto preview_session = *deuteros_amiga_->title_stage_session();
        const auto plan = preview_session.observe_post_command_pointer_route(observation);
        if (!plan) {
            result.error = "Deuteros post-command route did not match its owned boundary";
            return result;
        }
        auto next_memory = *native_runtime_memory_;
        NativeRuntimeEffectBatch batch{"deuteros-amiga-title-post-command-pointer-route",
            true,{}};
        for (std::size_t index=0; index<plan->effect_count; ++index) {
            batch.effects.push_back({index+1U,
                {NativeRuntimeAddressSpace::linear,std::nullopt,
                    plan->destination_addresses[index]},
                plan->destination_widths[index],NativeRuntimeByteOrder::big_endian,
                plan->destination_values[index]});
        }
        const auto applied=next_memory.apply(batch);
        if (!applied.accepted) {
            result.error="Runtime-memory application rejected: "+applied.error;
            return result;
        }
        if (!deuteros_amiga_->observe_title_post_command_pointer_route(observation)) {
            result.error="Deuteros post-command route disappeared before commit";
            return result;
        }
        *native_runtime_memory_=std::move(next_memory);
        result.accepted=true;
    } catch(const std::exception& e) {
        result.error=std::string("Deuteros post-command route rejected: ")+e.what();
    }
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_command_graphics_return(
    const DeuterosAmigaObservedGraphicsVectorReturn observation) {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !native_runtime_memory_) {
        result.error="Deuteros post-command graphics return requires an active owned title session";
        return result;
    }
    try {
        if (!deuteros_amiga_->title_stage_session()) {
            result.error="Deuteros post-command graphics return requires the owned title-stage session";
            return result;
        }
        auto preview=*deuteros_amiga_->title_stage_session();
        const auto plan=preview.observe_post_command_graphics_return(observation);
        if (!plan) {
            result.error="Deuteros post-command graphics return did not match its owned boundary";
            return result;
        }
        auto next_memory=*native_runtime_memory_;
        NativeRuntimeEffectBatch batch{"deuteros-amiga-title-post-command-graphics-return",
            true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,
                plan->cleared_byte_address},MemoryTransferElementWidth::byte,
                NativeRuntimeByteOrder::big_endian,plan->cleared_byte_value}}};
        const auto applied=next_memory.apply(batch);
        if (!applied.accepted) {
            result.error="Runtime-memory application rejected: "+applied.error;
            return result;
        }
        if (!deuteros_amiga_->observe_title_post_command_graphics_return(observation)) {
            result.error="Deuteros post-command graphics return disappeared before commit";
            return result;
        }
        *native_runtime_memory_=std::move(next_memory);
        result.accepted=true;
    } catch(const std::exception& e) {
        result.error=std::string("Deuteros post-command graphics return rejected: ")+e.what();
    }
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_title_post_command_first_dispatch() {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !deuteros_amiga_->title_stage_session()) {
        result.error="Deuteros first post-command dispatch requires an active owned title session";
        return result;
    }
    try {
        auto preview=*deuteros_amiga_->title_stage_session();
        if (!preview.advance_post_command_first_dispatch()) {
            result.error="Deuteros first post-command dispatch did not match its owned boundary";
            return result;
        }
        if (!deuteros_amiga_->advance_title_post_command_first_dispatch()) {
            result.error="Deuteros first post-command dispatch disappeared before commit";
            return result;
        }
        result.accepted=true;
    } catch(const std::exception& e) {
        result.error=std::string("Deuteros first post-command dispatch rejected: ")+e.what();
    }
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_command_first_dispatch_header(
    const DeuterosAmigaObservedTitleFirstDispatchHeader observation) {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !native_runtime_memory_
        || !deuteros_amiga_->title_stage_session()) {
        result.error="Deuteros first dispatch header requires an active owned title session";
        return result;
    }
    try {
        auto preview=*deuteros_amiga_->title_stage_session();
        const auto plan=preview.observe_post_command_first_dispatch_header(observation);
        if (!plan) {
            result.error="Deuteros first dispatch header did not match its owned boundary";
            return result;
        }
        auto next_memory=*native_runtime_memory_;
        NativeRuntimeEffectBatch batch{"deuteros-amiga-title-first-dispatch-header",true,{}};
        for(std::size_t i=0;i<2;++i) batch.effects.push_back({i+1U,
            {NativeRuntimeAddressSpace::linear,std::nullopt,plan->destination_addresses[i]},
            MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,
            plan->destination_values[i]});
        const auto applied=next_memory.apply(batch);
        if(!applied.accepted){result.error="Runtime-memory application rejected: "+applied.error;return result;}
        if(!deuteros_amiga_->observe_title_post_command_first_dispatch_header(observation)){
            result.error="Deuteros first dispatch header disappeared before commit";return result;}
        *native_runtime_memory_=std::move(next_memory);
        result.accepted=true;
    }catch(const std::exception&e){result.error=std::string("Deuteros first dispatch header rejected: ")+e.what();}
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_title_post_command_first_dispatch_packet() {
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::deuteros_amiga_title_stage
        ||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){
        result.error="Deuteros first dispatch packet requires an active owned title session";return result;}
    try{
        auto preview=*deuteros_amiga_->title_stage_session();
        const auto plan=preview.advance_post_command_first_dispatch_packet();
        if(!plan){result.error="Deuteros first dispatch packet did not match its owned boundary";return result;}
        auto next_memory=*native_runtime_memory_;
        NativeRuntimeEffectBatch batch{"deuteros-amiga-title-first-dispatch-packet",true,{}};
        for(std::size_t i=0;i<4;++i)batch.effects.push_back({i+1U,
            {NativeRuntimeAddressSpace::linear,std::nullopt,plan->destination_addresses[i]},
            plan->destination_widths[i],NativeRuntimeByteOrder::big_endian,plan->destination_values[i]});
        const auto applied=next_memory.apply(batch);
        if(!applied.accepted){result.error="Runtime-memory application rejected: "+applied.error;return result;}
        if(!deuteros_amiga_->advance_title_post_command_first_dispatch_packet()){
            result.error="Deuteros first dispatch packet disappeared before commit";return result;}
        *native_runtime_memory_=std::move(next_memory);result.accepted=true;
    }catch(const std::exception&e){result.error=std::string("Deuteros first dispatch packet rejected: ")+e.what();}
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_title_post_command_first_dispatch_decode(){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::deuteros_amiga_title_stage
        ||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){
        result.error="Deuteros first dispatch decode requires an active owned title session";return result;}
    try{
        auto preview=*deuteros_amiga_->title_stage_session();const auto plan=preview.advance_post_command_first_dispatch_decode();
        if(!plan){result.error="Deuteros first dispatch decode did not match its owned boundary";return result;}
        auto next_memory=*native_runtime_memory_;
        NativeRuntimeEffectBatch batch{"deuteros-amiga-title-first-dispatch-decode",true,{}};
        batch.effects.reserve(plan->destination_addresses.size());
        for(std::size_t i=0;i<plan->destination_addresses.size();++i)batch.effects.push_back({i+1U,
            {NativeRuntimeAddressSpace::linear,std::nullopt,plan->destination_addresses[i]},
            MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,plan->destination_values[i]});
        const auto applied=next_memory.apply(batch);if(!applied.accepted){result.error="Runtime-memory application rejected: "+applied.error;return result;}
        if(!deuteros_amiga_->advance_title_post_command_first_dispatch_decode()){result.error="Deuteros first dispatch decode disappeared before commit";return result;}
        *native_runtime_memory_=std::move(next_memory);result.accepted=true;
    }catch(const std::exception&e){result.error=std::string("Deuteros first dispatch decode rejected: ")+e.what();}
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_title_post_command_first_dispatch_caller_tail(){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::deuteros_amiga_title_stage
        ||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){
        result.error="Deuteros first dispatch caller tail requires an active owned title session";return result;}
    try{
        auto preview=*deuteros_amiga_->title_stage_session();
        if(!preview.advance_post_command_first_dispatch_caller_tail()){
            result.error="Deuteros first dispatch caller tail did not match its owned boundary";return result;}
        if(!deuteros_amiga_->advance_title_post_command_first_dispatch_caller_tail()){
            result.error="Deuteros first dispatch caller tail disappeared before commit";return result;}
        result.accepted=true;
    }catch(const std::exception&e){result.error=std::string("Deuteros first dispatch caller tail rejected: ")+e.what();}
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_command_first_dispatch_destination_words(
    const DeuterosAmigaObservedTitleFirstDispatchDestinationWords observation){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::deuteros_amiga_title_stage
        ||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){
        result.error="Deuteros first merge requires an active owned title session";return result;}
    try{
        auto preview=*deuteros_amiga_->title_stage_session();const auto plan=preview.observe_post_command_first_dispatch_destination_words(observation);
        if(!plan){result.error="Deuteros first merge did not match its owned boundary";return result;}
        auto next_memory=*native_runtime_memory_;
        for(std::size_t i=0;i<observation.source_addresses.size();++i){
            const auto address=observation.source_addresses[i];const auto word=observation.observed_words[i];
            const auto high=next_memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address});
            const auto low=next_memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address+1U});
            if((high&&*high!=static_cast<std::uint8_t>(word>>8U))
                ||(low&&*low!=static_cast<std::uint8_t>(word&0xffU))){
                result.error="Deuteros first merge observation contradicts owned runtime memory";return result;}
        }
        NativeRuntimeEffectBatch batch{"deuteros-amiga-title-first-dispatch-merge",true,{}};
        batch.effects.reserve(plan->destination_addresses.size());
        for(std::size_t i=0;i<plan->destination_addresses.size();++i)batch.effects.push_back({i+1U,
            {NativeRuntimeAddressSpace::linear,std::nullopt,plan->destination_addresses[i]},
            MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,plan->destination_values[i]});
        const auto applied=next_memory.apply(batch);if(!applied.accepted){result.error="Runtime-memory application rejected: "+applied.error;return result;}
        if(!deuteros_amiga_->observe_title_post_command_first_dispatch_destination_words(observation)){
            result.error="Deuteros first merge disappeared before commit";return result;}
        *native_runtime_memory_=std::move(next_memory);result.accepted=true;
    }catch(const std::exception&e){result.error=std::string("Deuteros first merge rejected: ")+e.what();}
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_title_post_command_second_dispatch(){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::deuteros_amiga_title_stage
        ||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){
        result.error="Deuteros second post-command dispatch requires an active owned title session";return result;}
    try{auto preview=*deuteros_amiga_->title_stage_session();
        if(!preview.advance_post_command_second_dispatch()){result.error="Deuteros second post-command dispatch did not match its owned boundary";return result;}
        if(!deuteros_amiga_->advance_title_post_command_second_dispatch()){result.error="Deuteros second post-command dispatch disappeared before commit";return result;}
        result.accepted=true;
    }catch(const std::exception&e){result.error=std::string("Deuteros second post-command dispatch rejected: ")+e.what();}
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_title_post_command_second_dispatch_decode(){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::deuteros_amiga_title_stage
        ||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){
        result.error="Deuteros second dispatch decode requires an active owned title session";return result;}
    try{auto preview=*deuteros_amiga_->title_stage_session();const auto plan=preview.advance_post_command_second_dispatch_decode();
        if(!plan){result.error="Deuteros second dispatch decode did not match its owned boundary";return result;}
        auto next_memory=*native_runtime_memory_;NativeRuntimeEffectBatch batch{"deuteros-amiga-title-second-dispatch-decode",true,{}};
        batch.effects.reserve(plan->destination_addresses.size());
        for(std::size_t i=0;i<plan->destination_addresses.size();++i)batch.effects.push_back({i+1U,
            {NativeRuntimeAddressSpace::linear,std::nullopt,plan->destination_addresses[i]},
            MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,plan->destination_values[i]});
        const auto applied=next_memory.apply(batch);if(!applied.accepted){result.error="Runtime-memory application rejected: "+applied.error;return result;}
        if(!deuteros_amiga_->advance_title_post_command_second_dispatch_decode()){result.error="Deuteros second dispatch decode disappeared before commit";return result;}
        *native_runtime_memory_=std::move(next_memory);result.accepted=true;
    }catch(const std::exception&e){result.error=std::string("Deuteros second dispatch decode rejected: ")+e.what();}
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_command_second_dispatch_destination_words(
    const DeuterosAmigaObservedTitleSecondDispatchDestinationWords observation){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::deuteros_amiga_title_stage
        ||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){
        result.error="Deuteros second merge requires an active owned title session";return result;}
    try{auto preview=*deuteros_amiga_->title_stage_session();const auto plan=preview.observe_post_command_second_dispatch_destination_words(observation);
        if(!plan){result.error="Deuteros second merge did not match its owned boundary";return result;}
        auto next_memory=*native_runtime_memory_;
        for(std::size_t i=0;i<observation.source_addresses.size();++i){const auto address=observation.source_addresses[i];const auto word=observation.observed_words[i];
            const auto high=next_memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address});
            const auto low=next_memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address+1U});
            if((high&&*high!=static_cast<std::uint8_t>(word>>8U))||(low&&*low!=static_cast<std::uint8_t>(word&0xffU))){result.error="Deuteros second merge observation contradicts owned runtime memory";return result;}}
        NativeRuntimeEffectBatch batch{"deuteros-amiga-title-second-dispatch-merge",true,{}};batch.effects.reserve(320);
        for(std::size_t i=0;i<plan->destination_addresses.size();++i)batch.effects.push_back({i+1U,
            {NativeRuntimeAddressSpace::linear,std::nullopt,plan->destination_addresses[i]},MemoryTransferElementWidth::word,
            NativeRuntimeByteOrder::big_endian,plan->destination_values[i]});
        const auto applied=next_memory.apply(batch);if(!applied.accepted){result.error="Runtime-memory application rejected: "+applied.error;return result;}
        if(!deuteros_amiga_->observe_title_post_command_second_dispatch_destination_words(observation)){result.error="Deuteros second merge disappeared before commit";return result;}
        *native_runtime_memory_=std::move(next_memory);result.accepted=true;
    }catch(const std::exception&e){result.error=std::string("Deuteros second merge rejected: ")+e.what();}
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_title_post_command_service_route_prefix(){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::deuteros_amiga_title_stage
        ||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){
        result.error="Deuteros post-command service prefix requires an active owned title session";return result;}
    try{auto preview=*deuteros_amiga_->title_stage_session();const auto plan=preview.advance_post_command_service_route_prefix();
        if(!plan){result.error="Deuteros post-command service prefix did not match its owned boundary";return result;}
        auto next_memory=*native_runtime_memory_;NativeRuntimeEffectBatch batch{"deuteros-amiga-title-post-command-service-prefix",true,{}};
        for(std::size_t i=0;i<plan->destination_addresses.size();++i)batch.effects.push_back({i+1U,
            {NativeRuntimeAddressSpace::linear,std::nullopt,plan->destination_addresses[i]},plan->destination_widths[i],
            NativeRuntimeByteOrder::big_endian,plan->destination_values[i]});
        const auto applied=next_memory.apply(batch);if(!applied.accepted){result.error="Runtime-memory application rejected: "+applied.error;return result;}
        if(!deuteros_amiga_->advance_title_post_command_service_route_prefix()){result.error="Deuteros post-command service prefix disappeared before commit";return result;}
        *native_runtime_memory_=std::move(next_memory);result.accepted=true;
    }catch(const std::exception&e){result.error=std::string("Deuteros post-command service prefix rejected: ")+e.what();}
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_command_service_first_return(
    const DeuterosAmigaObservedLocalCallReturn observation){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::deuteros_amiga_title_stage
        ||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){
        result.error="Deuteros post-command first service return requires an active owned title session";return result;}
    try{auto preview=*deuteros_amiga_->title_stage_session();
        if(!preview.observe_post_command_service_first_return(observation)){result.error="Deuteros post-command first service return did not match its owned boundary";return result;}
        if(!deuteros_amiga_->observe_title_post_command_service_first_return(observation)){result.error="Deuteros post-command first service return disappeared before commit";return result;}
        result.accepted=true;
    }catch(const std::exception&e){result.error=std::string("Deuteros post-command first service return rejected: ")+e.what();}
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_command_service_second_return(
    const DeuterosAmigaObservedLocalCallReturn observation){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::deuteros_amiga_title_stage
        ||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){
        result.error="Deuteros post-command second service return requires an active owned title session";return result;}
    try{auto preview=*deuteros_amiga_->title_stage_session();const auto plan=preview.observe_post_command_service_second_return(observation);
        if(!plan){result.error="Deuteros post-command second service return did not match its owned boundary";return result;}
        auto next_memory=*native_runtime_memory_;NativeRuntimeEffectBatch batch{"deuteros-amiga-title-post-command-second-service-return",true,
            {{1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->destination_address},MemoryTransferElementWidth::longword,
              NativeRuntimeByteOrder::big_endian,plan->selected_value}}};
        const auto applied=next_memory.apply(batch);if(!applied.accepted){result.error="Runtime-memory application rejected: "+applied.error;return result;}
        if(!deuteros_amiga_->observe_title_post_command_service_second_return(observation)){result.error="Deuteros post-command second service return disappeared before commit";return result;}
        *native_runtime_memory_=std::move(next_memory);result.accepted=true;
    }catch(const std::exception&e){result.error=std::string("Deuteros post-command second service return rejected: ")+e.what();}
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_command_service_third_return(
    const DeuterosAmigaObservedLocalCallReturn o){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros post-command third service return requires active title session";return r;}
    try{auto p=*deuteros_amiga_->title_stage_session();if(!p.observe_post_command_service_third_return(o)){r.error="Deuteros post-command third service return did not match boundary";return r;}
        if(!deuteros_amiga_->observe_title_post_command_service_third_return(o)){r.error="Deuteros post-command third service return disappeared";return r;}r.accepted=true;
    }catch(const std::exception&e){r.error=std::string("Deuteros post-command third service return rejected: ")+e.what();}return r;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_command_nested_words(
    const DeuterosAmigaObservedTitlePostCommandNestedWords o){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros post-command nested words require active title session";return r;}
    try{auto p=*deuteros_amiga_->title_stage_session();const auto plan=p.observe_post_command_nested_words(o);if(!plan){r.error="Deuteros post-command nested words did not match boundary";return r;}
        auto memory=*native_runtime_memory_;if(plan->writes_counter){NativeRuntimeEffectBatch b{"deuteros-amiga-title-post-command-nested-counter",true,
            {{1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->counter_destination},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,plan->counter_value}}};
            const auto a=memory.apply(b);if(!a.accepted){r.error="Runtime-memory application rejected: "+a.error;return r;}}
        if(!deuteros_amiga_->observe_title_post_command_nested_words(o)){r.error="Deuteros post-command nested words disappeared";return r;}
        *native_runtime_memory_=std::move(memory);r.accepted=true;
    }catch(const std::exception&e){r.error=std::string("Deuteros post-command nested words rejected: ")+e.what();}return r;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_command_nested_call_return(const DeuterosAmigaObservedLocalCallReturn o){
    DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros nested call return requires active title session";return r;}
    try{auto p=*deuteros_amiga_->title_stage_session();if(!p.observe_post_command_nested_call_return(o)){r.error="Deuteros nested call return did not match boundary";return r;}
        if(!deuteros_amiga_->observe_title_post_command_nested_call_return(o)){r.error="Deuteros nested call return disappeared";return r;}r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_title_post_command_nested_loop(){
    DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros nested loop requires active title session";return r;}
    try{auto p=*deuteros_amiga_->title_stage_session();const auto plan=p.advance_post_command_nested_loop();if(!plan){r.error="Deuteros nested loop did not match boundary";return r;}
        auto memory=*native_runtime_memory_;if(plan->writes_counter){NativeRuntimeEffectBatch b{"deuteros-amiga-title-post-command-nested-loop-counter",true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->counter_destination},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,plan->counter_value}}};const auto a=memory.apply(b);if(!a.accepted){r.error=a.error;return r;}}
        if(!deuteros_amiga_->advance_title_post_command_nested_loop()){r.error="Deuteros nested loop disappeared";return r;}*native_runtime_memory_=std::move(memory);r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_command_continuation_return(const DeuterosAmigaObservedLocalCallReturn o){DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros continuation return requires active title session";return r;}try{auto p=*deuteros_amiga_->title_stage_session();if(!p.observe_post_command_continuation_return(o)||!deuteros_amiga_->observe_title_post_command_continuation_return(o)){r.error="Deuteros continuation return did not match boundary";return r;}r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_command_pointer_chain(const DeuterosAmigaObservedTitlePostCommandPointerChain o){DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros pointer chain requires active title session";return r;}try{auto p=*deuteros_amiga_->title_stage_session();const auto plan=p.observe_post_command_pointer_chain(o);if(!plan){r.error="Deuteros pointer chain did not match boundary";return r;}auto m=*native_runtime_memory_;NativeRuntimeEffectBatch b{"deuteros-amiga-title-post-command-descriptor",true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->descriptor_destination},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,plan->descriptor_word}}};const auto a=m.apply(b);if(!a.accepted){r.error=a.error;return r;}if(!deuteros_amiga_->observe_title_post_command_pointer_chain(o)){r.error="Deuteros pointer chain disappeared";return r;}*native_runtime_memory_=std::move(m);r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_command_dispatch_destination(const DeuterosAmigaObservedTitlePostCommandDispatchDestination o){DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros dispatch destination requires active title session";return r;}try{auto p=*deuteros_amiga_->title_stage_session();if(!p.observe_post_command_dispatch_destination(o)||!deuteros_amiga_->observe_title_post_command_dispatch_destination(o)){r.error="Deuteros dispatch destination did not match boundary";return r;}r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_custom_chip_write(
    const DeuterosAmigaObservedCustomChipWrite observation) {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !deuteros_amiga_->title_stage_session()
        || !deuteros_amiga_->title_stage_session()->custom_chip_boundary()) {
        result.error="Deuteros title observation requires the active custom-chip boundary";
        return result;
    }
    const auto before=deuteros_amiga_->title_stage_session()->custom_chip_boundary()->observed_write_count();
    try {
        static_cast<void>(deuteros_amiga_->observe_title_custom_chip_write(observation));
        const auto* after=deuteros_amiga_->title_stage_session()->custom_chip_boundary();
        if (!after || after->observed_write_count()!=before+1) { result.error="Deuteros custom-chip observation did not advance"; return result; }
        result.accepted=true;
    } catch(const std::exception& e) { result.error=std::string("Deuteros title observation rejected: ")+e.what(); }
    return result;
}

DeuterosAmigaTitleDisplayTraceAdmission
ReleaseRuntimeCoordinator::admit_active_deuteros_amiga_title_display_trace(
    const ReferenceTrace& trace) {
    DeuterosAmigaTitleDisplayTraceAdmission rejected;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || active_->release.game != Game::deuteros
        || active_->release.platform != Platform::amiga
        || active_->release.language != "en"
        || active_->release.sha256 != trace.source_release.sha256) {
        rejected.error = "Active session has not reached the exact Deuteros Amiga title stage";
        return rejected;
    }
    auto admission = admit_deuteros_amiga_title_display_trace(trace);
    if (!admission.session) return admission;
    // Publish only after every external file has been consumed and rehashed.
    // The stored session has no paths or borrows into the external capture.
    deuteros_amiga_title_display_trace_ = *admission.session;
    session_snapshot_ = make_runtime_session_snapshot(*active_,
        RuntimeSessionKind::deuteros_amiga_title_display_trace_boundary);
    return admission;
}

std::optional<DeuterosAmigaTitleDisplayTraceCheckpoint>
ReleaseRuntimeCoordinator::deuteros_amiga_title_display_trace_checkpoint() const {
    if (!session_snapshot_
        || session_snapshot_->kind
            != RuntimeSessionKind::deuteros_amiga_title_display_trace_boundary
        || !deuteros_amiga_title_display_trace_) return std::nullopt;
    return deuteros_amiga_title_display_trace_->checkpoint();
}

std::optional<DeuterosAmigaTitlePlanarPatchSnapshot>
ReleaseRuntimeCoordinator::deuteros_amiga_title_planar_patch() const {
    if (!session_snapshot_
        || session_snapshot_->kind
            != RuntimeSessionKind::deuteros_amiga_title_display_trace_boundary
        || !deuteros_amiga_title_display_trace_ || !deuteros_amiga_
        || !deuteros_amiga_->title_stage_session() || !native_runtime_memory_
        || !deuteros_amiga_title_planar_base_
        || deuteros_amiga_title_planar_generation_ == 0) {
        return std::nullopt;
    }
    const auto& trace = deuteros_amiga_title_display_trace_->checkpoint();
    if (trace.display_layout_count != 1 || trace.bitplane_layout_count != 1
        || trace.palette_checkpoint_count != 1 || trace.frame_checkpoint_count != 1) {
        return std::nullopt;
    }
    const auto palette20 =
        deuteros_amiga_->title_stage_session()->graphics_setup_palette_evidence();
    std::array<RgbColor, 16> palette{};
    std::copy_n(palette20.begin(), palette.size(), palette.begin());
    return decode_deuteros_amiga_title_planar_patch(
        native_runtime_memory_->checkpoint(), *deuteros_amiga_title_planar_base_,
        deuteros_amiga_title_planar_generation_, palette);
}

std::optional<DeuterosAmigaTitlePlanarSurfaceSnapshot>
ReleaseRuntimeCoordinator::deuteros_amiga_title_planar_surface() const {
    if (!session_snapshot_
        || session_snapshot_->kind
            != RuntimeSessionKind::deuteros_amiga_title_display_trace_boundary
        || !deuteros_amiga_title_display_trace_ || !deuteros_amiga_
        || !deuteros_amiga_->title_stage_session() || !native_runtime_memory_
        || !deuteros_amiga_title_planar_surface_) {
        return std::nullopt;
    }
    const auto& trace = deuteros_amiga_title_display_trace_->checkpoint();
    if (trace.display_layout_count != 1 || trace.bitplane_layout_count != 1
        || trace.palette_checkpoint_count != 1 || trace.frame_checkpoint_count != 1) {
        return std::nullopt;
    }
    const auto palette20 =
        deuteros_amiga_->title_stage_session()->graphics_setup_palette_evidence();
    std::array<RgbColor, 16> palette{};
    std::copy_n(palette20.begin(), palette.size(), palette.begin());
    return deuteros_amiga_title_planar_surface_->snapshot(
        palette, native_runtime_memory_->checkpoint().checksum);
}

std::optional<DeuterosAtariBootstrapCheckpoint>
ReleaseRuntimeCoordinator::deuteros_atari_bootstrap_checkpoint() const {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::deuteros_atari_bootstrap
        || !deuteros_atari_) return std::nullopt;
    return deuteros_atari_->checkpoint();
}

std::optional<DeuterosAtariBootstrapPresentationSnapshot>
ReleaseRuntimeCoordinator::deuteros_atari_bootstrap_presentation() const {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::deuteros_atari_bootstrap
        || !deuteros_atari_) return std::nullopt;
    const auto& boot = deuteros_atari_->boot();
    return DeuterosAtariBootstrapPresentationSnapshot{
        deuteros_atari_->checkpoint(),
        boot.first_stage_offset,
        boot.first_stage_length,
        deuteros_atari_->first_stage_copy_execution(),
        deuteros_atari_->entry_execution(),
    };
}

std::optional<MillenniumAmigaBootstrapPresentationSnapshot>
ReleaseRuntimeCoordinator::millennium_amiga_bootstrap_presentation() const {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_amiga_bootstrap
        || !millennium_amiga_) return std::nullopt;
    return MillenniumAmigaBootstrapPresentationSnapshot{
        millennium_amiga_->plan(),
        millennium_amiga_->opaque_invocation_boundary(),
        millennium_amiga_->resident_evidence(),
    };
}

MillenniumAmigaBootstrapRelocatorObservationResult
ReleaseRuntimeCoordinator::observe_millennium_amiga_bootstrap_relocator_overread(
    const MillenniumAmigaBootstrapRelocatorObservation observation){
    MillenniumAmigaBootstrapRelocatorObservationResult result;
    if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_amiga_bootstrap||!millennium_amiga_relocator_||!native_runtime_memory_||millennium_amiga_relocator_overread_sequence_||observation.sequence==0){result.error="Relocator over-read requires the active direct Defjam bootstrap";return result;}
    try{auto next_session=*millennium_amiga_relocator_;next_session.observe_overread_byte(observation.instruction_address,observation.source_or_target_address,observation.value);const auto& effect=next_session.copy_effects().back();NativeRuntimeEffectBatch batch{"millennium-amiga-bootstrap-relocator-"+std::to_string(millennium_amiga_relocator_generation_)+"-overread",true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,effect.destination_address},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,effect.value}}};auto next_memory=*native_runtime_memory_;const auto applied=next_memory.apply(batch);if(!applied.accepted){result.error="Runtime-memory application rejected: "+applied.error;return result;}*millennium_amiga_relocator_=std::move(next_session);*native_runtime_memory_=std::move(next_memory);millennium_amiga_relocator_overread_sequence_=observation.sequence;result.accepted=true;}catch(const std::exception&e){result.error=std::string("Relocator over-read rejected: ")+e.what();}return result;
}

MillenniumAmigaBootstrapRelocatorObservationResult
ReleaseRuntimeCoordinator::observe_millennium_amiga_bootstrap_relocator_terminal_jump(
    const MillenniumAmigaBootstrapRelocatorObservation observation){
    MillenniumAmigaBootstrapRelocatorObservationResult result;
    if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_amiga_bootstrap||!millennium_amiga_relocator_||!millennium_amiga_relocator_overread_sequence_||millennium_amiga_relocator_terminal_sequence_||observation.sequence<=*millennium_amiga_relocator_overread_sequence_){result.error="Relocator jump requires the later active over-read generation";return result;}try{auto next=*millennium_amiga_relocator_;next.observe_terminal_jump(observation.instruction_address,observation.source_or_target_address);*millennium_amiga_relocator_=std::move(next);millennium_amiga_relocator_terminal_sequence_=observation.sequence;result.accepted=true;}catch(const std::exception&e){result.error=std::string("Relocator jump rejected: ")+e.what();}return result;
}

std::optional<MillenniumAmigaBootstrapRelocatorCheckpoint>
ReleaseRuntimeCoordinator::millennium_amiga_bootstrap_relocator_checkpoint()const{
    if(!session_snapshot_
        ||session_snapshot_->kind!=RuntimeSessionKind::millennium_amiga_bootstrap
        ||!millennium_amiga_relocator_) return std::nullopt;
    const auto& session=*millennium_amiga_relocator_;
    return MillenniumAmigaBootstrapRelocatorCheckpoint{
        millennium_amiga_relocator_generation_,session.state(),session.boundary(),
        session.copy_effects().size(),session.custom_chip_effect(),session.final_a3(),
        session.final_a5(),session.final_d1()};
}

std::optional<MillenniumAtariBootstrapPresentationSnapshot>
ReleaseRuntimeCoordinator::millennium_atari_bootstrap_presentation() const {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_ || !millennium_atari_config_consumer_) return std::nullopt;
    return MillenniumAtariBootstrapPresentationSnapshot{
        atari_st_prg_load_diagnostics(millennium_atari_->native_prg_image()),
        millennium_atari_->read_only_gemdos().checkpoint(),
        millennium_atari_config_consumer_->checkpoint(),
        millennium_atari_->bootstrap(), millennium_atari_->bss_entry(),
        millennium_atari_->bss_source(), millennium_atari_->target(),
        millennium_atari_->execution(), millennium_atari_->fopen_boundary(),
        millennium_atari_->fopen_result_gate(), millennium_atari_->fopen_fallthrough(),
        millennium_atari_->fread_frame_prefix(), millennium_atari_->fread_config_transfer(),
        millennium_atari_->root_inventory(), millennium_atari_->config(),
        millennium_atari_->config_entry(), millennium_atari_->fread_config_load_address_boundary(),
        millennium_atari_->fread_mapped_config_prelude(),
    };
}

MillenniumAtariConfigConsumerResult
ReleaseRuntimeCoordinator::observe_millennium_atari_status_register(
    const MillenniumAtariStatusRegisterObservation observation) {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_config_consumer_ || !native_runtime_memory_) {
        return {false, "Atari SR observation requires the active Millennium config consumer"};
    }
    try {
        auto next_consumer = *millennium_atari_config_consumer_;
        auto result = next_consumer.observe_status_register(observation);
        if (!result.accepted) return result;
        auto next_memory = *native_runtime_memory_;
        const auto batches = next_consumer.make_hardware_effect_batches(
            "millennium-atari-" + std::to_string(observation.generation) + "-sr-"
                + std::to_string(observation.sequence));
        for (const auto& batch : batches) {
            const auto applied = next_memory.apply(batch);
            if (!applied.accepted) {
                return {false, "Atari hardware effect rejected: " + applied.error};
            }
        }
        *millennium_atari_config_consumer_ = std::move(next_consumer);
        *native_runtime_memory_ = std::move(next_memory);
        return {true, {}};
    } catch (const std::exception& error) {
        return {false, std::string("Atari SR observation rejected: ") + error.what()};
    }
}

MillenniumAtariConfigConsumerResult
ReleaseRuntimeCoordinator::observe_millennium_atari_xbios_selector_two(
    const MillenniumAtariXbiosSelectorTwoObservation observation) {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_config_consumer_ || !native_runtime_memory_) {
        return {false, "XBIOS selector-2 result requires the active Millennium Atari consumer"};
    }
    try {
        auto next_consumer = *millennium_atari_config_consumer_;
        auto result = next_consumer.observe_xbios_selector_two(observation);
        if (!result.accepted) return result;
        auto next_memory = *native_runtime_memory_;
        const auto batch = next_consumer.make_selector_two_result_effect_batch(
            "millennium-atari-" + std::to_string(observation.generation)
                + "-xbios-2-" + std::to_string(observation.sequence));
        const auto applied = next_memory.apply(batch);
        if (!applied.accepted) {
            return {false, "XBIOS selector-2 memory effect rejected: " + applied.error};
        }
        *millennium_atari_config_consumer_ = std::move(next_consumer);
        *native_runtime_memory_ = std::move(next_memory);
        return {true, {}};
    } catch (const std::exception& error) {
        return {false, std::string("XBIOS selector-2 result rejected: ") + error.what()};
    }
}

MillenniumAtariConfigConsumerResult
ReleaseRuntimeCoordinator::observe_millennium_atari_xbios_selector_three(
    const MillenniumAtariXbiosSelectorThreeObservation observation) {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_config_consumer_ || !native_runtime_memory_) {
        return {false, "XBIOS selector-3 result requires the active Millennium Atari consumer"};
    }
    try {
        auto next_consumer = *millennium_atari_config_consumer_;
        auto result = next_consumer.observe_xbios_selector_three(observation);
        if (!result.accepted) return result;
        auto next_memory = *native_runtime_memory_;
        const auto batch = next_consumer.make_selector_three_result_effect_batch(
            "millennium-atari-" + std::to_string(observation.generation)
                + "-xbios-3-" + std::to_string(observation.sequence));
        const auto applied = next_memory.apply(batch);
        if (!applied.accepted) return {false, "XBIOS selector-3 memory effect rejected: " + applied.error};
        *millennium_atari_config_consumer_ = std::move(next_consumer);
        *native_runtime_memory_ = std::move(next_memory);
        return {true, {}};
    } catch (const std::exception& error) {
        return {false, std::string("XBIOS selector-3 result rejected: ") + error.what()};
    }
}

MillenniumAtariConfigConsumerResult
ReleaseRuntimeCoordinator::observe_millennium_atari_xbios_selector_four(
    const MillenniumAtariXbiosSelectorFourObservation observation) {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_config_consumer_ || !native_runtime_memory_) {
        return {false, "XBIOS selector-4 result requires the active Millennium Atari consumer"};
    }
    try {
        auto next_consumer = *millennium_atari_config_consumer_;
        auto result = next_consumer.observe_xbios_selector_four(observation);
        if (!result.accepted) return result;
        auto next_memory = *native_runtime_memory_;
        const auto batch = next_consumer.make_selector_four_result_effect_batch(
            "millennium-atari-" + std::to_string(observation.generation)
                + "-xbios-4-" + std::to_string(observation.sequence));
        const auto applied = next_memory.apply(batch);
        if (!applied.accepted) return {false, "XBIOS selector-4 memory effect rejected: " + applied.error};
        *millennium_atari_config_consumer_ = std::move(next_consumer);
        *native_runtime_memory_ = std::move(next_memory);
        return {true, {}};
    } catch (const std::exception& error) {
        return {false, std::string("XBIOS selector-4 result rejected: ") + error.what()};
    }
}

MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_line_a(
    const MillenniumAtariLineAObservation observation) {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_config_consumer_ || !native_runtime_memory_) {
        return {false, "Line-A result requires the active Millennium Atari consumer"};
    }
    try {
        auto next_consumer = *millennium_atari_config_consumer_;
        auto result = next_consumer.observe_line_a(observation);
        if (!result.accepted) return result;
        auto next_memory = *native_runtime_memory_;
        const auto batch = next_consumer.make_line_a_result_effect_batch(
            "millennium-atari-" + std::to_string(observation.generation)
                + "-line-a-" + std::to_string(observation.sequence));
        const auto applied = next_memory.apply(batch);
        if (!applied.accepted) return {false, "Line-A memory effect rejected: " + applied.error};
        *millennium_atari_config_consumer_ = std::move(next_consumer);
        *native_runtime_memory_ = std::move(next_memory);
        return {true, {}};
    } catch (const std::exception& error) {
        return {false, std::string("Line-A result rejected: ") + error.what()};
    }
}

MillenniumAtariConfigConsumerResult
ReleaseRuntimeCoordinator::observe_millennium_atari_xbios_selector_21(
    const MillenniumAtariXbiosSelector21Observation observation) {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_config_consumer_) {
        return {false, "XBIOS selector-21 result requires the active Millennium Atari consumer"};
    }
    auto next = *millennium_atari_config_consumer_;
    auto result = next.observe_xbios_selector_21(observation);
    if (!result.accepted) return result;
    *millennium_atari_config_consumer_ = std::move(next);
    return {true, {}};
}

MillenniumAtariConfigConsumerResult
ReleaseRuntimeCoordinator::observe_millennium_atari_xbios_selector_6(
    const MillenniumAtariXbiosSelector6Observation observation) {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_config_consumer_) return {false, "XBIOS selector-6 result requires the active Millennium Atari consumer"};
    auto next = *millennium_atari_config_consumer_;
    auto result = next.observe_xbios_selector_6(observation);
    if (!result.accepted) return result;
    *millennium_atari_config_consumer_ = std::move(next);
    return {true, {}};
}

MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_bchg_2b55a(
    const MillenniumAtariBchgObservation observation) {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_config_consumer_ || !native_runtime_memory_) return {false, "BCHG requires active Millennium Atari consumer"};
    const auto before = native_runtime_memory_->read_byte(
        {NativeRuntimeAddressSpace::linear, std::nullopt, observation.a2});
    if (!before || *before != observation.byte_before) return {false, "BCHG observed byte does not match native memory"};
    auto next = *millennium_atari_config_consumer_;
    auto result = next.observe_bchg_2b55a(observation);
    if (!result.accepted) return result;
    auto memory = *native_runtime_memory_;
    const auto applied = memory.apply(next.make_bchg_effect_batch("millennium-atari-bchg-" + std::to_string(observation.sequence)));
    if (!applied.accepted) return {false, applied.error};
    *millennium_atari_config_consumer_ = std::move(next);
    *native_runtime_memory_ = std::move(memory);
    return {true, {}};
}

MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::execute_millennium_atari_jsr_2b55a() {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_config_consumer_) return {false, "JSR $2b55a requires active Millennium Atari consumer"};
    auto next = *millennium_atari_config_consumer_;
    auto result = next.execute_jsr_2b55a();
    if (result.accepted) *millennium_atari_config_consumer_ = std::move(next);
    return result;
}

MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::execute_millennium_atari_bsr_2b59a() {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_config_consumer_ || !native_runtime_memory_) return {false, "BSR $2b59a requires active Millennium Atari consumer"};
    auto next = *millennium_atari_config_consumer_;
    auto result = next.execute_bsr_2b59a();
    if (!result.accepted) return result;
    auto memory = *native_runtime_memory_;
    const auto applied = memory.apply(next.make_bsr_2b59a_effect_batch("millennium-atari-bsr-2b59a"));
    if (!applied.accepted) return {false, applied.error};
    *millennium_atari_config_consumer_ = std::move(next);
    *native_runtime_memory_ = std::move(memory);
    return {true, {}};
}

MillenniumAtariConfigConsumerResult
ReleaseRuntimeCoordinator::observe_millennium_atari_d0_indexed_byte(
    const MillenniumAtariD0IndexedByteObservation observation) {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_config_consumer_ || !native_runtime_memory_) return {false, "D0-indexed byte requires active Millennium Atari consumer"};
    const auto actual = native_runtime_memory_->read_byte(
        {NativeRuntimeAddressSpace::linear, std::nullopt, observation.source_address});
    if (!actual || *actual != observation.source_byte) return {false, "D0-indexed source byte contradicts native memory"};
    auto next = *millennium_atari_config_consumer_;
    auto result = next.observe_d0_indexed_byte(observation);
    if (!result.accepted) return result;
    auto memory = *native_runtime_memory_;
    const auto applied = memory.apply(next.make_d0_indexed_effect_batch(
        "millennium-atari-indexed-" + std::to_string(observation.sequence)));
    if (!applied.accepted) return {false, applied.error};
    *millennium_atari_config_consumer_ = std::move(next);
    *native_runtime_memory_ = std::move(memory);
    return {true, {}};
}

MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::execute_millennium_atari_a1_setup() {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap || !millennium_atari_config_consumer_ || !native_runtime_memory_) return {false,"A1 setup requires active Millennium Atari consumer"};
    auto next=*millennium_atari_config_consumer_; auto result=next.execute_a1_setup(); if(!result.accepted)return result;
    auto memory=*native_runtime_memory_; const auto applied=memory.apply(next.make_a1_setup_effect_batch("millennium-atari-a1-setup")); if(!applied.accepted)return {false,applied.error};
    *millennium_atari_config_consumer_=std::move(next); *native_runtime_memory_=std::move(memory); return {true,{}};
}

MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_d0_indexed_word(const MillenniumAtariD0IndexedWordObservation o){
if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_||!native_runtime_memory_)return{false,"Indexed word requires active Millennium Atari consumer"};
const auto hi=native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,o.source_address});const auto lo=native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,o.source_address+1U});if(!hi||!lo||static_cast<std::uint16_t>((*hi<<8U)|*lo)!=o.source_word)return{false,"Indexed word contradicts native memory"};
auto next=*millennium_atari_config_consumer_;auto result=next.observe_d0_indexed_word(o);if(!result.accepted)return result;auto memory=*native_runtime_memory_;auto applied=memory.apply(next.make_d0_indexed_word_effect_batch("millennium-atari-indexed-word-"+std::to_string(o.sequence)));if(!applied.accepted)return{false,applied.error};*millennium_atari_config_consumer_=std::move(next);*native_runtime_memory_=std::move(memory);return{true,{}};}

MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_a0_indexed_word(const MillenniumAtariA0IndexedWordObservation o){
if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_||!native_runtime_memory_)return{false,"A0-indexed word requires active Millennium Atari consumer"};
const auto hi=native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,o.source_address});const auto lo=native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,o.source_address+1U});if(!hi||!lo||static_cast<std::uint16_t>((*hi<<8U)|*lo)!=o.source_word)return{false,"A0-indexed word contradicts native memory"};
auto next=*millennium_atari_config_consumer_;auto result=next.observe_a0_indexed_word(o);if(!result.accepted)return result;auto memory=*native_runtime_memory_;auto applied=memory.apply(next.make_a0_indexed_tail_effect_batch("millennium-atari-a0-indexed-"+std::to_string(o.sequence)));if(!applied.accepted)return{false,applied.error};*millennium_atari_config_consumer_=std::move(next);*native_runtime_memory_=std::move(memory);return{true,{}};}

MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::execute_millennium_atari_loop_iteration_setup(){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_||!native_runtime_memory_)return{false,"Loop setup requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.execute_loop_iteration_setup();if(!result.accepted)return result;auto memory=*native_runtime_memory_;auto applied=memory.apply(next.make_loop_iteration_setup_effect_batch("millennium-atari-loop-1"));if(!applied.accepted)return{false,applied.error};*millennium_atari_config_consumer_=std::move(next);*native_runtime_memory_=std::move(memory);return{true,{}};}

MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::execute_millennium_atari_loop_epilogue(){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_||!native_runtime_memory_)return{false,"Loop epilogue requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.execute_loop_epilogue();if(!result.accepted)return result;auto memory=*native_runtime_memory_;auto applied=memory.apply(next.make_loop_epilogue_effect_batch("millennium-atari-loop-epilogue"));if(!applied.accepted)return{false,applied.error};*millennium_atari_config_consumer_=std::move(next);*native_runtime_memory_=std::move(memory);return{true,{}};}

MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_movem_frame(const MillenniumAtariMovemFrameObservation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_)return{false,"MOVEM frame requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.observe_movem_frame(o);if(result.accepted)*millennium_atari_config_consumer_=std::move(next);return result;}

MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::execute_millennium_atari_jsr_2aa68(){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_)return{false,"JSR $2aa68 requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.execute_jsr_2aa68();if(result.accepted)*millennium_atari_config_consumer_=std::move(next);return result;}

MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_xbios_selector_38(const MillenniumAtariXbiosSelector38Observation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_)return{false,"Selector 38 requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.observe_xbios_selector_38(o);if(result.accepted)*millennium_atari_config_consumer_=std::move(next);return result;}

MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::execute_millennium_atari_jsr_2aa0c(){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_)return{false,"JSR $2aa0c requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.execute_jsr_2aa0c();if(result.accepted)*millennium_atari_config_consumer_=std::move(next);return result;}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_gemdos_selector_61(const MillenniumAtariGemdosSelector61Observation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_||!native_runtime_memory_)return{false,"GEMDOS selector 61 requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.observe_gemdos_selector_61(o);if(!result.accepted)return result;auto memory=*native_runtime_memory_;auto applied=memory.apply(next.make_gemdos_selector_61_effect_batch("millennium-atari-gemdos-61-"+std::to_string(o.sequence)));if(!applied.accepted)return{false,applied.error};*millennium_atari_config_consumer_=std::move(next);*native_runtime_memory_=std::move(memory);return{true,{}};}

RuntimeLaunchAdmission admit_runtime_launch(ReleaseRuntimeCoordinator& coordinator,
    const std::optional<LaunchRequest>& candidate, const std::vector<ReleaseArchive>& releases) {
    if (!candidate) {
        coordinator.reset();
        return {ReleaseRuntimeAdmission::identity_rejected,
            ReleaseRuntimeRejection::launch_identity};
    }
    const auto resolved = resolve_launch_request_identity(*candidate, releases);
    if (!resolved) {
        coordinator.reset();
        return {ReleaseRuntimeAdmission::identity_rejected,
            ReleaseRuntimeRejection::launch_identity};
    }
    static_cast<void>(coordinator.acquire(*resolved));
    return {coordinator.admission(), coordinator.rejection()};
}

std::unique_ptr<DeuterosAmigaOpening> load_deuteros_amiga_runtime(const ReleaseArchive& release) {
    try { return load_deuteros_amiga_runtime(VerifiedReleaseMedia::open(release)); }
    catch (...) { return {}; }
}

std::unique_ptr<DeuterosAmigaOpening> load_deuteros_amiga_runtime(const VerifiedReleaseMedia& media) {
    const auto& release = media.release();
    if (release.game != Game::deuteros || release.platform != Platform::amiga || release.language != "en") return {};
    constexpr auto clean_system_adf = "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38";
    constexpr auto clean_data_adf = "99909db1e190be02e049084743af44f00e331be6bf2d97b4831ada5fe4c30b4a";
    try {
        const auto main_stage = admit_native_code_image(media,
            "deuteros-amiga-clean-loaded-spans", "deuteros-amiga-clean-main-stage");
        const auto title_stage = admit_native_code_image(media,
            "deuteros-amiga-clean-loaded-spans", "deuteros-amiga-clean-title-handoff");
        if (!main_stage.accepted() || !title_stage.accepted()) return {};
        const auto system_image = media.extract(clean_system_adf);
        const auto data_image = media.extract(clean_data_adf);
        return system_image && data_image
            ? std::make_unique<DeuterosAmigaOpening>(std::move(*system_image), std::move(*data_image))
            : nullptr;
    } catch (...) { return {}; }
}

std::unique_ptr<DeuterosAtariBootstrapSession> load_deuteros_atari_runtime(const ReleaseArchive& release) {
    try { return load_deuteros_atari_runtime(VerifiedReleaseMedia::open(release)); }
    catch (...) { return {}; }
}

std::unique_ptr<DeuterosAtariBootstrapSession> load_deuteros_atari_runtime(const VerifiedReleaseMedia& media) {
    const auto& release = media.release();
    if (release.game != Game::deuteros || release.platform != Platform::atari_st || release.language != "en") return {};
    constexpr auto disk = "aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee";
    try {
        const auto first_stage = admit_native_code_image(media,
            "deuteros-atari-replicants-first-stage-linear",
            "deuteros-atari-replicants-first-stage");
        const auto second_stage = admit_native_code_image(media,
            "deuteros-atari-replicants-second-stage-linear",
            "deuteros-atari-replicants-second-stage");
        const auto killer_boot = admit_native_code_image(media,
            "deuteros-atari-killer-boot-linear", "deuteros-atari-killer-boot");
        if (!first_stage.accepted() || !second_stage.accepted() || !killer_boot.accepted()) return {};
        const auto image = media.extract(disk);
        return image ? std::make_unique<DeuterosAtariBootstrapSession>(std::move(*image)) : nullptr;
    } catch (...) { return {}; }
}

std::unique_ptr<MillenniumAmigaBootstrapSession> load_millennium_amiga_runtime(const ReleaseArchive& release) {
    try { return load_millennium_amiga_runtime(VerifiedReleaseMedia::open(release)); }
    catch (...) { return {}; }
}

std::unique_ptr<MillenniumAmigaBootstrapSession> load_millennium_amiga_runtime(const VerifiedReleaseMedia& media) {
    const auto& release = media.release();
    if (release.game != Game::millennium || release.platform != Platform::amiga || release.language != "en") return {};
    constexpr auto adf = "8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c";
    try {
        const auto image = media.extract(adf);
        return image ? std::make_unique<MillenniumAmigaBootstrapSession>(std::move(*image)) : nullptr;
    } catch (...) { return {}; }
}

std::unique_ptr<MillenniumAtariBootstrapSession> load_millennium_atari_runtime(const ReleaseArchive& release) {
    try { return load_millennium_atari_runtime(VerifiedReleaseMedia::open(release)); }
    catch (...) { return {}; }
}

std::unique_ptr<MillenniumAtariBootstrapSession> load_millennium_atari_runtime(const VerifiedReleaseMedia& media) {
    const auto& release = media.release();
    if (release.game != Game::millennium || release.platform != Platform::atari_st || release.language != "en") return {};
    constexpr auto disk = "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7";
    try {
        const auto image = media.extract(disk);
        if (!image) return {};
        const Fat12Disk volume{std::span<const std::uint8_t>(*image)};
        const auto* executable = volume.find("MILENIUM.TOS");
        return executable ? std::make_unique<MillenniumAtariBootstrapSession>(volume, volume.read(*executable)) : nullptr;
    } catch (...) { return {}; }
}

std::optional<MillenniumDosRuntimeAssets> load_millennium_dos_runtime(
    const ReleaseArchive& release) {
    try { return load_millennium_dos_runtime(VerifiedReleaseMedia::open(release)); }
    catch (...) { return std::nullopt; }
}

std::optional<MillenniumDosRuntimeAssets> load_millennium_dos_runtime(
    const VerifiedReleaseMedia& media) {
    const auto& release = media.release();
    // These profiles are asserted only for the selected DOS language. A
    // caller that selected Amiga, Atari ST, or an unrecognised DOS edition
    // receives no runtime object rather than a scan-order substitute.
    if (release.game != Game::millennium || release.platform != Platform::dos
        || (release.language != "en" && release.language != "es")) return std::nullopt;
    constexpr auto title_lib_sha256 =
        "6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678";
    constexpr auto gx_lib_sha256 =
        "4adf9991226deab4749ac07ad637851994f57d11f6dc45f3f5ce862b5bc34c2f";
    constexpr auto initial_save_sha256 =
        "a9b3d77534d3d575012f9553bfed9520edf92a83af408c977e7f0fd226a470e7";
    constexpr auto static_data_sha256 =
        "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d";
    constexpr auto ega640_sha256 =
        "ba003dd155fee868980f6ece933c33f9b22af68ed376cd64f4e027abd65baf6a";
    constexpr auto mcga_sha256 =
        "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228";
    constexpr auto sound_blaster_sha256 =
        "be5a00e0b71d893a3aeaaa1127b1e5b870fe734dc876e636c6a933b6444f1b72";
    constexpr auto covox_sha256 =
        "99e110b91534206a6b83680a3e11cceadd0e5ddf863560aed53dcbd2c49df7c4";
    try {
        if (release.language == "es") {
            constexpr auto spanish_image_sha256 =
                "1cb7d399ab22110317b1c7486a575c00895f12a17268d0c984ac264a5695961d";
            const auto image = media.extract(spanish_image_sha256);
            if (!image) return std::nullopt;
            const Fat12Disk disk{std::span<const std::uint8_t>(*image)};
            const auto* title_entry = disk.find("TITLE.LIB");
            const auto* titles_entry = disk.find("TITLES.EXE");
            const auto* static_data_entry = disk.find("2200AD4.BIN");
            if (!title_entry || !titles_entry || !static_data_entry) return std::nullopt;
            auto title_library_bytes = disk.read(*title_entry);
            const auto titles_bytes = disk.read(*titles_entry);
            const auto static_data = disk.read(*static_data_entry);
            static_cast<void>(parse_millennium_dos_spanish_title_presentation_evidence(
                titles_bytes, title_library_bytes));
            const MillenniumDosLib title_lib(std::move(title_library_bytes));
            const auto* p00 = title_lib.find("P00");
            if (!p00) return std::nullopt;
            const auto resource = title_lib.read(*p00);
            const auto bitmap = decode_millennium_dos_bitmap(resource);
            const auto palette = decode_millennium_dos_palette(resource, bitmap);
            return MillenniumDosRuntimeAssets{
                .title = {bitmap.width, bitmap.height,
                    {colorize_millennium_dos_bitmap(bitmap, palette)}},
                .language = "es",
                .gx_canvas = std::nullopt,
                .static_game_data = parse_millennium_dos_game_data(static_data),
                .static_data_evidence = parse_millennium_dos_static_data_evidence(static_data),
                .admitted_celestial_text = admit_all_game_text_from_source(
                    Game::millennium, Platform::dos, "2200AD4.BIN", static_data),
                .voice_bank = std::nullopt,
                .title_flow = std::nullopt,
                .sound_selection = std::nullopt,
                .sound_selection_prompt = std::nullopt,
                .sound_blaster_driver = std::nullopt,
                .covox_driver = std::nullopt,
                .spanish_title_boundary = parse_millennium_dos_spanish_title_boundary(titles_bytes),
                .game_flow = std::nullopt,
                .ega_video_driver = std::nullopt,
                .mcga_video_driver = std::nullopt,
                .initial_save = std::nullopt,
            };
        }
        const auto bytes = media.borrow(title_lib_sha256);
        if (!bytes) return std::nullopt;
        const MillenniumDosLib title_lib(*bytes);
        const auto gx_bytes = media.borrow(gx_lib_sha256);
        const auto titles_code = admit_native_code_image(media,
            "millennium-dos-titles-exe-linear", "millennium-dos-title-flow");
        const auto launcher_code = admit_native_code_image(media,
            "millennium-dos-mill-com-linear", "millennium-dos-launcher");
        const auto game_code = admit_native_code_image(media,
            "millennium-dos-2200ad-exe-linear", "millennium-dos-game-flow");
        const auto initial_save = media.borrow(initial_save_sha256);
        const auto static_data = media.borrow(static_data_sha256);
        const auto ega640 = media.borrow(ega640_sha256);
        const auto mcga = media.borrow(mcga_sha256);
        const auto sound_blaster = media.borrow(sound_blaster_sha256);
        const auto covox = media.borrow(covox_sha256);
        if (!gx_bytes || !titles_code.accepted() || !launcher_code.accepted()
            || !game_code.accepted() || !initial_save || !static_data || !ega640 || !mcga
            || !sound_blaster || !covox) {
            return std::nullopt;
        }
        // The live presentation admission deliberately uses the same
        // complete, hash-locked P00/P01..P25 model that inspection reports.
        // This keeps SDL from accepting an independently decoded P00 after a
        // later title-patch/profile boundary has ceased to match original
        // media. It still exposes only P00 as the static title frame: patch
        // composition, cadence, title input, and the DOS hand-off remain
        // unproven.
        const auto title_flow = parse_millennium_dos_title_flow(
            titles_code.view->bytes, launcher_code.view->bytes);
        const auto title_presentation = parse_millennium_dos_title_presentation_assets(
            title_lib, title_flow);
        const auto gx_canvas = parse_millennium_dos_gameplay_screen(*gx_bytes);
        const auto sound_selection = parse_millennium_dos_sound_selection(launcher_code.view->bytes);
        const auto sound_selection_prompt = extract_millennium_dos_sound_selection_prompt(
            launcher_code.view->bytes, sound_selection);
        return MillenniumDosRuntimeAssets{
            .title = {title_presentation.base_bitmap.width, title_presentation.base_bitmap.height,
                {title_presentation.base_rgba}},
            .language = "en",
            .gx_canvas = MillenniumDosPreviewAnimation{
                gx_canvas.canvas.width, gx_canvas.canvas.height, {gx_canvas.rgba}},
            .static_game_data = parse_millennium_dos_game_data(*static_data),
            .static_data_evidence = parse_millennium_dos_static_data_evidence(*static_data),
            .admitted_celestial_text = admit_all_game_text_from_source(
                Game::millennium, Platform::dos, "2200AD4.BIN", *static_data),
            .voice_bank = parse_millennium_dos_voice_bank(media),
            .title_flow = title_flow,
            .sound_selection = sound_selection,
            .sound_selection_prompt = sound_selection_prompt,
            .sound_blaster_driver = admit_millennium_dos_sound_driver_leaf(*sound_blaster),
            .covox_driver = admit_millennium_dos_sound_driver_leaf(*covox),
            .spanish_title_boundary = std::nullopt,
            .game_flow = parse_millennium_dos_game_flow(game_code.view->bytes),
            .ega_video_driver = parse_millennium_dos_video_driver(*ega640,
                MillenniumDosVideoDriverKind::ega640),
            .mcga_video_driver = parse_millennium_dos_video_driver(*mcga,
                MillenniumDosVideoDriverKind::mcga),
            .initial_save = MillenniumDosSaveSession(*initial_save),
        };
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace eon
