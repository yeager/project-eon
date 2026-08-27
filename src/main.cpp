#include "launcher.hpp"
#include "platform/game_data.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>

#include <array>
#include <filesystem>
#include <iostream>
#include <string>

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
    if (!std::filesystem::is_directory(request.data_directory)) {
        std::cerr << "Data directory does not exist: " << request.data_directory << '\n';
        return 2;
    }
    const auto releases = eon::find_release_archives(request.data_directory);
    if (releases.empty()) {
        std::cerr << "No recognised original release archives found.\n";
        return 3;
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
        }
        return found ? 0 : 5;
    }
    if (request.game && !eon::release_available(releases, *request.game, request.platform)) {
        std::cerr << "Requested original release is not present.\n";
        return 4;
    }

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

    Screen screen = request.game ? Screen::launching : Screen::menu;
    eon::Game selected = request.game.value_or(eon::Game::millennium);
    int focused = 0;
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
            if (screen == Screen::menu && event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_LEFT || event.key.key == SDLK_RIGHT) focused = 1 - focused;
                if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                    selected = cards[static_cast<std::size_t>(focused)].game;
                    screen = Screen::launching;
                }
            }
            if (screen == Screen::menu && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                float x = 0, y = 0;
                SDL_RenderCoordinatesFromWindow(renderer, event.button.x, event.button.y, &x, &y);
                for (std::size_t index = 0; index < cards.size(); ++index) {
                    if (inside(cards[index].bounds, x, y)) {
                        focused = static_cast<int>(index);
                        selected = cards[index].game;
                        screen = Screen::launching;
                    }
                }
            }
        }

        const bool modern = request.presentation == eon::Presentation::modern;
        SDL_SetRenderDrawColor(renderer, modern ? 5 : 0, modern ? 15 : 0, modern ? 27 : 0, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 205, 225, 235, 255);

        if (screen == Screen::menu) {
            draw_text(renderer, 64, 56, "PROJECT EON");
            draw_text(renderer, 64, 82, "SELECT GAME   |   F1: ORIGINAL / MODERN   |   ESC: QUIT");
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
            }
            draw_text(renderer, 64, 530, "ENTER / CLICK: START");
        } else {
            draw_text(renderer, 64, 56, "LAUNCH REQUEST ACCEPTED");
            draw_text(renderer, 64, 92, "Game: " + eon::name(selected));
            draw_text(renderer, 64, 116, modern ? "Presentation: Modern" : "Presentation: Original");
            draw_text(renderer, 64, 156, "Original data is present and selected.");
            draw_text(renderer, 64, 180, "The decoder/simulation is incomplete; no synthetic substitute will run.");
            draw_text(renderer, 64, 220, request.game ? "ESC: QUIT" : "ESC: BACK TO MENU");
        }
        SDL_RenderPresent(renderer);
    }

    for (auto& card : cards) SDL_DestroyTexture(card.texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
