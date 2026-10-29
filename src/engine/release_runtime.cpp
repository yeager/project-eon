#include "engine/release_runtime.hpp"
#include "engine/release_runtime_capability.hpp"
#include "engine/deuteros_amiga_owned_alternate_renderer.hpp"
#include "engine/deuteros_amiga_owned_commands.hpp"
#include "engine/deuteros_amiga_owned_optional_resource.hpp"
#include "engine/millennium_dos_game_function_four_dispatch.hpp"
#include "engine/millennium_dos_file_read_session.hpp"

#include "platform/game_data.hpp"
#include "data/reference_trace.hpp"
#include "data/reference_trace_registry.hpp"
#include "data/sha256.hpp"
#include "data/fat12.hpp"
#include "data/function_map.hpp"
#include "data/native_code_image_admission.hpp"
#include "data/m68k_executor.hpp"
#include "data/millennium_dos_bitmap.hpp"
#include "data/millennium_dos_gameplay_screen.hpp"
#include "data/millennium_dos_lib.hpp"
#include "data/millennium_dos_title_presentation.hpp"
#include "data/millennium_dos_video_driver.hpp"
#include "engine/millennium_dos_title_p00_recompilation.hpp"
#include "engine/millennium_dos_video_function_six_mcga_copy_loop_session.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

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

MillenniumDosOwnedRuntimeByteRequest derive_millennium_dos_owned_runtime_byte_request(
    const MillenniumDosNativeProcessCheckpoint& process,
    const NativeRuntimeMemory& owned_memory, const std::uint64_t sequence_floor) {
    MillenniumDosOwnedRuntimeByteRequest result;
    constexpr std::string_view release_sha256 =
        "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123";
    constexpr std::string_view game_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    constexpr std::uint16_t instruction_address = 0xd2fd;
    constexpr std::uint16_t source_address = 0x0082;
    if (process.release_sha256 != release_sha256
        || process.game_executable_sha256 != game_sha256
        || process.recovery_entry != MillenniumDosNativeRecoveryEntry::startup
        || !process.observed_child_process_entry || process.child_entry_sequence == 0
        || process.child_code_segment == 0
        || process.state
            != MillenniumDosNativeProcessState::startup_zero_path_continuation_byte_boundary
        || process.boundary.kind != MillenniumDosNativeBoundaryKind::runtime_byte
        || process.boundary.address != instruction_address
        || process.boundary.runtime_byte_address != source_address) {
        result.error = "Owned-byte request requires the exact authenticated $d2fd read boundary";
        return result;
    }
    const auto value = owned_memory.read_byte({NativeRuntimeAddressSpace::dos_segmented,
        process.child_code_segment, source_address});
    if (!value) {
        result.error = "DOS child CS:$0082 is absent from owned runtime memory";
        return result;
    }
    if (!owned_memory.read_byte({NativeRuntimeAddressSpace::dos_segmented,
            process.child_code_segment, 0x0122})) {
        result.error = "DOS child selector store target CS:$0122 is absent from owned runtime memory";
        return result;
    }
    const auto latest_sequence = std::max({process.last_observation_sequence,
        sequence_floor, process.child_entry_sequence});
    if (latest_sequence == std::numeric_limits<std::uint64_t>::max()) {
        result.error = "Owned DOS runtime-byte observation sequence is exhausted";
        return result;
    }
    result.accepted = true;
    result.observation = MillenniumDosNativeRuntimeByteObservation{
        latest_sequence + 1, instruction_address, source_address, *value};
    return result;
}

namespace {

// Some native main-stage waits are represented by lifecycle state before a
// detailed dependency checkpoint receives its next externally observed call.
// Keep their instruction addresses in one audited map so the SDL/front-end
// diagnostic reports a useful boundary without turning it into an emulated
// service or an inferred input event.
[[nodiscard]] std::uint32_t main_stage_external_stop_address(
    const DeuterosAmigaMainStageState state) {
    switch (state) {
    case DeuterosAmigaMainStageState::awaiting_2099e_exec_return: return 0x2099e;
    case DeuterosAmigaMainStageState::awaiting_209ca_exec_return: return 0x209ca;
    case DeuterosAmigaMainStageState::awaiting_209f0_exec_return: return 0x209f0;
    case DeuterosAmigaMainStageState::terminal_209fa_spin: return 0x209fa;
    case DeuterosAmigaMainStageState::awaiting_cia_a_bit_set: return 0x217e4;
    case DeuterosAmigaMainStageState::awaiting_initial_resource_load: return 0x21926;
    case DeuterosAmigaMainStageState::awaiting_initial_loop_service_return: return 0x217f8;
    case DeuterosAmigaMainStageState::awaiting_first_loop_graphics_return: return 0x21310;
    case DeuterosAmigaMainStageState::awaiting_second_loop_graphics_return: return 0x2132a;
    case DeuterosAmigaMainStageState::awaiting_loop_request_service: return 0x208b4;
    case DeuterosAmigaMainStageState::awaiting_optional_resource_cia: return 0x22330;
    default: return 0;
    }
}

[[nodiscard]] std::uint32_t main_stage_stop_address(
    const DeuterosAmigaTitleDependencyChainCheckpoint& checkpoint) {
    return checkpoint.stop_before_address != 0
        ? checkpoint.stop_before_address
        : main_stage_external_stop_address(checkpoint.main_stage_state);
}

struct DeuterosCiaPrefixResult {
    std::uint32_t data_register_zero = 0;
    std::uint8_t port_value = 0;
    std::uint16_t status_register = 0;
    std::uint32_t stop_address = 0;
    std::uint64_t instructions_executed = 0;
};

[[nodiscard]] DeuterosCiaPrefixResult execute_deuteros_cia_prefix(
    const std::span<const std::uint8_t> code, const std::uint8_t prior_port_value,
    const std::uint16_t source_word, const std::uint32_t initial_d0 = 0,
    const std::uint16_t initial_sr = 0) {
    constexpr std::uint32_t stop_address = 0x217f2;
    constexpr std::string_view source_hash =
        "cb9046ad20fffc0a52f43431949927b4d159ecc795fe5f62dbbd01accd0684c7";
    if (code.size() != 26U || to_hex(sha256(code)) != source_hash) {
        throw std::runtime_error("Deuteros CIA prefix source is unavailable");
    }
    const auto expected_port = static_cast<std::uint8_t>(prior_port_value | 0x02U);
    auto expected_sr = static_cast<std::uint16_t>(initial_sr & 0xfff0U);
    if ((source_word & 0x8000U) != 0U) expected_sr |= 0x0008U;
    if (source_word == 0U) expected_sr |= 0x0004U;
    // The first instruction BSETs bit 1 in the observed CIA-A byte. MOVE.W
    // then replaces D0's low word and condition codes, preserving D0's high
    // word, X, and the upper SR. Stop before the external BSR at $217f2.
    const auto data_register_zero = (initial_d0 & 0xffff0000U) | source_word;
    return {data_register_zero, expected_port, expected_sr, stop_address, 2};
}

} // namespace

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
    // A recognised original is not automatically a runnable target.  This
    // explicit gate retains Spanish Millennium DOS for preservation scans
    // while ensuring no parser, title pixels, input path, or session can be
    // reached through the native runtime by accident.
    if (capability->adapter == ReleaseRuntimeAdapter::preservation_only) {
        admission_ = ReleaseRuntimeAdmission::adapter_rejected;
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
    std::optional<MillenniumAtariPostConfigEntrySession> millennium_atari_post_config_entry;
    std::unique_ptr<DeuterosAmigaOpening> deuteros_amiga;
    std::unique_ptr<DeuterosAmigaPaulaMixer> deuteros_amiga_paula;
    std::optional<DeuterosAmigaEonMemoryArena> deuteros_amiga_memory_arena;
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
    case ReleaseRuntimeAdapter::preservation_only:
        // Kept for exhaustiveness. The gate above must reject before media
        // adapters are constructed.
        return false;
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
                *millennium_dos->sound_selection, millennium_dos->ibm_speaker_driver,
                millennium_dos->sound_blaster_driver,
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
        if(deuteros_amiga){
            // The verified boot block reads the complete first track to this
            // exact address before entering its loader. Retain that entire
            // source-backed image, including its genuinely zero $12ff4 cell;
            // later loader instructions own every subsequent mutation.
            const auto& stage=deuteros_amiga->bootstrap_load_stage();
            const auto bytes=deuteros_amiga->bootstrap_load_bytes();
            constexpr std::uint32_t source_offset=0x2c00;
            constexpr std::uint32_t length=0x1600;
            constexpr std::uint32_t destination=0x12800;
            constexpr std::uint32_t entry=0x12a4e;
            constexpr std::string_view source_sha256=
                "c3f7061b5bd1a9a762b8fff509b8406d51133775d6685c9fe1d66415c4507364";
            if(stage.disk_offset!=source_offset||stage.length!=length
                ||stage.destination!=destination||stage.entry_address!=entry
                ||bytes.size()!=length||to_hex(sha256(bytes))!=source_sha256)
                throw std::runtime_error("Deuteros bootstrap track is not the complete boot-block load");
            NativeRuntimeEffectBatch batch{"deuteros-amiga-bootstrap-track",true,{}};
            batch.effects.reserve(bytes.size());
            for(std::uint32_t i=0;i<bytes.size();++i)
                batch.effects.push_back({i+1,{NativeRuntimeAddressSpace::linear,std::nullopt,destination+i},
                    MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,
                    bytes[i]});
            const auto applied=runtime_memory.apply(batch);
            if(!applied.accepted)throw std::runtime_error(applied.error);
        }
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
            const auto screen_applied = runtime_memory.apply(
                millennium_atari->make_native_screen_buffer_effect_batch(
                    "millennium-atari-1-low-resolution-screen"));
            if (!screen_applied.accepted) throw std::runtime_error(screen_applied.error);
            const auto config_applied = runtime_memory.apply(
                millennium_atari->read_only_gemdos().make_fread_effect_batch(
                    "millennium-atari-1-config"));
            if (!config_applied.accepted) throw std::runtime_error(config_applied.error);
            millennium_atari_config_consumer.emplace(1, runtime_memory,
                millennium_atari->read_only_gemdos().checkpoint(),
                millennium_atari->fread_config_load_address_boundary(),
                millennium_atari->fread_mapped_config_prelude());
            millennium_atari_post_config_entry.emplace(1,
                millennium_atari->post_config_module_entry(),
                millennium_atari->post_config_payload());
        }
    } catch (const std::exception& error) {
        const std::string detail = error.what();
        reset(); admission_ = ReleaseRuntimeAdmission::adapter_rejected;
        rejection_ = ReleaseRuntimeRejection::child_session;
        rejection_detail_ = detail;
        return false;
    } catch (...) {
        reset(); admission_ = ReleaseRuntimeAdmission::adapter_rejected;
        rejection_ = ReleaseRuntimeRejection::child_session;
        rejection_detail_ = "Unknown child-session construction failure";
        return false;
    }
    if (runtime_generation_counter_ == std::numeric_limits<std::uint64_t>::max()) {
        admission_ = ReleaseRuntimeAdmission::adapter_rejected;
        rejection_ = ReleaseRuntimeRejection::lifecycle_transition;
        return false;
    }
    if (deuteros_amiga) {
        try {
            deuteros_amiga_memory_arena.emplace(runtime_generation_counter_ + 1U);
        } catch (...) {
            admission_ = ReleaseRuntimeAdmission::adapter_rejected;
            rejection_ = ReleaseRuntimeRejection::adapter_construction;
            return false;
        }
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
    millennium_atari_post_config_entry_ = std::move(millennium_atari_post_config_entry);
    deuteros_amiga_ = std::move(deuteros_amiga);
    deuteros_amiga_paula_ = std::move(deuteros_amiga_paula);
    deuteros_amiga_eon_memory_arena_ = std::move(deuteros_amiga_memory_arena);
    deuteros_atari_ = std::move(deuteros_atari);
    session_snapshot_ = std::move(session_snapshot);
    active_ = launch;
    active_media_ = std::move(media);
    native_runtime_memory_=std::move(runtime_memory);
    active_runtime_generation_ = ++runtime_generation_counter_;
    admission_ = ReleaseRuntimeAdmission::active;
    rejection_ = ReleaseRuntimeRejection::none;
    rejection_detail_.clear();
    return true;
}

void ReleaseRuntimeCoordinator::reset() {
    active_runtime_generation_ = 0;
    deuteros_amiga_trackdisk_compatibility_.reset();
    deuteros_amiga_exec_compatibility_.reset();
    deuteros_amiga_exec_compatibility_graph_.reset();
    deuteros_amiga_exec_task_.reset();
    deuteros_amiga_exec_first_port_.reset();
    deuteros_amiga_exec_second_port_.reset();
    deuteros_amiga_exec_unit0_open_.reset();
    deuteros_amiga_exec_unit1_open_.reset();
    deuteros_amiga_trackdisk_task_.reset();
    deuteros_amiga_trackdisk_unit0_binding_.reset();
    deuteros_amiga_trackdisk_unit1_binding_.reset();
    deuteros_amiga_input_device_compatibility_.reset();
    deuteros_amiga_graphics_library_compatibility_.reset();
    deuteros_amiga_makevport_prefix_.reset();
    deuteros_amiga_eon_memory_arena_.reset();
    deuteros_amiga_native_display_surface_.reset();
    deuteros_amiga_native_display_initialization_.reset();
    deuteros_amiga_native_input_handler_registration_.reset();
    deuteros_amiga_title_native_service_setup_plan_.reset();
    deuteros_amiga_title_host_input_sequence_ = 0;
    deuteros_amiga_interrupt_worker_last_sequence_ = 0;
    deuteros_amiga_vertb_session_.reset();
    deuteros_amiga_bootstrap_frame_.reset();
    deuteros_amiga_bootstrap_frame_generation_ = 0;
    deuteros_amiga_main_stage_palette_.reset();
    deuteros_amiga_pending_main_stage_frame_.reset();
    deuteros_amiga_main_stage_frame_.reset();
    deuteros_amiga_main_stage_frame_generation_ = 0;
    deuteros_amiga_title_program_entry_.reset();
    deuteros_amiga_bootstrap_graphics_boundary_.reset();
    deuteros_amiga_title_computed_dispatch_.reset();
    deuteros_amiga_native_audio_checkpoint_.reset();
    deuteros_amiga_native_audio_mixer_.reset();
    deuteros_amiga_native_audio_generation_=0;
    deuteros_amiga_disk_transition_.reset();
    native_runtime_memory_.reset();
    millennium_atari_config_consumer_.reset();
    millennium_atari_post_config_entry_.reset();
    millennium_dos_title_to_game_.reset();
    millennium_dos_title_to_game_generation_ = 0;
    millennium_dos_title_to_game_last_sequence_ = 0;
    millennium_dos_sibm_native_service_.reset();
    millennium_amiga_relocator_.reset();
    millennium_amiga_relocator_generation_=0;
    millennium_amiga_relocator_overread_sequence_.reset();
    millennium_amiga_relocator_terminal_sequence_.reset();
    millennium_amiga_setup_return_sequence_.reset();
    millennium_amiga_first_read_return_sequence_.reset();
    millennium_amiga_first_stage_entry_sequence_.reset();
    millennium_amiga_first_stage_illegal_sequence_.reset();
    millennium_amiga_second_illegal_sequence_.reset();
    millennium_amiga_first_trace_sequence_.reset();
    millennium_amiga_trace_branch_chain_sequence_.reset();
    millennium_amiga_trace_register_prefix_sequence_.reset();
    millennium_amiga_bus_error_prefix_sequence_.reset();
    millennium_amiga_custom_chip_exec_prefix_sequence_.reset();
    millennium_amiga_exec_transition_sequence_.reset();
    millennium_amiga_open_graphics_sequence_.reset();
    millennium_amiga_allocation_sequence_.reset();
    millennium_amiga_graphics_initialization_sequence_.reset();
    millennium_amiga_view_service_sequence_.reset();
    millennium_amiga_interrupt_control_sequence_.reset();
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
    millennium_dos_static_data_load_.reset();
    millennium_dos_sound_driver_load_.reset();
    millennium_dos_video_driver_load_.reset();
    millennium_dos_video_driver_load_generation_ = 0;
    millennium_dos_video_driver_load_last_sequence_ = 0;
    millennium_dos_video_int91_dispatch_.reset();
    millennium_dos_video_function_zero_return_chain_completed_ = false;
    millennium_dos_video_int91_dispatch_generation_ = 0;
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
    deuteros_amiga_title_native_service_setup_plan_.reset();
    deuteros_amiga_.reset();
    deuteros_amiga_opening_input_held_ = false;
    deuteros_atari_.reset();
    session_snapshot_.reset();
    active_media_.reset();
    active_.reset();
    admission_ = ReleaseRuntimeAdmission::unselected;
    rejection_ = ReleaseRuntimeRejection::none;
    rejection_detail_.clear();
}

MillenniumAmigaBootstrapRelocatorObservationResult
ReleaseRuntimeCoordinator::observe_millennium_amiga_first_stage_entry(
    const MillenniumAmigaFirstStageEntryObservation observation) {
    MillenniumAmigaBootstrapRelocatorObservationResult result;
    if (!millennium_amiga_relocator_ || !native_runtime_memory_
        || !millennium_amiga_first_read_return_sequence_
        || millennium_amiga_first_stage_entry_sequence_
        || observation.sequence <= *millennium_amiga_first_read_return_sequence_) {
        result.error = "First-stage entry requires the later materialized Defjam stage";
        return result;
    }
    try {
        const auto opcode = native_runtime_memory_->read_byte(
            {NativeRuntimeAddressSpace::linear,std::nullopt,0x41000});
        if (!opcode || *opcode != 0x60) {
            result.error = "First-stage entry is detached from owned native memory";
            return result;
        }
        auto next = *millennium_amiga_relocator_;
        const auto execution = next.execute_first_stage_entry(observation.registers);
        NativeRuntimeEffectBatch batch{"millennium-amiga-first-stage-entry-"+
            std::to_string(millennium_amiga_relocator_generation_),true,{}};
        batch.effects.reserve(18);
        for (std::size_t i=0;i<execution.snapshot.size();++i) {
            batch.effects.push_back({batch.effects.size()+1,
                {NativeRuntimeAddressSpace::linear,std::nullopt,
                    execution.snapshot_address+i*4},
                MemoryTransferElementWidth::longword,
                NativeRuntimeByteOrder::big_endian,execution.snapshot[i]});
        }
        batch.effects.push_back({batch.effects.size()+1,
            {NativeRuntimeAddressSpace::linear,std::nullopt,
                execution.transient_stack_address},
            MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,
            execution.installed_vector_value});
        batch.effects.push_back({batch.effects.size()+1,
            {NativeRuntimeAddressSpace::linear,std::nullopt,
                execution.installed_vector_address},
            MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,
            execution.installed_vector_value});
        auto memory = *native_runtime_memory_;
        const auto applied = memory.apply(batch);
        if (!applied.accepted) { result.error=applied.error; return result; }
        *millennium_amiga_relocator_=std::move(next);
        *native_runtime_memory_=std::move(memory);
        millennium_amiga_first_stage_entry_sequence_=observation.sequence;
        result.accepted=true;
    } catch (const std::exception& error) {
        result.error=std::string("First-stage entry rejected: ")+error.what();
    }
    return result;
}

MillenniumAmigaBootstrapRelocatorObservationResult
ReleaseRuntimeCoordinator::observe_millennium_amiga_allocation(
    const MillenniumAmigaAllocationRuntimeObservation o) {
    MillenniumAmigaBootstrapRelocatorObservationResult result;
    if (!millennium_amiga_relocator_ || !native_runtime_memory_
        || !millennium_amiga_open_graphics_sequence_
        || millennium_amiga_allocation_sequence_
        || o.sequence <= *millennium_amiga_open_graphics_sequence_) {
        result.error = "Allocation requires the opened graphics library";
        return result;
    }
    try {
        auto next = *millennium_amiga_relocator_;
        const auto execution = next.execute_allocation_consumer(o.allocation);
        for(std::uint32_t i=0;i<o.allocation.allocated_size;++i) {
            if(native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,
                    std::nullopt,execution.allocation_begin+i})) {
                result.error="Allocation overlaps owned runtime memory";
                return result;
            }
        }
        NativeRuntimeEffectBatch batch{"millennium-amiga-cleared-allocation-"
            +std::to_string(millennium_amiga_relocator_generation_),true,{}};
        batch.effects.reserve(o.allocation.allocated_size+2U);
        for(std::uint32_t i=0;i<o.allocation.allocated_size;++i)
            batch.effects.push_back({batch.effects.size()+1,
                {NativeRuntimeAddressSpace::linear,std::nullopt,execution.allocation_begin+i},
                MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,0});
        batch.effects.push_back({batch.effects.size()+1,
            {NativeRuntimeAddressSpace::linear,std::nullopt,execution.primary_pointer_address},
            MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,execution.allocation_begin});
        batch.effects.push_back({batch.effects.size()+1,
            {NativeRuntimeAddressSpace::linear,std::nullopt,execution.secondary_pointer_address},
            MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,execution.allocation_begin});
        auto memory=*native_runtime_memory_;const auto applied=memory.apply(batch);
        if(!applied.accepted){result.error=applied.error;return result;}
        *millennium_amiga_relocator_=std::move(next);*native_runtime_memory_=std::move(memory);
        millennium_amiga_allocation_sequence_=o.sequence;result.accepted=true;
    } catch(const std::exception& error){result.error=error.what();}
    return result;
}

MillenniumAmigaBootstrapRelocatorObservationResult
ReleaseRuntimeCoordinator::observe_millennium_amiga_graphics_initialization(
    const MillenniumAmigaGraphicsInitializationRuntimeObservation o) {
    MillenniumAmigaBootstrapRelocatorObservationResult result;
    if(!millennium_amiga_relocator_||!native_runtime_memory_||!millennium_amiga_allocation_sequence_
        ||millennium_amiga_graphics_initialization_sequence_||o.sequence<=*millennium_amiga_allocation_sequence_){
        result.error="Graphics initialization requires the owned allocation";return result;}
    try{
        auto next=*millennium_amiga_relocator_;const auto execution=next.execute_graphics_initialization(o.services);
        NativeRuntimeEffectBatch batch{"millennium-amiga-graphics-initialization-"+std::to_string(millennium_amiga_relocator_generation_),true,{}};
        batch.effects.reserve(execution.effects.size());
        for(const auto& effect:execution.effects){
            const auto width=effect.width==4?MemoryTransferElementWidth::longword:MemoryTransferElementWidth::word;
            batch.effects.push_back({batch.effects.size()+1,{NativeRuntimeAddressSpace::linear,std::nullopt,effect.address},width,NativeRuntimeByteOrder::big_endian,effect.value});
        }
        auto memory=*native_runtime_memory_;const auto applied=memory.apply(batch);
        if(!applied.accepted){result.error=applied.error;return result;}
        *millennium_amiga_relocator_=std::move(next);*native_runtime_memory_=std::move(memory);
        millennium_amiga_graphics_initialization_sequence_=o.sequence;result.accepted=true;
    }catch(const std::exception& error){result.error=error.what();}
    return result;
}

MillenniumAmigaBootstrapRelocatorObservationResult
ReleaseRuntimeCoordinator::observe_millennium_amiga_view_services(
    const MillenniumAmigaViewServiceRuntimeObservation o) {
    MillenniumAmigaBootstrapRelocatorObservationResult result;
    if(!millennium_amiga_relocator_||!native_runtime_memory_
        ||!millennium_amiga_graphics_initialization_sequence_
        ||millennium_amiga_view_service_sequence_
        ||o.sequence<=*millennium_amiga_graphics_initialization_sequence_){
        result.error="View services require graphics initialization";return result;}
    try{
        auto next=*millennium_amiga_relocator_;const auto x=next.execute_view_services(o.services);
        for(std::uint32_t i=0;i<x.allocation_size;++i)
            if(!native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,x.allocation_begin+i})){
                result.error="View clear lies outside owned allocation";return result;}
        NativeRuntimeEffectBatch b{"millennium-amiga-view-services-"+std::to_string(millennium_amiga_relocator_generation_),true,{}};
        b.effects={{1,{NativeRuntimeAddressSpace::linear,std::nullopt,x.viewport_address+4},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,x.bitmap_address},
            {2,{NativeRuntimeAddressSpace::linear,std::nullopt,0x41958},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,0x14},
            {3,{NativeRuntimeAddressSpace::linear,std::nullopt,x.saved_stack_address+4},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,x.saved_stack_value},
            {4,{NativeRuntimeAddressSpace::linear,std::nullopt,x.saved_stack_address},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,x.saved_stack_value},
            {5,{NativeRuntimeAddressSpace::linear,std::nullopt,x.setup_flag_address},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,x.setup_flag_value}};
        b.effects.reserve(5U+x.allocation_size);
        for(std::uint32_t i=0;i<x.allocation_size;++i)b.effects.push_back({b.effects.size()+1,{NativeRuntimeAddressSpace::linear,std::nullopt,x.allocation_begin+i},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,0});
        auto memory=*native_runtime_memory_;const auto applied=memory.apply(b);
        if(!applied.accepted){result.error=applied.error;return result;}
        *millennium_amiga_relocator_=std::move(next);*native_runtime_memory_=std::move(memory);
        millennium_amiga_view_service_sequence_=o.sequence;result.accepted=true;
    }catch(const std::exception& error){result.error=error.what();}
    return result;
}

MillenniumAmigaBootstrapRelocatorObservationResult
ReleaseRuntimeCoordinator::observe_millennium_amiga_interrupt_control(
    const MillenniumAmigaInterruptControlRuntimeObservation o) {
    MillenniumAmigaBootstrapRelocatorObservationResult result;
    if (!millennium_amiga_relocator_ || !millennium_amiga_view_service_sequence_
        || millennium_amiga_interrupt_control_sequence_
        || o.sequence <= *millennium_amiga_view_service_sequence_) {
        result.error = "Interrupt control requires the owned view-service continuation";
        return result;
    }
    try {
        auto next = *millennium_amiga_relocator_;
        static_cast<void>(next.execute_interrupt_control(o.control));
        *millennium_amiga_relocator_ = std::move(next);
        millennium_amiga_interrupt_control_sequence_ = o.sequence;
        result.accepted = true;
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}

MillenniumAmigaBootstrapRelocatorObservationResult
ReleaseRuntimeCoordinator::observe_millennium_amiga_first_stage_illegal_handler(
    const MillenniumAmigaFirstStageIllegalHandlerObservation observation) {
    MillenniumAmigaBootstrapRelocatorObservationResult result;
    if (!millennium_amiga_relocator_ || !native_runtime_memory_
        || !millennium_amiga_first_stage_entry_sequence_
        || millennium_amiga_first_stage_illegal_sequence_
        || observation.sequence <= *millennium_amiga_first_stage_entry_sequence_) {
        result.error = "ILLEGAL handler requires a later complete exception observation";
        return result;
    }
    try {
        auto next = *millennium_amiga_relocator_;
        const auto execution = next.execute_first_stage_illegal_handler(observation.exception);
        NativeRuntimeEffectBatch batch{"millennium-amiga-first-stage-illegal-"+
            std::to_string(millennium_amiga_relocator_generation_),true,{}};
        batch.effects.reserve(12);
        batch.effects.push_back({1,{NativeRuntimeAddressSpace::linear,std::nullopt,
            execution.exception_frame_address},MemoryTransferElementWidth::word,
            NativeRuntimeByteOrder::big_endian,execution.saved_status_register});
        batch.effects.push_back({2,{NativeRuntimeAddressSpace::linear,std::nullopt,
            execution.exception_frame_address+2},MemoryTransferElementWidth::longword,
            NativeRuntimeByteOrder::big_endian,execution.saved_program_counter});
        for (std::size_t i=0;i<execution.snapshot.size();++i) {
            batch.effects.push_back({batch.effects.size()+1,
                {NativeRuntimeAddressSpace::linear,std::nullopt,
                    execution.snapshot_address+i*4},
                MemoryTransferElementWidth::longword,
                NativeRuntimeByteOrder::big_endian,execution.snapshot[i]});
        }
        batch.effects.push_back({batch.effects.size()+1,
            {NativeRuntimeAddressSpace::linear,std::nullopt,0x10},
            MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,
            execution.installed_vector_value});
        auto memory=*native_runtime_memory_;
        const auto applied=memory.apply(batch);
        if(!applied.accepted){result.error=applied.error;return result;}
        *millennium_amiga_relocator_=std::move(next);
        *native_runtime_memory_=std::move(memory);
        millennium_amiga_first_stage_illegal_sequence_=observation.sequence;
        result.accepted=true;
    } catch (const std::exception& error) {
        result.error=std::string("First-stage ILLEGAL handler rejected: ")+error.what();
    }
    return result;
}

MillenniumAmigaBootstrapRelocatorObservationResult
ReleaseRuntimeCoordinator::observe_millennium_amiga_second_illegal_handler(
    const MillenniumAmigaSecondIllegalHandlerObservation observation) {
    MillenniumAmigaBootstrapRelocatorObservationResult result;
    if (!millennium_amiga_relocator_ || !native_runtime_memory_
        || !millennium_amiga_first_stage_illegal_sequence_
        || millennium_amiga_second_illegal_sequence_
        || observation.sequence <= *millennium_amiga_first_stage_illegal_sequence_) {
        result.error="Second ILLEGAL handler requires a later complete exception observation";
        return result;
    }
    try {
        auto next=*millennium_amiga_relocator_;
        const auto execution=next.execute_second_illegal_handler(observation.exception);
        NativeRuntimeEffectBatch batch{"millennium-amiga-second-illegal-"+
            std::to_string(millennium_amiga_relocator_generation_),true,{}};
        batch.effects.reserve(11);
        for(std::size_t i=0;i<execution.temporary_stack.size();++i)
            batch.effects.push_back({batch.effects.size()+1,
                {NativeRuntimeAddressSpace::linear,std::nullopt,
                    execution.temporary_stack_address+i*4},
                MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,
                execution.temporary_stack[i]});
        batch.effects.push_back({batch.effects.size()+1,
            {NativeRuntimeAddressSpace::linear,std::nullopt,execution.exception_frame_address},
            MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,
            execution.resulting_saved_status_register});
        batch.effects.push_back({batch.effects.size()+1,
            {NativeRuntimeAddressSpace::linear,std::nullopt,execution.exception_frame_address+2},
            MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,
            execution.resulting_saved_program_counter});
        const std::array<std::pair<std::uint32_t,std::uint32_t>,5> writes{{
            {0x20,execution.vector_8_value},{0x24,execution.vector_9_value},
            {execution.cursor_address,execution.cursor_value},
            {execution.saved_ciphertext_address,execution.saved_ciphertext_value},
            {execution.transformed_address,execution.transformed_value}}};
        for(const auto& [address,value]:writes)
            batch.effects.push_back({batch.effects.size()+1,
                {NativeRuntimeAddressSpace::linear,std::nullopt,address},
                MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,value});
        auto memory=*native_runtime_memory_;
        const auto applied=memory.apply(batch);
        if(!applied.accepted){result.error=applied.error;return result;}
        *millennium_amiga_relocator_=std::move(next);*native_runtime_memory_=std::move(memory);
        millennium_amiga_second_illegal_sequence_=observation.sequence;result.accepted=true;
    }catch(const std::exception& e){result.error=std::string("Second ILLEGAL handler rejected: ")+e.what();}
    return result;
}

MillenniumAmigaBootstrapRelocatorObservationResult
ReleaseRuntimeCoordinator::observe_millennium_amiga_first_trace_handler(
    const MillenniumAmigaFirstTraceHandlerObservation observation) {
    MillenniumAmigaBootstrapRelocatorObservationResult result;
    if (!millennium_amiga_relocator_ || !native_runtime_memory_
        || !millennium_amiga_second_illegal_sequence_
        || millennium_amiga_first_trace_sequence_
        || observation.sequence <= *millennium_amiga_second_illegal_sequence_) {
        result.error = "First trace handler requires a later complete exception observation";
        return result;
    }
    try {
        auto next = *millennium_amiga_relocator_;
        const auto execution = next.execute_first_trace_handler(observation.exception);
        NativeRuntimeEffectBatch batch{"millennium-amiga-first-trace-"
            + std::to_string(millennium_amiga_relocator_generation_), true, {}};
        batch.effects.reserve(7);
        for (std::size_t i = 0; i < execution.temporary_stack.size(); ++i) {
            batch.effects.push_back({batch.effects.size() + 1,
                {NativeRuntimeAddressSpace::linear, std::nullopt,
                    execution.temporary_stack_address + i * 4},
                MemoryTransferElementWidth::longword, NativeRuntimeByteOrder::big_endian,
                execution.temporary_stack[i]});
        }
        const std::array<std::pair<std::uint32_t, std::uint32_t>, 4> writes{{
            {execution.restored_address, execution.restored_value},
            {execution.cursor_address, execution.cursor_value},
            {execution.saved_ciphertext_address, execution.saved_ciphertext_value},
            {execution.transformed_address, execution.transformed_value},
        }};
        for (const auto& [address, value] : writes) {
            batch.effects.push_back({batch.effects.size() + 1,
                {NativeRuntimeAddressSpace::linear, std::nullopt, address},
                MemoryTransferElementWidth::longword, NativeRuntimeByteOrder::big_endian, value});
        }
        auto memory = *native_runtime_memory_;
        const auto applied = memory.apply(batch);
        if (!applied.accepted) { result.error = applied.error; return result; }
        *millennium_amiga_relocator_ = std::move(next);
        *native_runtime_memory_ = std::move(memory);
        millennium_amiga_first_trace_sequence_ = observation.sequence;
        result.accepted = true;
    } catch (const std::exception& error) {
        result.error = std::string("First trace handler rejected: ") + error.what();
    }
    return result;
}

MillenniumAmigaBootstrapRelocatorObservationResult
ReleaseRuntimeCoordinator::observe_millennium_amiga_trace_branch_chain(
    const MillenniumAmigaTraceBranchChainRuntimeObservation observation) {
    MillenniumAmigaBootstrapRelocatorObservationResult result;
    if(!millennium_amiga_relocator_||!native_runtime_memory_||!millennium_amiga_first_trace_sequence_
        ||millennium_amiga_trace_branch_chain_sequence_
        ||observation.sequence<=*millennium_amiga_first_trace_sequence_){
        result.error="Trace branch chain requires the later decrypted ADDX";return result;
    }
    try{
        auto next=*millennium_amiga_relocator_;
        const auto execution=next.execute_trace_branch_chain(observation.trace);
        NativeRuntimeEffectBatch batch{"millennium-amiga-trace-branch-chain-"+
            std::to_string(millennium_amiga_relocator_generation_),true,{}};
        batch.effects.reserve(execution.decryptions.size()*9);
        for(const auto& e:execution.decryptions){
            batch.effects.push_back({batch.effects.size()+1,{NativeRuntimeAddressSpace::linear,std::nullopt,e.exception_frame_address},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,e.saved_status_register});
            batch.effects.push_back({batch.effects.size()+1,{NativeRuntimeAddressSpace::linear,std::nullopt,e.exception_frame_address+2},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,e.saved_program_counter});
            for(std::size_t i=0;i<e.temporary_stack.size();++i)batch.effects.push_back({batch.effects.size()+1,{NativeRuntimeAddressSpace::linear,std::nullopt,e.temporary_stack_address+i*4},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,e.temporary_stack[i]});
            const std::array<std::pair<std::uint32_t,std::uint32_t>,4>w{{{e.restored_address,e.restored_value},{e.cursor_address,e.cursor_value},{e.saved_ciphertext_address,e.saved_ciphertext_value},{e.transformed_address,e.transformed_value}}};
            for(const auto&[address,value]:w)batch.effects.push_back({batch.effects.size()+1,{NativeRuntimeAddressSpace::linear,std::nullopt,address},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,value});
        }
        auto memory=*native_runtime_memory_;const auto applied=memory.apply(batch);
        if(!applied.accepted){result.error=applied.error;return result;}
        *millennium_amiga_relocator_=std::move(next);*native_runtime_memory_=std::move(memory);
        millennium_amiga_trace_branch_chain_sequence_=observation.sequence;result.accepted=true;
    }catch(const std::exception&e){result.error=std::string("Trace branch chain rejected: ")+e.what();}
    return result;
}
MillenniumAmigaBootstrapRelocatorObservationResult ReleaseRuntimeCoordinator::observe_millennium_amiga_trace_register_prefix(const MillenniumAmigaTraceRegisterPrefixRuntimeObservation o){MillenniumAmigaBootstrapRelocatorObservationResult r;if(!millennium_amiga_relocator_||!native_runtime_memory_||!millennium_amiga_trace_branch_chain_sequence_||millennium_amiga_trace_register_prefix_sequence_||o.sequence<=*millennium_amiga_trace_branch_chain_sequence_){r.error="Trace register prefix requires terminal branch trace";return r;}try{auto next=*millennium_amiga_relocator_;const auto x=next.execute_trace_register_prefix(o.trace);NativeRuntimeEffectBatch b{"millennium-amiga-trace-register-prefix-"+std::to_string(millennium_amiga_relocator_generation_),true,{}};b.effects.reserve(72);for(const auto&e:x.decryptions){b.effects.push_back({b.effects.size()+1,{NativeRuntimeAddressSpace::linear,std::nullopt,e.exception_frame_address},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,e.saved_status_register});b.effects.push_back({b.effects.size()+1,{NativeRuntimeAddressSpace::linear,std::nullopt,e.exception_frame_address+2},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,e.saved_program_counter});for(std::size_t i=0;i<3;++i)b.effects.push_back({b.effects.size()+1,{NativeRuntimeAddressSpace::linear,std::nullopt,e.temporary_stack_address+i*4},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,e.temporary_stack[i]});for(const auto&[address,value]:std::array<std::pair<std::uint32_t,std::uint32_t>,4>{{{e.restored_address,e.restored_value},{e.cursor_address,e.cursor_value},{e.saved_ciphertext_address,e.saved_ciphertext_value},{e.transformed_address,e.transformed_value}}})b.effects.push_back({b.effects.size()+1,{NativeRuntimeAddressSpace::linear,std::nullopt,address},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,value});}auto m=*native_runtime_memory_;const auto a=m.apply(b);if(!a.accepted){r.error=a.error;return r;}*millennium_amiga_relocator_=std::move(next);*native_runtime_memory_=std::move(m);millennium_amiga_trace_register_prefix_sequence_=o.sequence;r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}

MillenniumAmigaBootstrapRelocatorObservationResult ReleaseRuntimeCoordinator::observe_millennium_amiga_bus_error_prefix(const MillenniumAmigaBusErrorRuntimeObservation o){MillenniumAmigaBootstrapRelocatorObservationResult r;if(!millennium_amiga_relocator_||!native_runtime_memory_||!millennium_amiga_trace_register_prefix_sequence_||millennium_amiga_bus_error_prefix_sequence_||o.sequence<=*millennium_amiga_trace_register_prefix_sequence_){r.error="Bus-error prefix requires the decrypted memory-write boundary";return r;}try{auto next=*millennium_amiga_relocator_;const auto x=next.execute_bus_error_prefix(o.exception);NativeRuntimeEffectBatch b{"millennium-amiga-bus-error-prefix-"+std::to_string(millennium_amiga_relocator_generation_),true,{}};b.effects={
{1,{NativeRuntimeAddressSpace::linear,std::nullopt,o.exception.exception_frame_address},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,o.exception.saved_status_register},
{2,{NativeRuntimeAddressSpace::linear,std::nullopt,o.exception.exception_frame_address+2},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,o.exception.saved_program_counter},
{3,{NativeRuntimeAddressSpace::linear,std::nullopt,o.exception.exception_frame_address+6},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,o.exception.instruction_register},
{4,{NativeRuntimeAddressSpace::linear,std::nullopt,o.exception.exception_frame_address+8},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,o.exception.fault_address},
{5,{NativeRuntimeAddressSpace::linear,std::nullopt,o.exception.exception_frame_address+12},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,o.exception.special_status_word},
{6,{NativeRuntimeAddressSpace::linear,std::nullopt,x.saved_a1_address},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,x.saved_a1_value}};auto m=*native_runtime_memory_;const auto a=m.apply(b);if(!a.accepted){r.error=a.error;return r;}*millennium_amiga_relocator_=std::move(next);*native_runtime_memory_=std::move(m);millennium_amiga_bus_error_prefix_sequence_=o.sequence;r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}

MillenniumAmigaBootstrapRelocatorObservationResult ReleaseRuntimeCoordinator::observe_millennium_amiga_custom_chip_exec_prefix(const MillenniumAmigaCustomChipExecRuntimeObservation o){MillenniumAmigaBootstrapRelocatorObservationResult r;if(!millennium_amiga_relocator_||!native_runtime_memory_||!millennium_amiga_bus_error_prefix_sequence_||millennium_amiga_custom_chip_exec_prefix_sequence_||o.sequence<=*millennium_amiga_bus_error_prefix_sequence_){r.error="Custom-chip/ExecBase prefix requires the bus-error prefix";return r;}try{auto next=*millennium_amiga_relocator_;const auto x=next.execute_custom_chip_exec_prefix(o.hardware_and_exec_base);NativeRuntimeEffectBatch b{"millennium-amiga-custom-chip-exec-prefix-"+std::to_string(millennium_amiga_relocator_generation_),true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,x.saved_d0_address},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,x.saved_d0_value},{2,{NativeRuntimeAddressSpace::linear,std::nullopt,x.exec_base_source_address},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,x.resulting_a6}}};auto m=*native_runtime_memory_;const auto a=m.apply(b);if(!a.accepted){r.error=a.error;return r;}*millennium_amiga_relocator_=std::move(next);*native_runtime_memory_=std::move(m);millennium_amiga_custom_chip_exec_prefix_sequence_=o.sequence;r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}

MillenniumAmigaBootstrapRelocatorObservationResult ReleaseRuntimeCoordinator::observe_millennium_amiga_exec_transition(const MillenniumAmigaExecTransitionRuntimeObservation o){MillenniumAmigaBootstrapRelocatorObservationResult r;if(!millennium_amiga_relocator_||!native_runtime_memory_||!millennium_amiga_custom_chip_exec_prefix_sequence_||millennium_amiga_exec_transition_sequence_||o.sequence<=*millennium_amiga_custom_chip_exec_prefix_sequence_){r.error="Exec transition requires the custom-chip/ExecBase prefix";return r;}try{auto next=*millennium_amiga_relocator_;const auto x=next.execute_exec_transition(o.returns);NativeRuntimeEffectBatch b{"millennium-amiga-exec-transition-"+std::to_string(millennium_amiga_relocator_generation_),true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,x.saved_a1_address},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,x.saved_a1_value}}};auto m=*native_runtime_memory_;const auto a=m.apply(b);if(!a.accepted){r.error=a.error;return r;}*millennium_amiga_relocator_=std::move(next);*native_runtime_memory_=std::move(m);millennium_amiga_exec_transition_sequence_=o.sequence;r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}

MillenniumAmigaBootstrapRelocatorObservationResult
ReleaseRuntimeCoordinator::observe_millennium_amiga_open_graphics(
    const MillenniumAmigaOpenGraphicsRuntimeObservation o) {
    MillenniumAmigaBootstrapRelocatorObservationResult result;
    if (!millennium_amiga_relocator_ || !native_runtime_memory_
        || !millennium_amiga_exec_transition_sequence_
        || millennium_amiga_open_graphics_sequence_
        || o.sequence <= *millennium_amiga_exec_transition_sequence_) {
        result.error = "Open-graphics service requires the Exec transition";
        return result;
    }
    try {
        auto next = *millennium_amiga_relocator_;
        const auto execution = next.execute_open_graphics(o.service);
        auto memory = *native_runtime_memory_;
        // ADDQ consumes resident state. The original zero in the source image
        // alone cannot prove that a preceding observed service preserved it.
        const auto high = memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt, execution.open_count_address});
        const auto low = memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt, execution.open_count_address + 1U});
        if (!high || !low || *high != 0 || *low != 0) {
            result.error = "Open-graphics count contradicts owned runtime memory";
            return result;
        }
        NativeRuntimeEffectBatch batch{
            "millennium-amiga-open-graphics-"
                + std::to_string(millennium_amiga_relocator_generation_), true, {
            {1, {NativeRuntimeAddressSpace::linear, std::nullopt,
                execution.graphics_base_address}, MemoryTransferElementWidth::longword,
                NativeRuntimeByteOrder::big_endian, execution.graphics_base_value},
            {2, {NativeRuntimeAddressSpace::linear, std::nullopt,
                execution.open_count_address}, MemoryTransferElementWidth::word,
                NativeRuntimeByteOrder::big_endian, execution.open_count_value}}};
        const auto applied = memory.apply(batch);
        if (!applied.accepted) {
            result.error = applied.error;
            return result;
        }
        *millennium_amiga_relocator_ = std::move(next);
        *native_runtime_memory_ = std::move(memory);
        millennium_amiga_open_graphics_sequence_ = o.sequence;
        result.accepted = true;
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
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
ReleaseRuntimeCoordinator::begin_millennium_dos_sound_driver_load(
    const std::uint64_t sequence, const std::uint16_t code_segment,
    const MillenniumDosSoundDriverCodeSegmentProvenance provenance) {
    MillenniumDosSoundDriverLoadObservationResult rejected;
    if (!active_ || !session_snapshot_ || !millennium_dos_ || !native_runtime_memory_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_sound_driver_boundary
        || !millennium_dos_sound_selection_) {
        rejected.error = "Sound-driver loading requires the active selected-driver boundary";
        return rejected;
    }
    if (sequence == 0 || sequence <= millennium_dos_sound_driver_load_last_sequence_) {
        rejected.error = "Sound-driver observation sequence is stale or duplicated";
        return rejected;
    }
    if (sequence != 1 || code_segment == 0 || millennium_dos_sound_driver_load_
        || !millennium_dos_sound_selection_->selected_driver_is_admitted()) {
        rejected.error = "Sound-driver entry was already admitted or has no exact selected leaf";
        return rejected;
    }
    try {
            const auto selected = millennium_dos_sound_selection_->selected_driver();
            if (!selected) throw std::runtime_error("Selected driver identity is unavailable");
            if (!active_media_) throw std::runtime_error("Active verified media is unavailable");
            const auto launcher = admit_native_code_image(*active_media_,
                "millennium-dos-mill-com-linear", "millennium-dos-launcher");
            const auto driver = active_media_->borrow(selected->sha256);
            if (!launcher.accepted() || !driver) throw std::runtime_error("Selected driver bytes are unavailable");
            const char selected_character = selected->kind
                    == MillenniumDosSoundDriverKind::ibm_speaker ? '0'
                : selected->kind == MillenniumDosSoundDriverKind::sound_blaster ? '1' : '2';
            MillenniumDosSoundDriverLoadSession next(launcher.view->bytes, *driver,
                selected_character, code_segment);
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
            millennium_dos_sound_driver_code_segment_provenance_=provenance;
            millennium_dos_compatibility_runner_.emplace(
                millennium_dos_sound_driver_load_generation_, sequence, code_segment);
            return {true,{}};
    } catch(const std::exception& e) { rejected.error=e.what(); return rejected; }
}

MillenniumDosSoundDriverLoadObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_sound_driver_load(
    const MillenniumDosSoundDriverLoadObservation observation) {
    if (const auto* entry = std::get_if<MillenniumDosSoundDriverLoadEntryObservation>(&observation)) {
        return begin_millennium_dos_sound_driver_load(entry->sequence, entry->code_segment,
            MillenniumDosSoundDriverCodeSegmentProvenance::external_observation);
    }
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
            else if constexpr(std::is_same_v<T,MillenniumDosSoundDriverVectorObservation>)
                next.observe_vector_install(value.post_instruction,value.flags,
                    value.ivt_offset,value.ivt_segment);
            else if constexpr(std::is_same_v<T,MillenniumDosSoundDriverStackObservation>) next.observe_parent_stack(value.instruction,value.address,value.value);
            else if constexpr(std::is_same_v<T,MillenniumDosSoundDriverTitleExecObservation>) next.observe_title_exec_request(value.instruction,value.ax,value.dx,value.parameter_block);
            else throw std::runtime_error("Sound-driver entry observation is valid only before admission");
        },observation);
        if (const auto* vector = std::get_if<MillenniumDosSoundDriverVectorObservation>(&observation)) {
            NativeRuntimeEffectBatch batch{
                "millennium-dos-sound-driver-" +
                    std::to_string(millennium_dos_sound_driver_load_generation_) +
                    "-observed-int95-vector",
                true,
                {{1,
                  {NativeRuntimeAddressSpace::dos_segmented, std::uint16_t{0}, 0x254},
                  MemoryTransferElementWidth::word,
                  NativeRuntimeByteOrder::little_endian,
                  vector->ivt_offset},
                 {2,
                  {NativeRuntimeAddressSpace::dos_segmented, std::uint16_t{0}, 0x256},
                  MemoryTransferElementWidth::word,
                  NativeRuntimeByteOrder::little_endian,
                  vector->ivt_segment}}};
            const auto applied = memory.apply(batch);
            if (!applied.accepted) throw std::runtime_error(applied.error);
        }
        if(next.memory_effects().size()!=before_memory){
            NativeRuntimeEffectBatch batch{"millennium-dos-sound-driver-"+std::to_string(millennium_dos_sound_driver_load_generation_)+"-image",true,{}};
            batch.effects.reserve(next.memory_effects().size());
            for(std::size_t i=0;i<next.memory_effects().size();++i){const auto&e=next.memory_effects()[i];batch.effects.push_back({i+1,{NativeRuntimeAddressSpace::dos_segmented,e.segment,e.offset},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::little_endian,e.value});}
            const auto applied=memory.apply(batch);if(!applied.accepted)throw std::runtime_error(applied.error);
        }
        if(next.runtime_word_effects().size()!=before_words){
            NativeRuntimeEffectBatch batch{"millennium-dos-sound-driver-"+std::to_string(millennium_dos_sound_driver_load_generation_)+"-parent",true,{}};
            for(std::size_t i=0;i<next.runtime_word_effects().size();++i){const auto&e=next.runtime_word_effects()[i];batch.effects.push_back({i+1,{NativeRuntimeAddressSpace::dos_segmented,e.segment,e.address},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::little_endian,e.value});}
            const auto applied=memory.apply(batch);if(!applied.accepted)throw std::runtime_error(applied.error);
        }
        if(next.state()==MillenniumDosSoundDriverLoadState::title_exec_requested){
            if(!millennium_dos_->title_flow)throw std::runtime_error("English title flow is unavailable");
            constexpr std::string_view mill_sha =
                "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e";
            constexpr std::string_view titles_sha =
                "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
            if (!active_media_) throw std::runtime_error("Active verified media is unavailable");
            const auto& media=*active_media_;
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
    if(!session_snapshot_
        || (session_snapshot_->kind != RuntimeSessionKind::millennium_dos_sound_driver_boundary
            && session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title)
        || !millennium_dos_sound_driver_load_) return std::nullopt;
    const auto&s=*millennium_dos_sound_driver_load_;
    const auto code_segment = millennium_dos_compatibility_runner_
        ? millennium_dos_compatibility_runner_->code_segment() : std::uint16_t{0};
    return MillenniumDosSoundDriverLoadCheckpoint{
        millennium_dos_sound_driver_load_generation_,
        millennium_dos_sound_driver_load_last_sequence_, s.state(), s.boundary(),
        s.driver().kind, s.memory_effects().size(), s.file_handle(), s.load_segment(),
        code_segment, millennium_dos_sound_driver_code_segment_provenance_,
        s.runtime_word_effects(), s.runtime_byte_effects(), s.installed_vector(),
        s.vector_install_provenance(), s.parent_stack_provenance()};
}

MillenniumDosVideoDriverLoadObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_video_driver_load(
    const MillenniumDosVideoDriverLoadObservation observation) {
    MillenniumDosVideoDriverLoadObservationResult rejected;
    if (!active_ || !session_snapshot_ || !millennium_dos_ || !active_media_
        || !native_runtime_memory_
        || (session_snapshot_->kind != RuntimeSessionKind::millennium_dos_sound_driver_boundary
            && session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title)
        || !millennium_dos_->title_flow || millennium_dos_->language != "en") {
        rejected.error = "Video-driver observations require the active English Millennium DOS boundary";
        return rejected;
    }
    const auto sequence = std::visit([](const auto& value) { return value.sequence; }, observation);
    if (sequence == 0 || sequence <= millennium_dos_video_driver_load_last_sequence_) {
        rejected.error = "Video-driver observation sequence is stale or duplicated";
        return rejected;
    }
    try {
        millennium_dos_video_function_zero_return_chain_completed_ = false;
        auto memory = *native_runtime_memory_;
        auto next_session = millennium_dos_video_driver_load_;
        auto next_generation = millennium_dos_video_driver_load_generation_;
        if (const auto* selector = std::get_if<MillenniumDosVideoSelectorObservation>(&observation)) {
            if (next_session || selector->instruction_address != 0x0204)
                throw std::runtime_error("Video-driver loader entry is detached or already admitted");
            std::uint8_t selected = 0;
            if (selector->source == MillenniumDosVideoSelectorSource::command_tail) {
                switch (selector->command) {
                case 'e': case 'E': selected = 1; break;
                case 'm': case 'M': selected = 2; break;
                default: throw std::runtime_error("Unrecognized Millennium DOS video command-tail selector");
                }
            } else if (selector->source == MillenniumDosVideoSelectorSource::hardware_detector) {
                if (selector->raw_detector_result > 2)
                    throw std::runtime_error("Millennium DOS hardware detector result is outside 0..2");
                selected = selector->raw_detector_result == 1 ? 1 : 2;
            } else {
                throw std::runtime_error("Unknown Millennium DOS video selector provenance");
            }
            constexpr std::string_view mill_sha = "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e";
            constexpr std::string_view titles_sha = "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
            constexpr std::string_view ega_sha = "ba003dd155fee868980f6ece933c33f9b22af68ed376cd64f4e027abd65baf6a";
            constexpr std::string_view mcga_sha = "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228";
            const auto mill = active_media_->borrow(mill_sha);
            const auto titles = active_media_->borrow(titles_sha);
            const auto driver = active_media_->borrow(selected == 1 ? ega_sha : mcga_sha);
            if (!mill || !titles || !driver) throw std::runtime_error("Exact active video-loader media is unavailable");
            MillenniumDosVideoDriverLoadSession next(*titles, *mill, *driver,
                selected == 1 ? MillenniumDosVideoDriverKind::ega640 : MillenniumDosVideoDriverKind::mcga,
                selector->source, selected, selector->command, selector->raw_detector_result);
            next_session = std::move(next);
            ++next_generation;
        } else {
            if (!next_session)
                throw std::runtime_error("Video-driver selector observation at $0204 is required first");
            auto next = *next_session;
            if (const auto* dos = std::get_if<MillenniumDosVideoDriverDosObservation>(&observation)) {
                switch (next.state()) {
                case MillenniumDosVideoDriverLoadState::awaiting_open_result: next.observe_open_result(dos->instruction,dos->carry,dos->ax); break;
                case MillenniumDosVideoDriverLoadState::awaiting_seek_end_result: next.observe_seek_end_result(dos->instruction,dos->carry,dos->bx,dos->ax,dos->dx); break;
                case MillenniumDosVideoDriverLoadState::awaiting_allocation_result: next.observe_allocation_result(dos->instruction,dos->carry,dos->ax); break;
                case MillenniumDosVideoDriverLoadState::awaiting_seek_start_result: next.observe_seek_start_result(dos->instruction,dos->carry,dos->bx,dos->ax,dos->dx); break;
                case MillenniumDosVideoDriverLoadState::awaiting_read_result: next.observe_read_result(dos->instruction,dos->carry,dos->bx,dos->ax); break;
                case MillenniumDosVideoDriverLoadState::awaiting_close_result: next.observe_close_result(dos->instruction,dos->carry,dos->bx); break;
                default: throw std::runtime_error("Video-driver loader is not awaiting a DOS result");
                }
            } else if (const auto* request = std::get_if<MillenniumDosVideoDriverSetVectorRequestObservation>(&observation)) {
                next.observe_set_vector_request(request->instruction,request->interrupt,request->ax,request->dx);
            } else if (const auto* result = std::get_if<MillenniumDosVideoDriverSetVectorResultObservation>(&observation)) {
                next.observe_set_vector_dos_result(result->instruction,result->carry,result->ax);
            } else if (const auto* ivt = std::get_if<MillenniumDosVideoDriverIvtObservation>(&observation)) {
                next.observe_ivt_vector(ivt->interrupt_number,ivt->offset,ivt->segment);
            }
            if (next.memory_effects().size() != next_session->memory_effects().size()) {
                NativeRuntimeEffectBatch batch{"millennium-dos-video-driver-" + std::to_string(next_generation) + "-image",true,{}};
                batch.effects.reserve(next.memory_effects().size());
                for (std::size_t i=0;i<next.memory_effects().size();++i) {
                    const auto& effect=next.memory_effects()[i];
                    batch.effects.push_back({i+1,{NativeRuntimeAddressSpace::dos_segmented,effect.segment,effect.offset},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::little_endian,effect.value});
                }
                const auto applied=memory.apply(batch);
                if (!applied.accepted) throw std::runtime_error(applied.error);
            }
            next_session = std::move(next);
        }
        millennium_dos_video_driver_load_ = std::move(next_session);
        millennium_dos_video_driver_load_generation_ = next_generation;
        *native_runtime_memory_ = std::move(memory);
        millennium_dos_video_driver_load_last_sequence_ = sequence;
        return {true,{}};
    } catch (const std::exception& e) { rejected.error = e.what(); return rejected; }
}

std::optional<MillenniumDosVideoDriverLoadCheckpoint>
ReleaseRuntimeCoordinator::millennium_dos_video_driver_load_checkpoint() const {
    if (!session_snapshot_
        || (session_snapshot_->kind != RuntimeSessionKind::millennium_dos_sound_driver_boundary
            && session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title)
        || !millennium_dos_video_driver_load_) return std::nullopt;
    const auto& s = *millennium_dos_video_driver_load_;
    const auto b = s.boundary();
    return MillenniumDosVideoDriverLoadCheckpoint{millennium_dos_video_driver_load_generation_,
        millennium_dos_video_driver_load_last_sequence_,s.state(),b,s.driver().kind,
        s.selector_source(),s.selector(),s.observed_command(),s.raw_detector_result(),
        s.driver().byte_size,s.memory_effects().size(),
        s.file_handle(),s.load_segment(),s.set_vector_carry(),
        s.state()!=MillenniumDosVideoDriverLoadState::set_vector_result_boundary,s.set_vector_ax(),
        s.vector_provenance(),
        s.ivt_observation() ? std::optional<MillenniumDosVideoDriverIvtObservation>{
            MillenniumDosVideoDriverIvtObservation{millennium_dos_video_driver_load_last_sequence_,
                s.ivt_observation()->interrupt_number,s.ivt_observation()->offset,s.ivt_observation()->segment}}
            : std::nullopt,
        s.ivt_observation_matches_loaded_driver()};
}

std::optional<MillenniumDosVideoInt91FunctionZeroDispatch>
ReleaseRuntimeCoordinator::millennium_dos_video_int91_function_zero_dispatch() const {
    if (!millennium_dos_video_int91_dispatch_) return std::nullopt;
    return millennium_dos_video_int91_dispatch_->dispatch();
}

MillenniumDosVideoFunctionZeroObservationResult
ReleaseRuntimeCoordinator::begin_millennium_dos_video_int91_function_zero_dispatch() {
    MillenniumDosVideoFunctionZeroObservationResult result;
    if (!active_ || !active_media_ || !native_runtime_memory_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title
        || !millennium_dos_title_initialization_ || !millennium_dos_video_driver_load_
        || millennium_dos_title_initialization_->checkpoint().state
            != MillenniumDosTitleInitializationState::private_interrupt_result_boundary
        || millennium_dos_video_driver_load_->state()
            != MillenniumDosVideoDriverLoadState::ivt_observation_recorded
        || !millennium_dos_video_driver_load_->ivt_observation_matches_loaded_driver()) {
        result.error = "First function-zero dispatch requires the exact pending title and loaded INT 91h driver";
        return result;
    }
    if (millennium_dos_video_int91_dispatch_) {
        result.error = "First function-zero dispatch is already active";
        return result;
    }
    try {
        auto memory = *native_runtime_memory_;
        const auto kind = millennium_dos_video_driver_load_->driver().kind;
        const std::string_view driver_sha = kind == MillenniumDosVideoDriverKind::ega640
            ? "ba003dd155fee868980f6ece933c33f9b22af68ed376cd64f4e027abd65baf6a"
            : "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228";
        const auto exact_driver = active_media_->borrow(driver_sha);
        const auto* vector = millennium_dos_video_driver_load_->ivt_observation();
        if (!exact_driver || !vector)
            throw std::runtime_error("Exact video driver media or INT 91h vector is unavailable");
        std::vector<std::uint8_t> loaded_image;
        loaded_image.reserve(millennium_dos_video_driver_load_->driver().byte_size);
        for (std::size_t offset = 0;
            offset < millennium_dos_video_driver_load_->driver().byte_size; ++offset) {
            const auto byte = native_runtime_memory_->read_byte({
                NativeRuntimeAddressSpace::dos_segmented,
                millennium_dos_video_driver_load_->load_segment(), offset});
            if (!byte) throw std::runtime_error("Loaded video-driver memory image is incomplete");
            loaded_image.push_back(*byte);
        }
        const auto title = millennium_dos_title_initialization_->checkpoint();
        if (title.last_sequence == std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("Title sequence cannot advance to the first BIOS boundary");
        const auto effect = [&](const std::uint16_t address, const std::string_view reg)
            -> std::optional<std::uint16_t> {
            for (auto it = title.register_effects.rbegin(); it != title.register_effects.rend(); ++it)
                if (it->instruction_address == address && it->register_name == reg)
                    return it->value;
            return std::nullopt;
        };
        if (effect(0x1b80,"DS") != title.child_code_segment
            || effect(0x1b82,"ES") != title.child_code_segment
            || effect(0x1b86,"SS") != title.child_code_segment
            || effect(0x1b8b,"SP") != 0xda00)
            throw std::runtime_error("TITLES startup does not establish the exact private-call stack");
        std::optional<std::uint8_t> mcga_count_argument;
        if (kind == MillenniumDosVideoDriverKind::mcga) {
            constexpr std::string_view titles_sha =
                "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
            const auto titles = active_media_->borrow(titles_sha);
            if (!titles || titles->size() != 7022 || to_hex(sha256(*titles)) != titles_sha
                || effect(0x1b90,"ES") != title.child_code_segment
                || effect(0x1b92,"BX") != 0x1ac4
                || title.boundary.record_offset != 0x1ac4
                || title.boundary.record_segment != title.child_code_segment) {
                throw std::runtime_error("MCGA caller does not establish its exact TITLES count argument");
            }
            constexpr std::size_t titles_load_offset = 0x0100;
            constexpr std::size_t count_runtime_offset = 0x1ac4;
            constexpr std::size_t count_file_offset = count_runtime_offset - titles_load_offset;
            if (count_file_offset >= titles->size())
                throw std::runtime_error("MCGA count argument is outside TITLES.EXE");
            const auto expected_count = (*titles)[count_file_offset];
            const auto owned_count = memory.read_byte({NativeRuntimeAddressSpace::dos_segmented,
                title.child_code_segment,count_runtime_offset});
            if (!owned_count || *owned_count != expected_count || expected_count != 0x01) {
                throw std::runtime_error("MCGA count argument differs from owned hash-verified TITLES memory");
            }
            mcga_count_argument = *owned_count;
        }
        // Materialize only stack words established by the admitted CALL and
        // INT instructions. SI/DI/BP and FLAGS are intentionally left
        // uninitialized; their values are not needed for the MCGA CX=1 path.
        NativeRuntimeEffectBatch stack_frame{
            "millennium-dos-int91-stack-" + std::to_string(millennium_dos_video_driver_load_generation_),
            true, {}};
        const std::array<std::pair<std::uint16_t,std::uint16_t>,6> words{{
            {0xd9fe,0x1b98}, {0xd9fc,title.child_code_segment},
            {0xd9f4,title.child_code_segment}, {0xd9ee,0x0129},
            {0xd9f0,title.child_code_segment}, {0xd9ec,0x0012}}};
        for (const auto& [offset,value] : words) {
            const NativeRuntimeLocation low_location{NativeRuntimeAddressSpace::dos_segmented,
                title.child_code_segment,offset};
            auto high_location = low_location;
            ++high_location.offset;
            if (memory.read_byte(low_location) || memory.read_byte(high_location))
                throw std::runtime_error("TITLES private-call stack overlaps initialized runtime memory");
            stack_frame.effects.push_back({stack_frame.effects.size()+1,low_location,
                MemoryTransferElementWidth::word,NativeRuntimeByteOrder::little_endian,value});
        }
        const auto stack_applied = memory.apply(stack_frame);
        if (!stack_applied.accepted) throw std::runtime_error(stack_applied.error);
        MillenniumDosVideoInt91DispatchSession dispatch(
            title, *exact_driver, loaded_image, kind,
            millennium_dos_video_driver_load_->load_segment(),
            vector->interrupt_number, vector->offset, vector->segment,
            title.last_sequence + 1, mcga_count_argument);
        millennium_dos_video_int91_dispatch_ = std::move(dispatch);
        millennium_dos_video_int91_dispatch_generation_ =
            millennium_dos_video_driver_load_generation_;
        *native_runtime_memory_ = std::move(memory);
        result.accepted = true;
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}

MillenniumDosVideoFunctionZeroObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_video_function_zero_bios_result(
    const MillenniumDosVideoFunctionZeroBiosResult observation) {
    MillenniumDosVideoFunctionZeroObservationResult result;
    if (!active_ || !native_runtime_memory_ || !millennium_dos_video_int91_dispatch_
        || !millennium_dos_video_driver_load_
        || millennium_dos_video_int91_dispatch_generation_
            != millennium_dos_video_driver_load_generation_) {
        result.error = "Function-zero INT 10h result requires its active loaded-driver dispatch";
        return result;
    }
    auto next = *millennium_dos_video_int91_dispatch_;
    auto memory = *native_runtime_memory_;
    const auto prior_cache_write = next.cache_write();
    try {
        next.observe_int10_result(observation);
        const auto cache_write = next.cache_write();
        if (!prior_cache_write && cache_write) {
            const auto segment = next.dispatch().loaded_driver_segment;
            const NativeRuntimeLocation location{
                NativeRuntimeAddressSpace::dos_segmented, segment,
                cache_write->driver_local_address};
            const auto current = memory.read_byte(location);
            if (!current || *current != next.dispatch().initial_cached_mode) {
                throw std::runtime_error("Function-zero cache byte differs from the owned driver image");
            }
            NativeRuntimeEffectBatch batch{
                "millennium-dos-video-function-zero-cache-"
                    + std::to_string(millennium_dos_video_int91_dispatch_generation_)
                    + "-" + std::to_string(observation.sequence),
                true,
                {{1, location, MemoryTransferElementWidth::byte,
                    NativeRuntimeByteOrder::little_endian, cache_write->value}}};
            const auto applied = memory.apply(batch);
            if (!applied.accepted) throw std::runtime_error(applied.error);
        }
    } catch (const std::exception& error) {
        result.error = error.what();
        return result;
    }
    millennium_dos_video_int91_dispatch_ = std::move(next);
    *native_runtime_memory_ = std::move(memory);
    result.accepted = true;
    return result;
}

MillenniumDosVideoFunctionZeroObservationResult
ReleaseRuntimeCoordinator::service_millennium_dos_mcga_function_zero_boundary() {
    MillenniumDosVideoFunctionZeroObservationResult rejected;
    if (!active_ || !native_runtime_memory_ || !millennium_dos_video_int91_dispatch_
        || !millennium_dos_video_driver_load_
        || millennium_dos_video_int91_dispatch_generation_
            != millennium_dos_video_driver_load_generation_
        || millennium_dos_video_driver_load_->driver().kind
            != MillenniumDosVideoDriverKind::mcga) {
        rejected.error = "Eon MCGA mode policy requires its active loaded-driver dispatch";
        return rejected;
    }
    auto next = *millennium_dos_video_int91_dispatch_;
    auto memory = *native_runtime_memory_;
    const auto prior_cache_write = next.cache_write();
    if (!next.service_eon_mcga_function_zero_boundary()) {
        rejected.error = "Eon MCGA policy does not match the pending function-zero boundary";
        return rejected;
    }
    const auto cache_write = next.cache_write();
    if (!prior_cache_write && cache_write) {
        const auto segment = next.dispatch().loaded_driver_segment;
        const NativeRuntimeLocation location{
            NativeRuntimeAddressSpace::dos_segmented, segment,
            cache_write->driver_local_address};
        const auto current = memory.read_byte(location);
        if (!current || *current != next.dispatch().initial_cached_mode) {
            rejected.error = "Function-zero cache byte differs from the owned driver image";
            return rejected;
        }
        NativeRuntimeEffectBatch batch{
            "millennium-dos-video-function-zero-cache-eon-policy-"
                + std::to_string(millennium_dos_video_int91_dispatch_generation_)
                + "-" + std::to_string(next.function_zero().bios_results().back().sequence),
            true,
            {{1, location, MemoryTransferElementWidth::byte,
                NativeRuntimeByteOrder::little_endian, cache_write->value}}};
        const auto applied = memory.apply(batch);
        if (!applied.accepted) {
            rejected.error = applied.error;
            return rejected;
        }
    }
    millennium_dos_video_int91_dispatch_ = std::move(next);
    *native_runtime_memory_ = std::move(memory);
    rejected.accepted = true;
    return rejected;
}

MillenniumDosVideoFunctionZeroObservationResult
ReleaseRuntimeCoordinator::advance_millennium_dos_video_function_zero_success_postlude() {
    MillenniumDosVideoFunctionZeroObservationResult result;
    if (!active_ || !native_runtime_memory_ || !active_media_
        || !millennium_dos_video_int91_dispatch_ || !millennium_dos_video_driver_load_
        || millennium_dos_video_int91_dispatch_generation_ != millennium_dos_video_driver_load_generation_) {
        result.error = "Function-zero postlude requires its active loaded-driver dispatch";
        return result;
    }
    auto next = *millennium_dos_video_int91_dispatch_;
    try {
        next.advance_success_postlude_prefix();
    } catch (const std::exception& error) {
        result.error = error.what();
        return result;
    }
    millennium_dos_video_int91_dispatch_ = std::move(next);
    result.accepted = true;
    return result;
}

MillenniumDosVideoFunctionZeroObservationResult
ReleaseRuntimeCoordinator::advance_millennium_dos_video_function_zero_mcga_single_count_store() {
    MillenniumDosVideoFunctionZeroObservationResult result;
    if (!active_ || !native_runtime_memory_ || !active_media_
        || !millennium_dos_video_int91_dispatch_ || !millennium_dos_video_driver_load_
        || millennium_dos_video_int91_dispatch_generation_ != millennium_dos_video_driver_load_generation_) {
        result.error = "Function-zero MCGA store requires its active loaded-driver dispatch";
        return result;
    }
    auto next = *millennium_dos_video_int91_dispatch_;
    auto memory = *native_runtime_memory_;
    try {
        next.advance_mcga_single_count_store_prefix();
        const auto outcome = next.postlude_outcome();
        if (!outcome || outcome->endpoint != MillenniumDosVideoFunctionZeroPostludeEndpoint::mcga_ret
            || !outcome->local_write || next.dispatch().loaded_driver_segment
                != millennium_dos_video_driver_load_->load_segment()) {
            throw std::runtime_error("MCGA single-count prefix did not produce its exact local store");
        }
        const auto write = *outcome->local_write;
        const auto exact_driver = active_media_->borrow(
            "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228");
        if (!exact_driver || write.driver_local_address >= exact_driver->size()) {
            throw std::runtime_error("Exact MCGA image byte for single-count store is unavailable");
        }
        const NativeRuntimeLocation location{NativeRuntimeAddressSpace::dos_segmented,
            next.dispatch().loaded_driver_segment, write.driver_local_address};
        const auto current = memory.read_byte(location);
        if (!current || *current != (*exact_driver)[write.driver_local_address]) {
            throw std::runtime_error("MCGA single-count target differs from the owned driver image");
        }
        NativeRuntimeEffectBatch batch{
            "millennium-dos-video-function-zero-mcga-count-"
                + std::to_string(millennium_dos_video_int91_dispatch_generation_),
            true,
            {{1, location, MemoryTransferElementWidth::byte,
                NativeRuntimeByteOrder::little_endian, write.value}}};
        const auto applied = memory.apply(batch);
        if (!applied.accepted) throw std::runtime_error(applied.error);
    } catch (const std::exception& error) {
        result.error = error.what();
        return result;
    }
    millennium_dos_video_int91_dispatch_ = std::move(next);
    *native_runtime_memory_ = std::move(memory);
    result.accepted = true;
    return result;
}

MillenniumDosVideoFunctionZeroObservationResult
ReleaseRuntimeCoordinator::advance_millennium_dos_video_function_zero_mcga_return_chain() {
    MillenniumDosVideoFunctionZeroObservationResult result;
    if (!active_ || !native_runtime_memory_ || !millennium_dos_video_int91_dispatch_
        || !millennium_dos_video_driver_load_ || !millennium_dos_title_initialization_
        || !millennium_dos_title_child_compatibility_
        || millennium_dos_video_int91_dispatch_generation_ != millennium_dos_video_driver_load_generation_) {
        result.error = "MCGA return chain requires its active hash-bound title and driver state";
        return result;
    }
    if (millennium_dos_video_function_zero_return_chain_completed_) {
        result.error = "MCGA return chain has already completed";
        return result;
    }
    const auto checkpoint = millennium_dos_video_function_zero_checkpoint();
    if (!checkpoint || !checkpoint->postlude
        || checkpoint->postlude->endpoint != MillenniumDosVideoFunctionZeroPostludeEndpoint::mcga_ret
        || !checkpoint->stack_stop || !checkpoint->stack_stop->caller_return_word_value_known
        || checkpoint->stack_stop->near_return_ip != 0x0012) {
        result.error = "MCGA near-RET caller word is not verified in owned runtime memory";
        return result;
    }
    auto next_title = *millennium_dos_title_initialization_;
    const auto title = next_title.checkpoint();
    const auto child = millennium_dos_title_child_compatibility_->checkpoint();
    if (title.child_code_segment != child.allocation.segment || !checkpoint->outcome
        || checkpoint->outcome->verify_result.sequence == std::numeric_limits<std::uint64_t>::max()) {
        result.error = "MCGA return chain title allocation or sequence does not match";
        return result;
    }
    auto memory = *native_runtime_memory_;
    const auto word = [&](const std::uint16_t offset) -> std::optional<std::uint16_t> {
        const NativeRuntimeLocation low{NativeRuntimeAddressSpace::dos_segmented,
            child.allocation.segment,offset};
        auto high = low;
        ++high.offset;
        const auto lo = memory.read_byte(low);
        const auto hi = memory.read_byte(high);
        if (!lo || !hi) return std::nullopt;
        return static_cast<std::uint16_t>(*lo
            | (static_cast<std::uint16_t>(*hi) << 8U));
    };
    if (word(0xd9ec) != 0x0012 || word(0xd9ee) != 0x0129
        || word(0xd9f0) != child.allocation.segment
        || word(0xd9f4) != child.allocation.segment
        || word(0xd9fc) != child.allocation.segment
        || word(0xd9fe) != 0x1b98
        || word(0xd9f2).has_value() || word(0xd9f6).has_value()
        || word(0xd9f8).has_value() || word(0xd9fa).has_value()) {
        result.error = "MCGA stack frame differs from the known INT 91h and wrapper path";
        return result;
    }
    constexpr std::string_view mcga_hash =
        "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228";
    const auto exact_driver = active_media_->borrow(mcga_hash);
    if (!exact_driver || exact_driver->size() != millennium_dos_video_driver_load_->driver().byte_size) {
        result.error = "Hash-verified MCGA driver is unavailable for its handler return";
        return result;
    }
    for (std::size_t offset = 0; offset < 0x25; ++offset) {
        const auto actual = memory.read_byte({NativeRuntimeAddressSpace::dos_segmented,
            millennium_dos_video_driver_load_->load_segment(),offset});
        if (!actual || *actual != (*exact_driver)[offset]) {
            result.error = "MCGA dispatcher/IRET code differs from the verified image";
            return result;
        }
    }
    const auto handler_compare_value = (*exact_driver)[0x01e5];
    const auto handler_compare_memory = memory.read_byte({NativeRuntimeAddressSpace::dos_segmented,
        millennium_dos_video_driver_load_->load_segment(),0x01e5});
    if (handler_compare_value != 0 || handler_compare_memory != handler_compare_value) {
        result.error = "MCGA handler does not take the verified IRET branch for this state";
        return result;
    }
    const NativeRuntimeLocation handler_state{NativeRuntimeAddressSpace::dos_segmented,
        millennium_dos_video_driver_load_->load_segment(),0x01e4};
    NativeRuntimeEffectBatch handler_batch{
        "millennium-dos-int91-mcga-handler-" + std::to_string(millennium_dos_video_int91_dispatch_generation_),
        true, {{1,handler_state,MemoryTransferElementWidth::byte,
            NativeRuntimeByteOrder::little_endian,0}}};
    const auto handler_applied = memory.apply(handler_batch);
    if (!handler_applied.accepted) {
        result.error = handler_applied.error;
        return result;
    }
    const MillenniumDosTitlePrivateInterruptResultObservation returned{
        checkpoint->outcome->verify_result.sequence + 1U,0x0127,0x0129,
        checkpoint->postlude->ax,0,0,0,{},false};
    const auto prior_effects = title.memory_effects.size();
    try {
        next_title.observe_private_interrupt_result_after_owned_dispatch(returned);
        next_title.execute_selected_callee_start(
            returned.sequence + 1U,0x1bad,0x1ac6);
    } catch (const std::exception& error) {
        result.error = error.what();
        return result;
    }
    const auto completed_title = next_title.checkpoint();
    NativeRuntimeEffectBatch title_batch{
        "millennium-dos-mcga-title-return-" + std::to_string(millennium_dos_video_int91_dispatch_generation_),
        true,{}};
    for (std::size_t index = prior_effects; index < completed_title.memory_effects.size(); ++index) {
        const auto& effect = completed_title.memory_effects[index];
        title_batch.effects.push_back({title_batch.effects.size()+1,
            {NativeRuntimeAddressSpace::dos_segmented,
                effect.explicit_segment ? effect.segment : completed_title.child_code_segment,
                effect.offset},
            effect.width == MillenniumDosTitleInitializationEffectWidth::byte
                ? MemoryTransferElementWidth::byte : MemoryTransferElementWidth::word,
            NativeRuntimeByteOrder::little_endian,effect.value});
    }
    if (!title_batch.effects.empty()) {
        const auto title_applied = memory.apply(title_batch);
        if (!title_applied.accepted) {
            result.error = title_applied.error;
            return result;
        }
    }
    *native_runtime_memory_ = std::move(memory);
    millennium_dos_title_initialization_ = std::move(next_title);
    millennium_dos_video_function_zero_return_chain_completed_ = true;
    result.accepted = true;
    return result;
}

std::optional<MillenniumDosVideoFunctionZeroCheckpoint>
ReleaseRuntimeCoordinator::millennium_dos_video_function_zero_checkpoint() const {
    if (!millennium_dos_video_int91_dispatch_
        || millennium_dos_video_int91_dispatch_generation_ != millennium_dos_video_driver_load_generation_) {
        return std::nullopt;
    }
    const auto& session = *millennium_dos_video_int91_dispatch_;
    MillenniumDosVideoFunctionZeroCheckpoint checkpoint{
        millennium_dos_video_int91_dispatch_generation_, session.dispatch(), session.state(),
        session.boundary(), session.cache_write(), session.outcome(), session.postlude_outcome(), {}};
    if (checkpoint.postlude
        && checkpoint.postlude->endpoint
            == MillenniumDosVideoFunctionZeroPostludeEndpoint::mcga_ret) {
        std::optional<MillenniumDosTitleInitializationCheckpoint> title;
        if (millennium_dos_title_initialization_)
            title = millennium_dos_title_initialization_->checkpoint();
        const auto child = millennium_dos_title_child_compatibility_
            ? std::optional{millennium_dos_title_child_compatibility_->checkpoint()}
            : std::nullopt;
        const auto paragraphs = child
            ? child->allocation.paragraph_count : std::uint16_t{0};
        const bool same_segment = title && child
            && title->child_code_segment == child->allocation.segment;
        const auto backed_bytes = static_cast<std::uint32_t>(paragraphs) * 16U;
        const NativeRuntimeLocation return_ip_location{
            NativeRuntimeAddressSpace::dos_segmented,
            same_segment ? std::optional<std::uint16_t>{child->allocation.segment}
                         : std::nullopt,
            0xd9ec};
        const auto return_ip_low = native_runtime_memory_
            ? native_runtime_memory_->read_byte(return_ip_location) : std::nullopt;
        auto return_ip_high_location = return_ip_location;
        return_ip_high_location.offset++;
        const auto return_ip_high = native_runtime_memory_
            ? native_runtime_memory_->read_byte(return_ip_high_location) : std::nullopt;
        const bool return_word_backed = same_segment && backed_bytes >= 0xd9eeU
            && return_ip_low.has_value() && return_ip_high.has_value();
        const auto return_word = return_word_backed
            ? static_cast<std::uint16_t>(*return_ip_low
                | (static_cast<std::uint16_t>(*return_ip_high) << 8U))
            : std::uint16_t{0};
        checkpoint.stack_stop = MillenniumDosVideoFunctionZeroCheckpoint::StackStopBoundary{
            0x024b,title ? title->child_code_segment : std::uint16_t{0},0xd9ec,0x0da0,
            paragraphs,return_word,return_word_backed,return_word_backed};
        auto& stack = *checkpoint.stack_stop;
        stack.mcga_return_chain_completed = millennium_dos_video_function_zero_return_chain_completed_;
        stack.flags_value_available = same_segment && backed_bytes >= 0x0d9f4U
            && native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::dos_segmented,
                child->allocation.segment,0xd9f2}).has_value()
            && native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::dos_segmented,
                child->allocation.segment,0xd9f3}).has_value();
        const auto saved_es_low = native_runtime_memory_->read_byte({
            NativeRuntimeAddressSpace::dos_segmented,child->allocation.segment,0xd9f4});
        const auto saved_es_high = native_runtime_memory_->read_byte({
            NativeRuntimeAddressSpace::dos_segmented,child->allocation.segment,0xd9f5});
        const auto saved_ds_low = native_runtime_memory_->read_byte({
            NativeRuntimeAddressSpace::dos_segmented,child->allocation.segment,0xd9fc});
        const auto saved_ds_high = native_runtime_memory_->read_byte({
            NativeRuntimeAddressSpace::dos_segmented,child->allocation.segment,0xd9fd});
        if (saved_ds_low && saved_ds_high)
            stack.wrapper_saved_ds = static_cast<std::uint16_t>(*saved_ds_low
                | (static_cast<std::uint16_t>(*saved_ds_high) << 8U));
        if (saved_es_low && saved_es_high)
            stack.wrapper_saved_es = static_cast<std::uint16_t>(*saved_es_low
                | (static_cast<std::uint16_t>(*saved_es_high) << 8U));
        stack.wrapper_known_saved_segments_match = same_segment
            && saved_ds_low && saved_ds_high && saved_es_low && saved_es_high
            && stack.wrapper_saved_ds == child->allocation.segment
            && stack.wrapper_saved_es == child->allocation.segment;
    }
    return checkpoint;
}

std::optional<MillenniumDosCompatibilityRunnerCheckpoint>
ReleaseRuntimeCoordinator::tick_millennium_dos_compatibility_runner() {
  if (active_ && session_snapshot_ &&
      session_snapshot_->kind == RuntimeSessionKind::millennium_dos_title &&
      millennium_dos_title_initialization_ &&
      millennium_dos_compatibility_runner_ && native_runtime_memory_) {
    auto next = *millennium_dos_title_initialization_;
    auto runner = *millennium_dos_compatibility_runner_;
    auto memory = *native_runtime_memory_;
    const auto initial_effects = next.checkpoint().memory_effects.size();
    try {
      runner.synchronize_external_sequence(next.checkpoint().last_sequence);
      constexpr std::string_view library_hash =
          "6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678";
      if (!active_media_) throw std::runtime_error("Active verified media is unavailable");
      const auto& media = *active_media_;
      const auto library = media.borrow(library_hash);
      if (!library)
        throw std::runtime_error(
            "Exact TITLE.LIB compatibility source is unavailable");
      while (true) {
        const auto c = next.checkpoint();
        runner.synchronize_external_sequence(c.last_sequence);
        const auto sequence = runner.next_sequence();
        const auto memory_state =
            c.state >= MillenniumDosTitleInitializationState::
                           dos_resize_result_boundary &&
            c.state <= MillenniumDosTitleInitializationState::
                           dos_second_buffer_allocation_result_boundary;
        const auto library_memory =
            c.state == MillenniumDosTitleInitializationState::
                           dos_file_sized_allocation_result_boundary ||
            c.state == MillenniumDosTitleInitializationState::
                           dos_single_paragraph_allocation_result_boundary ||
            c.state == MillenniumDosTitleInitializationState::
                           dos_scratch_allocation_result_boundary ||
            c.state == MillenniumDosTitleInitializationState::
                           dos_scratch_free_result_boundary;
        if (memory_state || library_memory) {
          const bool allocation = c.dos_boundary.service == 0x48;
          bool carry = false;
          std::uint16_t ax = 0, bx = c.dos_boundary.bx;
          if (allocation) {
            const auto allocated =
                runner.allocate_title_paragraphs(c.dos_boundary.bx);
            if (allocated.allocation)
              ax = allocated.allocation->segment;
            else {
              carry = true;
              ax = 8;
              bx = runner.remaining_title_paragraphs();
              runner.record_automatic_operation();
            }
          } else {
            // No allocation owned by this arena is released at these sites.
            carry = true;
            ax = 9;
            runner.record_automatic_operation();
          }
          next.observe_dos_memory_result(
              {sequence, c.dos_boundary.interrupt_address,
               c.dos_boundary.return_address, carry, ax, bx,
               static_cast<std::uint16_t>(carry ? 0x0203 : 0x0202)});
          continue;
        }
        const bool file_state =
            c.state == MillenniumDosTitleInitializationState::
                           dos_file_open_result_boundary ||
            c.state == MillenniumDosTitleInitializationState::
                           dos_file_seek_result_boundary ||
            c.state == MillenniumDosTitleInitializationState::
                           dos_file_close_result_boundary ||
            c.state == MillenniumDosTitleInitializationState::
                           dos_library_open_result_boundary ||
            c.state == MillenniumDosTitleInitializationState::
                           dos_library_read_result_boundary ||
            c.state == MillenniumDosTitleInitializationState::
                           dos_library_close_result_boundary;
        if (file_state) {
          std::uint16_t ax = 0, cx = c.dos_boundary.cx, dx = c.dos_boundary.dx;
          if (c.dos_boundary.service == 0x3d)
            ax = runner.compatibility_file_handle();
          else if (c.dos_boundary.service == 0x42) {
            ax = static_cast<std::uint16_t>(library->size());
            dx = 0;
          } else if (c.dos_boundary.service == 0x3f) {
            std::size_t consumed = 0;
            for (const auto &receipt : c.dos_file_results)
              if (receipt.interrupt_address == 0x057c && !receipt.carry)
                consumed += receipt.ax;
            ax = consumed >= library->size()
                     ? 0
                     : static_cast<std::uint16_t>(std::min<std::size_t>(
                           cx, library->size() - consumed));
          }
          next.observe_dos_file_result({sequence,
                                        c.dos_boundary.interrupt_address,
                                        c.dos_boundary.return_address, false,
                                        ax, c.dos_boundary.bx, cx, dx, 0x0202},
                                       *library);
          runner.record_automatic_operation();
          const auto reached = next.checkpoint();
          if (reached.state == MillenniumDosTitleInitializationState::
                                   library_relocation_complete) {
            next.execute_post_relocation(runner.next_sequence(), *library);
            runner.record_automatic_operation();
          }
          const auto relocated = next.checkpoint();
          if (relocated.state == MillenniumDosTitleInitializationState::
                                     post_library_setup_call_boundary) {
            next.execute_post_library_setup(runner.next_sequence(), 0x1bef,
                                            0x1aac);
            runner.record_automatic_operation();
          }
          continue;
        }
        break;
      }
      const auto completed = next.checkpoint();
      // This is one atomic compatibility service, while recovered
      // instructions may revisit a byte.  Normalize to its final byte state
      // before admission: last writer wins exactly as the sequential
      // instruction stream does, and the native-memory batch remains bounded
      // and non-overlapping.
      std::map<NativeRuntimeLocation, std::uint8_t> final_bytes;
      for (std::size_t i = initial_effects; i < completed.memory_effects.size();
           ++i) {
        const auto &e = completed.memory_effects[i];
        const auto location = NativeRuntimeLocation{
            NativeRuntimeAddressSpace::dos_segmented,
            (e.explicit_segment || e.segment != 0) ? e.segment
                                                    : completed.child_code_segment,
            e.offset};
        final_bytes[location] = static_cast<std::uint8_t>(e.value & 0xffU);
        if (e.width == MillenniumDosTitleInitializationEffectWidth::word) {
          auto high = location;
          ++high.offset;
          final_bytes[high] = static_cast<std::uint8_t>(e.value >> 8U);
        }
      }
      if (!final_bytes.empty()) {
        NativeRuntimeEffectBatch batch{
            "millennium-dos-title-compatibility-chain-" +
                std::to_string(completed.last_sequence),
            true,
            {}};
        batch.effects.reserve(final_bytes.size());
        for (const auto &[location, value] : final_bytes) {
          batch.effects.push_back(
              {batch.effects.size() + 1, location,
               MemoryTransferElementWidth::byte,
               NativeRuntimeByteOrder::little_endian, value});
        }
        const auto applied = memory.apply(batch);
        if (!applied.accepted)
          throw std::runtime_error(applied.error);
      }
      millennium_dos_title_initialization_ = std::move(next);
      millennium_dos_compatibility_runner_ = std::move(runner);
      *native_runtime_memory_ = std::move(memory);
      auto out = millennium_dos_compatibility_runner_->checkpoint(
          MillenniumDosSoundDriverLoadState::title_exec_requested, {});
      out.title_state = completed.state;
      out.external_result_required = true;
      return out;
    } catch (const std::exception &e) {
      auto out = millennium_dos_compatibility_runner_->checkpoint(
          MillenniumDosSoundDriverLoadState::title_exec_requested, {},
          e.what());
      out.title_state =
          millennium_dos_title_initialization_->checkpoint().state;
      return out;
    }
  }
  if (!active_ || !session_snapshot_ ||
      session_snapshot_->kind !=
          RuntimeSessionKind::millennium_dos_sound_driver_boundary ||
      !millennium_dos_sound_driver_load_ ||
      !millennium_dos_compatibility_runner_ || !native_runtime_memory_)
    return std::nullopt;
  auto next = *millennium_dos_sound_driver_load_;
  auto runner = *millennium_dos_compatibility_runner_;
  auto memory = *native_runtime_memory_;
  auto next_video = millennium_dos_video_driver_load_;
  auto next_video_generation = millennium_dos_video_driver_load_generation_;
  bool video_state_changed = false;
  auto pending_title_entry = millennium_dos_title_exec_entry_;
  std::unique_ptr<MillenniumDosTitleSession> pending_title;
  try {
    // Native Eon has no DOS PSP command tail or video BIOS detector. Select
    // the MCGA driver profile explicitly as Eon's default runtime policy,
    // then run MILL.COM's hash-bound file/SetVect route in the same private
    // compatibility process before continuing to the sound-driver path.
    // This is not reported as an original command-tail or detector result.
    if (!next_video) {
      constexpr std::string_view mill_sha =
          "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e";
      constexpr std::string_view titles_sha =
          "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
      constexpr std::string_view mcga_sha =
          "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228";
      if (!active_media_) throw std::runtime_error("Verified video-loader media is unavailable");
      const auto mill = active_media_->borrow(mill_sha);
      const auto titles = active_media_->borrow(titles_sha);
      const auto mcga = active_media_->borrow(mcga_sha);
      if (!mill || !titles || !mcga)
        throw std::runtime_error("Exact English MCGA loader media is unavailable");
      next_video.emplace(*titles, *mill, *mcga, MillenniumDosVideoDriverKind::mcga,
          MillenniumDosVideoSelectorSource::eon_native_default, 2, 0, 0);
      ++next_video_generation;
      video_state_changed = true;
    }
    if (next_video->state() == MillenniumDosVideoDriverLoadState::awaiting_open_result) {
      next_video->observe_open_result(0x02d2, false, runner.compatibility_file_handle());
      runner.record_automatic_operation();
      next_video->observe_seek_end_result(0x02eb, false, runner.compatibility_file_handle(),
          static_cast<std::uint16_t>(next_video->driver().byte_size), 0);
      runner.record_automatic_operation();
      const auto allocation = runner.allocate_paragraphs(
          static_cast<std::uint32_t>((next_video->driver().byte_size + 15U) / 16U));
      if (!allocation.allocation) throw std::runtime_error(allocation.error);
      next_video->observe_allocation_result(0x02fa, false, allocation.allocation->segment);
      next_video->observe_seek_start_result(0x0309, false,
          runner.compatibility_file_handle(), 0, 0);
      runner.record_automatic_operation();
      next_video->observe_read_result(0x0313, false, runner.compatibility_file_handle(),
          static_cast<std::uint16_t>(next_video->driver().byte_size));
      NativeRuntimeEffectBatch image_batch{
          "millennium-dos-video-driver-" + std::to_string(next_video_generation) + "-image",
          true, {}};
      image_batch.effects.reserve(next_video->memory_effects().size());
      for (const auto& effect : next_video->memory_effects())
        image_batch.effects.push_back({image_batch.effects.size() + 1,
            {NativeRuntimeAddressSpace::dos_segmented, effect.segment, effect.offset},
            MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::little_endian,
            effect.value});
      const auto image_applied = memory.apply(image_batch);
      if (!image_applied.accepted) throw std::runtime_error(image_applied.error);
      runner.record_automatic_operation();
      next_video->observe_close_result(0x0319, false, runner.compatibility_file_handle());
      runner.record_automatic_operation();
      next_video->install_compatibility_set_vector(0x020c, 0x2591, 0,
          allocation.allocation->segment);
      NativeRuntimeEffectBatch vector_batch{
          "millennium-dos-video-driver-" + std::to_string(next_video_generation)
              + "-compat-int91-vector",
          true,
          {{1, {NativeRuntimeAddressSpace::dos_segmented, std::uint16_t{0}, 0x244},
              MemoryTransferElementWidth::word, NativeRuntimeByteOrder::little_endian, 0},
           {2, {NativeRuntimeAddressSpace::dos_segmented, std::uint16_t{0}, 0x246},
              MemoryTransferElementWidth::word, NativeRuntimeByteOrder::little_endian,
              allocation.allocation->segment}}};
      const auto vector_applied = memory.apply(vector_batch);
      if (!vector_applied.accepted) throw std::runtime_error(vector_applied.error);
      runner.record_automatic_operation();
      video_state_changed = true;
    }
    // These results are properties of the admitted immutable leaf and of
    // Eon's private one-leaf compatibility service. No guest DOS state is
    // inferred. Run until the next operation needs external process state.
    while (true) {
      switch (next.state()) {
      case MillenniumDosSoundDriverLoadState::awaiting_open_result:
        next.observe_open_result(0x02d2, false,
                                 runner.compatibility_file_handle());
        runner.record_automatic_operation();
        continue;
      case MillenniumDosSoundDriverLoadState::awaiting_seek_end_result:
        next.observe_seek_end_result(
            0x02eb, false, runner.compatibility_file_handle(),
            static_cast<std::uint16_t>(next.driver().byte_size), 0);
        runner.record_automatic_operation();
        continue;
      case MillenniumDosSoundDriverLoadState::awaiting_allocation_result: {
        const auto paragraphs =
            static_cast<std::uint32_t>((next.driver().byte_size + 15U) / 16U);
        const auto allocated = runner.allocate_paragraphs(paragraphs);
        if (!allocated.allocation)
          throw std::runtime_error(allocated.error);
        next.observe_allocation_result(0x02fa, false,
                                       allocated.allocation->segment);
        continue;
      }
      case MillenniumDosSoundDriverLoadState::awaiting_seek_start_result:
        next.observe_seek_start_result(
            0x0309, false, runner.compatibility_file_handle(), 0, 0);
        runner.record_automatic_operation();
        continue;
      case MillenniumDosSoundDriverLoadState::awaiting_read_result: {
        next.observe_read_result(
            0x0313, false, runner.compatibility_file_handle(),
            static_cast<std::uint16_t>(next.driver().byte_size));
        NativeRuntimeEffectBatch batch{
            "millennium-dos-sound-driver-" +
                std::to_string(millennium_dos_sound_driver_load_generation_) +
                "-image",
            true,
            {}};
        batch.effects.reserve(next.memory_effects().size());
        for (std::size_t i = 0; i < next.memory_effects().size(); ++i) {
          const auto &e = next.memory_effects()[i];
          batch.effects.push_back(
              {i + 1,
               {NativeRuntimeAddressSpace::dos_segmented, e.segment, e.offset},
               MemoryTransferElementWidth::byte,
               NativeRuntimeByteOrder::little_endian,
               e.value});
        }
        const auto applied = memory.apply(batch);
        if (!applied.accepted)
          throw std::runtime_error(applied.error);
        runner.record_automatic_operation();
        continue;
      }
      case MillenniumDosSoundDriverLoadState::awaiting_close_result:
        next.observe_close_result(0x0319, false,
                                  runner.compatibility_file_handle());
        runner.record_automatic_operation();
        continue;
      case MillenniumDosSoundDriverLoadState::awaiting_vector_install: {
        // MILL.COM's hash-bound request is DOS INT 21h/AH=25h for INT 95h,
        // with DS:DX pointing to the loaded driver at offset zero. Apply that
        // deterministic SetVect operation to this Eon compatibility process;
        // keep its provenance distinct from an observed original IVT value.
        next.install_compatibility_set_vector(
            0x0239, 0x2595, 0, next.load_segment());
        const auto* vector = next.installed_vector()
            ? &*next.installed_vector() : nullptr;
        if (!vector) throw std::runtime_error("DOS compatibility SetVect did not install INT 95h");
        NativeRuntimeEffectBatch batch{
            "millennium-dos-sound-driver-" +
                std::to_string(millennium_dos_sound_driver_load_generation_) +
                "-compat-int95-vector",
            true,
            {{1,
              {NativeRuntimeAddressSpace::dos_segmented, std::uint16_t{0}, 0x254},
              MemoryTransferElementWidth::word,
              NativeRuntimeByteOrder::little_endian,
              vector->offset},
             {2,
              {NativeRuntimeAddressSpace::dos_segmented, std::uint16_t{0}, 0x256},
              MemoryTransferElementWidth::word,
              NativeRuntimeByteOrder::little_endian,
              vector->segment}}};
        const auto applied = memory.apply(batch);
        if (!applied.accepted) throw std::runtime_error(applied.error);
        runner.record_automatic_operation();
        continue;
      }
      case MillenniumDosSoundDriverLoadState::awaiting_parent_stack:
        // MILL.COM starts with SP=$0100. Its selected-driver call at $0240
        // pushes the return address used by helper $031c, so the hash-bound
        // store at $032f writes SP=$00fe through DS=CS.
        next.install_compatibility_parent_stack(
            0x032f, 0x05f7, 0x00fe, runner.code_segment());
        {
          const auto &effects = next.runtime_word_effects();
          NativeRuntimeEffectBatch batch{
              "millennium-dos-sound-driver-" +
                  std::to_string(millennium_dos_sound_driver_load_generation_) +
                  "-compat-parent-stack",
              true,
              {}};
          batch.effects.reserve(effects.size());
          for (std::size_t index = 0; index < effects.size(); ++index) {
            const auto &effect = effects[index];
            batch.effects.push_back(
                {index + 1,
                 {NativeRuntimeAddressSpace::dos_segmented, effect.segment,
                  effect.address},
                 MemoryTransferElementWidth::word,
                 NativeRuntimeByteOrder::little_endian, effect.value});
          }
          const auto applied = memory.apply(batch);
          if (!applied.accepted) throw std::runtime_error(applied.error);
        }
        runner.record_automatic_operation();
        continue;
      case MillenniumDosSoundDriverLoadState::title_exec_boundary: {
        if (!millennium_dos_ || !millennium_dos_->title_flow)
          throw std::runtime_error("English title flow is unavailable");
        if (!active_media_) throw std::runtime_error("Active verified media is unavailable");
        const auto& media = *active_media_;
        const auto mill = media.borrow(
            "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e");
        const auto titles = media.borrow(
            "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6");
        if (!mill || !titles)
          throw std::runtime_error(
              "Exact TITLES.EXE process media is unavailable");
        pending_title_entry.emplace(*mill, *titles);
        pending_title = std::make_unique<MillenniumDosTitleSession>(
            *millennium_dos_->title_flow);
        // Exact literal request after the observed SP store. This does
        // not claim that DOS returned or that the child has completed.
        next.observe_title_exec_request(0x0336, 0x4b00, 0x068f, 0x067a);
        runner.record_automatic_operation();
        continue;
      }
      default:
        if (next.state() ==
                MillenniumDosSoundDriverLoadState::title_exec_requested &&
            pending_title_entry &&
            pending_title_entry->state() == MillenniumDosTitleExecEntryState::
                                                awaiting_child_process_entry) {
          constexpr std::string_view titles_sha =
              "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe"
              "6";
          if (!active_media_) throw std::runtime_error("Active verified media is unavailable");
          const auto& media = *active_media_;
          const auto titles = media.borrow(titles_sha);
          if (!titles)
            throw std::runtime_error(
                "Exact TITLES.EXE compatibility child leaf is unavailable");
          const auto allocated = runner.allocate_paragraphs(
              MillenniumDosTitleChildCompatibilityService::
                  required_paragraphs());
          if (!allocated.allocation)
            throw std::runtime_error(allocated.error);
          MillenniumDosTitleChildCompatibilityService child(
              *titles, *allocated.allocation);
          NativeRuntimeEffectBatch batch{
              "millennium-dos-title-child-" +
                  std::to_string(millennium_dos_sound_driver_load_generation_) +
                  "-image",
              true,
              {}};
          batch.effects.reserve(child.image_effects().size());
          for (const auto &effect : child.image_effects()) {
            batch.effects.push_back(
                {batch.effects.size() + 1,
                 {NativeRuntimeAddressSpace::dos_segmented,
                  allocated.allocation->segment, effect.offset},
                 MemoryTransferElementWidth::byte,
                 NativeRuntimeByteOrder::little_endian,
                 effect.value});
          }
          const auto applied = memory.apply(batch);
          if (!applied.accepted)
            throw std::runtime_error(applied.error);
          auto entry = *pending_title_entry;
          const auto child_entry_sequence = runner.next_sequence();
          entry.observe_child_process_entry(
              {child_entry_sequence, 0x0336, 0x4b00, 0x068f, 0x067a, 0x0100,
               allocated.allocation->segment,
               MillenniumDosTitleExecEntryProvenance::
                   eon_dos_compatibility_service});
          runner.record_automatic_operation();
          const auto prefix_sequence = runner.next_sequence();
          entry.execute_exact_entry_prefix(prefix_sequence, 0x0100, 0x0104,
                                           0x1b80);
          runner.record_automatic_operation();
          MillenniumDosTitleInitializationSession initialization(
              *titles, allocated.allocation->segment, prefix_sequence);
          const auto initialization_sequence = runner.next_sequence();
          initialization.execute_exact_startup(initialization_sequence, 0x1b80,
                                               0x1b95, 0x0122, 0x91);
          runner.record_automatic_operation();
          pending_title_entry = std::move(entry);
          millennium_dos_title_child_compatibility_.emplace(std::move(child));
          millennium_dos_title_initialization_.emplace(
              std::move(initialization));
          session_snapshot_ = make_runtime_session_snapshot(
              *active_, RuntimeSessionKind::millennium_dos_title);
        }
        millennium_dos_title_exec_entry_ = std::move(pending_title_entry);
        if (pending_title)
          millennium_dos_title_ = std::move(pending_title);
        if (video_state_changed) {
          millennium_dos_video_driver_load_ = std::move(next_video);
          millennium_dos_video_driver_load_generation_ = next_video_generation;
          millennium_dos_video_driver_load_last_sequence_ =
              runner.checkpoint(next.state(), next.boundary()).last_sequence;
        }
        millennium_dos_sound_driver_load_ = std::move(next);
        millennium_dos_compatibility_runner_ = std::move(runner);
        *native_runtime_memory_ = std::move(memory);
        auto checkpoint = millennium_dos_compatibility_runner_->checkpoint(
            millennium_dos_sound_driver_load_->state(),
            millennium_dos_sound_driver_load_->boundary());
        if (millennium_dos_title_initialization_) {
          checkpoint.external_result_required = true;
        }
        millennium_dos_sound_driver_load_last_sequence_ =
            checkpoint.last_sequence;
        return checkpoint;
      }
    }
    if (video_state_changed) {
      millennium_dos_video_driver_load_ = std::move(next_video);
      millennium_dos_video_driver_load_generation_ = next_video_generation;
      millennium_dos_video_driver_load_last_sequence_ =
          runner.checkpoint(next.state(), next.boundary()).last_sequence;
    }
    millennium_dos_sound_driver_load_ = std::move(next);
    millennium_dos_compatibility_runner_ = std::move(runner);
    *native_runtime_memory_ = std::move(memory);
    auto checkpoint = millennium_dos_compatibility_runner_->checkpoint(
        millennium_dos_sound_driver_load_->state(),
        millennium_dos_sound_driver_load_->boundary());
    millennium_dos_sound_driver_load_last_sequence_ = checkpoint.last_sequence;
    return checkpoint;
  } catch (const std::exception &e) {
    return millennium_dos_compatibility_runner_->checkpoint(
        millennium_dos_sound_driver_load_->state(),
        millennium_dos_sound_driver_load_->boundary(), e.what());
  }
}

bool ReleaseRuntimeCoordinator::advance_millennium_dos_title_local_continuation(
    std::string& error) {
  if (!session_snapshot_ ||
      session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title ||
      !millennium_dos_title_initialization_) {
    return false;
  }
  auto next = *millennium_dos_title_initialization_;
  auto memory = native_runtime_memory_ ? std::optional{*native_runtime_memory_}
                                       : std::nullopt;
  const auto prior_effects = next.checkpoint().memory_effects.size();
  bool memory_effects_applied = false;
  try {
    const auto checkpoint = next.checkpoint();
    bool native_ega_function_nineteen_advanced = false;
    bool native_mcga_function_nineteen_advanced = false;
    if (checkpoint.state
            == MillenniumDosTitleInitializationState::post_video_private_interrupt_result_boundary
        && checkpoint.boundary.interrupt_address == 0x0127
        && checkpoint.boundary.interrupt == 0x91
        && checkpoint.boundary.function == 0x0019
        && checkpoint.boundary.record_segment == checkpoint.child_code_segment
        && checkpoint.boundary.record_offset == 0x0fe9
        && active_media_ && memory && millennium_dos_video_driver_load_
        && millennium_dos_video_driver_load_->driver().kind
            == MillenniumDosVideoDriverKind::ega640
        && millennium_dos_video_driver_load_->state()
            == MillenniumDosVideoDriverLoadState::ivt_observation_recorded
        && millennium_dos_video_driver_load_->ivt_observation_matches_loaded_driver()) {
      constexpr std::string_view ega_sha =
          "ba003dd155fee868980f6ece933c33f9b22af68ed376cd64f4e027abd65baf6a";
      const auto exact_driver = active_media_->borrow(ega_sha);
      if (exact_driver && millennium_dos_video_driver_load_->ivt_observation()) {
        std::vector<std::uint8_t> loaded_driver;
        loaded_driver.reserve(millennium_dos_video_driver_load_->driver().byte_size);
        for (std::size_t offset = 0;
            offset < millennium_dos_video_driver_load_->driver().byte_size; ++offset) {
          const auto value = memory->read_byte({NativeRuntimeAddressSpace::dos_segmented,
              millennium_dos_video_driver_load_->load_segment(), offset});
          if (!value) {
            loaded_driver.clear();
            break;
          }
          loaded_driver.push_back(*value);
        }
        const auto native_return = execute_millennium_dos_ega_function_nineteen(
            *exact_driver, loaded_driver, checkpoint.boundary.function);
        if (native_return) {
          next.execute_post_video_ega_function_nineteen_return(
              checkpoint.last_sequence + 1U, 0x0127, 0x0129,
              native_return->driver_entry_address,
              native_return->handler_dispatch_offset, native_return->handler_return_ax);
          const auto returned = next.checkpoint();
          next.execute_post_video_followup(returned.last_sequence + 1U,
              0x1c17, 0x1725);
          native_ega_function_nineteen_advanced = true;
        }
      }
    }
    if (!native_ega_function_nineteen_advanced
        && checkpoint.state
            == MillenniumDosTitleInitializationState::post_video_private_interrupt_result_boundary
        && checkpoint.boundary.interrupt_address == 0x0127
        && checkpoint.boundary.interrupt == 0x91
        && checkpoint.boundary.function == 0x0019
        && checkpoint.boundary.record_segment == checkpoint.child_code_segment
        && checkpoint.boundary.record_offset == 0x0fe9
        && active_media_ && memory && millennium_dos_video_driver_load_
        && millennium_dos_video_driver_load_->driver().kind
            == MillenniumDosVideoDriverKind::mcga
        && millennium_dos_video_driver_load_->state()
            == MillenniumDosVideoDriverLoadState::ivt_observation_recorded
        && millennium_dos_video_driver_load_->ivt_observation_matches_loaded_driver()) {
      constexpr std::string_view titles_sha =
          "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
      constexpr std::string_view mcga_sha =
          "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228";
      const auto exact_titles = active_media_->borrow(titles_sha);
      const auto exact_driver = active_media_->borrow(mcga_sha);
      if (exact_titles && exact_driver
          && millennium_dos_video_driver_load_->ivt_observation()) {
        const auto load_segment = millennium_dos_video_driver_load_->load_segment();
        std::vector<std::uint8_t> loaded_driver;
        loaded_driver.reserve(millennium_dos_video_driver_load_->driver().byte_size);
        const auto read_range = [&memory](const std::uint16_t segment,
            const std::uint16_t offset, const std::size_t size)
            -> std::optional<std::vector<std::uint8_t>> {
          std::vector<std::uint8_t> bytes;
          bytes.reserve(size);
          for (std::size_t index = 0; index < size; ++index) {
            const auto address = static_cast<std::uint32_t>(offset) + index;
            if (address >= 0x10000U) return std::nullopt;
            const auto value = memory->read_byte({NativeRuntimeAddressSpace::dos_segmented,
                segment, static_cast<std::uint32_t>(address)});
            if (!value) return std::nullopt;
            bytes.push_back(*value);
          }
          return bytes;
        };
        for (std::size_t offset = 0;
            offset < millennium_dos_video_driver_load_->driver().byte_size; ++offset) {
          const auto value = memory->read_byte({NativeRuntimeAddressSpace::dos_segmented,
              load_segment, offset});
          if (!value) {
            loaded_driver.clear();
            break;
          }
          loaded_driver.push_back(*value);
        }
        const auto call_record = read_range(checkpoint.child_code_segment, 0x0fe9, 10);
        const auto palette_source = read_range(checkpoint.child_code_segment, 0x014c, 0x02fe);
        const auto compare_flag = memory->read_byte({NativeRuntimeAddressSpace::dos_segmented,
            load_segment, 0x09c7});
        const auto dispatch_flag = memory->read_byte({NativeRuntimeAddressSpace::dos_segmented,
            load_segment, 0x01e5});
        const auto palette_source_matches_owned_setup = [&]() {
          if (!palette_source) return false;
          for (std::size_t index = 0; index < palette_source->size(); ++index) {
            const auto address = static_cast<std::uint16_t>(0x014cU + index);
            std::optional<std::uint8_t> expected;
            for (auto effect = checkpoint.memory_effects.rbegin();
                effect != checkpoint.memory_effects.rend(); ++effect) {
              const auto segment = (effect->explicit_segment || effect->segment != 0)
                  ? effect->segment : checkpoint.child_code_segment;
              if (segment != checkpoint.child_code_segment) continue;
              if (effect->offset == address) {
                expected = static_cast<std::uint8_t>(effect->value);
                break;
              }
              if (effect->width == MillenniumDosTitleInitializationEffectWidth::word
                  && static_cast<unsigned>(effect->offset) + 1U == address) {
                expected = static_cast<std::uint8_t>(effect->value >> 8U);
                break;
              }
            }
            if (!expected || *expected != (*palette_source)[index]) return false;
          }
          return true;
        };
        if (call_record && palette_source && compare_flag && dispatch_flag
            && palette_source_matches_owned_setup()) {
          const auto native_return = execute_millennium_dos_mcga_function_nineteen(
              *exact_driver, loaded_driver, *exact_titles, *call_record, *palette_source,
              checkpoint.child_code_segment, 0xd9ea, checkpoint.boundary.function,
              *compare_flag, *dispatch_flag);
          if (native_return) {
            next.execute_post_video_mcga_function_nineteen_return(
                checkpoint.last_sequence + 1U, load_segment, *native_return);
            const auto returned = next.checkpoint();
            next.execute_post_video_followup(returned.last_sequence + 1U,
                0x1c17, 0x1725);
            native_mcga_function_nineteen_advanced = true;
          }
        }
      }
    }
    if (native_ega_function_nineteen_advanced
        || native_mcga_function_nineteen_advanced) {
      // The exact EGA function is one RET and has no memory effects. Its
      // source-derived return flows into the already hash-bound descriptor
      // setup. For MCGA, the source-derived DAC clear is committed with Eon's
      // explicit retrace-duration abstraction; the unresolved pointer remains
      // unread and the wrapper's unknown saved registers/FLAGS stay unknown.
    } else if (checkpoint.state
                   == MillenniumDosTitleInitializationState::graphics_record_private_interrupt_result_boundary
               && checkpoint.boundary.function == 0x0006 && active_media_ && memory) {
      constexpr std::string_view titles_sha = "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
      constexpr std::string_view library_sha = "6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678";
      constexpr std::string_view mcga_sha = "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228";
      const auto titles=active_media_->borrow(titles_sha), library_bytes=active_media_->borrow(library_sha), driver=active_media_->borrow(mcga_sha);
      const auto local_word=[&](const std::uint16_t offset)->std::optional<std::uint16_t>{
        for(auto it=checkpoint.memory_effects.rbegin();it!=checkpoint.memory_effects.rend();++it)
          if(!it->explicit_segment&&it->offset==offset&&it->width==MillenniumDosTitleInitializationEffectWidth::word)return it->value;
        return std::nullopt;};
      const auto source_segment=local_word(0x1343);
      if(!titles||!library_bytes||!driver||!source_segment||*source_segment==0
          ||local_word(0x1341)!=0x0000||local_word(0x133f)!=0x0140
          ||local_word(0x133d)!=0x00c8)return false;
      MillenniumDosLib library(*library_bytes);
      auto source=make_millennium_dos_title_p00_recompilation_batch(*titles,library,*source_segment);
      source.id+="-"+std::to_string(millennium_dos_sound_driver_load_generation_);
      for(const auto& effect:source.effects)if(const auto old=memory->read_byte(effect.location);old&&*old!=static_cast<std::uint8_t>(effect.value))
        throw std::runtime_error("Millennium P00 recompilation overlaps incompatible owned memory");
      if(const auto applied=memory->apply(source);!applied.accepted)throw std::runtime_error(applied.error);
      // The source decoder owns the DOS segment:offset view.  Function six
      // reads the same bytes through the 20-bit real-mode bus, and the native
      // memory model keeps those views deliberately distinct.  Mirror only
      // the hash-bound decoded frame to its exact A20-wrapped physical alias.
      NativeRuntimeEffectBatch real_mode_source{
          "millennium-dos-title-p00-real-mode-source-"
              +std::to_string(millennium_dos_sound_driver_load_generation_),true,{}};
      real_mode_source.effects.reserve(source.effects.size());
      const auto source_physical=static_cast<std::uint32_t>(*source_segment)*16U;
      for(const auto& effect:source.effects){
        const auto offset=static_cast<std::uint32_t>(effect.location.offset);
        real_mode_source.effects.push_back({effect.order,
            {NativeRuntimeAddressSpace::linear,std::nullopt,
                static_cast<std::uint32_t>((source_physical+offset)&0xfffffU)},
            effect.width,effect.byte_order,effect.value});
      }
      for(const auto& effect:real_mode_source.effects)
        if(const auto old=memory->read_byte(effect.location);old&&*old!=static_cast<std::uint8_t>(effect.value))
          throw std::runtime_error("Millennium P00 real-mode source overlaps incompatible owned memory");
      if(const auto applied=memory->apply(real_mode_source);!applied.accepted)
        throw std::runtime_error(applied.error);
      const MillenniumDosVideoFunctionSixMcgaCopyLoopRegisters registers{0x0140,0,0,0,0,0,0x00c8,*source_segment,0xa000,false};
      const auto copied=execute_millennium_dos_video_function_six_mcga_copy_loop(*driver,
          MillenniumDosVideoFunctionSixMcgaCopyLoopKind::word_aligned,registers,
          MillenniumDosRealModeAddressMapping::a20_wrapped_20_bit,*memory,
          "millennium-dos-title-p00-mcga-copy-"+std::to_string(millennium_dos_sound_driver_load_generation_),200,64'000);
      if(!copied.accepted||!copied.memory_applied||copied.byte_write_count!=64'000||copied.distinct_destination_bytes!=64'000)
        throw std::runtime_error("Millennium MCGA function-six P00 copy did not produce its bounded frame");
      next.execute_graphics_record_function_six_return(checkpoint.last_sequence+1U);
    } else if (checkpoint.state
                   == MillenniumDosTitleInitializationState::post_descriptor_private_interrupt_result_boundary
               && checkpoint.boundary.function == 0x001a && active_media_ && memory
               && millennium_dos_video_driver_load_) {
      constexpr std::string_view mcga_sha =
          "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228";
      constexpr std::string_view library_sha =
          "6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678";
      const auto driver = active_media_->borrow(mcga_sha);
      const auto library = active_media_->borrow(library_sha);
      const auto load_segment = millennium_dos_video_driver_load_->load_segment();
      const auto read_range = [&memory](const std::uint16_t segment,
          const std::uint16_t offset, const std::size_t size)
          -> std::optional<std::vector<std::uint8_t>> {
        std::vector<std::uint8_t> bytes;
        bytes.reserve(size);
        for (std::size_t index = 0; index < size; ++index) {
          const auto value = memory->read_byte({NativeRuntimeAddressSpace::dos_segmented,
              segment, static_cast<std::uint32_t>(offset) + index});
          if (!value) return std::nullopt;
          bytes.push_back(*value);
        }
        return bytes;
      };
      std::vector<std::uint8_t> loaded_driver;
      if (driver) {
        const auto loaded = read_range(load_segment, 0, driver->size());
        if (loaded) loaded_driver = *loaded;
      }
      const auto palette = read_range(checkpoint.child_code_segment, 0x014c, 0x02fe);
      const auto copy_flag = memory->read_byte({NativeRuntimeAddressSpace::dos_segmented,
          load_segment, 0x09c7});
      const auto palette_is_owned = [&]() {
        return palette && library && library->size() == 18'907U
            && std::equal(palette->begin(), palette->end(), library->begin() + 0x25f9U);
      };
      const std::array<std::uint8_t,10> record{0x00,0x00,0xff,0x00,0x19,0x00,
          0x4c,0x01,static_cast<std::uint8_t>(checkpoint.child_code_segment),
          static_cast<std::uint8_t>(checkpoint.child_code_segment >> 8U)};
      if (!driver || !library || !palette || !copy_flag)
        throw std::runtime_error("Millennium MCGA function-$001a lacks owned media or live driver state");
      if (!palette_is_owned())
        throw std::runtime_error("Millennium MCGA function-$001a palette differs from its owned TITLE.LIB source");
      const auto native_return = execute_millennium_dos_mcga_function_twenty_six(
          *driver, loaded_driver, record, *palette, checkpoint.child_code_segment,
          *copy_flag);
      if (!native_return)
        throw std::runtime_error("Millennium MCGA function-$001a rejected its hash-bound inputs");
      next.execute_graphics_record_function_twenty_six_return(
          checkpoint.last_sequence + 1U, *native_return);
    } else if (checkpoint.state
                   == MillenniumDosTitleInitializationState::descriptor_loop_wait_private_interrupt_boundary
               && checkpoint.boundary.function == 0x0013 && active_media_ && memory
               && millennium_dos_video_driver_load_
               && millennium_dos_video_driver_load_->driver().kind
                   == MillenniumDosVideoDriverKind::mcga) {
      constexpr std::string_view mcga_sha =
          "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228";
      const auto driver = active_media_->borrow(mcga_sha);
      const auto load_segment = millennium_dos_video_driver_load_->load_segment();
      if (!driver) throw std::runtime_error("Millennium function-$0013 lacks its admitted MCGA image");
      for (std::uint16_t offset = 0x0905; offset < 0x0913; ++offset) {
        const auto loaded = memory->read_byte({NativeRuntimeAddressSpace::dos_segmented,
            load_segment, offset});
        if (!loaded || *loaded != (*driver)[offset])
          throw std::runtime_error("Millennium function-$0013 driver poll loop differs from its admitted image");
      }
      MillenniumDosVideoFunction13Session wait(*driver, MillenniumDosVideoDriverKind::mcga);
      wait.observe_status({1,0x0908,0x03da,0});
      wait.observe_status({2,0x090d,0x03da,0x08});
      const auto outcome = wait.outcome();
      if (!outcome) throw std::runtime_error("Millennium function-$0013 retrace translation did not return");
      next.execute_descriptor_loop_wait_function_thirteen_return(
          checkpoint.last_sequence + 1U, *outcome);
    } else if (checkpoint.state
                   == MillenniumDosTitleInitializationState::descriptor_loop_post_wait_animation_call_boundary
               && checkpoint.continuation_address == 0x1917 && memory) {
      // Literal recompilation of $18f9 and $1712. Its indexed source is in
      // the child DOS prefix (CS:$0000..$03ff), not TITLES.EXE's file image.
      // The subsequent $174a record pair is also dynamically populated CS
      // memory. Both must be present in this native generation before use.
      const auto read_word = [&](const std::uint16_t offset) -> std::optional<std::uint16_t> {
        const auto low = memory->read_byte({NativeRuntimeAddressSpace::dos_segmented,
            checkpoint.child_code_segment, offset});
        const auto high = memory->read_byte({NativeRuntimeAddressSpace::dos_segmented,
            checkpoint.child_code_segment, static_cast<std::uint16_t>(offset + 1U)});
        if (!low || !high) return std::nullopt;
        return static_cast<std::uint16_t>(*low | (static_cast<std::uint16_t>(*high) << 8U));
      };
      const auto selector_state = read_word(0x1181);
      const auto selector_accumulator = read_word(0x18f7);
      if (!selector_state || !selector_accumulator) return false;
      const auto selector_index = static_cast<std::uint16_t>(
          static_cast<std::uint32_t>(*selector_state) + *selector_accumulator) & 0x03ffU;
      const auto selector_source = memory->read_byte({NativeRuntimeAddressSpace::dos_segmented,
          checkpoint.child_code_segment, selector_index});
      if (!selector_source) return false;
      next.execute_descriptor_loop_animation_selector({checkpoint.last_sequence + 1U,
          *selector_state, *selector_accumulator, *selector_source});
    } else if (checkpoint.state
                   == MillenniumDosTitleInitializationState::post_descriptor_private_interrupt_result_boundary
               && checkpoint.continuation_address == 0x0127) {
      throw std::runtime_error("Millennium function-$001a boundary is detached from the recovered request");
    } else if (checkpoint.state
                   == MillenniumDosTitleInitializationState::graphics_descriptor_far_read_boundary
               || checkpoint.state
                   == MillenniumDosTitleInitializationState::graphics_record_word_read_boundary
               || checkpoint.state
                   == MillenniumDosTitleInitializationState::graphics_record_second_word_read_boundary
               || checkpoint.state
                   == MillenniumDosTitleInitializationState::graphics_record_third_word_read_boundary
               || checkpoint.state
                   == MillenniumDosTitleInitializationState::graphics_record_byte_read_boundary
               || checkpoint.state
                   == MillenniumDosTitleInitializationState::graphics_record_second_byte_read_boundary) {
      // The descriptor setup immediately after the video call reads the first
      // TITLE.LIB record.  The DOS file service already wrote that exact,
      // hash-admitted file into this generation's native memory.  Consume only
      // these five source instructions when both their normalized address and
      // their bytes still match that owned file.  This is intentionally kept
      // separate from the later descriptor-loop driver: the next INT $91
      // request is still an explicit preservation boundary.
      constexpr std::string_view library_sha =
          "6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678";
      if (!memory || !active_media_)
        throw std::runtime_error("Graphics descriptor setup requires owned memory and verified media");
      const auto library = active_media_->borrow(library_sha);
      if (!library || library->size() != 18907)
        throw std::runtime_error("Exact TITLE.LIB graphics descriptor source is unavailable");
      std::optional<std::uint16_t> library_segment;
      for (auto effect = checkpoint.memory_effects.rbegin();
           effect != checkpoint.memory_effects.rend(); ++effect) {
        if (!effect->explicit_segment && effect->offset == 0x0e48
            && effect->width == MillenniumDosTitleInitializationEffectWidth::word) {
          library_segment = effect->value;
          break;
        }
      }
      const auto library_offset = [&](const std::uint16_t segment,
                                      const std::uint16_t offset,
                                      const std::size_t width)
          -> std::optional<std::size_t> {
        if (!library_segment || width == 0) return std::nullopt;
        const auto address = static_cast<std::uint32_t>(segment) * 16U + offset;
        const auto base = static_cast<std::uint32_t>(*library_segment) * 16U;
        if (address < base) return std::nullopt;
        const auto displacement = address - base;
        if (displacement > library->size() || width > library->size() - displacement)
          return std::nullopt;
        return static_cast<std::size_t>(displacement);
      };
      const auto read_owned_library_byte = [&](const std::uint16_t segment,
                                               const std::uint16_t offset)
          -> std::optional<std::uint8_t> {
        const auto file_offset = library_offset(segment, offset, 1);
        if (!file_offset) return std::nullopt;
        // NativeRuntimeMemory deliberately keeps DOS segment:offset keys
        // distinct. TITLE.LIB is loaded at its original base pointer, while
        // the x86 code normalizes some later reads across a segment boundary.
        // Resolve that representation alias only after deriving the physical
        // file offset and confirming the byte against the admitted source.
        auto value = memory->read_byte({NativeRuntimeAddressSpace::dos_segmented,
            segment, offset});
        if (!value) {
          value = memory->read_byte({NativeRuntimeAddressSpace::dos_segmented,
              *library_segment, static_cast<std::uint16_t>(*file_offset)});
        }
        if (!value || *value != (*library)[*file_offset]) return std::nullopt;
        return value;
      };
      const auto read_byte = [&](const MillenniumDosTitleFarByteBoundary& boundary)
          -> std::optional<std::uint8_t> {
        return read_owned_library_byte(
            boundary.source_segment, boundary.source_offset);
      };
      const auto read_word = [&](const MillenniumDosTitleFarReadBoundary& boundary)
          -> std::optional<std::uint16_t> {
        if (boundary.word_count != 1) return std::nullopt;
        const auto low = read_owned_library_byte(
            boundary.source_segment, boundary.source_offset);
        const auto high = read_owned_library_byte(boundary.source_segment,
            static_cast<std::uint16_t>(boundary.source_offset + 1U));
        if (!low || !high) return std::nullopt;
        return static_cast<std::uint16_t>(*low | (static_cast<std::uint16_t>(*high) << 8U));
      };
      const auto& far = checkpoint.far_read_boundary;
      const auto& byte = checkpoint.far_byte_boundary;
      if (checkpoint.state
              == MillenniumDosTitleInitializationState::graphics_descriptor_far_read_boundary
          && far.instruction_address == 0x13aa && far.word_count == 2
          && library_offset(far.source_segment, far.source_offset, 4)
          && far.source_segment == static_cast<std::uint16_t>(*library_segment + 0x0481U)
          && far.source_offset == 0x0003) {
        const auto first_low = read_owned_library_byte(far.source_segment, far.source_offset);
        const auto first_high = read_owned_library_byte(far.source_segment,
            static_cast<std::uint16_t>(far.source_offset + 1U));
        const auto second_low = read_owned_library_byte(far.source_segment,
            static_cast<std::uint16_t>(far.source_offset + 2U));
        const auto second_high = read_owned_library_byte(far.source_segment,
            static_cast<std::uint16_t>(far.source_offset + 3U));
        if (!first_low || !first_high || !second_low || !second_high)
          return false;
        next.observe_far_words({checkpoint.last_sequence + 1U, far.instruction_address,
            far.source_segment, far.source_offset,
            static_cast<std::uint16_t>(*first_low | (static_cast<std::uint16_t>(*first_high) << 8U)),
            static_cast<std::uint16_t>(*second_low | (static_cast<std::uint16_t>(*second_high) << 8U))});
      } else if (const auto value = read_word(far); value
          && (checkpoint.state
                  == MillenniumDosTitleInitializationState::graphics_record_word_read_boundary
              || checkpoint.state
                  == MillenniumDosTitleInitializationState::graphics_record_second_word_read_boundary
              || checkpoint.state
                  == MillenniumDosTitleInitializationState::graphics_record_third_word_read_boundary)) {
        next.observe_far_word({checkpoint.last_sequence + 1U, far.instruction_address,
            far.source_segment, far.source_offset, *value});
      } else if (const auto value = read_byte(byte); value
          && (checkpoint.state
                  == MillenniumDosTitleInitializationState::graphics_record_byte_read_boundary
              || checkpoint.state
                  == MillenniumDosTitleInitializationState::graphics_record_second_byte_read_boundary)) {
        next.observe_far_byte({checkpoint.last_sequence + 1U, byte.instruction_address,
            byte.source_segment, byte.source_offset, *value});
      } else {
        return false;
      }
    } else if (next.descriptor_loop_can_drive()) {
      constexpr std::string_view library_sha =
          "6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678";
      if (!memory || !active_media_)
        throw std::runtime_error("Title loop requires owned memory and verified media");
      const auto library = active_media_->borrow(library_sha);
      if (!library) throw std::runtime_error("Exact TITLE.LIB loop source is unavailable");
      const auto driven = next.drive_descriptor_loop_from_title_library(
          *memory, *library, {checkpoint.last_sequence + 1, 256});
      if (!driven.accepted) { error = driven.error; return false; }
      memory_effects_applied = true;
    } else {
      switch (checkpoint.state) {
      case MillenniumDosTitleInitializationState::descriptor_loop_complete_boundary:
        if (!memory)
          throw std::runtime_error("Title loop caller requires owned native memory");
        next.advance_owned_descriptor_loop_caller(
            checkpoint.last_sequence + 1, *memory);
        break;
      case MillenniumDosTitleInitializationState::selected_local_call_boundary:
        next.execute_selected_callee_start(
            checkpoint.last_sequence + 1, checkpoint.selected_call_address,
            checkpoint.selected_call_target);
        break;
      case MillenniumDosTitleInitializationState::selected_followup_call_boundary:
        next.execute_selected_followup_start(
            checkpoint.last_sequence + 1,
            checkpoint.selected_followup_call_address,
            checkpoint.selected_followup_call_target);
        break;
      case MillenniumDosTitleInitializationState::title_main_allocation_call_boundary:
        next.execute_title_main_allocation_start(
            checkpoint.last_sequence + 1, checkpoint.title_main_call_address,
            checkpoint.title_main_call_target);
        break;
      case MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_source_byte_boundary:
      case MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_first_lookup_byte_boundary:
      case MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_second_source_byte_boundary:
      case MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_second_lookup_byte_boundary: {
        // The mode-two decoder is a native continuation only when its exact
        // current segmented source byte is already owned by this runtime.  A
        // missing byte is an external preservation boundary, not a reason to
        // synthesize memory or make the session scheduler fail.
        if (!memory || checkpoint.far_byte_boundary.source_segment == 0
            || !memory->read_byte({NativeRuntimeAddressSpace::dos_segmented,
                checkpoint.far_byte_boundary.source_segment,
                checkpoint.far_byte_boundary.source_offset}))
          return false;
        const auto driven = next.drive_mode_two_from_owned_memory(*memory,
            {checkpoint.last_sequence + 1, 4});
        if (!driven.accepted) {
          error = driven.error;
          return false;
        }
        memory_effects_applied = true;
        break;
      }
      default:
        return false;
      }
    }
  } catch (const std::exception& exception) {
    error = exception.what();
    return false;
  }
  const auto checkpoint = next.checkpoint();
  if (!memory_effects_applied && checkpoint.memory_effects.size() > prior_effects) {
    if (!memory) {
      error = "Millennium title local continuation requires native memory";
      return false;
    }
    NativeRuntimeEffectBatch batch{
        "millennium-dos-title-local-" +
            std::to_string(millennium_dos_sound_driver_load_generation_) + "-" +
            std::to_string(checkpoint.last_sequence),
        true,
        {}};
    for (std::size_t i = prior_effects; i < checkpoint.memory_effects.size();
         ++i) {
      const auto& effect = checkpoint.memory_effects[i];
      batch.effects.push_back(
          {batch.effects.size() + 1,
           {NativeRuntimeAddressSpace::dos_segmented,
            (effect.explicit_segment || effect.segment != 0)
                ? effect.segment
                : checkpoint.child_code_segment,
            effect.offset},
           effect.width == MillenniumDosTitleInitializationEffectWidth::byte
               ? MemoryTransferElementWidth::byte
               : MemoryTransferElementWidth::word,
           NativeRuntimeByteOrder::little_endian,
           effect.value});
    }
    const auto applied = memory->apply(batch);
    if (!applied.accepted) {
      error = applied.error;
      return false;
    }
  }
  millennium_dos_title_initialization_ = std::move(next);
  if (memory)
    *native_runtime_memory_ = std::move(*memory);
  return true;
}

MillenniumDosTitleInitializationObservationResult
ReleaseRuntimeCoordinator::service_millennium_dos_mcga_title_function_four() {
  MillenniumDosTitleInitializationObservationResult result;
  constexpr std::string_view titles_sha =
      "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
  constexpr std::string_view mcga_sha =
      "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228";
  constexpr std::string_view function_four_sha =
      "7fbb6ca31a1a08015162f5cb6278b0feb2be77e40c98b685d7fbeeb09aaaf842";
  constexpr std::array<std::uint8_t, 11> function_four_bytes{
      0x26, 0x8a, 0x07, 0x25, 0x03, 0x00, 0x86, 0x06, 0xaf, 0x00, 0xc3};
  if (!active_ || !active_media_ || !native_runtime_memory_ || !session_snapshot_
      || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title
      || !millennium_dos_title_initialization_ || !millennium_dos_video_driver_load_
      || !millennium_dos_video_int91_dispatch_
      || millennium_dos_video_driver_load_->driver().kind
          != MillenniumDosVideoDriverKind::mcga
      || millennium_dos_video_driver_load_->state()
          != MillenniumDosVideoDriverLoadState::ivt_observation_recorded
      || !millennium_dos_video_driver_load_->ivt_observation_matches_loaded_driver()) {
    result.error = "MCGA title function-$04 requires its active loaded-driver route";
    return result;
  }
  auto next_title = *millennium_dos_title_initialization_;
  auto memory = *native_runtime_memory_;
  try {
    const auto title = next_title.checkpoint();
    const auto& call = title.selected_callee_boundary;
    if (title.state != MillenniumDosTitleInitializationState::
            selected_callee_private_interrupt_result_boundary
        || call.call_address != 0x1ace || call.wrapper_address != 0x0122
        || call.interrupt_address != 0x0127 || call.interrupt != 0x91
        || call.function != 0x0004 || call.record_segment != title.child_code_segment
        || call.record_offset != 0x1ac5 || call.result_observed
        || title.last_sequence > std::numeric_limits<std::uint64_t>::max() - 2U) {
      result.error = "MCGA title function-$04 is detached from its selected source call";
      return result;
    }
    const auto titles = active_media_->borrow(titles_sha);
    const auto driver = active_media_->borrow(mcga_sha);
    const auto* vector = millennium_dos_video_driver_load_->ivt_observation();
    const auto& profile = millennium_dos_video_driver_load_->driver();
    if (!titles || titles->size() != 7022 || to_hex(sha256(*titles)) != titles_sha
        || !driver || to_hex(sha256(*driver)) != mcga_sha || !vector
        || vector->interrupt_number != 0x91 || vector->offset != 0
        || vector->segment != millennium_dos_video_driver_load_->load_segment()
        || profile.function_four_address != 0x0815
        || profile.function_four_state_address != 0x00af
        || profile.function_four_input_offset != 0
        || profile.function_four_input_mask != 0x03
        || profile.dispatch_table_address != 0x0032
        || (*driver)[0x003a] != 0x15 || (*driver)[0x003b] != 0x08
        || to_hex(sha256(std::span<const std::uint8_t>(*driver).subspan(
            profile.function_four_address, function_four_bytes.size()))) != function_four_sha
        || !std::equal(function_four_bytes.begin(), function_four_bytes.end(),
            driver->begin() + profile.function_four_address)) {
      result.error = "MCGA title function-$04 does not match the admitted original driver";
      return result;
    }
    const auto input_file_offset = static_cast<std::size_t>(call.record_offset - 0x0100U);
    if (input_file_offset >= titles->size()) {
      result.error = "MCGA title function-$04 byte is outside TITLES.EXE";
      return result;
    }
    const auto input = memory.read_byte({NativeRuntimeAddressSpace::dos_segmented,
        title.child_code_segment, call.record_offset});
    if (!input || *input != (*titles)[input_file_offset]) {
      result.error = "MCGA title function-$04 input differs from owned TITLES.EXE memory";
      return result;
    }

    std::vector<std::uint8_t> loaded;
    loaded.reserve(profile.byte_size);
    for (std::size_t offset = 0; offset < profile.byte_size; ++offset) {
      const auto byte = memory.read_byte({NativeRuntimeAddressSpace::dos_segmented,
          millennium_dos_video_driver_load_->load_segment(), offset});
      if (!byte) {
        result.error = "MCGA title function-$04 loaded driver image is incomplete";
        return result;
      }
      loaded.push_back(*byte);
    }
    const auto state_before = loaded[profile.function_four_state_address];
    if (state_before != (*driver)[profile.function_four_state_address]
        || loaded[profile.function_zero_cached_mode_address] != 0x13
        || loaded[profile.function_thirty_one_state_address] != 0x01
        || loaded[0x01e4] != 0x00
        || !std::equal(driver->begin(), driver->begin() + 0x13, loaded.begin())) {
      result.error = "MCGA title function-$04 state preimage differs from recovered function-zero route";
      return result;
    }
    for (std::size_t offset = 0; offset < loaded.size(); ++offset) {
      if (offset == profile.function_zero_cached_mode_address
          || offset == profile.function_four_state_address
          || offset == profile.function_thirty_one_state_address
          || offset == 0x01e4) continue;
      if (loaded[offset] != (*driver)[offset]) {
        result.error = "MCGA title function-$04 found an unowned driver mutation";
        return result;
      }
    }

    // TITLES sets AX=$0004, ES=CS and BX=$1ac5 before INT $91. The exact
    // leaf loads AL, masks it to two bits, exchanges it with $00af and RETs.
    // IRET restores the caller's pre-INT flags, which remain unknown.
    const auto state_after = static_cast<std::uint8_t>(*input & 0x03U);
    const auto returned_ax = static_cast<std::uint16_t>(state_before);
    const auto call_sequence = title.last_sequence + 1U;
    next_title.observe_selected_callee_private_interrupt_result({
        call_sequence, 0x0127, 0x0129, 0x1ad1, returned_ax, 0, false});
    next_title.execute_selected_followup_start(call_sequence + 1U, 0x1ad1, 0x044c);

    NativeRuntimeEffectBatch effects{
        "millennium-dos-mcga-title-function-four-"
            + std::to_string(millennium_dos_video_int91_dispatch_generation_)
            + "-" + std::to_string(call_sequence), true, {}};
    effects.effects.push_back({1,
        {NativeRuntimeAddressSpace::dos_segmented,
            millennium_dos_video_driver_load_->load_segment(),
            profile.function_four_state_address},
        MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::little_endian, state_after});
    const auto completed = next_title.checkpoint();
    const auto previous_count = millennium_dos_title_initialization_->checkpoint()
        .memory_effects.size();
    for (std::size_t index = previous_count;
        index < completed.memory_effects.size(); ++index) {
      const auto& write = completed.memory_effects[index];
      const auto segment = write.explicit_segment ? write.segment : completed.child_code_segment;
      effects.effects.push_back({effects.effects.size() + 1,
          {NativeRuntimeAddressSpace::dos_segmented, segment, write.offset},
          write.width == MillenniumDosTitleInitializationEffectWidth::byte
              ? MemoryTransferElementWidth::byte : MemoryTransferElementWidth::word,
          NativeRuntimeByteOrder::little_endian, write.value});
    }
    const auto applied = memory.apply(effects);
    if (!applied.accepted) {
      result.error = applied.error;
      return result;
    }
    *native_runtime_memory_ = std::move(memory);
    millennium_dos_title_initialization_ = std::move(next_title);
    result.accepted = true;
  } catch (const std::exception& error) {
    result.error = error.what();
  }
  return result;
}

MillenniumDosSessionDriveResult
ReleaseRuntimeCoordinator::drive_millennium_dos_session(
    const std::uint32_t step_limit) {
  MillenniumDosSessionDriveResult result;
  if (step_limit == 0) {
    result.stop_reason = MillenniumDosSessionStopReason::failed;
    result.error =
        "Millennium deterministic session drive requires a nonzero step limit";
    return result;
  }
  if (!active_ || !session_snapshot_) {
    result.stop_reason = MillenniumDosSessionStopReason::inactive;
    result.error = "Millennium deterministic session drive requires an active session";
    return result;
  }
  if (!millennium_dos_compatibility_runner_) {
    result.stop_reason = MillenniumDosSessionStopReason::inactive;
    result.error = "Millennium deterministic session drive requires admitted sound-driver setup";
    return result;
  }
  const auto is_drivable = [&] {
    return session_snapshot_->kind ==
               RuntimeSessionKind::millennium_dos_sound_driver_boundary ||
           session_snapshot_->kind == RuntimeSessionKind::millennium_dos_title;
  };
  if (!is_drivable()) {
    result.stop_reason = MillenniumDosSessionStopReason::inactive;
    result.error = "Millennium deterministic session drive has no active DOS boundary";
    return result;
  }
  const auto boundary = [&]() -> std::uint16_t {
    if (millennium_dos_video_int91_dispatch_) {
      if (const auto video = millennium_dos_video_int91_dispatch_->boundary())
        return video->instruction_address;
    }
    if (millennium_dos_title_initialization_) {
      const auto checkpoint = millennium_dos_title_initialization_->checkpoint();
      result.title_state = checkpoint.state;
      if (checkpoint.continuation_address != 0)
        return checkpoint.continuation_address;
      if (checkpoint.state == MillenniumDosTitleInitializationState::
              selected_callee_private_interrupt_result_boundary
          && checkpoint.selected_callee_boundary.interrupt_address != 0)
        return checkpoint.selected_callee_boundary.interrupt_address;
      if (checkpoint.state == MillenniumDosTitleInitializationState::private_interrupt_result_boundary
          && checkpoint.boundary.interrupt_address != 0)
        return checkpoint.boundary.interrupt_address;
      if (checkpoint.dos_boundary.interrupt_address != 0)
        return checkpoint.dos_boundary.interrupt_address;
      return checkpoint.bios_boundary.interrupt_address;
    }
    if (millennium_dos_sound_driver_load_)
      return millennium_dos_sound_driver_load_->boundary().instruction_address;
    return 0;
  };
  const auto external_requirement = [&]()
      -> std::optional<MillenniumDosTitleExternalObservationRequirement> {
    if (millennium_dos_video_int91_dispatch_) {
      if (const auto video = millennium_dos_video_int91_dispatch_->boundary()) {
        return {{MillenniumDosTitleExternalObservationKind::video_function_zero_bios_result,
            video->instruction_address, 0, 0, 0, video->interrupt_number,
            static_cast<std::uint16_t>((video->ax >> 8U) & 0x00ffU)}};
      }
    }
    if (!millennium_dos_title_initialization_)
      return std::nullopt;
    const auto checkpoint = millennium_dos_title_initialization_->checkpoint();
    const std::uint16_t continuation = checkpoint.continuation_address != 0
        ? checkpoint.continuation_address
        : checkpoint.state == MillenniumDosTitleInitializationState::
                selected_callee_private_interrupt_result_boundary
            ? checkpoint.selected_callee_boundary.interrupt_address
            : std::uint16_t{0};
    if (checkpoint.state == MillenniumDosTitleInitializationState::private_interrupt_result_boundary
        && checkpoint.boundary.interrupt_address == 0x0127
        && checkpoint.boundary.interrupt == 0x91
        && checkpoint.boundary.wrapper_address == 0x0122
        && checkpoint.boundary.function == 0) {
      return {{MillenniumDosTitleExternalObservationKind::title_private_interrupt_result,
          checkpoint.boundary.interrupt_address, checkpoint.boundary.record_segment,
          checkpoint.boundary.record_offset, 0, checkpoint.boundary.interrupt,
          checkpoint.boundary.function}};
    }
    const auto vector_state = checkpoint.state
        == MillenniumDosTitleInitializationState::dos_get_vector_zero_result_boundary
        || checkpoint.state
            == MillenniumDosTitleInitializationState::dos_get_vector_four_result_boundary;
    if (vector_state && continuation == checkpoint.dos_boundary.interrupt_address)
      return {{MillenniumDosTitleExternalObservationKind::dos_vector_result,
          continuation, 0, 0, 0}};
    if (checkpoint.state
            == MillenniumDosTitleInitializationState::selected_callee_private_interrupt_result_boundary
        && continuation == 0x0127
        && checkpoint.selected_callee_boundary.interrupt == 0x91
        && checkpoint.selected_callee_boundary.wrapper_address == 0x0122
        && checkpoint.selected_callee_boundary.function != 0) {
      return {{MillenniumDosTitleExternalObservationKind::title_private_interrupt_result,
          continuation, checkpoint.selected_callee_boundary.record_segment,
          checkpoint.selected_callee_boundary.record_offset, 0, 0x91,
          checkpoint.selected_callee_boundary.function}};
    }
    const auto bios_state = checkpoint.state
        == MillenniumDosTitleInitializationState::bios_int15_first_result_boundary
        || checkpoint.state
            == MillenniumDosTitleInitializationState::bios_int15_second_result_boundary;
    if (bios_state && continuation == checkpoint.setup_bios_boundary.interrupt_address)
      return {{MillenniumDosTitleExternalObservationKind::setup_bios_result,
          continuation, 0, 0, 0}};
    const bool title_private_interrupt_state = checkpoint.state
        == MillenniumDosTitleInitializationState::selected_callee_private_interrupt_result_boundary
        || checkpoint.state
            == MillenniumDosTitleInitializationState::post_video_private_interrupt_result_boundary
        || checkpoint.state
            == MillenniumDosTitleInitializationState::graphics_record_private_interrupt_result_boundary
        || checkpoint.state
            == MillenniumDosTitleInitializationState::post_descriptor_private_interrupt_result_boundary;
    if (title_private_interrupt_state && continuation == 0x0127
        && checkpoint.boundary.interrupt == 0x91
        && checkpoint.boundary.wrapper_address == 0x0122
        && checkpoint.boundary.function != 0) {
      return {{MillenniumDosTitleExternalObservationKind::title_private_interrupt_result,
          continuation, checkpoint.boundary.record_segment,
          checkpoint.boundary.record_offset, 0, checkpoint.boundary.interrupt,
          checkpoint.boundary.function}};
    }
    const auto palette_state = checkpoint.state
        == MillenniumDosTitleInitializationState::bios_palette_interrupt_boundary
        || checkpoint.state
            == MillenniumDosTitleInitializationState::library_palette_bios_interrupt_boundary;
    if (palette_state && continuation == checkpoint.bios_boundary.interrupt_address)
      return {{MillenniumDosTitleExternalObservationKind::palette_bios_result,
          continuation, 0, 0, 0}};
    if (checkpoint.state
            == MillenniumDosTitleInitializationState::descriptor_loop_post_wait_animation_call_boundary
        && continuation == 0x1917) {
      // $18f9 masks the process-segment index to ten bits before LODSB. The
      // prefix is not original-media data, so diagnostics name the one
      // source-defined byte rather than inventing a DOS PSP value. The two
      // index words were produced by the already-owned title path; if either
      // is unavailable, retain the conservative full-prefix boundary.
      const auto read_word = [this, &checkpoint](const std::uint16_t offset)
          -> std::optional<std::uint16_t> {
        if (!native_runtime_memory_) return std::nullopt;
        const auto low = native_runtime_memory_->read_byte({
            NativeRuntimeAddressSpace::dos_segmented, checkpoint.child_code_segment, offset});
        const auto high = native_runtime_memory_->read_byte({
            NativeRuntimeAddressSpace::dos_segmented, checkpoint.child_code_segment,
            static_cast<std::uint16_t>(offset + 1U)});
        if (!low || !high) return std::nullopt;
        return static_cast<std::uint16_t>(*low
            | (static_cast<std::uint16_t>(*high) << 8U));
      };
      const auto selector_state = read_word(0x1181U);
      const auto selector_accumulator = read_word(0x18f7U);
      if (selector_state && selector_accumulator) {
        const auto selector_offset = static_cast<std::uint16_t>(
            (static_cast<std::uint32_t>(*selector_state) + *selector_accumulator)
            & 0x03ffU);
        return {{MillenniumDosTitleExternalObservationKind::process_prefix_byte,
            0x1909, checkpoint.child_code_segment, selector_offset, 1}};
      }
      return {{MillenniumDosTitleExternalObservationKind::process_prefix_byte,
          0x1909, checkpoint.child_code_segment, 0, 0x0400}};
    }
    if (continuation == checkpoint.far_byte_boundary.instruction_address
        && continuation != 0)
      return {{MillenniumDosTitleExternalObservationKind::far_byte,
          continuation, checkpoint.far_byte_boundary.source_segment,
          checkpoint.far_byte_boundary.source_offset, 1}};
    if (continuation == checkpoint.far_read_boundary.instruction_address
        && continuation != 0
        && (checkpoint.far_read_boundary.word_count == 1
            || checkpoint.far_read_boundary.word_count == 2))
      return {{checkpoint.far_read_boundary.word_count == 2
              ? MillenniumDosTitleExternalObservationKind::far_words
              : MillenniumDosTitleExternalObservationKind::far_word,
          continuation, checkpoint.far_read_boundary.source_segment,
          checkpoint.far_read_boundary.source_offset,
          checkpoint.far_read_boundary.word_count}};
    return std::nullopt;
  };
  result.accepted = true;
  for (; result.steps < step_limit; ++result.steps) {
    std::string local_error;
    if (advance_millennium_dos_title_local_continuation(local_error))
      continue;
    if (!local_error.empty()) {
      result.accepted = false;
      result.stop_reason = MillenniumDosSessionStopReason::failed;
      result.stop_before_address = boundary();
      result.error = std::move(local_error);
      return result;
    }
    if (!millennium_dos_video_int91_dispatch_ && millennium_dos_title_initialization_
        && millennium_dos_video_driver_load_
        && millennium_dos_video_driver_load_->state()
            == MillenniumDosVideoDriverLoadState::ivt_observation_recorded
        && millennium_dos_video_driver_load_->ivt_observation_matches_loaded_driver()) {
      const auto title = millennium_dos_title_initialization_->checkpoint();
      if (title.state == MillenniumDosTitleInitializationState::private_interrupt_result_boundary
          && title.boundary.interrupt_address == 0x0127
          && title.boundary.interrupt == 0x91
          && title.boundary.wrapper_address == 0x0122
          && title.boundary.function == 0) {
        const auto started = begin_millennium_dos_video_int91_function_zero_dispatch();
        if (!started.accepted) {
          result.accepted = false;
          result.stop_reason = MillenniumDosSessionStopReason::failed;
          result.stop_before_address = boundary();
          result.error = started.error;
          return result;
        }
        continue;
      }
    }
    if (millennium_dos_video_int91_dispatch_
        && millennium_dos_video_driver_load_
        && millennium_dos_video_driver_load_->driver().kind
            == MillenniumDosVideoDriverKind::mcga) {
      const auto pending = millennium_dos_video_int91_dispatch_->boundary();
      if (pending && pending->interrupt_number == 0x10
          && (pending->instruction_address == 0x01f4
              || pending->instruction_address == 0x01fc
              || pending->instruction_address == 0x0200)) {
        const auto serviced = service_millennium_dos_mcga_function_zero_boundary();
        if (!serviced.accepted) {
          result.accepted = false;
          result.stop_reason = MillenniumDosSessionStopReason::failed;
          result.stop_before_address = boundary();
          result.error = serviced.error;
          return result;
        }
        continue;
      }
      const auto function_zero = millennium_dos_video_function_zero_checkpoint();
      if (function_zero && function_zero->dispatch.initial_cx_from_title_argument
          && millennium_dos_video_int91_dispatch_->eon_mcga_mode_set_completed()
          && function_zero->state
              == MillenniumDosVideoFunctionZeroState::mode_match_continuation_boundary) {
        const auto continued = advance_millennium_dos_video_function_zero_success_postlude();
        if (!continued.accepted) {
          result.accepted = false;
          result.stop_reason = MillenniumDosSessionStopReason::failed;
          result.stop_before_address = boundary();
          result.error = continued.error;
          return result;
        }
        continue;
      }
      if (function_zero && function_zero->dispatch.initial_cx_from_title_argument
          && function_zero->postlude
          && function_zero->postlude->endpoint
              == MillenniumDosVideoFunctionZeroPostludeEndpoint::mcga_single_count_branch) {
        const auto stored = advance_millennium_dos_video_function_zero_mcga_single_count_store();
        if (!stored.accepted) {
          result.accepted = false;
          result.stop_reason = MillenniumDosSessionStopReason::failed;
          result.stop_before_address = boundary();
          result.error = stored.error;
          return result;
        }
        continue;
      }
      if (function_zero && function_zero->dispatch.initial_cx_from_title_argument
          && function_zero->postlude
          && function_zero->postlude->endpoint
              == MillenniumDosVideoFunctionZeroPostludeEndpoint::mcga_ret
          && !millennium_dos_video_function_zero_return_chain_completed_) {
        const auto returned = advance_millennium_dos_video_function_zero_mcga_return_chain();
        if (!returned.accepted) {
          result.accepted = false;
          result.stop_reason = MillenniumDosSessionStopReason::failed;
          result.stop_before_address = boundary();
          result.error = returned.error;
          return result;
        }
        continue;
      }
    }
    if (millennium_dos_title_initialization_) {
      const auto title = millennium_dos_title_initialization_->checkpoint();
      if (title.state == MillenniumDosTitleInitializationState::
              selected_callee_private_interrupt_result_boundary
          && title.selected_callee_boundary.interrupt_address == 0x0127
          && title.selected_callee_boundary.interrupt == 0x91
          && title.selected_callee_boundary.function == 0x0004
          && millennium_dos_video_function_zero_return_chain_completed_) {
        const auto serviced = service_millennium_dos_mcga_title_function_four();
        if (!serviced.accepted) {
          result.accepted = false;
          result.stop_reason = MillenniumDosSessionStopReason::failed;
          result.stop_before_address = boundary();
          result.error = serviced.error;
          return result;
        }
        continue;
      }
    }
    if (millennium_dos_title_initialization_) {
      const auto title = millennium_dos_title_initialization_->checkpoint();
      const auto& bios = title.bios_boundary;
      if ((title.state == MillenniumDosTitleInitializationState::dos_get_vector_zero_result_boundary
           || title.state == MillenniumDosTitleInitializationState::dos_get_vector_four_result_boundary)
          && millennium_dos_compatibility_runner_) {
        const auto vector = static_cast<std::uint8_t>(title.dos_boundary.ax_known_value & 0xffU);
        auto virtual_ivt = *millennium_dos_compatibility_runner_;
        const auto target = virtual_ivt.get_virtual_vector(vector);
        const auto serviced = observe_millennium_dos_title_dos_vector_result({
            title.last_sequence + 1U, title.dos_boundary.interrupt_address,
            title.dos_boundary.return_address, title.dos_boundary.ax_known_value,
            target.offset, target.segment, 0x0200, true});
        if (!serviced.accepted) {
          result.accepted = false;
          result.stop_reason = MillenniumDosSessionStopReason::failed;
          result.stop_before_address = boundary();
          result.error = serviced.error;
          return result;
        }
        *millennium_dos_compatibility_runner_ = std::move(virtual_ivt);
        continue;
      }
      if (title.state == MillenniumDosTitleInitializationState::timer_vector_far_read_boundary
          && title.far_read_boundary.source_segment == 0
          && title.far_read_boundary.source_offset == 0x0070
          && title.far_read_boundary.word_count == 2
          && millennium_dos_compatibility_runner_) {
        auto virtual_ivt = *millennium_dos_compatibility_runner_;
        const auto target = virtual_ivt.get_virtual_vector(0x1c);
        const auto serviced = observe_millennium_dos_title_far_words({
            title.last_sequence + 1U, title.far_read_boundary.instruction_address,
            0, 0x0070, target.offset, target.segment, true, true, true});
        if (!serviced.accepted) {
          result.accepted = false;
          result.stop_reason = MillenniumDosSessionStopReason::failed;
          result.stop_before_address = boundary();
          result.error = serviced.error;
          return result;
        }
        virtual_ivt.set_virtual_vector(0x1c, 0x11d8, title.child_code_segment);
        *millennium_dos_compatibility_runner_ = std::move(virtual_ivt);
        continue;
      }
      if (title.state == MillenniumDosTitleInitializationState::video_vector_far_read_boundary
          && title.far_read_boundary.source_segment == 0
          && title.far_read_boundary.source_offset == 0x0024
          && title.far_read_boundary.word_count == 2
          && millennium_dos_compatibility_runner_) {
        auto virtual_ivt = *millennium_dos_compatibility_runner_;
        const auto target = virtual_ivt.get_virtual_vector(0x09);
        const auto serviced = observe_millennium_dos_title_far_words({
            title.last_sequence + 1U, title.far_read_boundary.instruction_address,
            0, 0x0024, target.offset, target.segment, true, true, true});
        if (!serviced.accepted) {
          result.accepted = false;
          result.stop_reason = MillenniumDosSessionStopReason::failed;
          result.stop_before_address = boundary();
          result.error = serviced.error;
          return result;
        }
        virtual_ivt.set_virtual_vector(0x09, 0x126a, title.child_code_segment);
        *millennium_dos_compatibility_runner_ = std::move(virtual_ivt);
        continue;
      }
      if (title.state == MillenniumDosTitleInitializationState::dos_set_vector_zero_result_boundary
          || title.state
              == MillenniumDosTitleInitializationState::dos_set_vector_four_result_boundary) {
        const auto serviced = service_millennium_dos_title_set_vector_compatibility();
        if (!serviced.accepted) {
          result.accepted = false;
          result.stop_reason = MillenniumDosSessionStopReason::failed;
          result.stop_before_address = boundary();
          result.error = serviced.error;
          return result;
        }
        continue;
      }
      if (title.state == MillenniumDosTitleInitializationState::
              library_palette_bios_interrupt_boundary
          && bios.interrupt == 0x10 && bios.interrupt_address == 0x0fd8
          && bios.ax == 0x1012 && bios.bx == 0 && bios.cx == 0x00ff
          && bios.dx_known_mask == 0xffff && bios.dx_known_value == 0x014c
          && bios.source_address == 0x014c) {
        const auto serviced = observe_millennium_dos_title_bios_result({
            title.last_sequence + 1U, bios.interrupt_address, 0x0fda,
            0, 0, false, false});
        if (!serviced.accepted) {
          result.accepted = false;
          result.stop_reason = MillenniumDosSessionStopReason::failed;
          result.stop_before_address = boundary();
          result.error = serviced.error;
          return result;
        }
        continue;
      }
      // TITLES' mode-one startup issues BIOS INT 10h / AX=1010h once for
      // each of its sixteen DAC entries. The call's inputs and return site
      // are hash-bound by the title session; this standard BIOS service has
      // no defined AX/FLAGS result used by the caller. Model only its
      // deterministic service completion and retain those outputs unknown.
      if (title.state == MillenniumDosTitleInitializationState::
              bios_palette_interrupt_boundary
          && title.selected_followup_call_target == 0x044c
          && bios.interrupt == 0x10 && bios.interrupt_address == 0x046d
          && bios.ax == 0x1010 && bios.bx < 16
          && bios.dx_known_mask == 0xff00
          && bios.source_address == static_cast<std::uint16_t>(0x014c + bios.bx * 3U)) {
        const auto serviced = observe_millennium_dos_title_bios_result({
            title.last_sequence + 1U, bios.interrupt_address, 0x046f,
            0, 0, false, false});
        if (!serviced.accepted) {
          result.accepted = false;
          result.stop_reason = MillenniumDosSessionStopReason::failed;
          result.stop_before_address = boundary();
          result.error = serviced.error;
          return result;
        }
        continue;
      }
      const bool setup_bios_state = title.state
              == MillenniumDosTitleInitializationState::bios_int15_first_result_boundary
          || title.state
              == MillenniumDosTitleInitializationState::bios_int15_second_result_boundary;
      if (setup_bios_state && title.setup_bios_boundary.interrupt == 0x15
          && title.setup_bios_boundary.ax_known_mask == 0xffff
          && (title.setup_bios_boundary.ax_known_value & 0xff00U) == 0x0100U
          && (title.setup_bios_boundary.interrupt_address == 0x1ab9
              || title.setup_bios_boundary.interrupt_address == 0x1ac1)) {
        // IBM documents AH=01 as the cassette-motor-off service. Eon has no
        // cassette device, and the hash-locked caller consumes none of these
        // calls' register/FLAGS outputs, so model only a provenance-tagged
        // return. The hardware side effect and BIOS outputs remain unknown.
        const auto serviced = service_millennium_dos_title_setup_bios_compatibility_noop();
        if (!serviced.accepted) {
          result.accepted = false;
          result.stop_reason = MillenniumDosSessionStopReason::failed;
          result.stop_before_address = boundary();
          result.error = serviced.error;
          return result;
        }
        continue;
      }
    }
    const auto before_sequence = millennium_dos_title_initialization_
        ? millennium_dos_title_initialization_->checkpoint().last_sequence
        : millennium_dos_sound_driver_load_last_sequence_;
    const auto advanced = tick_millennium_dos_compatibility_runner();
    if (!advanced) {
      result.stop_reason = MillenniumDosSessionStopReason::external_observation;
      result.stop_before_address = boundary();
      result.external_observation_requirement = external_requirement();
      return result;
    }
    if (!advanced->error.empty()) {
      result.accepted = false;
      result.stop_reason = MillenniumDosSessionStopReason::failed;
      result.stop_before_address = boundary();
      result.error = advanced->error;
      return result;
    }
    const auto after_sequence = millennium_dos_title_initialization_
        ? millennium_dos_title_initialization_->checkpoint().last_sequence
        : millennium_dos_sound_driver_load_last_sequence_;
    if (after_sequence == before_sequence) {
      result.stop_reason = MillenniumDosSessionStopReason::external_observation;
      result.stop_before_address = boundary();
      result.external_observation_requirement = external_requirement();
      return result;
    }
  }
  result.stop_reason = MillenniumDosSessionStopReason::step_limit;
  result.stop_before_address = boundary();
  return result;
}

MillenniumDosTitleExecEntryObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_title_child_process_entry(
    const MillenniumDosTitleExecProcessEntry observation) {
    MillenniumDosTitleExecEntryObservationResult result;
    if(!active_||!session_snapshot_
        ||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_sound_driver_boundary
        ||!millennium_dos_sound_driver_load_||!millennium_dos_title_exec_entry_
        ||!millennium_dos_compatibility_runner_||!native_runtime_memory_
        ||observation.provenance
            !=MillenniumDosTitleExecEntryProvenance::observed_process_entry
        ||millennium_dos_sound_driver_load_->state()
            !=MillenniumDosSoundDriverLoadState::title_exec_requested){
        result.error="TITLES.EXE child entry requires an observed entry after the exact EXEC request";
        return result;
    }
    auto next=*millennium_dos_title_exec_entry_;
    auto memory=*native_runtime_memory_;
    auto runner=*millennium_dos_compatibility_runner_;
    try {
        next.observe_child_process_entry(observation);
        constexpr std::string_view titles_sha =
            "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
        if (!active_media_) throw std::runtime_error("Active verified media is unavailable");
        const auto titles = active_media_->borrow(titles_sha);
        if (!titles || titles->size() != 7022
            || to_hex(sha256(*titles)) != titles_sha) {
            throw std::runtime_error("Exact TITLES.EXE child image is unavailable");
        }
        NativeRuntimeEffectBatch batch{
            "millennium-dos-observed-title-child-" +
                std::to_string(millennium_dos_sound_driver_load_generation_) +
                "-image-" + std::to_string(observation.sequence),
            true,
            {}};
        batch.effects.reserve(titles->size());
        for (std::size_t index = 0; index < titles->size(); ++index) {
            const NativeRuntimeLocation location{
                NativeRuntimeAddressSpace::dos_segmented,
                observation.child_code_segment,
                0x0100U + index};
            if (const auto existing = memory.read_byte(location);
                existing && *existing != (*titles)[index]) {
                throw std::runtime_error(
                    "Observed TITLES.EXE segment overlaps different initialized bytes");
            }
            batch.effects.push_back({index + 1, location,
                MemoryTransferElementWidth::byte,
                NativeRuntimeByteOrder::little_endian, (*titles)[index]});
        }
        const auto applied = memory.apply(batch);
        if (!applied.accepted) throw std::runtime_error(applied.error);
        runner.synchronize_external_sequence(observation.sequence);
    }
    catch(const std::exception& e){result.error=e.what();return result;}
    millennium_dos_title_exec_entry_=std::move(next);
    millennium_dos_compatibility_runner_=std::move(runner);
    *native_runtime_memory_=std::move(memory);
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
        ||!millennium_dos_compatibility_runner_||!active_media_
        ||millennium_dos_title_exec_entry_->state()
            !=MillenniumDosTitleExecEntryState::entry_prefix_boundary){
        result.error="TITLES.EXE local prefix requires an explicit child-process entry";
        return result;
    }
    auto next=*millennium_dos_title_exec_entry_;
    auto runner=*millennium_dos_compatibility_runner_;
    try { next.execute_exact_entry_prefix(observation.sequence,
        observation.prefix_address,observation.jump_address,
        observation.jump_destination); }
    catch(const std::exception& e){result.error=e.what();return result;}
    try {
        constexpr std::string_view titles_sha =
            "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
        if (!active_media_) throw std::runtime_error("Active verified media is unavailable");
        const auto titles = active_media_->borrow(titles_sha);
        if (!titles) throw std::runtime_error("Exact TITLES.EXE child image is unavailable");
        runner.synchronize_external_sequence(observation.sequence);
        MillenniumDosTitleInitializationSession initialization(
            *titles, next.checkpoint().child_code_segment, observation.sequence);
        const auto startup_sequence = runner.next_sequence();
        initialization.execute_exact_startup(startup_sequence, 0x1b80,
            0x1b95, 0x0122, 0x91);
        runner.record_automatic_operation();
        millennium_dos_title_initialization_=std::move(initialization);
    } catch(const std::exception& e){result.error=e.what();return result;}
    millennium_dos_title_exec_entry_=std::move(next);
    millennium_dos_compatibility_runner_=std::move(runner);
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
    if (millennium_dos_video_int91_dispatch_) {
        result.error = "Video-driver BIOS results cannot substitute for the raw title INT 91h return";
        return result;
    }
    if(!active_||!session_snapshot_
        ||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_title
        ||!millennium_dos_title_initialization_||!native_runtime_memory_){
        result.error="Title private-interrupt result requires the active native title boundary";
        return result;
    }
    auto next=*millennium_dos_title_initialization_;
    auto memory=*native_runtime_memory_;
    const auto prior_effect_count=next.checkpoint().memory_effects.size();
    try {
        next.observe_private_interrupt_result(observation);
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
                effect.explicit_segment?effect.segment:checkpoint.child_code_segment,effect.offset},
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
                    effect.explicit_segment?effect.segment:checkpoint.child_code_segment,effect.offset},
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
        if (!active_media_) throw std::runtime_error("Active verified media is unavailable");
        const auto& media=*active_media_;
        const auto titles=media.borrow(titles_sha);
        if(!titles)throw std::runtime_error("Exact TITLES.EXE BIOS source is unavailable");
        auto next=*millennium_dos_title_initialization_;
        auto memory=*native_runtime_memory_;
        const auto prior_effect_count=next.checkpoint().memory_effects.size();
        next.observe_bios_palette_result(observation,*titles);
        if(next.checkpoint().state
            ==MillenniumDosTitleInitializationState::post_library_setup_call_boundary){
            const auto reached=next.checkpoint();
            next.execute_post_library_setup(reached.last_sequence+1,0x1bef,0x1aac);
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
                        effect.explicit_segment?effect.segment:checkpoint.child_code_segment,effect.offset},
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
                    effect.explicit_segment?effect.segment:checkpoint.child_code_segment,effect.offset},
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
        if (!active_media_) throw std::runtime_error("Active verified media is unavailable");
        const auto& media=*active_media_;
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
        // The original palette setup clears and then fills the same bytes.
        // Publish the final sequential byte state as one non-overlapping batch.
        std::map<NativeRuntimeLocation,std::uint8_t> final_bytes;
        for(std::size_t i=prior_effect_count;i<checkpoint.memory_effects.size();++i){
            const auto& effect=checkpoint.memory_effects[i];
            auto location=NativeRuntimeLocation{NativeRuntimeAddressSpace::dos_segmented,
                effect.explicit_segment?effect.segment:checkpoint.child_code_segment,
                effect.offset};
            final_bytes[location]=static_cast<std::uint8_t>(effect.value);
            if(effect.width==MillenniumDosTitleInitializationEffectWidth::word){
                ++location.offset;
                final_bytes[location]=static_cast<std::uint8_t>(effect.value>>8U);
            }
        }
        for(const auto& [location,value]:final_bytes)
            batch.effects.push_back({batch.effects.size()+1,location,
                MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::little_endian,value});
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
        for(std::size_t i=prior;i<checkpoint.memory_effects.size();++i){const auto& effect=checkpoint.memory_effects[i];batch.effects.push_back({batch.effects.size()+1,{NativeRuntimeAddressSpace::dos_segmented,effect.explicit_segment?effect.segment:checkpoint.child_code_segment,effect.offset},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::little_endian,effect.value});}
        const auto applied=memory.apply(batch);if(!applied.accepted){result.error=applied.error;return result;}
    }
    millennium_dos_title_initialization_=std::move(next);*native_runtime_memory_=std::move(memory);result.accepted=true;return result;
}
MillenniumDosTitleInitializationObservationResult
ReleaseRuntimeCoordinator::service_millennium_dos_title_set_vector_compatibility() {
    MillenniumDosTitleInitializationObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title
        || !millennium_dos_title_initialization_ || !native_runtime_memory_) {
        result.error = "Title DOS Set Vector service requires the active native title boundary";
        return result;
    }
    auto next = *millennium_dos_title_initialization_;
    auto memory = *native_runtime_memory_;
    const auto prior = next.checkpoint().memory_effects.size();
    try {
        const auto checkpoint = next.checkpoint();
        next.execute_dos_set_vector_compatibility(checkpoint.last_sequence + 1U);
    } catch (const std::exception& error) {
        result.error = error.what();
        return result;
    }
    const auto checkpoint = next.checkpoint();
    if (!millennium_dos_compatibility_runner_) {
        result.error = "DOS Set Vector has no generation-owned virtual IVT";
        return result;
    }
    const auto vector = static_cast<std::uint8_t>(checkpoint.dos_boundary.ax_known_value & 0xffU);
    auto virtual_ivt = *millennium_dos_compatibility_runner_;
    virtual_ivt.set_virtual_vector(
        vector, checkpoint.dos_boundary.dx, checkpoint.child_code_segment);
    if (checkpoint.memory_effects.size() > prior) {
        NativeRuntimeEffectBatch batch{
            "millennium-dos-title-set-vector-" + std::to_string(checkpoint.last_sequence),
            true, {}};
        for (std::size_t index = prior; index < checkpoint.memory_effects.size(); ++index) {
            const auto& effect = checkpoint.memory_effects[index];
            batch.effects.push_back({batch.effects.size() + 1U,
                {NativeRuntimeAddressSpace::dos_segmented,
                    effect.explicit_segment || effect.segment != 0
                        ? effect.segment : checkpoint.child_code_segment,
                    effect.offset},
                MemoryTransferElementWidth::word, NativeRuntimeByteOrder::little_endian,
                effect.value});
        }
        const auto applied = memory.apply(batch);
        if (!applied.accepted) {
            result.error = applied.error;
            return result;
        }
    }
    *millennium_dos_title_initialization_ = std::move(next);
    *millennium_dos_compatibility_runner_ = std::move(virtual_ivt);
    *native_runtime_memory_ = std::move(memory);
    result.accepted = true;
    return result;
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
MillenniumDosTitleInitializationObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_title_setup_bios_return(
    const MillenniumDosTitleSetupBiosReturnObservation observation) {
    MillenniumDosTitleInitializationObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title
        || !millennium_dos_title_initialization_ || !native_runtime_memory_) {
        result.error = "Title setup BIOS return requires the active native title boundary";
        return result;
    }
    auto next = *millennium_dos_title_initialization_;
    auto memory = *native_runtime_memory_;
    const auto prior = next.checkpoint().memory_effects.size();
    try {
        next.observe_setup_bios_return(observation);
        auto reached = next.checkpoint();
        if (reached.state
            == MillenniumDosTitleInitializationState::post_library_next_setup_call_boundary) {
            next.execute_next_setup(reached.last_sequence + 1, 0x1bf2, 0x11a7);
            reached = next.checkpoint();
        }
        if (reached.state
            == MillenniumDosTitleInitializationState::post_library_followup_call_boundary) {
            next.execute_followup_setup(reached.last_sequence + 1, 0x1bf5, 0x114e);
        }
    } catch (const std::exception& e) {
        result.error = e.what();
        return result;
    }
    const auto checkpoint = next.checkpoint();
    if (checkpoint.memory_effects.size() > prior) {
        NativeRuntimeEffectBatch batch{
            "millennium-dos-title-bios-return-" + std::to_string(observation.sequence), true, {}};
        for (std::size_t i = prior; i < checkpoint.memory_effects.size(); ++i) {
            const auto& effect = checkpoint.memory_effects[i];
            batch.effects.push_back({batch.effects.size() + 1,
                {NativeRuntimeAddressSpace::dos_segmented, checkpoint.child_code_segment,
                    effect.offset},
                effect.width == MillenniumDosTitleInitializationEffectWidth::byte
                    ? MemoryTransferElementWidth::byte : MemoryTransferElementWidth::word,
                NativeRuntimeByteOrder::little_endian, effect.value});
        }
        const auto applied = memory.apply(batch);
        if (!applied.accepted) {
            result.error = applied.error;
            return result;
        }
    }
    millennium_dos_title_initialization_ = std::move(next);
    *native_runtime_memory_ = std::move(memory);
    result.accepted = true;
    return result;
}
MillenniumDosTitleInitializationObservationResult
ReleaseRuntimeCoordinator::service_millennium_dos_title_setup_bios_compatibility_noop() {
    MillenniumDosTitleInitializationObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title
        || !millennium_dos_title_initialization_ || !native_runtime_memory_) {
        result.error = "Title setup BIOS compatibility service requires the active title boundary";
        return result;
    }
    auto next = *millennium_dos_title_initialization_;
    auto memory = *native_runtime_memory_;
    const auto prior = next.checkpoint().memory_effects.size();
    try {
        const auto checkpoint = next.checkpoint();
        next.execute_setup_bios_compatibility_noop(checkpoint.last_sequence + 1);
        auto reached = next.checkpoint();
        if (reached.state
            == MillenniumDosTitleInitializationState::post_library_next_setup_call_boundary) {
            next.execute_next_setup(reached.last_sequence + 1, 0x1bf2, 0x11a7);
            reached = next.checkpoint();
        }
        if (reached.state
            == MillenniumDosTitleInitializationState::post_library_followup_call_boundary) {
            next.execute_followup_setup(reached.last_sequence + 1, 0x1bf5, 0x114e);
        }
    } catch (const std::exception& e) {
        result.error = e.what();
        return result;
    }
    const auto checkpoint = next.checkpoint();
    if (checkpoint.memory_effects.size() > prior) {
        NativeRuntimeEffectBatch batch{
            "millennium-dos-title-ah01-noop-" + std::to_string(checkpoint.last_sequence),
            true, {}};
        for (std::size_t i = prior; i < checkpoint.memory_effects.size(); ++i) {
            const auto& effect = checkpoint.memory_effects[i];
            batch.effects.push_back({batch.effects.size() + 1,
                {NativeRuntimeAddressSpace::dos_segmented, checkpoint.child_code_segment,
                    effect.offset},
                effect.width == MillenniumDosTitleInitializationEffectWidth::byte
                    ? MemoryTransferElementWidth::byte : MemoryTransferElementWidth::word,
                NativeRuntimeByteOrder::little_endian, effect.value});
        }
        const auto applied = memory.apply(batch);
        if (!applied.accepted) {
            result.error = applied.error;
            return result;
        }
    }
    millennium_dos_title_initialization_ = std::move(next);
    *native_runtime_memory_ = std::move(memory);
    result.accepted = true;
    return result;
}
MillenniumDosTitleInitializationObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_title_far_words(
    const MillenniumDosTitleFarWordsObservation observation) {
    MillenniumDosTitleInitializationObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title
        || !millennium_dos_title_initialization_ || !native_runtime_memory_) {
        result.error = "Title far words require the active native title boundary";
        return result;
    }
    auto next = *millennium_dos_title_initialization_;
    auto memory = *native_runtime_memory_;
    const auto prior = next.checkpoint().memory_effects.size();
    try {
        next.observe_far_words(observation);
        auto reached = next.checkpoint();
        if (reached.state == MillenniumDosTitleInitializationState::post_vector_hook_call_boundary) {
            next.execute_video_hook_setup(reached.last_sequence + 1, 0x1bf8, 0x12a0);
            reached = next.checkpoint();
        }
        if (reached.state == MillenniumDosTitleInitializationState::post_video_hook_mode_call_boundary) {
            next.execute_post_video_mode_call(reached.last_sequence + 1,
                reached.selected_mode == 1 ? 0x1c07 : 0x1c02,
                reached.selected_mode == 1 ? 0x1ac6 : 0x1ada);
        }
    } catch (const std::exception& e) {
        result.error = e.what();
        return result;
    }
    const auto checkpoint = next.checkpoint();
    NativeRuntimeEffectBatch batch{
        "millennium-dos-title-vector-hook-" + std::to_string(observation.sequence), true, {}};
    for (std::size_t i = prior; i < checkpoint.memory_effects.size(); ++i) {
        const auto& effect = checkpoint.memory_effects[i];
        batch.effects.push_back({batch.effects.size() + 1,
            {NativeRuntimeAddressSpace::dos_segmented,
                effect.explicit_segment ? effect.segment : checkpoint.child_code_segment,
                effect.offset},
            effect.width == MillenniumDosTitleInitializationEffectWidth::byte
                ? MemoryTransferElementWidth::byte : MemoryTransferElementWidth::word,
            NativeRuntimeByteOrder::little_endian, effect.value});
    }
    const auto applied = memory.apply(batch);
    if (!applied.accepted) {
        result.error = applied.error;
        return result;
    }
    millennium_dos_title_initialization_ = std::move(next);
    *native_runtime_memory_ = std::move(memory);
    result.accepted = true;
    return result;
}
MillenniumDosTitleInitializationObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_title_far_word(const MillenniumDosTitleFarWordObservation observation){MillenniumDosTitleInitializationObservationResult result;if(!active_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_title||!millennium_dos_title_initialization_||!native_runtime_memory_){result.error="Title far word requires the active native title boundary";return result;}auto next=*millennium_dos_title_initialization_;auto memory=*native_runtime_memory_;const auto prior=next.checkpoint().memory_effects.size();try{next.observe_far_word(observation);}catch(const std::exception& e){result.error=e.what();return result;}const auto checkpoint=next.checkpoint();NativeRuntimeEffectBatch batch{"millennium-dos-title-record-word-"+std::to_string(observation.sequence),true,{}};for(std::size_t i=prior;i<checkpoint.memory_effects.size();++i){const auto& effect=checkpoint.memory_effects[i];batch.effects.push_back({batch.effects.size()+1,{NativeRuntimeAddressSpace::dos_segmented,effect.explicit_segment?effect.segment:checkpoint.child_code_segment,effect.offset},effect.width==MillenniumDosTitleInitializationEffectWidth::byte?MemoryTransferElementWidth::byte:MemoryTransferElementWidth::word,NativeRuntimeByteOrder::little_endian,effect.value});}if(!batch.effects.empty()){const auto applied=memory.apply(batch);if(!applied.accepted){result.error=applied.error;return result;}}millennium_dos_title_initialization_=std::move(next);*native_runtime_memory_=std::move(memory);result.accepted=true;return result;}
MillenniumDosTitleInitializationObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_title_far_byte(const MillenniumDosTitleFarByteObservation observation){MillenniumDosTitleInitializationObservationResult result;if(!active_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_title||!millennium_dos_title_initialization_||!native_runtime_memory_){result.error="Title far byte requires the active native title boundary";return result;}auto next=*millennium_dos_title_initialization_;auto memory=*native_runtime_memory_;const auto prior=next.checkpoint().memory_effects.size();try{next.observe_far_byte(observation);}catch(const std::exception& e){result.error=e.what();return result;}const auto checkpoint=next.checkpoint();NativeRuntimeEffectBatch batch{"millennium-dos-title-record-byte-"+std::to_string(observation.sequence),true,{}};for(std::size_t i=prior;i<checkpoint.memory_effects.size();++i){const auto& effect=checkpoint.memory_effects[i];batch.effects.push_back({batch.effects.size()+1,{NativeRuntimeAddressSpace::dos_segmented,effect.explicit_segment?effect.segment:checkpoint.child_code_segment,effect.offset},effect.width==MillenniumDosTitleInitializationEffectWidth::byte?MemoryTransferElementWidth::byte:MemoryTransferElementWidth::word,NativeRuntimeByteOrder::little_endian,effect.value});}const auto applied=memory.apply(batch);if(!applied.accepted){result.error=applied.error;return result;}millennium_dos_title_initialization_=std::move(next);*native_runtime_memory_=std::move(memory);result.accepted=true;return result;}

MillenniumDosTitleInitializationObservationResult ReleaseRuntimeCoordinator::observe_millennium_dos_title_continuation(MillenniumDosTitleContinuationObservation observation){
    struct Dispatcher {
        ReleaseRuntimeCoordinator& coordinator;
        MillenniumDosTitleInitializationObservationResult operator()(const MillenniumDosTitleDosVectorResultObservation value) const { return coordinator.observe_millennium_dos_title_dos_vector_result(value); }
        MillenniumDosTitleInitializationObservationResult operator()(const MillenniumDosTitleSetupBiosResultObservation value) const { return coordinator.observe_millennium_dos_title_setup_bios_result(value); }
        MillenniumDosTitleInitializationObservationResult operator()(const MillenniumDosTitleSetupBiosReturnObservation value) const { return coordinator.observe_millennium_dos_title_setup_bios_return(value); }
        MillenniumDosTitleInitializationObservationResult operator()(const MillenniumDosTitleBiosResultObservation value) const { return coordinator.observe_millennium_dos_title_bios_result(value); }
        MillenniumDosTitleInitializationObservationResult operator()(const MillenniumDosTitleFarWordsObservation value) const { return coordinator.observe_millennium_dos_title_far_words(value); }
        MillenniumDosTitleInitializationObservationResult operator()(const MillenniumDosTitleFarWordObservation value) const { return coordinator.observe_millennium_dos_title_far_word(value); }
        MillenniumDosTitleInitializationObservationResult operator()(const MillenniumDosTitleFarByteObservation value) const { return coordinator.observe_millennium_dos_title_far_byte(value); }
    };
    return std::visit(Dispatcher{*this}, observation);
}

MillenniumDosTitleModeTwoDriveResult ReleaseRuntimeCoordinator::drive_millennium_dos_title_mode_two(
    const MillenniumDosTitleModeTwoDriveRequest request) {
    if(!active_||!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_title
        ||!millennium_dos_title_initialization_||!native_runtime_memory_)
        return {false,false,0,"Mode-two drive requires the active native title boundary"};
    auto next=*millennium_dos_title_initialization_;
    auto memory=*native_runtime_memory_;
    auto result=next.drive_mode_two_from_owned_memory(memory,request);
    if(!result.accepted)return result;
    millennium_dos_title_initialization_=std::move(next);
    *native_runtime_memory_=std::move(memory);
    return result;
}

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

MillenniumDosTitleToGameObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_title_to_game_exec_request(
    const MillenniumDosTitleToGameExecRequest observation) {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title_handoff_boundary
        || !millennium_dos_title_to_game_
        || observation.sequence <= millennium_dos_title_to_game_last_sequence_) {
        return title_to_game_rejected(
            "Game EXEC request requires the active title-to-game boundary and a later sequence");
    }
    auto next = *millennium_dos_title_to_game_;
    try { next.observe_game_exec_request(observation); }
    catch (const std::exception& error) { return title_to_game_rejected(error.what()); }
    millennium_dos_title_to_game_ = std::move(next);
    millennium_dos_title_to_game_last_sequence_ = observation.sequence;
    return {true, {}};
}

MillenniumDosTitleToGameObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_title_to_game_process_entry(
    const MillenniumDosTitleToGameProcessEntry observation) {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title_handoff_boundary
        || !millennium_dos_title_to_game_
        || !active_ || !millennium_dos_native_process_
        || observation.sequence <= millennium_dos_title_to_game_last_sequence_) {
        return title_to_game_rejected(
            "2200AD.EXE process entry requires the active title-to-game boundary and a later sequence");
    }
    const auto prepared_child = millennium_dos_native_process_->checkpoint();
    if (!prepared_child || !prepared_child->static_recovery_entry
        || prepared_child->recovery_entry != MillenniumDosNativeRecoveryEntry::startup
        || prepared_child->game_executable_sha256
            != "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57"
        || prepared_child->release_sha256
            != "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123") {
        return title_to_game_rejected(
            "2200AD.EXE process entry is detached from the hash-admitted English child image");
    }
    if (!active_media_) {
        return title_to_game_rejected("Verified 2200AD.EXE media is unavailable");
    }
    const auto game = admit_native_code_image(*active_media_,
        "millennium-dos-2200ad-exe-linear", "millennium-dos-game-flow");
    if (!game.accepted()) {
        return title_to_game_rejected("Verified 2200AD.EXE entry image is unavailable");
    }
    auto next = *millennium_dos_title_to_game_;
    auto next_native_process = MillenniumDosNativeProcessAdmission::startup(
        active_->release.sha256, game.view->bytes);
    if (!next_native_process.admitted()) {
        return title_to_game_rejected(next_native_process.error());
    }
    try {
        next.observe_game_process_entry(observation);
        next.execute_game_entry_prefix(game.view->bytes);
        next.execute_game_startup_prefix(game.view->bytes);
        next_native_process.observe_child_process_entry(
            observation.sequence, observation.child_code_segment);

        // 2200AD's first private call is function $1f. If this same Eon
        // process has a matching, observed INT $91 vector and the complete
        // loaded driver still matches its original image apart from the
        // bounded driver-local state bytes, execute the six-byte function
        // against the owned state byte. Otherwise leave the private return as
        // the explicit external boundary.
        if (native_runtime_memory_ && active_media_ && millennium_dos_video_driver_load_
            && millennium_dos_video_driver_load_->state()
                == MillenniumDosVideoDriverLoadState::ivt_observation_recorded
            && millennium_dos_video_driver_load_->ivt_observation_matches_loaded_driver()
            && observation.sequence <= std::numeric_limits<std::uint64_t>::max() - 2U) {
            auto memory = *native_runtime_memory_;
            const auto kind = millennium_dos_video_driver_load_->driver().kind;
            const std::string_view driver_sha = kind == MillenniumDosVideoDriverKind::ega640
                ? "ba003dd155fee868980f6ece933c33f9b22af68ed376cd64f4e027abd65baf6a"
                : "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228";
            const auto exact_driver = active_media_->borrow(driver_sha);
            const auto* vector = millennium_dos_video_driver_load_->ivt_observation();
            if (!exact_driver || !vector) {
                throw std::runtime_error("Exact loaded video driver/vector is unavailable");
            }
            std::vector<std::uint8_t> loaded_driver;
            loaded_driver.reserve(millennium_dos_video_driver_load_->driver().byte_size);
            for (std::size_t offset = 0;
                offset < millennium_dos_video_driver_load_->driver().byte_size; ++offset) {
                const auto value = memory.read_byte({
                    NativeRuntimeAddressSpace::dos_segmented,
                    millennium_dos_video_driver_load_->load_segment(), offset});
                if (!value) {
                    throw std::runtime_error("Owned loaded video-driver memory is incomplete");
                }
                loaded_driver.push_back(*value);
            }
            const auto function31 = next_native_process.execute_startup_function_31_return(
                observation.sequence + 1U, *exact_driver, loaded_driver, kind,
                millennium_dos_video_driver_load_->load_segment(),
                vector->interrupt_number, vector->offset, vector->segment);
            if (!function31.accepted) {
                throw std::runtime_error(function31.error);
            }

            const auto function_four = execute_millennium_dos_game_function_four_dispatch(
                game.view->bytes, function31.returned_ax, *exact_driver, loaded_driver,
                kind, observation.child_code_segment,
                millennium_dos_video_driver_load_->load_segment(),
                vector->interrupt_number, vector->offset, vector->segment);
            if (!function_four.accepted) {
                throw std::runtime_error(function_four.error);
            }
            if (kind == MillenniumDosVideoDriverKind::ega640
                && function_four.continuation_address == 0x0476) {
                next_native_process.begin_startup_ega_palette(function_four);
            }

            // The exact wrapper and handler overwrite these child-stack
            // locations. Unknown saved SI/DI/BP and FLAGS stay absent instead
            // of inheriting title-process bytes from the reused DOS segment.
            std::vector<NativeRuntimeLocation> unknown_stack_bytes;
            unknown_stack_bytes.reserve(8);
            for (const auto& word : function_four.stack_words) {
                if (word.value) continue;
                unknown_stack_bytes.push_back({NativeRuntimeAddressSpace::dos_segmented,
                    observation.child_code_segment, word.offset});
                unknown_stack_bytes.push_back({NativeRuntimeAddressSpace::dos_segmented,
                    observation.child_code_segment, static_cast<std::uint64_t>(word.offset) + 1U});
            }
            const auto transaction_suffix = std::to_string(
                millennium_dos_video_driver_load_generation_) + "-"
                + std::to_string(observation.sequence);
            const auto forgotten = memory.forget(
                "millennium-dos-game-function-four-unknown-stack-" + transaction_suffix,
                unknown_stack_bytes);
            if (!forgotten.accepted) {
                throw std::runtime_error(forgotten.error);
            }
            NativeRuntimeEffectBatch effects{
                "millennium-dos-game-function-four-effects-" + transaction_suffix,
                true, {}};
            for (const auto& write : function_four.driver_writes) {
                effects.effects.push_back({effects.effects.size() + 1,
                    {NativeRuntimeAddressSpace::dos_segmented,
                        millennium_dos_video_driver_load_->load_segment(), write.offset},
                    MemoryTransferElementWidth::byte,
                    NativeRuntimeByteOrder::little_endian, write.after});
            }
            for (const auto& word : function_four.stack_words) {
                if (!word.value) continue;
                effects.effects.push_back({effects.effects.size() + 1,
                    {NativeRuntimeAddressSpace::dos_segmented,
                        observation.child_code_segment, word.offset},
                    MemoryTransferElementWidth::word,
                    NativeRuntimeByteOrder::little_endian, *word.value});
            }
            const auto applied = memory.apply(effects);
            if (!applied.accepted) {
                throw std::runtime_error(applied.error);
            }
            next_native_process.observe_private_interrupt_return({
                observation.sequence + 2U, 0x0129, function_four.returned_ax});
            const auto post_function_four = next_native_process.checkpoint();
            if (!post_function_four
                || post_function_four->boundary.address != function_four.continuation_address
                || post_function_four->last_observation_sequence != observation.sequence + 2U) {
                throw std::runtime_error(
                    "Function-$04 continuation did not reach its hash-bound caller boundary");
            }
            if (kind == MillenniumDosVideoDriverKind::mcga
                && function_four.continuation_address == 0x0455) {
                const auto allocation = advance_millennium_dos_startup_to_allocation_request(
                    game.view->bytes, function_four, observation.child_code_segment);
                if (!allocation.accepted) {
                    throw std::runtime_error(allocation.error);
                }
                const std::array<NativeRuntimeLocation,2> unknown_dx_word{{
                    {NativeRuntimeAddressSpace::dos_segmented,
                        observation.child_code_segment,0xd9fe},
                    {NativeRuntimeAddressSpace::dos_segmented,
                        observation.child_code_segment,0xd9ff}}};
                const auto forgotten_dx = memory.forget(
                    "millennium-dos-game-allocation-unknown-dx-"
                        + transaction_suffix, unknown_dx_word);
                if (!forgotten_dx.accepted) {
                    throw std::runtime_error(forgotten_dx.error);
                }
                NativeRuntimeEffectBatch allocation_stack{
                    "millennium-dos-game-allocation-stack-" + transaction_suffix,
                    true, {{1,{NativeRuntimeAddressSpace::dos_segmented,
                        observation.child_code_segment,0xd9fc},
                        MemoryTransferElementWidth::word,
                        NativeRuntimeByteOrder::little_endian,0xd2eb}}};
                const auto allocation_applied = memory.apply(allocation_stack);
                if (!allocation_applied.accepted) {
                    throw std::runtime_error(allocation_applied.error);
                }
                next_native_process.observe_startup_allocation_request(
                    allocation.instruction_address);
                const auto allocation_checkpoint = next_native_process.checkpoint();
                if (!allocation_checkpoint
                    || allocation_checkpoint->boundary.kind
                        != MillenniumDosNativeBoundaryKind::dos_interrupt
                    || allocation_checkpoint->boundary.address != 0xd201
                    || allocation_checkpoint->boundary.interrupt != 0x21) {
                    throw std::runtime_error(
                        "Startup did not reach the bounded DOS allocation request");
                }
            }
            *native_runtime_memory_ = std::move(memory);

        }
    }
    catch (const std::exception& e) { return title_to_game_rejected(e.what()); }
    millennium_dos_title_to_game_ = std::move(next);
    millennium_dos_native_process_ = std::move(next_native_process);
    millennium_dos_title_to_game_last_sequence_ = observation.sequence;
    return {true, {}};
}

MillenniumDosNativeProcessObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_native_private_interrupt_return(
    const MillenniumDosNativePrivateInterruptReturnObservation observation) {
    if (admission_ != ReleaseRuntimeAdmission::active || !active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title_handoff_boundary
        || active_->release.game != Game::millennium
        || active_->release.platform != Platform::dos || active_->release.language != "en"
        || active_->release.sha256
            != "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123"
        || !millennium_dos_title_to_game_ || !millennium_dos_native_process_) {
        return {false, "Native INT 91h return requires the active English title-to-game continuation"};
    }
    const auto process_checkpoint = millennium_dos_native_process_->checkpoint();
    if (millennium_dos_title_to_game_->state()
            != MillenniumDosTitleToGameState::game_startup_call_boundary
        || !process_checkpoint || !process_checkpoint->static_recovery_entry
        || process_checkpoint->recovery_entry != MillenniumDosNativeRecoveryEntry::startup
        || !process_checkpoint->observed_child_process_entry
        || process_checkpoint->child_entry_sequence
            != millennium_dos_title_to_game_->child_entry_sequence()
        || process_checkpoint->child_code_segment
            != millennium_dos_title_to_game_->child_code_segment()
        || process_checkpoint->boundary.kind != MillenniumDosNativeBoundaryKind::private_interrupt
        || process_checkpoint->boundary.address != observation.interrupt_return_address
        || observation.sequence <= process_checkpoint->last_observation_sequence
        || observation.sequence <= millennium_dos_title_to_game_->child_entry_sequence()) {
        return {false, "Native INT 91h return is detached from the exact first child startup boundary"};
    }
    try {
        millennium_dos_native_process_->observe_private_interrupt_return(observation);
        return {true, {}};
    } catch (const std::exception& error) {
        return {false, std::string("Native INT 91h return rejected: ") + error.what()};
    }
}

MillenniumDosNativeProcessObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_native_bios_interrupt_return(
    const MillenniumDosNativeBiosInterruptReturnObservation observation) {
    if (admission_ != ReleaseRuntimeAdmission::active || !active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title_handoff_boundary
        || active_->release.game != Game::millennium
        || active_->release.platform != Platform::dos || active_->release.language != "en"
        || active_->release.sha256
            != "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123"
        || !millennium_dos_title_to_game_ || !millennium_dos_native_process_) {
        return {false, "Native BIOS return requires the active English title-to-game continuation"};
    }
    const auto process_checkpoint = millennium_dos_native_process_->checkpoint();
    if (millennium_dos_title_to_game_->state()
            != MillenniumDosTitleToGameState::game_startup_call_boundary
        || !process_checkpoint
        || process_checkpoint->recovery_entry != MillenniumDosNativeRecoveryEntry::startup
        || process_checkpoint->state
            != MillenniumDosNativeProcessState::startup_palette_interrupt_boundary
        || !process_checkpoint->observed_child_process_entry
        || process_checkpoint->child_entry_sequence
            != millennium_dos_title_to_game_->child_entry_sequence()
        || process_checkpoint->child_code_segment
            != millennium_dos_title_to_game_->child_code_segment()
        || process_checkpoint->boundary.kind != MillenniumDosNativeBoundaryKind::bios_interrupt
        || process_checkpoint->boundary.address != observation.interrupt_instruction_address
        || process_checkpoint->boundary.interrupt != observation.interrupt_number
        || observation.return_code_segment != process_checkpoint->child_code_segment
        || observation.return_instruction_pointer != static_cast<std::uint16_t>(
            process_checkpoint->boundary.address + 2U)
        || observation.sequence <= process_checkpoint->last_observation_sequence
        || observation.sequence <= millennium_dos_title_to_game_last_sequence_
        || observation.sequence <= millennium_dos_title_to_game_->child_entry_sequence()) {
        return {false, "Native BIOS return is detached from the exact startup INT $10 boundary"};
    }
    try {
        millennium_dos_native_process_->observe_bios_interrupt_return(observation);
        const auto after_return = millennium_dos_native_process_->checkpoint();
        if (after_return
            && after_return->state
                == MillenniumDosNativeProcessState::startup_palette_loop_return_boundary) {
            if (!native_runtime_memory_ || after_return->startup_ega_palette_write_count != 16) {
                throw std::runtime_error("Completed EGA loop lacks its owned child memory");
            }
            const auto read_word = [&](const std::uint16_t offset)
                -> std::optional<std::uint16_t> {
                const auto lo = native_runtime_memory_->read_byte({
                    NativeRuntimeAddressSpace::dos_segmented,
                    after_return->child_code_segment, offset});
                const auto hi = native_runtime_memory_->read_byte({
                    NativeRuntimeAddressSpace::dos_segmented,
                    after_return->child_code_segment,
                    static_cast<std::uint16_t>(offset + 1U)});
                if (!lo || !hi) return std::nullopt;
                return static_cast<std::uint16_t>(*lo
                    | (static_cast<std::uint16_t>(*hi) << 8U));
            };
            const auto selector = native_runtime_memory_->read_byte({
                NativeRuntimeAddressSpace::dos_segmented,
                after_return->child_code_segment, 0xda05});
            const auto first_return = read_word(0xd9fc);
            const auto outer_return = read_word(0xd9fe);
            if (!selector || !first_return || !outer_return
                || *first_return != 0xd1c3 || *outer_return != 0xd2e5) {
                throw std::runtime_error("EGA caller stack or selector differs from owned memory");
            }
            auto updated_memory = *native_runtime_memory_;
            if (*selector == 2) {
                NativeRuntimeEffectBatch caller_store{
                    "millennium-dos-ega-caller-store-" + std::to_string(observation.sequence),
                    true, {{1,{NativeRuntimeAddressSpace::dos_segmented,
                        after_return->child_code_segment,0x0107},
                        MemoryTransferElementWidth::word,
                        NativeRuntimeByteOrder::little_endian,0xb800}}};
                const auto applied = updated_memory.apply(caller_store);
                if (!applied.accepted) throw std::runtime_error(applied.error);
            }
            const std::array<NativeRuntimeLocation, 2> saved_dx_word{{
                {NativeRuntimeAddressSpace::dos_segmented,
                    after_return->child_code_segment,0xd9fe},
                {NativeRuntimeAddressSpace::dos_segmented,
                    after_return->child_code_segment,0xd9ff}}};
            const auto forgotten_dx = updated_memory.forget(
                "millennium-dos-ega-allocation-unknown-dx-"
                    + std::to_string(observation.sequence), saved_dx_word);
            if (!forgotten_dx.accepted) throw std::runtime_error(forgotten_dx.error);
            NativeRuntimeEffectBatch allocation_stack{
                "millennium-dos-ega-allocation-stack-" + std::to_string(observation.sequence),
                true, {{1,{NativeRuntimeAddressSpace::dos_segmented,
                    after_return->child_code_segment,0xd9fc},
                    MemoryTransferElementWidth::word,
                    NativeRuntimeByteOrder::little_endian,0xd2eb}}};
            const auto stack_applied = updated_memory.apply(allocation_stack);
            if (!stack_applied.accepted) throw std::runtime_error(stack_applied.error);
            millennium_dos_native_process_->continue_startup_ega_caller(
                *selector, *first_return, *outer_return);
            millennium_dos_native_process_->continue_startup_ega_allocator_request(
                *outer_return);
            *native_runtime_memory_ = std::move(updated_memory);
            const auto allocator_checkpoint = millennium_dos_native_process_->checkpoint();
            if (!allocator_checkpoint
                || allocator_checkpoint->boundary.kind
                    != MillenniumDosNativeBoundaryKind::dos_interrupt
                || allocator_checkpoint->boundary.address != 0xd201
                || allocator_checkpoint->boundary.interrupt != 0x21
                || allocator_checkpoint->boundary.service != 0x4a) {
                throw std::runtime_error("EGA startup did not reach DOS AH=$4a boundary");
            }
        }
        return {true, {}};
    } catch (const std::exception& error) {
        return {false, std::string("Native BIOS return rejected: ") + error.what()};
    }
}

MillenniumDosNativeProcessObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_native_dos_interrupt_return(
    const MillenniumDosNativeDosInterruptReturnObservation observation) {
    if (admission_ != ReleaseRuntimeAdmission::active || !active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title_handoff_boundary
        || active_->release.game != Game::millennium
        || active_->release.platform != Platform::dos || active_->release.language != "en"
        || active_->release.sha256
            != "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123"
        || !millennium_dos_title_to_game_ || !millennium_dos_native_process_) {
        return {false, "Native DOS return requires the active English title-to-game continuation"};
    }
    const auto checkpoint = millennium_dos_native_process_->checkpoint();
    if (checkpoint
        && checkpoint->state
            == MillenniumDosNativeProcessState::startup_sibm_timer_vector_get_boundary) {
        const auto reject_sibm = [](const std::string_view reason) {
            return MillenniumDosNativeProcessObservationResult{false, std::string(reason)};
        };
        if (!millennium_dos_sibm_native_service_ || !active_media_
            || !millennium_dos_sound_driver_load_ || !native_runtime_memory_
            || millennium_dos_title_to_game_->state()
                != MillenniumDosTitleToGameState::game_startup_call_boundary
            || !checkpoint->observed_child_process_entry
            || checkpoint->child_entry_sequence
                != millennium_dos_title_to_game_->child_entry_sequence()
            || checkpoint->child_code_segment
                != millennium_dos_title_to_game_->child_code_segment()) {
            return reject_sibm("SIBM AH=$35 return requires the active loaded-driver service");
        }
        const auto service = millennium_dos_sibm_native_service_->checkpoint();
        if (service.state != MillenniumDosSibmNativeServiceState::complete
            || service.generation != millennium_dos_title_to_game_generation_
            || service.source_process_generation != millennium_dos_title_to_game_generation_
            || !service.native_timer_callback_activated
            || service.native_driver_segment != millennium_dos_sound_driver_load_->load_segment()
            || service.source_process_state
                != MillenniumDosNativeProcessState::startup_sibm_timer_vector_get_boundary
            || service.source_process_boundary.address != 0x0233
            || service.loaded_voice_count != 14
            || observation.sequence <= checkpoint->last_observation_sequence
            || observation.sequence <= millennium_dos_title_to_game_last_sequence_
            || observation.sequence <= millennium_dos_title_to_game_->child_entry_sequence()
            || observation.interrupt_instruction_address != 0x0233
            || observation.interrupt_number != 0x21 || observation.service != 0x35
            || observation.return_code_segment != service.native_driver_segment
            || observation.return_instruction_pointer != 0x0235) {
            return reject_sibm("SIBM AH=$35 return is detached from its loaded driver or service generation");
        }
        constexpr std::string_view game_sha256 =
            "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
        constexpr std::string_view driver_sha256 =
            "f3224caa43c1149907f852fa98816ed68c489b70f1ba795592d684d4e51f31b1";
        const auto game = active_media_->borrow(game_sha256);
        const auto driver = active_media_->borrow(driver_sha256);
        if (!game || !driver) {
            return reject_sibm("Hash-admitted SIBM sources are unavailable for the AH=$35 continuation");
        }
        const auto mode = checkpoint->startup_zero_path_selector_value;
        if (!mode || !checkpoint->startup_sibm_function_zero
            || checkpoint->startup_sibm_function_zero->state
                != MillenniumDosSibmFunctionZeroState::awaiting_timer_vector_get) {
            return reject_sibm("SIBM function-zero AH=$35 request is missing from the owned process");
        }
        try {
            MillenniumDosSibmFunctionZeroSession function_zero(*game, *driver);
            const auto prefix = function_zero.execute_startup_helper_prefix(*mode);
            if (prefix.state != MillenniumDosSibmFunctionZeroState::awaiting_timer_vector_get
                || prefix.function_input
                    != checkpoint->startup_sibm_function_zero->function_input
                || prefix.memory_effects
                    != checkpoint->startup_sibm_function_zero->memory_effects) {
                return reject_sibm("SIBM AH=$35 prefix differs from the active owned function-zero state");
            }
            const auto returned = function_zero.observe_timer_vector_get_return(
                observation.sequence, observation.interrupt_instruction_address,
                observation.return_instruction_pointer, observation.ax,
                observation.bx, observation.es);
            if (returned.state != MillenniumDosSibmFunctionZeroState::awaiting_timer_vector_set
                || returned.next_instruction_address != 0x0246
                || returned.memory_effects.size() != 8) {
                return reject_sibm(returned.error.empty()
                    ? "SIBM AH=$35 return did not expose the hash-bound AH=$25 request"
                    : returned.error);
            }
            auto next_memory = *native_runtime_memory_;
            NativeRuntimeEffectBatch stores{
                "millennium-dos-sibm-ah35-vector-result-"
                    + std::to_string(observation.sequence), true, {}};
            for (std::size_t index = 4; index < returned.memory_effects.size(); ++index) {
                const auto& effect = returned.memory_effects[index];
                const NativeRuntimeLocation location{
                    NativeRuntimeAddressSpace::dos_segmented,
                    service.native_driver_segment, effect.address};
                const auto prior = next_memory.read_byte(location);
                if (!prior || *prior != (*driver)[effect.address]) {
                    return reject_sibm("SIBM vector-result target differs from the admitted driver preimage");
                }
                stores.effects.push_back({stores.effects.size() + 1, location,
                    MemoryTransferElementWidth::byte,
                    NativeRuntimeByteOrder::little_endian, effect.value});
            }
            const auto applied = next_memory.apply(stores);
            if (!applied.accepted) {
                return reject_sibm("SIBM AH=$35 driver stores were rejected: " + applied.error);
            }
            millennium_dos_native_process_->observe_startup_sibm_timer_vector_get_return(
                observation, returned, service.native_driver_segment);
            *native_runtime_memory_ = std::move(next_memory);
            return {true, {}};
        } catch (const std::exception& error) {
            return reject_sibm(std::string("SIBM AH=$35 return rejected: ") + error.what());
        }
    }
    if (checkpoint
        && checkpoint->state
            == MillenniumDosNativeProcessState::startup_sibm_timer_vector_set_boundary) {
        const auto reject_sibm = [](const std::string_view reason) {
            return MillenniumDosNativeProcessObservationResult{false, std::string(reason)};
        };
        if (!millennium_dos_sibm_native_service_ || !active_media_
            || !millennium_dos_sound_driver_load_
            || millennium_dos_title_to_game_->state()
                != MillenniumDosTitleToGameState::game_startup_call_boundary
            || !checkpoint->observed_child_process_entry
            || checkpoint->child_entry_sequence
                != millennium_dos_title_to_game_->child_entry_sequence()
            || checkpoint->child_code_segment
                != millennium_dos_title_to_game_->child_code_segment()) {
            return reject_sibm("SIBM AH=$25 return requires the active loaded-driver service");
        }
        const auto service = millennium_dos_sibm_native_service_->checkpoint();
        const auto& prior = checkpoint->startup_sibm_function_zero;
        if (service.state != MillenniumDosSibmNativeServiceState::complete
            || service.generation != millennium_dos_title_to_game_generation_
            || service.source_process_generation != millennium_dos_title_to_game_generation_
            || !service.native_timer_callback_activated
            || service.native_driver_segment != millennium_dos_sound_driver_load_->load_segment()
            || service.source_process_state
                != MillenniumDosNativeProcessState::startup_sibm_timer_vector_get_boundary
            || service.source_process_boundary.address != 0x0233
            || !prior || prior->state != MillenniumDosSibmFunctionZeroState::awaiting_timer_vector_set
            || !prior->timer_vector_offset || !prior->timer_vector_segment
            || !prior->timer_vector_return_ax || prior->last_observation_sequence == 0
            || observation.sequence <= checkpoint->last_observation_sequence
            || observation.sequence <= millennium_dos_title_to_game_last_sequence_
            || observation.sequence <= millennium_dos_title_to_game_->child_entry_sequence()
            || observation.sequence <= prior->last_observation_sequence
            || observation.interrupt_instruction_address != 0x0246
            || observation.interrupt_number != 0x21 || observation.service != 0x25
            || observation.return_code_segment != service.native_driver_segment
            || observation.return_instruction_pointer != 0x0248) {
            return reject_sibm("SIBM AH=$25 return is detached from its loaded driver or prior AH=$35 result");
        }
        constexpr std::string_view game_sha256 =
            "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
        constexpr std::string_view driver_sha256 =
            "f3224caa43c1149907f852fa98816ed68c489b70f1ba795592d684d4e51f31b1";
        const auto game = active_media_->borrow(game_sha256);
        const auto driver = active_media_->borrow(driver_sha256);
        if (!game || !driver) {
            return reject_sibm("Hash-admitted SIBM sources are unavailable for the AH=$25 continuation");
        }
        try {
            MillenniumDosSibmFunctionZeroSession function_zero(*game, *driver);
            const auto mode = checkpoint->startup_zero_path_selector_value;
            if (!mode) {
                return reject_sibm("SIBM AH=$25 return has no hash-bound startup mode");
            }
            const auto prefix = function_zero.execute_startup_helper_prefix(*mode);
            const auto get_return = function_zero.observe_timer_vector_get_return(
                prior->last_observation_sequence, 0x0233, 0x0235,
                *prior->timer_vector_return_ax, *prior->timer_vector_offset,
                *prior->timer_vector_segment);
            if (prefix.state != MillenniumDosSibmFunctionZeroState::awaiting_timer_vector_get
                || prefix.function_input != prior->function_input
                || prefix.memory_effects.size() > prior->memory_effects.size()
                || get_return.state != MillenniumDosSibmFunctionZeroState::awaiting_timer_vector_set
                || get_return.memory_effects != prior->memory_effects) {
                return reject_sibm("SIBM AH=$25 request differs from its ordered AH=$35 continuation");
            }
            const auto returned = function_zero.observe_timer_vector_set_return(
                observation.sequence, observation.interrupt_instruction_address,
                observation.return_instruction_pointer, observation.ax, observation.bx,
                observation.cx, observation.dx, observation.flags, observation.es);
            if (returned.state
                    != MillenniumDosSibmFunctionZeroState::timer_vector_set_return_observed
                || returned.next_instruction_address != 0x0248
                || returned.last_observation_sequence != observation.sequence
                || returned.memory_effects != prior->memory_effects) {
                return reject_sibm(returned.error.empty()
                    ? "SIBM AH=$25 return did not stop at the first stack restore"
                    : returned.error);
            }
            millennium_dos_native_process_->observe_startup_sibm_timer_vector_set_return(
                observation, returned, service.native_driver_segment);
            return {true, {}};
        } catch (const std::exception& error) {
            return reject_sibm(std::string("SIBM AH=$25 return rejected: ") + error.what());
        }
    }
    const bool allocator_second_request = checkpoint
        && checkpoint->state
            == MillenniumDosNativeProcessState::startup_allocator_second_request_boundary;
    const bool allocator_third_request = checkpoint
        && checkpoint->state
            == MillenniumDosNativeProcessState::startup_allocator_third_request_boundary;
    const bool allocator_fourth_request = checkpoint
        && checkpoint->state
            == MillenniumDosNativeProcessState::startup_allocator_fourth_request_boundary;
    const bool allocator_fifth_request = checkpoint
        && checkpoint->state
            == MillenniumDosNativeProcessState::startup_allocator_fifth_request_boundary;
    const bool allocator_sixth_request = checkpoint
        && checkpoint->state
            == MillenniumDosNativeProcessState::startup_allocator_sixth_request_boundary;
    const bool allocator_seventh_request = checkpoint
        && checkpoint->state
            == MillenniumDosNativeProcessState::startup_allocator_seventh_request_boundary;
    const bool allocator_eighth_request = checkpoint
        && checkpoint->state
            == MillenniumDosNativeProcessState::startup_allocator_eighth_request_boundary;
    const bool allocator_ninth_request = checkpoint
        && checkpoint->state
            == MillenniumDosNativeProcessState::startup_allocator_ninth_request_boundary;
    const bool allocator_tenth_request = checkpoint
        && checkpoint->state
            == MillenniumDosNativeProcessState::startup_allocator_tenth_request_boundary;
    const bool allocator_eleventh_request = checkpoint
        && checkpoint->state
            == MillenniumDosNativeProcessState::startup_allocator_eleventh_request_boundary;
    const bool zero_path_file_open_request = checkpoint
        && checkpoint->state
            == MillenniumDosNativeProcessState::startup_zero_path_file_open_request_boundary;
    const bool zero_path_file_read_request = checkpoint
        && checkpoint->state
            == MillenniumDosNativeProcessState::startup_zero_path_file_read_request_boundary;
    const bool zero_path_vector_get_request = checkpoint
        && checkpoint->state
            == MillenniumDosNativeProcessState::startup_zero_path_vector_get_request_boundary;
    if (millennium_dos_title_to_game_->state()
            != MillenniumDosTitleToGameState::game_startup_call_boundary
        || !checkpoint
        || checkpoint->recovery_entry != MillenniumDosNativeRecoveryEntry::startup
        || (checkpoint->state
                != MillenniumDosNativeProcessState::startup_allocation_interrupt_boundary
            && checkpoint->state
                != MillenniumDosNativeProcessState::startup_allocator_second_request_boundary
            && checkpoint->state
                != MillenniumDosNativeProcessState::startup_allocator_free_request_boundary
            && checkpoint->state
                != MillenniumDosNativeProcessState::startup_allocator_third_request_boundary
            && checkpoint->state
                != MillenniumDosNativeProcessState::startup_allocator_file_open_request_boundary
            && checkpoint->state
                != MillenniumDosNativeProcessState::startup_allocator_file_seek_request_boundary
            && checkpoint->state
                != MillenniumDosNativeProcessState::startup_allocator_file_close_request_boundary
            && checkpoint->state
                != MillenniumDosNativeProcessState::startup_allocator_fourth_request_boundary
            && checkpoint->state
                != MillenniumDosNativeProcessState::startup_allocator_fifth_request_boundary
            && checkpoint->state
                != MillenniumDosNativeProcessState::startup_allocator_sixth_request_boundary
            && checkpoint->state
                != MillenniumDosNativeProcessState::startup_allocator_seventh_request_boundary
            && checkpoint->state
                != MillenniumDosNativeProcessState::startup_allocator_eighth_request_boundary
            && checkpoint->state
                != MillenniumDosNativeProcessState::startup_allocator_ninth_request_boundary
            && checkpoint->state
                != MillenniumDosNativeProcessState::startup_allocator_tenth_request_boundary
            && checkpoint->state
                != MillenniumDosNativeProcessState::startup_allocator_eleventh_request_boundary
            && checkpoint->state
                != MillenniumDosNativeProcessState::startup_zero_path_file_open_request_boundary
            && checkpoint->state
                != MillenniumDosNativeProcessState::startup_zero_path_file_read_request_boundary
            && checkpoint->state
                != MillenniumDosNativeProcessState::startup_zero_path_file_close_request_boundary
            && checkpoint->state
                != MillenniumDosNativeProcessState::startup_zero_path_vector_get_request_boundary
            && checkpoint->state
                != MillenniumDosNativeProcessState::startup_zero_path_vector_set_request_boundary)
        || !checkpoint->observed_child_process_entry
        || checkpoint->child_entry_sequence
            != millennium_dos_title_to_game_->child_entry_sequence()
        || checkpoint->child_code_segment
            != millennium_dos_title_to_game_->child_code_segment()
        || checkpoint->boundary.kind != MillenniumDosNativeBoundaryKind::dos_interrupt
        || checkpoint->boundary.address != observation.interrupt_instruction_address
        || checkpoint->boundary.interrupt != observation.interrupt_number
        || checkpoint->boundary.service != observation.service
        || observation.return_code_segment != checkpoint->child_code_segment
        || observation.sequence <= checkpoint->last_observation_sequence
        || observation.sequence <= millennium_dos_title_to_game_last_sequence_
        || observation.sequence <= millennium_dos_title_to_game_->child_entry_sequence()) {
        return {false, "Native DOS return is detached from the exact startup INT $21 boundary"};
    }
    try {
        std::optional<NativeRuntimeMemory> next_memory;
        if (zero_path_file_open_request) {
            if (!native_runtime_memory_) {
                return {false, "Startup file-handle store requires owned DOS child memory"};
            }
            next_memory = *native_runtime_memory_;
            const auto applied = next_memory->apply({
                "millennium-dos-startup-file-handle-store-"
                    + std::to_string(observation.sequence), true, {{
                    1, {NativeRuntimeAddressSpace::dos_segmented,
                        checkpoint->child_code_segment, 0xda00},
                    MemoryTransferElementWidth::word,
                    NativeRuntimeByteOrder::little_endian, observation.ax}}});
            if (!applied.accepted) {
                return {false, "Startup file-handle store was rejected: " + applied.error};
            }
        } else if (zero_path_file_read_request && (observation.flags & 1U) == 0U) {
            if (!native_runtime_memory_ || !active_media_
                || !checkpoint->startup_zero_path_file_read_request
                || !checkpoint->startup_zero_path_file_open_request
                || !checkpoint->startup_zero_path_file_open_dos_return) {
                return {false, "Startup file read has no owned request or verified media"};
            }
            const auto& request = *checkpoint->startup_zero_path_file_read_request;
            const auto& open = *checkpoint->startup_zero_path_file_open_request;
            std::string_view expected_hash;
            std::size_t expected_size = 0;
            if (open.dx_name_address == 0x1131) {
                expected_hash = "c31cb760d5f62a21b3baf9c09a6be413514780bd88eeac0273620e81b5d69318";
                expected_size = 1024;
            } else if (open.dx_name_address == 0x1149) {
                expected_hash = "063eefa58c98360d0ca2b4eaf9a77f8f9d13c619aee605dff6b0d0ee8b4a6b20";
                expected_size = 2040;
            } else {
                return {false, "Selected DOS text leaf is not present in the admitted release manifest"};
            }
            if (request.instruction_address != 0x0580 || request.service != 0x3f
                || request.cx != 0xffff || request.dx != 0 || request.ds == 0
                || request.bx != checkpoint->startup_zero_path_file_open_dos_return->ax
                || open.instruction_address != 0x054d || open.service != 0x3d
                || open.ax != 0x3d02 || !open.data_segment_is_code_segment
                || active_media_->release().sha256 != active_->release.sha256) {
                return {false, "Startup text read differs from its source-ordered DOS request"};
            }
            const auto segment_low = native_runtime_memory_->read_byte({
                NativeRuntimeAddressSpace::dos_segmented, checkpoint->child_code_segment, 0x011c});
            const auto segment_high = native_runtime_memory_->read_byte({
                NativeRuntimeAddressSpace::dos_segmented, checkpoint->child_code_segment, 0x011d});
            if (!segment_low || !segment_high
                || static_cast<std::uint16_t>(*segment_low
                    | (static_cast<std::uint16_t>(*segment_high) << 8U)) != request.ds) {
                return {false, "Startup text destination differs from the owned allocator result"};
            }
            const auto original_leaf = active_media_->borrow(expected_hash);
            if (!original_leaf || original_leaf->size() != expected_size) {
                return {false, "Selected DOS text leaf is absent or changed in verified release media"};
            }
            next_memory = *native_runtime_memory_;
            const auto committed = commit_millennium_dos_native_file_read(
                *next_memory, millennium_dos_title_to_game_generation_, observation.sequence,
                request.ds, request.dx, request.cx, observation.ax, false, *original_leaf);
            if (!committed.accepted) {
                return {false, "Startup text bytes were not committed: " + committed.error};
            }
        } else if (zero_path_vector_get_request) {
            if (!native_runtime_memory_) {
                return {false, "Vector-query result store requires owned DOS child memory"};
            }
            next_memory = *native_runtime_memory_;
            const auto applied = next_memory->apply({
                "millennium-dos-startup-int93-vector-store-"
                    + std::to_string(observation.sequence), true, {{
                    1, {NativeRuntimeAddressSpace::dos_segmented,
                        checkpoint->child_code_segment, 0x4255},
                    MemoryTransferElementWidth::word,
                    NativeRuntimeByteOrder::little_endian, observation.bx}, {
                    2, {NativeRuntimeAddressSpace::dos_segmented,
                        checkpoint->child_code_segment, 0x4257},
                    MemoryTransferElementWidth::word,
                    NativeRuntimeByteOrder::little_endian, observation.es}}});
            if (!applied.accepted) {
                return {false, "Vector-query result store was rejected: " + applied.error};
            }
        } else if (allocator_second_request) {
            if (!native_runtime_memory_
                || !native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::dos_segmented,
                    checkpoint->child_code_segment, 0xd12e})
                || !native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::dos_segmented,
                    checkpoint->child_code_segment, 0xd12f})) {
                return {false, "Allocator store target is not present in owned DOS child memory"};
            }
            next_memory = *native_runtime_memory_;
            const auto applied = next_memory->apply({
                "millennium-dos-allocator-ah48-store-"
                    + std::to_string(observation.sequence), true, {{
                    1, {NativeRuntimeAddressSpace::dos_segmented,
                        checkpoint->child_code_segment, 0xd12e},
                    MemoryTransferElementWidth::word,
                    NativeRuntimeByteOrder::little_endian, observation.bx}}});
            if (!applied.accepted) {
                return {false, "Allocator local store was rejected: " + applied.error};
            }
        } else if (allocator_third_request) {
            if (!native_runtime_memory_
                || !native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::dos_segmented,
                    checkpoint->child_code_segment, 0xd114})
                || !native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::dos_segmented,
                    checkpoint->child_code_segment, 0xd115})) {
                return {false, "Third allocator store target is not present in owned DOS child memory"};
            }
            next_memory = *native_runtime_memory_;
            const auto applied = next_memory->apply({
                "millennium-dos-allocator-ah48-third-store-"
                    + std::to_string(observation.sequence), true, {{
                    1, {NativeRuntimeAddressSpace::dos_segmented,
                    checkpoint->child_code_segment, 0xd114},
                    MemoryTransferElementWidth::word,
                    NativeRuntimeByteOrder::little_endian, observation.ax}}});
            if (!applied.accepted) {
                return {false, "Third allocator local store was rejected: " + applied.error};
            }
        } else if (allocator_fourth_request) {
            if (!native_runtime_memory_
                || !native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::dos_segmented,
                    checkpoint->child_code_segment, 0x0116})
                || !native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::dos_segmented,
                    checkpoint->child_code_segment, 0x0117})) {
                return {false, "Fourth allocator store target is not present in owned DOS child memory"};
            }
            next_memory = *native_runtime_memory_;
            const auto applied = next_memory->apply({
                "millennium-dos-allocator-ah48-fourth-store-"
                    + std::to_string(observation.sequence), true, {{
                    1, {NativeRuntimeAddressSpace::dos_segmented,
                        checkpoint->child_code_segment, 0x0116},
                    MemoryTransferElementWidth::word,
                    NativeRuntimeByteOrder::little_endian, observation.ax}}});
            if (!applied.accepted) {
                return {false, "Fourth allocator local store was rejected: " + applied.error};
            }
        } else if (allocator_fifth_request || allocator_sixth_request
            || allocator_seventh_request || allocator_eighth_request
            || allocator_tenth_request) {
            const std::uint16_t target = allocator_fifth_request ? std::uint16_t{0x011c}
                : allocator_sixth_request ? std::uint16_t{0x0118}
                : allocator_seventh_request ? std::uint16_t{0x010c} : std::uint16_t{0x0110};
            const std::uint16_t effective_target = allocator_tenth_request
                ? std::uint16_t{0x105a} : target;
            if (!native_runtime_memory_
                || !native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::dos_segmented,
                    checkpoint->child_code_segment, effective_target})
                || !native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::dos_segmented,
                    checkpoint->child_code_segment, static_cast<std::uint16_t>(effective_target + 1U)})) {
                return {false, "Allocator local result store target is not present in owned DOS child memory"};
            }
            next_memory = *native_runtime_memory_;
            const auto applied = next_memory->apply({
                std::string("millennium-dos-allocator-result-store-")
                    + std::to_string(observation.sequence), true, {{
                    1, {NativeRuntimeAddressSpace::dos_segmented,
                        checkpoint->child_code_segment, effective_target},
                    MemoryTransferElementWidth::word,
                    NativeRuntimeByteOrder::little_endian, observation.ax}}});
            if (!applied.accepted) {
                return {false, "Allocator local result store was rejected: " + applied.error};
            }
        } else if (allocator_ninth_request || allocator_eleventh_request) {
            const std::array<std::uint16_t, 2> targets = allocator_ninth_request
                ? std::array<std::uint16_t, 2>{0x1044, 0x1048}
                : std::array<std::uint16_t, 2>{0xd12a, 0xd128};
            if (!native_runtime_memory_) {
                return {false, "Allocator return stores require owned DOS child memory"};
            }
            next_memory = *native_runtime_memory_;
            NativeRuntimeEffectBatch batch{
                "millennium-dos-allocator-dual-store-"
                    + std::to_string(observation.sequence), true, {}};
            for (std::size_t index = 0; index < targets.size(); ++index) {
                if (!native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::dos_segmented,
                        checkpoint->child_code_segment, targets[index]})
                    || !native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::dos_segmented,
                        checkpoint->child_code_segment,
                        static_cast<std::uint16_t>(targets[index] + 1U)})) {
                    return {false, "Allocator dual-store target is not present in owned DOS child memory"};
                }
                batch.effects.push_back({index + 1U,
                    {NativeRuntimeAddressSpace::dos_segmented,
                        checkpoint->child_code_segment, targets[index]},
                    MemoryTransferElementWidth::word,
                    NativeRuntimeByteOrder::little_endian, observation.ax});
            }
            const auto applied = next_memory->apply(batch);
            if (!applied.accepted) {
                return {false, "Allocator dual-store application rejected: " + applied.error};
            }
        }
        millennium_dos_native_process_->observe_dos_interrupt_return(observation);
        if (next_memory) *native_runtime_memory_ = std::move(*next_memory);
        const auto continued = millennium_dos_native_process_->checkpoint();
        if (continued
            && continued->state
                == MillenniumDosNativeProcessState::startup_zero_path_continuation_byte_boundary
            && continued->boundary.kind == MillenniumDosNativeBoundaryKind::runtime_byte
            && continued->boundary.address == 0xd2fd
            && continued->boundary.runtime_byte_address == 0x0082) {
            const auto local_step =
                execute_millennium_dos_native_runtime_byte_from_owned_memory();
            if (!local_step.accepted) {
                return {true, "DOS return accepted; source-backed local $d2fd read failed: "
                    + local_step.error};
            }
        }
        return {true, {}};
    } catch (const std::exception& error) {
        return {false, std::string("Native DOS return rejected: ") + error.what()};
    }
}

MillenniumDosNativeProcessObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_native_runtime_byte(
    const MillenniumDosNativeRuntimeByteObservation observation) {
    if (admission_ != ReleaseRuntimeAdmission::active || !active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title_handoff_boundary
        || active_->release.game != Game::millennium
        || active_->release.platform != Platform::dos || active_->release.language != "en"
        || !millennium_dos_title_to_game_ || !millennium_dos_native_process_
        || !native_runtime_memory_) {
        return {false, "Native DOS byte requires the active owned English game process"};
    }
    const auto checkpoint = millennium_dos_native_process_->checkpoint();
    if (!checkpoint || !checkpoint->observed_child_process_entry
        || checkpoint->child_entry_sequence != millennium_dos_title_to_game_->child_entry_sequence()
        || checkpoint->child_code_segment != millennium_dos_title_to_game_->child_code_segment()
        || checkpoint->boundary.kind != MillenniumDosNativeBoundaryKind::runtime_byte
        || checkpoint->boundary.address != observation.instruction_address
        || checkpoint->boundary.runtime_byte_address != observation.runtime_address
        || observation.sequence <= checkpoint->last_observation_sequence
        || observation.sequence <= millennium_dos_title_to_game_last_sequence_
        || observation.sequence <= millennium_dos_title_to_game_->child_entry_sequence()) {
        return {false, "Native DOS byte is detached from the exact startup read boundary"};
    }
    const auto source_location = NativeRuntimeLocation{NativeRuntimeAddressSpace::dos_segmented,
        checkpoint->child_code_segment, observation.runtime_address};
    const auto owned_source = native_runtime_memory_->read_byte(source_location);
    if (!owned_source || *owned_source != observation.value) {
        return {false, "Startup CS:$0082 byte differs from owned DOS child memory"};
    }
    const auto decoded = static_cast<std::uint8_t>(observation.value - 0x30U);
    const NativeRuntimeLocation target{NativeRuntimeAddressSpace::dos_segmented,
        checkpoint->child_code_segment, 0x0122};
    if (!native_runtime_memory_->read_byte(target)) {
        return {false, "Startup selector store target is not present in owned DOS child memory"};
    }
    auto next_memory = *native_runtime_memory_;
    const auto applied = next_memory.apply({
        "millennium-dos-startup-selector-byte-store-"
            + std::to_string(observation.sequence), true, {{
            1, target, MemoryTransferElementWidth::byte,
            NativeRuntimeByteOrder::little_endian, decoded}}});
    if (!applied.accepted) {
        return {false, "Startup selector byte store was rejected: " + applied.error};
    }
    std::optional<MillenniumDosSibmFunctionZeroCheckpoint> sibm_prefix;
    if (checkpoint->state
        == MillenniumDosNativeProcessState::startup_zero_path_continuation_byte_boundary) {
        constexpr std::string_view game_sha256 =
            "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
        constexpr std::string_view sibm_driver_sha256 =
            "f3224caa43c1149907f852fa98816ed68c489b70f1ba795592d684d4e51f31b1";
        if (!active_media_) {
            return {false, "SIBM startup requires the active verified release media"};
        }
        const auto game = active_media_->borrow(game_sha256);
        const auto sibm_driver = active_media_->borrow(sibm_driver_sha256);
        if (!game) {
            return {false, "Hash-admitted English Millennium executable is unavailable"};
        }
        if (!sibm_driver) {
            return {false, "Hash-admitted SIBM driver is unavailable in the active release"};
        }
        const auto mode_byte = next_memory.read_byte(target);
        if (!mode_byte || *mode_byte != decoded) {
            return {false, "Owned SIBM mode byte differs from the decoded startup value"};
        }
        try {
            MillenniumDosSibmFunctionZeroSession function_zero(*game, *sibm_driver);
            sibm_prefix = function_zero.execute_startup_helper_prefix(*mode_byte);
            if (sibm_prefix->state
                    != MillenniumDosSibmFunctionZeroState::awaiting_timer_vector_get
                || sibm_prefix->next_instruction_address != 0x0233
                || sibm_prefix->dos_ax != 0x3508) {
                return {false, sibm_prefix->error.empty()
                    ? "SIBM helper did not reach the admitted AH=$35 boundary"
                    : sibm_prefix->error};
            }
        } catch (const std::exception& error) {
            return {false, std::string("Native SIBM startup prefix rejected: ") + error.what()};
        }
    }
    try {
        millennium_dos_native_process_->observe_runtime_byte(
            observation.sequence, observation.instruction_address,
            observation.runtime_address, observation.value);
        if (sibm_prefix) {
            millennium_dos_native_process_->observe_startup_sibm_function_zero_prefix(
                *sibm_prefix);
        }
        *native_runtime_memory_ = std::move(next_memory);
        if (sibm_prefix) {
            // The accepted selector byte now owns the exact `$0233` process
            // boundary. Start the Eon service as a parallel service session;
            // its callback/loader checkpoint never advances the DOS process.
            const auto native_service = activate_millennium_dos_sibm_native_service(
                millennium_dos_title_to_game_generation_);
            if (!native_service.accepted) {
                return {true, "DOS byte accepted at $0233; native SIBM service was not activated: "
                    + native_service.error};
            }
        }
        return {true, {}};
    } catch (const std::exception& error) {
        return {false, std::string("Native DOS byte rejected: ") + error.what()};
    }
}

MillenniumDosNativeProcessObservationResult
ReleaseRuntimeCoordinator::execute_millennium_dos_native_runtime_byte_from_owned_memory() {
    if (admission_ != ReleaseRuntimeAdmission::active || !active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title_handoff_boundary
        || active_->release.game != Game::millennium
        || active_->release.platform != Platform::dos || active_->release.language != "en"
        || !millennium_dos_title_to_game_ || !millennium_dos_native_process_
        || !native_runtime_memory_) {
        return {false, "Owned DOS runtime-byte execution requires the active English game process"};
    }
    const auto process = millennium_dos_native_process_->checkpoint();
    if (!process
        || process->child_entry_sequence
            != millennium_dos_title_to_game_->child_entry_sequence()
        || process->child_code_segment != millennium_dos_title_to_game_->child_code_segment()) {
        return {false, "Owned DOS runtime-byte process differs from the active child entry"};
    }
    const auto request = derive_millennium_dos_owned_runtime_byte_request(*process,
        *native_runtime_memory_, millennium_dos_title_to_game_last_sequence_);
    if (!request.accepted || !request.observation) return {false, request.error};
    return observe_millennium_dos_native_runtime_byte(*request.observation);
}

std::optional<MillenniumDosTitleToGameCheckpoint>
ReleaseRuntimeCoordinator::millennium_dos_title_to_game_checkpoint() const {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title_handoff_boundary
        || !millennium_dos_title_to_game_) return std::nullopt;
    return MillenniumDosTitleToGameCheckpoint{
        millennium_dos_title_to_game_generation_, millennium_dos_title_to_game_last_sequence_,
        millennium_dos_title_to_game_->state(), millennium_dos_title_to_game_->boundary(),
        millennium_dos_title_to_game_->effects(), millennium_dos_title_to_game_->register_effects(),
        millennium_dos_title_to_game_->stack_word_effects(),
        millennium_dos_title_to_game_->restored_stack_pointer(),
        millennium_dos_title_to_game_->child_status_al(),
        millennium_dos_title_to_game_->game_exec_sequence(),
        millennium_dos_title_to_game_->child_entry_sequence(),
        millennium_dos_title_to_game_->child_code_segment(),
        millennium_dos_title_to_game_->initial_stack_segment(),
        millennium_dos_title_to_game_->initial_stack_pointer(),
        millennium_dos_title_to_game_->child_entry_provenance()};
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
    auto checkpoint = millennium_dos_native_process_->checkpoint();
    if (!checkpoint || checkpoint->recovery_entry != MillenniumDosNativeRecoveryEntry::startup) {
        return std::nullopt;
    }
    return checkpoint;
}

MillenniumDosNativeProcessObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_static_data_loader_entry(
    const MillenniumDosStaticDataLoaderEntryObservation observation) {
    if (admission_ != ReleaseRuntimeAdmission::active || !active_ || !active_media_
        || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title_handoff_boundary
        || active_->release.game != Game::millennium
        || active_->release.platform != Platform::dos || active_->release.language != "en"
        || active_->release.sha256
            != "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123"
        || !millennium_dos_title_to_game_ || !millennium_dos_native_process_
        || !native_runtime_memory_ || millennium_dos_static_data_load_) {
        return {false, "Static-data loader requires one active English DOS game handoff"};
    }
    const auto handoff = millennium_dos_title_to_game_checkpoint();
    const auto process = millennium_dos_native_process_->checkpoint();
    if (!handoff || !process || process->recovery_entry != MillenniumDosNativeRecoveryEntry::startup
        || !process->observed_child_process_entry || process->child_code_segment == 0
        || process->child_code_segment != handoff->child_code_segment
        || process->child_entry_sequence != handoff->child_entry_sequence
        || millennium_dos_title_to_game_->state()
            != MillenniumDosTitleToGameState::game_startup_call_boundary
        || observation.sequence == 0
        || observation.sequence <= process->last_observation_sequence
        || observation.sequence <= millennium_dos_title_to_game_last_sequence_
        || observation.sequence <= handoff->child_entry_sequence
        || observation.code_segment != process->child_code_segment) {
        return {false, "Static-data loader entry is detached from the active DOS child"};
    }
    const auto low = native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::dos_segmented,
        process->child_code_segment, 0x0116});
    const auto high = native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::dos_segmented,
        process->child_code_segment, 0x0117});
    if (!low || !high) {
        return {false, "Static-data loader has no owned CS:$0116 allocation result"};
    }
    const auto data_segment = static_cast<std::uint16_t>(
        *low | (static_cast<std::uint16_t>(*high) << 8U));
    const auto game_executable = active_media_->borrow(
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57");
    if (!game_executable
        || active_media_->release().sha256 != active_->release.sha256) {
        return {false, "Static-data loader has no verified active game executable"};
    }
    try {
        MillenniumDosStaticDataLoadSession pending(*game_executable,
            process->child_code_segment, data_segment);
        pending.observe_loader_entry(observation.sequence, observation.call_instruction,
            observation.target_address, observation.return_address, observation.code_segment);
        millennium_dos_static_data_load_.emplace(std::move(pending));
        return {true, {}};
    } catch (const std::exception& error) {
        return {false, std::string("Static-data loader entry rejected: ") + error.what()};
    }
}

MillenniumDosNativeProcessObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_static_data_load_dos_interrupt_return(
    const MillenniumDosNativeDosInterruptReturnObservation observation) {
    if (admission_ != ReleaseRuntimeAdmission::active || !active_ || !active_media_
        || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title_handoff_boundary
        || active_->release.game != Game::millennium
        || active_->release.platform != Platform::dos || active_->release.language != "en"
        || active_->release.sha256
            != "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123"
        || !millennium_dos_native_process_ || !millennium_dos_static_data_load_
        || !native_runtime_memory_) {
        return {false, "Static-data DOS return requires its active independent loader boundary"};
    }
    const auto process = millennium_dos_native_process_->checkpoint();
    if (!process || !process->observed_child_process_entry
        || process->child_code_segment
            != millennium_dos_static_data_load_->checkpoint().code_segment
        || process->game_executable_sha256
            != "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57") {
        return {false, "Static-data DOS return has no exact active child process"};
    }
    try {
        auto pending = *millennium_dos_static_data_load_;
        const auto transfer = pending.observe_dos_return({
            observation.sequence, observation.interrupt_instruction_address,
            observation.interrupt_number, observation.service,
            observation.return_code_segment, observation.return_instruction_pointer,
            observation.ax, observation.bx, observation.cx, observation.dx,
            observation.flags});
        auto memory = *native_runtime_memory_;
        if (transfer) {
            constexpr std::string_view static_data_sha256 =
                "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d";
            const auto original_bytes = active_media_->borrow(static_data_sha256);
            if (!original_bytes || original_bytes->size() != transfer->returned_byte_count
                || process->child_code_segment != transfer->destination_segment) {
                throw std::runtime_error("Static-data destination or verified media does not match");
            }
            const auto low = memory.read_byte({NativeRuntimeAddressSpace::dos_segmented,
                process->child_code_segment, 0x0116});
            const auto high = memory.read_byte({NativeRuntimeAddressSpace::dos_segmented,
                process->child_code_segment, 0x0117});
            if (!low || !high
                || static_cast<std::uint16_t>(*low
                    | (static_cast<std::uint16_t>(*high) << 8U))
                    != transfer->destination_segment) {
                throw std::runtime_error("Static-data destination differs from owned CS:$0116");
            }
            const auto committed = commit_millennium_dos_native_file_read(
                memory, active_runtime_generation_, transfer->sequence,
                transfer->destination_segment, transfer->destination_offset,
                transfer->requested_byte_count, transfer->returned_byte_count,
                false, *original_bytes);
            if (!committed.accepted) {
                throw std::runtime_error("Static-data bytes were not committed: "
                    + committed.error);
            }
        }
        *millennium_dos_static_data_load_ = std::move(pending);
        *native_runtime_memory_ = std::move(memory);
        return {true, {}};
    } catch (const std::exception& error) {
        return {false, std::string("Static-data DOS return rejected: ") + error.what()};
    }
}

MillenniumDosNativeProcessObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_static_data_loader_return(
    const MillenniumDosStaticDataLoaderCallReturnObservation observation) {
    if (admission_ != ReleaseRuntimeAdmission::active || !active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title_handoff_boundary
        || !millennium_dos_static_data_load_ || !millennium_dos_native_process_) {
        return {false, "No active Millennium DOS static-data loader"};
    }
    const auto process = millennium_dos_native_process_->checkpoint();
    if (!process || !process->observed_child_process_entry
        || process->child_code_segment != observation.code_segment) {
        return {false, "Static-data loader return has no exact active DOS child"};
    }
    try {
        auto pending = *millennium_dos_static_data_load_;
        pending.observe_loader_return(observation.sequence, observation.instruction_address,
            observation.returned_to, observation.code_segment);
        *millennium_dos_static_data_load_ = std::move(pending);
        return {true, {}};
    } catch (const std::exception& error) {
        return {false, std::string("Static-data loader return rejected: ") + error.what()};
    }
}

std::optional<MillenniumDosStaticDataLoadCheckpoint>
ReleaseRuntimeCoordinator::millennium_dos_static_data_load_checkpoint() const {
    if (!active_ || !session_snapshot_ || !millennium_dos_static_data_load_
        || active_->release.game != Game::millennium
        || active_->release.platform != Platform::dos || active_->release.language != "en"
        || active_->release.sha256
            != "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123") {
        return std::nullopt;
    }
    return millennium_dos_static_data_load_->checkpoint();
}

MillenniumDosSibmNativeServiceResult
ReleaseRuntimeCoordinator::activate_millennium_dos_sibm_native_service(
    const std::uint64_t expected_generation) {
    MillenniumDosSibmNativeServiceResult result;
    if (admission_ != ReleaseRuntimeAdmission::active || !active_ || !session_snapshot_
        || active_->release.game != Game::millennium
        || active_->release.platform != Platform::dos || active_->release.language != "en"
        || active_->release.sha256
            != "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123"
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title_handoff_boundary
        || !active_media_ || !millennium_dos_ || !millennium_dos_->sibm_native_voice_bank
        || !millennium_dos_title_to_game_ || !millennium_dos_native_process_
        || !millennium_dos_sound_driver_load_ || !millennium_dos_compatibility_runner_) {
        result.error = "SIBM native service requires the active English Millennium DOS handoff";
        return result;
    }
    if (millennium_dos_sibm_native_service_) {
        result.error = "SIBM native service is already active for this handoff";
        return result;
    }
    const auto process = millennium_dos_native_process_->checkpoint();
    const auto handoff = millennium_dos_title_to_game_checkpoint();
    if (!process || !handoff || expected_generation == 0
        || expected_generation != millennium_dos_title_to_game_generation_
        || process->child_entry_sequence != handoff->child_entry_sequence
        || process->child_code_segment != handoff->child_code_segment) {
        result.error = "SIBM native service generation does not match the active handoff";
        return result;
    }
    constexpr std::string_view sibm_driver_sha256 =
        "f3224caa43c1149907f852fa98816ed68c489b70f1ba795592d684d4e51f31b1";
    const auto driver = active_media_->borrow(sibm_driver_sha256);
    if (!driver) {
        result.error = "Hash-admitted SIBM driver is unavailable in the active release";
        return result;
    }
    const auto& loaded_driver = *millennium_dos_sound_driver_load_;
    if (!loaded_driver.has_eon_sibm_int95()) {
        result.error = "SIBM startup requires the loaded IBM speaker driver at the owned INT $95 vector";
        return result;
    }
    // The 2200AD child word at CS:$011c belongs to its own INT $93 helper,
    // not to SIBM. The SIBM code segment is the paragraph allocated by
    // MILL.COM and selected by the authenticated INT $95 vector.
    const auto native_driver_segment = loaded_driver.load_segment();
    if (native_driver_segment == 0 || native_driver_segment == process->child_code_segment) {
        result.error = "Owned SIBM destination segment is invalid or aliases the game child";
        return result;
    }
    auto runner = *millennium_dos_compatibility_runner_;
    const auto arena_paragraphs = MillenniumDosSibmStartupSession::
        required_startup_paragraphs(*millennium_dos_->sibm_native_voice_bank);
    const auto reserved_arena = runner.allocate_paragraphs(arena_paragraphs);
    if (!reserved_arena.allocation) {
        result.error = "Native SIBM startup could not reserve its shared DOS memory range: "
            + reserved_arena.error;
        return result;
    }
    const auto arena_begin = reserved_arena.allocation->segment;
    const auto arena_end32 = static_cast<std::uint32_t>(arena_begin) + arena_paragraphs;
    const auto inside_reserved_range = [arena_begin, arena_end32](const std::uint16_t segment) {
        return segment >= arena_begin && static_cast<std::uint32_t>(segment) < arena_end32;
    };
    if (inside_reserved_range(native_driver_segment)
        || inside_reserved_range(process->child_code_segment)
        || arena_end32 > std::numeric_limits<std::uint16_t>::max()) {
        result.error = "Native SIBM startup reservation aliases another DOS allocation";
        return result;
    }
    try {
        millennium_dos_sibm_native_service_.emplace(expected_generation,
            millennium_dos_title_to_game_generation_, *process, *driver,
            *millennium_dos_->sibm_native_voice_bank, native_driver_segment,
            arena_begin, static_cast<std::uint16_t>(arena_end32), runner.generation());
        result.checkpoint = millennium_dos_sibm_native_service_->checkpoint();
        result.accepted = result.checkpoint->state
            == MillenniumDosSibmNativeServiceState::complete;
        if (!result.accepted) {
            result.error = result.checkpoint->error;
        } else {
            auto memory = *native_runtime_memory_;
            const auto applied = commit_millennium_dos_sibm_native_memory(
                memory, *result.checkpoint, *driver);
            if (!applied.accepted) throw std::runtime_error(applied.error);
            *native_runtime_memory_ = std::move(memory);
            *millennium_dos_compatibility_runner_ = std::move(runner);
        }
    } catch (const std::exception& error) {
        result.error = error.what();
        millennium_dos_sibm_native_service_.reset();
    }
    return result;
}

std::optional<MillenniumDosSibmNativeServiceCheckpoint>
ReleaseRuntimeCoordinator::millennium_dos_sibm_native_service_checkpoint() const {
    if (admission_ != ReleaseRuntimeAdmission::active || !active_ || !session_snapshot_
        || active_->release.game != Game::millennium
        || active_->release.platform != Platform::dos || active_->release.language != "en"
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_title_handoff_boundary
        || !millennium_dos_sibm_native_service_) return std::nullopt;
    return millennium_dos_sibm_native_service_->checkpoint();
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

MillenniumDosTitleHandoffTraceAdmission
ReleaseRuntimeCoordinator::admit_millennium_dos_title_handoff_reference_trace(
    const ReferenceTrace& trace) const {
    MillenniumDosTitleHandoffTraceAdmission rejected;
    constexpr std::string_view mill_sha256 =
        "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e";
    constexpr std::string_view titles_sha256 =
        "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
    const auto* descriptor = reference_trace_adapter_descriptor(trace.adapter);
    if (!descriptor
        || descriptor->runtime_policy != ReferenceTraceRuntimePolicy::transient_title_handoff
        || trace.source_release.game != descriptor->game
        || trace.source_release.platform != descriptor->platform
        || trace.source_release.language != descriptor->language
        || trace.source_release.sha256 != descriptor->release_sha256) {
        rejected.error = "Reference trace does not name the exact Millennium DOS title-handoff boundary";
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
        const auto launcher = media.extract(mill_sha256);
        const auto titles = media.extract(titles_sha256);
        if (!launcher || !titles) {
            rejected.error = "Verified title-handoff leaves are unavailable";
            return rejected;
        }
        return admit_millennium_dos_title_handoff_trace(*launcher, *titles, events);
    } catch (...) {
        rejected.error = "Unable to admit the Millennium DOS title-handoff boundary";
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
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_post_overlay_loop
        || !millennium_dos_post_overlay_loop_ || !millennium_dos_native_process_
        || !native_runtime_memory_) {
        result.error = "Runtime-byte observation requires the active post-overlay loop";
        return result;
    }
    try {
        const auto process = millennium_dos_native_process_->checkpoint();
        if (!process || !process->observed_child_process_entry
            || process->child_code_segment == 0
            || process->release_sha256 != active_->release.sha256
            || process->game_executable_sha256
                != "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57") {
            result.error = "Runtime-byte observation has no exact active DOS child segment";
            return result;
        }
        auto pending = *millennium_dos_post_overlay_loop_;
        const auto prior_effect_count = pending.runtime_effects().size();
        pending.observe_runtime_byte(
            observation.load_address, observation.runtime_address, observation.value);
        const auto& effects = pending.runtime_effects();
        if (effects.size() != prior_effect_count + 1U) {
            result.error = "Post-overlay runtime-byte boundary produced no unique memory effect";
            return result;
        }
        const auto& effect = effects.back();
        if (effect.address != observation.runtime_address
            || effect.previous != observation.value
            || effect.value != static_cast<std::uint8_t>(observation.value ^ 1U)) {
            result.error = "Post-overlay runtime-byte effect differs from the observed XOR source";
            return result;
        }

        auto memory = *native_runtime_memory_;
        const auto applied = commit_millennium_dos_post_overlay_runtime_byte_effect(
            memory, active_runtime_generation_, prior_effect_count + 1U,
            process->child_code_segment, effect);
        if (!applied.accepted) {
            result.error = applied.error;
            return result;
        }
        *native_runtime_memory_ = std::move(memory);
        *millennium_dos_post_overlay_loop_ = std::move(pending);
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

MillenniumDosSharedHelperObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_shared_helper_entry(
    const MillenniumDosSharedHelperEntryObservation observation) {
    MillenniumDosSharedHelperObservationResult result;
    if (!session_snapshot_
        || (session_snapshot_->kind != RuntimeSessionKind::millennium_dos_seventh_function
            && session_snapshot_->kind != RuntimeSessionKind::millennium_dos_post_overlay_loop)
        || !millennium_dos_seventh_function_ || !millennium_dos_native_process_
        || !millennium_dos_static_data_load_ || millennium_dos_shared_helper_) {
        result.error = "Shared-helper entry requires its loaded static-data segment and active F7 boundary";
        return result;
    }
    const auto loaded = millennium_dos_static_data_load_->checkpoint();
    if (loaded.state != MillenniumDosStaticDataLoadState::complete || !loaded.bytes_read) {
        result.error = "Shared-helper entry requires the source-ordered 2200AD4.BIN read";
        return result;
    }
    const auto boundary = millennium_dos_seventh_function_->boundary();
    if (observation.sequence == 0 || observation.call_instruction != 0x7537
        || observation.target_address != 0x0666 || observation.caller_ax != 0x012a
        || boundary.kind != MillenniumDosSeventhFunctionBoundaryKind::call_return
        || boundary.instruction_address != observation.call_instruction
        || boundary.call_target != observation.target_address
        || boundary.known_ax != observation.caller_ax) {
        result.error = "Shared-helper entry does not match the exact F7 call";
        return result;
    }
    try {
        millennium_dos_shared_helper_.emplace(
            millennium_dos_native_process_->make_shared_helper_session(observation.caller_ax));
        millennium_dos_shared_helper_entry_ = observation;
        result.accepted = true;
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}
MillenniumDosSharedHelperObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_special_action_helper_entry(
    const MillenniumDosSpecialActionHelperEntryObservation observation) {
    MillenniumDosSharedHelperObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_post_overlay_loop
        || !millennium_dos_post_overlay_loop_ || !millennium_dos_native_process_
        || !millennium_dos_static_data_load_
        || !native_runtime_memory_ || millennium_dos_shared_helper_) {
        result.error = "Special-action helper requires its active dispatch boundary";
        return result;
    }
    const auto loaded_static_data = millennium_dos_static_data_load_->checkpoint();
    if (loaded_static_data.state != MillenniumDosStaticDataLoadState::complete
        || !loaded_static_data.bytes_read) {
        result.error = "Special-action helper requires the source-ordered 2200AD4.BIN read";
        return result;
    }
    const auto boundary = millennium_dos_post_overlay_loop_->boundary();
    if (observation.sequence == 0 || observation.dispatch_call != 0xd40e
        || observation.dispatch_target != 0x11a4 || observation.runtime_address != 0x07f9
        || observation.helper_call != 0x11b7 || observation.helper_target != 0x0666
        || millennium_dos_post_overlay_loop_->observed_action()
            != std::optional<std::uint8_t>{0x0b}
        || boundary.kind != MillenniumDosPostOverlayLoopBoundaryKind::dispatch_call
        || boundary.instruction_address != observation.dispatch_call
        || boundary.call_target != observation.dispatch_target) {
        result.error = "Special-action helper entry does not match exact action-$0b dispatch";
        return result;
    }
    try {
        const auto process = millennium_dos_native_process_->checkpoint();
        if (!process || !process->observed_child_process_entry
            || process->child_code_segment == 0
            || process->release_sha256 != active_->release.sha256
            || process->game_executable_sha256
                != "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57") {
            throw std::runtime_error("Special-action entry has no exact active DOS child segment");
        }

        const NativeRuntimeLocation location{NativeRuntimeAddressSpace::dos_segmented,
            process->child_code_segment, observation.runtime_address};
        auto memory = *native_runtime_memory_;
        if (const auto current = memory.read_byte(location); current) {
            if (*current != observation.runtime_value) {
                throw std::runtime_error(
                    "Special-action byte observation contradicts owned DOS memory");
            }
        } else {
            NativeRuntimeEffectBatch batch{
                "millennium-dos-special-action-byte-" + std::to_string(active_runtime_generation_)
                    + "-" + std::to_string(observation.sequence),
                true,
                {{1, location, MemoryTransferElementWidth::byte,
                    NativeRuntimeByteOrder::little_endian, observation.runtime_value}}};
            const auto applied = memory.apply(batch);
            if (!applied.accepted) throw std::runtime_error(applied.error);
        }

        auto game = millennium_dos_native_process_->make_game_session();
        static_cast<void>(game.observe_action({0x0f05, 0x0b}));
        const auto prefix = game.observe_first_special_action(
            {observation.runtime_address, observation.runtime_value});
        if (prefix.selected_ax_value != observation.caller_ax) {
            throw std::runtime_error("Explicit AX differs from the hash-bound prefix");
        }
        static_cast<void>(game.observe_first_special_action_shared_helper_prefix());
        auto helper = millennium_dos_native_process_->make_shared_helper_session(
            observation.caller_ax);

        // Execute the helper's two local reads only when both operands are
        // already completely owned. Missing values retain the explicit
        // observation boundary; no DOS segment or far-table value is guessed.
        auto helper_memory = std::move(memory);
        static_cast<void>(advance_millennium_dos_shared_helper_from_owned_memory(
            helper, helper_memory, active_runtime_generation_, process->child_code_segment));

        *native_runtime_memory_ = std::move(helper_memory);
        millennium_dos_shared_helper_.emplace(std::move(helper));
        millennium_dos_shared_helper_entry_ = MillenniumDosSharedHelperEntryObservation{
            observation.helper_call, observation.helper_target,
            observation.caller_ax, observation.sequence};
        millennium_dos_special_action_ = std::move(game);
        result.accepted = true;
    } catch (const std::exception& exception) {
        result.error = std::string("Special-action helper entry rejected: ")
            + exception.what();
    }
    return result;
}
#define EON_SHARED_HELPER_FORWARD(name,type,body) MillenniumDosSharedHelperObservationResult ReleaseRuntimeCoordinator::name(const type o){MillenniumDosSharedHelperObservationResult r;if(!session_snapshot_||(session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_seventh_function&&session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_post_overlay_loop)||!millennium_dos_shared_helper_||millennium_dos_shared_helper_return_){r.error="No active shared helper";return r;}try{millennium_dos_shared_helper_->body;r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
MillenniumDosSharedHelperObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_shared_helper_word(
    const MillenniumDosSharedHelperWordObservation observation) {
    MillenniumDosSharedHelperObservationResult result;
    if (!active_ || !session_snapshot_
        || (session_snapshot_->kind != RuntimeSessionKind::millennium_dos_seventh_function
            && session_snapshot_->kind != RuntimeSessionKind::millennium_dos_post_overlay_loop)
        || !millennium_dos_shared_helper_ || !millennium_dos_native_process_
        || !native_runtime_memory_ || millennium_dos_shared_helper_return_) {
        result.error = "Shared-helper segment read requires its active native child";
        return result;
    }
    try {
        const auto process = millennium_dos_native_process_->checkpoint();
        if (!process || !process->observed_child_process_entry
            || process->child_code_segment == 0
            || process->release_sha256 != active_->release.sha256
            || process->game_executable_sha256
                != "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57") {
            throw std::runtime_error("Shared-helper segment read has no exact DOS child segment");
        }
        auto helper = *millennium_dos_shared_helper_;
        const auto segment_check = validate_millennium_dos_shared_helper_segment_word(
            *native_runtime_memory_, process->child_code_segment, observation.value);
        if (!segment_check.accepted) throw std::runtime_error(segment_check.error);
        helper.observe_runtime_word(observation.instruction_address,
            observation.address, observation.value);
        if (helper.effects().size() != 1U) {
            throw std::runtime_error("Shared-helper emitted an unexpected effect count");
        }
        auto memory = *native_runtime_memory_;
        const auto applied = commit_millennium_dos_shared_helper_effect(
            memory, active_runtime_generation_, helper.effects().size(),
            process->child_code_segment, helper.effects().front());
        if (!applied.accepted) throw std::runtime_error(applied.error);

        *native_runtime_memory_ = std::move(memory);
        *millennium_dos_shared_helper_ = std::move(helper);
        result.accepted = true;
    } catch (const std::exception& exception) {
        result.error = std::string("Shared-helper segment read rejected: ")
            + exception.what();
    }
    return result;
}
MillenniumDosSharedHelperObservationResult
ReleaseRuntimeCoordinator::observe_millennium_dos_shared_helper_far_word(
    const MillenniumDosSharedHelperFarWordObservation observation) {
    MillenniumDosSharedHelperObservationResult result;
    if (!active_ || !session_snapshot_
        || (session_snapshot_->kind != RuntimeSessionKind::millennium_dos_seventh_function
            && session_snapshot_->kind != RuntimeSessionKind::millennium_dos_post_overlay_loop)
        || !millennium_dos_shared_helper_ || !millennium_dos_native_process_
        || !native_runtime_memory_ || millennium_dos_shared_helper_return_) {
        result.error = "Shared-helper table read requires its active native child";
        return result;
    }
    try {
        const auto process = millennium_dos_native_process_->checkpoint();
        if (!process || !process->observed_child_process_entry
            || process->child_code_segment == 0
            || process->release_sha256 != active_->release.sha256
            || process->game_executable_sha256
                != "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57") {
            throw std::runtime_error("Shared-helper table read has no exact DOS child segment");
        }
        auto helper = *millennium_dos_shared_helper_;
        helper.observe_far_word(observation.instruction_address, observation.segment,
            observation.offset, observation.value);

        auto memory = *native_runtime_memory_;
        const auto applied = commit_millennium_dos_shared_helper_table_word(
            memory, active_runtime_generation_, observation.segment,
            observation.offset, observation.value);
        if (!applied.accepted) throw std::runtime_error(applied.error);

        *native_runtime_memory_ = std::move(memory);
        *millennium_dos_shared_helper_ = std::move(helper);
        result.accepted = true;
    } catch (const std::exception& exception) {
        result.error = std::string("Shared-helper table read rejected: ")
            + exception.what();
    }
    return result;
}
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
EON_SIXTH_FORWARD(observe_millennium_dos_sixth_function_al,
    MillenniumDosSixthFunctionAlObservation,
    observe_al(observation.instruction_address, observation.value),
    "Sixth-function AL observation")
#undef EON_SIXTH_FORWARD

MillenniumDosSixthFunctionObservationResult
ReleaseRuntimeCoordinator::begin_millennium_dos_sixth_function_restoration() {
    MillenniumDosSixthFunctionObservationResult result;
    if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_dos_sixth_function
        ||!millennium_dos_sixth_function_){result.error="Sixth-function restoration requires the active session";return result;}
    try{millennium_dos_sixth_function_->begin_restoration();result.accepted=true;}
    catch(const std::exception& exception){result.error=std::string("Sixth-function restoration rejected: ")+exception.what();}
    return result;
}

MillenniumDosSixthFunctionObservationResult
ReleaseRuntimeCoordinator::begin_millennium_dos_sixth_function_caller_helper_prefix() {
    MillenniumDosSixthFunctionObservationResult result;
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_sixth_function
        || !millennium_dos_sixth_function_) {
        result.error = "F6 caller-helper prefix requires the active session";
        return result;
    }
    try {
        millennium_dos_sixth_function_->begin_restoration_caller_helper_prefix();
        result.accepted = true;
    } catch (const std::exception& exception) {
        result.error = std::string("F6 caller-helper prefix rejected: ") + exception.what();
    }
    return result;
}

std::optional<MillenniumDosSixthFunctionCheckpoint>
ReleaseRuntimeCoordinator::millennium_dos_sixth_function_checkpoint() const {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_dos_sixth_function
        || !millennium_dos_sixth_function_) return std::nullopt;
    const auto& session = *millennium_dos_sixth_function_;
    return MillenniumDosSixthFunctionCheckpoint{session.state(), session.boundary(),
        session.effects(), session.shifted_bl_values(),
        session.caller_helper_far_clear_effect(), session.caller_helper_saved_byte(),
        session.caller_helper_state_clear_effect(), session.caller_helper_bulk_effects()};
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
        auto kind = MillenniumDosOwnedFunctionBoundaryKind::local_return;
        switch (boundary.kind) {
        case MillenniumDosSixthFunctionBoundaryKind::register_al:
            kind=MillenniumDosOwnedFunctionBoundaryKind::register_value;
            break;
        case MillenniumDosSixthFunctionBoundaryKind::runtime_word: kind=MillenniumDosOwnedFunctionBoundaryKind::runtime_word; break;
        case MillenniumDosSixthFunctionBoundaryKind::runtime_byte: kind=MillenniumDosOwnedFunctionBoundaryKind::runtime_byte; break;
        case MillenniumDosSixthFunctionBoundaryKind::call_return: kind=MillenniumDosOwnedFunctionBoundaryKind::call_return; break;
        case MillenniumDosSixthFunctionBoundaryKind::register_bl: kind=MillenniumDosOwnedFunctionBoundaryKind::register_value; break;
        case MillenniumDosSixthFunctionBoundaryKind::local_return: kind=MillenniumDosOwnedFunctionBoundaryKind::local_return; break;
        case MillenniumDosSixthFunctionBoundaryKind::external_continuation: kind=MillenniumDosOwnedFunctionBoundaryKind::external_continuation; break;
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
    case RuntimeInputContract::deuteros_amiga_title_raw_key:
        if (session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage) {
            return RuntimeInputDisposition::rejected;
        }
        if (observation.kind == RuntimeInputObservationKind::deuteros_amiga_raw_key_event) {
            return dispatch_deuteros_amiga_title_raw_key({1,0,
                observation.raw_key_code,observation.qualifier,0,0}).accepted
                ? RuntimeInputDisposition::observed : RuntimeInputDisposition::rejected;
        }
        if (observation.kind == RuntimeInputObservationKind::deuteros_amiga_raw_mouse_button_event) {
            return dispatch_deuteros_amiga_title_raw_key({2,0,
                observation.raw_mouse_code,observation.qualifier,0,0}).accepted
                ? RuntimeInputDisposition::observed : RuntimeInputDisposition::rejected;
        }
        if (observation.kind == RuntimeInputObservationKind::deuteros_amiga_raw_mouse_motion_event) {
            const auto result = dispatch_deuteros_amiga_title_raw_key({2,0,
                0x00ff,observation.qualifier,observation.raw_mouse_x,
                observation.raw_mouse_y});
            return !result.accepted ? RuntimeInputDisposition::rejected
                : result.stop_address == 0x201b6
                    ? RuntimeInputDisposition::boundary_reached
                    : RuntimeInputDisposition::observed;
        }
        return RuntimeInputDisposition::rejected;
    case RuntimeInputContract::millennium_dos_post_overlay_console_queue:
        if (session_snapshot_->kind != RuntimeSessionKind::millennium_dos_post_overlay_loop
            || !millennium_dos_post_overlay_loop_) {
            return RuntimeInputDisposition::rejected;
        }
        if (observation.kind == RuntimeInputObservationKind::millennium_dos_console_flush) {
            millennium_dos_post_overlay_loop_->clear_console_input();
            return RuntimeInputDisposition::observed;
        }
        if (observation.kind == RuntimeInputObservationKind::millennium_dos_console_character) {
            const auto value = static_cast<std::uint8_t>(
                static_cast<unsigned char>(observation.ascii_character));
            return millennium_dos_post_overlay_loop_->enqueue_console_character(value)
                ? RuntimeInputDisposition::observed : RuntimeInputDisposition::rejected;
        }
        if (observation.kind == RuntimeInputObservationKind::millennium_dos_function_key) {
            return millennium_dos_post_overlay_loop_->enqueue_function_key(
                observation.dos_scan_code)
                ? RuntimeInputDisposition::observed : RuntimeInputDisposition::rejected;
        }
        return RuntimeInputDisposition::rejected;
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
        const auto title_snapshot = *session_snapshot_;
        session_snapshot_ = make_runtime_session_snapshot(*active_,
            RuntimeSessionKind::millennium_dos_sound_driver_boundary);
        // The old route made the visible launcher wait for a test-only host
        // injection of an arbitrary child CS.  This is now explicitly Eon's
        // compatibility-process address, not an original allocation or a
        // captured value. Exact leaf operations, SetVect, the parent SP store
        // and the bounded TITLES.EXE child entry are handled by separate Eon
        // compatibility services with explicit provenance.
        constexpr std::uint16_t compatibility_process_code_segment = 0xe000;
        if (!begin_millennium_dos_sound_driver_load(1,
                compatibility_process_code_segment,
                MillenniumDosSoundDriverCodeSegmentProvenance::eon_compatibility_process)
                .accepted) {
            session_snapshot_ = title_snapshot;
            return RuntimeInputDisposition::rejected;
        }
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
        if (!active_media_) throw std::runtime_error("Active verified media is unavailable");
        const auto& media = *active_media_;
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
        if (!active_ || !deuteros_amiga_->title_stage_session()
            || !native_runtime_memory_) return std::nullopt;
        // The boot track contains a distinct graphics initialization image
        // at $13000. Execute its authenticated prefix before the title image
        // overwrites that address, and stop at AvailMem without manufacturing
        // a memory-size return or allowing the title allocator to run.
        constexpr std::uint32_t bootstrap_entry = 0x13000;
        constexpr std::uint32_t bootstrap_length = 108;
        constexpr std::uint32_t open_call = 0x1300c;
        constexpr std::uint32_t open_return = 0x13010;
        constexpr std::uint32_t availmem_call = 0x13028;
        constexpr std::string_view bootstrap_hash =
            "ca2490219557649ff335d47ccc9d15fcbaa34da2ab9c723feb6bcea147970c0f";
        auto memory = *native_runtime_memory_;
        const auto code = memory.read_linear_range(bootstrap_entry, bootstrap_length);
        if (!code || to_hex(sha256(*code)) != bootstrap_hash) return std::nullopt;

        DeuterosAmigaExecCompatibilitySession exec(active_runtime_generation_);
        const auto exec_pointer = exec.install_bootstrap_exec_base_pointer(memory);
        if (!exec_pointer || exec_pointer->origin
                != DeuterosAmigaExecCompatibilityOrigin::eon_native_compatibility
            || exec_pointer->pointer_cell != 4
            || exec_pointer->virtual_exec_base
                != DeuterosAmigaExecCompatibilitySession::exec_base_address) {
            return std::nullopt;
        }
        std::array<std::uint8_t, 4> exec_pointer_bytes{
            static_cast<std::uint8_t>(exec_pointer->virtual_exec_base >> 24U),
            static_cast<std::uint8_t>(exec_pointer->virtual_exec_base >> 16U),
            static_cast<std::uint8_t>(exec_pointer->virtual_exec_base >> 8U),
            static_cast<std::uint8_t>(exec_pointer->virtual_exec_base)};
        std::array<m68k::MemoryRange, 1> prefix_memory{{
            {4, exec_pointer_bytes, false}}};
        m68k::MachineState initial;
        initial.pc = bootstrap_entry;
        const auto prefix = m68k::execute(*code, bootstrap_entry, initial,
            prefix_memory, 3, open_call);
        if (prefix.reason != m68k::StopReason::requested_address
            || prefix.state.pc != open_call || prefix.instructions_executed != 3
            || prefix.state.data[0] != 0 || prefix.state.address[1] != 0x12c28
            || prefix.state.address[6] != exec_pointer->virtual_exec_base) {
            return std::nullopt;
        }
        constexpr std::string_view graphics_name = "graphics.library";
        const auto name_bytes = memory.read_linear_range(0x12c28,
            graphics_name.size() + 1U);
        if (!name_bytes || !std::equal(graphics_name.begin(), graphics_name.end(),
                name_bytes->begin()) || name_bytes->back() != 0) {
            return std::nullopt;
        }
        DeuterosAmigaGraphicsLibraryCompatibilitySession graphics(
            active_runtime_generation_);
        const auto open_return_value = graphics.open_bootstrap_graphics_library(
            bootstrap_entry, prefix.state.address[1], 4, open_call, -0x228,
            open_return, graphics_name, 0);
        if (!open_return_value
            || open_return_value->origin != DeuterosAmigaServiceOrigin::native_compatibility
            || open_return_value->native_service_token == 0) {
            return std::nullopt;
        }

        const auto mutable_prefix = memory.read_linear_range(0x12fec, 12);
        if (!mutable_prefix) return std::nullopt;
        std::vector<std::uint8_t> mutable_bytes = *mutable_prefix;
        std::array<m68k::MemoryRange, 2> continuation_memory{{
            {4, exec_pointer_bytes, false}, {0x12fec, mutable_bytes, true}}};
        m68k::MachineState continuation;
        continuation.pc = open_return;
        continuation.data[0] = open_return_value->native_service_token;
        const auto local = m68k::execute(*code, bootstrap_entry, continuation,
            continuation_memory, 5, availmem_call);
        if (local.reason != m68k::StopReason::requested_address
            || local.state.pc != availmem_call || local.instructions_executed != 5
            || local.state.address[6] != exec_pointer->virtual_exec_base
            || static_cast<std::uint16_t>(local.state.data[1]) != 4
            || mutable_bytes[0] != static_cast<std::uint8_t>(
                open_return_value->native_service_token >> 24U)
            || mutable_bytes[1] != static_cast<std::uint8_t>(
                open_return_value->native_service_token >> 16U)
            || mutable_bytes[2] != static_cast<std::uint8_t>(
                open_return_value->native_service_token >> 8U)
            || mutable_bytes[3] != static_cast<std::uint8_t>(
                open_return_value->native_service_token)
            || mutable_bytes[8] != 0 || mutable_bytes[9] != 0
            || mutable_bytes[10] != 0xab || mutable_bytes[11] != 0) {
            return std::nullopt;
        }
        const auto initial_display = memory.read_linear_range(0x12ff4, 4);
        if (!initial_display || *initial_display != std::vector<std::uint8_t>(4, 0))
            return std::nullopt;
        NativeRuntimeEffectBatch initialization{
            "deuteros-amiga-eon-bootstrap-openlibrary-prefix-"
                + std::to_string(active_runtime_generation_), true,
            {{1, {NativeRuntimeAddressSpace::linear, std::nullopt, 0x12fec},
                MemoryTransferElementWidth::longword,
                NativeRuntimeByteOrder::big_endian,
                open_return_value->native_service_token},
             {2, {NativeRuntimeAddressSpace::linear, std::nullopt, 0x12ff4},
                MemoryTransferElementWidth::longword,
                NativeRuntimeByteOrder::big_endian, 0xab00}}};
        if (!memory.apply(initialization).accepted) return std::nullopt;
        DeuterosAmigaBootstrapGraphicsBoundarySnapshot bootstrap_boundary;
        bootstrap_boundary.runtime_generation = active_runtime_generation_;
        bootstrap_boundary.source_address = bootstrap_entry;
        bootstrap_boundary.source_length = bootstrap_length;
        bootstrap_boundary.source_sha256 = std::string(bootstrap_hash);
        bootstrap_boundary.exec_base_pointer_cell = 4;
        bootstrap_boundary.eon_virtual_exec_base = exec_pointer->virtual_exec_base;
        bootstrap_boundary.open_library_call = open_call;
        bootstrap_boundary.open_library_return = open_return;
        bootstrap_boundary.eon_graphics_library_token = open_return_value->native_service_token;
        bootstrap_boundary.stop_before_address = availmem_call;
        bootstrap_boundary.stop_vector = -0xd8;
        bootstrap_boundary.stop_return_address = 0x1302c;
        bootstrap_boundary.stop_d1_value = 4;
        bootstrap_boundary.source_instruction_count = 8;
        bootstrap_boundary.initial_display_base = 0;
        bootstrap_boundary.display_base_after_open_library = 0xab00;
        bootstrap_boundary.open_library_origin = open_return_value->origin;
        deuteros_amiga_bootstrap_graphics_boundary_ = std::move(bootstrap_boundary);
        deuteros_amiga_graphics_library_compatibility_ = graphics;
        *native_runtime_memory_ = std::move(memory);
        session_snapshot_ = make_runtime_session_snapshot(*active_,
            RuntimeSessionKind::deuteros_amiga_bootstrap_graphics_boundary);
        deuteros_amiga_opening_input_held_ = false;
        deuteros_amiga_paula_.reset();
        return events;
    }
    return events;
}

DeuterosAmigaMainStageDriveResult
ReleaseRuntimeCoordinator::drive_deuteros_amiga_main_stage(const std::uint32_t step_limit) {
    DeuterosAmigaMainStageDriveResult result;
    if (step_limit == 0) {
        result.error = "Deuteros main-stage drive requires a nonzero step limit";
        return result;
    }
    if (!active_ || !deuteros_amiga_ || !deuteros_amiga_->title_stage_session()
        || !native_runtime_memory_) {
        result.error = "Deuteros main-stage drive requires an active title session";
        return result;
    }
    result.accepted = true;
    const auto execute_initial_loop_service = [&]() {
        DeuterosAmigaTitleDependencyObservationResult advanced;
        try {
            if (!active_ || !deuteros_amiga_ || !deuteros_amiga_->title_stage_session()
                || !native_runtime_memory_) {
                advanced.error = "Deuteros initial loop service requires active owned memory";
                return advanced;
            }
            auto pending = *deuteros_amiga_->title_stage_session();
            if (!pending.advance_main_stage_loop_service_native()) {
                advanced.error = "Deuteros initial loop service is outside its deterministic boundary";
                return advanced;
            }
            auto memory = *native_runtime_memory_;
            std::map<std::uint32_t, std::uint8_t> writes;
            const auto read = [&](const std::uint32_t address, const std::uint32_t width) {
                if (width == 0 || width > 4 || address > 0x1000000U - width)
                    throw std::runtime_error("Deuteros initial audio read is outside native memory");
                std::uint32_t value = 0;
                for (std::uint32_t index = 0; index < width; ++index) {
                    const auto staged = writes.find(address + index);
                    const auto byte = staged != writes.end()
                        ? std::optional<std::uint8_t>{staged->second}
                        : memory.read_byte({NativeRuntimeAddressSpace::linear,
                            std::nullopt, address + index});
                    if (!byte) throw std::runtime_error(
                        "Deuteros initial audio source is absent from owned memory");
                    value = (value << 8U) | *byte;
                }
                return value;
            };
            const auto write = [&](const std::uint32_t address,
                const MemoryTransferElementWidth transfer_width, const std::uint32_t value) {
                std::uint32_t width = 0;
                switch (transfer_width) {
                case MemoryTransferElementWidth::byte: width = 1; break;
                case MemoryTransferElementWidth::word: width = 2; break;
                case MemoryTransferElementWidth::longword: width = 4; break;
                }
                if (address > 0x1000000U - width)
                    throw std::runtime_error("Deuteros initial audio write is outside native memory");
                for (std::uint32_t index = 0; index < width; ++index) {
                    const auto shift = (width - index - 1U) * 8U;
                    writes[address + index] = static_cast<std::uint8_t>(value >> shift);
                }
            };
            const auto require_hash = [&](const std::uint32_t address,
                const std::uint32_t length, const std::string_view expected) {
                std::vector<std::uint8_t> bytes;
                bytes.reserve(length);
                for (std::uint32_t index = 0; index < length; ++index) {
                    const auto byte = read(address + index, 1);
                    bytes.push_back(static_cast<std::uint8_t>(byte));
                }
                if (to_hex(sha256(bytes)) != expected)
                    throw std::runtime_error("Deuteros initial local service bytes changed");
            };
            require_hash(0x22a5a, 16,
                "ec2f836b1613a0aaf24099396c38c467d04c937cafaaeb5d529494491700ecf9");
            require_hash(0x22ab8, 0xd2,
                "592bc54b35e0075154b66abcc615e3bab580626898d3801e2142cdf8bd658614");
            require_hash(0x217fe, 30,
                "382932f57f5d3d181f93727198a6f5b7d6fff91522eed0438778a43e8ca417d5");

            // $22a5a clears its software selector and tail-calls the
            // hash-locked $22ab8 routine with sound zero on all four channels.
            write(0x22a30, MemoryTransferElementWidth::byte, 0);
            std::uint32_t sound = 0;
            std::uint16_t channels = 15;
            stage_deuteros_amiga_owned_sound(sound, channels, read, write);
            write(0x21720, MemoryTransferElementWidth::word, 0);
            write(0x2171e, MemoryTransferElementWidth::word, 0);
            write(0x210f2, MemoryTransferElementWidth::word, 1);

            NativeRuntimeEffectBatch batch{"deuteros-amiga-initial-loop-service-native", true, {}};
            batch.effects.reserve(writes.size());
            std::size_t sequence = 1;
            for (const auto& [address, value] : writes) {
                batch.effects.push_back({sequence++,
                    {NativeRuntimeAddressSpace::linear, std::nullopt, address},
                    MemoryTransferElementWidth::byte,
                    NativeRuntimeByteOrder::big_endian, value});
            }
            const auto applied = memory.apply(batch);
            if (!applied.accepted) {
                advanced.error = applied.error;
                return advanced;
            }
            if (!deuteros_amiga_->advance_main_stage_loop_service_native()) {
                advanced.error = "Deuteros initial loop service boundary changed before commit";
                return advanced;
            }
            *native_runtime_memory_ = std::move(memory);
            advanced.accepted = true;
        } catch (const std::exception& error) {
            advanced.error = error.what();
        }
        return advanced;
    };
    const auto execute_main_stage_exec_compatibility = [&]() {
        DeuterosAmigaTitleDependencyObservationResult advanced;
        if (!deuteros_amiga_exec_compatibility_ || !deuteros_amiga_
            || !deuteros_amiga_->title_stage_session() || !native_runtime_memory_) {
            advanced.error = "Deuteros main-stage Exec compatibility is unavailable";
            return advanced;
        }
        try {
            auto exec = *deuteros_amiga_exec_compatibility_;
            auto pending = *deuteros_amiga_->title_stage_session();
            auto memory = *native_runtime_memory_;
            const auto sequence = pending.main_stage_last_command_sequence() + 1U;
            const auto origin = DeuterosAmigaMainStageExecReturnOrigin::eon_native_compatibility;
            std::optional<DeuterosAmigaObservedMainStageExecReturn> service_result;
            if (pending.main_stage_state() == DeuterosAmigaMainStageState::awaiting_2099e_exec_return) {
                const auto task = exec.find_main_stage_task(4, 0x2099e, -0x126, 0x209a2, 0);
                if (!task || task->task_address != DeuterosAmigaExecCompatibilitySession::task_address)
                    throw std::runtime_error("Eon Exec graph rejected main-stage FindTask(NULL)");
                service_result = DeuterosAmigaObservedMainStageExecReturn{
                    sequence, 0x2099e, -0x126, 0x209a2, task->task_address};
                service_result->origin = origin;
                auto plan = pending.observe_main_stage_2099e_exec_return(*service_result);
                if (!plan) throw std::runtime_error("Main-stage FindTask transition was rejected");
                NativeRuntimeEffectBatch batch{"deuteros-amiga-main-stage-findtask-compat-"
                    + std::to_string(active_runtime_generation_), true, {}};
                const std::array widths{MemoryTransferElementWidth::longword,
                    MemoryTransferElementWidth::byte, MemoryTransferElementWidth::byte,
                    MemoryTransferElementWidth::byte, MemoryTransferElementWidth::longword};
                for (std::size_t i = 0; i < plan->destination_addresses.size(); ++i)
                    batch.effects.push_back({i + 1U,
                        {NativeRuntimeAddressSpace::linear, std::nullopt,
                            plan->destination_addresses[i]}, widths[i],
                        NativeRuntimeByteOrder::big_endian, plan->values[i]});
                const auto applied = memory.apply(batch);
                if (!applied.accepted) throw std::runtime_error(applied.error);
            } else if (pending.main_stage_state() == DeuterosAmigaMainStageState::awaiting_209ca_exec_return) {
                const auto port = exec.add_main_stage_port(memory, 4, 0x209ca, -0x162,
                    0x209ce, 0x20954);
                if (!port) throw std::runtime_error("Eon Exec graph rejected main-stage AddPort");
                service_result = DeuterosAmigaObservedMainStageExecReturn{
                    sequence, 0x209ca, -0x162, 0x209ce, 0};
                service_result->origin = origin;
                service_result->result_d0_defined = false; // AddPort is void; caller ignores D0.
                auto plan = pending.observe_main_stage_209ca_exec_return(*service_result);
                if (!plan) throw std::runtime_error("Main-stage AddPort transition was rejected");
                NativeRuntimeEffectBatch batch{"deuteros-amiga-main-stage-addport-compat-"
                    + std::to_string(active_runtime_generation_), true, {}};
                for (std::size_t i = 0; i < plan->destination_addresses.size(); ++i)
                    batch.effects.push_back({i + 1U,
                        {NativeRuntimeAddressSpace::linear, std::nullopt,
                            plan->destination_addresses[i]}, MemoryTransferElementWidth::longword,
                        NativeRuntimeByteOrder::big_endian, plan->values[i]});
                const auto applied = memory.apply(batch);
                if (!applied.accepted) throw std::runtime_error(applied.error);
            } else if (pending.main_stage_state() == DeuterosAmigaMainStageState::awaiting_209f0_exec_return) {
                const auto opened = exec.open_main_stage_trackdisk_unit(memory, 4,
                    0x209f0, -0x1bc, 0x209f4, 0x20982, 0x2091c, 0x20954, 0, 0);
                if (!opened || opened->service_generated_io_error != 0)
                    throw std::runtime_error("Eon trackdisk service rejected main-stage OpenDevice");
                service_result = DeuterosAmigaObservedMainStageExecReturn{
                    sequence, 0x209f0, -0x1bc, 0x209f4, 0};
                service_result->call_a0 = 0x20982;
                service_result->call_a1 = 0x2091c;
                service_result->origin = origin;
                auto plan = pending.observe_main_stage_209f0_exec_return(*service_result);
                if (!plan || plan->terminal_spin)
                    throw std::runtime_error("Main-stage OpenDevice success transition was rejected");
                NativeRuntimeEffectBatch batch{"deuteros-amiga-main-stage-open-device-compat-"
                    + std::to_string(active_runtime_generation_), true, {
                    {1, {NativeRuntimeAddressSpace::linear, std::nullopt,
                            plan->destination_addresses[0]}, MemoryTransferElementWidth::longword,
                        NativeRuntimeByteOrder::big_endian, plan->values[0]},
                    {2, {NativeRuntimeAddressSpace::linear, std::nullopt,
                            plan->destination_addresses[1]}, MemoryTransferElementWidth::byte,
                        NativeRuntimeByteOrder::big_endian, plan->values[1]}}};
                const auto applied = memory.apply(batch);
                if (!applied.accepted) throw std::runtime_error(applied.error);
            } else {
                advanced.error = "Deuteros main-stage Exec compatibility is outside its three exact calls";
                return advanced;
            }
            bool committed = false;
            if (pending.main_stage_state() == DeuterosAmigaMainStageState::awaiting_209ca_exec_return)
                committed = static_cast<bool>(deuteros_amiga_->observe_main_stage_2099e_exec_return(*service_result));
            else if (pending.main_stage_state() == DeuterosAmigaMainStageState::awaiting_209f0_exec_return)
                committed = static_cast<bool>(deuteros_amiga_->observe_main_stage_209ca_exec_return(*service_result));
            else
                committed = static_cast<bool>(deuteros_amiga_->observe_main_stage_209f0_exec_return(*service_result));
            if (!committed)
                throw std::runtime_error("Deuteros title session changed before native Exec commit");
            *deuteros_amiga_exec_compatibility_ = std::move(exec);
            *native_runtime_memory_ = std::move(memory);
            advanced.accepted = true;
        } catch (const std::exception& error) {
            advanced.error = error.what();
        }
        return advanced;
    };
    for (; result.steps < step_limit;) {
        const auto checkpoint = deuteros_amiga_title_dependency_chain_checkpoint();
        if (!checkpoint) {
            result.accepted = false;
            result.error = "Deuteros main-stage checkpoint disappeared during drive";
            return result;
        }
        DeuterosAmigaTitleDependencyObservationResult advanced;
        bool deterministic = true;
        if (checkpoint->main_stage_state == DeuterosAmigaMainStageState::awaiting_reentry_d0) {
            advanced = advance_deuteros_amiga_main_stage_reentry_d0();
        } else if (checkpoint->main_stage_state == DeuterosAmigaMainStageState::awaiting_20994_entry) {
            advanced = advance_deuteros_amiga_main_stage_20994_exec_entry();
        } else if (checkpoint->main_stage_state == DeuterosAmigaMainStageState::awaiting_2099e_exec_return
            || checkpoint->main_stage_state == DeuterosAmigaMainStageState::awaiting_209ca_exec_return
            || checkpoint->main_stage_state == DeuterosAmigaMainStageState::awaiting_209f0_exec_return) {
            advanced = execute_main_stage_exec_compatibility();
        } else if (checkpoint->main_stage_state
            == DeuterosAmigaMainStageState::awaiting_initial_loop_service_return) {
            advanced = execute_initial_loop_service();
        } else if (checkpoint->main_stage_state == DeuterosAmigaMainStageState::awaiting_loop_prepare_body) {
            advanced = advance_deuteros_amiga_main_stage_loop_prepare_body();
        } else if (checkpoint->main_stage_state == DeuterosAmigaMainStageState::awaiting_record_loop) {
            advanced = advance_deuteros_amiga_main_stage_record_loop();
        } else if (checkpoint->main_stage_state
                == DeuterosAmigaMainStageState::awaiting_first_loop_graphics_return
            || checkpoint->main_stage_state
                == DeuterosAmigaMainStageState::awaiting_second_loop_graphics_return) {
            advanced = execute_deuteros_amiga_main_stage_loop_graphics_native_service();
        } else if (checkpoint->main_stage_loop_graphics
            && (checkpoint->main_stage_loop_graphics->next_instruction_address == 0x21380
                || checkpoint->main_stage_loop_graphics->next_instruction_address == 0x214aa)) {
            advanced = advance_deuteros_amiga_main_stage_scheduler_pass();
        } else if (checkpoint->main_stage_loop_graphics
            && checkpoint->main_stage_loop_graphics->pending_read_instruction == 0x216d0) {
            advanced = advance_deuteros_amiga_view_selection();
        } else if (checkpoint->main_stage_loop_graphics
            && checkpoint->main_stage_loop_graphics->next_instruction_address == 0x214ee) {
            advanced = advance_deuteros_amiga_command_palette();
        } else if (checkpoint->main_stage_loop_graphics
            && checkpoint->main_stage_loop_graphics->next_call_address == 0x2231e) {
            advanced = advance_deuteros_amiga_fade_buffers();
        } else {
            deterministic = false;
        }
        if (!deterministic) {
            result.awaiting_external_observation = true;
            result.stop_before_address = main_stage_stop_address(*checkpoint);
            return result;
        }
        if (!advanced.accepted) {
            result.accepted = false;
            result.error = advanced.error.empty()
                ? "Deuteros deterministic main-stage transition failed"
                : advanced.error;
            return result;
        }
        ++result.steps;
    }
    result.step_limit_reached = true;
    if (const auto checkpoint = deuteros_amiga_title_dependency_chain_checkpoint()) {
        result.stop_before_address = main_stage_stop_address(*checkpoint);
    }
    return result;
}

DeuterosAmigaSessionDriveResult
ReleaseRuntimeCoordinator::drive_deuteros_amiga_session(const std::uint32_t step_limit) {
    DeuterosAmigaSessionDriveResult result;
    if (step_limit == 0) {
        result.stop_reason = DeuterosAmigaSessionStopReason::failed;
        result.error = "Deuteros deterministic session drive requires a nonzero step limit";
        return result;
    }
    if (!active_ || !session_snapshot_ || !deuteros_amiga_) {
        result.stop_reason = DeuterosAmigaSessionStopReason::inactive;
        result.error = "Deuteros deterministic session drive requires an active session";
        return result;
    }
    result.accepted = true;
    if (session_snapshot_->kind
        == RuntimeSessionKind::deuteros_amiga_bootstrap_graphics_boundary) {
        if (!deuteros_amiga_bootstrap_graphics_boundary_
            || (deuteros_amiga_bootstrap_graphics_boundary_->stop_before_address != 0x13028
                && deuteros_amiga_bootstrap_graphics_boundary_->stop_before_address != 0x13034
                && deuteros_amiga_bootstrap_graphics_boundary_->stop_before_address != 0x13058
                && deuteros_amiga_bootstrap_graphics_boundary_->stop_before_address != 0x1306c
                && deuteros_amiga_bootstrap_graphics_boundary_->stop_before_address != 0x13154
                && deuteros_amiga_bootstrap_graphics_boundary_->stop_before_address != 0x13188)) {
            result.accepted = false;
            result.stop_reason = DeuterosAmigaSessionStopReason::failed;
            result.error = "Deuteros bootstrap service checkpoint is absent";
            return result;
        }
        result.steps = deuteros_amiga_bootstrap_graphics_boundary_->source_instruction_count;
        const auto& boundary = *deuteros_amiga_bootstrap_graphics_boundary_;
        if (boundary.bootstrap_makevport_prefix_receipt) {
            result.stop_reason = DeuterosAmigaSessionStopReason::native_translation_boundary;
            result.stop_before_address = boundary.bootstrap_makevport_resume_address;
            result.external_requirement.reset();
            return result;
        }
        result.stop_reason = DeuterosAmigaSessionStopReason::external_observation;
        result.stop_before_address = boundary.stop_before_address;
        if (result.stop_before_address == 0x13058) {
            result.external_requirement =
                "graphics.library vector -$168 at $13058, return $1305c";
        } else if (result.stop_before_address == 0x1306c) {
            result.external_requirement =
                "Eon-native paired-view continuation is available at $1306c";
        } else if (result.stop_before_address == 0x13154) {
            result.external_requirement =
                "graphics.library vector -$c6 at $13154, return $13158";
        } else if (result.stop_before_address == 0x13188) {
            result.external_requirement =
                "graphics.library vector -$d8 at $13188, return $1318c";
        } else {
            const auto call = result.stop_before_address;
            const auto return_address = deuteros_amiga_bootstrap_graphics_boundary_->stop_return_address;
            const auto d1 = deuteros_amiga_bootstrap_graphics_boundary_->stop_d1_value;
            std::ostringstream requirement;
            requirement << "Amiga Exec AvailMem at $" << std::hex << call
                << " (LVO -$d8), D1=$" << d1 << ", return $" << return_address;
            result.external_requirement = requirement.str();
        }
        return result;
    }
    try {
    for (; result.steps < step_limit; ++result.steps) {
        DeuterosAmigaTitleDependencyObservationResult advanced;
        bool deterministic = true;
        std::uint32_t external_stop_override = 0;
        if (session_snapshot_->kind == RuntimeSessionKind::deuteros_amiga_title_program_entry) {
            advanced = advance_deuteros_amiga_title_program_entry();
        } else if (session_snapshot_->kind == RuntimeSessionKind::deuteros_amiga_title_stage
            && deuteros_amiga_->title_stage_session()) {
            enum class Action { none, local_prefix, native_privilege, native_open_library,
                post_open, native_display_surface, native_display_initialize,
                native_input_handler, native_service_setup, native_trackdisk_read, controller_seed,
                first_dispatch, first_packet, first_decode, first_tail,
                second_dispatch, second_decode, service_prefix, nested_loop,
                selected_stream, descriptor_loop, repeated_nested, caller_indirect,
                tail_subroutine, profile_two };
            Action action = Action::none;
            const auto accepts = [&](auto operation) {
                auto pending = *deuteros_amiga_->title_stage_session();
                return static_cast<bool>(operation(pending));
            };
            if (accepts([](auto& p){return p.execute_local_prefix();})) action=Action::local_prefix;
            else if (accepts([](auto& p){return p.execute_native_privilege_transition();})) action=Action::native_privilege;
            else if (deuteros_amiga_->title_stage_session()->open_library_boundary()
                && deuteros_amiga_->title_stage_session()->open_library_boundary()->state
                    == DeuterosAmigaTitleOpenLibraryBoundaryState::awaiting_open_library_return)
                action=Action::native_open_library;
            else if (accepts([](auto& p){return p.advance_post_open_library_local_path();})) action=Action::post_open;
            else if (deuteros_amiga_->title_stage_session()->open_library_boundary()
                && deuteros_amiga_->title_stage_session()->open_library_boundary()->state
                    == DeuterosAmigaTitleOpenLibraryBoundaryState::awaiting_external_display_base_read)
                action=Action::native_display_surface;
            else if (deuteros_amiga_->title_stage_session()->open_library_boundary()
                && deuteros_amiga_->title_stage_session()->open_library_boundary()->native_display_surface_return
                && !deuteros_amiga_native_display_initialization_
                && deuteros_amiga_native_display_surface_
                && deuteros_amiga_native_display_surface_->allocation())
                action=Action::native_display_initialize;
            else if (deuteros_amiga_native_display_initialization_
                && !deuteros_amiga_native_input_handler_registration_)
                action=Action::native_input_handler;
            else if (deuteros_amiga_native_input_handler_registration_
                && !deuteros_amiga_title_native_service_setup_plan_)
                action=Action::native_service_setup;
            else if (accepts([](auto& p){return p.advance_controller_pointer_seed();})) action=Action::controller_seed;
            else if (accepts([](auto& p){return p.advance_post_command_first_dispatch();})) action=Action::first_dispatch;
            else if (accepts([](auto& p){return p.advance_post_command_first_dispatch_packet();})) action=Action::first_packet;
            else if (accepts([](auto& p){return p.advance_post_command_first_dispatch_decode();})) action=Action::first_decode;
            else if (accepts([](auto& p){return p.advance_post_command_first_dispatch_caller_tail();})) action=Action::first_tail;
            else if (accepts([](auto& p){return p.advance_post_command_second_dispatch();})) action=Action::second_dispatch;
            else if (accepts([](auto& p){return p.advance_post_command_second_dispatch_decode();})) action=Action::second_decode;
            else if (accepts([](auto& p){return p.advance_post_command_service_route_prefix();})) action=Action::service_prefix;
            else if (accepts([](auto& p){return p.advance_post_command_nested_loop();})) action=Action::nested_loop;
            else if (accepts([](auto& p){return p.advance_post_command_selected_stream();})) action=Action::selected_stream;
            else if (accepts([](auto& p){return p.advance_post_command_descriptor_loop();})) action=Action::descriptor_loop;
            else if (accepts([](auto& p){return p.advance_post_adjusted_repeated_nested_loop();})) action=Action::repeated_nested;
            else if (accepts([](auto& p){return p.advance_post_adjusted_caller_indirect();})) action=Action::caller_indirect;
            else if (accepts([](auto& p){return p.advance_title_tail_subroutine();})) action=Action::tail_subroutine;
            else if (accepts([](auto& p){return p.advance_title_profile_two_bootstrap();})) action=Action::profile_two;
            else if (deuteros_amiga_->next_native_title_load_doio_ordinal())
                action=Action::native_trackdisk_read;
            switch(action) {
            case Action::local_prefix: advanced=advance_deuteros_amiga_title_local_prefix(); break;
            case Action::native_privilege: advanced=advance_deuteros_amiga_title_native_privilege_transition(); break;
            case Action::native_open_library:
                advanced=provide_deuteros_amiga_title_native_graphics_library(); break;
            case Action::post_open: advanced=advance_deuteros_amiga_title_post_open_library_local_path(); break;
            case Action::native_display_surface:
                advanced=provide_deuteros_amiga_title_native_display_surface(); break;
            case Action::native_display_initialize:
                advanced=initialize_deuteros_amiga_title_native_display(); break;
            case Action::native_input_handler:
                advanced=register_deuteros_amiga_title_native_input_handler(); break;
            case Action::native_service_setup:
                advanced=advance_deuteros_amiga_title_native_service_setup();
                break;
            case Action::native_trackdisk_read: {
                const auto service = service_deuteros_amiga_title_trackdisk_read();
                advanced.accepted = service.accepted;
                advanced.error = service.error;
                break;
            }
            case Action::controller_seed: advanced=advance_deuteros_amiga_title_controller_pointer_seed(); break;
            case Action::first_dispatch: advanced=advance_deuteros_amiga_title_post_command_first_dispatch(); break;
            case Action::first_packet: advanced=advance_deuteros_amiga_title_post_command_first_dispatch_packet(); break;
            case Action::first_decode: advanced=advance_deuteros_amiga_title_post_command_first_dispatch_decode(); break;
            case Action::first_tail: advanced=advance_deuteros_amiga_title_post_command_first_dispatch_caller_tail(); break;
            case Action::second_dispatch: advanced=advance_deuteros_amiga_title_post_command_second_dispatch(); break;
            case Action::second_decode: advanced=advance_deuteros_amiga_title_post_command_second_dispatch_decode(); break;
            case Action::service_prefix: advanced=advance_deuteros_amiga_title_post_command_service_route_prefix(); break;
            case Action::nested_loop: advanced=advance_deuteros_amiga_title_post_command_nested_loop(); break;
            case Action::selected_stream: advanced=advance_deuteros_amiga_title_post_command_selected_stream(); break;
            case Action::descriptor_loop: advanced=advance_deuteros_amiga_title_post_command_descriptor_loop(); break;
            case Action::repeated_nested: advanced=advance_deuteros_amiga_title_post_adjusted_repeated_nested_loop(); break;
            case Action::caller_indirect: advanced=advance_deuteros_amiga_title_post_adjusted_caller_indirect(); break;
            case Action::tail_subroutine: advanced=advance_deuteros_amiga_title_tail_subroutine(); break;
            case Action::profile_two: advanced=advance_deuteros_amiga_title_profile_two_bootstrap(); break;
            case Action::none: {
                const auto main = drive_deuteros_amiga_main_stage(1);
                if (main.steps == 1 && main.accepted) continue;
                if (!main.accepted && !main.error.empty()) {
                    advanced.error = main.error;
                } else {
                    // The coarse title checkpoint is intentionally sparse at
                    // early main-stage service waits. Preserve the native
                    // driver's exact instruction boundary for the outward
                    // diagnostics instead of reporting an ambiguous zero.
                    external_stop_override = main.stop_before_address;
                    deterministic = false;
                }
                break;
            }}
        } else deterministic = false;
        if (!deterministic) {
            result.stop_reason = DeuterosAmigaSessionStopReason::external_observation;
            if (const auto checkpoint=deuteros_amiga_title_dependency_chain_checkpoint()) {
                result.stop_before_address=checkpoint->stop_before_address;
                result.native_service_setup_plan = checkpoint->native_service_setup_plan;
                result.controller_pointer_seed_plan = checkpoint->controller_pointer_seed_plan;
                result.observed_service_setup_plan_present =
                    checkpoint->service_setup_local_plan.has_value();
            }
            if (external_stop_override != 0) result.stop_before_address = external_stop_override;
            return result;
        }
        if (!advanced.accepted) {
            result.accepted=false;
            result.stop_reason=DeuterosAmigaSessionStopReason::failed;
            result.error=advanced.error;
            return result;
        }
    }
    result.stop_reason=DeuterosAmigaSessionStopReason::step_limit;
    if (const auto checkpoint=deuteros_amiga_title_dependency_chain_checkpoint()) {
        result.stop_before_address=checkpoint->stop_before_address;
        result.native_service_setup_plan = checkpoint->native_service_setup_plan;
        result.controller_pointer_seed_plan = checkpoint->controller_pointer_seed_plan;
        result.observed_service_setup_plan_present =
            checkpoint->service_setup_local_plan.has_value();
    }
    return result;
    } catch (const std::exception& exception) {
        result.accepted=false;
        result.stop_reason=DeuterosAmigaSessionStopReason::failed;
        result.error=exception.what();
        return result;
    }
}

std::optional<std::uint64_t>
ReleaseRuntimeCoordinator::deuteros_amiga_worker_scenario_generation() const {
    if (active_runtime_generation_ == 0 || !active_ || !active_media_ || !deuteros_amiga_
        || !session_snapshot_ || session_snapshot_->game != Game::deuteros
        || session_snapshot_->platform != Platform::amiga
        || session_snapshot_->release_sha256 != active_->release.sha256
        || active_->release.game != Game::deuteros
        || active_->release.platform != Platform::amiga) {
        return std::nullopt;
    }
    return active_runtime_generation_;
}

DeuterosAmigaWorkerScenarioResult
ReleaseRuntimeCoordinator::evaluate_deuteros_amiga_worker_scenario(
    const std::uint64_t generation, std::string scenario_label,
    const std::uint32_t memory_base_address,
    const std::span<const std::uint8_t> hypothetical_memory) const {
    DeuterosAmigaWorkerScenarioResult result;
    constexpr std::size_t maximum_scenario_bytes = 64U * 1024U;
    const auto current_generation = deuteros_amiga_worker_scenario_generation();
    if (!current_generation || generation != *current_generation) {
        result.error = "Deuteros worker scenario requires the current active runtime generation";
        return result;
    }
    if (scenario_label.empty() || scenario_label.size() > 80
        || !std::all_of(scenario_label.begin(), scenario_label.end(), [](const char value) {
            const auto c = static_cast<unsigned char>(value);
            return c >= 0x20U && c <= 0x7eU;
        })) {
        result.error = "Deuteros worker scenario requires a short printable hypothetical label";
        return result;
    }
    if (hypothetical_memory.empty() || hypothetical_memory.size() > maximum_scenario_bytes) {
        result.error = "Deuteros worker scenario memory is empty or exceeds its 64 KiB bound";
        return result;
    }
    if (static_cast<std::uint64_t>(memory_base_address) + hypothetical_memory.size()
        > (std::uint64_t{1} << 32U)) {
        result.error = "Deuteros worker scenario range exceeds the 32-bit address space";
        return result;
    }
    try {
        const auto worker = deuteros_amiga_->installed_interrupt_worker();
        const auto translated = evaluate_deuteros_amiga_installed_interrupt_worker(
            worker, memory_base_address, hypothetical_memory);
        result.accepted = true;
        result.generation = generation;
        result.scenario_label = std::move(scenario_label);
        result.worker_sha256 = worker.raw_sha256;
        result.memory_base_address = translated.memory_base_address;
        result.register_writes = translated.register_writes;
    } catch (const std::exception& exception) {
        result.error = exception.what();
    }
    return result;
}

DeuterosAmigaInterruptWorkerObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_interrupt_worker(
    DeuterosAmigaInterruptWorkerEntryObservation observation) {
    DeuterosAmigaInterruptWorkerObservationResult result;
    if (!active_ || !active_media_ || !deuteros_amiga_ || !session_snapshot_
        || !native_runtime_memory_ || session_snapshot_->game != Game::deuteros
        || session_snapshot_->platform != Platform::amiga
        || session_snapshot_->release_sha256 != active_->release.sha256
        || active_->release.game != Game::deuteros
        || active_->release.platform != Platform::amiga) {
        result.error = "Deuteros interrupt worker requires the active owned Amiga runtime";
        return result;
    }
    if (observation.generation != active_runtime_generation_
        || observation.trace_sequence == 0
        || observation.trace_sequence <= deuteros_amiga_interrupt_worker_last_sequence_
        || observation.instruction_address != 0x22816
        || (observation.stack_pointer & 1U) != 0) {
        result.error = "Deuteros interrupt worker entry identity or sequence is invalid";
        return result;
    }
    std::uint32_t observed_return_address = 0;
    for (const auto byte : observation.return_address_bytes) {
        observed_return_address = (observed_return_address << 8U) | byte;
    }
    if (observed_return_address != 0x224eeU) {
        result.error = "Deuteros interrupt worker entry lacks its observed nested-BSR return address";
        return result;
    }
    if (observation.stack_pointer > std::numeric_limits<std::uint32_t>::max() - 3U) {
        result.error = "Deuteros interrupt worker stack return slot overflows native memory";
        return result;
    }
    try {
        auto next_memory = *native_runtime_memory_;
        const auto read = [&next_memory](const std::uint32_t address) {
            return next_memory.read_byte(
                {NativeRuntimeAddressSpace::linear, std::nullopt, address});
        };
        for (std::size_t index = 0; index < observation.return_address_bytes.size(); ++index) {
            const auto owned = read(observation.stack_pointer + static_cast<std::uint32_t>(index));
            if (!owned || *owned != observation.return_address_bytes[index]) {
                result.error = "Deuteros interrupt worker return bytes differ from owned stack memory";
                return result;
            }
        }
        const auto vector = next_memory.read_linear_range(0x6c, 4);
        std::uint32_t vector_target = 0;
        if (vector) {
            for (const auto byte : *vector) {
                vector_target = (vector_target << 8U) | byte;
            }
        }
        if (!vector || vector_target != 0x224ccU) {
            result.error = "Deuteros interrupt vector does not point to the admitted $224cc handler";
            return result;
        }

        constexpr std::uint32_t stage_base = 0x20000;
        constexpr std::size_t stage_length = 0x4200;
        constexpr std::string_view worker_call_sha256 =
            "874cf9ebfc8984141de5733d5ad5615ccec7ebd1e60b51776b5b9da8c52c9074";
        const auto stage = deuteros_amiga_->bootstrap_profile_payload(
            stage_base, stage_length, 0x5800);
        if (stage.size() != stage_length
            || to_hex(sha256(stage.subspan(0x24cc, 0x1e)))
                != "67858c74d3f4e217fd0797f2415989c7a309d385479512ac7b57759a557b2663"
            || to_hex(sha256(stage.subspan(0x2816, 0x1d2)))
                != "661854d6976ab520b0398e2545003d3fe59692fc0de54f0f810f379cf25ccaf8"
            || to_hex(sha256(stage.subspan(0x24ea, 4))) != worker_call_sha256) {
            result.error = "Deuteros interrupt handler or worker bytes failed hash-bound admission";
            return result;
        }
        constexpr std::array<std::pair<std::size_t, std::size_t>, 3> resident_code_ranges{{
            {0x24cc, 0x1e}, {0x24ea, 4}, {0x2816, 0x1d2}}};
        for (const auto& [offset, length] : resident_code_ranges) {
            for (std::size_t index = 0; index < length; ++index) {
                const auto current = read(stage_base + static_cast<std::uint32_t>(offset + index));
                if (!current || *current != stage[offset + index]) {
                    result.error = "Deuteros interrupt code is not present in owned runtime memory";
                    return result;
                }
            }
        }

        const auto worker = deuteros_amiga_->installed_interrupt_worker();
        const auto translated = evaluate_deuteros_amiga_installed_interrupt_worker_sparse(
            worker, read);
        if (!translated.memory_writes.empty()) {
            NativeRuntimeEffectBatch batch{
                "deuteros-amiga-interrupt-worker-" + std::to_string(observation.generation)
                    + "-" + std::to_string(observation.trace_sequence), true, {}};
            batch.effects.reserve(translated.memory_writes.size());
            for (const auto& write : translated.memory_writes) {
                batch.effects.push_back({batch.effects.size() + 1,
                    {NativeRuntimeAddressSpace::linear, std::nullopt, write.address},
                    MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian,
                    write.value});
            }
            const auto applied = next_memory.apply(batch);
            if (!applied.accepted) {
                result.error = applied.error;
                return result;
            }
        }
        result.accepted = true;
        result.generation = observation.generation;
        result.trace_sequence = observation.trace_sequence;
        result.worker_sha256 = worker.raw_sha256;
        result.memory_byte_writes = translated.memory_writes.size();
        result.register_writes = translated.register_writes;
        *native_runtime_memory_ = std::move(next_memory);
        deuteros_amiga_interrupt_worker_last_sequence_ = observation.trace_sequence;
    } catch (const std::exception& exception) {
        result.error = exception.what();
    }
    return result;
}

DeuterosAmigaVertbObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_vertb_entry(
    const DeuterosAmigaVertbEntryObservation observation) {
    DeuterosAmigaVertbObservationResult result;
    if (!active_ || !active_media_ || !deuteros_amiga_ || !session_snapshot_
        || !native_runtime_memory_ || !deuteros_amiga_vertb_session_
        || session_snapshot_->game != Game::deuteros
        || session_snapshot_->platform != Platform::amiga
        || session_snapshot_->release_sha256 != active_->release.sha256
        || active_->release.game != Game::deuteros
        || active_->release.platform != Platform::amiga) {
        result.error = "Deuteros VERTB entry requires an admitted registered Amiga session";
        return result;
    }
    if (observation.generation != active_runtime_generation_
        || observation.trace_sequence == 0 || observation.instruction_address != 0x207ec) {
        result.error = "Deuteros VERTB entry identity or sequence is invalid";
        return result;
    }
    if (deuteros_amiga_->title_stage_session()
        && observation.trace_sequence
            <= deuteros_amiga_->title_stage_session()->main_stage_last_command_sequence()) {
        result.error = "Deuteros VERTB entry sequence must follow the latest title-stage command";
        return result;
    }
    try {
        auto pending = *deuteros_amiga_vertb_session_;
        auto memory = *native_runtime_memory_;
        const auto read = [&memory](const std::uint32_t address) {
            return memory.read_byte({NativeRuntimeAddressSpace::linear, std::nullopt, address});
        };
        const auto translated = pending.observe_invocation(observation.trace_sequence,
            observation.instruction_address, read);
        if (!translated.memory_writes.empty()) {
            NativeRuntimeEffectBatch batch{
                "deuteros-amiga-vertb-" + std::to_string(observation.generation)
                    + "-" + std::to_string(observation.trace_sequence), true, {}};
            batch.effects.reserve(translated.memory_writes.size());
            for (const auto& write : translated.memory_writes) {
                batch.effects.push_back({batch.effects.size() + 1,
                    {NativeRuntimeAddressSpace::linear, std::nullopt, write.address},
                    MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian,
                    write.value});
            }
            const auto applied = memory.apply(batch);
            if (!applied.accepted) {
                result.error = applied.error;
                return result;
            }
        }
        *native_runtime_memory_ = std::move(memory);
        *deuteros_amiga_vertb_session_ = std::move(pending);
        result.accepted = true;
        result.generation = observation.generation;
        result.trace_sequence = observation.trace_sequence;
        result.memory_byte_writes = translated.memory_writes.size();
        result.register_writes = translated.register_writes;
    } catch (const std::exception& exception) {
        result.error = exception.what();
    }
    return result;
}

ActiveNativeSessionDriveResult
ReleaseRuntimeCoordinator::drive_active_native_session(const std::uint32_t step_limit) {
    ActiveNativeSessionDriveResult result;
    if (!session_snapshot_) {
        result.error = "Active native session drive requires an admitted session";
        return result;
    }

    switch (session_snapshot_->kind) {
    case RuntimeSessionKind::millennium_dos_sound_driver_boundary:
    case RuntimeSessionKind::millennium_dos_title:
        result.driver = ActiveNativeSessionDriver::millennium_dos;
        result.millennium_dos = drive_millennium_dos_session(step_limit);
        return result;

    case RuntimeSessionKind::deuteros_amiga_title_stage:
    case RuntimeSessionKind::deuteros_amiga_title_program_entry:
    case RuntimeSessionKind::deuteros_amiga_bootstrap_graphics_boundary:
        result.driver = ActiveNativeSessionDriver::deuteros_amiga;
        result.deuteros_amiga = drive_deuteros_amiga_session(step_limit);
        if (session_snapshot_->kind
                == RuntimeSessionKind::deuteros_amiga_bootstrap_graphics_boundary
            && result.deuteros_amiga->accepted
            && result.deuteros_amiga->stop_reason
                == DeuterosAmigaSessionStopReason::external_observation
            && result.deuteros_amiga->stop_before_address == 0x13028) {
            const auto initial_steps = result.deuteros_amiga->steps;
            if (initial_steps < step_limit) {
                const auto memory_services =
                    advance_deuteros_amiga_bootstrap_eon_memory_services();
                if (!memory_services.accepted) {
                    result.error = memory_services.error.empty()
                        ? "Deuteros Eon memory profile could not initialize the bootstrap"
                        : memory_services.error;
                    result.deuteros_amiga->accepted = false;
                    result.deuteros_amiga->error = result.error;
                    return result;
                }
                const auto view_pair = advance_deuteros_amiga_bootstrap_view_pair_native();
                if (!view_pair.accepted) {
                    result.error = view_pair.error.empty()
                        ? "Deuteros Eon paired-view path could not initialize the display memory"
                        : view_pair.error;
                    result.deuteros_amiga->accepted = false;
                    result.deuteros_amiga->error = result.error;
                    return result;
                }
                auto continuation = drive_deuteros_amiga_session(step_limit - initial_steps);
                continuation.steps += initial_steps;
                result.deuteros_amiga = std::move(continuation);
            }
        }
        if (session_snapshot_->kind
                == RuntimeSessionKind::deuteros_amiga_bootstrap_graphics_boundary
            && result.deuteros_amiga && result.deuteros_amiga->accepted
            && result.deuteros_amiga->stop_reason
                == DeuterosAmigaSessionStopReason::external_observation
            && result.deuteros_amiga->stop_before_address == 0x13188U) {
            const auto boundary = deuteros_amiga_bootstrap_graphics_boundary_;
            if (!boundary || !boundary->bootstrap_makevport_prefix_receipt) {
                const auto translated = advance_deuteros_amiga_bootstrap_makevport_prefix();
                if (!translated.accepted) {
                    result.error = translated.error.empty()
                        ? "Deuteros native MakeVPort prefix failed at its admitted caller"
                        : translated.error;
                    result.deuteros_amiga->accepted = false;
                    result.deuteros_amiga->stop_reason = DeuterosAmigaSessionStopReason::failed;
                    result.deuteros_amiga->error = result.error;
                    return result;
                }
            }
            const auto translated_boundary = deuteros_amiga_bootstrap_graphics_boundary_;
            if (!translated_boundary || !translated_boundary->bootstrap_makevport_prefix_receipt) {
                result.error = "Deuteros MakeVPort prefix receipt was not retained";
                result.deuteros_amiga->accepted = false;
                result.deuteros_amiga->stop_reason = DeuterosAmigaSessionStopReason::failed;
                result.deuteros_amiga->error = result.error;
                return result;
            }
            result.deuteros_amiga->stop_reason = DeuterosAmigaSessionStopReason::native_translation_boundary;
            result.deuteros_amiga->stop_before_address =
                translated_boundary->bootstrap_makevport_resume_address;
            result.deuteros_amiga->external_requirement.reset();
        }
        return result;

    case RuntimeSessionKind::millennium_atari_bootstrap:
        result.driver = ActiveNativeSessionDriver::millennium_atari;
        result.millennium_atari = drive_millennium_atari_session(step_limit);
        return result;

    default:
        result.error = "Active native session has no deterministic driver: "
            + std::string(runtime_session_kind_label(session_snapshot_->kind));
        return result;
    }
}

MillenniumAtariSessionDriveResult
ReleaseRuntimeCoordinator::drive_millennium_atari_session(const std::uint32_t step_limit) {
    MillenniumAtariSessionDriveResult result;
    if (step_limit == 0) {
        result.error = "Millennium Atari session drive requires a nonzero step limit";
        return result;
    }
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_config_consumer_ || !native_runtime_memory_) {
        result.error = "Millennium Atari session drive requires an active config consumer";
        return result;
    }

    // Drive only bounded services explicitly owned by Eon's native TOS
    // compatibility layer. Unsupported guest observations and Line-A drawing
    // operations remain hard boundaries; this is not a CPU emulator.
    for (; result.steps < step_limit;) {
        auto next = *millennium_atari_config_consumer_;
        // Effect batches apply atomically inside NativeRuntimeMemory. Avoid
        // cloning its full address map for every bounded guest transition.
        auto& memory = *native_runtime_memory_;
        const auto checkpoint = next.checkpoint();
        const auto stop_address = [&] {
            switch (checkpoint.state) {
            case MillenniumAtariConfigConsumerState::xbios_trap_boundary:
            case MillenniumAtariConfigConsumerState::xbios_selector_three_boundary:
            case MillenniumAtariConfigConsumerState::xbios_selector_four_boundary:
            case MillenniumAtariConfigConsumerState::xbios_selector_21_boundary:
            case MillenniumAtariConfigConsumerState::xbios_selector_6_boundary:
            case MillenniumAtariConfigConsumerState::xbios_selector_38_boundary:
                return checkpoint.xbios_trap_address;
            case MillenniumAtariConfigConsumerState::gemdos_selector_61_boundary:
                return checkpoint.gemdos_trap_address;
            case MillenniumAtariConfigConsumerState::gemdos_selector_63_boundary:
                return checkpoint.gemdos_63_trap_address;
            case MillenniumAtariConfigConsumerState::gemdos_selector_62_boundary:
                return checkpoint.gemdos_62_trap_address;
            case MillenniumAtariConfigConsumerState::jsr_2a5c2_boundary:
                return checkpoint.next_jsr_target;
            case MillenniumAtariConfigConsumerState::fread_prefix_boundary:
                return checkpoint.boundary_instruction_address;
            case MillenniumAtariConfigConsumerState::jsr_2b2be_boundary:
            case MillenniumAtariConfigConsumerState::jsr_2aa68_boundary:
            case MillenniumAtariConfigConsumerState::jsr_2aa0c_boundary:
                return checkpoint.next_jsr_target;
            case MillenniumAtariConfigConsumerState::game_init_source_byte_boundary:
            case MillenniumAtariConfigConsumerState::game_init_zero_copy_boundary:
            case MillenniumAtariConfigConsumerState::game_init_zero_counter_branch_boundary:
            case MillenniumAtariConfigConsumerState::game_init_bit6_clear_boundary:
            case MillenniumAtariConfigConsumerState::game_init_bit7_set_boundary:
            case MillenniumAtariConfigConsumerState::game_init_second_source_boundary:
                return checkpoint.game_init_next_instruction;
            case MillenniumAtariConfigConsumerState::game_init_jsr_2b448_boundary:
                return checkpoint.next_jsr_target;
            case MillenniumAtariConfigConsumerState::game_init_palette_transform_boundary:
                return checkpoint.game_init_next_instruction;
            case MillenniumAtariConfigConsumerState::game_init_palette_xbios_selector_6_boundary:
            case MillenniumAtariConfigConsumerState::game_init_palette_outer_recurrence_boundary:
            case MillenniumAtariConfigConsumerState::game_init_palette_terminal_xbios_selector_6_boundary:
            case MillenniumAtariConfigConsumerState::game_init_palette_rts_boundary:
                return checkpoint.game_init_next_instruction;
            case MillenniumAtariConfigConsumerState::game_init_second_config_rts_boundary:
                return checkpoint.game_init_second_config_rts_address;
            case MillenniumAtariConfigConsumerState::game_init_post_second_config_xbios_38_boundary:
                return checkpoint.xbios_trap_address;
            case MillenniumAtariConfigConsumerState::game_init_config_final_rts_boundary:
                return checkpoint.game_init_config_final_rts_address;
            case MillenniumAtariConfigConsumerState::line_a_init_boundary:
                return checkpoint.line_a_init_address;
            case MillenniumAtariConfigConsumerState::jsr_2b55a_boundary:
                return checkpoint.next_jsr_target;
            case MillenniumAtariConfigConsumerState::bsr_2b59a_boundary:
                return checkpoint.bsr_target;
            case MillenniumAtariConfigConsumerState::d0_indexed_write_boundary:
                return checkpoint.indexed_instruction_address;
            case MillenniumAtariConfigConsumerState::a1_setup_boundary:
                return checkpoint.indexed_instruction_address;
            case MillenniumAtariConfigConsumerState::d0_indexed_word_boundary:
                return checkpoint.indexed_word_instruction_address;
            case MillenniumAtariConfigConsumerState::a0_indexed_word_boundary:
                return checkpoint.a0_indexed_instruction_address;
            case MillenniumAtariConfigConsumerState::loop_branch_boundary:
                return checkpoint.loop_branch_target;
            case MillenniumAtariConfigConsumerState::movem_restore_boundary:
                return checkpoint.movem_instruction_address;
            default:
                return checkpoint.boundary_instruction_address;
            }
        };
        const auto sequence = checkpoint.last_sequence + 1U;
        MillenniumAtariConfigConsumerResult serviced;
        try {
            switch (checkpoint.state) {
            case MillenniumAtariConfigConsumerState::status_register_boundary:
                serviced = next.service_native_tos_status_register(sequence);
                break;
            case MillenniumAtariConfigConsumerState::xbios_trap_boundary:
                serviced = next.service_native_tos_xbios_selector_two(sequence);
                if (serviced.accepted) {
                    const auto applied = memory.apply(next.make_selector_two_result_effect_batch(
                        "eon-millennium-atari-tos-physbase-" + std::to_string(sequence)));
                    if (!applied.accepted) serviced = {false, applied.error};
                }
                break;
            case MillenniumAtariConfigConsumerState::xbios_selector_three_boundary:
                serviced = next.service_native_tos_xbios_selector_three(sequence);
                if (serviced.accepted) {
                    const auto applied = memory.apply(next.make_selector_three_result_effect_batch(
                        "eon-millennium-atari-tos-logbase-" + std::to_string(sequence)));
                    if (!applied.accepted) serviced = {false, applied.error};
                }
                break;
            case MillenniumAtariConfigConsumerState::xbios_selector_four_boundary:
                serviced = next.service_native_tos_xbios_selector_four(sequence);
                if (serviced.accepted) {
                    const auto applied = memory.apply(next.make_selector_four_result_effect_batch(
                        "eon-millennium-atari-tos-getrez-" + std::to_string(sequence)));
                    if (!applied.accepted) serviced = {false, applied.error};
                }
                break;
            case MillenniumAtariConfigConsumerState::line_a_init_boundary: {
                constexpr std::uint32_t workspace_base = 0x00038000U;
                constexpr std::size_t workspace_size = 0x1000U;
                constexpr std::uint32_t contrl_address = workspace_base + 0x100U;
                constexpr std::uint32_t intin_address = workspace_base + 0x200U;
                constexpr std::uint32_t ptsin_address = workspace_base + 0x400U;
                constexpr std::uint32_t intout_address = workspace_base + 0x800U;
                constexpr std::uint32_t ptsout_address = workspace_base + 0xa00U;
                for (std::size_t index = 0; index < workspace_size; ++index) {
                    if (memory.read_byte({NativeRuntimeAddressSpace::linear, std::nullopt,
                            workspace_base + static_cast<std::uint32_t>(index)})) {
                        serviced = {false, "Eon Line-A workspace overlaps initialized native memory"};
                        break;
                    }
                }
                if (!serviced.error.empty()) break;

                std::vector<std::uint8_t> workspace(workspace_size, 0);
                const auto put_word = [&workspace](const std::size_t offset,
                    const std::uint16_t value) {
                    workspace[offset] = static_cast<std::uint8_t>(value >> 8U);
                    workspace[offset + 1U] = static_cast<std::uint8_t>(value);
                };
                const auto put_long = [&workspace](const std::size_t offset,
                    const std::uint32_t value) {
                    workspace[offset] = static_cast<std::uint8_t>(value >> 24U);
                    workspace[offset + 1U] = static_cast<std::uint8_t>(value >> 16U);
                    workspace[offset + 2U] = static_cast<std::uint8_t>(value >> 8U);
                    workspace[offset + 3U] = static_cast<std::uint8_t>(value);
                };
                // TOS low resolution has four planes and a 160-byte scanline.
                // The Line-A prefix consumed by this exact PRG reads INTIN and
                // PTSIN from +8/+12 of the standard parameter block.
                put_word(0, 4);
                put_word(2, 160);
                put_long(4, contrl_address);
                put_long(8, intin_address);
                put_long(12, ptsin_address);
                put_long(16, intout_address);
                put_long(20, ptsout_address);
                NativeRuntimeEffectBatch workspace_batch{
                    "eon-millennium-atari-line-a-workspace-" + std::to_string(sequence),
                    true, {}};
                workspace_batch.effects.reserve(workspace.size() + 2U);
                for (std::size_t index = 0; index < workspace.size(); ++index) {
                    workspace_batch.effects.push_back({index + 1U,
                        {NativeRuntimeAddressSpace::linear, std::nullopt,
                            workspace_base + static_cast<std::uint32_t>(index)},
                        MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian,
                        workspace[index]});
                }
                serviced = next.service_native_tos_line_a(sequence,
                    workspace_base, intin_address, ptsin_address);
                if (serviced.accepted) {
                    auto result_batch = next.make_line_a_result_effect_batch(
                        "eon-millennium-atari-line-a-results-" + std::to_string(sequence));
                    for (const auto& effect : result_batch.effects) {
                        auto appended = effect;
                        appended.order = workspace_batch.effects.size() + 1U;
                        workspace_batch.effects.push_back(std::move(appended));
                    }
                    const auto applied = memory.apply(workspace_batch);
                    if (!applied.accepted) serviced = {false, applied.error};
                }
                break;
            }
            case MillenniumAtariConfigConsumerState::xbios_selector_6_boundary: {
                const auto bytes = memory.read_linear_range(
                    checkpoint.selector_6_pointer_argument, 32U);
                if (!bytes || bytes->size() != 32U) {
                    serviced = {false, "Atari Setpalette source is unavailable in native memory"};
                    break;
                }
                std::array<std::uint16_t, 16> palette_words{};
                for (std::size_t index = 0; index < palette_words.size(); ++index) {
                    palette_words[index] = static_cast<std::uint16_t>(
                        (static_cast<std::uint16_t>((*bytes)[index * 2U]) << 8U)
                        | (*bytes)[index * 2U + 1U]);
                }
                serviced = next.service_native_tos_xbios_selector_6(sequence, palette_words);
                break;
            }
            case MillenniumAtariConfigConsumerState::xbios_selector_21_boundary:
                serviced = next.service_native_tos_xbios_selector_21(sequence);
                break;
            case MillenniumAtariConfigConsumerState::movem_restore_boundary:
                serviced = next.execute_movem_restore_compatibility();
                break;
            case MillenniumAtariConfigConsumerState::jsr_2aa68_boundary:
                serviced = next.execute_jsr_2aa68();
                break;
            case MillenniumAtariConfigConsumerState::xbios_selector_38_boundary: {
                auto staged_memory = memory;
                const auto environment = next.make_native_tos_vbl_environment_effect_batch(
                    "eon-millennium-atari-tos-vbl-environment-" + std::to_string(sequence),
                    staged_memory);
                const auto environment_applied = staged_memory.apply(environment);
                if (!environment_applied.accepted) {
                    serviced = {false, environment_applied.error};
                    break;
                }
                serviced = next.service_native_tos_xbios_selector_38(sequence, staged_memory);
                if (serviced.accepted) {
                    const auto callback_applied = staged_memory.apply(next.make_selector_38_effect_batch(
                        "eon-millennium-atari-tos-supexec-callback-" + std::to_string(sequence)));
                    if (!callback_applied.accepted) serviced = {false, callback_applied.error};
                    else memory = std::move(staged_memory);
                }
                break;
            }
            case MillenniumAtariConfigConsumerState::jsr_2aa0c_boundary:
                serviced = next.execute_jsr_2aa0c();
                break;
            case MillenniumAtariConfigConsumerState::gemdos_selector_61_boundary:
                serviced = next.service_native_tos_gemdos_selector_61(sequence, memory);
                if (serviced.accepted) {
                    const auto applied = memory.apply(next.make_gemdos_selector_61_effect_batch(
                        "eon-millennium-atari-mill22c-fopen-" + std::to_string(sequence)));
                    if (!applied.accepted) serviced = {false, applied.error};
                }
                break;
            case MillenniumAtariConfigConsumerState::jsr_2a5c2_boundary:
                serviced = next.execute_jsr_2a5c2();
                break;
            case MillenniumAtariConfigConsumerState::gemdos_selector_63_boundary: {
                if (checkpoint.game_init_second_config_open) {
                    if (checkpoint.gemdos_63_handle
                            != checkpoint.game_init_second_config_handle
                        || checkpoint.gemdos_63_buffer != 0x2c24aU
                        || checkpoint.gemdos_63_count != 0x7d42U
                        || checkpoint.gemdos_filename_pointer != 0x2a634U) {
                        serviced = {false,
                            "Atari second-config Fread differs from the admitted MILL22D.INF request"};
                        break;
                    }
                    constexpr std::array<std::uint8_t, 12> filename{
                        'M', 'I', 'L', 'L', '2', '2', 'D', '.', 'I', 'N', 'F', 0,
                    };
                    const auto source_name = memory.read_linear_range(0x2a634U,
                        filename.size());
                    if (!source_name || !std::equal(filename.begin(), filename.end(),
                            source_name->begin())) {
                        serviced = {false,
                            "Atari second-config Fread filename changed after native Fopen"};
                        break;
                    }
                    const auto payload = millennium_atari_->
                        make_game_init_second_config_fread_service_effect_batch(
                            "eon-millennium-atari-mill22d-fread-" + std::to_string(sequence));
                    serviced = next.service_game_init_second_config_fread_result(
                        static_cast<std::uint32_t>(payload.effects.size()));
                    if (serviced.accepted) {
                        const auto applied = memory.apply(payload);
                        if (!applied.accepted) serviced = {false, applied.error};
                    }
                    break;
                }
                constexpr std::uint32_t expected_size = 9597U;
                if (checkpoint.gemdos_63_handle != 6U || checkpoint.gemdos_63_buffer != 0x2c24aU
                    || checkpoint.gemdos_63_count != 0x7d42U
                    || checkpoint.gemdos_filename_pointer != 0x2a640U) {
                    serviced = {false, "Atari Fread does not match the admitted MILL22C.INF request"};
                    break;
                }
                constexpr std::array<std::uint8_t, 12> filename{
                    'M', 'I', 'L', 'L', '2', '2', 'C', '.', 'I', 'N', 'F', 0,
                };
                const auto source_name = memory.read_linear_range(0x2a640U, filename.size());
                if (!source_name || !std::equal(filename.begin(), filename.end(), source_name->begin())) {
                    serviced = {false, "Atari Fread filename changed after native Fopen"};
                    break;
                }
                serviced = next.observe_gemdos_selector_63({checkpoint.generation, sequence,
                    checkpoint.gemdos_63_trap_address, checkpoint.gemdos_63_selector,
                    static_cast<std::int32_t>(expected_size)});
                if (serviced.accepted) {
                    const auto payload = millennium_atari_->make_game_init_decoder_fread_effect_batch(
                        static_cast<std::int32_t>(expected_size),
                        "eon-millennium-atari-mill22c-fread-" + std::to_string(sequence));
                    const auto applied = memory.apply(payload);
                    if (!applied.accepted) serviced = {false, applied.error};
                }
                break;
            }
            case MillenniumAtariConfigConsumerState::gemdos_selector_62_boundary:
                if (checkpoint.game_init_second_config_open) {
                    if (checkpoint.gemdos_62_handle
                        != checkpoint.game_init_second_config_handle) {
                        serviced = {false,
                            "Atari second-config Fclose handle differs from native MILL22D.INF Fopen"};
                        break;
                    }
                    serviced = next.service_game_init_second_config_fclose_result();
                    break;
                }
                if (checkpoint.gemdos_62_handle != 6U) {
                    serviced = {false, "Atari Fclose handle does not match native MILL22C.INF Fopen"};
                    break;
                }
                serviced = next.observe_gemdos_selector_62({checkpoint.generation, sequence,
                    checkpoint.gemdos_62_trap_address, checkpoint.gemdos_62_selector, 0});
                break;
            case MillenniumAtariConfigConsumerState::fread_prefix_boundary: {
                const auto read_word = [&memory](const std::uint32_t address)
                    -> std::optional<std::uint16_t> {
                    const auto bytes = memory.read_linear_range(address, 2U);
                    if (!bytes || bytes->size() != 2U) return std::nullopt;
                    return static_cast<std::uint16_t>((static_cast<std::uint16_t>((*bytes)[0]) << 8U)
                        | (*bytes)[1]);
                };
                const auto first = read_word(0x2c24cU);
                const auto second = read_word(0x2c24eU);
                if (!first || !second) {
                    serviced = {false, "Atari decoder Fread prefix is absent from native memory"};
                    break;
                }
                serviced = next.observe_fread_prefix({checkpoint.generation, sequence,
                    0x2c24cU, *first, 0x2c24eU, *second});
                if (serviced.accepted) {
                    const auto applied = memory.apply(next.make_fread_prefix_effect_batch(
                        "eon-millennium-atari-mill22c-prefix-" + std::to_string(sequence)));
                    if (!applied.accepted) serviced = {false, applied.error};
                }
                break;
            }
            case MillenniumAtariConfigConsumerState::jsr_2b2be_boundary:
                serviced = next.execute_jsr_2b2be();
                if (serviced.accepted) {
                    const auto applied = memory.apply(next.make_game_init_setup_effect_batch(
                        "eon-millennium-atari-game-init-setup-" + std::to_string(sequence)));
                    if (!applied.accepted) serviced = {false, applied.error};
                }
                break;
            case MillenniumAtariConfigConsumerState::game_init_source_byte_boundary: {
                const auto source = memory.read_byte({NativeRuntimeAddressSpace::linear,
                    std::nullopt, checkpoint.game_init_source_address});
                if (!source) {
                    serviced = {false, "Atari game-init source byte is absent from native memory"};
                    break;
                }
                serviced = next.observe_game_init_source_byte({checkpoint.generation, sequence,
                    checkpoint.game_init_next_instruction, checkpoint.game_init_source_address, *source});
                break;
            }
            case MillenniumAtariConfigConsumerState::game_init_zero_copy_boundary: {
                const auto first = memory.read_byte({NativeRuntimeAddressSpace::linear,
                    std::nullopt, checkpoint.game_init_source_address});
                const auto second = memory.read_byte({NativeRuntimeAddressSpace::linear,
                    std::nullopt, checkpoint.game_init_source_address + 1U});
                if (!first || !second) {
                    serviced = {false, "Atari game-init literal pair is absent from native memory"};
                    break;
                }
                serviced = next.observe_game_init_zero_pair({checkpoint.generation, sequence,
                    checkpoint.game_init_next_instruction, checkpoint.game_init_source_address,
                    *first, checkpoint.game_init_source_address + 1U, *second});
                if (serviced.accepted) {
                    const auto applied = memory.apply(next.make_game_init_zero_pair_effect_batch(
                        "eon-millennium-atari-game-init-zero-pair-" + std::to_string(sequence)));
                    if (!applied.accepted) serviced = {false, applied.error};
                }
                break;
            }
            case MillenniumAtariConfigConsumerState::game_init_zero_counter_branch_boundary:
                serviced = next.execute_game_init_zero_counter_branch();
                break;
            case MillenniumAtariConfigConsumerState::game_init_second_source_boundary: {
                const auto source = memory.read_byte({NativeRuntimeAddressSpace::linear,
                    std::nullopt, checkpoint.game_init_source_address});
                if (!source) {
                    serviced = {false, "Atari replicated game-init byte is absent from native memory"};
                    break;
                }
                serviced = next.observe_game_init_replicated_byte({checkpoint.generation,
                    sequence, checkpoint.game_init_next_instruction,
                    checkpoint.game_init_source_address, *source});
                if (serviced.accepted) {
                    const auto applied = memory.apply(next.make_game_init_alternate_effect_batch(
                        "eon-millennium-atari-game-init-repeat-" + std::to_string(sequence)));
                    if (!applied.accepted) serviced = {false, applied.error};
                }
                break;
            }
            case MillenniumAtariConfigConsumerState::game_init_bit7_set_boundary: {
                const auto first = memory.read_byte({NativeRuntimeAddressSpace::linear,
                    std::nullopt, checkpoint.game_init_source_address});
                const auto second = memory.read_byte({NativeRuntimeAddressSpace::linear,
                    std::nullopt, checkpoint.game_init_source_address + 1U});
                if (!first || !second) {
                    serviced = {false, "Atari swapped game-init pair is absent from native memory"};
                    break;
                }
                serviced = next.observe_game_init_swapped_pair({checkpoint.generation, sequence,
                    0x2b37aU, checkpoint.game_init_source_address,
                    *first, checkpoint.game_init_source_address + 1U, *second});
                if (serviced.accepted) {
                    const auto applied = memory.apply(next.make_game_init_alternate_effect_batch(
                        "eon-millennium-atari-game-init-swap-" + std::to_string(sequence)));
                    if (!applied.accepted) serviced = {false, applied.error};
                }
                break;
            }
            case MillenniumAtariConfigConsumerState::game_init_bit6_clear_boundary: {
                const auto count = memory.read_byte({NativeRuntimeAddressSpace::linear,
                    std::nullopt, checkpoint.game_init_source_address});
                const auto first = memory.read_byte({NativeRuntimeAddressSpace::linear,
                    std::nullopt, checkpoint.game_init_source_address + 1U});
                const auto second = memory.read_byte({NativeRuntimeAddressSpace::linear,
                    std::nullopt, checkpoint.game_init_source_address + 2U});
                if (!count || !first || !second) {
                    serviced = {false, "Atari extended game-init run is absent from native memory"};
                    break;
                }
                serviced = next.observe_game_init_extended_run({checkpoint.generation,
                    sequence, 0x2b3c0U,
                    checkpoint.game_init_source_address, *count,
                    checkpoint.game_init_source_address + 1U, *first,
                    checkpoint.game_init_source_address + 2U, *second});
                if (serviced.accepted) {
                    const auto applied = memory.apply(next.make_game_init_alternate_effect_batch(
                        "eon-millennium-atari-game-init-extended-" + std::to_string(sequence)));
                    if (!applied.accepted) serviced = {false, applied.error};
                }
                break;
            }
            case MillenniumAtariConfigConsumerState::game_init_complete:
                // The recovered caller's RTS and the following JSR are fully
                // source-bound local work. Keep the state transition in the
                // active driver so it does not stop one instruction early.
                serviced = next.execute_game_init_return();
                break;
            case MillenniumAtariConfigConsumerState::game_init_jsr_2b448_boundary:
                serviced = next.execute_game_init_palette_copy_prefix();
                if (serviced.accepted) {
                    const auto applied = memory.apply(
                        next.make_game_init_palette_copy_effect_batch(
                            "eon-millennium-atari-game-init-palette-copy-"
                                + std::to_string(sequence)));
                    if (!applied.accepted) serviced = {false, applied.error};
                }
                break;
            case MillenniumAtariConfigConsumerState::game_init_palette_transform_boundary: {
                MillenniumAtariGameInitPaletteWordsObservation observation{
                    checkpoint.generation, sequence,
                    checkpoint.game_init_next_instruction, 0x2b3c8U, 0x2b428U, {}};
                for (std::size_t index = 0; index < observation.destination_words.size(); ++index) {
                    const auto address = observation.destination_address
                        + static_cast<std::uint32_t>(index * 2U);
                    const auto bytes = memory.read_linear_range(address, 2U);
                    if (!bytes || bytes->size() != 2U) {
                        serviced = {false, "Atari palette transform destination is absent from native memory"};
                        break;
                    }
                    observation.destination_words[index] = static_cast<std::uint16_t>(
                        (static_cast<std::uint16_t>((*bytes)[0]) << 8U) | (*bytes)[1]);
                }
                if (!serviced.error.empty()) break;
                serviced = next.observe_game_init_palette_words(observation);
                if (serviced.accepted) {
                    const auto applied = memory.apply(
                        next.make_game_init_palette_arithmetic_effect_batch(
                            "eon-millennium-atari-game-init-palette-transform-"
                                + std::to_string(sequence)));
                    if (!applied.accepted) serviced = {false, applied.error};
                }
                break;
            }
            case MillenniumAtariConfigConsumerState::game_init_palette_xbios_selector_6_boundary:
            case MillenniumAtariConfigConsumerState::game_init_palette_terminal_xbios_selector_6_boundary: {
                std::array<std::uint16_t,16> palette_words{};
                for (std::size_t index = 0; index < palette_words.size(); ++index) {
                    const auto bytes = memory.read_linear_range(0x2b428U
                        + static_cast<std::uint32_t>(index * 2U), 2U);
                    if (!bytes || bytes->size() != 2U) {
                        serviced = {false, "Atari Setpalette words are absent from native memory"};
                        break;
                    }
                    palette_words[index] = static_cast<std::uint16_t>(
                        (static_cast<std::uint16_t>((*bytes)[0]) << 8U) | (*bytes)[1]);
                }
                if (!serviced.error.empty()) break;
                serviced = next.service_native_tos_game_init_palette_selector_6(
                    sequence, palette_words);
                break;
            }
            case MillenniumAtariConfigConsumerState::game_init_palette_rts_boundary:
                serviced = next.execute_game_init_palette_rts_compatibility();
                break;
            case MillenniumAtariConfigConsumerState::game_init_second_config_rts_boundary:
                serviced = next.execute_game_init_second_config_rts();
                break;
            case MillenniumAtariConfigConsumerState::game_init_palette_outer_recurrence_boundary: {
                MillenniumAtariGameInitPaletteRecurrenceObservation observation{
                    checkpoint.generation, sequence, checkpoint.game_init_next_instruction,
                    0x2b3c8U, 0x2b428U, {}, {}};
                const auto source = memory.read_linear_range(observation.source_address,
                    observation.source_bytes.size());
                if (!source || source->size() != observation.source_bytes.size()) {
                    serviced = {false, "Atari palette recurrence source is absent from native memory"};
                    break;
                }
                std::copy(source->begin(), source->end(), observation.source_bytes.begin());
                for (std::size_t index = 0; index < observation.destination_words.size(); ++index) {
                    const auto bytes = memory.read_linear_range(observation.destination_address
                        + static_cast<std::uint32_t>(index * 2U), 2U);
                    if (!bytes || bytes->size() != 2U) {
                        serviced = {false, "Atari palette recurrence words are absent from native memory"};
                        break;
                    }
                    observation.destination_words[index] = static_cast<std::uint16_t>(
                        (static_cast<std::uint16_t>((*bytes)[0]) << 8U) | (*bytes)[1]);
                }
                if (!serviced.error.empty()) break;
                serviced = next.observe_game_init_palette_recurrence(observation);
                if (serviced.accepted) {
                    const auto applied = memory.apply(
                        next.make_game_init_palette_arithmetic_effect_batch(
                            "eon-millennium-atari-game-init-palette-recurrence-"
                                + std::to_string(sequence)));
                    if (!applied.accepted) serviced = {false, applied.error};
                }
                break;
            }
            case MillenniumAtariConfigConsumerState::jsr_2b55a_boundary:
                serviced = next.execute_jsr_2b55a();
                break;
            case MillenniumAtariConfigConsumerState::bsr_2b59a_boundary:
                serviced = next.execute_bsr_2b59a();
                if (serviced.accepted) {
                    const auto applied = memory.apply(next.make_bsr_2b59a_effect_batch(
                        "eon-millennium-atari-palette-bsr-clear-" + std::to_string(sequence)));
                    if (!applied.accepted) serviced = {false, applied.error};
                }
                break;
            case MillenniumAtariConfigConsumerState::d0_indexed_write_boundary: {
                const auto first = memory.read_byte({NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x2bdfdU});
                const auto second = memory.read_byte({NativeRuntimeAddressSpace::linear,
                    std::nullopt, 0x2b6aeU});
                if (!first || !second) {
                    serviced = {false, "Atari indexed byte source is unavailable in native memory"};
                    break;
                }
                serviced = next.observe_d0_indexed_byte({checkpoint.generation, sequence,
                    0x2b5a6U, 0U, 0x2bdfdU, *first, 0x2b6aeU, *second});
                if (serviced.accepted) {
                    const auto applied = memory.apply(next.make_d0_indexed_effect_batch(
                        "eon-millennium-atari-config-indexed-bytes-" + std::to_string(sequence)));
                    if (!applied.accepted) serviced = {false, applied.error};
                }
                break;
            }
            case MillenniumAtariConfigConsumerState::a1_setup_boundary:
                serviced = next.execute_a1_setup();
                if (serviced.accepted) {
                    const auto applied = memory.apply(next.make_a1_setup_effect_batch(
                        "eon-millennium-atari-config-a1-setup-" + std::to_string(sequence)));
                    if (!applied.accepted) serviced = {false, applied.error};
                }
                break;
            case MillenniumAtariConfigConsumerState::d0_indexed_word_boundary: {
                const auto word_address = static_cast<std::uint32_t>(
                    0x2bdfeLL + static_cast<std::int16_t>(checkpoint.loop_d0_value));
                const auto bytes = memory.read_linear_range(word_address, 2U);
                if (!bytes || bytes->size() != 2U) {
                    serviced = {false, "Atari indexed word source is unavailable in native memory"};
                    break;
                }
                const auto value = static_cast<std::uint16_t>(((*bytes)[0] << 8U) | (*bytes)[1]);
                serviced = next.observe_d0_indexed_word({checkpoint.generation, sequence,
                    0x2b5deU, checkpoint.loop_d0_value, word_address, value});
                if (serviced.accepted) {
                    const auto applied = memory.apply(next.make_d0_indexed_word_effect_batch(
                        "eon-millennium-atari-config-indexed-word-" + std::to_string(sequence)));
                    if (!applied.accepted) serviced = {false, applied.error};
                }
                break;
            }
            case MillenniumAtariConfigConsumerState::a0_indexed_word_boundary: {
                const auto address = static_cast<std::uint32_t>(
                    static_cast<std::int64_t>(checkpoint.callee_a3)
                    + static_cast<std::int16_t>(checkpoint.indexed_word_value));
                const auto bytes = memory.read_linear_range(address, 2U);
                if (!bytes || bytes->size() != 2U) {
                    serviced = {false, "Atari A0-indexed word source is unavailable in native memory"};
                    break;
                }
                const auto value = static_cast<std::uint16_t>(((*bytes)[0] << 8U) | (*bytes)[1]);
                serviced = next.observe_a0_indexed_word({checkpoint.generation, sequence,
                    checkpoint.a0_indexed_instruction_address, 0U, address, value});
                if (serviced.accepted) {
                    const auto applied = memory.apply(next.make_a0_indexed_tail_effect_batch(
                        "eon-millennium-atari-config-a0-tail-" + std::to_string(sequence)));
                    if (!applied.accepted) serviced = {false, applied.error};
                }
                break;
            }
            case MillenniumAtariConfigConsumerState::loop_branch_boundary:
                if (checkpoint.loop_branch_target == 0x2b5b8U) {
                    serviced = next.execute_loop_iteration_setup();
                    if (serviced.accepted) {
                        const auto applied = memory.apply(next.make_loop_iteration_setup_effect_batch(
                            "eon-millennium-atari-config-loop-setup-" + std::to_string(sequence)));
                        if (!applied.accepted) serviced = {false, applied.error};
                    }
                } else {
                    serviced = next.execute_loop_epilogue();
                    if (serviced.accepted) {
                        const auto applied = memory.apply(next.make_loop_epilogue_effect_batch(
                            "eon-millennium-atari-config-loop-epilogue-" + std::to_string(sequence)));
                        if (!applied.accepted) serviced = {false, applied.error};
                    }
                }
                break;
            default:
                result.state = checkpoint.state;
                result.state_id = static_cast<std::uint8_t>(checkpoint.state);
                result.last_sequence = checkpoint.last_sequence;
                result.local_instruction_count = checkpoint.local_instruction_count;
                result.loop_iteration = checkpoint.loop_iteration;
                result.loop_d7_value = checkpoint.loop_d7_value;
                result.palette_helper_entry_count = checkpoint.palette_helper_entry_count;
                result.stop_before_address = stop_address();
                result.status_register_origin = checkpoint.status_register_origin;
                result.line_a_origin = checkpoint.line_a_result_origin;
                result.selector_21_origin = checkpoint.selector_21_result_origin;
                result.selector_6_origin = checkpoint.selector_6_result_origin;
                result.selector_38_origin = checkpoint.selector_38_result_origin;
                result.selector_6_palette_loaded = checkpoint.selector_6_palette_loaded;
                result.movem_registers_preserved_opaque =
                    checkpoint.movem_registers_preserved_opaque;
                result.abstract_movem_rts_stack_delta =
                    checkpoint.abstract_movem_rts_stack_delta;
                result.requires_external_observation = checkpoint.state !=
                    MillenniumAtariConfigConsumerState::game_init_complete;
                result.accepted = true;
                return result;
            }
        } catch (const std::exception& error) {
            result.error = error.what();
            return result;
        }
        if (!serviced.accepted) {
            result.error = serviced.error;
            return result;
        }
        *millennium_atari_config_consumer_ = std::move(next);
        ++result.steps;
    }

    const auto& checkpoint = millennium_atari_config_consumer_->checkpoint();
    result.state = checkpoint.state;
    result.state_id = static_cast<std::uint8_t>(checkpoint.state);
    result.last_sequence = checkpoint.last_sequence;
    result.local_instruction_count = checkpoint.local_instruction_count;
    result.loop_iteration = checkpoint.loop_iteration;
    result.loop_d7_value = checkpoint.loop_d7_value;
    result.palette_helper_entry_count = checkpoint.palette_helper_entry_count;
    if (checkpoint.state == MillenniumAtariConfigConsumerState::line_a_init_boundary)
        result.stop_before_address = checkpoint.line_a_init_address;
    else if (checkpoint.state == MillenniumAtariConfigConsumerState::xbios_trap_boundary
        || checkpoint.state == MillenniumAtariConfigConsumerState::xbios_selector_three_boundary
        || checkpoint.state == MillenniumAtariConfigConsumerState::xbios_selector_four_boundary
        || checkpoint.state == MillenniumAtariConfigConsumerState::xbios_selector_21_boundary
        || checkpoint.state == MillenniumAtariConfigConsumerState::xbios_selector_6_boundary
        || checkpoint.state == MillenniumAtariConfigConsumerState::xbios_selector_38_boundary)
        result.stop_before_address = checkpoint.xbios_trap_address;
    else if (checkpoint.state == MillenniumAtariConfigConsumerState::gemdos_selector_61_boundary)
        result.stop_before_address = checkpoint.gemdos_trap_address;
    else if (checkpoint.state == MillenniumAtariConfigConsumerState::gemdos_selector_63_boundary)
        result.stop_before_address = checkpoint.gemdos_63_trap_address;
    else if (checkpoint.state == MillenniumAtariConfigConsumerState::gemdos_selector_62_boundary)
        result.stop_before_address = checkpoint.gemdos_62_trap_address;
    else if (checkpoint.state == MillenniumAtariConfigConsumerState::jsr_2aa68_boundary
        || checkpoint.state == MillenniumAtariConfigConsumerState::jsr_2aa0c_boundary)
        result.stop_before_address = checkpoint.next_jsr_target;
    else if (checkpoint.state == MillenniumAtariConfigConsumerState::game_init_source_byte_boundary
        || checkpoint.state == MillenniumAtariConfigConsumerState::game_init_zero_copy_boundary
        || checkpoint.state == MillenniumAtariConfigConsumerState::game_init_zero_counter_branch_boundary
        || checkpoint.state == MillenniumAtariConfigConsumerState::game_init_bit6_clear_boundary
        || checkpoint.state == MillenniumAtariConfigConsumerState::game_init_bit7_set_boundary
        || checkpoint.state == MillenniumAtariConfigConsumerState::game_init_second_source_boundary)
        result.stop_before_address = checkpoint.game_init_next_instruction;
    else
        result.stop_before_address = checkpoint.boundary_instruction_address;
    result.status_register_origin = checkpoint.status_register_origin;
    result.line_a_origin = checkpoint.line_a_result_origin;
    result.selector_21_origin = checkpoint.selector_21_result_origin;
    result.selector_6_origin = checkpoint.selector_6_result_origin;
    result.selector_38_origin = checkpoint.selector_38_result_origin;
    result.selector_6_palette_loaded = checkpoint.selector_6_palette_loaded;
    result.movem_registers_preserved_opaque = checkpoint.movem_registers_preserved_opaque;
    result.abstract_movem_rts_stack_delta = checkpoint.abstract_movem_rts_stack_delta;
    result.requires_external_observation = checkpoint.state !=
        MillenniumAtariConfigConsumerState::game_init_complete;
    result.step_limit_reached = true;
    result.accepted = true;
    return result;
}

std::optional<std::vector<float>>
ReleaseRuntimeCoordinator::render_deuteros_amiga_opening_audio(const std::size_t frames) {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_opening
        || !deuteros_amiga_paula_ || !deuteros_amiga_paula_->has_active_channels()) {
        return std::nullopt;
    }
    return deuteros_amiga_paula_->render(frames);
}

std::optional<DeuterosAmigaNativeAudioCheckpoint>
ReleaseRuntimeCoordinator::deuteros_amiga_native_audio_checkpoint() const {
    if(!active_||!session_snapshot_
        ||session_snapshot_->kind!=RuntimeSessionKind::deuteros_amiga_title_stage)
        return std::nullopt;
    return deuteros_amiga_native_audio_checkpoint_;
}

std::optional<std::vector<float>>
ReleaseRuntimeCoordinator::render_deuteros_amiga_native_audio(const std::size_t frames){
    if(!active_||!session_snapshot_
        ||session_snapshot_->kind!=RuntimeSessionKind::deuteros_amiga_title_stage
        ||!deuteros_amiga_native_audio_checkpoint_||!deuteros_amiga_native_audio_mixer_
        ||!deuteros_amiga_native_audio_mixer_->audible())return std::nullopt;
    auto rendered=deuteros_amiga_native_audio_mixer_->render(frames);
    if(rendered.empty())return std::nullopt;
    return rendered;
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
        deuteros_amiga_opening_input_held_,
        deuteros_amiga_->palette_index(),
        deuteros_amiga_->active_channel_count(),
        deuteros_amiga_->frame_composed_on_last_tick(),
        deuteros_amiga_->rgba_frame(),
        {deuteros_amiga_->admitted_game_text().begin(),
            deuteros_amiga_->admitted_game_text().end()},
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
        {deuteros_amiga_->admitted_game_text().begin(),
            deuteros_amiga_->admitted_game_text().end()},
    };
}

DeuterosAmigaTitleHostInputResult
ReleaseRuntimeCoordinator::dispatch_deuteros_amiga_title_raw_key(
    const DeuterosAmigaTitleInputEventPrefix event) {
    DeuterosAmigaTitleHostInputResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !deuteros_amiga_->title_stage_session()
        || !native_runtime_memory_) {
        result.error = "Deuteros raw-key dispatch requires the active owned title stage";
        return result;
    }
    if (deuteros_amiga_title_host_input_sequence_
        == std::numeric_limits<std::uint64_t>::max()) {
        result.error = "Deuteros host input sequence is exhausted";
        return result;
    }
    try {
        auto compatibility = deuteros_amiga_input_device_compatibility_;
        const auto& title = *deuteros_amiga_->title_stage_session();
        if (!compatibility.handler_added()
            && !compatibility.register_title_handler(
                title.callback_registration_profile())) {
            result.error = "Deuteros input-device compatibility profile is unsupported";
            return result;
        }
        const auto original = title.original_bytes();
        auto runtime_stage = native_runtime_memory_->read_linear_range(
            title.stage().destination, original.size());
        if (!runtime_stage) {
            result.error = "Deuteros title bytes are not fully initialized in owned memory";
            return result;
        }
        const auto runtime_before = *runtime_stage;
        const auto callback = execute_deuteros_amiga_owned_title_raw_key_callback(
            original, title.stage().destination, *runtime_stage, event,
            title.callback_code());
        auto memory = *native_runtime_memory_;
        NativeRuntimeEffectBatch batch{
            "deuteros-amiga-title-host-input-"
                + std::to_string(active_runtime_generation_) + "-"
                + std::to_string(deuteros_amiga_title_host_input_sequence_ + 1U),
            true, {}};
        batch.effects.reserve(16);
        for (std::size_t index = 0; index < runtime_stage->size(); ++index) {
            if ((*runtime_stage)[index] == runtime_before[index]) continue;
            batch.effects.push_back({batch.effects.size() + 1U,
                {NativeRuntimeAddressSpace::linear, std::nullopt,
                    title.stage().destination + index},
                MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian,
                (*runtime_stage)[index]});
        }
        if (!batch.effects.empty()) {
            const auto applied = memory.apply(batch);
            if (!applied.accepted) {
                result.error = "Deuteros callback writes were rejected: " + applied.error;
                return result;
            }
        }
        deuteros_amiga_input_device_compatibility_ = compatibility;
        ++deuteros_amiga_title_host_input_sequence_;
        *native_runtime_memory_ = std::move(memory);
        result.accepted = true;
        result.event_class = callback.event_class;
        result.event_code = callback.raw_key_code;
        result.raw_key_code = callback.raw_key_code;
        result.pending_count_before = callback.pending_count_before;
        result.pending_count_after = callback.pending_count_after;
        result.enqueued_byte = callback.enqueued_byte;
        result.stop_address = callback.stop_address;
        if (callback.move_sprite_call_reached) {
            result.pointer_position = std::array<std::uint16_t, 2>{
                callback.pointer_x, callback.pointer_y};
        }
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
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
        result.native_input_handler_registered =
            custom->native_input_handler_registration().has_value();
        result.stop_before_address = custom->stop_before_address();
    } else if (result.open_library) {
        result.stop_before_address = result.open_library->stop_before_address;
    } else {
        result.stop_before_address = result.exec.stop_before_address;
    }
    result.service_setup_boundary_armed = result.callback_exec_return_observed
        || result.native_input_handler_registered;
    result.service_setup_local_plan = deuteros_amiga_title_service_setup_plan_;
    result.native_service_setup_plan =
        title.native_service_setup_plan();
    result.controller_pointer_seed_plan =
        title.controller_pointer_seed_plan();
    result.second_service_local_plan = deuteros_amiga_title_second_service_plan_;
    result.third_service_local_plan = deuteros_amiga_title_third_service_plan_;
    result.fourth_service_local_plan = deuteros_amiga_title_fourth_service_plan_;
    result.fifth_service_local_plan = deuteros_amiga_title_fifth_service_plan_;
    if (result.controller_pointer_seed_plan)
        result.stop_before_address = result.controller_pointer_seed_plan->stop_before_address;
    else if (result.native_service_setup_plan)
        result.stop_before_address = result.native_service_setup_plan->stop_before_address;
    else if (result.fifth_service_local_plan) result.stop_before_address=result.fifth_service_local_plan->stop_before_address;
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
    result.main_stage_loop_prepare=title.main_stage_loop_prepare_body_plan();
    result.main_stage_loop_graphics=title.main_stage_loop_graphics_plan();
    result.main_stage_state=title.main_stage_state();
    result.main_stage_last_command_sequence=title.main_stage_last_command_sequence();
    if(result.main_stage_loop_graphics){
        const auto& plan=*result.main_stage_loop_graphics;
        result.stop_before_address=plan.pending_read_instruction!=0
            ?plan.pending_read_instruction:(plan.next_instruction_address!=0
                ?plan.next_instruction_address:plan.next_call_address);
    }else if(result.main_stage_loop_prepare){
        result.stop_before_address=result.main_stage_loop_prepare->next_call_address;
    }
    return result;
}

std::optional<DeuterosAmigaBootstrapFrameSnapshot>
ReleaseRuntimeCoordinator::deuteros_amiga_bootstrap_frame() const {
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage) {
        return std::nullopt;
    }
    return deuteros_amiga_bootstrap_frame_;
}

DeuterosAmigaMainStageFrame
ReleaseRuntimeCoordinator::deuteros_amiga_main_stage_frame() const {
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage) {
        return {};
    }
    return deuteros_amiga_main_stage_frame_;
}

std::optional<DeuterosAmigaTitleProgramEntrySnapshot>
ReleaseRuntimeCoordinator::deuteros_amiga_title_program_entry() const {
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_program_entry) {
        return std::nullopt;
    }
    return deuteros_amiga_title_program_entry_;
}

std::optional<DeuterosAmigaBootstrapGraphicsBoundarySnapshot>
ReleaseRuntimeCoordinator::deuteros_amiga_bootstrap_graphics_boundary() const {
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind
            != RuntimeSessionKind::deuteros_amiga_bootstrap_graphics_boundary) {
        return std::nullopt;
    }
    return deuteros_amiga_bootstrap_graphics_boundary_;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_bootstrap_availmem_return(
    const DeuterosAmigaBootstrapAvailMemReturnObservation observation) {
    DeuterosAmigaTitleDependencyObservationResult result;
    constexpr std::string_view expected_hash =
        "ca2490219557649ff335d47ccc9d15fcbaa34da2ab9c723feb6bcea147970c0f";
    if (!active_ || !native_runtime_memory_ || !session_snapshot_
        || session_snapshot_->kind
            != RuntimeSessionKind::deuteros_amiga_bootstrap_graphics_boundary
        || !deuteros_amiga_bootstrap_graphics_boundary_) {
        result.error = "Deuteros bootstrap AvailMem return requires its active boundary";
        return result;
    }
    auto& boundary = *deuteros_amiga_bootstrap_graphics_boundary_;
    if (boundary.source_sha256 != expected_hash || boundary.source_address != 0x13000
        || boundary.source_length != 108 || boundary.stop_before_address != 0x13028
        || boundary.stop_vector != -0xd8 || boundary.stop_return_address != 0x1302c
        || observation.trace_sequence == 0
        || observation.trace_sequence <= boundary.last_observation_sequence
        || observation.call_address != 0x13028 || observation.return_address != 0x1302c
        || observation.vector != -0xd8
        || observation.exec_base != boundary.eon_virtual_exec_base) {
        result.error = "Deuteros bootstrap AvailMem return does not match its active source boundary";
        return result;
    }

    auto memory = *native_runtime_memory_;
    const auto code = memory.read_linear_range(boundary.source_address, boundary.source_length);
    const auto exec_pointer = memory.read_linear_range(boundary.exec_base_pointer_cell, 4);
    const auto library_token = memory.read_linear_range(0x12fec, 4);
    const auto display_base = memory.read_linear_range(0x12ff4, 4);
    if (!code || to_hex(sha256(*code)) != expected_hash || !exec_pointer || !library_token
        || !display_base
        || *display_base != std::vector<std::uint8_t>{0, 0, 0xab, 0}) {
        result.error = "Deuteros bootstrap AvailMem route lacks its authentic source-owned memory base";
        return result;
    }
    const auto decode_be32 = [](const std::vector<std::uint8_t>& bytes) {
        return (static_cast<std::uint32_t>(bytes[0]) << 24U)
            | (static_cast<std::uint32_t>(bytes[1]) << 16U)
            | (static_cast<std::uint32_t>(bytes[2]) << 8U)
            | static_cast<std::uint32_t>(bytes[3]);
    };
    if (decode_be32(*exec_pointer) != boundary.eon_virtual_exec_base
        || decode_be32(*library_token) != boundary.eon_graphics_library_token) {
        result.error = "Deuteros bootstrap AvailMem route has detached compatibility pointers";
        return result;
    }

    std::array<std::uint8_t, 4> exec_bytes{};
    std::array<std::uint8_t, 4> library_bytes{};
    std::copy(exec_pointer->begin(), exec_pointer->end(), exec_bytes.begin());
    std::copy(library_token->begin(), library_token->end(), library_bytes.begin());
    std::array<m68k::MemoryRange, 2> memory_ranges{{
        {boundary.exec_base_pointer_cell, exec_bytes, false},
        {0x12fec, library_bytes, false},
    }};
    m68k::MachineState initial;
    initial.pc = observation.return_address;
    initial.sr = observation.result_status_register;
    initial.data[0] = observation.result_d0;
    initial.data[1] = 4;
    initial.address[6] = observation.exec_base;
    constexpr std::array<std::uint32_t, 2> next_boundaries{0x13034, 0x13058};
    const auto execution = m68k::execute(*code, boundary.source_address, initial,
        memory_ranges, 8, next_boundaries);
    const bool zero = (observation.result_status_register & 0x0004U) != 0;
    const std::uint32_t expected_next = zero ? 0x13034 : 0x13058;
    if (execution.reason != m68k::StopReason::requested_address
        || execution.state.pc != expected_next || execution.instructions_executed
            != (zero ? 3U : 3U)
        || execution.branches.size() != 1
        || execution.branches.front().instruction_address != 0x1302c
        || execution.branches.front().status_register != observation.result_status_register
        || execution.branches.front().taken != !zero) {
        result.error = "Deuteros bootstrap AvailMem return did not reach the source-selected next service";
        return result;
    }
    if ((zero && (execution.state.data[1] != 2
            || execution.state.address[6] != boundary.eon_virtual_exec_base))
        || (!zero && (execution.state.address[1] != 0x12e00
            || execution.state.address[6] != boundary.eon_graphics_library_token))) {
        result.error = "Deuteros bootstrap AvailMem branch registers do not match authentic source";
        return result;
    }

    boundary.last_observation_sequence = observation.trace_sequence;
    boundary.source_instruction_count += static_cast<std::uint32_t>(
        execution.instructions_executed);
    boundary.availmem_d0_result = observation.result_d0;
    boundary.availmem_status_register = observation.result_status_register;
    boundary.stop_before_address = expected_next;
    boundary.stop_vector = zero ? -0xd8 : -0x168;
    boundary.stop_return_address = expected_next + 4U;
    boundary.stop_d1_value = zero ? 2U : 0U;
    result.accepted = true;
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_bootstrap_second_availmem_return(
    const DeuterosAmigaBootstrapSecondAvailMemReturnObservation observation) {
    DeuterosAmigaTitleDependencyObservationResult result;
    constexpr std::string_view expected_hash =
        "ca2490219557649ff335d47ccc9d15fcbaa34da2ab9c723feb6bcea147970c0f";
    if (!active_ || !native_runtime_memory_ || !session_snapshot_
        || session_snapshot_->kind
            != RuntimeSessionKind::deuteros_amiga_bootstrap_graphics_boundary
        || !deuteros_amiga_bootstrap_graphics_boundary_) {
        result.error = "Deuteros second bootstrap AvailMem return requires its active boundary";
        return result;
    }
    auto& boundary = *deuteros_amiga_bootstrap_graphics_boundary_;
    if (boundary.source_sha256 != expected_hash || boundary.source_address != 0x13000
        || boundary.source_length != 108 || boundary.stop_before_address != 0x13034
        || boundary.stop_vector != -0xd8 || boundary.stop_return_address != 0x13038
        || boundary.stop_d1_value != 2 || boundary.availmem_status_register != 0x0004
        || observation.trace_sequence == 0
        || observation.trace_sequence <= boundary.last_observation_sequence
        || observation.call_address != 0x13034 || observation.return_address != 0x13038
        || observation.vector != -0xd8 || observation.d1_value != 2
        || observation.exec_base != boundary.eon_virtual_exec_base) {
        result.error = "Deuteros second bootstrap AvailMem return does not match its source boundary";
        return result;
    }

    auto memory = *native_runtime_memory_;
    const auto code = memory.read_linear_range(boundary.source_address, boundary.source_length);
    const auto exec_pointer = memory.read_linear_range(boundary.exec_base_pointer_cell, 4);
    const auto library_token = memory.read_linear_range(0x12fec, 4);
    const auto display_base = memory.read_linear_range(0x12ff4, 4);
    if (!code || to_hex(sha256(*code)) != expected_hash || !exec_pointer || !library_token
        || !display_base
        || *display_base != std::vector<std::uint8_t>{0, 0, 0xab, 0}) {
        result.error = "Deuteros second AvailMem route lacks its authentic source-owned memory base";
        return result;
    }
    const auto decode_be32 = [](const std::vector<std::uint8_t>& bytes) {
        return (static_cast<std::uint32_t>(bytes[0]) << 24U)
            | (static_cast<std::uint32_t>(bytes[1]) << 16U)
            | (static_cast<std::uint32_t>(bytes[2]) << 8U)
            | static_cast<std::uint32_t>(bytes[3]);
    };
    if (decode_be32(*exec_pointer) != boundary.eon_virtual_exec_base
        || decode_be32(*library_token) != boundary.eon_graphics_library_token) {
        result.error = "Deuteros second AvailMem route has detached compatibility pointers";
        return result;
    }

    std::array<std::uint8_t, 4> exec_bytes{};
    std::array<std::uint8_t, 4> library_bytes{};
    std::array<std::uint8_t, 4> display_bytes{};
    std::copy(exec_pointer->begin(), exec_pointer->end(), exec_bytes.begin());
    std::copy(library_token->begin(), library_token->end(), library_bytes.begin());
    std::copy(display_base->begin(), display_base->end(), display_bytes.begin());
    std::array<m68k::MemoryRange, 3> memory_ranges{{
        {boundary.exec_base_pointer_cell, exec_bytes, false},
        {0x12fec, library_bytes, false},
        {0x12ff4, display_bytes, true},
    }};
    m68k::MachineState initial;
    initial.pc = observation.return_address;
    initial.data[0] = observation.result_d0;
    initial.data[1] = observation.d1_value;
    initial.address[6] = observation.exec_base;
    constexpr std::uint32_t display_base_store = 0x8ab00;
    const auto below_threshold = (observation.result_d0 & 0x80000000U) != 0U
        || observation.result_d0 < 0x00080000U;
    constexpr std::array<std::uint32_t, 1> next_boundary{0x13058};
    const auto execution = m68k::execute(*code, boundary.source_address, initial,
        memory_ranges, 8, next_boundary);
    const auto expected_display_base = below_threshold ? 0x0000ab00U : display_base_store;
    const auto expected_d0_after = below_threshold
        ? observation.result_d0 : display_base_store;
    if (execution.reason != m68k::StopReason::requested_address
        || execution.state.pc != 0x13058
        || execution.instructions_executed != (below_threshold ? 4U : 6U)
        || execution.branches.size() != 1
        || execution.branches.front().instruction_address != 0x1303e
        || execution.branches.front().target_address != 0x1304c
        || execution.branches.front().taken != below_threshold
        || execution.state.address[1] != 0x12e00
        || execution.state.address[6] != boundary.eon_graphics_library_token
        || execution.state.data[0] != expected_d0_after
        || display_bytes != std::array<std::uint8_t, 4>{
            static_cast<std::uint8_t>(expected_display_base >> 24U),
            static_cast<std::uint8_t>(expected_display_base >> 16U),
            static_cast<std::uint8_t>(expected_display_base >> 8U),
            static_cast<std::uint8_t>(expected_display_base)}) {
        result.error = "Deuteros second AvailMem return did not follow the authentic signed threshold branch";
        return result;
    }

    if (!below_threshold) {
        NativeRuntimeEffectBatch source_store{
            "deuteros-amiga-bootstrap-availmem-display-base-"
                + std::to_string(active_runtime_generation_), true,
            {{1, {NativeRuntimeAddressSpace::linear, std::nullopt, 0x12ff4},
                MemoryTransferElementWidth::longword,
                NativeRuntimeByteOrder::big_endian, display_base_store}}};
        if (!memory.apply(source_store).accepted) {
            result.error = "Deuteros second AvailMem display-base store could not be committed";
            return result;
        }
        *native_runtime_memory_ = std::move(memory);
    }
    boundary.last_observation_sequence = observation.trace_sequence;
    boundary.second_availmem_d0_result = observation.result_d0;
    boundary.display_base_after_second_availmem = expected_display_base;
    boundary.stop_before_address = 0x13058;
    boundary.stop_vector = -0x168;
    boundary.stop_return_address = 0x1305c;
    boundary.stop_d1_value = 0;
    boundary.source_instruction_count += static_cast<std::uint32_t>(
        execution.instructions_executed);
    result.accepted = true;
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_bootstrap_graphics_compatibility() {
    DeuterosAmigaTitleDependencyObservationResult result;
    constexpr std::string_view expected_hash =
        "ca2490219557649ff335d47ccc9d15fcbaa34da2ab9c723feb6bcea147970c0f";
    if (!active_ || !native_runtime_memory_ || !session_snapshot_
        || session_snapshot_->kind
            != RuntimeSessionKind::deuteros_amiga_bootstrap_graphics_boundary
        || !deuteros_amiga_bootstrap_graphics_boundary_
        || !deuteros_amiga_graphics_library_compatibility_
        || !deuteros_amiga_graphics_library_compatibility_->service_opened()) {
        result.error = "Deuteros bootstrap graphics compatibility requires its active OpenLibrary route";
        return result;
    }

    auto& boundary = *deuteros_amiga_bootstrap_graphics_boundary_;
    const bool first_availmem_z_clear = boundary.availmem_status_register
        && ((*boundary.availmem_status_register & 0x0004U) == 0);
    const bool second_availmem_completed = boundary.availmem_status_register
        && ((*boundary.availmem_status_register & 0x0004U) != 0)
        && boundary.second_availmem_d0_result.has_value();
    if (boundary.source_sha256 != expected_hash || boundary.source_address != 0x13000
        || boundary.source_length != 108 || boundary.stop_before_address != 0x13058
        || boundary.stop_vector != -0x168 || boundary.stop_return_address != 0x1305c
        || (!first_availmem_z_clear && !second_availmem_completed)
        || boundary.bootstrap_init_view_receipt
        || boundary.bootstrap_init_viewport_receipt) {
        result.error = "Deuteros bootstrap graphics compatibility requires a fresh source-selected $13058 boundary after observed AvailMem";
        return result;
    }

    try {
        auto memory = *native_runtime_memory_;
        auto graphics = *deuteros_amiga_graphics_library_compatibility_;
        const auto code = memory.read_linear_range(boundary.source_address,
            boundary.source_length);
        const auto exec_pointer = memory.read_linear_range(boundary.exec_base_pointer_cell, 4);
        const auto library_token = memory.read_linear_range(0x12fec, 4);
        const auto display_base = memory.read_linear_range(0x12ff4, 4);
        if (!code || to_hex(sha256(*code)) != expected_hash || !exec_pointer
            || !library_token || !display_base
            || *display_base != std::vector<std::uint8_t>{
                static_cast<std::uint8_t>(boundary.display_base_after_second_availmem.value_or(0xab00)
                    >> 24U),
                static_cast<std::uint8_t>(boundary.display_base_after_second_availmem.value_or(0xab00)
                    >> 16U),
                static_cast<std::uint8_t>(boundary.display_base_after_second_availmem.value_or(0xab00)
                    >> 8U),
                static_cast<std::uint8_t>(boundary.display_base_after_second_availmem.value_or(0xab00))}) {
            result.error = "Deuteros bootstrap graphics compatibility lost its admitted source or selected display base";
            return result;
        }
        const auto decode_be32 = [](const std::vector<std::uint8_t>& bytes) {
            return (static_cast<std::uint32_t>(bytes[0]) << 24U)
                | (static_cast<std::uint32_t>(bytes[1]) << 16U)
                | (static_cast<std::uint32_t>(bytes[2]) << 8U)
                | static_cast<std::uint32_t>(bytes[3]);
        };
        if (decode_be32(*exec_pointer) != boundary.eon_virtual_exec_base
            || decode_be32(*library_token) != boundary.eon_graphics_library_token) {
            result.error = "Deuteros bootstrap graphics compatibility has detached Eon service pointers";
            return result;
        }

        const auto view = graphics.initialize_view(memory, 0x13058, -0x168,
            0x1305c, 0x12e00);
        if (!view || view->origin != DeuterosAmigaServiceOrigin::native_compatibility
            || view->runtime_generation != boundary.runtime_generation
            || view->service_sequence != 2 || view->object_address != 0x12e00) {
            result.error = "Eon InitView compatibility rejected the authenticated bootstrap call";
            return result;
        }

        std::array<std::uint8_t, 4> exec_bytes{};
        std::array<std::uint8_t, 4> library_bytes{};
        std::copy(exec_pointer->begin(), exec_pointer->end(), exec_bytes.begin());
        std::copy(library_token->begin(), library_token->end(), library_bytes.begin());
        std::array<m68k::MemoryRange, 2> memory_ranges{{
            {boundary.exec_base_pointer_cell, exec_bytes, false},
            {0x12fec, library_bytes, false},
        }};
        m68k::MachineState initial;
        initial.pc = 0x1305c;
        initial.address[1] = view->object_address;
        initial.address[6] = boundary.eon_graphics_library_token;
        constexpr std::array<std::uint32_t, 1> viewport_boundary{0x13068};
        const auto view_return = m68k::execute(*code, boundary.source_address,
            initial, memory_ranges, 8, viewport_boundary);
        if (view_return.reason != m68k::StopReason::requested_address
            || view_return.state.pc != 0x13068
            || view_return.instructions_executed == 0
            || view_return.state.address[0] != 0x12e12
            || view_return.state.address[6] != boundary.eon_graphics_library_token) {
            result.error = "Authenticated InitView caller continuation did not reach its exact InitVPort request";
            return result;
        }
        const auto viewport = graphics.initialize_viewport(memory, 0x13068, -0x0cc,
            0x1306c, view_return.state.address[0]);
        if (!viewport || viewport->origin != DeuterosAmigaServiceOrigin::native_compatibility
            || viewport->runtime_generation != boundary.runtime_generation
            || viewport->service_sequence != 3 || viewport->object_address != 0x12e12) {
            result.error = "Eon InitVPort compatibility rejected the authenticated bootstrap call";
            return result;
        }

        boundary.bootstrap_init_view_receipt = *view;
        boundary.bootstrap_init_viewport_receipt = *viewport;
        boundary.bootstrap_graphics_init_origin = DeuterosAmigaServiceOrigin::native_compatibility;
        boundary.bootstrap_graphics_init_source_stop = 0x1306c;
        boundary.stop_before_address = 0x1306c;
        boundary.stop_vector = 0;
        boundary.stop_return_address = 0x1306c;
        boundary.stop_d1_value = 0;
        boundary.source_instruction_count += static_cast<std::uint32_t>(
            view_return.instructions_executed);
        *deuteros_amiga_graphics_library_compatibility_ = std::move(graphics);
        *native_runtime_memory_ = std::move(memory);
        result.accepted = true;
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_bootstrap_eon_memory_services() {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!active_ || !native_runtime_memory_ || !session_snapshot_
        || session_snapshot_->kind
            != RuntimeSessionKind::deuteros_amiga_bootstrap_graphics_boundary
        || !deuteros_amiga_bootstrap_graphics_boundary_
        || !deuteros_amiga_eon_memory_arena_) {
        result.error = "Deuteros Eon memory services require the active bootstrap arena";
        return result;
    }

    auto& boundary = *deuteros_amiga_bootstrap_graphics_boundary_;
    auto pending_arena = *deuteros_amiga_eon_memory_arena_;
    if (pending_arena.runtime_generation() != active_runtime_generation_
        || boundary.runtime_generation != active_runtime_generation_) {
        result.error = "Deuteros Eon memory arena belongs to a different runtime generation";
        return result;
    }

    while (boundary.stop_before_address == DeuterosAmigaEonMemoryArena::first_call_address
        || boundary.stop_before_address == DeuterosAmigaEonMemoryArena::second_call_address) {
        const auto call = boundary.stop_before_address;
        const auto return_address = call + 4U;
        const auto receipt = pending_arena.avail_mem(*native_runtime_memory_, call,
            return_address, boundary.stop_d1_value);
        if (!receipt || receipt->runtime_generation != active_runtime_generation_
            || receipt->service_sequence <= boundary.last_observation_sequence) {
            result.error = "Deuteros Eon memory arena rejected the ordered AvailMem request";
            return result;
        }
        if (call == DeuterosAmigaEonMemoryArena::first_call_address) {
            const DeuterosAmigaBootstrapAvailMemReturnObservation observation{
                receipt->service_sequence, receipt->call_address,
                receipt->return_address, boundary.eon_virtual_exec_base, -0xd8,
                receipt->free_bytes, receipt->result_status_register};
            result = observe_deuteros_amiga_bootstrap_availmem_return(observation);
            if (!result.accepted) return result;
            boundary.eon_first_availmem = *receipt;
        } else {
            const DeuterosAmigaBootstrapSecondAvailMemReturnObservation observation{
                receipt->service_sequence, receipt->call_address,
                receipt->return_address, boundary.eon_virtual_exec_base, -0xd8,
                receipt->memory_flags, receipt->free_bytes};
            result = observe_deuteros_amiga_bootstrap_second_availmem_return(observation);
            if (!result.accepted) return result;
            boundary.eon_second_availmem = *receipt;
        }
        *deuteros_amiga_eon_memory_arena_ = pending_arena;
    }

    if (boundary.stop_before_address != 0x13058) {
        result.error = "Deuteros Eon AvailMem path did not select the authenticated InitView call";
        return result;
    }
    return advance_deuteros_amiga_bootstrap_graphics_compatibility();
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_bootstrap_view_pair_native() {
    DeuterosAmigaTitleDependencyObservationResult result;
    constexpr std::uint32_t source_address = 0x1306c;
    constexpr std::size_t source_length = 674;
    constexpr std::string_view source_hash =
        "171b18ac0a6796d1edf2fd693113b706fcf809bccdc29db0aa99f14a5372a9b5";
    if (!active_ || !native_runtime_memory_ || !session_snapshot_
        || session_snapshot_->kind
            != RuntimeSessionKind::deuteros_amiga_bootstrap_graphics_boundary
        || !deuteros_amiga_bootstrap_graphics_boundary_
        || !deuteros_amiga_graphics_library_compatibility_
        || !deuteros_amiga_eon_memory_arena_) {
        result.error = "Deuteros paired view setup requires its active native bootstrap";
        return result;
    }
    auto& boundary = *deuteros_amiga_bootstrap_graphics_boundary_;
    if (boundary.runtime_generation != active_runtime_generation_
        || boundary.stop_before_address != source_address
        || boundary.bootstrap_graphics_init_source_stop != source_address
        || !boundary.eon_first_availmem || !boundary.eon_second_availmem
        || boundary.display_base_after_second_availmem
            != DeuterosAmigaEonMemoryArena::selected_plane_address
        || boundary.bootstrap_init_rastport_receipt
        || !boundary.bootstrap_init_view_receipt
        || !boundary.bootstrap_init_viewport_receipt
        || boundary.bootstrap_init_view_receipt->service_sequence != 2
        || boundary.bootstrap_init_viewport_receipt->service_sequence != 3) {
        result.error = "Deuteros paired view setup is detached from its Eon-selected bootstrap path";
        return result;
    }

    try {
        auto memory = *native_runtime_memory_;
        auto graphics = *deuteros_amiga_graphics_library_compatibility_;
        const auto layout_source = memory.read_linear_range(source_address, source_length);
        if (!layout_source || to_hex(sha256(*layout_source)) != source_hash
            || to_hex(sha256(memory.read_linear_range(0x13000, 108).value_or(
                std::vector<std::uint8_t>{})))
                != "ca2490219557649ff335d47ccc9d15fcbaa34da2ab9c723feb6bcea147970c0f") {
            throw std::runtime_error("Deuteros paired layout code is not the admitted original transfer");
        }

        const auto read_be = [&](const std::map<std::uint32_t, std::uint8_t>& writes,
            const std::uint32_t address, const std::uint32_t width) {
            if (address > 0x01000000U - width || (width > 1U && (address & 1U))) {
                throw std::runtime_error("Deuteros paired-layout read exceeds native memory");
            }
            std::uint32_t value = 0;
            for (std::uint32_t offset = 0; offset < width; ++offset) {
                const auto found = writes.find(address + offset);
                const auto byte = found != writes.end()
                    ? std::optional<std::uint8_t>{found->second}
                    : memory.read_byte({NativeRuntimeAddressSpace::linear, std::nullopt,
                        address + offset});
                if (!byte) throw std::runtime_error("Deuteros paired-layout source byte is not owned");
                value = (value << 8U) | *byte;
            }
            return value;
        };
        std::map<std::uint32_t, std::uint8_t> staged;
        const auto write_be = [&](const std::uint32_t address,
            const MemoryTransferElementWidth width, const std::uint32_t value) {
            const auto byte_count = static_cast<std::uint32_t>(width);
            if (address > 0x01000000U - byte_count
                || (byte_count > 1U && (address & 1U))) {
                throw std::runtime_error("Deuteros paired-layout write exceeds native memory");
            }
            for (std::uint32_t offset = 0; offset < byte_count; ++offset) {
                staged[address + offset] = static_cast<std::uint8_t>(
                    value >> ((byte_count - offset - 1U) * 8U));
            }
        };
        const auto apply_staged = [&](const std::string& batch_id) {
            NativeRuntimeEffectBatch batch{batch_id, true, {}};
            batch.effects.reserve(staged.size());
            std::size_t order = 1;
            for (const auto& [address, value] : staged) {
                batch.effects.push_back({order++,
                    {NativeRuntimeAddressSpace::linear, std::nullopt, address},
                    MemoryTransferElementWidth::byte,
                    NativeRuntimeByteOrder::big_endian, value});
            }
            const auto applied = memory.apply(batch);
            if (!applied.accepted) throw std::runtime_error(applied.error);
            staged.clear();
        };

        const auto library_token = read_be(staged, 0x12fec, 4);
        if (library_token == 0 || library_token != boundary.eon_graphics_library_token
            || read_be(staged, 0x12ff4, 4)
                != DeuterosAmigaEonMemoryArena::selected_plane_address) {
            throw std::runtime_error("Deuteros Eon display-memory choice is detached from owned data");
        }

        DeuterosAmigaMainStageLoopGraphicsPlan plan;
        plan.next_call_address = 0x13068;
        plan.next_return_address = 0x1306c;
        plan.a0_value = 0x12e12;
        plan.a6_value = library_token;
        plan.next_vector = -0x0cc;
        DeuterosAmigaObservedLoopRequestService init_vport_return{
            boundary.bootstrap_init_viewport_receipt->service_sequence,
            0x13062, 0x12fec, library_token, 0x13068, 0x1306c, 0, -0x0cc};
        plan = execute_deuteros_amiga_outer_service(plan, init_vport_return,
            [&](const auto address, const auto width) { return read_be(staged, address, width); },
            write_be);
        if (plan.next_call_address != 0x1309e || plan.next_return_address != 0x130a2
            || plan.next_vector != -0x186 || plan.a0_value != 0x12e3a
            || plan.d0_value != 4 || plan.d1_value != 320 || plan.d2_value != 200) {
            throw std::runtime_error("Deuteros original layout did not reach its InitBitMap request");
        }
        apply_staged("deuteros-amiga-eon-bootstrap-view-prefix-"
            + std::to_string(active_runtime_generation_));

        const auto bitmap = graphics.initialize_bitmap(memory, plan.next_call_address,
            plan.next_vector, plan.next_return_address, plan.a0_value,
            static_cast<std::uint16_t>(plan.d0_value),
            static_cast<std::uint16_t>(plan.d1_value),
            static_cast<std::uint16_t>(plan.d2_value));
        if (!bitmap || bitmap->origin != DeuterosAmigaServiceOrigin::native_compatibility
            || bitmap->runtime_generation != active_runtime_generation_
            || bitmap->service_sequence != 4 || bitmap->object_address != 0x12e3a) {
            throw std::runtime_error("Eon InitBitMap service rejected the authenticated layout request");
        }

        DeuterosAmigaObservedLoopRequestService init_bitmap_return{
            bitmap->service_sequence, 0x13098, 0x12fec, library_token,
            0x1309e, 0x130a2, 0, -0x186};
        plan = execute_deuteros_amiga_outer_service(plan, init_bitmap_return,
            [&](const auto address, const auto width) { return read_be(staged, address, width); },
            write_be);
        constexpr std::uint32_t expected_d0 =
            DeuterosAmigaEonMemoryArena::selected_plane_address + 24000U;
        if (plan.next_call_address != 0x13154 || plan.next_return_address != 0x13158
            || plan.next_vector != -0x0c6 || plan.a0_value != 0x12e12
            || plan.a6_value != library_token || plan.d0_value != expected_d0
            || plan.d1_value != 8000 || plan.d2_value != 0xffff || plan.write_count != 0) {
            throw std::runtime_error("Deuteros layout continuation escaped its MakeVPort boundary");
        }

        const auto rastport = graphics.initialize_rastport(memory,
            plan.next_call_address, plan.next_vector, plan.next_return_address,
            plan.a1_value);
        if (!rastport || rastport->origin != DeuterosAmigaServiceOrigin::native_compatibility
            || rastport->runtime_generation != active_runtime_generation_
            || rastport->service_sequence != 5 || rastport->object_address != 0x12e6e
            || graphics.system_font_address() != 0x00d10000U) {
            throw std::runtime_error("Eon InitRastPort service rejected the authenticated call");
        }
        DeuterosAmigaObservedLoopRequestService init_rastport_return{
            rastport->service_sequence, 0x1314e, 0x12fec, library_token,
            0x13154, 0x13158, 0, -0x0c6};
        plan = execute_deuteros_amiga_outer_service(plan, init_rastport_return,
            [&](const auto address, const auto width) { return read_be(staged, address, width); },
            write_be);
        if (plan.next_call_address != 0x13188 || plan.next_return_address != 0x1318c
            || plan.next_vector != -0x0d8 || plan.a0_value != 0x12e00
            || plan.a1_value != 0x12e12 || read_be(staged, 0x12e72, 4) != 0x12e3a) {
            throw std::runtime_error("Deuteros InitRastPort return did not reach MakeVPort");
        }

        const auto plane_batch = deuteros_amiga_eon_memory_arena_->reserve_selected_planes(
            memory, DeuterosAmigaEonMemoryArena::selected_plane_address,
            DeuterosAmigaEonMemoryArena::four_plane_bytes,
            "deuteros-amiga-eon-chip-plane-reservation-"
                + std::to_string(active_runtime_generation_));
        if (!plane_batch) {
            throw std::runtime_error("Eon chip arena could not reserve the source-selected plane range");
        }
        apply_staged("deuteros-amiga-eon-bootstrap-view-layout-"
            + std::to_string(active_runtime_generation_));
        const auto plane_applied = memory.apply(*plane_batch);
        if (!plane_applied.accepted) throw std::runtime_error(plane_applied.error);

        if (read_be({}, 0x12e16, 4) != 0x12ec4
            || read_be({}, 0x12ec6, 2) != 20
            || read_be({}, 0x12ec8, 4) != 0x12ecc
            || read_be({}, 0x12e42, 4) != 0xab00
            || read_be({}, 0x12e46, 4) != 0xca40
            || read_be({}, 0x12e4a, 4) != 0xe980
            || read_be({}, 0x12e4e, 4) != expected_d0) {
            throw std::runtime_error("Deuteros paired layout failed its owned ColorMap/plane checks");
        }
        for (std::uint32_t i = 0; i < 40; ++i) {
            const auto byte = memory.read_byte({NativeRuntimeAddressSpace::linear,
                std::nullopt, 0x12ecc + i});
            if (!byte || *byte != 0) {
                throw std::runtime_error("Deuteros initial ColorMap is not source-cleared");
            }
        }

        boundary.paired_layout_source_address = source_address;
        boundary.paired_layout_source_length = source_length;
        boundary.paired_layout_source_sha256 = std::string(source_hash);
        boundary.paired_layout_stop_address = plan.next_call_address;
        boundary.paired_layout_plane_base = DeuterosAmigaEonMemoryArena::selected_plane_address;
        boundary.paired_layout_plane_bytes = DeuterosAmigaEonMemoryArena::four_plane_bytes;
        boundary.paired_layout_origin = DeuterosAmigaServiceOrigin::native_compatibility;
        boundary.stop_before_address = plan.next_call_address;
        boundary.stop_vector = plan.next_vector;
        boundary.stop_return_address = plan.next_return_address;
        boundary.stop_d1_value = plan.d1_value;
        boundary.bootstrap_graphics_init_source_stop = plan.next_call_address;
        boundary.bootstrap_init_rastport_receipt = *rastport;
        *deuteros_amiga_graphics_library_compatibility_ = std::move(graphics);
        *native_runtime_memory_ = std::move(memory);
        result.accepted = true;
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_bootstrap_makevport_return(
    const DeuterosAmigaObservedMakeVPortReturn observation) {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!active_ || !native_runtime_memory_ || !session_snapshot_
        || session_snapshot_->kind
            != RuntimeSessionKind::deuteros_amiga_bootstrap_graphics_boundary
        || !deuteros_amiga_bootstrap_graphics_boundary_
        || !deuteros_amiga_graphics_library_compatibility_) {
        result.error = "Deuteros MakeVPort observation requires its active bootstrap boundary";
        return result;
    }

    auto& boundary = *deuteros_amiga_bootstrap_graphics_boundary_;
    if (boundary.runtime_generation != active_runtime_generation_
        || boundary.stop_before_address != 0x13188U
        || boundary.stop_vector != -0x0d8
        || boundary.stop_return_address != 0x1318cU
        || boundary.paired_layout_stop_address != 0x13188U
        || boundary.bootstrap_graphics_init_source_stop != 0x13188U
        || !boundary.bootstrap_init_rastport_receipt
        || boundary.bootstrap_init_rastport_receipt->runtime_generation
            != active_runtime_generation_
        || boundary.bootstrap_init_rastport_receipt->service_sequence != 5
        || boundary.bootstrap_makevport_return_receipt
        || boundary.bootstrap_makevport_prefix_receipt
        || observation.trace_sequence <= boundary.last_observation_sequence) {
        result.error = "Deuteros MakeVPort observation is detached from its exact native call boundary";
        return result;
    }

    const auto receipt = deuteros_amiga_graphics_library_compatibility_->
        observe_bootstrap_makevport_return(observation);
    if (!receipt || receipt->runtime_generation != active_runtime_generation_
        || receipt->service_sequence != 6
        || receipt->modeled_memory_effect_count != 0
        || receipt->origin != DeuterosAmigaServiceOrigin::external_observation) {
        result.error = "Deuteros MakeVPort return observation failed typed admission";
        return result;
    }

    // Preserve the boundary at $13188: the observation proves only a vector
    // return. The routine's allocations and Copper writes have not been
    // supplied, so the caller cannot safely continue to MrgCop yet.
    boundary.bootstrap_makevport_return_receipt = *receipt;
    boundary.last_observation_sequence = observation.trace_sequence;
    result.accepted = true;
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_bootstrap_makevport_prefix() {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!active_ || !native_runtime_memory_ || !session_snapshot_
        || session_snapshot_->kind
            != RuntimeSessionKind::deuteros_amiga_bootstrap_graphics_boundary
        || !deuteros_amiga_bootstrap_graphics_boundary_
        || !deuteros_amiga_graphics_library_compatibility_) {
        result.error = "Deuteros MakeVPort prefix requires its active native bootstrap";
        return result;
    }
    auto& boundary = *deuteros_amiga_bootstrap_graphics_boundary_;
    if (boundary.runtime_generation != active_runtime_generation_
        || boundary.stop_before_address != 0x13188U
        || boundary.stop_vector != -0x0d8
        || boundary.stop_return_address != 0x1318cU
        || boundary.paired_layout_stop_address != 0x13188U
        || !boundary.bootstrap_init_rastport_receipt
        || boundary.bootstrap_init_rastport_receipt->runtime_generation
            != active_runtime_generation_
        || boundary.bootstrap_init_rastport_receipt->service_sequence != 5
        || boundary.bootstrap_makevport_return_receipt
        || boundary.bootstrap_makevport_prefix_receipt
        || !deuteros_amiga_graphics_library_compatibility_->service_opened()) {
        result.error = "Deuteros MakeVPort prefix is detached from its verified caller state";
        return result;
    }

    try {
        auto memory = *native_runtime_memory_;
        auto prefix = deuteros_amiga_makevport_prefix_.value_or(
            DeuterosAmigaMakeVPortPrefixSession(active_runtime_generation_));
        const auto receipt = prefix.begin_bootstrap_call(memory, 0x13188U, -0x0d8,
            0x1318cU, 0x12e00U, 0x12e12U,
            boundary.eon_graphics_library_token,
            boundary.eon_graphics_library_token, 6);
        if (!receipt || receipt->runtime_generation != active_runtime_generation_
            || receipt->workspace_address != 0x00d20000U
            || receipt->copper_storage_address != 0x00d20022U
            || receipt->next_copper_instruction_address != 0x00d20028U
            || receipt->copper_instructions_emitted != 1
            || receipt->resume_address != 0xfcc7b0U) {
            result.error = "Deuteros MakeVPort source prefix rejected the exact ViewPort preimage";
            return result;
        }
        const auto colors = prefix.translate_bootstrap_color_ranges(memory);
        if (!colors || colors->runtime_generation != active_runtime_generation_
            || colors->first_copper_instruction_address != 0x00d20028U
            || colors->next_copper_instruction_address != 0x00d200a0U
            || colors->low_color_instruction_count != 16U
            || colors->high_color_instruction_count != 4U
            || colors->total_copper_instruction_count != 21U
            || colors->bitmap_depth != 4U || colors->color_map_entry_count != 20U
            || colors->resume_address != 0xfcc934U) {
            result.error = "Deuteros MakeVPort color-list translation rejected the exact display preimage";
            return result;
        }
        const auto geometry = prefix.translate_bootstrap_geometry_prefix(memory);
        if (!geometry || geometry->runtime_generation != active_runtime_generation_
            || geometry->first_instruction_address != 0xfcd010U
            || geometry->next_instruction_address != 0xfcd2a4U
            || geometry->copied_width != 320U || geometry->copied_height != 0U
            || geometry->mode_mask != 0x4000U || geometry->line_mask != 0x000fU
            || geometry->opaque_graphics_library_token
                != boundary.eon_graphics_library_token
            || geometry->graphics_max_display_column != 0x01d1U
            || geometry->graphics_min_display_column != 0x0030U
            || geometry->computed_display_height != 0x0081U
            || geometry->bitmap_bytes_per_row != 40U
            || geometry->computed_horizontal_delta != -32
            || geometry->symbolic_result_vertical_position != 56U
            || geometry->symbolic_result_horizontal_position != 208U
            || geometry->symbolic_result_width != 320U
            || geometry->symbolic_result_height != 0U
            || geometry->symbolic_result_rasinfo_offset != 0U
            || geometry->symbolic_result_row_bytes != 40U
            || geometry->geometry_source_begin != 0xfcd010U
            || geometry->geometry_source_end != 0xfcd2a4U
            || geometry->geometry_source_sha256
                != "1c8e89a6e9edddad0f7ac00468f8dc37bd914da57f1a8ba32cf56341ab870961"
            || geometry->graphics_init_source_begin != 0xfcaea4U
            || geometry->graphics_init_source_end != 0xfcaeb0U
            || geometry->stack_frame_address_materialized
            || geometry->rom_slice_sha256
                != "fbb257909f9200f2b603df5a058d92d26c5d4ccc7160dc6c3bdb87ecb4e3f32c") {
            result.error = "Deuteros MakeVPort geometry prefix rejected its exact source preimage";
            return result;
        }
        const auto caller = prefix.translate_bootstrap_caller_prefix(memory, *geometry);
        if (!caller || caller->runtime_generation != active_runtime_generation_
            || caller->first_instruction_address != 0xfcc946U
            || caller->next_instruction_address != 0xfcc9a2U
            || caller->copper_instruction_address != 0x00d200a0U
            || caller->next_copper_instruction_address != 0x00d200a6U
            || caller->partial_copper_first_word != 0U
            || caller->partial_copper_second_word != 0x2f00U
            || caller->custom_chip_base != 0x00dff000U
            || caller->geometry_height != 0U || caller->copper_register != 0xf08eU
            || caller->copper_value != 0x0500U
            || caller->source_begin != 0xfcc946U || caller->source_end != 0xfcc9a2U
            || caller->source_sha256
                != "ce1bfc1a50073999d629997883ac22674a869daae77aab0d310c5eab59456087") {
            result.error = "Deuteros MakeVPort caller prefix rejected its source-bound geometry return";
            return result;
        }
        const auto symbolic_timing_word =
            prefix.translate_bootstrap_symbolic_timing_word(memory);
        if (!symbolic_timing_word
            || symbolic_timing_word->runtime_generation != active_runtime_generation_
            || symbolic_timing_word->instruction_address != 0xfcc9a2U
            || symbolic_timing_word->copper_word_address != 0x00d200aaU
            || symbolic_timing_word->possible_values
                != std::array<std::uint16_t, 2>{{0x0200U, 0x0302U}}
            || symbolic_timing_word->next_instruction_address != 0xfcc9c0U
            || symbolic_timing_word->next_copper_instruction_address != 0x00d200acU
            || symbolic_timing_word->current_node_instruction_count != 23U
            || symbolic_timing_word->source_begin != 0xfcc9a2U
            || symbolic_timing_word->source_end != 0xfcc9c0U
            || symbolic_timing_word->source_sha256
                != "f2f2fe1db329a1ad48ede990e77f70c0212161d4de54ac6b2ee9843d23cb6fd0"
            || symbolic_timing_word->copper_node_helper_begin != 0xfca358U
            || symbolic_timing_word->copper_node_helper_end != 0xfca3eeU
            || symbolic_timing_word->copper_node_helper_sha256
                != "31c75b5db29ae12d9a29b3b816f790fec90b2df2170e1ad38a8ebf6ea01d6a44"
            || memory.read_byte({NativeRuntimeAddressSpace::linear, std::nullopt,
                    symbolic_timing_word->copper_word_address})) {
            result.error = "Deuteros MakeVPort timing word did not remain an explicit source-bound uncertainty";
            return result;
        }
        const auto symbolic_mode_prefix =
            prefix.translate_bootstrap_symbolic_mode_prefix(memory);
        if (!symbolic_mode_prefix
            || symbolic_mode_prefix->runtime_generation != active_runtime_generation_
            || symbolic_mode_prefix->possible_graphics_timing_modes
                != std::array<std::uint16_t, 2>{{0x0200U, 0x0302U}}
            || symbolic_mode_prefix->copper_instruction_address != 0x00d200acU
            || symbolic_mode_prefix->copper_first_word != 0U
            || symbolic_mode_prefix->copper_second_word != 0xf104U
            || symbolic_mode_prefix->copper_third_word
                != symbolic_mode_prefix->viewport_sprite_priority
            || symbolic_mode_prefix->current_node_instruction_count != 24U
            || symbolic_mode_prefix->next_instruction_address != 0xfcca42U
            || symbolic_mode_prefix->source_begin != 0xfcc9c0U
            || symbolic_mode_prefix->source_end != 0xfcca42U
            || symbolic_mode_prefix->source_sha256
                != "af563f6c58eae2b4e8b8e31e2fd7474e916a87e7f3e576c1f75fc2f34dc60e03"
            || memory.read_byte({NativeRuntimeAddressSpace::linear, std::nullopt,
                    symbolic_timing_word->copper_word_address})) {
            result.error = "Deuteros MakeVPort symbolic mode prefix rejected its exact ViewPort preimage";
            return result;
        }
        auto translated_receipt = *receipt;
        translated_receipt.next_copper_instruction_address =
            colors->next_copper_instruction_address;
        translated_receipt.copper_instructions_emitted =
            colors->total_copper_instruction_count + 3U;
        translated_receipt.next_copper_instruction_address = 0x00d200b2U;
        translated_receipt.resume_address = symbolic_mode_prefix->next_instruction_address;
        *native_runtime_memory_ = std::move(memory);
        deuteros_amiga_makevport_prefix_ = std::move(prefix);
        boundary.bootstrap_makevport_prefix_receipt = translated_receipt;
        boundary.bootstrap_makevport_color_range_receipt = *colors;
        boundary.bootstrap_makevport_geometry_prefix_receipt = *geometry;
        boundary.bootstrap_makevport_caller_prefix_receipt = *caller;
        boundary.bootstrap_makevport_symbolic_timing_word_receipt = *symbolic_timing_word;
        boundary.bootstrap_makevport_symbolic_mode_prefix_receipt = *symbolic_mode_prefix;
        boundary.bootstrap_makevport_resume_address = symbolic_mode_prefix->next_instruction_address;
        result.accepted = true;
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}

DeuterosAmigaComputedDispatchResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_computed_dispatch(
    const DeuterosAmigaComputedDispatchObservation observation) {
    DeuterosAmigaComputedDispatchResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !deuteros_amiga_->title_stage_session()
        || !native_runtime_memory_) {
        result.error = "Deuteros computed dispatch requires the active owned title stage";
        return result;
    }
    try {
        if (!deuteros_amiga_title_computed_dispatch_) {
            deuteros_amiga_title_computed_dispatch_.emplace(
                deuteros_amiga_->title_stage_session()->original_bytes());
        }
        return deuteros_amiga_title_computed_dispatch_->observe_call(
            observation, *native_runtime_memory_);
    } catch (const std::exception& error) {
        result.error = error.what();
        return result;
    }
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_title_program_entry() {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_program_entry
        || !deuteros_amiga_ || !native_runtime_memory_ || !deuteros_amiga_title_program_entry_) {
        result.error = "Deuteros title program entry requires its active owned boundary";
        return result;
    }
    try {
        auto memory = *native_runtime_memory_;
        const auto profile=deuteros_amiga_title_program_entry_->profile;
        const auto prefix=deuteros_amiga_->prepare_title_stage_program_entry(profile);
        if(!prefix)
            throw std::runtime_error("Deuteros profile-selected title entry was not admitted");
        const auto transaction=prepare_deuteros_amiga_title_program_entry_transaction(
            *deuteros_amiga_title_program_entry_,memory,*prefix);
        if(!transaction.accepted)throw std::runtime_error(transaction.error);
        const auto applied=memory.apply(transaction.batch);
        if(!applied.accepted)throw std::runtime_error(applied.error);

        const auto committed=deuteros_amiga_->commit_title_stage_program_entry(profile);
        if(!committed||*committed!=*prefix)
            throw std::runtime_error("Deuteros profile-selected title entry changed before commit");
        *native_runtime_memory_=std::move(memory);
        deuteros_amiga_title_program_entry_.reset();
        deuteros_amiga_title_load_copy_.reset();
        deuteros_amiga_title_load_copy_generation_=0;
        deuteros_amiga_title_command_generation_=0;
        deuteros_amiga_title_service_setup_plan_.reset();
        deuteros_amiga_title_second_service_plan_.reset();
        deuteros_amiga_title_third_service_plan_.reset();
        deuteros_amiga_title_fourth_service_plan_.reset();
        deuteros_amiga_title_fifth_service_plan_.reset();
        deuteros_amiga_title_planar_base_.reset();
        deuteros_amiga_title_planar_generation_=0;
        deuteros_amiga_title_planar_surface_.reset();
        deuteros_amiga_title_display_trace_.reset();
        session_snapshot_=make_runtime_session_snapshot(*active_,RuntimeSessionKind::deuteros_amiga_title_stage);
        result.accepted=true;
    }catch(const std::exception& e){result.error=e.what();}
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
EON_DEUTEROS_TITLE_ADVANCE(observe_deuteros_amiga_title_callback_exec_return(const DeuterosAmigaObservedCallbackExecReturn o), deuteros_amiga_->observe_title_callback_exec_return(o))
#undef EON_DEUTEROS_TITLE_ADVANCE

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_title_native_privilege_transition() {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !deuteros_amiga_->title_stage_session()) {
        result.error = "Deuteros native privilege translation requires the active owned title stage";
        return result;
    }
    try {
        const auto transition = deuteros_amiga_->execute_title_native_privilege_transition();
        if (!transition) {
            result.error = "Deuteros native privilege translation did not match the owned Exec boundary";
            return result;
        }
        result.accepted = true;
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_callback_producer(
    const DeuterosAmigaObservedTitleCallbackProducer observation) {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !native_runtime_memory_) {
        result.error = "Deuteros callback producer requires an active owned title stage";
        return result;
    }
    try {
        const auto read_word = [this](const std::uint32_t address)
            -> std::optional<std::uint16_t> {
            const auto bytes = native_runtime_memory_->read_linear_range(address, 2);
            if (!bytes || bytes->size() != 2) return std::nullopt;
            return static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>((*bytes)[0]) << 8U) | (*bytes)[1]);
        };
        const auto& current_memory = *native_runtime_memory_;
        const auto current_mirror = current_memory.read_byte(
            {NativeRuntimeAddressSpace::linear, std::nullopt, 0x1ef2e});
        const auto current_selector = read_word(0x1ee0e);
        const auto current_queue = current_memory.read_linear_range(0x1eec0, 21);
        const auto current_pending = read_word(0x1eed6);
        if (!current_mirror || !current_selector || !current_queue || !current_pending
            || current_queue->size() != observation.queue_before.size()
            || *current_mirror != observation.mirrored_event_before
            || *current_selector != observation.selector_word_before
            || *current_pending != observation.pending_count_before
            || !std::equal(current_queue->begin(), current_queue->end(),
                observation.queue_before.begin())) {
            result.error = "Deuteros callback producer preimage differs from owned title memory";
            return result;
        }

        const auto* owned_title_stage = deuteros_amiga_->title_stage_session()
            ? &*deuteros_amiga_->title_stage_session() : nullptr;
        if (!owned_title_stage) {
            result.error = "Deuteros callback producer requires its owned title-stage session";
            return result;
        }
        auto preview_session = *owned_title_stage;
        const auto plan = preview_session.observe_callback_producer(observation);
        if (!plan) {
            result.error = "Deuteros callback producer did not match its registered boundary";
            return result;
        }

        NativeRuntimeEffectBatch batch{
            "deuteros-amiga-title-callback-producer-"
                + std::to_string(observation.return_trace_sequence), true, {}};
        const auto& producer = plan->producer;
        batch.effects = {
            {1, {NativeRuntimeAddressSpace::linear, std::nullopt,
                    producer.mirrored_event_address},
                MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian,
                producer.mirrored_event_value},
            {2, {NativeRuntimeAddressSpace::linear, std::nullopt,
                    producer.selector_word_address},
                MemoryTransferElementWidth::word, NativeRuntimeByteOrder::big_endian,
                producer.selector_word_value},
            {3, {NativeRuntimeAddressSpace::linear, std::nullopt,
                    producer.destination_address + producer.destination_offset},
                MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian,
                producer.queued_byte},
            {4, {NativeRuntimeAddressSpace::linear, std::nullopt,
                    producer.pending_count_address},
                MemoryTransferElementWidth::word, NativeRuntimeByteOrder::big_endian,
                producer.pending_count_after}};
        auto next_memory = *native_runtime_memory_;
        const auto applied = next_memory.apply(batch);
        if (!applied.accepted) {
            result.error = "Deuteros callback producer memory application rejected: "
                + applied.error;
            return result;
        }

        const auto committed = deuteros_amiga_->observe_title_callback_producer(observation);
        if (!committed || *committed != *plan) {
            result.error = "Deuteros callback producer changed before commit";
            return result;
        }
        *native_runtime_memory_ = std::move(next_memory);
        result.accepted = true;
    } catch (const std::exception& error) {
        result.error = std::string("Deuteros callback producer rejected: ") + error.what();
    }
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_move_sprite(
    const DeuterosAmigaObservedTitleMoveSpriteCall observation) {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !native_runtime_memory_) {
        result.error = "Deuteros MoveSprite observation requires an active owned title stage";
        return result;
    }
    try {
        const auto read_word = [this](const std::uint32_t address)
            -> std::optional<std::uint16_t> {
            const auto bytes = native_runtime_memory_->read_linear_range(address, 2);
            if (!bytes || bytes->size() != 2) return std::nullopt;
            return static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>((*bytes)[0]) << 8U) | (*bytes)[1]);
        };
        const auto read_long = [this](const std::uint32_t address)
            -> std::optional<std::uint32_t> {
            const auto bytes = native_runtime_memory_->read_linear_range(address, 4);
            if (!bytes || bytes->size() != 4) return std::nullopt;
            return (static_cast<std::uint32_t>((*bytes)[0]) << 24U)
                | (static_cast<std::uint32_t>((*bytes)[1]) << 16U)
                | (static_cast<std::uint32_t>((*bytes)[2]) << 8U)
                | static_cast<std::uint32_t>((*bytes)[3]);
        };
        const std::array<std::pair<std::uint32_t, std::uint16_t>, 6> expected_inputs{{
            {0x1ee10, observation.input.x_delta},
            {0x1ffca, observation.input.x_min},
            {0x1ffcc, observation.input.x_max},
            {0x1ee12, observation.input.y_delta},
            {0x1ffd0, observation.input.y_min},
            {0x1ffd2, observation.input.y_max}}};
        for (const auto& [address, expected] : expected_inputs) {
            const auto current = read_word(address);
            if (!current || *current != expected) {
                result.error = "Deuteros MoveSprite preimage differs from owned title memory";
                return result;
            }
        }

        const auto* owned_title_stage = deuteros_amiga_->title_stage_session()
            ? &*deuteros_amiga_->title_stage_session() : nullptr;
        if (!owned_title_stage) {
            result.error = "Deuteros MoveSprite observation requires its owned title-stage session";
            return result;
        }
        auto preview_session = *owned_title_stage;
        const auto plan = preview_session.observe_callback_move_sprite(observation);
        if (!plan) {
            result.error = "Deuteros MoveSprite samples did not match the registered callback route";
            return result;
        }
        const auto owned_graphics_base = read_long(0x12fec);
        if (!owned_graphics_base || *owned_graphics_base == 0
            || observation.a6 != *owned_graphics_base) {
            result.error = "Deuteros MoveSprite A6 differs from the owned graphics.library base";
            return result;
        }
        // The native host callback has already executed the hash-locked
        // coordinate update before it stops at the external graphics.library
        // vector. Validate the routine's computed postimage here; comparing
        // these cells with the callback's entry coordinates would reject a
        // real mouse move (or tempt a caller to apply the delta twice).
        const auto current_x = read_word(0x1ffc8);
        const auto current_y = read_word(0x1ffce);
        if (!current_x || !current_y
            || *current_x != plan->expected.x_after_clamp
            || *current_y != plan->expected.y_after_clamp) {
            result.error = "Deuteros MoveSprite postimage differs from owned clamped coordinates";
            return result;
        }
        const auto committed = deuteros_amiga_->observe_title_callback_move_sprite(observation);
        if (!committed || *committed != *plan) {
            result.error = "Deuteros MoveSprite observation changed before commit";
            return result;
        }
        result.move_sprite = *plan;
        result.accepted = true;
    } catch (const std::exception& error) {
        result.error = std::string("Deuteros MoveSprite observation rejected: ") + error.what();
    }
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_response_queue(
    const DeuterosAmigaObservedTitleResponseQueue observation) {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !native_runtime_memory_) {
        result.error = "Deuteros response queue requires an active owned title stage";
        return result;
    }
    try {
        constexpr std::uint32_t queue_address = 0x1eec0;
        constexpr std::uint32_t pending_address = 0x1eed6;
        const auto queue = native_runtime_memory_->read_linear_range(queue_address, 21);
        const auto pending_bytes = native_runtime_memory_->read_linear_range(pending_address, 2);
        if (!queue || queue->size() != 21 || !pending_bytes || pending_bytes->size() != 2) {
            throw std::runtime_error("Deuteros response queue is not present in owned title memory");
        }
        const auto pending = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>((*pending_bytes)[0]) << 8U)
            | (*pending_bytes)[1]);
        if (!std::equal(queue->begin(), queue->end(),
                observation.queue_snapshots[0].begin())
            || pending != observation.pending_counts[0]) {
            throw std::runtime_error(
                "Deuteros response-queue capture preimage differs from owned title memory");
        }

        const auto* owned_title_stage = deuteros_amiga_->title_stage_session()
            ? &*deuteros_amiga_->title_stage_session() : nullptr;
        if (!owned_title_stage) {
            throw std::runtime_error("Deuteros response queue requires its owned title-stage session");
        }
        auto preview_session = *owned_title_stage;
        const auto plan = preview_session.observe_response_queue(observation);
        if (!plan) {
            throw std::runtime_error("Deuteros response-queue observations did not match the owned boundary");
        }

        NativeRuntimeEffectBatch batch{
            "deuteros-amiga-title-response-queue-"
                + std::to_string(observation.sequence), true, {}};
        batch.effects.reserve(21);
        for (std::uint32_t index = 0; index < 20; ++index) {
            batch.effects.push_back({index + 1U,
                {NativeRuntimeAddressSpace::linear, std::nullopt, queue_address + index},
                MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian,
                plan->dequeue.shifted_bytes[index]});
        }
        batch.effects.push_back({21,
            {NativeRuntimeAddressSpace::linear, std::nullopt, pending_address},
            MemoryTransferElementWidth::word, NativeRuntimeByteOrder::big_endian,
            plan->dequeue.pending_count_after});

        auto next_memory = *native_runtime_memory_;
        const auto applied = next_memory.apply(batch);
        if (!applied.accepted) {
            throw std::runtime_error("Deuteros response-queue memory application rejected: "
                + applied.error);
        }
        const auto committed = deuteros_amiga_->observe_title_response_queue(observation);
        if (!committed || *committed != *plan) {
            throw std::runtime_error("Deuteros response queue changed before commit");
        }
        *native_runtime_memory_ = std::move(next_memory);
        result.response_queue = *plan;
        result.accepted = true;
    } catch (const std::exception& error) {
        result.error = std::string("Deuteros response queue rejected: ") + error.what();
    }
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_display_base(
    const DeuterosAmigaObservedDisplayBaseRead o){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::deuteros_amiga_title_stage
        ||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()||!native_runtime_memory_){
        result.error="Deuteros display setup requires active owned memory";return result;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto plan=pending.observe_display_base_and_advance(o);
        if(!plan){result.error="Deuteros display setup did not match boundary";return result;}
        if(o.observed_value==0||(o.observed_value&1U)!=0
            ||plan->clear_write_width!=4||plan->clear_byte_count!=0x7d00
            ||o.observed_value>0x1000000U-plan->clear_byte_count)
            throw std::runtime_error("Deuteros display clear is outside aligned native memory");
        auto memory=*native_runtime_memory_;
        for(std::uint32_t i=0;i<4;++i){
            const auto existing=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,o.source_address+i});
            if(existing&&*existing!=((o.observed_value>>(24U-i*8U))&0xffU))
                throw std::runtime_error("Deuteros display-base observation contradicts owned memory");
        }
        NativeRuntimeEffectBatch batch{"deuteros-amiga-title-display-setup",true,{}};
        batch.effects.reserve(30U+plan->clear_byte_count/4U);
        const auto add=[&](std::uint32_t address,MemoryTransferElementWidth width,std::uint32_t value){
            batch.effects.push_back({batch.effects.size()+1,
                {NativeRuntimeAddressSpace::linear,std::nullopt,address},width,
                NativeRuntimeByteOrder::big_endian,value});
        };
        // The source cell is admitted by the exact read observation; all
        // remaining writes below are the hash-verified local instructions.
        add(o.source_address,MemoryTransferElementWidth::longword,o.observed_value);
        for(const auto address:plan->base_pointer_destinations)
            add(address,MemoryTransferElementWidth::longword,o.observed_value);
        add(plan->palette_count_address,MemoryTransferElementWidth::word,
            plan->palette_count);
        add(plan->palette_pointer_address,MemoryTransferElementWidth::longword,
            plan->palette_destination_address);
        for(std::size_t i=0;i<plan->palette_words.size();++i)
            add(plan->palette_destination_address+static_cast<std::uint32_t>(i)*2U,
                MemoryTransferElementWidth::word,plan->palette_words[i]);
        add(plan->derived_pointer_destination_address,MemoryTransferElementWidth::longword,
            plan->derived_pointer_value);
        add(plan->cleared_word_address,MemoryTransferElementWidth::word,0);
        for(const auto address:plan->caller_pointer_copy_destinations)
            add(address,MemoryTransferElementWidth::longword,o.observed_value);
        for(std::uint32_t i=0;i<plan->clear_byte_count;i+=4)
            add(plan->clear_destination+i,MemoryTransferElementWidth::longword,0);
        const auto applied=memory.apply(batch);
        if(!applied.accepted){result.error=applied.error;return result;}
        if(!deuteros_amiga_->observe_title_display_base(o)){
            result.error="Deuteros display setup disappeared before commit";return result;
        }
        *native_runtime_memory_=std::move(memory);result.accepted=true;
    }catch(const std::exception&e){result.error=e.what();}
    return result;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::provide_deuteros_amiga_title_native_graphics_library() {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !deuteros_amiga_->title_stage_session()
        || active_runtime_generation_ == 0) {
        result.error = "Deuteros native graphics service requires the active title stage";
        return result;
    }
    try {
        const auto& setup = deuteros_amiga_->title_stage_session()->graphics_setup();
        if (!deuteros_amiga_graphics_library_compatibility_) {
            deuteros_amiga_graphics_library_compatibility_.emplace(
                active_runtime_generation_);
        }
        const auto service_return =
            deuteros_amiga_graphics_library_compatibility_->open_title_graphics_library(
                setup.entry_address, setup.library_name_address, setup.exec_base_address,
                setup.entry_address + 0x0cU, setup.exec_vector,
                setup.entry_address + 0x10U, setup.library_name, 0);
        if (!service_return) {
            result.error = "Deuteros native graphics service rejected the exact OpenLibrary request";
            return result;
        }
        if (!deuteros_amiga_->provide_title_native_open_library_service_return(
                *service_return, active_runtime_generation_)) {
            result.error = "Deuteros native graphics service return missed its source boundary";
            return result;
        }
        result.accepted = true;
    } catch (const std::exception& exception) {
        result.error = exception.what();
    }
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::provide_deuteros_amiga_title_native_display_surface() {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !deuteros_amiga_->title_stage_session()
        || !native_runtime_memory_ || active_runtime_generation_ == 0
        || !deuteros_amiga_graphics_library_compatibility_
        || !deuteros_amiga_graphics_library_compatibility_->service_opened()) {
        result.error = "Deuteros native display surface requires its active graphics service";
        return result;
    }
    try {
        auto surface_session = deuteros_amiga_native_display_surface_.value_or(
            DeuterosAmigaNativeDisplaySurfaceSession(active_runtime_generation_));
        const auto surface = surface_session.allocate_title_surface(
            native_runtime_memory_->checkpoint());
        if (!surface) {
            result.error = "Deuteros native display arena has no free title-plane range";
            return result;
        }

        auto pending = *deuteros_amiga_->title_stage_session();
        const auto plan = pending.provide_native_display_surface(
            *surface, active_runtime_generation_);
        if (!plan || !plan->native_surface
            || plan->display_base_value != surface->base_address
            || plan->display_base_source_address != 0x12ff4
            || plan->clear_destination != surface->base_address
            || plan->clear_byte_count != surface->byte_count
            || plan->clear_write_width != 4
            || (surface->base_address & 1U) != 0
            || surface->base_address > 0x01000000U - surface->byte_count) {
            result.error = "Deuteros native display plan escaped its exact surface allocation";
            return result;
        }

        auto memory = *native_runtime_memory_;
        for (std::uint32_t index = 0; index < 4; ++index) {
            const auto existing = memory.read_byte({NativeRuntimeAddressSpace::linear,
                std::nullopt, plan->display_base_source_address + index});
            const auto expected = static_cast<std::uint8_t>(
                surface->base_address >> ((3U - index) * 8U));
            if (existing && *existing != expected) {
                result.error = "Deuteros native display base contradicts initialized runtime memory";
                return result;
            }
        }

        NativeRuntimeEffectBatch batch{
            "deuteros-amiga-native-display-setup-"
                + std::to_string(active_runtime_generation_), true, {}};
        batch.effects.reserve(32U + plan->clear_byte_count / 4U);
        const auto add = [&](const std::uint32_t address,
            const MemoryTransferElementWidth width, const std::uint32_t value) {
            batch.effects.push_back({batch.effects.size() + 1U,
                {NativeRuntimeAddressSpace::linear, std::nullopt, address}, width,
                NativeRuntimeByteOrder::big_endian, value});
        };
        add(plan->display_base_source_address, MemoryTransferElementWidth::longword,
            surface->base_address);
        for (const auto address : plan->base_pointer_destinations)
            add(address, MemoryTransferElementWidth::longword, surface->base_address);
        add(plan->palette_count_address, MemoryTransferElementWidth::word,
            plan->palette_count);
        add(plan->palette_pointer_address, MemoryTransferElementWidth::longword,
            plan->palette_destination_address);
        for (std::size_t index = 0; index < plan->palette_words.size(); ++index) {
            add(plan->palette_destination_address + static_cast<std::uint32_t>(index) * 2U,
                MemoryTransferElementWidth::word, plan->palette_words[index]);
        }
        add(plan->derived_pointer_destination_address, MemoryTransferElementWidth::longword,
            plan->derived_pointer_value);
        add(plan->cleared_word_address, MemoryTransferElementWidth::word, 0);
        for (const auto address : plan->caller_pointer_copy_destinations)
            add(address, MemoryTransferElementWidth::longword, surface->base_address);
        for (std::uint32_t offset = 0; offset < plan->clear_byte_count; offset += 4U)
            add(plan->clear_destination + offset, MemoryTransferElementWidth::longword, 0);

        const auto applied = memory.apply(batch);
        if (!applied.accepted) {
            result.error = "Deuteros native display memory commit was rejected: " + applied.error;
            return result;
        }
        if (!deuteros_amiga_->provide_title_native_display_surface(
                *surface, active_runtime_generation_)) {
            result.error = "Deuteros native display surface missed its source boundary";
            return result;
        }
        *native_runtime_memory_ = std::move(memory);
        deuteros_amiga_native_display_surface_ = std::move(surface_session);
        result.accepted = true;
    } catch (const std::exception& exception) {
        result.error = exception.what();
    }
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::initialize_deuteros_amiga_title_native_display() {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !deuteros_amiga_->title_stage_session()
        || active_runtime_generation_ == 0 || !deuteros_amiga_native_display_surface_
        || !deuteros_amiga_native_display_surface_->allocation()) {
        result.error = "Deuteros native display initialization requires its allocated title surface";
        return result;
    }
    const auto& surface = *deuteros_amiga_native_display_surface_->allocation();
    auto pending = *deuteros_amiga_->title_stage_session();
    const auto initialization = pending.initialize_native_display(
        active_runtime_generation_, surface.base_address, surface.byte_count);
    if (!initialization || initialization->runtime_generation != active_runtime_generation_
        || initialization->base_address != surface.base_address
        || initialization->byte_count != surface.byte_count
        || initialization->source_entry_address != 0x40498
        || initialization->custom_register_offsets
            != std::array<std::uint16_t, 4>{0x40, 0x42, 0x9a, 0x96}
        || initialization->custom_register_values
            != std::array<std::uint16_t, 4>{0x7fff, 0x7fff, 0xc000, 0x87ff}) {
        result.error = "Deuteros native display initialization did not match its hash-locked setup";
        return result;
    }
    if (!deuteros_amiga_->initialize_title_native_display(active_runtime_generation_,
            surface.base_address, surface.byte_count)) {
        result.error = "Deuteros native display boundary changed before initialization commit";
        return result;
    }
    deuteros_amiga_native_display_initialization_ = *initialization;
    result.accepted = true;
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::register_deuteros_amiga_title_native_input_handler() {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !deuteros_amiga_->title_stage_session()
        || !native_runtime_memory_
        || active_runtime_generation_ == 0 || !deuteros_amiga_native_display_initialization_
        || deuteros_amiga_native_input_handler_registration_
        || deuteros_amiga_trackdisk_compatibility_
        || deuteros_amiga_exec_compatibility_) {
        result.error = "Deuteros native input handler requires initialized title display";
        return result;
    }
    auto compatibility = deuteros_amiga_input_device_compatibility_;
    const auto registration = compatibility.register_title_handler_native(
        deuteros_amiga_->title_stage_session()->callback_registration_profile(),
        active_runtime_generation_);
    if (!registration || registration->runtime_generation != active_runtime_generation_
        || registration->service_sequence != 3
        || registration->add_handler_call_address != 0x1f04e
        || registration->callback_address != 0x1f056
        || registration->origin != DeuterosAmigaInputHandlerOrigin::native_compatibility) {
        result.error = "Deuteros native input-device compatibility rejected the exact handler";
        return result;
    }

    if (!deuteros_amiga_->accept_title_native_input_handler_registration(*registration)) {
        result.error = "Deuteros native handler registration missed its title-stage boundary";
        return result;
    }
    deuteros_amiga_input_device_compatibility_ = std::move(compatibility);
    deuteros_amiga_native_input_handler_registration_ = *registration;
    result.accepted = true;
    return result;
}

DeuterosAmigaNativeTrackdiskCoordinatorResult
ReleaseRuntimeCoordinator::service_deuteros_amiga_title_trackdisk_read() {
    DeuterosAmigaNativeTrackdiskCoordinatorResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !deuteros_amiga_trackdisk_compatibility_
        || !deuteros_amiga_trackdisk_unit0_binding_
        || !deuteros_amiga_exec_compatibility_graph_
        || !deuteros_amiga_exec_unit0_open_ || !native_runtime_memory_) {
        result.error = "Deuteros native ETD_READ requires the active title trackdisk service";
        return result;
    }

    try {
        if (active_runtime_generation_ == 0
            || deuteros_amiga_trackdisk_compatibility_->runtime_generation()
                != active_runtime_generation_
            || deuteros_amiga_trackdisk_unit0_binding_->runtime_generation
                != active_runtime_generation_
            || deuteros_amiga_exec_compatibility_graph_->runtime_generation
                != active_runtime_generation_
            || deuteros_amiga_exec_unit0_open_->runtime_generation
                != active_runtime_generation_
            || deuteros_amiga_exec_unit0_open_->origin
                != DeuterosAmigaExecCompatibilityOrigin::eon_native_compatibility) {
            result.error = "Deuteros native ETD_READ token belongs to a different runtime generation";
            return result;
        }
        auto trackdisk = *deuteros_amiga_trackdisk_compatibility_;
        auto memory = *native_runtime_memory_;
        auto pending = *deuteros_amiga_->title_stage_session();
        const auto ordinal = deuteros_amiga_->next_native_title_load_doio_ordinal();
        if (!ordinal || *ordinal != trackdisk.next_title_read_ordinal()) {
            result.error = "Deuteros native ETD_READ is not the next owned $20934 request";
            return result;
        }

        // These are the exact four hash-bound IOExtTD operands recovered from
        // the title loader's $20934 loop. Keep the game's request pointer and
        // staging pointer as read-only preconditions; Eon writes only the
        // command fields the local loop sets immediately before DoIO.
        constexpr std::uint32_t request = 0x205e4U;
        constexpr std::uint32_t request_pointer_cell = 0x206a0U;
        constexpr std::uint32_t staging_pointer_cell = 0x206a4U;
        const auto request_pointer = memory.read_linear_range(request_pointer_cell, 4);
        const auto staging_pointer = memory.read_linear_range(staging_pointer_cell, 4);
        if (!request_pointer || !staging_pointer) {
            result.error = "Deuteros native ETD_READ lost its owned request/staging pointers";
            return result;
        }
        const auto read_be32 = [](const std::vector<std::uint8_t>& bytes) {
            return (static_cast<std::uint32_t>(bytes[0]) << 24U)
                | (static_cast<std::uint32_t>(bytes[1]) << 16U)
                | (static_cast<std::uint32_t>(bytes[2]) << 8U)
                | static_cast<std::uint32_t>(bytes[3]);
        };
        const auto io_data = read_be32(*staging_pointer);
        const auto request_device = memory.read_linear_range(request + 0x14U, 4);
        const auto request_unit = memory.read_linear_range(request + 0x18U, 4);
        if (read_be32(*request_pointer) != request || io_data == 0
            || io_data > 0x01000000U - 0x1600U
            || !request_device || !request_unit
            || read_be32(*request_device) != deuteros_amiga_exec_unit0_open_->device_address
            || read_be32(*request_unit) != deuteros_amiga_exec_unit0_open_->unit_address) {
            result.error = "Deuteros native ETD_READ pointer cells do not match the owned source path";
            return result;
        }
        NativeRuntimeEffectBatch request_batch{
            "deuteros-amiga-native-etd-read-request-"
                + std::to_string(active_runtime_generation_) + "-" + std::to_string(*ordinal),
            true, {}};
        const auto add = [&](const std::uint32_t address,
            const MemoryTransferElementWidth width, const std::uint32_t value) {
            request_batch.effects.push_back({request_batch.effects.size() + 1U,
                {NativeRuntimeAddressSpace::linear, std::nullopt, address}, width,
                NativeRuntimeByteOrder::big_endian, value});
        };
        add(request + 0x1cU, MemoryTransferElementWidth::word, 0x8002U);
        add(request + 0x1eU, MemoryTransferElementWidth::byte, 0U);
        add(request + 0x24U, MemoryTransferElementWidth::longword, 0x1600U);
        add(request + 0x28U, MemoryTransferElementWidth::longword, io_data);
        add(request + 0x2cU, MemoryTransferElementWidth::longword,
            0x14000U + *ordinal * 0x1600U);
        add(request + 0x30U, MemoryTransferElementWidth::longword, 0xffffffffU);
        const auto prepared = memory.apply(request_batch);
        if (!prepared.accepted) {
            result.error = prepared.error;
            return result;
        }
        const auto read = trackdisk.do_title_read(0x20934, 0x205e4,
            deuteros_amiga_trackdisk_unit0_binding_->opaque_device_token,
            deuteros_amiga_->system_disk_image(), memory);
        if (!read) {
            result.error = "Deuteros native ETD_READ request did not match owned memory or Disk 1";
            return result;
        }
        if (read->io_error != 0 || read->actual_bytes != 0x1600U) {
            result.error = "Deuteros native ETD_READ did not complete its exact source chunk";
            return result;
        }
        const auto request_error = memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt, request + 0x1fU});
        const auto staging = memory.read_linear_range(read->buffer_address,
            read->actual_bytes);
        const auto destination = 0x26cc0U + *ordinal * 0x1600U;
        if (!request_error || *request_error != 0 || !staging
            || !memory.read_linear_range(destination, read->actual_bytes)) {
            result.error = "Deuteros native ETD_READ completion or loader copy range is not owned";
            return result;
        }
        NativeRuntimeEffectBatch copy_batch{
            "deuteros-amiga-native-etd-read-copy-"
                + std::to_string(active_runtime_generation_) + "-" + std::to_string(*ordinal),
            true, {}};
        copy_batch.effects.reserve(staging->size());
        for (std::size_t index = 0; index < staging->size(); ++index) {
            copy_batch.effects.push_back({index + 1U,
                {NativeRuntimeAddressSpace::linear, std::nullopt,
                    destination + static_cast<std::uint32_t>(index)},
                MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian,
                (*staging)[index]});
        }
        const auto copied = memory.apply(copy_batch);
        if (!copied.accepted) {
            result.error = copied.error;
            return result;
        }
        if (!pending.complete_native_load_doio_ordinal(*ordinal)) {
            result.error = "Deuteros native ETD_READ ordinal could not commit to the bounded title gate";
            return result;
        }
        if (!deuteros_amiga_->complete_native_title_load_doio_ordinal(*ordinal)) {
            result.error = "Deuteros native ETD_READ title gate changed before commit";
            return result;
        }
        *deuteros_amiga_trackdisk_compatibility_ = std::move(trackdisk);
        *native_runtime_memory_ = std::move(memory);
        result.read = *read;
        result.accepted = true;
    } catch (const std::exception& e) {
        result.error = e.what();
    }
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_title_post_open_library_local_path(){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::deuteros_amiga_title_stage
        ||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()||!native_runtime_memory_){
        result.error="Deuteros post-OpenLibrary path requires active title memory";return result;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto plan=pending.advance_post_open_library_local_path();
        if(!plan){result.error="Deuteros post-OpenLibrary path did not match boundary";return result;}
        auto memory=*native_runtime_memory_;
        const auto increment_high=memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt,plan->increment_word_address});
        const auto increment_low=memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt,plan->increment_word_address+1U});
        if(!increment_high||!increment_low)
            throw std::runtime_error("Deuteros graphics setup counter is not owned");
        const auto prior=static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(*increment_high)<<8U)|*increment_low);
        const auto incremented=static_cast<std::uint16_t>(prior+plan->increment_delta);
        NativeRuntimeEffectBatch batch{"deuteros-amiga-graphics-library-base",true,{{
            1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->observed_result_store_address},
            MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,
            plan->observed_result_store_value},{
            2,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->increment_word_address},
            MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,incremented}}};
        const auto applied=memory.apply(batch);
        if(!applied.accepted){result.error=applied.error;return result;}
        if(!deuteros_amiga_->advance_title_post_open_library_local_path()){
            result.error="Deuteros post-OpenLibrary path disappeared before commit";return result;
        }
        *native_runtime_memory_=std::move(memory);result.accepted=true;
    }catch(const std::exception&e){result.error=e.what();}
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_title_native_service_setup() {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !deuteros_amiga_->title_stage_session()
        || !native_runtime_memory_ || deuteros_amiga_exec_compatibility_graph_
        || !deuteros_amiga_native_input_handler_registration_
        || deuteros_amiga_title_native_service_setup_plan_) {
        result.error = "Deuteros native service setup requires the title input handler and pending $206d4 boundary";
        return result;
    }
    const auto& title_stage = *deuteros_amiga_->title_stage_session();
    const auto& exec_boundary = title_stage.exec_boundary();
    const auto& privilege = exec_boundary.native_privilege_transition;
    if (!title_stage.local_prefix_executed()
        || exec_boundary.stack_pointer_value != 0x40b62
        || exec_boundary.state != DeuterosAmigaTitleExecBoundaryState::before_open_library_boundary
        || !privilege || privilege->user_state_stack_argument != 0x7fff0
        || !privilege->host_privilege_transition_elided
        || privilege->user_state_call_address != 0x40468
        || privilege->user_state_return_address != 0x4046c) {
        result.error = "Deuteros virtual stack-bank context is detached from the hash-locked title prefix";
        return result;
    }
    try {
        // Install the Eon-owned Exec and trackdisk compatibility objects at
        // the source setup boundary, after the title has registered input.
        // Their addresses and service outcomes are Eon policy, not captured
        // Exec state or observed Amiga returns.
        auto exec_compatibility = DeuterosAmigaExecCompatibilitySession(
            active_runtime_generation_);
        auto memory = *native_runtime_memory_;
        const auto apply_title_open_success_locals = [&](const std::uint32_t request,
            const std::uint32_t unit) {
            // The title writes IOExtTD.iotd_Count and io_Flags after each successful
            // OpenDevice return. The Exec compatibility outcome supplies only
            // its typed Eon success result; these following writes stay bound
            // to the exact local setup instructions.
            const auto count = memory.read_linear_range(request + 0x30U, 4U);
            const auto flags = memory.read_linear_range(request + 0x1eU, 1U);
            if (!count || !flags) return false;
            NativeRuntimeEffectBatch local_result{
                "deuteros-amiga-title-open-success-local-"
                    + std::to_string(active_runtime_generation_) + "-"
                    + std::to_string(unit), true, {}};
            local_result.effects.push_back({1,
                {NativeRuntimeAddressSpace::linear, std::nullopt, request + 0x30U},
                MemoryTransferElementWidth::longword, NativeRuntimeByteOrder::big_endian,
                0xffffffffU});
            local_result.effects.push_back({2,
                {NativeRuntimeAddressSpace::linear, std::nullopt, request + 0x1eU},
                MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian, 0U});
            if (!memory.apply(local_result).accepted) return false;
            const auto updated_count = memory.read_linear_range(request + 0x30U, 4U);
            const auto updated_flags = memory.read_byte({NativeRuntimeAddressSpace::linear,
                std::nullopt, request + 0x1eU});
            const auto service_error = memory.read_byte({NativeRuntimeAddressSpace::linear,
                std::nullopt, request + 0x1fU});
            return updated_count
                && (*updated_count)[0] == 0xffU && (*updated_count)[1] == 0xffU
                && (*updated_count)[2] == 0xffU && (*updated_count)[3] == 0xffU
                && updated_flags == std::optional<std::uint8_t>{0}
                && service_error == std::optional<std::uint8_t>{0};
        };
        const auto exec_graph = exec_compatibility.install_graph(memory);
        if (!exec_graph || exec_graph->origin
                != DeuterosAmigaExecCompatibilityOrigin::eon_native_compatibility) {
            result.error = "Deuteros native Exec compatibility could not reserve its guest graph";
            return result;
        }
        const auto exec_task = exec_compatibility.find_current_task(
            4, 0x206de, -0x126, 0x206e2, 0);
        if (!exec_task || exec_task->task_address != exec_graph->task_address) {
            result.error = "Deuteros native Exec compatibility rejected FindTask(NULL)";
            return result;
        }

        auto trackdisk = DeuterosAmigaTrackdiskCompatibilitySession(active_runtime_generation_);
        const auto disk1 = deuteros_amiga_->system_disk_image();
        const auto disk1_hash = to_hex(sha256(disk1));
        if (!trackdisk.attach_unit_media(0, disk1_hash)) {
            result.error = "Deuteros native trackdisk could not bind the admitted system Disk 1";
            return result;
        }
        const auto task = trackdisk.find_title_task(4, 0x206de, -0x126, 0x206e2, 0);
        if (!task) {
            result.error = "Deuteros native trackdisk rejected the source FindTask call";
            return result;
        }
        const auto first_port = trackdisk.add_title_port(4, 0x2070c, -0x162, 0x20710,
            0x2061c, 0x206ac, task->opaque_token, 4, 0x7f, 1);
        if (!first_port) {
            result.error = "Deuteros native trackdisk rejected the first source AddPort call";
            return result;
        }
        const auto exec_first_port = exec_compatibility.add_title_port(memory,
            4, 0x2070c, -0x162, 0x20710, 0x2061c);
        if (!exec_first_port || exec_first_port->task_address != exec_graph->task_address) {
            result.error = "Deuteros native Exec compatibility rejected first AddPort";
            return result;
        }
        const auto unit0 = trackdisk.bind_title_unit_service(4, 0x20732, -0x1bc, 0x20736,
            0x206ac, "trackdisk.device", 0x205e4, 0x2061c, 0, 0);
        if (!unit0 || !unit0->device_service_available
            || unit0->media_sha256 != disk1_hash) {
            result.error = "Deuteros native trackdisk rejected the source unit 0 OpenDevice";
            return result;
        }
        const auto exec_unit0 = exec_compatibility.open_title_trackdisk_unit(memory,
            4, 0x20732, -0x1bc, 0x20736, 0x206ac, "trackdisk.device",
            0x205e4, 0x2061c, 0, 0);
        if (!exec_unit0 || exec_unit0->service_generated_io_error != 0
            || exec_unit0->unit != 0) {
            result.error = "Deuteros native Exec compatibility rejected unit-0 OpenDevice";
            return result;
        }
        if (!apply_title_open_success_locals(0x205e4U, 0U)) {
            result.error = "Deuteros title unit-0 success locals missed their owned IORequest";
            return result;
        }
        const auto second_port = trackdisk.add_title_port(4, 0x2077a, -0x162, 0x2077e,
            0x20676, 0x206ac, task->opaque_token, 4, 0x7f, 1);
        if (!second_port) {
            result.error = "Deuteros native trackdisk rejected the second source AddPort call";
            return result;
        }
        const auto exec_second_port = exec_compatibility.add_title_port(memory,
            4, 0x2077a, -0x162, 0x2077e, 0x20676);
        if (!exec_second_port || exec_second_port->task_address != exec_graph->task_address) {
            result.error = "Deuteros native Exec compatibility rejected second AddPort";
            return result;
        }
        const auto unit1 = trackdisk.bind_title_unit_service(4, 0x207a0, -0x1bc, 0x207a4,
            0x206ac, "trackdisk.device", 0x2063e, 0x20676, 1, 0);
        if (!unit1 || !unit1->device_service_available) {
            result.error = "Deuteros native trackdisk rejected the source unit 1 OpenDevice";
            return result;
        }
        const auto exec_unit1 = exec_compatibility.open_title_trackdisk_unit(memory,
            4, 0x207a0, -0x1bc, 0x207a4, 0x206ac, "trackdisk.device",
            0x2063e, 0x20676, 1, 0);
        if (!exec_unit1 || exec_unit1->service_generated_io_error != 0
            || exec_unit1->unit != 1) {
            result.error = "Deuteros native Exec compatibility rejected unit-1 OpenDevice";
            return result;
        }
        if (!apply_title_open_success_locals(0x2063eU, 1U)) {
            result.error = "Deuteros title unit-1 success locals missed their owned IORequest";
            return result;
        }

        const auto plan = deuteros_amiga_->accept_title_native_exec_service_setup(
            *exec_graph, *exec_task, *exec_first_port, *exec_unit0,
            *exec_second_port, *exec_unit1);
        if (!plan || plan->runtime_generation != active_runtime_generation_
            || plan->origin != DeuterosAmigaExecCompatibilityOrigin::eon_native_compatibility
            || plan->setup_entry_address != 0x206d4
            || plan->supervisor_stack_argument != 0x7fff0
            || plan->user_stack_pointer != 0x40b62
            || plan->caller_return_word_address != 0x40b5e
            || plan->saved_task_word_address != 0x40b5a
            || plan->service_call_low_water_address != 0x40b56
            || plan->saved_task_value != exec_graph->task_address
            || plan->final_stack_pointer != 0x40b62
            || plan->final_pc != 0x404bc
            || !plan->assumes_balanced_intervening_calls
            || !plan->assumes_userstate_bank_switch_succeeds
            || plan->stop_before_address != 0x404bc) {
            result.error = "Deuteros Eon-native Exec service receipt did not match its entry boundary";
            return result;
        }
        *native_runtime_memory_ = std::move(memory);
        deuteros_amiga_exec_compatibility_ = std::move(exec_compatibility);
        deuteros_amiga_exec_compatibility_graph_ = *exec_graph;
        deuteros_amiga_exec_task_ = *exec_task;
        deuteros_amiga_exec_first_port_ = *exec_first_port;
        deuteros_amiga_exec_second_port_ = *exec_second_port;
        deuteros_amiga_exec_unit0_open_ = *exec_unit0;
        deuteros_amiga_exec_unit1_open_ = *exec_unit1;
        deuteros_amiga_trackdisk_compatibility_ = std::move(trackdisk);
        deuteros_amiga_trackdisk_task_ = *task;
        deuteros_amiga_trackdisk_unit0_binding_ = *unit0;
        deuteros_amiga_trackdisk_unit1_binding_ = *unit1;
        deuteros_amiga_title_native_service_setup_plan_ = *plan;
        result.accepted = true;
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}

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
    if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::deuteros_amiga_title_stage||!deuteros_amiga_
        ||!deuteros_amiga_->title_stage_session()||!native_runtime_memory_){
        result.error="Deuteros controller seed requires the active title-stage boundary";return result;}
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto plan=pending.advance_controller_pointer_seed();
        if(!plan){result.error="Deuteros controller seed did not match the next owned boundary";return result;}
        if(plan->helper_entry_address!=0x206be||plan->pointer_source_address!=0x20698
            ||plan->pointer_value!=0x205e4||plan->pointer_destination_address!=0x206a0
            ||plan->helper_rts_address!=0x206d2||plan->caller_d1_value!=0x13000
            ||plan->seed_call_address!=0x404c2||plan->seed_call_target!=0x403e6
            ||plan->seed_value!=0x1c482||plan->seed_destination_address!=0x1f97c
            ||plan->next_call_address!=0x404ce||plan->next_call_target!=0x403f4
            ||plan->stop_before_address!=0x404ce||plan->preceding_trace_sequence==0){
            result.error="Deuteros controller seed plan escaped its source-validated route";return result;
        }
        auto memory=*native_runtime_memory_;
        std::uint32_t pointer=0;
        for(std::uint32_t byte=0;byte<4;++byte){
            const auto value=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,
                plan->pointer_source_address+byte});
            if(!value){result.error="Deuteros controller pointer source is not owned";return result;}
            pointer=(pointer<<8U)|*value;
        }
        if(pointer!=plan->pointer_value){
            result.error="Deuteros controller pointer source changed before the helper";return result;
        }
        NativeRuntimeEffectBatch batch{"deuteros-amiga-controller-pointer-seed-"
            +std::to_string(active_runtime_generation_)+"-"
            +std::to_string(plan->preceding_trace_sequence),true,{{
                1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->pointer_destination_address},
                MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,pointer},
                {2,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->seed_destination_address},
                MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,plan->seed_value}}};
        const auto applied=memory.apply(batch);
        if(!applied.accepted){result.error=applied.error;return result;}
        if(!deuteros_amiga_->advance_title_controller_pointer_seed()){
            result.error="Deuteros controller seed boundary disappeared before commit";return result;
        }
        *native_runtime_memory_=std::move(memory);result.accepted=true;
    }catch(const std::exception&e){result.error=std::string("Deuteros controller seed rejected: ")+e.what();}
    return result;
}
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_service_batch_graphics_return,observe_title_service_batch_graphics_return,DeuterosAmigaObservedGraphicsVectorReturn)
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_graphics_service_first_return,observe_title_graphics_service_first_return,DeuterosAmigaObservedGraphicsVectorReturn)
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_graphics_service_third_return,observe_title_graphics_service_third_return,DeuterosAmigaObservedGraphicsVectorReturn)
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_tail_first_graphics_return,observe_title_tail_first_graphics_return,DeuterosAmigaObservedGraphicsVectorReturn)
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_tail_second_graphics_return,observe_title_tail_second_graphics_return,DeuterosAmigaObservedGraphicsVectorReturn)
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_tail_repeated_graphics_return,observe_title_tail_repeated_graphics_return,DeuterosAmigaObservedGraphicsVectorReturn)
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_tail_repeated_wrapper_graphics_return,observe_title_tail_repeated_wrapper_graphics_return,DeuterosAmigaObservedGraphicsVectorReturn)
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_tail_exec_return,observe_title_tail_exec_return,DeuterosAmigaObservedTailExecReturn)
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_load_doio_return,observe_title_load_doio_return,DeuterosAmigaObservedTitleLoadDoIoReturn)
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_load_doio_status,observe_title_load_doio_status,DeuterosAmigaObservedTitleLoadDoIoStatus)
EON_DEUTEROS_BATCH_FORWARD(observe_deuteros_amiga_title_load_service_return,observe_title_load_service_return,DeuterosAmigaObservedLocalCallReturn)
#undef EON_DEUTEROS_BATCH_FORWARD

// These two observations expose values which the original code immediately
// commits into its own address space.  Keep the observation itself external,
// but commit only those proven local effects to Eon's native memory.  Planning
// against a copied session preserves atomicity: a rejected owned-memory batch
// cannot consume an emulator observation or advance the recovered state.
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_service_batch_runtime_word(
    const DeuterosAmigaObservedServiceWordRead observation) {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !native_runtime_memory_ || !deuteros_amiga_title_fifth_service_plan_) {
        result.error = "Deuteros service word requires the active fifth-service boundary";
        return result;
    }
    try {
        auto pending = *deuteros_amiga_->title_stage_session();
        const auto plan = pending.observe_service_batch_runtime_word(observation);
        if (!plan) { result.error = "Deuteros service word did not match the next owned boundary"; return result; }
        auto memory = *native_runtime_memory_;
        NativeRuntimeEffectBatch batch{"deuteros-amiga-title-service-word-" + std::to_string(observation.trace_sequence), true,
            {{1, {NativeRuntimeAddressSpace::linear, std::nullopt, plan->destination_address},
                MemoryTransferElementWidth::word, NativeRuntimeByteOrder::big_endian,
                plan->destination_value}}};
        const auto applied = memory.apply(batch);
        if (!applied.accepted) { result.error = "Runtime-memory application rejected: " + applied.error; return result; }
        if (!deuteros_amiga_->observe_title_service_batch_runtime_word(observation)) {
            result.error = "Deuteros service word disappeared before commit";
            return result;
        }
        *native_runtime_memory_ = std::move(memory);
        result.accepted = true;
    } catch (const std::exception& error) { result.error = std::string("Deuteros service word rejected: ") + error.what(); }
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_graphics_service_second_return(
    const DeuterosAmigaObservedGraphicsVectorReturn observation) {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !native_runtime_memory_ || !deuteros_amiga_title_fifth_service_plan_) {
        result.error = "Deuteros second graphics-service return requires the active fifth-service boundary";
        return result;
    }
    try {
        auto pending = *deuteros_amiga_->title_stage_session();
        const auto plan = pending.observe_graphics_service_second_return(observation);
        if (!plan) { result.error = "Deuteros second graphics-service return did not match the next owned boundary"; return result; }
        NativeRuntimeEffectBatch batch{"deuteros-amiga-title-graphics-service-second-" + std::to_string(observation.trace_sequence), true, {
            {1, {NativeRuntimeAddressSpace::linear, std::nullopt, plan->result_byte_destination},
                MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian, plan->result_low_byte},
            {2, {NativeRuntimeAddressSpace::linear, std::nullopt, plan->pointer_destination},
                MemoryTransferElementWidth::longword, NativeRuntimeByteOrder::big_endian, plan->pointer_value},
            {3, {NativeRuntimeAddressSpace::linear, std::nullopt, plan->descriptor_address + plan->descriptor_offsets[0]},
                MemoryTransferElementWidth::word, NativeRuntimeByteOrder::big_endian, plan->descriptor_values[0]},
            {4, {NativeRuntimeAddressSpace::linear, std::nullopt, plan->descriptor_address + plan->descriptor_offsets[1]},
                MemoryTransferElementWidth::word, NativeRuntimeByteOrder::big_endian, plan->descriptor_values[1]},
            {5, {NativeRuntimeAddressSpace::linear, std::nullopt, plan->descriptor_address + plan->descriptor_offsets[2]},
                MemoryTransferElementWidth::word, NativeRuntimeByteOrder::big_endian, plan->descriptor_values[2]},
        }};
        auto memory = *native_runtime_memory_;
        const auto applied = memory.apply(batch);
        if (!applied.accepted) { result.error = "Runtime-memory application rejected: " + applied.error; return result; }
        if (!deuteros_amiga_->observe_title_graphics_service_second_return(observation)) {
            result.error = "Deuteros second graphics-service return disappeared before commit";
            return result;
        }
        *native_runtime_memory_ = std::move(memory);
        result.accepted = true;
    } catch (const std::exception& error) { result.error = std::string("Deuteros second graphics-service return rejected: ") + error.what(); }
    return result;
}

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_tail_copy_words(
    const DeuterosAmigaObservedTailCopyWords observation) {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !native_runtime_memory_ || !deuteros_amiga_title_fifth_service_plan_) {
        result.error = "Deuteros tail copy requires the active fifth-service boundary";
        return result;
    }
    try {
        auto pending = *deuteros_amiga_->title_stage_session();
        const auto plan = pending.observe_tail_copy_words(observation);
        if (!plan) { result.error = "Deuteros tail copy did not match the next owned boundary"; return result; }
        NativeRuntimeEffectBatch batch{"deuteros-amiga-title-tail-copy-" + std::to_string(observation.trace_sequence), true, {}};
        batch.effects.reserve(plan->destination_addresses.size() + plan->literal_destination_addresses.size());
        std::size_t order = 1;
        for (std::size_t index = 0; index < plan->destination_addresses.size(); ++index) {
            batch.effects.push_back({order++, {NativeRuntimeAddressSpace::linear, std::nullopt, plan->destination_addresses[index]},
                MemoryTransferElementWidth::word, NativeRuntimeByteOrder::big_endian, plan->destination_values[index]});
        }
        for (std::size_t index = 0; index < plan->literal_destination_addresses.size(); ++index) {
            batch.effects.push_back({order++, {NativeRuntimeAddressSpace::linear, std::nullopt, plan->literal_destination_addresses[index]},
                MemoryTransferElementWidth::word, NativeRuntimeByteOrder::big_endian, plan->literal_values[index]});
        }
        auto memory = *native_runtime_memory_;
        const auto applied = memory.apply(batch);
        if (!applied.accepted) { result.error = "Runtime-memory application rejected: " + applied.error; return result; }
        if (!deuteros_amiga_->observe_title_tail_copy_words(observation)) {
            result.error = "Deuteros tail copy disappeared before commit";
            return result;
        }
        *native_runtime_memory_ = std::move(memory);
        result.accepted = true;
    } catch (const std::exception& error) { result.error = std::string("Deuteros tail copy rejected: ") + error.what(); }
    return result;
}

#define EON_DEUTEROS_TAIL_SELECTION(method, session_method, opening_method, label) \
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::method( \
    const DeuterosAmigaObservedTailSelectionWords observation) { \
    DeuterosAmigaTitleDependencyObservationResult result; \
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage \
        || !deuteros_amiga_ || !native_runtime_memory_ || !deuteros_amiga_title_fifth_service_plan_) { \
        result.error = "Deuteros " label " requires the active fifth-service boundary"; return result; \
    } \
    try { \
        auto pending = *deuteros_amiga_->title_stage_session(); \
        const auto plan = pending.session_method(observation); \
        if (!plan) { result.error = "Deuteros " label " did not match the next owned boundary"; return result; } \
        NativeRuntimeEffectBatch batch{"deuteros-amiga-title-" label "-" + std::to_string(observation.trace_sequence), true, { \
            {1, {NativeRuntimeAddressSpace::linear, std::nullopt, plan->destination_addresses[0]}, MemoryTransferElementWidth::word, NativeRuntimeByteOrder::big_endian, plan->selected_words[0]}, \
            {2, {NativeRuntimeAddressSpace::linear, std::nullopt, plan->destination_addresses[1]}, MemoryTransferElementWidth::word, NativeRuntimeByteOrder::big_endian, plan->selected_words[1]}, \
        }}; \
        auto memory = *native_runtime_memory_; \
        const auto applied = memory.apply(batch); \
        if (!applied.accepted) { result.error = "Runtime-memory application rejected: " + applied.error; return result; } \
        if (!deuteros_amiga_->opening_method(observation)) { result.error = "Deuteros " label " disappeared before commit"; return result; } \
        *native_runtime_memory_ = std::move(memory); result.accepted = true; \
    } catch (const std::exception& error) { result.error = std::string("Deuteros " label " rejected: ") + error.what(); } \
    return result; \
}
EON_DEUTEROS_TAIL_SELECTION(observe_deuteros_amiga_title_tail_selection_words, observe_tail_selection_words, observe_title_tail_selection_words, "tail-selection")
EON_DEUTEROS_TAIL_SELECTION(observe_deuteros_amiga_title_tail_repeated_selection_words, observe_tail_repeated_selection_words, observe_title_tail_repeated_selection_words, "repeated-tail-selection")
#undef EON_DEUTEROS_TAIL_SELECTION

DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_tail_source_table(
    const DeuterosAmigaObservedTailSourceTable observation) {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !native_runtime_memory_ || !deuteros_amiga_title_fifth_service_plan_) {
        result.error = "Deuteros tail source table requires the active fifth-service boundary";
        return result;
    }
    try {
        auto pending = *deuteros_amiga_->title_stage_session();
        const auto plan = pending.observe_tail_source_table(observation);
        if (!plan) { result.error = "Deuteros tail source table did not match the next owned boundary"; return result; }
        NativeRuntimeEffectBatch batch{"deuteros-amiga-title-tail-source-table-" + std::to_string(observation.trace_sequence), true, {
            {1, {NativeRuntimeAddressSpace::linear, std::nullopt, plan->destination_addresses[0]},
                MemoryTransferElementWidth::longword, NativeRuntimeByteOrder::big_endian, plan->destination_values[0]},
            {2, {NativeRuntimeAddressSpace::linear, std::nullopt, plan->destination_addresses[1]},
                MemoryTransferElementWidth::longword, NativeRuntimeByteOrder::big_endian, plan->destination_values[1]},
            {3, {NativeRuntimeAddressSpace::linear, std::nullopt, plan->descriptor_address + plan->descriptor_offsets[0]},
                MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian, plan->descriptor_values[0]},
            {4, {NativeRuntimeAddressSpace::linear, std::nullopt, plan->descriptor_address + plan->descriptor_offsets[1]},
                MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian, plan->descriptor_values[1]},
            {5, {NativeRuntimeAddressSpace::linear, std::nullopt, plan->descriptor_address + plan->descriptor_offsets[2]},
                MemoryTransferElementWidth::longword, NativeRuntimeByteOrder::big_endian, plan->descriptor_values[2]},
            {6, {NativeRuntimeAddressSpace::linear, std::nullopt, plan->descriptor_address + plan->descriptor_offsets[3]},
                MemoryTransferElementWidth::longword, NativeRuntimeByteOrder::big_endian, plan->descriptor_values[3]},
        }};
        auto memory = *native_runtime_memory_;
        const auto applied = memory.apply(batch);
        if (!applied.accepted) { result.error = "Runtime-memory application rejected: " + applied.error; return result; }
        if (!deuteros_amiga_->observe_title_tail_source_table(observation)) {
            result.error = "Deuteros tail source table disappeared before commit";
            return result;
        }
        *native_runtime_memory_ = std::move(memory);
        result.accepted = true;
    } catch (const std::exception& error) { result.error = std::string("Deuteros tail source table rejected: ") + error.what(); }
    return result;
}

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
    DeuterosAmigaTitleDependencyObservationResult result;if(!deuteros_amiga_||!deuteros_amiga_title_load_copy_||!deuteros_amiga_title_load_copy_->checkpoint().complete||!native_runtime_memory_){result.error="Deuteros dispatch base requires the completed owned load copy";return result;}try{auto pending=*deuteros_amiga_->title_stage_session();const auto plan=pending.observe_load_dispatch_table_base(observation);if(!plan){result.error="Deuteros dispatch base did not match the next owned boundary";return result;}const auto owned=native_runtime_memory_->read_linear_range(observation.source_address,4);if(owned){const auto value=(static_cast<std::uint32_t>((*owned)[0])<<24U)|(static_cast<std::uint32_t>((*owned)[1])<<16U)|(static_cast<std::uint32_t>((*owned)[2])<<8U)|(*owned)[3];if(value!=observation.observed_value){result.error="Deuteros dispatch base differs from owned runtime memory";return result;}}if(!deuteros_amiga_->observe_title_load_dispatch_table_base(observation)){result.error="Deuteros dispatch base disappeared before commit";return result;}result.accepted=true;}catch(const std::exception&e){result.error=std::string("Deuteros dispatch base rejected: ")+e.what();}return result;
}

DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_load_dispatch_table_word(const DeuterosAmigaObservedLoadDispatchTableWord observation){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!deuteros_amiga_||!native_runtime_memory_){
        result.error="Deuteros dispatch word requires the active owned title runtime";
        return result;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto plan=pending.observe_load_dispatch_table_word(observation);
        if(!plan){result.error="Deuteros dispatch word did not match the next owned boundary";return result;}
        const auto owned=native_runtime_memory_->read_linear_range(observation.source_address,2);
        if(owned){
            const auto value=static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>((*owned)[0])<<8U)|(*owned)[1]);
            if(value!=observation.observed_value){
                result.error="Deuteros dispatch word differs from owned runtime memory";
                return result;
            }
        }
        NativeRuntimeEffectBatch batch{
            "deuteros-amiga-title-dispatch-"+std::to_string(deuteros_amiga_title_load_copy_generation_),
            true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->byte_write_address},
                MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,
                plan->byte_write_value}}};
        auto next=*native_runtime_memory_;
        const auto applied=next.apply(batch);
        if(!applied.accepted){result.error="Runtime-memory application rejected: "+applied.error;return result;}
        if(!deuteros_amiga_->observe_title_load_dispatch_table_word(observation)){
            result.error="Deuteros dispatch word disappeared before commit";
            return result;
        }
        *native_runtime_memory_=std::move(next);
        result.accepted=true;
    }catch(const std::exception&e){
        result.error=std::string("Deuteros dispatch word rejected: ")+e.what();
    }
    return result;
}

DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_command_opcode(const DeuterosAmigaObservedTitleCommandOpcode observation){
    DeuterosAmigaTitleDependencyObservationResult result;if(!deuteros_amiga_){result.error="Deuteros command opcode requires the active owned title runtime";return result;}try{if(native_runtime_memory_){const auto owned=native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,observation.source_address});if(owned&&*owned!=observation.observed_value){result.error="Deuteros command opcode differs from owned runtime memory";return result;}}if(!deuteros_amiga_->observe_title_command_opcode(observation)){result.error="Deuteros command opcode did not match the next owned boundary";return result;}++deuteros_amiga_title_command_generation_;result.accepted=true;}catch(const std::exception&e){result.error=std::string("Deuteros command opcode rejected: ")+e.what();}return result;
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
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_command_negative_phase(
    const DeuterosAmigaObservedTitleCommandNegativePhase observation) {
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
        const auto preview_plan = preview_session.observe_command_negative_service_phase(observation);
        const auto committed_plan = deuteros_amiga_->observe_title_command_negative_service_phase(observation);
        if (preview_plan.has_value() != committed_plan.has_value()) {
            result.error = "Deuteros negative-service preview and commit diverged";
            return result;
        }
        result.accepted = true;
    } catch (const std::exception& e) {
        result.error = std::string("Deuteros negative-service phase rejected: ") + e.what();
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
        for(std::size_t i=0;i<observation.mask_source_addresses.size();++i){
            const auto address=observation.mask_source_addresses[i];const auto word=observation.observed_mask_words[i];
            const auto high=next_memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address});
            const auto low=next_memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address+1U});
            if((high&&*high!=static_cast<std::uint8_t>(word>>8U))
                ||(low&&*low!=static_cast<std::uint8_t>(word&0xffU))){
                result.error="Deuteros first merge mask observation contradicts owned runtime memory";return result;}
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
ReleaseRuntimeCoordinator::advance_deuteros_amiga_title_post_command_selected_stream(){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros selected stream requires active title session";return r;}
    try{auto p=*deuteros_amiga_->title_stage_session();const auto plan=p.advance_post_command_selected_stream();if(!plan){r.error="Deuteros selected stream did not match boundary";return r;}
        auto m=*native_runtime_memory_;NativeRuntimeEffectBatch b{"deuteros-amiga-title-post-command-selected-stream",true,{}};b.effects.reserve(plan->destination_addresses.size());
        for(std::size_t i=0;i<plan->destination_addresses.size();++i)b.effects.push_back({i+1U,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->destination_addresses[i]},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,plan->destination_values[i]});
        const auto a=m.apply(b);if(!a.accepted){r.error=a.error;return r;}
        if(!deuteros_amiga_->advance_title_post_command_selected_stream()){r.error="Deuteros selected stream disappeared";return r;}*native_runtime_memory_=std::move(m);r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}return r;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_command_descriptor_call_return(const DeuterosAmigaObservedLocalCallReturn o){DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros descriptor call requires active title session";return r;}try{auto p=*deuteros_amiga_->title_stage_session();if(!p.observe_post_command_descriptor_call_return(o)||!deuteros_amiga_->observe_title_post_command_descriptor_call_return(o)){r.error="Deuteros descriptor call did not match boundary";return r;}r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_title_post_command_descriptor_loop(){DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros descriptor loop requires active title session";return r;}try{auto p=*deuteros_amiga_->title_stage_session();const auto plan=p.advance_post_command_descriptor_loop();if(!plan){r.error="Deuteros descriptor loop did not match boundary";return r;}auto m=*native_runtime_memory_;if(plan->completed){NativeRuntimeEffectBatch b{"deuteros-amiga-title-post-command-next-descriptor",true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->descriptor_destination},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,plan->descriptor_value}}};const auto a=m.apply(b);if(!a.accepted){r.error=a.error;return r;}}if(!deuteros_amiga_->advance_title_post_command_descriptor_loop()){r.error="Deuteros descriptor loop disappeared";return r;}*native_runtime_memory_=std::move(m);r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_command_descriptor_byte(const DeuterosAmigaObservedTitlePostCommandDescriptorByte o){DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros descriptor byte requires active title session";return r;}try{auto p=*deuteros_amiga_->title_stage_session();const auto plan=p.observe_post_command_descriptor_byte(o);if(!plan){r.error="Deuteros descriptor byte did not match boundary";return r;}auto m=*native_runtime_memory_;NativeRuntimeEffectBatch b{"deuteros-amiga-title-post-command-adjusted-descriptor",true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->descriptor_destination},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,plan->result_descriptor}}};const auto a=m.apply(b);if(!a.accepted){r.error=a.error;return r;}if(!deuteros_amiga_->observe_title_post_command_descriptor_byte(o)){r.error="Deuteros descriptor byte disappeared";return r;}*native_runtime_memory_=std::move(m);r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_command_adjusted_dispatch_destination(
    const DeuterosAmigaObservedTitlePostCommandAdjustedDispatchDestination o){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){
        r.error="Deuteros adjusted dispatch requires active title session";return r;}
    try{auto preview=*deuteros_amiga_->title_stage_session();const auto plan=preview.observe_post_command_adjusted_dispatch_destination(o);
        if(!plan){r.error="Deuteros adjusted dispatch destination did not match boundary";return r;}
        auto memory=*native_runtime_memory_;NativeRuntimeEffectBatch batch{"deuteros-amiga-title-post-command-adjusted-dispatch",true,{}};
        batch.effects.reserve(plan->destination_addresses.size());
        for(std::size_t i=0;i<plan->destination_addresses.size();++i)batch.effects.push_back({i+1U,
            {NativeRuntimeAddressSpace::linear,std::nullopt,plan->destination_addresses[i]},
            MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,plan->destination_values[i]});
        const auto applied=memory.apply(batch);if(!applied.accepted){r.error="Runtime-memory application rejected: "+applied.error;return r;}
        if(!deuteros_amiga_->observe_title_post_command_adjusted_dispatch_destination(o)){
            r.error="Deuteros adjusted dispatch disappeared before commit";return r;}
        *native_runtime_memory_=std::move(memory);r.accepted=true;
    }catch(const std::exception&e){r.error=std::string("Deuteros adjusted dispatch rejected: ")+e.what();}
    return r;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_adjusted_caller_pointer(
    const DeuterosAmigaObservedTitlePostAdjustedCallerPointer o){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){
        r.error="Deuteros post-adjusted caller pointer requires active title session";return r;}
    try{auto preview=*deuteros_amiga_->title_stage_session();const auto plan=preview.observe_post_adjusted_caller_pointer(o);
        if(!plan){r.error="Deuteros post-adjusted caller pointer did not match boundary";return r;}
        auto memory=*native_runtime_memory_;NativeRuntimeEffectBatch batch{"deuteros-amiga-title-post-adjusted-caller-pointer",true,
            {{1,{NativeRuntimeAddressSpace::linear,std::nullopt,o.source_address},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,o.observed_pointer}}};
        const auto applied=memory.apply(batch);if(!applied.accepted){r.error=applied.error;return r;}
        if(!deuteros_amiga_->observe_title_post_adjusted_caller_pointer(o)){r.error="Deuteros post-adjusted caller pointer disappeared before commit";return r;}
        *native_runtime_memory_=std::move(memory);r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}
    return r;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_adjusted_object_gate(
    const DeuterosAmigaObservedTitlePostAdjustedObjectGate o){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){
        r.error="Deuteros post-adjusted object gate requires active title session";return r;}
    try{auto preview=*deuteros_amiga_->title_stage_session();const auto plan=preview.observe_post_adjusted_object_gate(o);
        if(!plan){r.error="Deuteros post-adjusted object gate did not match boundary";return r;}
        auto memory=*native_runtime_memory_;NativeRuntimeEffectBatch batch{"deuteros-amiga-title-post-adjusted-object-gate",true,
            {{1,{NativeRuntimeAddressSpace::linear,std::nullopt,o.first_source_address},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,o.first_value},
             {2,{NativeRuntimeAddressSpace::linear,std::nullopt,o.second_source_address},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,o.second_value}}};
        const auto applied=memory.apply(batch);if(!applied.accepted){r.error=applied.error;return r;}
        if(!deuteros_amiga_->observe_title_post_adjusted_object_gate(o)){r.error="Deuteros post-adjusted object gate disappeared before commit";return r;}
        *native_runtime_memory_=std::move(memory);r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}
    return r;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_adjusted_first_helper_return(
    const DeuterosAmigaObservedLocalCallReturn o){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){
        r.error="Deuteros post-adjusted first helper return requires active title session";return r;}
    try{auto preview=*deuteros_amiga_->title_stage_session();
        if(!preview.observe_post_adjusted_first_helper_return(o)){
            r.error="Deuteros post-adjusted first helper return did not match boundary";return r;}
        if(!deuteros_amiga_->observe_title_post_adjusted_first_helper_return(o)){
            r.error="Deuteros post-adjusted first helper return disappeared before commit";return r;}
        r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}
    return r;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_adjusted_second_helper_return(
    const DeuterosAmigaObservedLocalCallReturn o){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){
        r.error="Deuteros post-adjusted second helper return requires active title session";return r;}
    try{auto preview=*deuteros_amiga_->title_stage_session();
        if(!preview.observe_post_adjusted_second_helper_return(o)){
            r.error="Deuteros post-adjusted second helper return did not match boundary";return r;}
        if(!deuteros_amiga_->observe_title_post_adjusted_second_helper_return(o)){
            r.error="Deuteros post-adjusted second helper return disappeared before commit";return r;}
        r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}
    return r;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_adjusted_rts_frame(
    const DeuterosAmigaObservedTitlePostAdjustedRtsFrame o){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){
        r.error="Deuteros post-adjusted RTS frame requires active title session";return r;}
    try{auto preview=*deuteros_amiga_->title_stage_session();
        if(!preview.observe_post_adjusted_rts_frame(o)){r.error="Deuteros post-adjusted RTS frame did not match boundary";return r;}
        auto memory=*native_runtime_memory_;NativeRuntimeEffectBatch batch{"deuteros-amiga-title-post-adjusted-rts-frame",true,
            {{1,{NativeRuntimeAddressSpace::linear,std::nullopt,o.frame_address},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,o.return_address}}};
        const auto applied=memory.apply(batch);if(!applied.accepted){r.error=applied.error;return r;}
        if(!deuteros_amiga_->observe_title_post_adjusted_rts_frame(o)){r.error="Deuteros post-adjusted RTS frame disappeared before commit";return r;}
        *native_runtime_memory_=std::move(memory);r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}
    return r;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_adjusted_repeated_nested_words(
    const DeuterosAmigaObservedTitlePostCommandNestedWords o){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!native_runtime_memory_
        ||!deuteros_amiga_->title_stage_session()){
        r.error="Deuteros repeated local-service words require active title session";return r;}
    try{auto preview=*deuteros_amiga_->title_stage_session();
        const auto plan=preview.observe_post_adjusted_repeated_nested_words(o);
        if(!plan){r.error="Deuteros repeated local-service words did not match boundary";return r;}
        auto memory=*native_runtime_memory_;
        if(plan->writes_counter){NativeRuntimeEffectBatch batch{
            "deuteros-amiga-title-repeated-local-service-counter",true,
            {{1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->counter_destination},
                MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,
                plan->counter_value}}};
            const auto applied=memory.apply(batch);
            if(!applied.accepted){r.error=applied.error;return r;}}
        if(!deuteros_amiga_->observe_title_post_adjusted_repeated_nested_words(o)){
            r.error="Deuteros repeated local-service words disappeared before commit";return r;}
        *native_runtime_memory_=std::move(memory);r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}return r;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_adjusted_repeated_nested_call_return(
    const DeuterosAmigaObservedLocalCallReturn o){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){
        r.error="Deuteros repeated local-service return requires active title session";return r;}
    try{auto preview=*deuteros_amiga_->title_stage_session();
        if(!preview.observe_post_adjusted_repeated_nested_call_return(o)){
            r.error="Deuteros repeated local-service return did not match boundary";return r;}
        if(!deuteros_amiga_->observe_title_post_adjusted_repeated_nested_call_return(o)){
            r.error="Deuteros repeated local-service return disappeared before commit";return r;}
        r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}return r;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_title_post_adjusted_repeated_nested_loop(){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!native_runtime_memory_
        ||!deuteros_amiga_->title_stage_session()){
        r.error="Deuteros repeated local-service loop requires active title session";return r;}
    try{auto preview=*deuteros_amiga_->title_stage_session();
        const auto plan=preview.advance_post_adjusted_repeated_nested_loop();
        if(!plan){r.error="Deuteros repeated local-service loop did not match boundary";return r;}
        auto memory=*native_runtime_memory_;
        if(plan->writes_counter){NativeRuntimeEffectBatch batch{
            "deuteros-amiga-title-repeated-local-service-loop-counter-"
                +std::to_string(plan->iteration),true,
            {{1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->counter_destination},
                MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,
                plan->counter_value}}};
            const auto applied=memory.apply(batch);
            if(!applied.accepted){r.error=applied.error;return r;}}
        if(!deuteros_amiga_->advance_title_post_adjusted_repeated_nested_loop()){
            r.error="Deuteros repeated local-service loop disappeared before commit";return r;}
        *native_runtime_memory_=std::move(memory);r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}return r;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_title_post_adjusted_caller_indirect(){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){
        r.error="Deuteros post-adjusted caller continuation requires active title session";return r;}
    try{auto preview=*deuteros_amiga_->title_stage_session();
        if(!preview.advance_post_adjusted_caller_indirect()){
            r.error="Deuteros post-adjusted caller continuation did not match boundary";return r;}
        if(!deuteros_amiga_->advance_title_post_adjusted_caller_indirect()){
            r.error="Deuteros post-adjusted caller continuation disappeared before commit";return r;}
        r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}return r;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_adjusted_caller_indirect_return(
    const DeuterosAmigaObservedTitlePostAdjustedIndirectReturn o){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){
        r.error="Deuteros post-adjusted indirect return requires active title session";return r;}
    try{auto preview=*deuteros_amiga_->title_stage_session();
        const auto plan=preview.observe_post_adjusted_caller_indirect_return(o);
        if(!plan){r.error="Deuteros post-adjusted indirect return did not match boundary";return r;}
        auto memory=*native_runtime_memory_;NativeRuntimeEffectBatch batch{
            "deuteros-amiga-title-post-adjusted-caller-shift",true,
            {{1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->destination_address},
                MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,
                plan->destination_word}}};
        const auto applied=memory.apply(batch);if(!applied.accepted){r.error=applied.error;return r;}
        if(!deuteros_amiga_->observe_title_post_adjusted_caller_indirect_return(o)){
            r.error="Deuteros post-adjusted indirect return disappeared before commit";return r;}
        *native_runtime_memory_=std::move(memory);r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}return r;
}
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_adjusted_caller_37180_return(const DeuterosAmigaObservedTitlePostAdjusted37180Return o){DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros $37180 return requires active title session";return r;}try{auto p=*deuteros_amiga_->title_stage_session();const auto plan=p.observe_post_adjusted_caller_37180_return(o);if(!plan){r.error="Deuteros $37180 return did not match boundary";return r;}auto m=*native_runtime_memory_;NativeRuntimeEffectBatch b{"deuteros-amiga-title-post-adjusted-caller-copy",true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->destination_address},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,plan->destination_long}}};const auto a=m.apply(b);if(!a.accepted){r.error=a.error;return r;}if(!deuteros_amiga_->observe_title_post_adjusted_caller_37180_return(o)){r.error="Deuteros $37180 return disappeared before commit";return r;}*native_runtime_memory_=std::move(m);r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_adjusted_mode_return(const DeuterosAmigaObservedLocalCallReturn o){DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros selected mode return requires active title session";return r;}try{auto p=*deuteros_amiga_->title_stage_session();if(!p.observe_post_adjusted_mode_return(o)){r.error="Deuteros selected mode return did not match boundary";return r;}if(!deuteros_amiga_->observe_title_post_adjusted_mode_return(o)){r.error="Deuteros selected mode return disappeared before commit";return r;}r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_adjusted_222c0_return(const DeuterosAmigaObservedLocalCallReturn o){DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros $222c0 return requires active title session";return r;}try{auto p=*deuteros_amiga_->title_stage_session();if(!p.observe_post_adjusted_222c0_return(o)){r.error="Deuteros $222c0 return did not match boundary";return r;}if(!deuteros_amiga_->observe_title_post_adjusted_222c0_return(o)){r.error="Deuteros $222c0 return disappeared before commit";return r;}r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_adjusted_timer_state(const DeuterosAmigaObservedTitlePostAdjustedTimerState o){DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros timer observation requires active title session";return r;}try{auto p=*deuteros_amiga_->title_stage_session();const auto plan=p.observe_post_adjusted_timer_state(o);if(!plan){r.error="Deuteros timer observation did not match boundary";return r;}auto m=*native_runtime_memory_;std::vector<NativeRuntimeWriteEffect> effects;std::size_t order=1;if(plan->word_changed)effects.push_back({order++,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->word_destination},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,plan->word_value});if(plan->clear_timer)effects.push_back({order,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->timer_destination},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,0});if(!effects.empty()){NativeRuntimeEffectBatch b{"deuteros-amiga-title-post-adjusted-timer",true,std::move(effects)};const auto a=m.apply(b);if(!a.accepted){r.error=a.error;return r;}}if(!deuteros_amiga_->observe_title_post_adjusted_timer_state(o)){r.error="Deuteros timer observation disappeared before commit";return r;}*native_runtime_memory_=std::move(m);r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_adjusted_4069a_return(const DeuterosAmigaObservedLocalCallReturn o){DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros $4069a return requires active title session";return r;}try{auto p=*deuteros_amiga_->title_stage_session();const auto plan=p.observe_post_adjusted_4069a_return(o);if(!plan){r.error="Deuteros $4069a return did not match boundary";return r;}auto m=*native_runtime_memory_;NativeRuntimeEffectBatch b{"deuteros-amiga-title-post-adjusted-due-clear",true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->timer_destination},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,0}}};const auto a=m.apply(b);if(!a.accepted){r.error=a.error;return r;}if(!deuteros_amiga_->observe_title_post_adjusted_4069a_return(o)){r.error="Deuteros $4069a return disappeared before commit";return r;}*native_runtime_memory_=std::move(m);r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_adjusted_join_byte(const DeuterosAmigaObservedTitlePostAdjustedJoinByte o){DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros join-byte observation requires active title session";return r;}try{auto p=*deuteros_amiga_->title_stage_session();if(!p.observe_post_adjusted_join_byte(o)){r.error="Deuteros join-byte observation did not match boundary";return r;}if(!deuteros_amiga_->observe_title_post_adjusted_join_byte(o)){r.error="Deuteros join-byte observation disappeared before commit";return r;}r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_adjusted_1f9a4_return(const DeuterosAmigaObservedLocalCallReturn o){DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros $1f9a4 return requires active title session";return r;}try{auto p=*deuteros_amiga_->title_stage_session();if(!p.observe_post_adjusted_1f9a4_return(o)){r.error="Deuteros $1f9a4 return did not match boundary";return r;}if(!deuteros_amiga_->observe_title_post_adjusted_1f9a4_return(o)){r.error="Deuteros $1f9a4 return disappeared before commit";return r;}r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_adjusted_1fe88_return(const DeuterosAmigaObservedTitlePostAdjusted1fe88Return o){DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros $1fe88 return requires active title session";return r;}try{auto p=*deuteros_amiga_->title_stage_session();if(!p.observe_post_adjusted_1fe88_return(o)){r.error="Deuteros $1fe88 return did not match boundary";return r;}if(!deuteros_amiga_->observe_title_post_adjusted_1fe88_return(o)){r.error="Deuteros $1fe88 return disappeared before commit";return r;}r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_adjusted_final_gate(const DeuterosAmigaObservedTitlePostAdjustedFinalGate o){DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros final gate requires active title session";return r;}try{auto p=*deuteros_amiga_->title_stage_session();if(!p.observe_post_adjusted_final_gate(o)){r.error="Deuteros final gate did not match boundary";return r;}if(!deuteros_amiga_->observe_title_post_adjusted_final_gate(o)){r.error="Deuteros final gate disappeared before commit";return r;}r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_captured_title_selector_passage(
    const std::uint64_t sequence, const std::uint32_t incoming_d0,
    const std::array<DeuterosAmigaObservedTitleSelectorHelperReturn,2> returns) {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !deuteros_amiga_->title_stage_session()) {
        result.error = "Captured selector passage requires the active Deuteros Amiga title stage";
        return result;
    }
    try {
        auto pending = *deuteros_amiga_->title_stage_session();
        if (!pending.observe_captured_title_selector_passage(sequence,incoming_d0,returns)) {
            result.error = "Captured selector passage is stale or already consumed";
            return result;
        }
        if (!deuteros_amiga_->observe_captured_title_selector_passage(
                sequence,incoming_d0,returns)) {
            result.error = "Captured selector passage disappeared before commit";
            return result;
        }
        result.accepted = true;
    } catch (const std::exception& error) {
        result.error = std::string("Captured selector passage rejected: ") + error.what();
    }
    return result;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_captured_title_selector_dispatch(
    const DeuterosAmigaObservedTitleSelectorDispatchRead primary,
    const std::optional<DeuterosAmigaObservedTitleSelectorDispatchRead> secondary) {
    DeuterosAmigaTitleDependencyObservationResult result;
    if (!active_ || !session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::deuteros_amiga_title_stage
        || !deuteros_amiga_ || !deuteros_amiga_->title_stage_session()) {
        result.error = "Captured selector dispatch requires the active Deuteros Amiga title stage";
        return result;
    }
    try {
        auto pending = *deuteros_amiga_->title_stage_session();
        const auto plan = pending.observe_captured_title_selector_dispatch(primary, secondary);
        if (!plan) {
            result.error = "Captured selector dispatch is stale or does not match the original branch";
            return result;
        }
        if (!native_runtime_memory_) {
            result.error = "Captured selector dispatch requires owned title-stage memory";
            return result;
        }
        auto memory = *native_runtime_memory_;
        const auto admit_read = [&](const DeuterosAmigaObservedTitleSelectorDispatchRead& read,
                                    const std::size_t order) {
            const NativeRuntimeLocation location{NativeRuntimeAddressSpace::linear,
                std::nullopt, read.source_address};
            const auto resident = memory.read_byte(location);
            if (resident && *resident != read.value) {
                throw std::runtime_error(
                    "Captured selector byte disagrees with owned title-stage memory");
            }
            return NativeRuntimeWriteEffect{order, location,
                MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian,
                read.value};
        };
        NativeRuntimeEffectBatch batch{
            "deuteros-amiga-title-selector-dispatch-" +
                std::to_string(primary.trace_sequence), true,
            {admit_read(primary, 1)}};
        if (secondary) batch.effects.push_back(admit_read(*secondary, 2));
        const auto applied = memory.apply(batch);
        if (!applied.accepted) {
            result.error = applied.error;
            return result;
        }
        if (!deuteros_amiga_->observe_captured_title_selector_dispatch(primary, secondary)) {
            result.error = "Captured selector dispatch disappeared before commit";
            return result;
        }
        *native_runtime_memory_ = std::move(memory);
        result.accepted = true;
        result.selector_dispatch = *plan;
    } catch (const std::exception& error) {
        result.error = std::string("Captured selector dispatch rejected: ") + error.what();
    }
    return result;
}
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_adjusted_input_return(const DeuterosAmigaObservedTitlePostAdjustedInputReturn o){DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros joined input return requires active title session";return r;}try{auto p=*deuteros_amiga_->title_stage_session();const auto plan=p.observe_post_adjusted_input_return(o);if(!plan){r.error="Deuteros joined input return did not match boundary";return r;}auto m=*native_runtime_memory_;NativeRuntimeEffectBatch b{"deuteros-amiga-title-post-adjusted-input-toggle",true,{}};if(plan->writes_toggle)b.effects.push_back({1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->toggle_destination},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,plan->toggle_word});const auto a=m.apply(b);if(!a.accepted){r.error=a.error;return r;}if(!deuteros_amiga_->observe_title_post_adjusted_input_return(o)){r.error="Deuteros joined input return disappeared before commit";return r;}*native_runtime_memory_=std::move(m);r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_post_adjusted_repeated_input_return(const DeuterosAmigaObservedLocalCallReturn o){DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros repeated input return requires active title session";return r;}try{auto p=*deuteros_amiga_->title_stage_session();const auto plan=p.observe_post_adjusted_repeated_input_return(o);if(!plan){r.error="Deuteros repeated input return did not match boundary";return r;}auto m=*native_runtime_memory_;NativeRuntimeEffectBatch b{"deuteros-amiga-title-post-adjusted-colour-"+std::to_string(o.trace_sequence),true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->colour_destination},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,plan->colour_word}}};const auto a=m.apply(b);if(!a.accepted){r.error=a.error;return r;}if(!deuteros_amiga_->observe_title_post_adjusted_repeated_input_return(o)){r.error="Deuteros repeated input return disappeared before commit";return r;}*native_runtime_memory_=std::move(m);r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_tail_copy(const DeuterosAmigaObservedTitleTailCopy o){DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros title-tail copy requires active title session";return r;}try{auto p=*deuteros_amiga_->title_stage_session();const auto plan=p.observe_title_tail_copy(o);if(!plan){r.error="Deuteros title-tail copy did not match boundary";return r;}auto m=*native_runtime_memory_;NativeRuntimeEffectBatch b{"deuteros-amiga-title-tail-copy-"+std::to_string(o.trace_sequence),true,{}};b.effects.reserve(plan->source_bytes.size());for(std::size_t i=0;i<plan->source_bytes.size();++i){const NativeRuntimeLocation source{NativeRuntimeAddressSpace::linear,std::nullopt,static_cast<std::uint64_t>(o.source_address+i)};const auto resident=m.read_byte(source);const auto value=resident.value_or(plan->source_bytes[i]);b.effects.push_back({i+1,{NativeRuntimeAddressSpace::linear,std::nullopt,static_cast<std::uint64_t>(o.destination_address+i)},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,value});}const auto a=m.apply(b);if(!a.accepted){r.error=a.error;return r;}if(!deuteros_amiga_->observe_title_tail_copy(o)){r.error="Deuteros title-tail copy disappeared before commit";return r;}*native_runtime_memory_=std::move(m);r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::advance_deuteros_amiga_title_tail_subroutine(){DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros title-tail subroutine requires active title session";return r;}try{auto p=*deuteros_amiga_->title_stage_session();if(!p.advance_title_tail_subroutine()){r.error="Deuteros title-tail subroutine did not match boundary";return r;}if(!deuteros_amiga_->advance_title_tail_subroutine()){r.error="Deuteros title-tail subroutine disappeared before commit";return r;}r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_tail_initial_service_return(const DeuterosAmigaObservedLocalCallReturn o){DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros title-tail service return requires active title session";return r;}try{auto p=*deuteros_amiga_->title_stage_session();const auto plan=p.observe_title_tail_initial_service_return(o);if(!plan){r.error="Deuteros title-tail service return did not match boundary";return r;}auto m=*native_runtime_memory_;NativeRuntimeEffectBatch b{"deuteros-amiga-title-tail-work-area",true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->word_destination},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,plan->word_value},{2,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->long_destination},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,plan->long_value}}};const auto a=m.apply(b);if(!a.accepted){r.error=a.error;return r;}if(!deuteros_amiga_->observe_title_tail_initial_service_return(o)){r.error="Deuteros title-tail service return disappeared before commit";return r;}*native_runtime_memory_=std::move(m);r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_tail_exec_return(const DeuterosAmigaObservedTitleTailExecReturn o){DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros title-tail Exec return requires active title session";return r;}try{auto p=*deuteros_amiga_->title_stage_session();if(!p.observe_title_tail_exec_return(o)){r.error="Deuteros title-tail Exec return did not match boundary";return r;}if(!deuteros_amiga_->observe_title_tail_exec_return(o)){r.error="Deuteros title-tail Exec return disappeared before commit";return r;}r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_tail_compare_longs(const DeuterosAmigaObservedTitleTailCompareLongs o){DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros title-tail comparison requires active title session";return r;}try{auto p=*deuteros_amiga_->title_stage_session();if(!p.observe_title_tail_compare_longs(o)){r.error="Deuteros title-tail comparison did not match boundary";return r;}if(!deuteros_amiga_->observe_title_tail_compare_longs(o)){r.error="Deuteros title-tail comparison disappeared before commit";return r;}r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_title_tail_controller_long(const DeuterosAmigaObservedTitleTailControllerLong o){DeuterosAmigaTitleDependencyObservationResult r;if(!active_||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){r.error="Deuteros title-tail controller read requires active title session";return r;}try{auto p=*deuteros_amiga_->title_stage_session();const auto plan=p.observe_title_tail_controller_long(o);if(!plan){r.error="Deuteros title-tail controller read did not match boundary";return r;}auto m=*native_runtime_memory_;std::uint32_t resident_value=0;bool resident_complete=true;for(std::uint32_t i=0;i<4;++i){const auto byte=m.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,static_cast<std::uint64_t>(o.source_address+i)});if(!byte){resident_complete=false;break;}resident_value=(resident_value<<8U)|*byte;}if(resident_complete&&resident_value!=o.value){r.error="Deuteros title-tail controller observation disagrees with owned memory";return r;}NativeRuntimeEffectBatch b{"deuteros-amiga-title-tail-bootstrap",true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->controller_destination},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,plan->controller_value},{2,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->profile_destination},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,plan->profile_value}}};const auto a=m.apply(b);if(!a.accepted){r.error=a.error;return r;}if(!deuteros_amiga_->observe_title_tail_controller_long(o)){r.error="Deuteros title-tail controller read disappeared before commit";return r;}*native_runtime_memory_=std::move(m);r.accepted=true;}catch(const std::exception&e){r.error=e.what();}return r;}
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::advance_deuteros_amiga_title_profile_two_bootstrap(){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){
        r.error="Deuteros profile-two bootstrap requires active title session";return r;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto plan=pending.advance_title_profile_two_bootstrap();
        if(!plan){r.error="Deuteros profile-two bootstrap did not match boundary";return r;}
        auto memory=*native_runtime_memory_;
        const auto read_big32=[&](const std::uint32_t address)->std::optional<std::uint32_t>{
            std::uint32_t value=0;
            for(std::uint32_t i=0;i<4;++i){
                const auto byte=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,
                    static_cast<std::uint64_t>(address+i)});
                if(!byte)return std::nullopt;
                value=(value<<8U)|*byte;
            }
            return value;
        };
        if(read_big32(plan->controller_cell)!=plan->controller_value
            ||read_big32(plan->profile_cell)!=plan->profile_value){
            r.error="Deuteros profile-two bootstrap cells disagree with owned memory";return r;
        }
        if(plan->profile_value!=2){
            r.error="Deuteros profile-two bootstrap has an unexpected profile word";return r;
        }
        NativeRuntimeEffectBatch batch{"deuteros-amiga-profile-two-main-stage-load",true,{}};
        batch.effects.reserve(plan->source_bytes.size()+1U);
        // The original $12a4e dispatcher copies the selected profile's low
        // word from $12ffc to $12a34 before entering the profile loader.
        batch.effects.push_back({1,{NativeRuntimeAddressSpace::linear,std::nullopt,0x12a34},
            MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,
            static_cast<std::uint16_t>(plan->profile_value)});
        for(std::size_t i=0;i<plan->source_bytes.size();++i)
            batch.effects.push_back({i+2,{NativeRuntimeAddressSpace::linear,std::nullopt,
                static_cast<std::uint64_t>(plan->destination_address)+i},MemoryTransferElementWidth::byte,
                NativeRuntimeByteOrder::big_endian,plan->source_bytes[i]});
        const auto applied=memory.apply(batch);
        if(!applied.accepted){r.error=applied.error;return r;}
        if(!deuteros_amiga_->advance_title_profile_two_bootstrap()){
            r.error="Deuteros profile-two bootstrap disappeared before commit";return r;
        }
        *native_runtime_memory_=std::move(memory);r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}
    return r;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_main_stage_reentry_d0(
    const DeuterosAmigaObservedMainStageReentryD0 o) {
    return apply_deuteros_amiga_main_stage_reentry_d0(o);
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_main_stage_reentry_d0() {
    return apply_deuteros_amiga_main_stage_reentry_d0(std::nullopt);
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::apply_deuteros_amiga_main_stage_reentry_d0(
    const std::optional<DeuterosAmigaObservedMainStageReentryD0> observation) {
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!native_runtime_memory_||!deuteros_amiga_->title_stage_session()){
        r.error="Deuteros main-stage re-entry requires active title session";return r;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto plan=observation
            ? pending.observe_main_stage_reentry_d0(*observation)
            : pending.advance_main_stage_reentry_d0();
        if(!plan){r.error="Deuteros main-stage re-entry did not match boundary";return r;}
        auto memory=*native_runtime_memory_;
        if(plan->d0_word_source_span_size==0
            ||plan->d0_word_instruction_address<plan->d0_word_source_span_address
            ||plan->d0_word_instruction_address>=plan->d0_word_source_span_address
                +plan->d0_word_source_span_size){
            r.error="Deuteros re-entry D0 instruction is outside its hash-bound source span";return r;
        }
        std::vector<std::uint8_t> d0_instruction_source;
        d0_instruction_source.reserve(plan->d0_word_source_span_size);
        for(std::uint32_t i=0;i<plan->d0_word_source_span_size;++i){
            const auto byte=memory.read_byte({NativeRuntimeAddressSpace::linear,
                std::nullopt,plan->d0_word_source_span_address+i});
            if(!byte){r.error="Deuteros re-entry D0 instruction source is not owned";return r;}
            d0_instruction_source.push_back(*byte);
        }
        if(to_hex(sha256(d0_instruction_source))!=plan->d0_word_source_sha256){
            r.error="Deuteros re-entry D0 instruction source hash changed";return r;
        }
        const auto d0_source_high=memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt,plan->d0_word_source_address});
        const auto d0_source_low=memory.read_byte({NativeRuntimeAddressSpace::linear,
            std::nullopt,plan->d0_word_source_address+1U});
        if(!d0_source_high||!d0_source_low
            ||static_cast<std::uint16_t>((std::uint16_t(*d0_source_high)<<8U)|*d0_source_low)
                !=plan->d0_word_source_value){
            r.error="Deuteros re-entry D0 word is not backed by the owned profile source";return r;
        }
        std::vector<std::uint8_t> resident_stage;resident_stage.reserve(0x4200);
        for(std::uint32_t i=0;i<0x4200;++i){
            const auto byte=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,
                static_cast<std::uint64_t>(0x20000U+i)});
            if(!byte){r.error="Deuteros main-stage re-entry lacks complete owned stage";return r;}
            resident_stage.push_back(*byte);
        }
        if(to_hex(sha256(resident_stage))
            !="a82c0d6a12e156e0832d632a6c40dd58713a00b611dbcba7289aa16b0969a0a6"){
            r.error="Deuteros main-stage re-entry owned stage hash changed";return r;
        }
        NativeRuntimeEffectBatch batch{"deuteros-amiga-main-stage-reentry-prefix",true,{
            {1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->controller_destination},
                MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,plan->controller_value},
            {2,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->mode_destination},
                MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,plan->mode_value}}};
        const auto applied=memory.apply(batch);
        if(!applied.accepted){r.error=applied.error;return r;}
        const bool committed=observation
            ? static_cast<bool>(deuteros_amiga_->observe_main_stage_reentry_d0(*observation))
            : static_cast<bool>(deuteros_amiga_->advance_main_stage_reentry_d0());
        if(!committed){r.error="Deuteros main-stage re-entry disappeared before commit";return r;}
        *native_runtime_memory_=std::move(memory);r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}
    return r;
}
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_main_stage_first_exec_return(
    const DeuterosAmigaObservedMainStageExecReturn o){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){
        r.error="Deuteros main-stage first Exec return requires active title session";return r;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        if(!pending.observe_main_stage_first_exec_return(o)){
            r.error="Deuteros main-stage first Exec return did not match boundary";return r;
        }
        if(!deuteros_amiga_->observe_main_stage_first_exec_return(o)){
            r.error="Deuteros main-stage first Exec return disappeared before commit";return r;
        }
        r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}
    return r;
}
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_main_stage_second_exec_return(
    const DeuterosAmigaObservedMainStageExecReturn o){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){
        r.error="Deuteros main-stage second Exec return requires active title session";return r;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        if(!pending.observe_main_stage_second_exec_return(o)){
            r.error="Deuteros main-stage second Exec return did not match boundary";return r;
        }
        if(!deuteros_amiga_->observe_main_stage_second_exec_return(o)){
            r.error="Deuteros main-stage second Exec return disappeared before commit";return r;
        }
        r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}
    return r;
}
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_main_stage_first_local_return(
    const DeuterosAmigaObservedLocalCallReturn o){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){
        r.error="Deuteros main-stage first local return requires active title session";return r;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        if(!pending.observe_main_stage_first_local_return(o)){
            r.error="Deuteros main-stage first local return did not match boundary";return r;
        }
        if(!deuteros_amiga_->observe_main_stage_first_local_return(o)){
            r.error="Deuteros main-stage first local return disappeared before commit";return r;
        }
        r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}
    return r;
}
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_main_stage_pointer_service_return(
    const DeuterosAmigaObservedMainStagePointerServiceReturn o){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()
        ||!native_runtime_memory_){
        r.error="Deuteros main-stage pointer service return requires active native memory";return r;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto plan=pending.observe_main_stage_pointer_service_return(o);
        if(!plan){r.error="Deuteros main-stage pointer service return did not match boundary";return r;}
        auto memory=*native_runtime_memory_;
        NativeRuntimeEffectBatch batch{"deuteros-amiga-main-stage-pointer-service",true,{
            {1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->zero_word_destination},
                MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,0},
            {2,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->pointer_destinations[0]},
                MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,o.observed_pointer},
            {3,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->pointer_destinations[1]},
                MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,o.observed_pointer}}};
        const auto applied=memory.apply(batch);
        if(!applied.accepted){r.error=applied.error;return r;}
        if(!deuteros_amiga_->observe_main_stage_pointer_service_return(o)){
            r.error="Deuteros main-stage pointer service return disappeared before commit";return r;
        }
        *native_runtime_memory_=std::move(memory);r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}
    return r;
}
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::observe_deuteros_amiga_main_stage_audio_setup(
    const DeuterosAmigaObservedMainStageAudioSetup o){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()
        ||!native_runtime_memory_){
        r.error="Deuteros main-stage audio setup requires active native memory";return r;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto plan=pending.observe_main_stage_audio_setup(o);
        if(!plan){r.error="Deuteros main-stage audio setup did not match boundary";return r;}
        auto memory=*native_runtime_memory_;
        NativeRuntimeEffectBatch batch{"deuteros-amiga-main-stage-audio-setup",true,{}};
        for(std::size_t i=0;i<plan->custom_register_addresses.size();++i)
            batch.effects.push_back({i+1,{NativeRuntimeAddressSpace::linear,std::nullopt,
                plan->custom_register_addresses[i]},MemoryTransferElementWidth::word,
                NativeRuntimeByteOrder::big_endian,plan->custom_register_values[i]});
        for(std::size_t i=0;i<plan->pointer_destinations.size();++i)
            batch.effects.push_back({batch.effects.size()+1,
                {NativeRuntimeAddressSpace::linear,std::nullopt,plan->pointer_destinations[i]},
                MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,
                o.observed_final_pointers[i]});
        const auto applied=memory.apply(batch);
        if(!applied.accepted){r.error=applied.error;return r;}
        if(!deuteros_amiga_->observe_main_stage_audio_setup(o)){
            r.error="Deuteros main-stage audio setup disappeared before commit";return r;
        }
        *native_runtime_memory_=std::move(memory);r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}
    return r;
}
DeuterosAmigaTitleDependencyObservationResult ReleaseRuntimeCoordinator::advance_deuteros_amiga_main_stage_20994_exec_entry(){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){
        r.error="Deuteros main-stage $20994 entry requires active title session";return r;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        if(!pending.advance_main_stage_20994_exec_entry()){
            r.error="Deuteros main-stage $20994 entry did not match boundary";return r;
        }
        if(!deuteros_amiga_->advance_main_stage_20994_exec_entry()){
            r.error="Deuteros main-stage $20994 entry disappeared before commit";return r;
        }
        r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}
    return r;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_main_stage_2099e_exec_return(
    const DeuterosAmigaObservedMainStageExecReturn o){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()
        ||!native_runtime_memory_){
        r.error="Deuteros main-stage $2099e Exec return requires active title session";
        return r;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto plan=pending.observe_main_stage_2099e_exec_return(o);
        if(!plan){r.error="Deuteros main-stage $2099e Exec return did not match boundary";return r;}
        auto memory=*native_runtime_memory_;
        NativeRuntimeEffectBatch batch{"deuteros-amiga-main-stage-2099e-return",true,{}};
        const std::array widths{MemoryTransferElementWidth::longword,
            MemoryTransferElementWidth::byte,MemoryTransferElementWidth::byte,
            MemoryTransferElementWidth::byte,MemoryTransferElementWidth::longword};
        for(std::size_t i=0;i<plan->destination_addresses.size();++i)
            batch.effects.push_back({i+1,
                {NativeRuntimeAddressSpace::linear,std::nullopt,plan->destination_addresses[i]},
                widths[i],NativeRuntimeByteOrder::big_endian,plan->values[i]});
        const auto applied=memory.apply(batch);
        if(!applied.accepted){r.error=applied.error;return r;}
        if(!deuteros_amiga_->observe_main_stage_2099e_exec_return(o)){
            r.error="Deuteros main-stage $2099e Exec return disappeared before commit";
            return r;
        }
        *native_runtime_memory_=std::move(memory);r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}
    return r;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_main_stage_209ca_exec_return(
    const DeuterosAmigaObservedMainStageExecReturn o){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()
        ||!native_runtime_memory_){
        r.error="Deuteros main-stage $209ca Exec return requires active title session";
        return r;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto plan=pending.observe_main_stage_209ca_exec_return(o);
        if(!plan){r.error="Deuteros main-stage $209ca Exec return did not match boundary";return r;}
        auto memory=*native_runtime_memory_;
        NativeRuntimeEffectBatch batch{"deuteros-amiga-main-stage-209ca-return",true,{}};
        for(std::size_t i=0;i<plan->destination_addresses.size();++i)
            batch.effects.push_back({i+1,
                {NativeRuntimeAddressSpace::linear,std::nullopt,plan->destination_addresses[i]},
                MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,
                plan->values[i]});
        const auto applied=memory.apply(batch);
        if(!applied.accepted){r.error=applied.error;return r;}
        if(!deuteros_amiga_->observe_main_stage_209ca_exec_return(o)){
            r.error="Deuteros main-stage $209ca Exec return disappeared before commit";
            return r;
        }
        *native_runtime_memory_=std::move(memory);r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}
    return r;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_main_stage_209f0_exec_return(
    const DeuterosAmigaObservedMainStageExecReturn o){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()
        ||!native_runtime_memory_){
        r.error="Deuteros main-stage $209f0 Exec return requires active title session";
        return r;
    }
    try{
        if(o.call_a0!=0x20982||o.call_a1!=0x2091c||o.call_d0!=0||o.call_d1!=0){
            r.error="Deuteros main-stage $209f0 OpenDevice registers do not match trackdisk unit 0";
            return r;
        }
        constexpr std::string_view device_name="trackdisk.device";
        for(std::size_t index=0;index<=device_name.size();++index){
            const auto byte=native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,
                std::nullopt,o.call_a0+static_cast<std::uint32_t>(index)});
            const auto expected=index==device_name.size()?0U:
                static_cast<unsigned char>(device_name[index]);
            if(!byte||*byte!=expected){
                r.error="Deuteros main-stage $209f0 A0 does not name the owned trackdisk.device string";
                return r;
            }
        }
        for(std::uint32_t offset=0;offset<0x34;++offset){
            if(!native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,
                    std::nullopt,o.call_a1+offset})){
                r.error="Deuteros main-stage $209f0 IORequest is outside owned native memory";
                return r;
            }
        }
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto plan=pending.observe_main_stage_209f0_exec_return(o);
        if(!plan){r.error="Deuteros main-stage $209f0 Exec return did not match boundary";return r;}
        auto memory=*native_runtime_memory_;
        if(!plan->terminal_spin){
            NativeRuntimeEffectBatch batch{"deuteros-amiga-main-stage-209f0-zero-return",true,{
                {1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->destination_addresses[0]},
                    MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,plan->values[0]},
                {2,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->destination_addresses[1]},
                    MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,plan->values[1]}}};
            const auto applied=memory.apply(batch);
            if(!applied.accepted){r.error=applied.error;return r;}
        }
        if(!deuteros_amiga_->observe_main_stage_209f0_exec_return(o)){
            r.error="Deuteros main-stage $209f0 Exec return disappeared before commit";
            return r;
        }
        *native_runtime_memory_=std::move(memory);r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}
    return r;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_main_stage_cia_a_bit_set(
    const DeuterosAmigaObservedMainStageCiaABitSet o){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()
        ||!native_runtime_memory_){
        r.error="Deuteros main-stage CIA-A bit-set requires active title session";
        return r;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto plan=pending.observe_main_stage_cia_a_bit_set(o);
        if(!plan){r.error="Deuteros main-stage CIA-A bit-set did not match boundary";return r;}
        auto memory=*native_runtime_memory_;
        const auto high=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,
            o.source_word_address});
        const auto low=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,
            o.source_word_address+1U});
        if(!high||!low||static_cast<std::uint16_t>((std::uint16_t(*high)<<8U)|*low)
                !=o.source_word){
            r.error="Deuteros main-stage CIA-A continuation source word contradicts owned memory";
            return r;
        }
        const auto execution = execute_deuteros_cia_prefix(
            deuteros_amiga_->main_stage_cia_prefix_code(), o.prior_port_value, o.source_word);
        if (execution.stop_address != 0x217f2 || execution.instructions_executed != 2U
            || execution.port_value != plan->resulting_port_value
            || static_cast<std::uint16_t>(execution.data_register_zero) != plan->values[0]) {
            r.error="Deuteros main-stage CIA prefix execution contradicts its admitted continuation";
            return r;
        }
        NativeRuntimeEffectBatch batch{"deuteros-amiga-main-stage-cia-a-bit-set",true,{
            {1,{NativeRuntimeAddressSpace::linear,std::nullopt,o.port_address},
                MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,
                plan->resulting_port_value},
            {2,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->destination_addresses[0]},
                MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,plan->values[0]},
            {3,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->destination_addresses[1]},
                MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,plan->values[1]}}};
        const auto applied=memory.apply(batch);
        if(!applied.accepted){r.error=applied.error;return r;}
        if(!deuteros_amiga_->observe_main_stage_cia_a_bit_set(o)){
            r.error="Deuteros main-stage CIA-A bit-set disappeared before commit";
            return r;
        }
        *native_runtime_memory_=std::move(memory);r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}
    return r;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_main_stage_resource_load(
    const DeuterosAmigaObservedMainStageResourceLoad o){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()
        ||!native_runtime_memory_){r.error="Deuteros main-stage resource load requires active title session";return r;}
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto plan=pending.observe_main_stage_resource_load(o);
        if(!plan){r.error="Deuteros main-stage resource load did not match caller";return r;}
        const auto transfer=deuteros_amiga_->main_resource_transfer(o.resource_index);
        if(!transfer||transfer->resource_index!=o.resource_index||transfer->payload.size()<4
            ||transfer->payload_destination_address!=plan->payload_destination
            ||transfer->probe_destination_address!=plan->probe_destination){
            r.error="Deuteros main-stage resource media transfer is unavailable";return r;
        }
        auto memory=*native_runtime_memory_;
        NativeRuntimeEffectBatch batch{"deuteros-amiga-main-stage-resource-"
            +std::to_string(o.resource_index)+"-"+std::to_string(o.trace_sequence),true,{}};
        const auto read=[&](std::uint32_t address,std::uint32_t width){
            if((width>1&&(address&1U))||address>0x1000000U-width)
                throw std::runtime_error("Deuteros resource source is outside aligned native memory");
            std::uint32_t value=0;
            for(std::uint32_t i=0;i<width;++i){
                const auto byte=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address+i});
                if(!byte)throw std::runtime_error("Deuteros recurring resource source is not owned");
                value=(value<<8U)|*byte;
            }
            return value;
        };
        if(plan->recurring){
            if(read(0x21708+o.resource_index*4U,4)!=transfer->source_disk_offset)
                throw std::runtime_error("Deuteros owned resource table contradicts supplied media");
            for(const auto address:{0x21704U,0x21706U})
                batch.effects.push_back({batch.effects.size()+1,{NativeRuntimeAddressSpace::linear,std::nullopt,address},
                    MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,o.resource_index});
        }
        batch.effects.reserve(4U+transfer->payload.size());
        for(std::size_t i=0;i<4;++i)batch.effects.push_back({batch.effects.size()+1,
            {NativeRuntimeAddressSpace::linear,std::nullopt,plan->probe_destination+i},
            MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,
            transfer->payload[i]});
        for(std::size_t i=0;i<transfer->payload.size();++i)batch.effects.push_back({batch.effects.size()+1,
            {NativeRuntimeAddressSpace::linear,std::nullopt,plan->payload_destination+i},
            MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,
            transfer->payload[i]});
        if(plan->recurring)batch.effects.push_back({batch.effects.size()+1,
            {NativeRuntimeAddressSpace::linear,std::nullopt,0xdff016},MemoryTransferElementWidth::byte,
            NativeRuntimeByteOrder::big_endian,o.retry_port_value});
        const auto applied=memory.apply(batch);
        if(!applied.accepted){r.error=applied.error;return r;}
        std::uint32_t restart_d0=0;
        if(plan->recurring&&plan->next_call_address==0x21816){
            std::size_t stores=0;
            const auto write=[&](std::uint32_t address,MemoryTransferElementWidth width,std::uint32_t value){
                const auto effect=memory.apply({"deuteros-amiga-resource-restart-"+std::to_string(o.trace_sequence)+
                    "-"+std::to_string(stores++),true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,address},
                    width,NativeRuntimeByteOrder::big_endian,value}}});
                if(!effect.accepted)throw std::runtime_error(effect.error);
            };
            restart_d0=restart_deuteros_amiga_owned_loop(read,write);
        }
        if(!deuteros_amiga_->observe_main_stage_resource_load(o,restart_d0)){
            r.error="Deuteros main-stage resource load disappeared before commit";return r;}
        *native_runtime_memory_=std::move(memory);r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}
    return r;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_main_stage_loop_service_return(
    const DeuterosAmigaObservedLocalCallReturn o){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()
        ||!native_runtime_memory_){r.error="Deuteros main-stage loop service return requires active title session";return r;}
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto plan=pending.observe_main_stage_loop_service_return(o);
        if(!plan){r.error="Deuteros main-stage loop service return did not match boundary";return r;}
        auto memory=*native_runtime_memory_;
        NativeRuntimeEffectBatch batch{"deuteros-amiga-main-stage-loop-state",true,{}};
        for(std::size_t i=0;i<plan->destination_addresses.size();++i)
            batch.effects.push_back({i+1,
                {NativeRuntimeAddressSpace::linear,std::nullopt,plan->destination_addresses[i]},
                MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,
                plan->values[i]});
        const auto applied=memory.apply(batch);
        if(!applied.accepted){r.error=applied.error;return r;}
        if(!deuteros_amiga_->observe_main_stage_loop_service_return(o)){
            r.error="Deuteros main-stage loop service return disappeared before commit";return r;}
        *native_runtime_memory_=std::move(memory);r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}
    return r;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_main_stage_loop_prepare_body(){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()
        ||!native_runtime_memory_){r.error="Deuteros main-stage loop prepare body requires active title session";return r;}
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto plan=pending.advance_main_stage_loop_prepare_body();
        if(!plan){r.error="Deuteros main-stage loop prepare body did not match boundary";return r;}
        auto memory=*native_runtime_memory_;
        const auto read16=[&](const std::uint32_t address)->std::optional<std::uint16_t>{
            const auto high=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address});
            const auto low=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address+1U});
            if(!high||!low)return std::nullopt;
            return static_cast<std::uint16_t>((std::uint16_t(*high)<<8U)|*low);
        };
        const auto read32=[&](const std::uint32_t address)->std::optional<std::uint32_t>{
            std::uint32_t value=0;
            for(std::uint32_t i=0;i<4;++i){
                const auto byte=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address+i});
                if(!byte)return std::nullopt;
                value=(value<<8U)|*byte;
            }
            return value;
        };
        const auto count=read16(plan->count_source_address);
        const auto final_word=read16(plan->final_word_source_address);
        const auto a6=read32(plan->a6_source_address);
        if(!count||!final_word||!a6){r.error="Deuteros loop prepare body source is absent from owned memory";return r;}
        NativeRuntimeEffectBatch batch{"deuteros-amiga-main-stage-loop-prepare-body-"+
            std::to_string(memory.diagnostics().applied_batch_count),true,{
            {1,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->cleared_word_address},
                MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,0},
            {2,{NativeRuntimeAddressSpace::linear,std::nullopt,plan->count_destination},
                MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,*count}}};
        std::size_t sequence=3;
        for(std::uint32_t i=0;i<plan->pointer_count;++i){
            const auto source=read32(plan->pointer_source_address+i*4U);
            if(!source){r.error="Deuteros loop prepare pointer table is absent from owned memory";return r;}
            const auto value=*source==0?0U:*source+plan->source_base;
            batch.effects.push_back({sequence++,
                {NativeRuntimeAddressSpace::linear,std::nullopt,plan->pointer_destination+i*4U},
                MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,value});
        }
        std::uint32_t next_a1=0;
        for(std::size_t i=0;i<plan->trailing_source_addresses.size();++i){
            const auto source=read32(plan->trailing_source_addresses[i]);
            if(!source){r.error="Deuteros loop prepare trailing source is absent from owned memory";return r;}
            std::uint32_t value=0;
            if(plan->trailing_destination_addresses[i]==0x207eb)value=*source==0?0U:1U;
            else value=(*source==0&&plan->nullable[i])?0U:*source+plan->source_base;
            if(plan->trailing_destination_addresses[i]==plan->a1_source_address)
                next_a1=value;
            batch.effects.push_back({sequence++,
                {NativeRuntimeAddressSpace::linear,std::nullopt,plan->trailing_destination_addresses[i]},
                plan->destination_widths[i],NativeRuntimeByteOrder::big_endian,value});
        }
        batch.effects.push_back({sequence++,
            {NativeRuntimeAddressSpace::linear,std::nullopt,plan->final_byte_destination},
            MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,
            static_cast<std::uint8_t>(*final_word)});
        batch.effects.push_back({sequence,
            {NativeRuntimeAddressSpace::linear,std::nullopt,plan->second_cleared_word_address},
            MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,0});
        const auto applied=memory.apply(batch);
        if(!applied.accepted){r.error=applied.error;return r;}
        if(!deuteros_amiga_->advance_main_stage_loop_prepare_body(next_a1,*a6)){
            r.error="Deuteros main-stage loop prepare body disappeared before commit";return r;}
        *native_runtime_memory_=std::move(memory);r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}
    return r;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::execute_deuteros_amiga_main_stage_loop_graphics_native_service(){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()
        ||!native_runtime_memory_){
        result.error="Deuteros native LoadRGB4 service requires an active owned title";return result;
    }
    try{
        const auto chain=deuteros_amiga_title_dependency_chain_checkpoint();
        if(!chain){result.error="Deuteros LoadRGB4 caller checkpoint is absent";return result;}
        const bool first=chain->main_stage_state
            ==DeuterosAmigaMainStageState::awaiting_first_loop_graphics_return;
        const bool second=chain->main_stage_state
            ==DeuterosAmigaMainStageState::awaiting_second_loop_graphics_return;
        if(!first&&!second){result.error="Deuteros LoadRGB4 is outside its two recovered caller boundaries";return result;}
        const std::uint32_t expected_call=first?0x21310U:0x2132aU;
        const std::uint32_t expected_return=first?0x21314U:0x2132eU;
        const std::uint32_t a0=first
            ?(chain->main_stage_loop_prepare?chain->main_stage_loop_prepare->a0_value:0U)
            :(chain->main_stage_loop_graphics?chain->main_stage_loop_graphics->a0_value:0U);
        const std::uint32_t a1=first
            ?(chain->main_stage_loop_prepare?chain->main_stage_loop_prepare->a1_value:0U)
            :(chain->main_stage_loop_graphics?chain->main_stage_loop_graphics->a1_value:0U);
        const std::uint32_t d0=first
            ?(chain->main_stage_loop_prepare?chain->main_stage_loop_prepare->d0_value:0U)
            :(chain->main_stage_loop_graphics?chain->main_stage_loop_graphics->d0_value:0U);
        const std::uint32_t a6=first
            ?(chain->main_stage_loop_prepare?chain->main_stage_loop_prepare->a6_value:0U)
            :(chain->main_stage_loop_graphics?chain->main_stage_loop_graphics->a6_value:0U);
        const auto next_call=first
            ?(chain->main_stage_loop_prepare?chain->main_stage_loop_prepare->next_call_address:0U)
            :(chain->main_stage_loop_graphics?chain->main_stage_loop_graphics->next_call_address:0U);
        const auto next_return=first
            ?(chain->main_stage_loop_prepare?chain->main_stage_loop_prepare->next_return_address:0U)
            :(chain->main_stage_loop_graphics?chain->main_stage_loop_graphics->next_return_address:0U);
        const auto next_vector=first
            ?(chain->main_stage_loop_prepare?chain->main_stage_loop_prepare->next_vector:0)
            :(chain->main_stage_loop_graphics?chain->main_stage_loop_graphics->next_vector:0);
        if(next_call!=expected_call||next_return!=expected_return||next_vector!=-0xc0
            ||a0!=(first?0x12e12U:0x12f12U)||a1==0
            ||static_cast<std::uint16_t>(d0)!=16||a6==0){
            result.error="Deuteros LoadRGB4 call does not match its hash-bound A0/A1/D0/A6 contract";return result;
        }
        auto memory=*native_runtime_memory_;
        const auto read16=[&](const std::uint32_t address)->std::optional<std::uint16_t>{
            const auto high=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address});
            const auto low=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address+1U});
            if(!high||!low)return std::nullopt;
            return static_cast<std::uint16_t>((std::uint16_t(*high)<<8U)|*low);
        };
        const auto read32=[&](const std::uint32_t address)->std::optional<std::uint32_t>{
            std::uint32_t value=0;
            for(std::uint32_t index=0;index<4;++index){
                const auto byte=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address+index});
                if(!byte)return std::nullopt;
                value=(value<<8U)|*byte;
            }
            return value;
        };
        std::array<std::uint16_t,16> colors{};
        for(std::size_t index=0;index<colors.size();++index){
            const auto color=read16(a1+static_cast<std::uint32_t>(index*2U));
            if(!color||( *color&0xf000U)!=0){
                result.error="Deuteros LoadRGB4 source palette is absent or not RGB4";return result;
            }
            colors[index]=*color;
        }
        std::vector<NativeRuntimeWriteEffect> effects;
        const auto viewport_map=read32(a0+4U);
        if(!viewport_map){result.error="Deuteros LoadRGB4 ViewPort ColorMap pointer is not owned";return result;}
        if(*viewport_map!=0){
            const auto map_count=read16(*viewport_map+2U);
            const auto map_colors=read32(*viewport_map+4U);
            if(!map_count||!map_colors){
                result.error="Deuteros LoadRGB4 ColorMap count or table pointer is not owned";return result;
            }
            const auto count=std::min<std::uint32_t>(16U,
                std::min<std::uint32_t>(32U,*map_count));
            if(count<16U){
                result.error="Deuteros LoadRGB4 ColorMap has fewer than the caller's 16 entries";return result;
            }
            if(*map_colors!=0){
                effects.reserve(count);
                for(std::uint32_t index=0;index<count;++index){
                    if(!read16(*map_colors+index*2U)){
                        result.error="Deuteros LoadRGB4 destination ColorMap is outside owned memory";return result;
                    }
                    effects.push_back({effects.size()+1,
                        {NativeRuntimeAddressSpace::linear,std::nullopt,*map_colors+index*2U},
                        MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,
                        colors[index]});
                }
            }
        }
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto next_a1=first?read32(0x21266):std::optional<std::uint32_t>{0};
        const auto next_a6=first?read32(0x12fec):std::optional<std::uint32_t>{0};
        if(!next_a1||!next_a6){result.error="Deuteros LoadRGB4 caller reload source is not owned";return result;}
        const auto plan=pending.advance_main_stage_loop_graphics_native_service(*next_a1,*next_a6);
        if(!plan){
            result.error="Deuteros native LoadRGB4 caller transition was rejected";return result;
        }
        for(std::size_t index=0;index<plan->write_count;++index){
            effects.push_back({effects.size()+1,
                {NativeRuntimeAddressSpace::linear,std::nullopt,plan->write_addresses[index]},
                plan->write_widths[index],NativeRuntimeByteOrder::big_endian,
                plan->write_values[index]});
        }
        if(!effects.empty()){
            const auto applied=memory.apply({"deuteros-amiga-native-loadrgb4-"
                +std::to_string(expected_call)+"-"
                +std::to_string(memory.diagnostics().applied_batch_count),true,
                std::move(effects)});
            if(!applied.accepted){result.error=applied.error;return result;}
        }
        std::optional<std::array<std::uint16_t,16>> accepted_palette;
        std::shared_ptr<const DeuterosAmigaMainStageFrameSnapshot> recolored_frame;
        std::optional<DeuterosAmigaMainStageFrameSnapshot> recolored_pending;
        if(second){
            accepted_palette=colors;
            if(deuteros_amiga_pending_main_stage_frame_){
                auto recolored=recolor_deuteros_amiga_main_stage_frame(
                    *deuteros_amiga_pending_main_stage_frame_,*accepted_palette,
                    deuteros_amiga_pending_main_stage_frame_->generation);
                if(!recolored){result.error="Deuteros LoadRGB4 pending frame recolor failed";return result;}
                recolored_pending=std::move(*recolored);
            }
            if(deuteros_amiga_main_stage_frame_){
                auto recolored=recolor_deuteros_amiga_main_stage_frame(
                    *deuteros_amiga_main_stage_frame_,*accepted_palette,
                    deuteros_amiga_main_stage_frame_generation_+1U);
                if(!recolored){result.error="Deuteros LoadRGB4 published frame recolor failed";return result;}
                recolored_frame=std::make_shared<const DeuterosAmigaMainStageFrameSnapshot>(
                    std::move(*recolored));
            }
        }
        if(!deuteros_amiga_->advance_main_stage_loop_graphics_native_service(*next_a1,*next_a6)){
            result.error="Deuteros LoadRGB4 caller disappeared before native commit";return result;
        }
        *native_runtime_memory_=std::move(memory);
        if(accepted_palette){
            deuteros_amiga_main_stage_palette_=*accepted_palette;
            if(recolored_pending)deuteros_amiga_pending_main_stage_frame_=std::move(*recolored_pending);
            if(recolored_frame){
                deuteros_amiga_main_stage_frame_=std::move(recolored_frame);
                ++deuteros_amiga_main_stage_frame_generation_;
            }
        }
        result.accepted=true;
    }catch(const std::exception&error){result.error=error.what();}
    return result;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_main_stage_loop_graphics_return(
    const DeuterosAmigaObservedMainStageExecReturn o){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()
        ||!native_runtime_memory_){
        result.error="Deuteros loop graphics return requires active owned memory";return result;
    }
    try{
        const auto read_long=[&](const std::uint32_t address){
            std::uint32_t value=0;
            for(std::uint32_t i=0;i<4;++i){
                const auto byte=native_runtime_memory_->read_byte(
                    {NativeRuntimeAddressSpace::linear,std::nullopt,address+i});
                if(!byte)throw std::runtime_error("Deuteros loop graphics source is not owned");
                value=(value<<8U)|*byte;
            }
            return value;
        };
        // The first return executes two actual memory reloads. The second
        // reaches a local call without reading either cell again.
        const auto a1=o.call_address==0x21310?read_long(0x21266):0U;
        const auto a6=o.call_address==0x21310?read_long(0x12fec):0U;
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto current=pending.main_stage_loop_graphics_plan();
        const auto plan=pending.advance_main_stage_loop_graphics_return(o,a1,a6);
        if(!plan){
            result.error="Deuteros loop graphics return did not match boundary";return result;
        }
        auto memory=*native_runtime_memory_;
        std::optional<std::array<std::uint16_t,16>> accepted_palette;
        if(o.call_address==0x2132a){
            if(!current||current->next_call_address!=0x2132a)
                throw std::runtime_error("Deuteros main-stage palette call is not pending");
            const auto source=current->a1_value;
            std::array<std::uint16_t,16> palette{};
            for(std::uint32_t color=0;color<palette.size();++color){
                const auto high=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,source+color*2U});
                const auto low=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,source+color*2U+1U});
                if(!high||!low)throw std::runtime_error("Deuteros main-stage palette is not owned");
                palette[color]=static_cast<std::uint16_t>((std::uint16_t(*high)<<8U)|*low);
                if((palette[color]&0xf000U)!=0)throw std::runtime_error("Deuteros main-stage palette is not RGB4");
            }
            accepted_palette=palette;
        }
        if(plan->write_count!=0){
            NativeRuntimeEffectBatch batch{"deuteros-amiga-loop-local-request-"+std::to_string(o.trace_sequence),true,{}};
            for(std::size_t i=0;i<plan->write_count;++i)
                batch.effects.push_back({i+1,
                    {NativeRuntimeAddressSpace::linear,std::nullopt,plan->write_addresses[i]},
                    plan->write_widths[i],NativeRuntimeByteOrder::big_endian,plan->write_values[i]});
            const auto applied=memory.apply(batch);
            if(!applied.accepted){result.error=applied.error;return result;}
        }
        if(!deuteros_amiga_->advance_main_stage_loop_graphics_return(o,a1,a6)){
            result.error="Deuteros loop graphics return disappeared before commit";return result;
        }
        *native_runtime_memory_=std::move(memory);
        if(accepted_palette){
            deuteros_amiga_main_stage_palette_=*accepted_palette;
            if(deuteros_amiga_pending_main_stage_frame_){
                auto recolored=recolor_deuteros_amiga_main_stage_frame(
                    *deuteros_amiga_pending_main_stage_frame_,*accepted_palette,
                    deuteros_amiga_pending_main_stage_frame_->generation);
                if(!recolored)throw std::runtime_error("Deuteros pending frame palette update failed");
                deuteros_amiga_pending_main_stage_frame_=std::move(*recolored);
            }
            if(deuteros_amiga_main_stage_frame_){
                auto recolored=recolor_deuteros_amiga_main_stage_frame(
                    *deuteros_amiga_main_stage_frame_,*accepted_palette,
                    deuteros_amiga_main_stage_frame_generation_+1);
                if(!recolored)throw std::runtime_error("Deuteros published frame palette update failed");
                deuteros_amiga_main_stage_frame_=
                    std::make_shared<const DeuterosAmigaMainStageFrameSnapshot>(std::move(*recolored));
                ++deuteros_amiga_main_stage_frame_generation_;
            }
        }
        result.accepted=true;
    }catch(const std::exception&e){result.error=e.what();}
    return result;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_loop_request_service(
    const DeuterosAmigaObservedLoopRequestService o){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()||!native_runtime_memory_){
        result.error="Deuteros loop request service requires active owned memory";return result;
    }
    try{
        auto memory=*native_runtime_memory_;
        std::uint32_t pointer=0;
        for(std::uint32_t i=0;i<4;++i){
            const auto byte=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,0x2126a+i});
            if(!byte)throw std::runtime_error("Deuteros optional pointer is not owned");
            pointer=(pointer<<8U)|*byte;
            const auto base_byte=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,4+i});
            if(base_byte&&*base_byte!=((o.exec_base>>(24U-i*8U))&0xffU))
                throw std::runtime_error("Deuteros ExecBase observation contradicts owned memory");
        }
        auto pending=*deuteros_amiga_->title_stage_session();
        if(!pending.advance_main_stage_loop_request_return(o,pointer)){
            result.error="Deuteros loop request service did not match boundary";return result;
        }
        std::optional<DeuterosAmigaVertbSession> registered_vertb;
        if(!deuteros_amiga_vertb_session_){
            registered_vertb.emplace(deuteros_amiga_->vertb_handler_profile());
            registered_vertb->observe_registration({o.trace_sequence,o.call_address,
                o.return_address,o.vector,5,0x2086a,0x207ec});
        }
        const auto applied=memory.apply({"deuteros-amiga-loop-exec-base-"+std::to_string(o.trace_sequence),true,{{
            1,{NativeRuntimeAddressSpace::linear,std::nullopt,4},
            MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,o.exec_base}}});
        if(!applied.accepted){result.error=applied.error;return result;}
        if(!deuteros_amiga_->advance_main_stage_loop_request_return(o,pointer)){
            result.error="Deuteros loop request service disappeared before commit";return result;
        }
        *native_runtime_memory_=std::move(memory);
        if(registered_vertb)deuteros_amiga_vertb_session_=std::move(*registered_vertb);
        result.accepted=true;
    }catch(const std::exception&e){result.error=e.what();}
    return result;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_optional_resource_cia(
    const DeuterosAmigaObservedOptionalResourceCia o){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()||!native_runtime_memory_){
        result.error="Deuteros optional resource initialization requires active owned memory";
        return result;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto boundary=pending.main_stage_loop_graphics_plan();
        if(!boundary||boundary->next_call_address!=0x2133c
            ||boundary->local_call_target!=0x22330||boundary->next_return_address!=0x21342
            ||boundary->d0_value==0){
            result.error="Deuteros optional resource initializer did not match boundary";return result;
        }
        auto memory=*native_runtime_memory_;
        const auto read=[&](std::uint32_t address,std::uint32_t width){
            if((width>1&&(address&1U))||width==0||width>4||address>0x1000000U-width)
                throw std::runtime_error("Deuteros optional resource read is outside aligned native memory");
            std::uint32_t value=0;
            for(std::uint32_t byte=0;byte<width;++byte){
                const auto cell=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address+byte});
                if(!cell)throw std::runtime_error("Deuteros optional resource source is not owned");
                value=(value<<8U)|*cell;
            }
            return value;
        };
        std::size_t stores=0;
        const auto write=[&](std::uint32_t address,MemoryTransferElementWidth width,std::uint32_t value){
            const auto size=static_cast<std::uint32_t>(width);
            if((size>1&&(address&1U))||address>0x1000000U-size)
                throw std::runtime_error("Deuteros optional resource write is outside aligned native memory");
            const auto applied=memory.apply({"deuteros-amiga-optional-init-"+
                std::to_string(o.trace_sequence)+"-"+std::to_string(stores++),true,{{
                1,{NativeRuntimeAddressSpace::linear,std::nullopt,address},width,
                NativeRuntimeByteOrder::big_endian,value}}});
            if(!applied.accepted)throw std::runtime_error(applied.error);
        };
        const auto initialized=initialize_deuteros_amiga_owned_optional_resource(
            boundary->d0_value,o.prior_port_value,read,write);
        if(!pending.observe_main_stage_optional_resource_cia(o,initialized)){
            result.error="Deuteros optional resource observation did not match boundary";return result;
        }
        if(!deuteros_amiga_->observe_main_stage_optional_resource_cia(o,initialized)){
            result.error="Deuteros optional resource boundary disappeared before commit";return result;
        }
        *native_runtime_memory_=std::move(memory);result.accepted=true;
    }catch(const std::exception&e){result.error=e.what();}
    return result;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_main_stage_record_loop(){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()||!native_runtime_memory_){
        result.error="Deuteros record loop requires active owned memory";return result;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        if(!pending.advance_main_stage_record_loop()){
            result.error="Deuteros record loop did not match boundary";return result;
        }
        auto memory=*native_runtime_memory_;
        const auto read=[&](const std::uint32_t address,const std::uint32_t width){
            if((address&1U)!=0||address>0x1000000U-width)
                throw std::runtime_error("Deuteros record source is outside aligned native memory");
            std::uint32_t value=0;
            for(std::uint32_t i=0;i<width;++i){
                const auto byte=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address+i});
                if(!byte)throw std::runtime_error("Deuteros record source is not owned");
                value=(value<<8U)|*byte;
            }
            return value;
        };
        std::size_t store_index=0;
        const auto record_generation=memory.diagnostics().applied_batch_count;
        const auto write=[&](const std::uint32_t address,const MemoryTransferElementWidth width,
            const std::uint32_t value){
            const auto applied=memory.apply({"deuteros-amiga-record-store-"+std::to_string(record_generation)+
                "-"+std::to_string(store_index++),true,{{
                1,{NativeRuntimeAddressSpace::linear,std::nullopt,address},width,
                NativeRuntimeByteOrder::big_endian,value}}});
            if(!applied.accepted)throw std::runtime_error(applied.error);
        };
        const auto count=read(0x21248,2);
        // SUBQ.W then DBRA: zero is 65536 iterations, not an empty loop.
        const std::uint32_t iterations=count==0?0x10000U:count;
        std::uint32_t last_a1=0,last_d0=0;
        for(std::uint32_t i=0;i<iterations;++i){
            const auto pointer=read(0x2124a+i*4U,4);
            last_d0=pointer;
            if(pointer==0)continue;
            last_a1=pointer;
            const auto destination=0x210f8+i*24U;
            // Preserve instruction order even when source and destination
            // overlap. All stores remain private until the whole loop succeeds.
            write(destination,MemoryTransferElementWidth::longword,read(pointer,4));
            write(destination+4,MemoryTransferElementWidth::longword,read(pointer+4,4));
            write(destination+8,MemoryTransferElementWidth::word,read(pointer+8,2));
            write(destination+16,MemoryTransferElementWidth::longword,pointer+10U);
            last_d0=pointer+10U;
        }
        if(!deuteros_amiga_->advance_main_stage_record_loop(0x2124a+iterations*4U,last_a1,last_d0)){
            result.error="Deuteros record loop disappeared before commit";return result;
        }
        *native_runtime_memory_=std::move(memory);result.accepted=true;
    }catch(const std::exception&e){result.error=e.what();}
    return result;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_main_stage_scheduler_pass(){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()||!native_runtime_memory_){
        result.error="Deuteros scheduler requires active owned memory";return result;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto prior=pending.main_stage_loop_graphics_plan();
        const auto resumed=prior&&prior->next_instruction_address==0x214aa
            ?prior->command_stop:std::nullopt;
        if(!pending.advance_main_stage_scheduler_pass()){
            result.error="Deuteros scheduler did not match boundary";return result;
        }
        auto memory=*native_runtime_memory_;
        const auto read=[&](std::uint32_t address,std::uint32_t width){
            if((width>1&&(address&1U))||address>0x1000000U-width)
                throw std::runtime_error("Deuteros scheduler source is outside aligned native memory");
            std::uint32_t value=0;
            for(std::uint32_t i=0;i<width;++i){
                const auto byte=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address+i});
                if(!byte)throw std::runtime_error("Deuteros scheduler source is not owned");
                value=(value<<8U)|*byte;
            }
            return value;
        };
        std::size_t stores=0;
        const auto pass_id=memory.diagnostics().applied_batch_count;
        const auto write=[&](std::uint32_t address,MemoryTransferElementWidth width,std::uint32_t value){
            const auto applied=memory.apply({"deuteros-amiga-scheduler-store-"+std::to_string(pass_id)+"-"+std::to_string(stores++),true,{{
                1,{NativeRuntimeAddressSpace::linear,std::nullopt,address},width,NativeRuntimeByteOrder::big_endian,value}}});
            if(!applied.accepted)throw std::runtime_error(applied.error);
        };
        std::optional<DeuterosAmigaOwnedCommandStop> command_stop;
        std::size_t command_budget=resumed?resumed->scheduler_commands_remaining:4096;
        const auto count=resumed?resumed->scheduler_iterations:read(0x21248,2);
        const auto iterations=count?count:0x10000U;
        const auto start_index=resumed?resumed->scheduler_index:0U;
        if(resumed&&(count==0||count>0x10000U||start_index>=count
            ||resumed->record!=0x210f8+start_index*24U))
            throw std::runtime_error("Deuteros scheduler resume state is inconsistent");
        const auto retain_stop=[&](DeuterosAmigaOwnedCommandStop stop,std::uint32_t index){
            stop.scheduler_index=index;stop.scheduler_iterations=iterations;
            stop.scheduler_commands_remaining=command_budget;command_stop=stop;
        };
        for(std::uint32_t i=start_index;i<iterations;++i){
            const auto record=0x210f8+i*24U;
            if(resumed&&i==start_index){
                auto stop=execute_deuteros_amiga_owned_commands(record,resumed->cursor,
                    resumed->d0,read,write,command_budget);
                if(stop.instruction){retain_stop(stop,i);break;}
            }
            // Each local RTS branches back to $2138e for this same record.
            // The shared budget also bounds zero-duration yielding loops.
            for(;;){
            const auto cursor=read(record+16,4);
            if(cursor==0)break;
            const auto mode=read(record+6,2);
            bool revisit=false;
            const auto command=[&](std::uint32_t d0){
                auto stop=execute_deuteros_amiga_owned_commands(record,cursor,d0,read,write,command_budget);
                if(stop.instruction)retain_stop(stop,i);
                else revisit=true;
            };
            const auto timer=[&](std::uint32_t d0_high){
                const auto value=read(record+8,2);
                if(value==0)command(d0_high&0xffff0000U);
                else write(record+8,MemoryTransferElementWidth::word,value-1U);
            };
            if(mode!=0&&(mode&0xffU)==3){timer(cursor);}
            else if(mode!=0&&(mode&0xffU)==5){
                if(((read(0x22a20,4)-1U)&0xffffU)==read(record+8,2)
                    &&read(record+10,2)<read(0x22a16,2))
                    command((cursor&0xffff0000U)|read(record+10,2));
            }
            else if(mode!=0&&(mode&0xffU)==6){
                const auto value=read(record+12,4);
                const auto low=value&0xffffU;
                if((low&0xffU)!=0){
                    write(record+14,MemoryTransferElementWidth::word,(low-1U)&0xffffU);
                }else{
                    write(record+14,MemoryTransferElementWidth::word,value>>16U);
                    write(record+4,MemoryTransferElementWidth::word,
                        (read(record+4,2)+read(record+10,2))&0xffffU);
                    timer(value<<16U);
                }
            }
            else if(mode!=0&&(mode&0xffU)==0x14){
                if(read(0x2171e,1)!=0&&read(0x21720,2)!=0)
                    command((cursor&0xffff0000U)|mode);
            }
            else write(record+16,MemoryTransferElementWidth::longword,0);
            if(command_stop||!revisit)break;
            }
            if(command_stop)break;
        }
        if(!deuteros_amiga_->advance_main_stage_scheduler_pass(command_stop)){
            result.error="Deuteros scheduler disappeared before commit";return result;
        }
        *native_runtime_memory_=std::move(memory);result.accepted=true;
    }catch(const std::exception&e){result.error=e.what();}
    return result;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_frame_buffer(const DeuterosAmigaObservedFrameBuffer o){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()||!native_runtime_memory_){
        result.error="Deuteros frame buffer requires active owned memory";return result;
    }
    try{
        auto memory=*native_runtime_memory_;
        std::map<std::uint32_t,std::uint8_t> sprite_bytes;
        const auto read=[&](std::uint32_t address,std::uint32_t width){
            if((width>1&&(address&1U))||address>0x1000000U-width)
                throw std::runtime_error("Deuteros frame source is outside aligned native memory");
            std::uint32_t value=0;
            for(std::uint32_t i=0;i<width;++i){
                const auto pending_byte=sprite_bytes.find(address+i);
                const auto byte=pending_byte!=sprite_bytes.end()?std::optional<std::uint8_t>(pending_byte->second)
                    :memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address+i});
                if(!byte)throw std::runtime_error("Deuteros frame source is not owned");
                value=(value<<8U)|*byte;
            }
            return value;
        };
        const auto counter=static_cast<std::uint16_t>(read(0x21696,2)+1U);
        // $216a4 reads this cell even on the odd branch that overwrites A0.
        static_cast<void>(read(0x12ff4,4));
        auto pending=*deuteros_amiga_->title_stage_session();
        if(!pending.advance_main_stage_frame_buffer(o,counter)){
            result.error="Deuteros frame buffer did not match boundary";return result;
        }
        for(std::uint32_t i=0;i<4;++i){
            const auto existing=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,o.pointer_address+i});
            if(existing&&*existing!=((o.buffer_address>>(24U-i*8U))&0xffU))
                throw std::runtime_error("Deuteros frame pointer contradicts owned memory");
        }
        const auto frame_id=std::to_string(o.trace_sequence);
        NativeRuntimeEffectBatch batch{"deuteros-amiga-frame-clear-"+frame_id,true,{}};
        batch.effects.reserve(8004);
        const auto add=[&](std::uint32_t address,MemoryTransferElementWidth width,std::uint32_t value){
            batch.effects.push_back({batch.effects.size()+1,{NativeRuntimeAddressSpace::linear,std::nullopt,address},
                width,NativeRuntimeByteOrder::big_endian,value});
        };
        add(o.port_address,MemoryTransferElementWidth::byte,o.port_value);
        add(o.pointer_address,MemoryTransferElementWidth::longword,o.buffer_address);
        add(0x21696,MemoryTransferElementWidth::word,counter);
        add(0x20128,MemoryTransferElementWidth::longword,o.buffer_address);
        for(std::uint32_t i=0;i<0x7d00;i+=4)
            add(o.buffer_address+i,MemoryTransferElementWidth::longword,0);
        const auto cleared=memory.apply(batch);
        if(!cleared.accepted){result.error=cleared.error;return result;}
        const auto sprite_write=[&](std::uint32_t address,MemoryTransferElementWidth width,std::uint32_t value){
            const auto bytes=static_cast<std::uint32_t>(width);
            if((bytes>1&&(address&1U))||address>0x1000000U-bytes)
                throw std::runtime_error("Deuteros sprite write is outside aligned native memory");
            for(std::uint32_t i=0;i<bytes;++i)
                sprite_bytes[address+i]=static_cast<std::uint8_t>(value>>((bytes-1-i)*8U));
        };
        const auto count=read(0x21248,2);
        std::uint8_t active_count=0;
        for(std::uint32_t i=0;i<(count?count:0x10000U);++i){
            const auto record=0x210f8+i*24U;
            if(read(record+6,2)==0)continue;
            active_count=static_cast<std::uint8_t>(active_count+1U);
            const auto selector=read(record,2);
            if(selector==0xff)continue;
            if(selector==0xfe){
                apply_deuteros_amiga_owned_alternate_renderer(read(record+12,4),read,sprite_write);
                continue;
            }
            draw_deuteros_amiga_owned_sprite(record,read,sprite_write);
        }
        if(!sprite_bytes.empty()){
            NativeRuntimeEffectBatch sprite_batch{"deuteros-amiga-frame-sprites-"+frame_id,true,{}};
            sprite_batch.effects.reserve(sprite_bytes.size());
            // The read-through overlay already evaluated stores in instruction
            // order. Publish its final unique bytes as one atomic state delta.
            for(const auto& [address,value]:sprite_bytes)
                sprite_batch.effects.push_back({sprite_batch.effects.size()+1,
                    {NativeRuntimeAddressSpace::linear,std::nullopt,address},MemoryTransferElementWidth::byte,
                    NativeRuntimeByteOrder::big_endian,value});
            const auto drawn=memory.apply(sprite_batch);
            if(!drawn.accepted){result.error=drawn.error;return result;}
        }
        const auto counted=memory.apply({"deuteros-amiga-frame-active-count-"+frame_id,true,{{
            1,{NativeRuntimeAddressSpace::linear,std::nullopt,0x210f2},MemoryTransferElementWidth::word,
            NativeRuntimeByteOrder::big_endian,active_count}}});
        if(!counted.accepted){result.error=counted.error;return result;}
        if(!deuteros_amiga_main_stage_palette_){
            result.error="Deuteros frame has no accepted main-stage palette";return result;
        }
        auto staged_frame=decode_deuteros_amiga_main_stage_frame(
            memory,o.buffer_address,counter,*deuteros_amiga_main_stage_palette_,
            deuteros_amiga_main_stage_frame_generation_+1);
        if(!staged_frame){result.error="Deuteros composed frame is incomplete";return result;}
        if(!deuteros_amiga_->advance_main_stage_frame_buffer(o,counter)){
            result.error="Deuteros frame buffer disappeared before commit";return result;
        }
        *native_runtime_memory_=std::move(memory);
        deuteros_amiga_pending_main_stage_frame_=std::move(*staged_frame);
        result.accepted=true;
    }catch(const std::exception&e){result.error=e.what();}
    return result;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_view_selection(){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()||!native_runtime_memory_){
        result.error="Deuteros view selection requires active owned memory";return result;
    }
    try{
        const auto read=[&](std::uint32_t address,std::uint32_t width){
            std::uint32_t value=0;
            for(std::uint32_t i=0;i<width;++i){
                const auto byte=native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address+i});
                if(!byte)throw std::runtime_error("Deuteros view-selection source is not owned");
                value=(value<<8U)|*byte;
            }
            return value;
        };
        const auto counter=static_cast<std::uint16_t>(read(0x21696,2));
        const auto base=read(0x12fec,4);
        auto pending=*deuteros_amiga_->title_stage_session();
        if(!pending.advance_main_stage_view_selection(counter,base)){
            result.error="Deuteros view selection did not match boundary";return result;
        }
        if(!deuteros_amiga_->advance_main_stage_view_selection(counter,base)){
            result.error="Deuteros view selection disappeared before commit";return result;
        }
        result.accepted=true;
    }catch(const std::exception&e){result.error=e.what();}
    return result;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_outer_input(const DeuterosAmigaObservedOuterInput o){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()||!native_runtime_memory_){
        result.error="Deuteros outer input requires active owned memory";return result;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto current=pending.main_stage_loop_graphics_plan();
        if(!current||(current->next_instruction_address!=0x21822&&current->pending_read_instruction!=0x2185e
            &&current->pending_read_instruction!=0x222ac&&current->pending_read_instruction!=0x218be
            &&current->pending_read_instruction!=0x217e4)){
            result.error="Deuteros outer input did not match boundary";return result;
        }
        auto memory=*native_runtime_memory_;
        const auto read=[&](std::uint32_t address,std::uint32_t width){
            if((width>1&&(address&1U))||address>0x1000000U-width)
                throw std::runtime_error("Deuteros outer input source is outside aligned native memory");
            std::uint32_t value=0;
            for(std::uint32_t i=0;i<width;++i){
                const auto byte=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address+i});
                if(!byte)throw std::runtime_error("Deuteros outer input source is not owned");
                value=(value<<8U)|*byte;
            }
            return value;
        };
        std::size_t stores=0;
        const auto write=[&](std::uint32_t address,MemoryTransferElementWidth width,std::uint32_t value){
            const auto applied=memory.apply({"deuteros-amiga-outer-input-"+std::to_string(o.trace_sequence)+
                "-"+std::to_string(stores++),true,{{
                1,{NativeRuntimeAddressSpace::linear,std::nullopt,address},width,NativeRuntimeByteOrder::big_endian,value}}});
            if(!applied.accepted)throw std::runtime_error(applied.error);
        };
        DeuterosAmigaOuterInputRoute route;
        if(o.instruction_address==0x217e4){
            const auto source_word=read(0x21704,2);
            const auto execution = execute_deuteros_cia_prefix(
                deuteros_amiga_->main_stage_cia_prefix_code(), o.value,
                static_cast<std::uint16_t>(source_word), current->d0_value);
            if (execution.stop_address != 0x217f2 || execution.instructions_executed != 2U)
                throw std::runtime_error("Deuteros native CIA prefix crossed its BSR boundary");
            write(0xbfe001,MemoryTransferElementWidth::byte,execution.port_value);
            route={0x21926,execution.data_register_zero,0x217f6};
            route.status_register=execution.status_register;
            route.instructions_executed=execution.instructions_executed;
        }else{
            route=execute_deuteros_amiga_owned_outer_input(o.instruction_address,o.value,
                current->d0_value,read,write,deuteros_amiga_->main_stage_outer_input_code());
        }
        if(!pending.observe_main_stage_outer_input(o,route)){
            result.error="Deuteros outer input route rejected";return result;
        }
        if(!deuteros_amiga_->observe_main_stage_outer_input(o,route)){
            result.error="Deuteros outer input disappeared before commit";return result;
        }
        *native_runtime_memory_=std::move(memory);result.accepted=true;
    }catch(const std::exception&e){result.error=e.what();}
    return result;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_outer_service(const DeuterosAmigaObservedLoopRequestService o){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()||!native_runtime_memory_){
        result.error="Deuteros outer service requires active owned memory";return result;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto current=pending.main_stage_loop_graphics_plan();
        if(!current){result.error="Deuteros outer service did not match boundary";return result;}
        auto memory=*native_runtime_memory_;
        std::map<std::uint32_t,std::uint8_t> service_bytes;
        const auto read=[&](std::uint32_t address,std::uint32_t width){
            if((width>1&&(address&1U))||address>0x1000000U-width)
                throw std::runtime_error("Deuteros outer service source is outside aligned native memory");
            std::uint32_t value=0;
            for(std::uint32_t i=0;i<width;++i){
                const auto found=service_bytes.find(address+i);
                const auto byte=found!=service_bytes.end()?std::optional<std::uint8_t>(found->second)
                    :memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address+i});
                if(!byte)throw std::runtime_error("Deuteros outer service source is not owned");
                value=(value<<8U)|*byte;
            }
            return value;
        };
        const auto write=[&](std::uint32_t address,MemoryTransferElementWidth width,std::uint32_t value){
            const auto count=static_cast<std::uint32_t>(width);
            if((count>1&&(address&1U))||address>0x1000000U-count)
                throw std::runtime_error("Deuteros outer service destination is outside aligned native memory");
            for(std::uint32_t byte=0;byte<count;++byte)
                service_bytes[address+byte]=static_cast<std::uint8_t>(value>>((count-1-byte)*8U));
        };
        if(current->next_call_address==0x12ad2){
            const auto request=current->a1_value;
            const auto destination=read(request+40,4);
            const auto payload=deuteros_amiga_->bootstrap_profile_payload(
                destination,read(request+36,4),read(request+44,4));
            // Complete the native read in private memory before executing its
            // caller continuation. Any rejected return discards all bytes.
            for(std::size_t i=0;i<payload.size();++i)
                write(destination+static_cast<std::uint32_t>(i),MemoryTransferElementWidth::byte,payload[i]);
        }
        if(current->next_call_address==0x12b98||current->next_call_address==0x12c0a){
            const auto request=current->a1_value;
            const auto destination=read(request+40,4);
            const auto payload=deuteros_amiga_->bootstrap_profile_five_payload(
                destination,read(request+36,4),read(request+44,4));
            for(std::size_t i=0;i<payload.size();++i)
                write(destination+static_cast<std::uint32_t>(i),MemoryTransferElementWidth::byte,payload[i]);
        }
        if(current->next_call_address==0x13354){
            const auto request=current->a1_value;
            const auto destination=read(request+40,4);
            const auto payload=deuteros_amiga_->bootstrap_auxiliary_payload(
                destination,read(request+36,4),read(request+44,4));
            for(std::size_t i=0;i<payload.size();++i)
                write(destination+static_cast<std::uint32_t>(i),MemoryTransferElementWidth::byte,payload[i]);
        }
        const auto service_plan=execute_deuteros_amiga_outer_service(*current,o,read,write);
        auto plan=service_plan;
        if(!pending.observe_main_stage_outer_service(o,service_plan)){
            result.error="Deuteros outer service continuation rejected";return result;
        }
        if(plan.next_call_address==0x13358&&plan.local_call_target==0x1fe00){
            const auto decoded=decode_deuteros_amiga_bootstrap_auxiliary(
                deuteros_amiga_->bootstrap_auxiliary_payload(0x1fe00,0x1800,0xb000));
            plan=execute_deuteros_amiga_auxiliary_local_path(plan,decoded,read,write);
            if(!pending.advance_main_stage_auxiliary_local_path(plan)){
                result.error="Deuteros auxiliary local continuation rejected";return result;
            }
        }
        if(!service_bytes.empty()){
            NativeRuntimeEffectBatch batch{"deuteros-amiga-outer-service-"+std::to_string(o.trace_sequence),true,{}};
            batch.effects.reserve(service_bytes.size());
            for(const auto&[address,value]:service_bytes)
                batch.effects.push_back({batch.effects.size()+1,{NativeRuntimeAddressSpace::linear,std::nullopt,address},
                    MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,value});
            const auto applied=memory.apply(batch);
            if(!applied.accepted)throw std::runtime_error(applied.error);
        }
        std::optional<DeuterosAmigaBootstrapFrameSnapshot> bootstrap_frame;
        std::optional<DeuterosAmigaTitleProgramEntrySnapshot> program_entry;
        std::optional<DeuterosAmigaNativeAudioPreparation> native_audio;
        if(current->next_call_address==0x200b2&&plan.next_instruction_address==0x2178e){
            native_audio=prepare_deuteros_amiga_native_audio(plan.main_stage_audio_results,
                memory,deuteros_amiga_native_audio_generation_+1,o.trace_sequence);
            if(!native_audio->accepted){result.error=native_audio->error;return result;}
        }
        // Publish only after the observed graphics.library LoadRGB4 call has
        // returned. Reaching its call boundary after decrunch/deinterleave is
        // necessary, but is not evidence that the original accepted it.
        if(current->next_call_address==0x133aa){
            bootstrap_frame=decode_deuteros_amiga_bootstrap_frame(
                memory.checkpoint(),read(0x12ff4,4),
                deuteros_amiga_bootstrap_frame_generation_+1);
            if(!bootstrap_frame){
                result.error="Deuteros bootstrap frame bytes did not match the admitted frame";return result;
            }
        }
        if(current->next_call_address==0x12b0a&&plan.next_instruction_address==0x13000
            &&(plan.d0_value&0xffffU)==5&&plan.next_call_address==0&&plan.next_vector==0){
            constexpr std::size_t entry_size=6;
            std::vector<std::uint8_t> owned(entry_size);
            for(std::size_t i=0;i<owned.size();++i){
                const auto byte=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,0x13000+i});
                if(!byte){result.error="Deuteros title program-entry JMP is not owned";return result;}
                owned[i]=*byte;
            }
            const auto opcode=static_cast<std::uint16_t>((owned[0]<<8U)|owned[1]);
            const auto target=(static_cast<std::uint32_t>(owned[2])<<24U)
                |(static_cast<std::uint32_t>(owned[3])<<16U)
                |(static_cast<std::uint32_t>(owned[4])<<8U)|owned[5];
            if(opcode!=0x4ef9||target!=0x40426){result.error="Deuteros title program-entry JMP did not match admitted media";return result;}
            program_entry=DeuterosAmigaTitleProgramEntrySnapshot{
                5,0x13000,0x40426,plan.a1_value,memory.checkpoint().checksum,to_hex(sha256(owned))};
        }
        if(!deuteros_amiga_->observe_main_stage_outer_service(o,service_plan)){
            result.error="Deuteros outer service disappeared before commit";return result;
        }
        if(plan.next_call_address==0x133aa
            &&!deuteros_amiga_->advance_main_stage_auxiliary_local_path(plan)){
            result.error="Deuteros auxiliary local continuation disappeared before commit";return result;
        }
        if(bootstrap_frame){
            deuteros_amiga_bootstrap_frame_=std::move(bootstrap_frame);
            ++deuteros_amiga_bootstrap_frame_generation_;
        }
        *native_runtime_memory_=std::move(memory);
        if(program_entry){
            deuteros_amiga_title_program_entry_=std::move(program_entry);
            session_snapshot_=make_runtime_session_snapshot(
                *active_,RuntimeSessionKind::deuteros_amiga_title_program_entry);
        }
        if(native_audio){
            auto mixer=std::make_unique<DeuterosAmigaNativeAudioMixer>();
            static_cast<void>(mixer->install(std::move(native_audio->voices)));
            deuteros_amiga_native_audio_checkpoint_=std::move(native_audio->checkpoint);
            deuteros_amiga_native_audio_mixer_=std::move(mixer);
            ++deuteros_amiga_native_audio_generation_;
        }
        result.accepted=true;
    }catch(const std::exception&e){result.error=e.what();}
    return result;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::begin_deuteros_amiga_disk_transition(const AmigaDiskKind selected_disk){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()||!native_runtime_memory_){
        result.error="Deuteros disk transition requires active owned memory";return result;
    }
    if(selected_disk!=AmigaDiskKind::dos&&selected_disk!=AmigaDiskKind::deuteros_data){
        result.error="Deuteros disk transition requires an explicitly selected disk";return result;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto current=pending.main_stage_loop_graphics_plan();
        if(!current||current->next_instruction_address!=0x219f8||deuteros_amiga_disk_transition_){
            result.error="Deuteros disk transition did not match boundary";return result;
        }
        auto memory=*native_runtime_memory_;
        std::map<std::uint32_t,std::uint8_t> writes;
        const auto read=[&](const std::uint32_t address,const std::uint32_t width){
            if((width>1&&(address&1U))||address>0x1000000U-width)
                throw std::runtime_error("Deuteros disk transition source is outside aligned native memory");
            std::uint32_t value=0;
            for(std::uint32_t i=0;i<width;++i){
                const auto found=writes.find(address+i);
                const auto byte=found!=writes.end()?std::optional<std::uint8_t>(found->second)
                    :memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address+i});
                if(!byte)throw std::runtime_error("Deuteros disk transition source is not owned");
                value=(value<<8U)|*byte;
            }
            return value;
        };
        const auto write=[&](const std::uint32_t address,const MemoryTransferElementWidth width,
            const std::uint32_t value){
            const auto count=static_cast<std::uint32_t>(width);
            if((count>1&&(address&1U))||address>0x1000000U-count)
                throw std::runtime_error("Deuteros disk transition destination is outside aligned native memory");
            for(std::uint32_t i=0;i<count;++i)
                writes[address+i]=static_cast<std::uint8_t>(value>>((count-1-i)*8U));
        };
        const auto boot=deuteros_amiga_->disk_transition_boot_block(selected_disk);
        const auto media_read=[&](const std::uint32_t offset,const std::uint32_t width){
            if(width!=1||offset>=boot.size())throw std::runtime_error("Deuteros disk transition media read is outside boot block");
            return static_cast<std::uint32_t>(boot[offset]);
        };
        const auto plan=begin_deuteros_amiga_owned_disk_transition(boot.size(),read,write,media_read);
        if(!pending.advance_main_stage_disk_transition(plan)){
            result.error="Deuteros disk transition continuation rejected";return result;
        }
        NativeRuntimeEffectBatch batch{"deuteros-amiga-disk-transition",true,{}};
        batch.effects.reserve(writes.size());
        for(const auto&[address,value]:writes)
            batch.effects.push_back({batch.effects.size()+1,{NativeRuntimeAddressSpace::linear,std::nullopt,address},
                MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,value});
        const auto applied=memory.apply(batch);
        if(!applied.accepted)throw std::runtime_error(applied.error);
        if(!deuteros_amiga_->advance_main_stage_disk_transition(plan)){
            result.error="Deuteros disk transition disappeared before commit";return result;
        }
        *native_runtime_memory_=std::move(memory);
        if(plan.state==DeuterosAmigaOwnedDiskTransitionState::awaiting_palette_return)
            deuteros_amiga_disk_transition_=plan;
        result.accepted=true;
    }catch(const std::exception&e){result.error=e.what();}
    return result;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_disk_transition_palette_return(
    const DeuterosAmigaObservedDiskTransitionPaletteReturn o){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()
        ||!native_runtime_memory_||!deuteros_amiga_disk_transition_){
        result.error="Deuteros disk-message palette return requires an active transition";return result;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();auto memory=*native_runtime_memory_;
        std::map<std::uint32_t,std::uint8_t> writes;
        const auto read=[&](const std::uint32_t address,const std::uint32_t width){
            if((width>1&&(address&1U))||address>0x1000000U-width)throw std::runtime_error("Deuteros disk-message source is invalid");
            std::uint32_t value=0;
            for(std::uint32_t i=0;i<width;++i){
                const auto found=writes.find(address+i);
                const auto byte=found!=writes.end()?std::optional<std::uint8_t>(found->second):memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address+i});
                if(!byte)throw std::runtime_error("Deuteros disk-message source is not owned");
                value=(value<<8U)|*byte;
            }return value;
        };
        const auto write=[&](const std::uint32_t address,const MemoryTransferElementWidth width,const std::uint32_t value){
            const auto count=static_cast<std::uint32_t>(width);
            if((count>1&&(address&1U))||address>0x1000000U-count)throw std::runtime_error("Deuteros disk-message destination is invalid");
            for(std::uint32_t i=0;i<count;++i)writes[address+i]=static_cast<std::uint8_t>(value>>((count-1-i)*8U));
        };
        const auto plan=resume_deuteros_amiga_owned_disk_transition_palette(*deuteros_amiga_disk_transition_,o,read,write);
        if(!pending.advance_main_stage_disk_transition(plan)){result.error="Deuteros disk-message continuation rejected";return result;}
        NativeRuntimeEffectBatch batch{"deuteros-amiga-disk-message",true,{}};batch.effects.reserve(writes.size());
        for(const auto&[address,value]:writes)batch.effects.push_back({batch.effects.size()+1,{NativeRuntimeAddressSpace::linear,std::nullopt,address},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,value});
        const auto applied=memory.apply(batch);if(!applied.accepted)throw std::runtime_error(applied.error);
        if(!deuteros_amiga_->advance_main_stage_disk_transition(plan)){result.error="Deuteros disk-message continuation disappeared before commit";return result;}
        *native_runtime_memory_=std::move(memory);deuteros_amiga_disk_transition_=plan;result.accepted=true;
    }catch(const std::exception&e){result.error=e.what();}return result;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_disk_transition_input(
    const DeuterosAmigaObservedDiskTransitionInput o){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()||!deuteros_amiga_disk_transition_){
        result.error="Deuteros disk-message input requires an active transition";return result;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto plan=resume_deuteros_amiga_owned_disk_transition_input(*deuteros_amiga_disk_transition_,o);
        if(!pending.advance_main_stage_disk_transition(plan)){result.error="Deuteros disk-message input continuation rejected";return result;}
        if(!deuteros_amiga_->advance_main_stage_disk_transition(plan)){result.error="Deuteros disk-message input disappeared before commit";return result;}
        deuteros_amiga_disk_transition_=plan;
        if(plan.state==DeuterosAmigaOwnedDiskTransitionState::retry_disk_check)deuteros_amiga_disk_transition_.reset();
        result.accepted=true;
    }catch(const std::exception&e){result.error=e.what();}return result;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_outer_counter(const DeuterosAmigaObservedOuterCounter o){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()||!native_runtime_memory_){
        result.error="Deuteros outer counter requires active owned memory";return result;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto plan=pending.observe_main_stage_outer_counter(o);
        if(!plan){result.error="Deuteros outer counter did not match boundary";return result;}
        auto memory=*native_runtime_memory_;
        NativeRuntimeEffectBatch batch{"deuteros-amiga-outer-counter-"+std::to_string(o.trace_sequence),true,{{
            1,{NativeRuntimeAddressSpace::linear,std::nullopt,0x2079e},MemoryTransferElementWidth::longword,
            NativeRuntimeByteOrder::big_endian,o.value}}};
        if(plan->next_call_address==0x218f8)
            batch.effects.push_back({2,{NativeRuntimeAddressSpace::linear,std::nullopt,0x12fe4},
                MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,plan->d0_value});
        const auto applied=memory.apply(batch);
        if(!applied.accepted){result.error=applied.error;return result;}
        if(!deuteros_amiga_->observe_main_stage_outer_counter(o)){
            result.error="Deuteros outer counter disappeared before commit";return result;
        }
        *native_runtime_memory_=std::move(memory);result.accepted=true;
    }catch(const std::exception&e){result.error=e.what();}
    return result;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_fade_buffers(){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()||!native_runtime_memory_){
        result.error="Deuteros fade buffers require active owned memory";return result;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto current=pending.main_stage_loop_graphics_plan();
        if(!current||current->next_call_address!=0x2231e){
            result.error="Deuteros fade buffers did not match boundary";return result;
        }
        // Both clears and their return are one private transaction. The
        // second call sees the first call's writes even if buffers alias.
        auto memory=*native_runtime_memory_;
        std::map<std::uint32_t,std::uint8_t> overlay;
        NativeRuntimeEffectBatch batch{"deuteros-amiga-fade-buffers-"+
            std::to_string(memory.diagnostics().applied_batch_count),true,{}};
        const auto read=[&](std::uint32_t address,std::uint32_t width){
            if((width>1&&(address&1U))||address>0x1000000U-width)
                throw std::runtime_error("Deuteros fade buffer source is outside aligned native memory");
            std::uint32_t value=0;
            for(std::uint32_t i=0;i<width;++i){
                const auto found=overlay.find(address+i);
                const auto byte=found!=overlay.end()?std::optional<std::uint8_t>(found->second)
                    :memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address+i});
                if(!byte)throw std::runtime_error("Deuteros fade buffer source is not owned");
                value=(value<<8U)|*byte;
            }
            return value;
        };
        const auto write=[&](std::uint32_t address,MemoryTransferElementWidth width,std::uint32_t value){
            const auto bytes=static_cast<std::uint32_t>(width);
            for(std::uint32_t i=0;i<bytes;++i)
                overlay[address+i]=static_cast<std::uint8_t>(value>>((bytes-1-i)*8U));
        };
        static_cast<void>(clear_deuteros_amiga_owned_frame(read,write));
        const auto final_a0=clear_deuteros_amiga_owned_frame(read,write);
        const auto cleanup_d0=execute_deuteros_amiga_owned_cleanup(read,write);
        if(!pending.advance_main_stage_fade_buffers(final_a0,cleanup_d0)){
            result.error="Deuteros fade buffer continuation rejected";return result;
        }
        // Admission represents final state, not a bus trace. Counters and
        // aliased buffers may have been written repeatedly during execution.
        batch.effects.reserve(overlay.size());
        for(const auto&[address,value]:overlay)
            batch.effects.push_back({batch.effects.size()+1,{NativeRuntimeAddressSpace::linear,std::nullopt,address},
                MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,value});
        const auto applied=memory.apply(batch);
        if(!applied.accepted){result.error=applied.error;return result;}
        if(!deuteros_amiga_->advance_main_stage_fade_buffers(final_a0,cleanup_d0)){
            result.error="Deuteros fade buffers disappeared before commit";return result;
        }
        *native_runtime_memory_=std::move(memory);result.accepted=true;
    }catch(const std::exception&e){result.error=e.what();}
    return result;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::advance_deuteros_amiga_command_palette(){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()||!native_runtime_memory_){
        result.error="Deuteros command palette requires active owned memory";return result;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto current=pending.main_stage_loop_graphics_plan();
        if(!current||!current->command_stop||current->next_instruction_address!=0x214ee){
            result.error="Deuteros command palette did not match boundary";return result;
        }
        const auto read_long=[&](std::uint32_t address){
            std::uint32_t value=0;
            for(std::uint32_t i=0;i<4;++i){
                const auto byte=native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address+i});
                if(!byte)throw std::runtime_error("Deuteros command palette source is not owned");
                value=(value<<8U)|*byte;
            }
            return value;
        };
        const auto index=current->command_stop->d0&0xffffU;
        const auto displacement=(index<<5U)&0xffffU;
        const auto palette=read_long(0x21266)+(displacement<0x8000U?displacement:displacement-0x10000U);
        const auto library=read_long(0x12fec);
        if(!pending.advance_main_stage_command_palette(palette,library)){
            result.error="Deuteros command palette prefix rejected";return result;
        }
        auto memory=*native_runtime_memory_;
        const auto applied=memory.apply({"deuteros-amiga-command-palette-"+
            std::to_string(memory.diagnostics().applied_batch_count),true,{{
            1,{NativeRuntimeAddressSpace::linear,std::nullopt,0x210f6},MemoryTransferElementWidth::word,
            NativeRuntimeByteOrder::big_endian,index}}});
        if(!applied.accepted){result.error=applied.error;return result;}
        if(!deuteros_amiga_->advance_main_stage_command_palette(palette,library)){
            result.error="Deuteros command palette disappeared before commit";return result;
        }
        *native_runtime_memory_=std::move(memory);result.accepted=true;
    }catch(const std::exception&e){result.error=e.what();}
    return result;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_command_palette_return(const DeuterosAmigaObservedMainStageExecReturn o){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()||!native_runtime_memory_){
        result.error="Deuteros command palette return requires active owned memory";return result;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        const auto current=pending.main_stage_loop_graphics_plan();
        std::uint32_t library=0;
        if(current&&(current->next_call_address==0x21514||current->next_call_address==0x222fc)){
            for(std::uint32_t i=0;i<4;++i){
                const auto byte=native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,0x12fec+i});
                if(!byte)throw std::runtime_error("Deuteros command palette library is not owned");
                library=(library<<8U)|*byte;
            }
        }
        auto memory=*native_runtime_memory_;
        std::optional<std::array<std::uint16_t,16>> accepted_palette;
        const bool second_command_palette=current&&current->next_call_address==0x2152a;
        const bool second_fade_palette=current&&current->next_call_address==0x22312;
        if(second_command_palette||second_fade_palette){
            const auto source=second_fade_palette?0x12eccU:current->command_palette_address;
            std::array<std::uint16_t,16> palette{};
            for(std::uint32_t color=0;color<palette.size();++color){
                const auto high=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,source+color*2U});
                const auto low=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,source+color*2U+1U});
                if(!high||!low)throw std::runtime_error("Deuteros accepted palette source is not owned");
                palette[color]=static_cast<std::uint16_t>((std::uint16_t(*high)<<8U)|*low);
                if((palette[color]&0xf000U)!=0)throw std::runtime_error("Deuteros accepted palette source is not RGB4");
            }
            accepted_palette=palette;
        }
        std::optional<std::uint16_t> fade_remaining;
        if(current&&current->outer_fade_return!=0&&current->next_call_address==0x22312){
            const auto high=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,0x2229a});
            const auto low=memory.read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,0x2229b});
            if(!high||!low)throw std::runtime_error("Deuteros fade counter is not owned");
            fade_remaining=static_cast<std::uint16_t>(((static_cast<std::uint32_t>(*high)<<8U)|*low)-8U);
            const auto applied=memory.apply({"deuteros-amiga-fade-counter-"+std::to_string(o.trace_sequence),true,{{
                1,{NativeRuntimeAddressSpace::linear,std::nullopt,0x2229a},MemoryTransferElementWidth::word,
                NativeRuntimeByteOrder::big_endian,*fade_remaining}}});
            if(!applied.accepted)throw std::runtime_error(applied.error);
        }
        if(!pending.observe_main_stage_command_palette_return(o,library,fade_remaining)){
            result.error="Deuteros command palette return did not match boundary";return result;
        }
        if(!deuteros_amiga_->observe_main_stage_command_palette_return(o,library,fade_remaining)){
            result.error="Deuteros command palette return disappeared before commit";return result;
        }
        *native_runtime_memory_=std::move(memory);
        if(accepted_palette){
            deuteros_amiga_main_stage_palette_=*accepted_palette;
            if(deuteros_amiga_pending_main_stage_frame_){
                auto recolored=recolor_deuteros_amiga_main_stage_frame(
                    *deuteros_amiga_pending_main_stage_frame_,*accepted_palette,
                    deuteros_amiga_pending_main_stage_frame_->generation);
                if(!recolored)throw std::runtime_error("Deuteros pending frame palette update failed");
                deuteros_amiga_pending_main_stage_frame_=std::move(*recolored);
            }
            if(deuteros_amiga_main_stage_frame_){
                auto recolored=recolor_deuteros_amiga_main_stage_frame(
                    *deuteros_amiga_main_stage_frame_,*accepted_palette,
                    deuteros_amiga_main_stage_frame_generation_+1);
                if(!recolored)throw std::runtime_error("Deuteros published frame palette update failed");
                deuteros_amiga_main_stage_frame_=
                    std::make_shared<const DeuterosAmigaMainStageFrameSnapshot>(std::move(*recolored));
                ++deuteros_amiga_main_stage_frame_generation_;
            }
        }
        result.accepted=true;
    }catch(const std::exception&e){result.error=e.what();}
    return result;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_view_wait(const DeuterosAmigaObservedViewWait o){
    DeuterosAmigaTitleDependencyObservationResult result;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()||!native_runtime_memory_){
        result.error="Deuteros view wait requires active owned memory";return result;
    }
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        if(!pending.observe_main_stage_view_wait(o)){
            result.error="Deuteros view wait did not match boundary";return result;
        }
        auto memory=*native_runtime_memory_;
        const auto applied=memory.apply({"deuteros-amiga-view-wait-"+std::to_string(o.trace_sequence),true,{{
            1,{NativeRuntimeAddressSpace::linear,std::nullopt,o.port_address},
            MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,o.value}}});
        if(!applied.accepted){result.error=applied.error;return result;}
        if(!deuteros_amiga_->observe_main_stage_view_wait(o)){
            result.error="Deuteros view wait disappeared before commit";return result;
        }
        *native_runtime_memory_=std::move(memory);
        if((o.value&0x20U)!=0&&deuteros_amiga_pending_main_stage_frame_){
            deuteros_amiga_main_stage_frame_=
                std::make_shared<const DeuterosAmigaMainStageFrameSnapshot>(
                    std::move(*deuteros_amiga_pending_main_stage_frame_));
            deuteros_amiga_pending_main_stage_frame_.reset();
            ++deuteros_amiga_main_stage_frame_generation_;
        }
        result.accepted=true;
    }catch(const std::exception&e){result.error=e.what();}
    return result;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_main_stage_loop_prepare_return(
    const DeuterosAmigaObservedLocalCallReturn o){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){
        r.error="Deuteros main-stage loop prepare return requires active title session";return r;}
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        if(!pending.observe_main_stage_loop_prepare_return(o)){
            r.error="Deuteros main-stage loop prepare return did not match boundary";return r;}
        if(!deuteros_amiga_->observe_main_stage_loop_prepare_return(o)){
            r.error="Deuteros main-stage loop prepare return disappeared before commit";return r;}
        r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}
    return r;
}
DeuterosAmigaTitleDependencyObservationResult
ReleaseRuntimeCoordinator::observe_deuteros_amiga_main_stage_loop_scheduler_return(
    const DeuterosAmigaObservedLocalCallReturn o){
    DeuterosAmigaTitleDependencyObservationResult r;
    if(!active_||!deuteros_amiga_||!deuteros_amiga_->title_stage_session()){
        r.error="Deuteros main-stage loop scheduler return requires active title session";return r;}
    try{
        auto pending=*deuteros_amiga_->title_stage_session();
        if(!pending.observe_main_stage_loop_scheduler_return(o)){
            r.error="Deuteros main-stage loop scheduler return did not match boundary";return r;}
        if(!deuteros_amiga_->observe_main_stage_loop_scheduler_return(o)){
            r.error="Deuteros main-stage loop scheduler return disappeared before commit";return r;}
        r.accepted=true;
    }catch(const std::exception&e){r.error=e.what();}
    return r;
}
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
        millennium_amiga_->first_stage_entry_boundary(),
        millennium_amiga_->input_device_open_boundary(),
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

MillenniumAmigaBootstrapRelocatorObservationResult
ReleaseRuntimeCoordinator::observe_millennium_amiga_bootstrap_setup_call_return(
    const MillenniumAmigaBootstrapRelocatorObservation observation){
    MillenniumAmigaBootstrapRelocatorObservationResult result;
    if(!millennium_amiga_relocator_||!millennium_amiga_relocator_terminal_sequence_||millennium_amiga_setup_return_sequence_||observation.sequence<=*millennium_amiga_relocator_terminal_sequence_){result.error="Setup return requires the transferred direct Defjam bootstrap";return result;}
    try{auto next=*millennium_amiga_relocator_;next.observe_setup_call_return(observation.instruction_address,observation.source_or_target_address);*millennium_amiga_relocator_=std::move(next);millennium_amiga_setup_return_sequence_=observation.sequence;result.accepted=true;}catch(const std::exception&e){result.error=std::string("Setup return rejected: ")+e.what();}return result;
}

MillenniumAmigaBootstrapRelocatorObservationResult
ReleaseRuntimeCoordinator::observe_millennium_amiga_bootstrap_first_read_return(
    const MillenniumAmigaBootstrapRelocatorObservation observation){
    MillenniumAmigaBootstrapRelocatorObservationResult result;
    if(!millennium_amiga_relocator_||!native_runtime_memory_||!millennium_amiga_setup_return_sequence_||millennium_amiga_first_read_return_sequence_||observation.sequence<=*millennium_amiga_setup_return_sequence_){result.error="First-read return requires the later direct Defjam setup generation";return result;}
    try{
        auto next=*millennium_amiga_relocator_;
        next.observe_first_read_return(observation.instruction_address,
            observation.source_or_target_address,observation.value);
        NativeRuntimeEffectBatch batch{"millennium-amiga-first-stage-"+
            std::to_string(millennium_amiga_relocator_generation_),true,{}};
        const auto bytes=next.first_stage_bytes();
        batch.effects.reserve(bytes.size());
        for(std::size_t i=0;i<bytes.size();++i){
            batch.effects.push_back({i+1,
                {NativeRuntimeAddressSpace::linear,std::nullopt,0x41000+i},
                MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,bytes[i]});
        }
        auto memory=*native_runtime_memory_;
        const auto applied=memory.apply(batch);
        if(!applied.accepted){result.error="First-stage materialization rejected: "+applied.error;return result;}
        *millennium_amiga_relocator_=std::move(next);
        *native_runtime_memory_=std::move(memory);
        millennium_amiga_first_read_return_sequence_=observation.sequence;
        result.accepted=true;
    }catch(const std::exception&e){result.error=std::string("First-read return rejected: ")+e.what();}return result;
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
        session.final_a5(),session.final_d1(),
        (session.state()==MillenniumAmigaBootstrapRelocatorState::awaiting_opaque_first_stage
            || session.state()==MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_illegal_exception
            || session.state()==MillenniumAmigaBootstrapRelocatorState::awaiting_second_first_stage_illegal_exception
            || session.state()==MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_trace_exception
            || session.state()==MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_decrypted_instruction
            || session.state()==MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_decrypted_memory_write
            || session.state()==MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_custom_chip_write
            || session.state()==MillenniumAmigaBootstrapRelocatorState::awaiting_first_stage_exec_service)
            ? session.first_stage_bytes().size():0,
        session.first_stage_sha256(),session.first_stage_entry_execution(),
        session.first_stage_illegal_execution(),session.second_illegal_execution(),
        session.first_trace_execution(),session.trace_branch_chain_execution(),session.trace_register_prefix_execution(),
        session.bus_error_prefix_execution(),session.custom_chip_exec_prefix_execution(),session.exec_transition_execution(),session.open_graphics_execution(),session.allocation_consumer_execution(),session.graphics_initialization_execution(),session.view_service_execution(),session.interrupt_control_execution()};
}

std::optional<MillenniumAtariBootstrapPresentationSnapshot>
ReleaseRuntimeCoordinator::millennium_atari_bootstrap_presentation() const {
    if (!session_snapshot_ || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_ || !millennium_atari_config_consumer_) return std::nullopt;
    return MillenniumAtariBootstrapPresentationSnapshot{
        atari_st_prg_load_diagnostics(millennium_atari_->native_prg_image()),
        millennium_atari_->title_frame(),
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
        millennium_atari_post_config_entry_
            ? std::optional<MillenniumAtariPostConfigEntryCheckpoint>{
                millennium_atari_post_config_entry_->checkpoint()}
            : std::nullopt,
    };
}

MillenniumAtariConfigConsumerResult
ReleaseRuntimeCoordinator::observe_millennium_atari_post_config_entry_jump(
    const MillenniumAtariPostConfigEntryJumpObservation observation) {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_config_consumer_ || !millennium_atari_post_config_entry_
        || !native_runtime_memory_) {
        return {false, "Post-config entry jump requires active Millennium Atari runtime"};
    }
    if (millennium_atari_config_consumer_->checkpoint().state
            != MillenniumAtariConfigConsumerState::game_init_post_config_complete
        || observation.sequence <= millennium_atari_config_consumer_->checkpoint().last_sequence) {
        return {false, "Post-config entry jump requires completed typed GEMDOS caller return"};
    }
    if (millennium_atari_config_consumer_->checkpoint()
            .game_init_post_config_rts_return_address != 0x11e00U) {
        return {false, "Post-config entry jump requires the loader RTS to return to $11e00"};
    }
    auto next = *millennium_atari_post_config_entry_;
    const auto result = next.observe_entry_jump(observation, *native_runtime_memory_);
    if (result.accepted) *millennium_atari_post_config_entry_ = std::move(next);
    return {result.accepted, result.error};
}

MillenniumAtariConfigConsumerResult
ReleaseRuntimeCoordinator::observe_millennium_atari_post_config_status_register(
    const MillenniumAtariStatusRegisterObservation observation) {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_config_consumer_ || !millennium_atari_post_config_entry_
        || !native_runtime_memory_) {
        return {false, "Post-config SR observation requires active Millennium Atari runtime"};
    }
    try {
        auto next = *millennium_atari_post_config_entry_;
        auto result = next.observe_status_register(observation, *native_runtime_memory_);
        if (!result.accepted) return {false, result.error};
        auto memory = *native_runtime_memory_;
        const auto batch = next.make_hardware_effect_batch(
            "millennium-atari-post-config-sr-" + std::to_string(observation.generation)
                + "-" + std::to_string(observation.sequence));
        if (!batch.effects.empty()) {
            const auto applied = memory.apply(batch);
            if (!applied.accepted) {
                return {false, "Post-config PSG effect rejected: " + applied.error};
            }
        }
        *millennium_atari_post_config_entry_ = std::move(next);
        *native_runtime_memory_ = std::move(memory);
        return {true, {}};
    } catch (const std::exception& error) {
        return {false, std::string("Post-config SR observation rejected: ") + error.what()};
    }
}

MillenniumAtariConfigConsumerResult
ReleaseRuntimeCoordinator::observe_millennium_atari_post_config_xbios_26_return(
    const MillenniumAtariPostConfigXbiosReturnObservation observation) {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_post_config_entry_) {
        return {false, "Post-config XBIOS return requires active Millennium Atari runtime"};
    }
    auto next = *millennium_atari_post_config_entry_;
    const auto result = next.observe_xbios_26_return(observation);
    if (result.accepted) *millennium_atari_post_config_entry_ = std::move(next);
    return {result.accepted, result.error};
}

MillenniumAtariConfigConsumerResult
ReleaseRuntimeCoordinator::observe_millennium_atari_post_config_xbios_result(
    const MillenniumAtariPostConfigXbiosResultObservation observation) {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_post_config_entry_ || !native_runtime_memory_) {
        return {false, "Post-config XBIOS result requires active Millennium Atari runtime"};
    }
    try {
        auto next = *millennium_atari_post_config_entry_;
        const auto result = next.observe_xbios_result(observation, *native_runtime_memory_);
        if (!result.accepted) return {false, result.error};
        auto memory = *native_runtime_memory_;
        const auto batch = next.make_xbios_result_effect_batch(
            "millennium-atari-post-config-xbios-" + std::to_string(observation.generation)
                + "-" + std::to_string(observation.sequence));
        if (!batch.effects.empty()) {
            const auto applied = memory.apply(batch);
            if (!applied.accepted) {
                return {false, "Post-config XBIOS result store rejected: " + applied.error};
            }
        }
        *millennium_atari_post_config_entry_ = std::move(next);
        *native_runtime_memory_ = std::move(memory);
        return {true, {}};
    } catch (const std::exception& error) {
        return {false, std::string("Post-config XBIOS result rejected: ") + error.what()};
    }
}

MillenniumAtariConfigConsumerResult
ReleaseRuntimeCoordinator::observe_millennium_atari_post_config_line_a_return(
    const MillenniumAtariPostConfigLineAObservation observation) {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_post_config_entry_ || !native_runtime_memory_) {
        return {false, "Post-config Line-A return requires active Millennium Atari runtime"};
    }
    try {
        auto next = *millennium_atari_post_config_entry_;
        const auto result = next.observe_line_a_return(observation, *native_runtime_memory_);
        if (!result.accepted) return {false, result.error};
        auto memory = *native_runtime_memory_;
        const auto batch = next.make_line_a_continuation_effect_batch(
            "millennium-atari-post-config-line-a-" + std::to_string(observation.generation)
                + "-" + std::to_string(observation.sequence));
        if (!batch.effects.empty()) {
            const auto applied = memory.apply(batch);
            if (!applied.accepted) {
                return {false, "Post-config Line-A continuation rejected: " + applied.error};
            }
        }
        *millennium_atari_post_config_entry_ = std::move(next);
        *native_runtime_memory_ = std::move(memory);
        return {true, {}};
    } catch (const std::exception& error) {
        return {false, std::string("Post-config Line-A return rejected: ") + error.what()};
    }
}

MillenniumAtariConfigConsumerResult
ReleaseRuntimeCoordinator::observe_millennium_atari_post_config_local_call(
    MillenniumAtariPostConfigLocalCallObservation observation) {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_post_config_entry_ || !native_runtime_memory_) {
        return {false, "Post-config local call requires active Millennium Atari runtime"};
    }
    try {
        auto next = *millennium_atari_post_config_entry_;
        const auto result = next.observe_local_call_2340c(observation, *native_runtime_memory_);
        if (!result.accepted) return {false, result.error};
        auto memory = *native_runtime_memory_;
        const auto batch = next.make_local_call_2340c_effect_batch(
            "millennium-atari-post-config-local-call-" + std::to_string(observation.generation)
                + "-" + std::to_string(observation.sequence));
        const auto applied = memory.apply(batch);
        if (!applied.accepted) {
            return {false, "Post-config local call effects rejected: " + applied.error};
        }
        const auto continuation = next.execute_bytecode_continuation(memory);
        if (!continuation.accepted) {
            return {false, "Post-config bytecode continuation rejected: "
                + continuation.error};
        }
        const auto continuation_batch = next.make_bytecode_continuation_effect_batch(
            "millennium-atari-post-config-bytecode-"
                + std::to_string(observation.generation) + "-"
                + std::to_string(observation.sequence));
        const auto continuation_applied = memory.apply(continuation_batch);
        if (!continuation_applied.accepted) {
            return {false, "Post-config bytecode effects rejected: "
                + continuation_applied.error};
        }
        *millennium_atari_post_config_entry_ = std::move(next);
        *native_runtime_memory_ = std::move(memory);
        return {true, {}};
    } catch (const std::exception& error) {
        return {false, std::string("Post-config local call rejected: ") + error.what()};
    }
}

MillenniumAtariConfigConsumerResult
ReleaseRuntimeCoordinator::observe_millennium_atari_post_config_crawcin_branch(
    const MillenniumAtariPostConfigCrawcinBranchObservation observation) {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_post_config_entry_ || !native_runtime_memory_) {
        return {false, "Post-config Crawcin branch requires active Millennium Atari runtime"};
    }
    auto next = *millennium_atari_post_config_entry_;
    const auto result = next.observe_crawcin_flag_branch(observation, *native_runtime_memory_);
    if (result.accepted) *millennium_atari_post_config_entry_ = std::move(next);
    return {result.accepted, result.error};
}

MillenniumAtariConfigConsumerResult
ReleaseRuntimeCoordinator::observe_millennium_atari_post_config_gemdos_fopen(
    const MillenniumAtariPostConfigFopenObservation observation) {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_post_config_entry_ || !native_runtime_memory_) {
        return {false, "Post-config GEMDOS Fopen requires active Millennium Atari runtime"};
    }
    auto next = *millennium_atari_post_config_entry_;
    const auto result = next.observe_gemdos_fopen(observation, *native_runtime_memory_);
    if (result.accepted) *millennium_atari_post_config_entry_ = std::move(next);
    return {result.accepted, result.error};
}

MillenniumAtariConfigConsumerResult
ReleaseRuntimeCoordinator::observe_millennium_atari_post_config_gemdos_fopen_return(
    const MillenniumAtariPostConfigFopenReturnObservation observation) {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_post_config_entry_ || !native_runtime_memory_) {
        return {false, "Post-config GEMDOS Fopen return requires active Millennium Atari runtime"};
    }
    try {
        auto next = *millennium_atari_post_config_entry_;
        const auto result = next.observe_gemdos_fopen_return(observation, *native_runtime_memory_);
        if (!result.accepted) return {false, result.error};
        auto memory = *native_runtime_memory_;
        auto batch = next.make_gemdos_fopen_result_effect_batch(
            "millennium-atari-post-config-fopen-result-"
                + std::to_string(observation.generation) + "-"
                + std::to_string(observation.sequence));
        const auto applied = memory.apply(batch);
        if (!applied.accepted) return {false, "Post-config Fopen result store rejected: " + applied.error};
        *millennium_atari_post_config_entry_ = std::move(next);
        *native_runtime_memory_ = std::move(memory);
        return {true, {}};
    } catch (const std::exception& error) {
        return {false, std::string("Post-config GEMDOS Fopen return rejected: ") + error.what()};
    }
}

MillenniumAtariConfigConsumerResult
ReleaseRuntimeCoordinator::observe_millennium_atari_post_config_gemdos_fcreate(
    const MillenniumAtariPostConfigFcreateObservation observation) {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_post_config_entry_ || !native_runtime_memory_) {
        return {false, "Post-config GEMDOS Fcreate requires active Millennium Atari runtime"};
    }
    auto next = *millennium_atari_post_config_entry_;
    const auto result = next.observe_gemdos_fcreate(observation, *native_runtime_memory_);
    if (result.accepted) *millennium_atari_post_config_entry_ = std::move(next);
    return {result.accepted, result.error};
}

MillenniumAtariConfigConsumerResult
ReleaseRuntimeCoordinator::observe_millennium_atari_post_config_gemdos_fcreate_return(
    const MillenniumAtariPostConfigFcreateReturnObservation observation) {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_post_config_entry_ || !native_runtime_memory_) {
        return {false, "Post-config GEMDOS Fcreate return requires active Millennium Atari runtime"};
    }
    try {
        auto next = *millennium_atari_post_config_entry_;
        const auto result = next.observe_gemdos_fcreate_return(observation, *native_runtime_memory_);
        if (!result.accepted) return {false, result.error};
        auto memory = *native_runtime_memory_;
        const auto batch = next.make_gemdos_fcreate_result_effect_batch(
            "millennium-atari-post-config-fcreate-result-"
                + std::to_string(observation.generation) + "-"
                + std::to_string(observation.sequence));
        const auto applied = memory.apply(batch);
        if (!applied.accepted) {
            return {false, "Post-config Fcreate result store rejected: " + applied.error};
        }
        *millennium_atari_post_config_entry_ = std::move(next);
        *native_runtime_memory_ = std::move(memory);
        return {true, {}};
    } catch (const std::exception& error) {
        return {false, std::string("Post-config GEMDOS Fcreate return rejected: ") + error.what()};
    }
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

MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::execute_millennium_atari_loop_iteration_setup(){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_||!native_runtime_memory_)return{false,"Loop setup requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.execute_loop_iteration_setup();if(!result.accepted)return result;auto memory=*native_runtime_memory_;const auto iteration=next.checkpoint().loop_iteration;auto applied=memory.apply(next.make_loop_iteration_setup_effect_batch("millennium-atari-loop-"+std::to_string(iteration)));if(!applied.accepted)return{false,applied.error};*millennium_atari_config_consumer_=std::move(next);*native_runtime_memory_=std::move(memory);return{true,{}};}

MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::execute_millennium_atari_loop_epilogue(){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_||!native_runtime_memory_)return{false,"Loop epilogue requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.execute_loop_epilogue();if(!result.accepted)return result;auto memory=*native_runtime_memory_;auto applied=memory.apply(next.make_loop_epilogue_effect_batch("millennium-atari-loop-epilogue"));if(!applied.accepted)return{false,applied.error};*millennium_atari_config_consumer_=std::move(next);*native_runtime_memory_=std::move(memory);return{true,{}};}

MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_movem_frame(const MillenniumAtariMovemFrameObservation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_)return{false,"MOVEM frame requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.observe_movem_frame(o);if(result.accepted)*millennium_atari_config_consumer_=std::move(next);return result;}

MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::execute_millennium_atari_jsr_2aa68(){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_)return{false,"JSR $2aa68 requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.execute_jsr_2aa68();if(result.accepted)*millennium_atari_config_consumer_=std::move(next);return result;}

MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_xbios_selector_38(const MillenniumAtariXbiosSelector38Observation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_)return{false,"Selector 38 requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.observe_xbios_selector_38(o);if(result.accepted)*millennium_atari_config_consumer_=std::move(next);return result;}

MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::execute_millennium_atari_jsr_2aa0c(){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_)return{false,"JSR $2aa0c requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.execute_jsr_2aa0c();if(result.accepted)*millennium_atari_config_consumer_=std::move(next);return result;}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_gemdos_selector_61(const MillenniumAtariGemdosSelector61Observation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_||!native_runtime_memory_)return{false,"GEMDOS selector 61 requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.observe_gemdos_selector_61(o);if(!result.accepted)return result;auto memory=*native_runtime_memory_;auto applied=memory.apply(next.make_gemdos_selector_61_effect_batch("millennium-atari-gemdos-61-"+std::to_string(o.sequence)));if(!applied.accepted)return{false,applied.error};*millennium_atari_config_consumer_=std::move(next);*native_runtime_memory_=std::move(memory);return{true,{}};}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::execute_millennium_atari_jsr_2a5c2(){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_)return{false,"JSR $2a5c2 requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.execute_jsr_2a5c2();if(result.accepted)*millennium_atari_config_consumer_=std::move(next);return result;}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_gemdos_selector_63(const MillenniumAtariGemdosSelector63Observation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_||!millennium_atari_||!native_runtime_memory_)return{false,"GEMDOS selector 63 requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;const auto result=next.observe_gemdos_selector_63(o);if(!result.accepted)return result;try{auto memory=*native_runtime_memory_;if(next.checkpoint().gemdos_filename_pointer==0x2a640){constexpr std::array<std::uint8_t,12> filename{'M','I','L','L','2','2','C','.','I','N','F',0};const auto observed=memory.read_linear_range(0x2a640,filename.size());if(!observed||!std::equal(filename.begin(),filename.end(),observed->begin()))return{false,"Pre-decode Fread is not bound to the native MILL22C.INF filename"};const auto batch=millennium_atari_->make_game_init_decoder_fread_effect_batch(o.result_d0,"millennium-atari-decoder-fread-"+std::to_string(o.sequence));if(!batch.effects.empty()){const auto applied=memory.apply(batch);if(!applied.accepted)return{false,applied.error};}}*millennium_atari_config_consumer_=std::move(next);*native_runtime_memory_=std::move(memory);return{true,{}};}catch(const std::exception& error){return{false,error.what()};}}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_gemdos_selector_62(const MillenniumAtariGemdosSelector62Observation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_)return{false,"GEMDOS selector 62 requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.observe_gemdos_selector_62(o);if(result.accepted)*millennium_atari_config_consumer_=std::move(next);return result;}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_fread_prefix(const MillenniumAtariFreadPrefixObservation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_||!native_runtime_memory_)return{false,"Fread prefix requires active Millennium Atari consumer"};const auto read=[&](const std::uint32_t address){return native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address});};const auto b0=read(o.first_address),b1=read(o.first_address+1U),b2=read(o.second_address),b3=read(o.second_address+1U);if(!b0||!b1||!b2||!b3||((std::uint16_t(*b0)<<8U)|*b1)!=o.first_word||((std::uint16_t(*b2)<<8U)|*b3)!=o.second_word)return{false,"Fread prefix contradicts the admitted native file buffer"};auto next=*millennium_atari_config_consumer_;auto result=next.observe_fread_prefix(o);if(!result.accepted)return result;auto memory=*native_runtime_memory_;auto applied=memory.apply(next.make_fread_prefix_effect_batch("millennium-atari-fread-prefix-"+std::to_string(o.sequence)));if(!applied.accepted)return{false,applied.error};*millennium_atari_config_consumer_=std::move(next);*native_runtime_memory_=std::move(memory);return{true,{}};}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::execute_millennium_atari_jsr_2b2be(){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_||!native_runtime_memory_)return{false,"JSR $2b2be requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.execute_jsr_2b2be();if(!result.accepted)return result;auto memory=*native_runtime_memory_;auto applied=memory.apply(next.make_game_init_setup_effect_batch("millennium-atari-game-init-setup"));if(!applied.accepted)return{false,applied.error};*millennium_atari_config_consumer_=std::move(next);*native_runtime_memory_=std::move(memory);return{true,{}};}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_game_init_source_byte(const MillenniumAtariGameInitSourceByteObservation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_||!native_runtime_memory_)return{false,"Game-init source byte requires active Millennium Atari consumer"};const auto source=native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,o.source_address});if(!source||*source!=o.source_byte)return{false,"Game-init source byte contradicts the admitted native file buffer"};auto next=*millennium_atari_config_consumer_;auto result=next.observe_game_init_source_byte(o);if(!result.accepted)return result;auto memory=*native_runtime_memory_;auto applied=memory.apply(next.make_game_init_source_byte_effect_batch("millennium-atari-game-init-source-"+std::to_string(o.sequence)));if(!applied.accepted)return{false,applied.error};*millennium_atari_config_consumer_=std::move(next);*native_runtime_memory_=std::move(memory);return{true,{}};}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_game_init_zero_pair(const MillenniumAtariGameInitZeroPairObservation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_||!native_runtime_memory_)return{false,"Game-init zero pair requires active Millennium Atari consumer"};const auto first=native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,o.first_source_address});const auto second=native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,o.second_source_address});if(!first||!second||*first!=o.first_source_byte||*second!=o.second_source_byte)return{false,"Game-init zero pair contradicts native memory"};auto next=*millennium_atari_config_consumer_;auto result=next.observe_game_init_zero_pair(o);if(!result.accepted)return result;auto memory=*native_runtime_memory_;auto applied=memory.apply(next.make_game_init_zero_pair_effect_batch("millennium-atari-game-init-zero-pair-"+std::to_string(o.sequence)));if(!applied.accepted)return{false,applied.error};*millennium_atari_config_consumer_=std::move(next);*native_runtime_memory_=std::move(memory);return{true,{}};}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::execute_millennium_atari_game_init_zero_counter_branch(){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_)return{false,"Game-init zero counter branch requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.execute_game_init_zero_counter_branch();if(result.accepted)*millennium_atari_config_consumer_=std::move(next);return result;}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_game_init_replicated_byte(const MillenniumAtariGameInitReplicatedByteObservation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_||!native_runtime_memory_)return{false,"Replicated-byte run requires active Millennium Atari consumer"};const auto source=native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,o.source_address});if(!source||*source!=o.source_byte)return{false,"Replicated-byte source contradicts native memory"};auto next=*millennium_atari_config_consumer_;auto result=next.observe_game_init_replicated_byte(o);if(!result.accepted)return result;auto memory=*native_runtime_memory_;auto applied=memory.apply(next.make_game_init_alternate_effect_batch("millennium-atari-game-init-replicated-"+std::to_string(o.sequence)));if(!applied.accepted)return{false,applied.error};*millennium_atari_config_consumer_=std::move(next);*native_runtime_memory_=std::move(memory);return{true,{}};}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_game_init_swapped_pair(const MillenniumAtariGameInitSwappedPairObservation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_||!native_runtime_memory_)return{false,"Swapped-pair run requires active Millennium Atari consumer"};const auto first=native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,o.first_source_address});const auto second=native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,o.second_source_address});if(!first||!second||*first!=o.first_source_byte||*second!=o.second_source_byte)return{false,"Swapped-pair source contradicts native memory"};auto next=*millennium_atari_config_consumer_;auto result=next.observe_game_init_swapped_pair(o);if(!result.accepted)return result;auto memory=*native_runtime_memory_;auto applied=memory.apply(next.make_game_init_alternate_effect_batch("millennium-atari-game-init-swapped-"+std::to_string(o.sequence)));if(!applied.accepted)return{false,applied.error};*millennium_atari_config_consumer_=std::move(next);*native_runtime_memory_=std::move(memory);return{true,{}};}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_game_init_extended_run(const MillenniumAtariGameInitExtendedRunObservation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_||!native_runtime_memory_)return{false,"Extended run requires active Millennium Atari consumer"};const auto count=native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,o.count_source_address});const auto first=native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,o.first_value_address});const auto second=native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,o.second_value_address});if(!count||!first||!second||*count!=o.count_low_byte||*first!=o.first_value_byte||*second!=o.second_value_byte)return{false,"Extended-run source contradicts native memory"};auto next=*millennium_atari_config_consumer_;auto result=next.observe_game_init_extended_run(o);if(!result.accepted)return result;auto memory=*native_runtime_memory_;auto applied=memory.apply(next.make_game_init_alternate_effect_batch("millennium-atari-game-init-extended-"+std::to_string(o.sequence)));if(!applied.accepted)return{false,applied.error};*millennium_atari_config_consumer_=std::move(next);*native_runtime_memory_=std::move(memory);return{true,{}};}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::execute_millennium_atari_game_init_return(){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_)return{false,"Game-init return requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.execute_game_init_return();if(result.accepted)*millennium_atari_config_consumer_=std::move(next);return result;}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::execute_millennium_atari_game_init_palette_copy_prefix(){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_||!native_runtime_memory_)return{false,"Palette-copy prefix requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.execute_game_init_palette_copy_prefix();if(!result.accepted)return result;auto memory=*native_runtime_memory_;auto applied=memory.apply(next.make_game_init_palette_copy_effect_batch("millennium-atari-palette-copy-prefix"));if(!applied.accepted)return{false,applied.error};*millennium_atari_config_consumer_=std::move(next);*native_runtime_memory_=std::move(memory);return{true,{}};}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_game_init_palette_words(const MillenniumAtariGameInitPaletteWordsObservation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_||!native_runtime_memory_)return{false,"Palette arithmetic requires active Millennium Atari consumer"};for(std::size_t i=0;i<o.destination_words.size();++i){const auto address=o.destination_address+static_cast<std::uint32_t>(i*2U);const auto hi=native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address});const auto lo=native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address+1U});if(!hi||!lo||static_cast<std::uint16_t>((*hi<<8U)|*lo)!=o.destination_words[i])return{false,"Palette-word observation contradicts native memory"};}auto next=*millennium_atari_config_consumer_;auto result=next.observe_game_init_palette_words(o);if(!result.accepted)return result;auto memory=*native_runtime_memory_;auto applied=memory.apply(next.make_game_init_palette_arithmetic_effect_batch("millennium-atari-palette-arithmetic-"+std::to_string(o.sequence)));if(!applied.accepted)return{false,applied.error};*millennium_atari_config_consumer_=std::move(next);*native_runtime_memory_=std::move(memory);return{true,{}};}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_game_init_palette_xbios_selector_6(const MillenniumAtariGameInitPaletteXbios6Observation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_)return{false,"Palette XBIOS selector-6 result requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.observe_game_init_palette_xbios_selector_6(o);if(result.accepted)*millennium_atari_config_consumer_=std::move(next);return result;}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_game_init_palette_recurrence(const MillenniumAtariGameInitPaletteRecurrenceObservation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_||!native_runtime_memory_)return{false,"Recurrent palette pass requires active Millennium Atari consumer"};for(std::size_t i=0;i<o.source_bytes.size();++i){const auto value=native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,o.source_address+static_cast<std::uint32_t>(i)});if(!value||*value!=o.source_bytes[i])return{false,"Recurrent palette source contradicts native memory"};}for(std::size_t i=0;i<o.destination_words.size();++i){const auto address=o.destination_address+static_cast<std::uint32_t>(i*2U);const auto hi=native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address});const auto lo=native_runtime_memory_->read_byte({NativeRuntimeAddressSpace::linear,std::nullopt,address+1U});if(!hi||!lo||static_cast<std::uint16_t>((*hi<<8U)|*lo)!=o.destination_words[i])return{false,"Recurrent palette words contradict native memory"};}auto next=*millennium_atari_config_consumer_;auto result=next.observe_game_init_palette_recurrence(o);if(!result.accepted)return result;auto memory=*native_runtime_memory_;auto applied=memory.apply(next.make_game_init_palette_arithmetic_effect_batch("millennium-atari-palette-recurrence-"+std::to_string(o.sequence)));if(!applied.accepted)return{false,applied.error};*millennium_atari_config_consumer_=std::move(next);*native_runtime_memory_=std::move(memory);return{true,{}};}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_game_init_palette_rts(const MillenniumAtariGameInitPaletteRtsObservation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_)return{false,"Palette RTS requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.observe_game_init_palette_rts(o);if(result.accepted)*millennium_atari_config_consumer_=std::move(next);return result;}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_game_init_second_config_fopen(const MillenniumAtariGemdosSelector61Observation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_||!native_runtime_memory_)return{false,"Second config Fopen requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.observe_game_init_second_config_fopen(o);if(!result.accepted)return result;auto memory=*native_runtime_memory_;auto applied=memory.apply(next.make_gemdos_selector_61_effect_batch("millennium-atari-second-config-fopen-"+std::to_string(o.sequence)));if(!applied.accepted)return{false,applied.error};*millennium_atari_config_consumer_=std::move(next);*native_runtime_memory_=std::move(memory);return{true,{}};}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_game_init_second_config_fread(const MillenniumAtariGemdosSelector63Observation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_||!millennium_atari_||!native_runtime_memory_)return{false,"Second config Fread requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.observe_game_init_second_config_fread(o);if(!result.accepted)return result;try{auto memory=*native_runtime_memory_;auto batch=millennium_atari_->make_game_init_second_config_fread_effect_batch(o.result_d0,"millennium-atari-second-config-fread-"+std::to_string(o.sequence));if(!batch.effects.empty()){auto applied=memory.apply(batch);if(!applied.accepted)return{false,applied.error};}*millennium_atari_config_consumer_=std::move(next);*native_runtime_memory_=std::move(memory);return{true,{}};}catch(const std::exception& error){return{false,error.what()};}}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::service_millennium_atari_game_init_second_config_fread(){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_||!millennium_atari_||!native_runtime_memory_)return{false,"Second config Fread service requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;try{auto batch=millennium_atari_->make_game_init_second_config_fread_service_effect_batch("eon-millennium-atari-second-config-fread");auto result=next.service_game_init_second_config_fread_result(static_cast<std::uint32_t>(batch.effects.size()));if(!result.accepted)return result;auto memory=*native_runtime_memory_;if(!batch.effects.empty()){auto applied=memory.apply(batch);if(!applied.accepted)return{false,applied.error};}*millennium_atari_config_consumer_=std::move(next);*native_runtime_memory_=std::move(memory);return{true,{}};}catch(const std::exception& error){return{false,error.what()};}}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::service_millennium_atari_game_init_second_config_fclose(){
    if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap
        ||!millennium_atari_config_consumer_)
        return{false,"Second config Fclose service requires active Millennium Atari consumer"};
    auto next=*millennium_atari_config_consumer_;
    const auto result=next.service_game_init_second_config_fclose_result();
    if(!result.accepted)return result;
    const auto returned=next.execute_game_init_second_config_rts();
    if(!returned.accepted)return returned;
    *millennium_atari_config_consumer_=std::move(next);
    return{true,{}};
}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_game_init_second_config_fclose(const MillenniumAtariGemdosSelector62Observation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_)return{false,"Second config Fclose requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.observe_game_init_second_config_fclose(o);if(result.accepted)*millennium_atari_config_consumer_=std::move(next);return result;}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_game_init_second_config_rts(const MillenniumAtariGameInitSecondConfigRtsObservation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_)return{false,"Second config RTS requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.observe_game_init_second_config_rts(o);if(result.accepted)*millennium_atari_config_consumer_=std::move(next);return result;}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_game_init_second_config_xbios_38(const MillenniumAtariXbiosSelector38Observation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_)return{false,"Second config XBIOS selector-38 requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.observe_game_init_second_config_xbios_38(o);if(result.accepted)*millennium_atari_config_consumer_=std::move(next);return result;}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_game_init_config_final_rts(const MillenniumAtariGameInitSecondConfigRtsObservation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_)return{false,"Final config RTS requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.observe_game_init_config_final_rts(o);if(result.accepted)*millennium_atari_config_consumer_=std::move(next);return result;}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_game_init_post_config_fopen(const MillenniumAtariGemdosSelector61Observation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_)return{false,"Post-config Fopen requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.observe_game_init_post_config_fopen(o);if(result.accepted)*millennium_atari_config_consumer_=std::move(next);return result;}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_game_init_post_config_fread(const MillenniumAtariGemdosSelector63Observation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_||!millennium_atari_||!native_runtime_memory_)return{false,"Post-config Fread requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.observe_game_init_post_config_fread(o);if(!result.accepted)return result;try{auto memory=*native_runtime_memory_;auto batch=millennium_atari_->make_post_config_fread_effect_batch(o.result_d0,"millennium-atari-post-config-fread-"+std::to_string(o.sequence));if(!batch.effects.empty()){auto applied=memory.apply(batch);if(!applied.accepted)return{false,applied.error};}*millennium_atari_config_consumer_=std::move(next);*native_runtime_memory_=std::move(memory);return{true,{}};}catch(const std::exception& error){return{false,error.what()};}}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_game_init_post_config_fclose(const MillenniumAtariGemdosSelector62Observation o){if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap||!millennium_atari_config_consumer_||!native_runtime_memory_)return{false,"Post-config Fclose requires active Millennium Atari consumer"};auto next=*millennium_atari_config_consumer_;auto result=next.observe_game_init_post_config_fclose(o);if(!result.accepted)return result;auto memory=*native_runtime_memory_;auto applied=memory.apply(next.make_game_init_post_config_effect_batch("millennium-atari-post-config-"+std::to_string(o.sequence)));if(!applied.accepted)return{false,applied.error};*millennium_atari_config_consumer_=std::move(next);*native_runtime_memory_=std::move(memory);return{true,{}};}
MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::observe_millennium_atari_game_init_post_config_rts(
    const MillenniumAtariGameInitSecondConfigRtsObservation observation) {
    if (!session_snapshot_
        || session_snapshot_->kind != RuntimeSessionKind::millennium_atari_bootstrap
        || !millennium_atari_config_consumer_) {
        return {false, "Post-config RTS requires active Millennium Atari consumer"};
    }

    auto next_consumer = *millennium_atari_config_consumer_;
    const auto result = next_consumer.observe_game_init_post_config_rts(observation);
    if (!result.accepted) return result;

    // The caller enters MILL22B.INF only when the observed loader RTS returns
    // to its Fread buffer and the loaded entry bytes validate. Keep both
    // session copies private until that cross-session transition succeeds.
    // Other typed return destinations do not assert that the module executed.
    if (observation.return_address == 0x11e00U) {
        if (!millennium_atari_post_config_entry_ || !native_runtime_memory_) {
            return {false, "MILL22B.INF entry state is unavailable at loader return"};
        }
        auto next_entry = *millennium_atari_post_config_entry_;
        const auto entry_result = next_entry.execute_entry_jump(
            *native_runtime_memory_, observation.sequence, observation.return_address);
        if (!entry_result.accepted) return {false, entry_result.error};
        *millennium_atari_post_config_entry_ = std::move(next_entry);
    }

    *millennium_atari_config_consumer_ = std::move(next_consumer);
    return {true, {}};
}

MillenniumAtariConfigConsumerResult ReleaseRuntimeCoordinator::service_millennium_atari_game_init_second_config_fopen(){
    if(!session_snapshot_||session_snapshot_->kind!=RuntimeSessionKind::millennium_atari_bootstrap
        ||!millennium_atari_config_consumer_||!millennium_atari_||!native_runtime_memory_)
        return{false,"Second config Fopen service requires active Millennium Atari consumer"};
    try{
        constexpr std::array<std::uint8_t,12> filename{
            'M','I','L','L','2','2','D','.','i','n','f',0};
        const auto observed_name=native_runtime_memory_->read_linear_range(0x2a634,filename.size());
        if(!observed_name||!std::equal(filename.begin(),filename.end(),observed_name->begin()))
            return{false,"Game-init Fopen filename differs from owned MILL22D.INF bytes"};
        const auto source_check=millennium_atari_->make_game_init_second_config_fread_service_effect_batch(
            "validate-eon-millennium-atari-second-config-source");
        if(source_check.effects.size()!=18428U)
            return{false,"Game-init Fopen source does not match the authenticated MILL22D.INF length"};
        auto next=*millennium_atari_config_consumer_;
        constexpr std::uint16_t virtual_handle=0x7ffeU;
        const auto serviced=next.service_game_init_second_config_fopen_result(virtual_handle);
        if(!serviced.accepted)return serviced;
        auto memory=*native_runtime_memory_;
        const auto applied=memory.apply(next.make_gemdos_selector_61_effect_batch(
            "eon-millennium-atari-second-config-fopen"));
        if(!applied.accepted)return{false,applied.error};
        *millennium_atari_config_consumer_=std::move(next);
        *native_runtime_memory_=std::move(memory);
        return{true,{}};
    }catch(const std::exception& error){return{false,error.what()};}
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
    constexpr auto ibm_speaker_sha256 =
        "f3224caa43c1149907f852fa98816ed68c489b70f1ba795592d684d4e51f31b1";
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
                .admitted_launcher_text = {},
                .voice_bank = std::nullopt,
                .sibm_native_voice_bank = std::nullopt,
                .title_flow = std::nullopt,
                .sound_selection = std::nullopt,
                .sound_selection_prompt = std::nullopt,
                .ibm_speaker_driver = std::nullopt,
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
        const auto ibm_speaker = media.borrow(ibm_speaker_sha256);
        if (!gx_bytes || !titles_code.accepted() || !launcher_code.accepted()
            || !game_code.accepted() || !initial_save || !static_data || !ega640 || !mcga
            || !sound_blaster || !covox || !ibm_speaker) {
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
            .admitted_launcher_text = admit_all_game_text_from_source(
                Game::millennium, Platform::dos, "MILL.COM", launcher_code.view->bytes),
            .voice_bank = parse_millennium_dos_voice_bank(media),
            .sibm_native_voice_bank = load_millennium_dos_sibm_native_voice_bank(
                game_code.view->bytes, *ibm_speaker, media),
            .title_flow = title_flow,
            .sound_selection = sound_selection,
            .sound_selection_prompt = sound_selection_prompt,
            .ibm_speaker_driver = admit_millennium_dos_sound_driver_leaf(*ibm_speaker),
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
