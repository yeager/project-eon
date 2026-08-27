#include "launcher.hpp"
#include "engine/deuteros_amiga_opening.hpp"
#include "data/amiga_adf.hpp"
#include "data/deuteros_amiga_bundle.hpp"
#include "data/deuteros_amiga_channel_vm.hpp"
#include "data/deuteros_amiga_frame.hpp"
#include "data/deuteros_amiga_loader.hpp"
#include "data/millennium_dos_bitmap.hpp"
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
#include <optional>
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
    const auto& handoff = plan.title_handoff_profile;
    std::cout << "          Title input profile: disk 0x" << std::hex << handoff.disk_offset
        << ", length 0x" << handoff.length << ", memory 0x" << handoff.destination
        << std::dec << '\n';
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
}

std::optional<PreviewAnimation> load_millennium_preview(
    const std::vector<eon::ReleaseArchive>& releases) {
    constexpr auto title_lib_sha256 =
        "6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678";
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
        return PreviewAnimation{bitmap.width, bitmap.height,
            {eon::colorize_millennium_dos_bitmap(bitmap, palette)}};
    } catch (const std::exception& error) {
        std::cerr << "Unable to decode Millennium title preview: " << error.what() << '\n';
        return std::nullopt;
    }
}

std::optional<PreviewAnimation> load_deuteros_preview(
    const std::vector<eon::ReleaseArchive>& releases) {
    constexpr auto clean_system_adf =
        "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38";
    const auto release = std::find_if(releases.begin(), releases.end(), [](const auto& candidate) {
        return candidate.game == eon::Game::deuteros && candidate.platform == eon::Platform::amiga;
    });
    if (release == releases.end()) return std::nullopt;
    try {
        const auto image = eon::extract_asset_by_sha256(release->path, clean_system_adf);
        if (!image) return std::nullopt;
        eon::DeuterosAmigaOpening opening(*image);
        PreviewAnimation preview{eon::DeuterosAmigaFrame::width,
            eon::DeuterosAmigaFrame::height, {}};
        constexpr std::size_t maximum_verified_ticks = 512;
        for (std::size_t tick = 0; tick < maximum_verified_ticks; ++tick) {
            static_cast<void>(opening.tick());
            if (!opening.frame_composed_on_last_tick()) break;
            const auto frame = opening.rgba_frame();
            if (!frame) break;
            preview.rgba_frames.push_back(*frame);
        }
        if (preview.rgba_frames.empty()) return std::nullopt;
        return preview;
    } catch (const std::exception& error) {
        std::cerr << "Unable to decode Deuteros preview: " << error.what() << '\n';
        return std::nullopt;
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
            if (release.game == eon::Game::millennium
                && release.platform == eon::Platform::dos
                && release.language == "en") {
                report_millennium_dos(release);
            }
        }
        return found ? 0 : 5;
    }
    if (request.game && !eon::release_available(releases, *request.game, request.platform)) {
        std::cerr << "Requested original release is not present.\n";
        return 4;
    }
    std::optional<PreviewAnimation> deuteros_preview;
    if (!releases.empty()) deuteros_preview = load_deuteros_preview(releases);
    const auto millennium_preview = load_millennium_preview(releases);

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
    SDL_Texture* preview_texture = nullptr;
    const auto create_deuteros_preview_texture = [&] {
        if (!deuteros_preview || preview_texture) return;
        preview_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STREAMING, deuteros_preview->width, deuteros_preview->height);
        if (preview_texture) {
            SDL_UpdateTexture(preview_texture, nullptr, deuteros_preview->rgba_frames.front().data(),
                deuteros_preview->width * 4);
        }
    };
    create_deuteros_preview_texture();
    SDL_Texture* millennium_preview_texture = nullptr;
    if (millennium_preview) {
        millennium_preview_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STATIC, millennium_preview->width, millennium_preview->height);
        if (millennium_preview_texture) {
            SDL_UpdateTexture(millennium_preview_texture, nullptr,
                millennium_preview->rgba_frames.front().data(), millennium_preview->width * 4);
        }
    }

    Screen screen = request.game ? Screen::launching : Screen::menu;
    eon::Game selected = request.game.value_or(eon::Game::millennium);
    int focused = 0;
    std::uint64_t animation_start = SDL_GetTicks();
    std::size_t displayed_preview_frame = 0;
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
                        animation_start = SDL_GetTicks();
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
                            animation_start = SDL_GetTicks();
                        }
                    }
                }
            }
        }

        if (!scanner.done()) {
            static_cast<void>(scanner.advance(show_scanner ? 32 : 1));
            releases = scanner.releases();
            if (!deuteros_preview) deuteros_preview = load_deuteros_preview(releases);
            create_deuteros_preview_texture();
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
            if (selected == eon::Game::millennium && millennium_preview_texture && millennium_preview) {
                draw_text(renderer, 64, 220, "AUTHENTIC DOS TITLE - P00 INDICES + VGA RGB6 DAC");
                SDL_SetTextureScaleMode(millennium_preview_texture,
                    modern ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
                const float scale = 2.0F;
                SDL_FRect preview_bounds{64, 250,
                    static_cast<float>(millennium_preview->width) * scale,
                    static_cast<float>(millennium_preview->height) * scale};
                SDL_RenderTexture(renderer, millennium_preview_texture, nullptr, &preview_bounds);
                draw_text(renderer, 64, 680, request.game ? "ESC: QUIT" : "ESC: BACK TO MENU");
            } else if (selected == eon::Game::deuteros && preview_texture && deuteros_preview) {
                const auto elapsed_ticks = static_cast<std::size_t>((SDL_GetTicks() - animation_start) / 20U);
                const auto frame_index = std::min(elapsed_ticks,
                    deuteros_preview->rgba_frames.size() - 1);
                if (frame_index != displayed_preview_frame) {
                    SDL_UpdateTexture(preview_texture, nullptr,
                        deuteros_preview->rgba_frames[frame_index].data(), deuteros_preview->width * 4);
                    displayed_preview_frame = frame_index;
                }
                draw_text(renderer, 64, 220, "AUTHENTIC AMIGA OPENING - ORIGINAL CHANNEL PROGRAM + PALETTE");
                SDL_SetTextureScaleMode(preview_texture,
                    modern ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
                const float scale = 2.0F;
                SDL_FRect preview_bounds{64, 250,
                    static_cast<float>(deuteros_preview->width) * scale,
                    static_cast<float>(deuteros_preview->height) * scale};
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
    SDL_DestroyTexture(preview_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
