#include "launcher.hpp"
#include "engine/deuteros_amiga_opening.hpp"
#include "engine/deuteros_amiga_paula.hpp"
#include "engine/millennium_dos_title_session.hpp"
#include "engine/millennium_dos_save_session.hpp"
#include "data/amiga_adf.hpp"
#include "data/atari_st_prg.hpp"
#include "data/deuteros_amiga_bundle.hpp"
#include "data/deuteros_amiga_audio.hpp"
#include "data/deuteros_amiga_channel_vm.hpp"
#include "data/deuteros_amiga_frame.hpp"
#include "data/deuteros_amiga_loader.hpp"
#include "data/deuteros_amiga_title_stage.hpp"
#include "data/deuteros_atari_boot.hpp"
#include "data/fat12.hpp"
#include "data/millennium_dos_bitmap.hpp"
#include "data/millennium_dos_game_data.hpp"
#include "data/millennium_dos_gameplay_screen.hpp"
#include "data/millennium_amiga_loader.hpp"
#include "data/millennium_dos_lib.hpp"
#include "data/millennium_dos_title_flow.hpp"
#include "data/zip_archive.hpp"
#include "platform/game_data.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

enum class Screen { menu, launching };

struct Card {
    eon::Game game;
    const char* title;
    const char* subtitle;
    const char* filename;
    SDL_FRect bounds;
    SDL_Texture* texture = nullptr;
};

struct PreviewAnimation {
    int width = 0;
    int height = 0;
    std::vector<std::vector<std::uint8_t>> rgba_frames;
};

// These three data products are deliberately kept together: the title image,
// title executable hand-off, and GX canvas all come from the same verified
// English DOS archive.  The canvas becomes visible only after the executable's
// recovered console-poll hand-off; this is a presentation boundary, not a
// claim that IMG01 names or implements the full game UI.
struct MillenniumDosLaunchAssets {
    PreviewAnimation title;
    PreviewAnimation gx_canvas;
    eon::MillenniumDosTitleFlow title_flow;
    // This is intentionally the original serialized image, not a projected
    // game model.  The launcher exposes only the recovered positional words
    // once TITLES.EXE has made its verified hand-off.
    eon::MillenniumDosSaveSession initial_save;
};

void draw_text(SDL_Renderer* renderer, float x, float y, const std::string& text) {
    SDL_RenderDebugText(renderer, x, y, text.c_str());
}

bool inside(const SDL_FRect& rectangle, float x, float y) {
    return x >= rectangle.x && x <= rectangle.x + rectangle.w
        && y >= rectangle.y && y <= rectangle.y + rectangle.h;
}

SDL_Texture* load_card(SDL_Renderer* renderer, const char* filename) {
    const std::array<std::filesystem::path, 3> candidates{{
        std::filesystem::path(SDL_GetBasePath()) / "assets" / "cards" / filename,
        std::filesystem::path(EON_ASSET_DIR) / "cards" / filename,
        std::filesystem::path("assets") / "cards" / filename,
    }};
    for (const auto& path : candidates) {
        if (SDL_Texture* texture = IMG_LoadTexture(renderer, path.string().c_str())) return texture;
    }
    std::cerr << "Unable to load card " << filename << ": " << SDL_GetError() << '\n';
    return nullptr;
}

void report_deuteros_amiga(const eon::ReleaseArchive& release) {
    constexpr auto clean_system_adf =
        "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38";
    const auto image = eon::extract_asset_by_sha256(release.path, clean_system_adf);
    if (!image) return;
    const eon::AmigaAdf disk(*image);
    const auto plan = eon::parse_deuteros_amiga_load_plan(disk);
    std::cout << "          ADF boot checksum valid; main stage disk 0x" << std::hex
        << plan.main_stage.disk_offset << " -> memory 0x" << plan.main_stage.destination
        << ", entry 0x" << plan.main_stage.entry_address << std::dec << '\n';
    for (std::size_t index = 0; index < 2; ++index) {
        const auto bundle = eon::parse_deuteros_amiga_bundle(
            disk, plan.resource_disk_offsets[index]);
        std::cout << "          Resource bundle " << index << ": disk 0x" << std::hex
            << bundle.disk_offset << ", 0x" << bundle.length << std::dec
            << " bytes, " << bundle.object_count << " objects, mode "
            << bundle.mode_flag << '\n';
    }
    const auto opening_bundle = eon::parse_deuteros_amiga_bundle(
        disk, plan.resource_disk_offsets[0]);
    const auto sound_bank = eon::parse_deuteros_amiga_sound_bank(disk, opening_bundle);
    std::cout << "          Opening Paula table: " << sound_bank.sounds.size()
        << " original DMA records\n";
    const auto& handoff = plan.title_handoff_profile;
    const auto title_stage = eon::parse_deuteros_amiga_title_stage(disk, plan);
    std::cout << "          Title input profile: disk 0x" << std::hex << handoff.disk_offset
        << ", length 0x" << handoff.length << ", memory 0x" << handoff.destination
        << ", entry 0x" << plan.title_stage.entry_address << std::dec << '\n';
    std::cout << "          Title stage: mode " << title_stage.special_mode << " -> byte 0x"
        << std::hex << title_stage.special_mode_byte_address << ", config 0x"
        << title_stage.special_mode_configuration_value << std::dec << "; loop 0x"
        << std::hex << title_stage.main_loop_address << std::dec << '\n';
    std::cout << "          Timed title transition: 0x" << std::hex
        << title_stage.transition_source_palette_address << " -> 0x"
        << title_stage.transition_work_palette_address << ", " << std::dec
        << title_stage.transition_palette_word_count << " RGB4 words, mask 0x"
        << std::hex << title_stage.transition_palette_mask << std::dec << '\n';
}

