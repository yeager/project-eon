#include "engine/release_runtime.hpp"

#include "platform/game_data.hpp"
#include "data/fat12.hpp"
#include "data/millennium_dos_bitmap.hpp"
#include "data/millennium_dos_gameplay_screen.hpp"
#include "data/millennium_dos_lib.hpp"

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
        return false;
    }
    std::optional<VerifiedReleaseMedia> media;
    try {
        media = VerifiedReleaseMedia::open(launch.release);
    } catch (...) {
        admission_ = ReleaseRuntimeAdmission::archive_rejected;
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
    std::unique_ptr<DeuterosAtariBootstrapSession> deuteros_atari;
    std::optional<RuntimeSessionSnapshot> session_snapshot;
    switch (launch.release.game) {
    case Game::millennium:
        switch (launch.release.platform) {
        case Platform::dos:
            millennium_dos = load_millennium_dos_runtime(*media);
            if (millennium_dos) session_snapshot = make_runtime_session_snapshot(launch,
                RuntimeSessionKind::millennium_dos_title);
            break;
        case Platform::amiga:
            millennium_amiga = load_millennium_amiga_runtime(*media);
            if (millennium_amiga) session_snapshot = make_runtime_session_snapshot(launch,
                RuntimeSessionKind::millennium_amiga_bootstrap);
            break;
        case Platform::atari_st:
            millennium_atari = load_millennium_atari_runtime(*media);
            if (millennium_atari) session_snapshot = make_runtime_session_snapshot(launch,
                RuntimeSessionKind::millennium_atari_bootstrap);
            break;
        }
        break;
    case Game::deuteros:
        switch (launch.release.platform) {
        case Platform::dos: break;
        case Platform::amiga:
            deuteros_amiga = load_deuteros_amiga_runtime(*media);
            if (deuteros_amiga) session_snapshot = make_runtime_session_snapshot(launch,
                RuntimeSessionKind::deuteros_amiga_opening);
            break;
        case Platform::atari_st:
            deuteros_atari = load_deuteros_atari_runtime(*media);
            if (deuteros_atari) session_snapshot = make_runtime_session_snapshot(launch,
                RuntimeSessionKind::deuteros_atari_bootstrap);
            break;
        }
        break;
    }
    if (!session_snapshot || (!millennium_dos && !millennium_amiga && !millennium_atari
        && !deuteros_amiga && !deuteros_atari)) {
        admission_ = ReleaseRuntimeAdmission::adapter_rejected;
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
        return false;
    }
    millennium_dos_ = std::move(millennium_dos);
    millennium_dos_sound_selection_ = std::move(millennium_dos_sound_selection);
    millennium_dos_title_ = std::move(millennium_dos_title);
    millennium_amiga_ = std::move(millennium_amiga);
    millennium_atari_ = std::move(millennium_atari);
    deuteros_amiga_ = std::move(deuteros_amiga);
    deuteros_atari_ = std::move(deuteros_atari);
    session_snapshot_ = std::move(session_snapshot);
    active_ = launch;
    admission_ = ReleaseRuntimeAdmission::active;
    return true;
}

void ReleaseRuntimeCoordinator::reset() {
    millennium_dos_sound_selection_.reset();
    millennium_dos_title_.reset();
    millennium_dos_.reset();
    millennium_amiga_.reset();
    millennium_atari_.reset();
    deuteros_amiga_.reset();
    deuteros_amiga_opening_input_held_ = false;
    deuteros_atari_.reset();
    session_snapshot_.reset();
    active_.reset();
    admission_ = ReleaseRuntimeAdmission::unselected;
}

RuntimeInputDisposition ReleaseRuntimeCoordinator::observe_input(
    const RuntimeInputObservation& observation) {
    // Do not accept a generic input event just because a session is active.
    // The two branches below are the complete, typed evidence currently
    // admitted for Millennium DOS; all other adapters fail closed.
    if (!session_snapshot_) {
        return RuntimeInputDisposition::rejected;
    }
    if (session_snapshot_->kind == RuntimeSessionKind::deuteros_amiga_opening) {
        if (observation.kind != RuntimeInputObservationKind::opening_input_held) {
            return RuntimeInputDisposition::rejected;
        }
        deuteros_amiga_opening_input_held_ = observation.ascii_character != '\0';
        return RuntimeInputDisposition::observed;
    }
    if (session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title) {
        return RuntimeInputDisposition::rejected;
    }
    if (observation.kind == RuntimeInputObservationKind::opening_input_held) {
        return RuntimeInputDisposition::rejected;
    }
    if (observation.kind == RuntimeInputObservationKind::ascii_character) {
        if (!millennium_dos_sound_selection_) return RuntimeInputDisposition::rejected;
        return millennium_dos_sound_selection_->accept_ascii_character(observation.ascii_character)
            ? RuntimeInputDisposition::boundary_reached : RuntimeInputDisposition::ignored;
    }
    if (!millennium_dos_title_) return RuntimeInputDisposition::rejected;
    return millennium_dos_title_->poll_console(true)
        ? RuntimeInputDisposition::boundary_reached : RuntimeInputDisposition::ignored;
}

