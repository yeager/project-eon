#include "engine/release_runtime.hpp"
#include "engine/release_runtime_capability.hpp"

#include "platform/game_data.hpp"
#include "data/reference_trace.hpp"
#include "data/reference_trace_registry.hpp"
#include "data/sha256.hpp"
#include "data/fat12.hpp"
#include "data/function_map.hpp"
#include "data/millennium_dos_bitmap.hpp"
#include "data/millennium_dos_gameplay_screen.hpp"
#include "data/millennium_dos_lib.hpp"
#include "data/millennium_dos_title_presentation.hpp"

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
    std::unique_ptr<MillenniumAtariBootstrapSession> millennium_atari;
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
        constexpr std::string_view game_sha256 =
            "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
        const auto game = media->borrow(game_sha256);
        if (!game) {
            admission_ = ReleaseRuntimeAdmission::adapter_rejected;
            rejection_ = ReleaseRuntimeRejection::adapter_construction;
            return false;
        }
        auto prepared = MillenniumDosNativeProcessAdmission::startup(
            launch.release.sha256, *game);
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
    millennium_dos_ = std::move(millennium_dos);
    millennium_dos_sound_selection_ = std::move(millennium_dos_sound_selection);
    millennium_dos_title_ = std::move(millennium_dos_title);
    millennium_dos_native_process_ = std::move(millennium_dos_native_process);
    millennium_amiga_ = std::move(millennium_amiga);
    millennium_atari_ = std::move(millennium_atari);
    deuteros_amiga_ = std::move(deuteros_amiga);
    deuteros_amiga_paula_ = std::move(deuteros_amiga_paula);
    deuteros_atari_ = std::move(deuteros_atari);
    session_snapshot_ = std::move(session_snapshot);
    active_ = launch;
    admission_ = ReleaseRuntimeAdmission::active;
    rejection_ = ReleaseRuntimeRejection::none;
    return true;
}

void ReleaseRuntimeCoordinator::reset() {
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
    millennium_dos_bdf_service_.reset();
    millennium_dos_second_function_callback_transfer_.reset();
    millennium_dos_gx_startup_.reset();
    millennium_dos_post_overlay_loop_.reset();
    millennium_dos_native_process_.reset();
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
#undef EON_BDF_FORWARD
MillenniumDosBdfObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_bdf_external_return(const MillenniumDosBdfExternalReturnObservation o){MillenniumDosBdfObservationResult r;if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_second_function_callback||!millennium_dos_bdf_service_||!millennium_dos_second_function_callback_transfer_||millennium_dos_bdf_service_->boundary().kind!=MillenniumDosBdfServiceBoundaryKind::local_return){r.error="$0bdf external return requires its active local RET boundary";return r;}const auto admitted=millennium_dos_second_function_callback_transfer_->observe_return({o.sequence,o.return_instruction,o.returned_to});r.accepted=admitted.accepted;r.error=admitted.error;return r;}
std::optional<MillenniumDosBdfCheckpoint>ReleaseRuntimeCoordinator::millennium_dos_bdf_checkpoint()const{if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_second_function_callback||!millennium_dos_bdf_service_||!millennium_dos_second_function_callback_transfer_)return std::nullopt;const auto&s=*millennium_dos_bdf_service_;return MillenniumDosBdfCheckpoint{s.state(),s.boundary(),s.effects(),millennium_dos_second_function_callback_transfer_->checkpoint()};}

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
    session_snapshot_ = make_runtime_session_snapshot(*active_,
        RuntimeSessionKind::millennium_dos_title_handoff_boundary);
    return RuntimeInputDisposition::boundary_reached;
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

std::optional<MillenniumAtariBootstrapPresentationSnapshot>
ReleaseRuntimeCoordinator::millennium_atari_bootstrap_presentation() const {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_) return std::nullopt;
    return MillenniumAtariBootstrapPresentationSnapshot{
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
    constexpr auto titles_sha256 =
        "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
    constexpr auto launcher_sha256 =
        "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e";
    constexpr auto game_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
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
        const auto titles = media.borrow(titles_sha256);
        const auto launcher = media.borrow(launcher_sha256);
        const auto game = media.borrow(game_sha256);
        const auto initial_save = media.borrow(initial_save_sha256);
        const auto static_data = media.borrow(static_data_sha256);
        const auto ega640 = media.borrow(ega640_sha256);
        const auto mcga = media.borrow(mcga_sha256);
        const auto sound_blaster = media.borrow(sound_blaster_sha256);
        const auto covox = media.borrow(covox_sha256);
        if (!gx_bytes || !titles || !launcher || !game || !initial_save || !static_data || !ega640 || !mcga
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
        const auto title_flow = parse_millennium_dos_title_flow(*titles, *launcher);
        const auto title_presentation = parse_millennium_dos_title_presentation_assets(
            title_lib, title_flow);
        const auto gx_canvas = parse_millennium_dos_gameplay_screen(*gx_bytes);
        const auto sound_selection = parse_millennium_dos_sound_selection(*launcher);
        const auto sound_selection_prompt = extract_millennium_dos_sound_selection_prompt(
            *launcher, sound_selection);
        return MillenniumDosRuntimeAssets{
            .title = {title_presentation.base_bitmap.width, title_presentation.base_bitmap.height,
                {title_presentation.base_rgba}},
            .language = "en",
            .gx_canvas = MillenniumDosPreviewAnimation{
                gx_canvas.canvas.width, gx_canvas.canvas.height, {gx_canvas.rgba}},
            .static_game_data = parse_millennium_dos_game_data(*static_data),
            .static_data_evidence = parse_millennium_dos_static_data_evidence(*static_data),
            .voice_bank = parse_millennium_dos_voice_bank(media),
            .title_flow = title_flow,
            .sound_selection = sound_selection,
            .sound_selection_prompt = sound_selection_prompt,
            .sound_blaster_driver = admit_millennium_dos_sound_driver_leaf(*sound_blaster),
            .covox_driver = admit_millennium_dos_sound_driver_leaf(*covox),
            .spanish_title_boundary = std::nullopt,
            .game_flow = parse_millennium_dos_game_flow(*game),
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