void report_millennium_dos(const eon::ReleaseArchive& release) {
    constexpr auto title_lib_sha256 =
        "6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678";
    const auto title_bytes = eon::extract_asset_by_sha256(release.path, title_lib_sha256);
    if (!title_bytes) return;
    const eon::MillenniumDosLib title_lib(*title_bytes);
    const auto* p00 = title_lib.find("P00");
    if (!p00) throw std::runtime_error("Verified Millennium TITLE.LIB has no P00 entry");
    const auto resource = title_lib.read(*p00);
    const auto bitmap = eon::decode_millennium_dos_bitmap(resource);
    const auto palette = eon::decode_millennium_dos_palette(resource, bitmap);
    std::cout << "          TITLE.LIB P00: " << bitmap.width << 'x' << bitmap.height
        << ", codec " << static_cast<unsigned>(bitmap.codec)
        << ", indices 0.." << static_cast<unsigned>(bitmap.max_palette_index)
        << ", RGB6 DAC entries 256, logical translation "
        << palette.logical_to_dac.size() << "\n";
    constexpr auto gx_lib_sha256 =
        "4adf9991226deab4749ac07ad637851994f57d11f6dc45f3f5ce862b5bc34c2f";
    const auto gx_bytes = eon::extract_asset_by_sha256(release.path, gx_lib_sha256);
    if (!gx_bytes) throw std::runtime_error("Verified Millennium GX.LIB missing");
    const auto gx_canvas = eon::parse_millennium_dos_gameplay_screen(*gx_bytes);
    std::cout << "          GX.LIB IMG00 -> IMG01: " << gx_canvas.canvas.width << 'x'
        << gx_canvas.canvas.height << " original indexed canvas\n";
    constexpr auto titles_sha256 =
        "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
    constexpr auto launcher_sha256 =
        "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e";
    const auto titles = eon::extract_asset_by_sha256(release.path, titles_sha256);
    const auto launcher = eon::extract_asset_by_sha256(release.path, launcher_sha256);
    if (!titles || !launcher) throw std::runtime_error("Verified Millennium title flow assets missing");
    const auto flow = eon::parse_millennium_dos_title_flow(*titles, *launcher);
    std::cout << "          TITLES.EXE: resource " << flow.title_resource_index
        << ", " << flow.intro_transition_steps << " transition steps, key poll INT 0x"
        << std::hex << static_cast<unsigned>(flow.input_interrupt) << std::dec
        << "; launcher hand-off " << flow.launcher_title_program << " -> "
        << flow.launcher_game_program << '\n';
    constexpr auto static_data_sha256 =
        "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d";
    const auto static_data = eon::extract_asset_by_sha256(release.path, static_data_sha256);
    if (!static_data) throw std::runtime_error("Verified Millennium static game data missing");
    const auto game_data = eon::parse_millennium_dos_game_data(*static_data);
    std::cout << "          2200AD4.BIN: " << game_data.celestial_labels.size()
        << " original celestial labels (" << game_data.celestial_labels[4].text << ")\n";
    constexpr auto initial_save_sha256 =
        "a9b3d77534d3d575012f9553bfed9520edf92a83af408c977e7f0fd226a470e7";
    const auto initial_save = eon::extract_asset_by_sha256(release.path, initial_save_sha256);
    if (!initial_save) throw std::runtime_error("Verified Millennium initial save missing");
    const eon::MillenniumDosSaveSession save_session(*initial_save);
    std::cout << "          2200SAVE.I: SHA-256 " << save_session.sha256()
        << ", version 0x" << std::hex << save_session.layout().version << std::dec
        << ", " << save_session.layout().state_table.size()
        << " recovered state-table records\n";
    // These are the literal positional words restored by the verified load
    // path.  Their gameplay meanings remain deliberately unnamed.
    for (std::size_t index = 0; index < save_session.layout().state_table.size(); ++index) {
        const auto& record = save_session.state_record(index);
        std::cout << "            [" << index << "] +00=0x" << std::hex
            << record.runtime_offset_0 << " +04=0x" << record.runtime_offset_4
            << " +06=0x" << record.runtime_offset_6 << " +08=0x"
            << record.runtime_offset_8 << std::dec << '\n';
    }
}