std::optional<DeuterosAmigaVmEvents> ReleaseRuntimeCoordinator::tick_deuteros_amiga_opening() {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_opening
        || !deuteros_amiga_ || deuteros_amiga_->title_handed_off()) return std::nullopt;
    auto events = deuteros_amiga_->tick(deuteros_amiga_opening_input_held_);
    if (events.title_handoff) {
        // The opening object remains owner of the original ADF/title-stage
        // evidence, but the live session has crossed its last recovered VBL
        // frame. Publish the narrower state atomically so diagnostics and
        // input routing cannot describe or tick a completed opening.
        if (!active_ || !deuteros_amiga_->title_stage_session()) return std::nullopt;
        session_snapshot_ = make_runtime_session_snapshot(*active_,
            RuntimeSessionKind::deuteros_amiga_title_stage);
        deuteros_amiga_opening_input_held_ = false;
    }
    return events;
}

RuntimeLaunchAdmission admit_runtime_launch(ReleaseRuntimeCoordinator& coordinator,
    const std::optional<LaunchRequest>& candidate, const std::vector<ReleaseArchive>& releases) {
    if (!candidate) {
        coordinator.reset();
        return {ReleaseRuntimeAdmission::identity_rejected};
    }
    const auto resolved = resolve_launch_request_identity(*candidate, releases);
    if (!resolved) {
        coordinator.reset();
        return {ReleaseRuntimeAdmission::identity_rejected};
    }
    static_cast<void>(coordinator.acquire(*resolved));
    return {coordinator.admission()};
}

std::unique_ptr<DeuterosAmigaOpening> load_deuteros_amiga_runtime(const ReleaseArchive& release) {
    try { return load_deuteros_amiga_runtime(VerifiedReleaseMedia::open(release)); }
    catch (...) { return {}; }
}

std::unique_ptr<DeuterosAmigaOpening> load_deuteros_amiga_runtime(const VerifiedReleaseMedia& media) {
    const auto& release = media.release();
    if (release.game != Game::deuteros || release.platform != Platform::amiga || release.language != "en") return {};
    constexpr auto clean_system_adf = "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38";
    try {
        const auto image = media.extract(clean_system_adf);
        return image ? std::make_unique<DeuterosAmigaOpening>(std::move(*image)) : nullptr;
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
        const Fat12Disk volume(*image);
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
            const Fat12Disk disk(*image);
            const auto* title_entry = disk.find("TITLE.LIB");
            const auto* titles_entry = disk.find("TITLES.EXE");
            if (!title_entry || !titles_entry) return std::nullopt;
            auto title_library_bytes = disk.read(*title_entry);
            const auto titles_bytes = disk.read(*titles_entry);
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
        const auto bytes = media.extract(title_lib_sha256);
        if (!bytes) return std::nullopt;
        const MillenniumDosLib title_lib(*bytes);
        const auto* p00 = title_lib.find("P00");
        if (!p00) return std::nullopt;
        const auto resource = title_lib.read(*p00);
        const auto bitmap = decode_millennium_dos_bitmap(resource);
        const auto palette = decode_millennium_dos_palette(resource, bitmap);
        const auto gx_bytes = media.extract(gx_lib_sha256);
        const auto titles = media.extract(titles_sha256);
        const auto launcher = media.extract(launcher_sha256);
        const auto game = media.extract(game_sha256);
        const auto initial_save = media.extract(initial_save_sha256);
        const auto ega640 = media.extract(ega640_sha256);
        const auto mcga = media.extract(mcga_sha256);
        const auto sound_blaster = media.extract(sound_blaster_sha256);
        const auto covox = media.extract(covox_sha256);
        if (!gx_bytes || !titles || !launcher || !game || !initial_save || !ega640 || !mcga
            || !sound_blaster || !covox) {
            return std::nullopt;
        }
        const auto gx_canvas = parse_millennium_dos_gameplay_screen(*gx_bytes);
        const auto sound_selection = parse_millennium_dos_sound_selection(*launcher);
        const auto sound_selection_prompt = extract_millennium_dos_sound_selection_prompt(
            *launcher, sound_selection);
        return MillenniumDosRuntimeAssets{
            .title = {bitmap.width, bitmap.height,
                {colorize_millennium_dos_bitmap(bitmap, palette)}},
            .language = "en",
            .gx_canvas = MillenniumDosPreviewAnimation{
                gx_canvas.canvas.width, gx_canvas.canvas.height, {gx_canvas.rgba}},
            .title_flow = parse_millennium_dos_title_flow(*titles, *launcher),
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
