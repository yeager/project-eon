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
    millennium_dos_sound_selection_.reset();
    millennium_dos_title_.reset();
    millennium_dos_.reset();
    millennium_amiga_.reset();
    millennium_atari_.reset();
    deuteros_amiga_paula_.reset();
    deuteros_amiga_.reset();
    deuteros_amiga_opening_input_held_ = false;
    deuteros_atari_.reset();
    session_snapshot_.reset();
    active_.reset();
    admission_ = ReleaseRuntimeAdmission::unselected;
    rejection_ = ReleaseRuntimeRejection::none;
}

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
    return MillenniumDosStaticDispatchDiagnostics{
        .action_poll_address = flow.action_poll_address,
        .first_action = flow.function_key_first_action,
        .action_count = flow.function_key_count,
        .table_address = flow.function_key_table_address,
        .table_stride = flow.function_key_table_stride,
        .dispatch_address = flow.function_key_dispatch_address,
        .handler_addresses = {flow.first_function_key.handler_address,
            flow.second_function_key.handler_address, flow.third_function_key.handler_address,
            flow.fourth_function_key.handler_address, flow.fifth_function_key.handler_address,
            flow.sixth_function_key.handler_address, flow.seventh_function_key.handler_address,
            flow.eighth_function_key.handler_address, flow.ninth_function_key.handler_address,
            flow.tenth_function_key.handler_address},
    };
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