void report_millennium_amiga(const eon::ReleaseArchive& release) {
    // Defjam's image contains the recovered raw-sector loader. Other supplied
    // crack variants preserve the shared media ranges but patch this stage.
    constexpr auto loader_adf_sha256 =
        "8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c";
    const auto image = eon::extract_asset_by_sha256(release.path, loader_adf_sha256);
    if (!image) return;
    const eon::AmigaAdf disk(*image);
    const auto plan = eon::parse_millennium_amiga_load_plan(disk);
    const auto resident = eon::parse_millennium_amiga_resident_entry(disk, plan);
    std::cout << "          raw loader: disk 0x" << std::hex
        << plan.first_stage.disk_offset << " + 0x" << plan.first_stage.length
        << " -> memory 0x" << plan.first_stage.destination
        << "; disk 0x" << plan.resident_stage.disk_offset << " + 0x"
        << plan.resident_stage.length << " -> entry 0x" << plan.resident_entry
        << ", marker 0x" << plan.loader_magic << std::dec << '\n'
        << "          resident gate: entry 0x" << std::hex << resident.entry_address
        << " calls 0x" << resident.initializer_address << "; d3 != 0 ORs 0x"
        << resident.d3_nonzero_or_mask << " into d0, stores word at 0x"
        << resident.result_word_address << std::dec << '\n';
}

void report_millennium_atari_st(const eon::ReleaseArchive& release) {
    constexpr auto equinox_disk_sha256 =
        "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7";
    const auto image = eon::extract_asset_by_sha256(release.path, equinox_disk_sha256);
    if (!image) return;
    const eon::Fat12Disk disk(*image);
    const auto* executable = disk.find("MILENIUM.TOS");
    if (!executable) throw std::runtime_error("Verified Millennium Atari ST disk has no MILENIUM.TOS");
    const auto prg = eon::parse_atari_st_prg(disk.read(*executable));
    std::cout << "          MILENIUM.TOS: text " << prg.text_bytes << ", data "
        << prg.data_bytes << ", BSS " << prg.bss_bytes << ", "
        << prg.relocation_count << " relocations (0x" << std::hex
        << prg.first_relocation_offset << "..0x" << prg.last_relocation_offset
        << std::dec << ")\n";
}

void report_deuteros_atari_st(const eon::ReleaseArchive& release) {
    // The corpus has no pristine ST master: this hash identifies the supplied
    // Replicants Disk 1 whose boot code contains the recovered XBIOS stage.
    constexpr auto replicants_disk1_sha256 =
        "aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee";
    constexpr auto disk2_sha256 =
        "5501ce3fd79c9b37cf695692a8012267db23dacd8a2cc64c0c7b7e4305971193";
    const auto disk1_image = eon::extract_asset_by_sha256(release.path, replicants_disk1_sha256);
    const auto disk2_image = eon::extract_asset_by_sha256(release.path, disk2_sha256);
    if (!disk1_image || !disk2_image) return;
    const eon::DeuterosAtariDisk disk1(*disk1_image);
    const eon::DeuterosAtariDisk disk2(*disk2_image);
    const auto& stage = disk1.boot_profile();
    const auto& continuation = disk2.boot_profile();
    std::cout << "          protected ST media: " << stage.total_sectors << " sectors, "
        << stage.sectors_per_track << " sectors/track, boot checksum 0x" << std::hex
        << stage.boot_checksum << std::dec << "; FAT root intentionally unavailable\n";
    if (stage.has_recovered_first_stage) {
        const auto first_stage = disk1.read_sectors(stage.first_stage_track, stage.first_stage_side,
            stage.first_stage_sector, stage.first_stage_sector_count);
        const auto profile = eon::parse_deuteros_atari_first_stage(first_stage);
        const auto second_stage = disk1.read_sectors(profile.next_track, profile.next_side,
            profile.next_sector, profile.next_sector_count);
        const auto second_profile = eon::parse_deuteros_atari_second_stage(second_stage);
        std::cout << "          Disk 1 XBIOS first stage: track " << stage.first_stage_track
            << ", side " << static_cast<unsigned>(stage.first_stage_side) << ", sectors "
            << static_cast<unsigned>(stage.first_stage_sector) << ".."
            << stage.first_stage_sector_count << " (" << first_stage.size()
            << " original bytes)\n";
        std::cout << "          First-stage control flow: entry +0x" << std::hex
            << profile.entry_offset << ", checksum +0x" << profile.checksum_start_offset
            << " +0x" << profile.checksum_byte_count << " (seed 0x"
            << profile.checksum_seed << ", expected 0x" << profile.checksum_expected
            << "); next raw load track " << std::dec << profile.next_track << ", side "
            << static_cast<unsigned>(profile.next_side) << ", sectors "
            << static_cast<unsigned>(profile.next_sector) << ".." << profile.next_sector_count
            << " -> RAM 0x" << std::hex << profile.next_destination << std::dec << '\n';
        std::cout << "          Track-2 loader: supervisor stack 0x" << std::hex
            << second_profile.supervisor_stack << ", application stack 0x"
            << second_profile.application_stack << ", direct entry 0x"
            << second_profile.direct_entry << std::dec << "; raw reader +0x"
            << std::hex << second_profile.raw_read_routine_offset << std::dec
            << " caps at " << second_profile.raw_read_max_sector_count << " sectors\n";
    }
    std::cout << "          Disk 2 boot continuation: "
        << (continuation.killer_boot_signature ? "KILLER_BOOT signature" : "unclassified")
        << ", branch 0x" << std::hex << continuation.boot_branch_target << std::dec << '\n';
}

std::optional<MillenniumDosLaunchAssets> load_millennium_launch_assets(
    const std::vector<eon::ReleaseArchive>& releases) {
    constexpr auto title_lib_sha256 =
        "6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678";
    constexpr auto gx_lib_sha256 =
        "4adf9991226deab4749ac07ad637851994f57d11f6dc45f3f5ce862b5bc34c2f";
    constexpr auto titles_sha256 =
        "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
    constexpr auto launcher_sha256 =
        "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e";
    constexpr auto initial_save_sha256 =
        "a9b3d77534d3d575012f9553bfed9520edf92a83af408c977e7f0fd226a470e7";
    const auto release = std::find_if(releases.begin(), releases.end(), [](const auto& candidate) {
        return candidate.game == eon::Game::millennium && candidate.platform == eon::Platform::dos
            && candidate.language == "en";
    });
    if (release == releases.end()) return std::nullopt;
    try {
        const auto bytes = eon::extract_asset_by_sha256(release->path, title_lib_sha256);
        if (!bytes) return std::nullopt;
        const eon::MillenniumDosLib title_lib(*bytes);
        const auto* p00 = title_lib.find("P00");
        if (!p00) return std::nullopt;
        const auto resource = title_lib.read(*p00);
        const auto bitmap = eon::decode_millennium_dos_bitmap(resource);
        const auto palette = eon::decode_millennium_dos_palette(resource, bitmap);
        const auto gx_bytes = eon::extract_asset_by_sha256(release->path, gx_lib_sha256);
        const auto titles = eon::extract_asset_by_sha256(release->path, titles_sha256);
        const auto launcher = eon::extract_asset_by_sha256(release->path, launcher_sha256);
        const auto initial_save = eon::extract_asset_by_sha256(release->path, initial_save_sha256);
        if (!gx_bytes || !titles || !launcher || !initial_save) return std::nullopt;
        const auto gx_canvas = eon::parse_millennium_dos_gameplay_screen(*gx_bytes);
        return MillenniumDosLaunchAssets{
            .title = {bitmap.width, bitmap.height,
                {eon::colorize_millennium_dos_bitmap(bitmap, palette)}},
            .gx_canvas = {gx_canvas.canvas.width, gx_canvas.canvas.height, {gx_canvas.rgba}},
            .title_flow = eon::parse_millennium_dos_title_flow(*titles, *launcher),
            .initial_save = eon::MillenniumDosSaveSession(*initial_save),
        };
    } catch (const std::exception& error) {
        std::cerr << "Unable to load Millennium DOS launch assets: " << error.what() << '\n';
        return std::nullopt;
    }
}

std::unique_ptr<eon::DeuterosAmigaOpening> load_deuteros_opening(
    const std::vector<eon::ReleaseArchive>& releases) {
    constexpr auto clean_system_adf =
        "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38";
    const auto release = std::find_if(releases.begin(), releases.end(), [](const auto& candidate) {
        return candidate.game == eon::Game::deuteros && candidate.platform == eon::Platform::amiga;
    });
    if (release == releases.end()) return {};
    try {
        const auto image = eon::extract_asset_by_sha256(release->path, clean_system_adf);
        if (!image) return {};
        return std::make_unique<eon::DeuterosAmigaOpening>(std::move(*image));
    } catch (const std::exception& error) {
        std::cerr << "Unable to start Deuteros opening: " << error.what() << '\n';
        return {};
    }
}

} // namespace

int main(int argc, char** argv) {
    const auto parsed = eon::parse_command_line(argc, argv);
    if (parsed.help) {
        std::cout << eon::usage();
        return 0;
    }
    if (!parsed.request) {
        std::cerr << parsed.error << "\n\n" << eon::usage();
        return 2;
    }
    auto request = *parsed.request;
    if (!std::filesystem::is_directory(request.data_directory)
        && !std::filesystem::is_regular_file(request.data_directory)) {
        if (request.data_directory_is_default) {
            std::error_code error;
            std::filesystem::create_directories(request.data_directory, error);
        }
    }
    if (!std::filesystem::is_directory(request.data_directory)
        && !std::filesystem::is_regular_file(request.data_directory)) {
        std::cerr << "Data path does not exist: " << request.data_directory << '\n';
        return 2;
    }
    eon::ReleaseScanner scanner(request.data_directory);
    std::vector<eon::ReleaseArchive> releases;
    // Direct launches and command-line verification intentionally wait for a
    // complete answer. The graphical menu instead advances this scanner after
    // its first frame, mirroring OpenCaptive's non-blocking data scanner.
    if (request.verify_game || request.game) {
        while (!scanner.advance(64)) {
        }
        releases = scanner.releases();
        if (releases.empty()) {
            std::cerr << "No recognised original release archives found.\n";
            return 3;
        }
    }
    if (request.verify_game) {
        bool found = false;
        for (const auto& release : releases) {
            if (release.game != *request.verify_game) continue;
            found = true;
            std::cout << "VERIFIED  " << eon::name(release.game) << " / "
                << eon::name(release.platform) << " / " << release.language << '\n'
                << "          " << release.sha256 << '\n'
                << "          " << release.path << '\n';
            if (release.game == eon::Game::deuteros
                && release.platform == eon::Platform::amiga) {
                report_deuteros_amiga(release);
            }
            if (release.game == eon::Game::deuteros
                && release.platform == eon::Platform::atari_st) {
                report_deuteros_atari_st(release);
            }
            if (release.game == eon::Game::millennium
                && release.platform == eon::Platform::amiga
                && release.language == "en") {
                report_millennium_amiga(release);
            }
            if (release.game == eon::Game::millennium
                && release.platform == eon::Platform::dos
                && release.language == "en") {
                report_millennium_dos(release);
            }
            if (release.game == eon::Game::millennium
                && release.platform == eon::Platform::atari_st
                && release.language == "en") {
                report_millennium_atari_st(release);
            }
        }
        return found ? 0 : 5;
    }
    if (request.game && !eon::release_available(releases, *request.game, request.platform)) {
        std::cerr << "Requested original release is not present.\n";
        return 4;
    }
    const auto millennium_assets = load_millennium_launch_assets(releases);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Project Eon", 1280, 720, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        std::cerr << "SDL_CreateWindowAndRenderer failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderLogicalPresentation(renderer, 1280, 720, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(renderer, 1);

    std::array<Card, 2> cards{{
        {eon::Game::millennium, "MILLENNIUM 2.2", "RETURN TO EARTH", "millennium.png", {64, 170, 552, 310}},
        {eon::Game::deuteros, "DEUTEROS", "THE NEXT MILLENNIUM", "deuteros.png", {664, 170, 552, 310}},
    }};
    for (auto& card : cards) card.texture = load_card(renderer, card.filename);
    std::unique_ptr<eon::DeuterosAmigaOpening> deuteros_opening;
    std::unique_ptr<eon::DeuterosAmigaPaulaMixer> deuteros_paula;
    SDL_AudioStream* deuteros_audio_stream = nullptr;
    SDL_Texture* preview_texture = nullptr;
    const auto create_deuteros_opening_texture = [&] {
        if (!deuteros_opening || preview_texture) return;
        preview_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STREAMING, eon::DeuterosAmigaFrame::width,
            eon::DeuterosAmigaFrame::height);
    };
    const auto start_deuteros_audio = [&] {
        if (!deuteros_opening) return;
        deuteros_paula = std::make_unique<eon::DeuterosAmigaPaulaMixer>(
            deuteros_opening->sound_bank());
        if (deuteros_audio_stream) {
            static_cast<void>(SDL_ClearAudioStream(deuteros_audio_stream));
            return;
        }
        const SDL_AudioSpec format{SDL_AUDIO_F32, 2, 48'000};
        deuteros_audio_stream = SDL_OpenAudioDeviceStream(
            SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &format, nullptr, nullptr);
        if (!deuteros_audio_stream) {
            std::cerr << "Unable to open Deuteros audio output: " << SDL_GetError() << '\n';
            return;
        }
        if (!SDL_ResumeAudioStreamDevice(deuteros_audio_stream)) {
            std::cerr << "Unable to start Deuteros audio output: " << SDL_GetError() << '\n';
            SDL_DestroyAudioStream(deuteros_audio_stream);
            deuteros_audio_stream = nullptr;
        }
    };
    SDL_Texture* millennium_preview_texture = nullptr;
    SDL_Texture* millennium_gx_canvas_texture = nullptr;
    if (millennium_assets) {
        millennium_preview_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STATIC, millennium_assets->title.width, millennium_assets->title.height);
        if (millennium_preview_texture) {
            SDL_UpdateTexture(millennium_preview_texture, nullptr,
                millennium_assets->title.rgba_frames.front().data(), millennium_assets->title.width * 4);
        }
        millennium_gx_canvas_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STATIC, millennium_assets->gx_canvas.width,
            millennium_assets->gx_canvas.height);
        if (millennium_gx_canvas_texture) {
            SDL_UpdateTexture(millennium_gx_canvas_texture, nullptr,
                millennium_assets->gx_canvas.rgba_frames.front().data(),
                millennium_assets->gx_canvas.width * 4);
        }
    }

    Screen screen = request.game ? Screen::launching : Screen::menu;
    eon::Game selected = request.game.value_or(eon::Game::millennium);
    int focused = 0;
    std::uint64_t deuteros_last_tick = SDL_GetTicks();
    bool deuteros_input_pressed = false;
    std::optional<std::uint32_t> deuteros_title_resource;
    std::unique_ptr<eon::MillenniumDosTitleSession> millennium_title_session;
    std::size_t millennium_state_page = 0;
    const auto start_millennium_title = [&] {
        if (millennium_assets) {
            millennium_title_session = std::make_unique<eon::MillenniumDosTitleSession>(
                millennium_assets->title_flow);
            millennium_state_page = 0;
        }
    };
    if (screen == Screen::launching && selected == eon::Game::millennium) {
        start_millennium_title();
    }
    bool show_scanner = false;
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
                if (screen == Screen::launching && !request.game) screen = Screen::menu;
                else running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F1 && !event.key.repeat) {
                request.presentation = request.presentation == eon::Presentation::original
                    ? eon::Presentation::modern : eon::Presentation::original;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat
                && screen == Screen::launching && selected == eon::Game::millennium
                && event.key.key != SDLK_ESCAPE && event.key.key != SDLK_F1
                && millennium_title_session) {
                if (millennium_title_session->handed_off()
                    && (event.key.key == SDLK_LEFT || event.key.key == SDLK_RIGHT)) {
                    constexpr std::size_t records_per_page = 8;
                    constexpr std::size_t page_count =
                        (eon::MillenniumDosSaveLayout::state_table_count + records_per_page - 1)
                        / records_per_page;
                    if (event.key.key == SDLK_LEFT && millennium_state_page > 0) {
                        --millennium_state_page;
                    }
                    if (event.key.key == SDLK_RIGHT && millennium_state_page + 1 < page_count) {
                        ++millennium_state_page;
                    }
                    continue;
                }
                // TITLES.EXE uses DOS' non-blocking character availability
                // poll, rather than a game-specific action key.  SDL's key
                // event supplies that availability signal; the recovered
                // session alone decides the one-way launcher hand-off.
                static_cast<void>(millennium_title_session->poll_console(true));
            }
            if ((event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP)
                && screen == Screen::launching && selected == eon::Game::deuteros
                && (event.key.key == SDLK_SPACE || event.key.key == SDLK_RETURN)) {
                // $14 consumes the input word last polled by the original loop.
                // Feed the physical held state, leaving acceptance to the VM's
                // recovered timing and input-gate logic.
                if (event.type == SDL_EVENT_KEY_DOWN) deuteros_input_pressed = true;
                if (event.type == SDL_EVENT_KEY_UP) deuteros_input_pressed = false;
            }
            if (screen == Screen::menu && event.type == SDL_EVENT_KEY_DOWN
                && event.key.key == SDLK_D && !event.key.repeat) {
                show_scanner = !show_scanner;
            }
            if (screen == Screen::menu && event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_LEFT || event.key.key == SDLK_RIGHT) focused = 1 - focused;
                if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                    const auto game = cards[static_cast<std::size_t>(focused)].game;
                    if (eon::release_available(releases, game, std::nullopt)) {
                        selected = game;
                        screen = Screen::launching;
                        if (selected == eon::Game::millennium) start_millennium_title();
                        if (selected == eon::Game::deuteros) {
                            deuteros_opening = load_deuteros_opening(releases);
                            create_deuteros_opening_texture();
                            start_deuteros_audio();
                            deuteros_last_tick = SDL_GetTicks();
                            deuteros_title_resource.reset();
                        }
                    }
                }
            }
            if (screen == Screen::menu && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                float x = 0, y = 0;
                SDL_RenderCoordinatesFromWindow(renderer, event.button.x, event.button.y, &x, &y);
                for (std::size_t index = 0; index < cards.size(); ++index) {
                    if (inside(cards[index].bounds, x, y)) {
                        focused = static_cast<int>(index);
                        if (eon::release_available(releases, cards[index].game, std::nullopt)) {
                            selected = cards[index].game;
                            screen = Screen::launching;
                            if (selected == eon::Game::millennium) start_millennium_title();
                            if (selected == eon::Game::deuteros) {
                                deuteros_opening = load_deuteros_opening(releases);
                                create_deuteros_opening_texture();
                                start_deuteros_audio();
                                deuteros_last_tick = SDL_GetTicks();
                                deuteros_title_resource.reset();
                            }
                        }
                    }
                }
            }
        }

        if (!scanner.done()) {
            static_cast<void>(scanner.advance(show_scanner ? 32 : 1));
            releases = scanner.releases();
        }
        if (screen == Screen::launching && selected == eon::Game::deuteros
            && !deuteros_opening) {
            deuteros_opening = load_deuteros_opening(releases);
            create_deuteros_opening_texture();
            start_deuteros_audio();
            deuteros_last_tick = SDL_GetTicks();
        }
        if (screen == Screen::launching && selected == eon::Game::deuteros
            && deuteros_opening) {
            constexpr std::uint64_t scheduler_period_ms = 20;
            constexpr std::size_t maximum_catch_up_ticks = 4;
            const auto now = SDL_GetTicks();
            std::size_t tick_count = 0;
            while (now - deuteros_last_tick >= scheduler_period_ms
                && tick_count < maximum_catch_up_ticks) {
                const auto events = deuteros_opening->tick(deuteros_input_pressed);
                if (deuteros_paula) {
                    for (const auto& sound : events.sounds) {
                        if (!deuteros_paula->submit(sound) && sound.sound != 0) {
                            std::cerr << "Deuteros event uses unsupported Paula descriptor "
                                << sound.sound << " / mask 0x" << std::hex << sound.channels
                                << std::dec << '\n';
                        }
                    }
                }
                if (!events.alternate_resources.empty()) {
                    // Opcode $0f exposes this original bundle-relative target.
                    // It is retained as evidence for the subsequent verified
                    // stage, not given an invented title/menu interpretation.
                    deuteros_title_resource = events.alternate_resources.front();
                }
                deuteros_last_tick += scheduler_period_ms;
                ++tick_count;
            }
            if (tick_count == maximum_catch_up_ticks
                && now - deuteros_last_tick >= scheduler_period_ms) {
                deuteros_last_tick = now;
            }
            // VM events are proven at 50 Hz, but the exact relation between
            // that scheduler and host-device latency is not yet recovered.
            // Keep just one VBL of original PCM queued; do not manufacture a
            // silent/fallback waveform to fill the device.
            if (deuteros_audio_stream && deuteros_paula && deuteros_paula->has_active_channels()) {
                constexpr int queued_target_bytes = 960 * 2 * static_cast<int>(sizeof(float));
                constexpr int bytes_per_frame = 2 * static_cast<int>(sizeof(float));
                int queued = SDL_GetAudioStreamQueued(deuteros_audio_stream);
                while (queued >= 0 && queued < queued_target_bytes
                    && deuteros_paula->has_active_channels()) {
                    const auto missing_frames = static_cast<std::size_t>(
                        (queued_target_bytes - queued + bytes_per_frame - 1) / bytes_per_frame);
                    const auto samples = deuteros_paula->render(missing_frames);
                    if (!SDL_PutAudioStreamData(deuteros_audio_stream, samples.data(),
                        static_cast<int>(samples.size() * sizeof(float)))) {
                        std::cerr << "Unable to queue Deuteros audio: " << SDL_GetError() << '\n';
                        break;
                    }
                    queued = SDL_GetAudioStreamQueued(deuteros_audio_stream);
                }
            }
        }

        const bool modern = request.presentation == eon::Presentation::modern;
        SDL_SetRenderDrawColor(renderer, modern ? 5 : 0, modern ? 15 : 0, modern ? 27 : 0, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 205, 225, 235, 255);

        if (screen == Screen::menu) {
            draw_text(renderer, 64, 56, "PROJECT EON");
            draw_text(renderer, 64, 82, "SELECT GAME   |   D: DATA SCAN   |   F1: ORIGINAL / MODERN   |   ESC: QUIT");
            for (std::size_t index = 0; index < cards.size(); ++index) {
                auto& card = cards[index];
                if (card.texture) SDL_RenderTexture(renderer, card.texture, nullptr, &card.bounds);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 185);
                SDL_FRect label{card.bounds.x, card.bounds.y + card.bounds.h - 62, card.bounds.w, 62};
                SDL_RenderFillRect(renderer, &label);
                SDL_SetRenderDrawColor(renderer, index == static_cast<std::size_t>(focused) ? 255 : 130,
                    index == static_cast<std::size_t>(focused) ? 195 : 150, 80, 255);
                SDL_RenderRect(renderer, &card.bounds);
                draw_text(renderer, card.bounds.x + 18, card.bounds.y + card.bounds.h - 45, card.title);
                draw_text(renderer, card.bounds.x + 18, card.bounds.y + card.bounds.h - 25, card.subtitle);
                const auto available = eon::release_available(releases, card.game, std::nullopt);
                draw_text(renderer, card.bounds.x + 18, card.bounds.y + card.bounds.h + 16,
                    available ? "VERIFIED ORIGINAL DATA" : scanner.done() ? "ORIGINAL DATA NOT FOUND" : "SCANNING ORIGINAL DATA...");
            }
            draw_text(renderer, 64, 530, "ENTER / CLICK: START");
            if (show_scanner) {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 220);
                SDL_FRect overlay{64, 572, 1152, 104};
                SDL_RenderFillRect(renderer, &overlay);
                draw_text(renderer, 86, 594, "DATA SCANNER (content hashes, read-only)");
                draw_text(renderer, 86, 618, "Files scanned: " + std::to_string(scanner.scanned_count())
                    + " / " + std::to_string(scanner.candidate_count()));
                draw_text(renderer, 86, 642, scanner.done()
                    ? "Complete. Only hash-verified original releases are selectable."
                    : "Scanning in progress. Press D to hide this progress panel.");
            }
        } else {
            draw_text(renderer, 64, 56, "LAUNCH REQUEST ACCEPTED");
            draw_text(renderer, 64, 92, "Game: " + eon::name(selected));
            draw_text(renderer, 64, 116, modern ? "Presentation: Modern" : "Presentation: Original");
            draw_text(renderer, 64, 156, "Original data is present and selected.");
            draw_text(renderer, 64, 180, "The simulation is incomplete; no synthetic substitute will run.");
            if (selected == eon::Game::millennium && millennium_preview_texture && millennium_assets) {
                const bool millennium_handed_off = millennium_title_session
                    && millennium_title_session->handed_off()
                    && millennium_gx_canvas_texture;
                SDL_Texture* texture = millennium_handed_off ? millennium_gx_canvas_texture
                                                              : millennium_preview_texture;
                const auto& image = millennium_handed_off ? millennium_assets->gx_canvas
                                                            : millennium_assets->title;
                if (millennium_handed_off) {
                    draw_text(renderer, 64, 220,
                        "AUTHENTIC DOS HANDOFF - TITLES.EXE -> 2200ad.exe; GX.LIB IMG00 -> IMG01");
                    draw_text(renderer, 64, 238,
                        "ORIGINAL GX CANVAS + READ-ONLY 2200SAVE.I POSITIONAL TABLE");
                } else {
                    draw_text(renderer, 64, 220, "AUTHENTIC DOS TITLE - P00 INDICES + VGA RGB6 DAC");
                    draw_text(renderer, 64, 238,
                        "PRESS ANY KEY: ORIGINAL INT 21h/AH=06h TITLE HANDOFF");
                }
                SDL_SetTextureScaleMode(texture,
                    modern ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
                const float scale = millennium_handed_off ? 1.65F : 2.0F;
                SDL_FRect preview_bounds{64, 250,
                    static_cast<float>(image.width) * scale,
                    static_cast<float>(image.height) * scale};
                SDL_RenderTexture(renderer, texture, nullptr, &preview_bounds);
                if (millennium_handed_off) {
                    const auto& save = millennium_assets->initial_save;
                    constexpr std::size_t records_per_page = 8;
                    const auto first_record = millennium_state_page * records_per_page;
                    std::ostringstream heading;
                    heading << "2200SAVE.I READ-ONLY  SHA-256 " << save.sha256()
                            << "  VERSION 0x" << std::hex << save.layout().version << std::dec;
                    draw_text(renderer, 610, 270, heading.str());
                    draw_text(renderer, 610, 290,
                        "RECOVERED POSITIONAL WORDS ONLY; NO INFERRED GAME SEMANTICS");
                    for (std::size_t row = 0; row < records_per_page; ++row) {
                        const auto record_index = first_record + row;
                        if (record_index >= save.layout().state_table.size()) break;
                        const auto& record = save.state_record(record_index);
                        std::ostringstream line;
                        line << '[' << record_index << "] +00=0x" << std::hex
                             << record.runtime_offset_0 << " +04=0x" << record.runtime_offset_4
                             << " +06=0x" << record.runtime_offset_6 << " +08=0x"
                             << record.runtime_offset_8;
                        draw_text(renderer, 610, 316.0F + static_cast<float>(row) * 21.0F,
                            line.str());
                    }
                    std::ostringstream pager;
                    pager << "PAGE " << (millennium_state_page + 1) << "/"
                          << ((eon::MillenniumDosSaveLayout::state_table_count + records_per_page - 1)
                              / records_per_page)
                          << "  LEFT/RIGHT: TABLE PAGE";
                    draw_text(renderer, 610, 496, pager.str());
                }
                draw_text(renderer, 64, 680, request.game ? "ESC: QUIT" : "ESC: BACK TO MENU");
            } else if (selected == eon::Game::deuteros && preview_texture && deuteros_opening) {
                const auto frame = deuteros_opening->rgba_frame();
                if (frame) SDL_UpdateTexture(preview_texture, nullptr, frame->data(),
                    eon::DeuterosAmigaFrame::width * 4);
                draw_text(renderer, 64, 220, "AUTHENTIC AMIGA OPENING - ORIGINAL CHANNEL PROGRAM + PALETTE");
                draw_text(renderer, 64, 238, "HOLD SPACE / ENTER: ORIGINAL INPUT SIGNAL");
                draw_text(renderer, 64, 252, "PAULA: ORIGINAL PCM + PERIOD + VOLUME (FIRST DMA PASS)");
                if (deuteros_title_resource) {
                    std::ostringstream handoff;
                    handoff << std::hex << *deuteros_title_resource;
                    draw_text(renderer, 64, 268, "ORIGINAL TITLE HANDOFF: RESOURCE 0x"
                        + handoff.str()
                        + " -> STAGE ENTRY 0x40426 (REIMPLEMENTATION IN PROGRESS)");
                }
                SDL_SetTextureScaleMode(preview_texture,
                    modern ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
                const float scale = 2.0F;
                SDL_FRect preview_bounds{64, deuteros_title_resource ? 290.0F : 274.0F,
                    static_cast<float>(eon::DeuterosAmigaFrame::width) * scale,
                    static_cast<float>(eon::DeuterosAmigaFrame::height) * scale};
                SDL_RenderTexture(renderer, preview_texture, nullptr, &preview_bounds);
                draw_text(renderer, 64, 580, request.game ? "ESC: QUIT" : "ESC: BACK TO MENU");
            } else {
                draw_text(renderer, 64, 220, request.game ? "ESC: QUIT" : "ESC: BACK TO MENU");
            }
        }
        SDL_RenderPresent(renderer);
    }

    for (auto& card : cards) SDL_DestroyTexture(card.texture);
    SDL_DestroyTexture(millennium_preview_texture);
    SDL_DestroyTexture(millennium_gx_canvas_texture);
    SDL_DestroyTexture(preview_texture);
    SDL_DestroyAudioStream(deuteros_audio_stream);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
