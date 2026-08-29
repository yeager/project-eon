#include "launcher.hpp"
#include "launcher_text.hpp"
#include "i18n.hpp"
#include "engine/deuteros_amiga_opening.hpp"
#include "engine/deuteros_amiga_paula.hpp"
#include "engine/deuteros_atari_bootstrap_session.hpp"
#include "engine/millennium_dos_game_session.hpp"
#include "engine/millennium_dos_title_session.hpp"
#include "engine/millennium_dos_save_session.hpp"
#include "engine/millennium_atari_bootstrap_session.hpp"
#include "engine/millennium_amiga_bootstrap_session.hpp"
#include "data/amiga_adf.hpp"
#include "data/atari_st_prg.hpp"
#include "data/atari_st_stx.hpp"
#include "data/creative_voice.hpp"
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
#include "data/millennium_dos_game_flow.hpp"
#include "data/millennium_dos_gameplay_screen.hpp"
#include "data/millennium_dos_last_screen.hpp"
#include "data/millennium_amiga_loader.hpp"
#include "data/millennium_dos_lib.hpp"
#include "data/millennium_dos_title_flow.hpp"
#include "data/millennium_dos_title_exit.hpp"
#include "data/millennium_dos_title_transition.hpp"
#include "data/millennium_dos_video_driver.hpp"
#include "data/millennium_dos_sound_driver.hpp"
#include "data/millennium_save_comparison.hpp"
#include "data/modern_asset_pack.hpp"
#include "data/modern_pixel_reconstruction.hpp"
#include "data/sha256.hpp"
#include "data/reference_trace.hpp"
#include "data/recovery_map.hpp"
#include "data/zip_archive.hpp"
#include "platform/game_data.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <limits>
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
// The launcher deliberately separates the three choices into pages.  A game
// card cannot accidentally start whichever platform happened to be selected:
// the platform page records an explicit, hash-verified choice first, then the
// profile page records Original, Modern, or a user-tuned Modern launch.
enum class LauncherPage { games, platforms, profiles };

const eon::Translator* active_translator = nullptr;
std::unique_ptr<eon::UnicodeTextRenderer> active_text_renderer;

struct Card {
    eon::Game game;
    const char* title;
    const char* subtitle;
    const char* filename;
    SDL_FRect bounds;
    SDL_Texture* texture = nullptr;
};

struct PlatformCard {
    eon::Platform platform;
    const char* title;
    const char* filename;
    SDL_FRect bounds;
    SDL_Texture* texture = nullptr;
};

enum class ProfileChoice { original, modern, custom };

struct ProfileCard {
    ProfileChoice choice;
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

// Modern options are renderer state only. They are deliberately independent
// from original input, media, simulation state and save bytes.
struct ModernGraphicsSettings {
    bool smooth_scaling = true;
    // Reconstruct an edge-aware 2x renderer texture from a decoded original
    // surface. The source vector is retained unchanged and the result exists
    // only in process memory; Original never takes this path.
    bool pixel_reconstruction = true;
    bool scanlines = false;
    bool frame = true;
    // The selected output mode controls only the SDL window.  Original frame
    // dimensions, indexed pixels and simulation state remain unchanged.
    std::size_t output_resolution_index = 0;
    std::size_t aspect_ratio_index = 0;
    int focused_option = 0;
};

struct OutputResolution {
    int width = 1280;
    int height = 720;
};

constexpr std::array<OutputResolution, 3> output_resolutions{{
    {1280, 720}, {1600, 900}, {1920, 1080},
}};

// Every option has an explicit display ratio.  The renderer derives both
// viewport dimensions from the same ratio so it never independently stretches
// width and height into an accidental, visually odd shape.
constexpr std::array<float, 3> display_aspect_ratios{{4.0F / 3.0F, 8.0F / 5.0F, 16.0F / 9.0F}};
constexpr std::array<const char*, 3> display_aspect_names{{
    "ORIGINAL 4:3", "SQUARE PIXELS 8:5", "WIDESCREEN 16:9",
}};

std::size_t output_resolution_index_for(const eon::DisplayPreferences& display) {
    for (std::size_t index = 0; index < output_resolutions.size(); ++index) {
        if (output_resolutions[index].width == display.width
            && output_resolutions[index].height == display.height) return index;
    }
    throw std::runtime_error("Unsupported validated display resolution");
}

// These data products come from the same verified English DOS archive. The
// title is launchable; GX remains read-only inspection evidence because the
// console poll proves neither the DOS return nor 2200AD startup.
struct MillenniumDosLaunchAssets {
    PreviewAnimation title;
    std::string language;
    std::optional<PreviewAnimation> gx_canvas;
    std::optional<eon::MillenniumDosTitleFlow> title_flow;
    std::optional<eon::MillenniumDosGameFlow> game_flow;
    // Both private video drivers are loaded from the same verified DOS
    // release as the launcher. Keeping their parsed ABI profiles alongside
    // the launch assets prevents the SDL path from silently relying on a
    // report-only parser, while still leaving driver selection/execution to
    // a later, evidence-backed startup implementation.
    std::optional<eon::MillenniumDosVideoDriverProfile> ega_video_driver;
    std::optional<eon::MillenniumDosVideoDriverProfile> mcga_video_driver;
    // This is intentionally the original serialized image, not a projected
    // game model.  The launcher exposes only the recovered positional words
    // once TITLES.EXE has made its verified hand-off.
    std::optional<eon::MillenniumDosSaveSession> initial_save;
};

void draw_text(SDL_Renderer* renderer, float x, float y, const std::string& text) {
    const auto translated = active_translator ? active_translator->translate(text) : std::string_view(text);
    const std::string localized(translated);
    if (active_text_renderer && active_text_renderer->draw(x, y, localized)) return;
    // SDL_RenderDebugText is an emergency diagnostic only. SDL3 documents it
    // as ASCII-only; a renderer setup failure must not replace localized UTF-8
    // with transliterations or synthetic text.
    SDL_RenderDebugText(renderer, x, y, localized.c_str());
}

// These are launcher labels, rather than names read from original media. Keep
// the CLI/provenance names in game_data untouched: only rendered launcher UI
// passes through the catalog.
std::string_view launcher_game_label(const eon::Game game) {
    return game == eon::Game::millennium ? "MILLENNIUM 2.2" : "DEUTEROS";
}

std::string_view launcher_platform_label(const eon::Platform platform) {
    switch (platform) {
    case eon::Platform::dos: return "DOS";
    case eon::Platform::amiga: return "AMIGA";
    case eon::Platform::atari_st: return "ATARI ST";
    }
    return "UNKNOWN PLATFORM";
}

// Modern presentation is deliberately renderer-only. It frames original
// decoded surfaces and may derive a transient, opt-in in-memory Scale2x
// texture from them; it neither changes an input, simulation, save byte, nor
// writes or substitutes supplied media.
void draw_modern_chrome(SDL_Renderer* renderer) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 9, 28, 48, 235);
    SDL_FRect header{32, 24, 1216, 112};
    SDL_RenderFillRect(renderer, &header);
    SDL_SetRenderDrawColor(renderer, 39, 202, 213, 210);
    SDL_RenderRect(renderer, &header);
    SDL_SetRenderDrawColor(renderer, 19, 86, 116, 110);
    for (int x = 48; x < 1248; x += 48) {
        SDL_RenderLine(renderer, static_cast<float>(x), 144, static_cast<float>(x), 696);
    }
    for (int y = 168; y < 696; y += 48) {
        SDL_RenderLine(renderer, 32, static_cast<float>(y), 1248, static_cast<float>(y));
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void draw_modern_surface_frame(SDL_Renderer* renderer, const SDL_FRect& bounds) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 4, 13, 25, 210);
    SDL_FRect shadow{bounds.x - 12, bounds.y - 12, bounds.w + 24, bounds.h + 24};
    SDL_RenderFillRect(renderer, &shadow);
    SDL_SetRenderDrawColor(renderer, 39, 202, 213, 235);
    SDL_RenderRect(renderer, &shadow);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void draw_scanlines(SDL_Renderer* renderer, const SDL_FRect& bounds) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 56);
    for (float y = bounds.y + 1; y < bounds.y + bounds.h; y += 2) {
        SDL_RenderLine(renderer, bounds.x, y, bounds.x + bounds.w, y);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

// This report is intentionally shared by every platform inspector. The map
// has already been bounded by the rehashed outer archive and parser-profile
// manifest; printing it neither extracts more media nor dispatches any guest
// code. It is a diagnostic equivalent of a symbol map, not a hook table.
void report_recovery_map(const eon::ReleaseArchive& release) {
    const auto entries = eon::recovery_map_for_release(release.sha256);
    if (entries.empty()) return;
    std::cout << "          RECOVERY MAP  " << entries.size()
        << " hash-bound static path" << (entries.size() == 1 ? "" : "s") << '\n';
    for (const auto& entry : entries) {
        if (!eon::release_has_recovery_map_entry(release.sha256, entry.id)) {
            throw std::runtime_error("Recovery-map entry lost its parser-profile binding");
        }
        std::cout << "            " << entry.id << ": profile " << entry.parser_profile_id
            << ", " << entry.cpu << " " << entry.source_address << ", "
            << entry.evidence_level << "; " << entry.runtime_status << '\n';
    }
}

SDL_FRect aspect_viewport(const float x, const float y, const float maximum_width,
    const float maximum_height, const ModernGraphicsSettings& settings) {
    const auto ratio = display_aspect_ratios.at(settings.aspect_ratio_index);
    float width = maximum_width;
    float height = width / ratio;
    if (height > maximum_height) {
        height = maximum_height;
        width = height * ratio;
    }
    // Center inside the allocated presentation region.  This makes a wider
    // or narrower chosen ratio deliberate and legible, never a clipped crop.
    return {x + (maximum_width - width) / 2.0F, y + (maximum_height - height) / 2.0F,
        width, height};
}

void draw_modern_graphics_popup(SDL_Renderer* renderer,
    const ModernGraphicsSettings& settings) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 3, 10, 20, 240);
    SDL_FRect panel{356, 142, 568, 430};
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 39, 202, 213, 255);
    SDL_RenderRect(renderer, &panel);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    draw_text(renderer, 390, 174, "MODERN GRAPHICS SETTINGS");
    draw_text(renderer, 390, 212, "UP/DOWN: SELECT   LEFT/RIGHT: CHANGE   F10: CLOSE");
    constexpr std::array<const char*, 6> names{{
        "OUTPUT RESOLUTION", "ASPECT RATIO", "PIXEL RECONSTRUCTION", "SMOOTH SCALING", "SCANLINES", "MODERN FRAME",
    }};
    const auto& resolution = output_resolutions.at(settings.output_resolution_index);
    const std::array<std::string, 6> values{{
        std::to_string(resolution.width) + "x" + std::to_string(resolution.height),
        display_aspect_names.at(settings.aspect_ratio_index),
        settings.pixel_reconstruction ? "SCALE2X (MEMORY ONLY)" : "OFF (ORIGINAL PIXELS)",
        settings.smooth_scaling ? "ON" : "OFF",
        settings.scanlines ? "ON" : "OFF",
        settings.frame ? "ON" : "OFF",
    }};
    for (std::size_t index = 0; index < names.size(); ++index) {
        SDL_SetRenderDrawColor(renderer, index == static_cast<std::size_t>(settings.focused_option)
                ? 255 : 205, index == static_cast<std::size_t>(settings.focused_option) ? 195 : 225,
            index == static_cast<std::size_t>(settings.focused_option) ? 80 : 235, 255);
        draw_text(renderer, 390, 256.0F + static_cast<float>(index) * 42.0F,
            std::string(index == static_cast<std::size_t>(settings.focused_option) ? "> " : "  ") + names[index]);
        draw_text(renderer, 690, 256.0F + static_cast<float>(index) * 42.0F, values[index]);
    }
    SDL_SetRenderDrawColor(renderer, 205, 225, 235, 255);
    draw_text(renderer, 390, 546, "SETTINGS APPLY TO SDL RENDERING ONLY.");
}

bool inside(const SDL_FRect& rectangle, float x, float y) {
    return x >= rectangle.x && x <= rectangle.x + rectangle.w
        && y >= rectangle.y && y <= rectangle.y + rectangle.h;
}

SDL_Texture* load_card(SDL_Renderer* renderer, const char* filename) {
    const auto base = std::filesystem::path(SDL_GetBasePath());
    const std::array<std::filesystem::path, 4> candidates{{
        base / "assets" / "cards" / filename,
        base / "Resources" / "assets" / "cards" / filename,
        std::filesystem::path(EON_ASSET_DIR) / "cards" / filename,
        std::filesystem::path("assets") / "cards" / filename,
    }};
    for (const auto& path : candidates) {
        if (SDL_Texture* texture = IMG_LoadTexture(renderer, path.string().c_str())) return texture;
    }
    std::cerr << "Unable to load card " << filename << ": " << SDL_GetError() << '\n';
    return nullptr;
}

std::optional<std::filesystem::path> find_font_directory() {
    const auto base = std::filesystem::path(SDL_GetBasePath());
    const std::array<std::filesystem::path, 4> candidates{{
        base / "assets" / "fonts",
        base / "Resources" / "assets" / "fonts",
        std::filesystem::path(EON_FONT_DIR),
        std::filesystem::path("assets") / "fonts",
    }};
    for (const auto& candidate : candidates) {
        if (std::filesystem::is_regular_file(candidate / "NotoSans-Regular.ttf")) return candidate;
    }
    return std::nullopt;
}

void report_deuteros_amiga(const eon::ReleaseArchive& release) {
    constexpr auto clean_system_adf =
        "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38";
    const auto image = eon::extract_verified_release_asset(release, clean_system_adf);
    if (!image) return;
    const eon::AmigaAdf disk(*image);
    const auto plan = eon::parse_deuteros_amiga_load_plan(disk);
    std::cout << "          ADF boot checksum valid; main stage disk 0x" << std::hex
        << plan.main_stage.disk_offset << " -> memory 0x" << plan.main_stage.destination
        << ", entry 0x" << plan.main_stage.entry_address << std::dec << '\n';
    const auto& main_entry = plan.main_stage_entry;
    std::cout << "          Main entry evidence: incoming A1/D0 -> 0x" << std::hex
        << main_entry.incoming_controller_cell << "/0x" << main_entry.incoming_mode_cell
        << ", stack 0x" << main_entry.stack_address << ", ceiling 0x"
        << main_entry.memory_ceiling << ", raw init calls 0x"
        << main_entry.initialization_calls[0] << "/0x" << main_entry.initialization_calls[1]
        << std::dec << '\n';
    std::cout << "          Main loop evidence: 0x" << std::hex << main_entry.loop_address
        << " resets words 0x" << main_entry.first_state_word_address << "/0x"
        << main_entry.second_state_word_address << ", enables 0x"
        << main_entry.scheduler_enable_word_address << "=0x"
        << main_entry.scheduler_enable_word_value << ", services 0x"
        << main_entry.loop_first_service_address << "/0x" << main_entry.loop_scheduler_address
        << ", probes 0x" << main_entry.first_input_address << " bit " << std::dec
        << static_cast<unsigned>(main_entry.first_input_bit) << " and 0x" << std::hex
        << main_entry.second_input_address << " bit " << std::dec
        << static_cast<unsigned>(main_entry.second_input_bit) << '\n';
    std::cout << "          >2 scheduler evidence: state 0x" << std::hex
        << main_entry.scheduler_state_base_address << ", count 0x"
        << main_entry.scheduler_channel_count_address << ", stride 0x"
        << main_entry.scheduler_channel_stride << ", program/selector/value +0x"
        << main_entry.scheduler_active_program_offset << "/+0x"
        << main_entry.scheduler_wait_selector_offset << "/+0x"
        << main_entry.scheduler_wait_value_offset << ", selectors 0x" << std::hex
        << static_cast<unsigned>(main_entry.scheduler_wait_selectors[0]) << "/0x"
        << static_cast<unsigned>(main_entry.scheduler_wait_selectors[1]) << "/0x"
        << static_cast<unsigned>(main_entry.scheduler_wait_selectors[2]) << "/0x"
        << static_cast<unsigned>(main_entry.scheduler_wait_selectors[3])
        << "; tail bit " << std::dec << static_cast<unsigned>(main_entry.scheduler_tail_probe_bit)
        << " at 0x" << std::hex << main_entry.scheduler_tail_probe_address
        << " -> 0x" << main_entry.scheduler_tail_service_address << std::dec << '\n';
    std::cout << "          Input dispatch evidence: 0x" << std::hex
        << main_entry.input_dispatch_address << " compares word 0x"
        << main_entry.input_dispatch_state_address << " with " << std::dec
        << main_entry.input_dispatch_compare_value << ", clamps below to "
        << main_entry.input_dispatch_clamped_value << ", routes <= to 0x"
        << std::hex << main_entry.input_dispatch_service_address << " and > to 0x"
        << main_entry.input_dispatch_continue_address << std::dec << '\n';
    std::cout << "          <= service evidence: increments word 0x" << std::hex
        << main_entry.dispatch_service_state_address << "; result " << std::dec
        << main_entry.dispatch_service_first_exit_value << " -> 0x" << std::hex
        << main_entry.dispatch_service_first_exit_address << " (profile cell 0x"
        << main_entry.first_exit_profile_cell_address << " = " << std::dec
        << main_entry.first_exit_profile_value << ", return slots 0x" << std::hex
        << main_entry.bootstrap_controller_return_cell << "/0x"
        << main_entry.bootstrap_profile_return_cell << "); result " << std::dec
        << main_entry.dispatch_service_second_exit_value << " -> 0x" << std::hex
        << main_entry.dispatch_service_second_exit_address << " (cell 0x"
        << main_entry.second_exit_profile_cell_address << " = " << std::dec
        << main_entry.second_exit_initial_profile_value << ", service 0x" << std::hex
        << main_entry.second_exit_service_address << ", match 0x"
        << main_entry.second_exit_service_match_value << " -> 0x"
        << main_entry.second_exit_matched_return_address << ")" << std::dec << '\n';
    for (std::size_t index = 0; index < 2; ++index) {
        const auto bundle = eon::parse_deuteros_amiga_bundle(
            disk, plan.resource_disk_offsets[index]);
        std::cout << "          Resource bundle " << index << ": disk 0x" << std::hex
            << bundle.disk_offset << ", 0x" << bundle.length << std::dec
            << " bytes, " << bundle.object_count << " objects, mode "
            << bundle.mode_flag << '\n';
    }
    for (std::uint16_t index = 0; index < 2; ++index) {
        const auto transfer = eon::read_deuteros_amiga_main_resource(disk, plan, index);
        if (transfer) {
            std::cout << "          Resource transfer " << index << ": ADF 0x" << std::hex
                << transfer->source_disk_offset << " -> RAM 0x"
                << transfer->payload_destination_address << ", 0x"
                << transfer->payload_length << std::dec << " original bytes (in memory only)\n";
            const auto observation = eon::sample_deuteros_amiga_main_resource_consumer(
                *transfer, main_entry, 0, 0);
            std::cout << "            $2016a raw observation (seed/counter 0): offset 0x"
                << std::hex << observation.table_offset << ", word 0x"
                << observation.sampled_word << ", result 0x" << observation.addend_result
                << ", seed 0x" << observation.seed_after << std::dec << '\n';
        }
    }
    std::cout << "          Resource consumer: 0x" << std::hex
        << main_entry.resource_consumer_address << " loads 0x"
        << main_entry.resource_consumer_base_address << ", seed/counter 0x"
        << main_entry.resource_consumer_seed_address << "/0x"
        << main_entry.resource_consumer_counter_address << ", mask 0x"
        << main_entry.resource_consumer_index_mask << ", +0x"
        << main_entry.resource_consumer_word_addend << "; command arms 0x"
        << main_entry.resource_consumer_command_words[0] << "@0x"
        << main_entry.resource_consumer_call_sites[0] << "/0x"
        << main_entry.resource_consumer_command_words[1] << "@0x"
        << main_entry.resource_consumer_call_sites[1] << std::dec << '\n';
    std::cout << "          Channel command 0x10: writes 0x" << std::hex
        << main_entry.channel_request_value << " to 0x"
        << main_entry.channel_request_cell_address << "; loop tests at 0x"
        << main_entry.channel_request_loop_test_address << " and nonzero branch 0x"
        << main_entry.channel_request_loop_branch_address << " -> 0x"
        << main_entry.channel_request_continuation_address << std::dec << '\n';
    const auto channel_request_continuation =
        eon::parse_deuteros_amiga_channel_request_continuation(disk, plan);
    std::cout << "          Channel-request static continuation: ADF 0x" << std::hex
        << plan.main_stage.disk_offset + channel_request_continuation.entry_address
            - plan.main_stage.destination
        << ", entry 0x" << channel_request_continuation.entry_address
        << "; BSR 0x" << channel_request_continuation.local_call_addresses[0] << " -> 0x"
        << channel_request_continuation.local_call_targets[0] << ", 0x"
        << channel_request_continuation.local_call_addresses[1] << " -> 0x"
        << channel_request_continuation.local_call_targets[1] << "; bit " << std::dec
        << static_cast<unsigned>(channel_request_continuation.input_test_bit) << " loop 0x"
        << std::hex << channel_request_continuation.input_zero_branch_address << " -> 0x"
        << channel_request_continuation.input_zero_branch_target << "; final 0x"
        << channel_request_continuation.final_branch_address << " -> 0x"
        << channel_request_continuation.final_branch_target << std::dec << "; SHA-256 "
        << channel_request_continuation.raw_sha256
        << " (static only; no condition, call, or input-port execution)\n";
    const auto channel_request_first_callee =
        eon::parse_deuteros_amiga_channel_request_first_callee(
            disk, plan, channel_request_continuation);
    std::cout << "          Channel-request first callee: ADF 0x" << std::hex
        << plan.main_stage.disk_offset + channel_request_first_callee.entry_address
            - plan.main_stage.destination
        << ", entry 0x" << channel_request_first_callee.entry_address
        << "; bit " << std::dec << static_cast<unsigned>(channel_request_first_callee.input_test_bit)
        << " branch 0x" << std::hex << channel_request_first_callee.input_zero_branch_address
        << " -> 0x" << channel_request_first_callee.input_zero_branch_target
        << "; DBRA 0x" << channel_request_first_callee.loop_branch_address << " -> 0x"
        << channel_request_first_callee.loop_branch_target << "; vectors 0x"
        << channel_request_first_callee.vector_call_addresses[0] << "/0x"
        << channel_request_first_callee.vector_call_addresses[1] << "; final services 0x"
        << channel_request_first_callee.final_service_call_addresses[0] << "/0x"
        << channel_request_first_callee.final_service_call_addresses[1] << " -> 0x"
        << channel_request_first_callee.final_service_target << "; SHA-256 "
        << channel_request_first_callee.raw_sha256 << std::dec
        << " (static only; no polls, writes, vectors, calls, or returns executed)\n";
    const auto channel_request_second_callee =
        eon::parse_deuteros_amiga_channel_request_second_callee(
            disk, plan, channel_request_continuation);
    std::cout << "          Channel-request second callee: ADF 0x" << std::hex
        << plan.main_stage.disk_offset + channel_request_second_callee.entry_address
            - plan.main_stage.destination
        << ", entry 0x" << channel_request_second_callee.entry_address
        << "; longword 0x" << channel_request_second_callee.copied_longword_source_address
        << " -> 0x" << channel_request_second_callee.copied_longword_destination_address
        << "; clears 0x" << channel_request_second_callee.cleared_word_addresses[0]
        << "/0x" << channel_request_second_callee.cleared_word_addresses[1] << "/0x"
        << channel_request_second_callee.cleared_word_addresses[2] << "/0x"
        << channel_request_second_callee.cleared_word_addresses[3] << "; 0x"
        << channel_request_second_callee.final_word_value << " -> 0x"
        << channel_request_second_callee.final_word_address << "; RTS 0x"
        << channel_request_second_callee.return_address << "; SHA-256 "
        << channel_request_second_callee.raw_sha256 << std::dec
        << " (static only; no low-memory/custom-register effects executed)\n";
    const auto channel_request_following_service =
        eon::parse_deuteros_amiga_channel_request_following_service(
            disk, plan, channel_request_continuation);
    std::cout << "          Channel-request following service: ADF 0x" << std::hex
        << plan.main_stage.disk_offset + channel_request_following_service.entry_address
            - plan.main_stage.destination
        << ", entry 0x" << channel_request_following_service.entry_address
        << "; execution 0x" << channel_request_following_service.execution_entry_address
        << ", embedded table 0x" << channel_request_following_service.embedded_table_address
        << ", descriptors 0x" << channel_request_following_service.descriptor_base_address
        << " stride 0x" << channel_request_following_service.descriptor_stride
        << "; RTS 0x" << channel_request_following_service.return_address
        << "; SHA-256 " << channel_request_following_service.raw_sha256 << std::dec
        << " (static only; no descriptor, flag, or runtime-cell effects executed)\n";
    const auto channel_request_adjacent_entry =
        eon::parse_deuteros_amiga_channel_request_adjacent_entry(
            disk, plan, channel_request_following_service);
    std::cout << "          Channel-request adjacent entry: ADF 0x" << std::hex
        << plan.main_stage.disk_offset + channel_request_adjacent_entry.entry_address
            - plan.main_stage.destination
        << ", entry 0x" << channel_request_adjacent_entry.entry_address
        << "; test 0x" << channel_request_adjacent_entry.tested_byte_address
        << ", zero 0x" << channel_request_adjacent_entry.zero_branch_address
        << " -> 0x" << channel_request_adjacent_entry.zero_branch_target
        << ", early RTS 0x" << channel_request_adjacent_entry.early_return_address
        << "; descriptors 0x" << channel_request_adjacent_entry.descriptor_base_address
        << " stride 0x" << channel_request_adjacent_entry.descriptor_stride
        << "; final RTS 0x" << channel_request_adjacent_entry.final_return_address
        << "; SHA-256 " << channel_request_adjacent_entry.raw_sha256 << std::dec
        << " (static only; no caller state, pointers, branches, or copies executed)\n";
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
    std::cout << "          Title initialization requirement: stack 0x" << std::hex
        << title_stage.initialization_stack_address << ", Exec base 0x"
        << title_stage.initialization_exec_base_address << " vectors -0x"
        << static_cast<std::uint16_t>(-title_stage.initialization_exec_vectors[0]) << "/-0x"
        << static_cast<std::uint16_t>(-title_stage.initialization_exec_vectors[1])
        << ", D0 0x" << title_stage.initialization_exec_allocation_size
        << "; custom 0x" << title_stage.initialization_custom_base_address
        << "+40/42/9a/96 (not called or written)" << std::dec << '\n';
    const auto post_exec_pointer_seed =
        eon::parse_deuteros_amiga_title_post_exec_pointer_seed_profile(disk, plan);
    std::cout << "          Conditional post-Exec pointer seed: call 0x" << std::hex
        << post_exec_pointer_seed.call_site_address << " with D1=0x"
        << post_exec_pointer_seed.caller_d1_literal << " -> local 0x"
        << post_exec_pointer_seed.callee_address << ", literal 0x"
        << post_exec_pointer_seed.literal_value << " to cell 0x"
        << post_exec_pointer_seed.destination_address
        << " (prior ABI returns unmodelled)" << std::dec << '\n';
    const auto post_exec_state_init =
        eon::parse_deuteros_amiga_title_post_exec_state_init_profile(disk, plan);
    std::cout << "          Conditional post-Exec local state init: call 0x" << std::hex
        << post_exec_state_init.caller_address << " -> 0x"
        << post_exec_state_init.entry_address << "; words 0x"
        << post_exec_state_init.cleared_word_value << "/0x"
        << post_exec_state_init.initial_word_value << " -> 0x"
        << post_exec_state_init.cleared_word_address << "/0x"
        << post_exec_state_init.initial_word_address << ", long 0x"
        << post_exec_state_init.initial_long_value << " -> 0x"
        << post_exec_state_init.initial_long_address << ", word 0x"
        << post_exec_state_init.copied_word_source_address << " -> 0x"
        << post_exec_state_init.copied_word_destination_address
        << " (prior ABI returns unmodelled)" << std::dec << '\n';
    const auto post_exec_third_service =
        eon::parse_deuteros_amiga_title_post_exec_third_service_profile(disk, plan);
    std::cout << "          Conditional post-Exec third service: call 0x" << std::hex
        << post_exec_third_service.caller_address << " -> 0x"
        << post_exec_third_service.dispatch_entry_address << " -> local 0x"
        << post_exec_third_service.graphics_service_address << "; vectors -0x"
        << static_cast<std::uint16_t>(-post_exec_third_service.graphics_library_vectors[0])
        << "/-0x" << static_cast<std::uint16_t>(-post_exec_third_service.graphics_library_vectors[1])
        << "/-0x" << static_cast<std::uint16_t>(-post_exec_third_service.graphics_library_vectors[2])
        << ", tail 0x" << post_exec_third_service.dispatcher_tail_jump_address
        << " (prior ABI returns unmodelled)" << std::dec << '\n';
    const auto post_exec_tail_dispatch =
        eon::parse_deuteros_amiga_title_post_exec_tail_dispatch_profile(disk, plan);
    std::cout << "          Conditional post-Exec tail dispatch: 0x" << std::hex
        << post_exec_tail_dispatch.caller_address << " -> 0x"
        << post_exec_tail_dispatch.entry_address << "; BSR targets 0x"
        << post_exec_tail_dispatch.local_call_addresses[0] << "/0x"
        << post_exec_tail_dispatch.local_call_addresses[1] << "/0x"
        << post_exec_tail_dispatch.local_call_addresses[2] << "/0x"
        << post_exec_tail_dispatch.local_call_addresses[3] << ", RTS 0x"
        << post_exec_tail_dispatch.return_address
        << " (prior ABI returns unmodelled)" << std::dec << '\n';
    const auto post_exec_tail_first_callee =
        eon::parse_deuteros_amiga_title_post_exec_tail_first_callee_profile(disk, plan);
    std::cout << "          Conditional tail first callee: BSR 0x" << std::hex
        << post_exec_tail_first_callee.caller_address << " -> local 0x"
        << post_exec_tail_first_callee.entry_address << "; A0/A1 0x"
        << post_exec_tail_first_callee.a0_literal << "/0x"
        << post_exec_tail_first_callee.a1_literal << ", vector -0x"
        << static_cast<std::uint16_t>(-post_exec_tail_first_callee.graphics_library_vector)
        << " via cell 0x" << post_exec_tail_first_callee.graphics_library_base_address
        << " (ABI return unmodelled)" << std::dec << '\n';
    const auto post_exec_tail_second_callee =
        eon::parse_deuteros_amiga_title_post_exec_tail_second_callee_profile(disk, plan);
    std::cout << "          Conditional tail second callee: BSR 0x" << std::hex
        << post_exec_tail_second_callee.caller_address << " -> local 0x"
        << post_exec_tail_second_callee.entry_address << "; cells 0x"
        << post_exec_tail_second_callee.selection_cells[0] << "/0x"
        << post_exec_tail_second_callee.selection_cells[3] << ", vector -0x"
        << static_cast<std::uint16_t>(-post_exec_tail_second_callee.graphics_library_vector)
        << " via cell 0x" << post_exec_tail_second_callee.graphics_library_base_address
        << " (ABI return unmodelled)" << std::dec << '\n';
    const auto post_exec_tail_third_callee =
        eon::parse_deuteros_amiga_title_post_exec_tail_third_callee_profile(disk, plan);
    std::cout << "          Conditional tail third callee: BSR 0x" << std::hex
        << post_exec_tail_third_callee.caller_address << " -> re-entry 0x"
        << post_exec_tail_third_callee.entry_address << ", continuation 0x"
        << post_exec_tail_third_callee.caller_continuation_address << ", RTS 0x"
        << post_exec_tail_third_callee.routine_return_address
        << " (ABI return unmodelled)" << std::dec << '\n';
    const auto post_exec_tail_fourth_callee =
        eon::parse_deuteros_amiga_title_post_exec_tail_fourth_callee_profile(disk, plan);
    std::cout << "          Conditional tail fourth callee: BSR 0x" << std::hex
        << post_exec_tail_fourth_callee.caller_address << " -> local 0x"
        << post_exec_tail_fourth_callee.entry_address << "; A0/A1 0x"
        << post_exec_tail_fourth_callee.a0_literal << "/0x"
        << post_exec_tail_fourth_callee.a1_literal << ", vector -0x"
        << static_cast<std::uint16_t>(-post_exec_tail_fourth_callee.graphics_library_vector)
        << " via cell 0x" << post_exec_tail_fourth_callee.graphics_library_base_address
        << " (ABI return unmodelled)" << std::dec << '\n';
    const auto post_exec_tail_return =
        eon::parse_deuteros_amiga_title_post_exec_tail_return_profile(disk, plan);
    std::cout << "          Conditional tail return: 0x" << std::hex
        << post_exec_tail_return.continuation_address << " table 0x"
        << post_exec_tail_return.source_table_address << " -> cells 0x"
        << post_exec_tail_return.destination_addresses[0] << "/0x"
        << post_exec_tail_return.destination_addresses[1] << "; local 0x"
        << post_exec_tail_return.local_service_address << " vector -0x"
        << static_cast<std::uint16_t>(-post_exec_tail_return.exec_vector)
        << " via cell 0x" << post_exec_tail_return.exec_base_address
        << " (ABI return unmodelled)" << std::dec << '\n';
    const auto post_exec_tail_return_continuation =
        eon::parse_deuteros_amiga_title_post_exec_tail_return_continuation_profile(disk, plan);
    std::cout << "          Conditional tail return continuation: 0x" << std::hex
        << post_exec_tail_return_continuation.continuation_address << "; calls 0x"
        << post_exec_tail_return_continuation.direct_call_addresses[0] << "/0x"
        << post_exec_tail_return_continuation.direct_call_addresses[12]
        << ", indirect literal 0x"
        << post_exec_tail_return_continuation.indirect_call_pointer_literal
        << ", stop 0x" << post_exec_tail_return_continuation.stop_before_address
        << " (all ABI returns unmodelled)" << std::dec << '\n';
    const auto post_exec_tail_flag_gate =
        eon::parse_deuteros_amiga_title_post_exec_tail_flag_gate_profile(disk, plan);
    std::cout << "          Conditional tail flag gate: 0x" << std::hex
        << post_exec_tail_flag_gate.entry_address << " cells 0x"
        << post_exec_tail_flag_gate.source_word_addresses[0] << "/0x"
        << post_exec_tail_flag_gate.source_word_addresses[1] << "; jump 0x"
        << post_exec_tail_flag_gate.absolute_jump_target << ", calls 0x"
        << post_exec_tail_flag_gate.direct_call_targets[0] << "/0x"
        << post_exec_tail_flag_gate.direct_call_targets[2] << ", stop 0x"
        << post_exec_tail_flag_gate.stop_after_address
        << " (all results and writes unmodelled)" << std::dec << '\n';
    const auto post_exec_tail_flag_gate_first_callee =
        eon::parse_deuteros_amiga_title_post_exec_tail_flag_gate_first_callee_profile(disk, plan);
    std::cout << "          Flag-gate first callee: JSR 0x" << std::hex
        << post_exec_tail_flag_gate_first_callee.caller_address << " -> 0x"
        << post_exec_tail_flag_gate_first_callee.entry_address << "; byte 0x"
        << post_exec_tail_flag_gate_first_callee.tested_byte_address << ", word 0x"
        << post_exec_tail_flag_gate_first_callee.first_loop_word_address
        << ", RTS 0x" << post_exec_tail_flag_gate_first_callee.terminal_return_address
        << " (cell values and loop execution unmodelled)" << std::dec << '\n';
    const auto post_exec_tail_flag_gate_copy_callee =
        eon::parse_deuteros_amiga_title_post_exec_tail_flag_gate_copy_callee_profile(disk, plan);
    std::cout << "          Flag-gate copy callee: JSR 0x" << std::hex
        << post_exec_tail_flag_gate_copy_callee.caller_addresses[0] << "/0x"
        << post_exec_tail_flag_gate_copy_callee.caller_addresses[1] << " -> 0x"
        << post_exec_tail_flag_gate_copy_callee.entry_address << "; 0x"
        << post_exec_tail_flag_gate_copy_callee.transferred_byte_count << " byte 0x"
        << post_exec_tail_flag_gate_copy_callee.source_address << " -> 0x"
        << post_exec_tail_flag_gate_copy_callee.destination_address << ", D1 loop 0x"
        << static_cast<unsigned>(post_exec_tail_flag_gate_copy_callee.delay_loop_counter)
        << ", gate 0x"
        << post_exec_tail_flag_gate_copy_callee.gate_word_address << ", RTS 0x"
        << post_exec_tail_flag_gate_copy_callee.terminal_return_address
        << " (cell values, transfer, loop, and increment unmodelled)" << std::dec << '\n';
    std::cout << "          Timed title transition: 0x" << std::hex
        << title_stage.transition_source_palette_address << " -> 0x"
        << title_stage.transition_work_palette_address << ", " << std::dec
        << title_stage.transition_palette_word_count << " RGB4 words, mask 0x"
        << std::hex << title_stage.transition_palette_mask << std::dec << '\n';
    std::cout << "          Bootstrap profile table: entry 5 0x" << std::hex
        << title_stage.bootstrap_profile_five_address << " -> BSR 0x"
        << title_stage.bootstrap_profile_five_first_call_address << std::dec
        << " (return intentionally unmodelled)\n";
    std::cout << "          Profile 5 helper: controller cell 0x" << std::hex
        << title_stage.bootstrap_profile_five_helper_controller_cell << ", writes +0x"
        << title_stage.bootstrap_profile_five_helper_long_offset << "=0x"
        << title_stage.bootstrap_profile_five_helper_long_value << ", +0x"
        << title_stage.bootstrap_profile_five_helper_word_offset << "=0x"
        << title_stage.bootstrap_profile_five_helper_word_value << std::dec
        << ", +0x" << std::hex
        << title_stage.bootstrap_profile_five_helper_byte_offset << "=0x"
        << static_cast<unsigned>(title_stage.bootstrap_profile_five_helper_byte_value) << std::dec
        << ", then vector -0x1c8 (return unmodelled)\n";
    std::cout << "          Transition gate: counter 0x" << std::hex
        << title_stage.timer_counter_address << " >= 0x" << title_stage.timer_threshold
        << ", skip when word 0x" << title_stage.timer_dispatch_inhibit_address
        << " == 0x" << title_stage.timer_dispatch_inhibit_value
        << "; return clears 0x" << title_stage.timer_counter_reset_address << std::dec << '\n';
    std::cout << "          Transition return: compares 0x" << std::hex
        << title_stage.transition_first_compare_address << ", 0x"
        << title_stage.transition_second_compare_address << ", 0x"
        << title_stage.transition_third_compare_address << "; second phase 0x"
        << title_stage.transition_second_phase_source_address << " -> 0x"
        << title_stage.transition_second_phase_first_work_address << "/0x"
        << title_stage.transition_second_phase_second_work_address << ", slot 0x"
        << title_stage.transition_second_phase_work_pointer_address << ", rts 0x"
        << title_stage.transition_return_address << std::dec << '\n';
    std::cout << "          Post-transition control: word 0x" << std::hex
        << title_stage.post_transition_control_address << " reset to 0; helper chain 0x"
        << title_stage.post_transition_first_helper_address << " -> 0x"
        << title_stage.post_transition_second_helper_address << " -> 0x"
        << title_stage.post_transition_third_helper_address << " -> 0x"
        << title_stage.post_transition_response_helper_address << "; response 0x"
        << title_stage.post_transition_response_code << ", compares 0x"
        << title_stage.post_transition_first_compare_value << "/0x"
        << title_stage.post_transition_second_compare_value << "/0x"
        << title_stage.post_transition_third_compare_value << ", rts 0x"
        << title_stage.post_transition_return_address << std::dec << '\n';
    std::cout << "          Title-stage selector: 0x" << std::hex
        << title_stage.post_transition_selector_address << " masks D0 with 0x"
        << title_stage.post_transition_selector_input_mask << ", divides by 0x"
        << title_stage.post_transition_selector_first_divisor << "/0x"
        << title_stage.post_transition_selector_second_divisor << ", adds 0x"
        << title_stage.post_transition_selector_addend << ", clears 0x"
        << title_stage.post_transition_selector_flag_address << ", then jumps within title stage to 0x"
        << title_stage.post_transition_selector_dispatch_address << std::dec << '\n';
    std::cout << "          Selector dispatch: signed byte 0x" << std::hex
        << title_stage.post_transition_dispatch_state_address << " zero/positive branches 0x"
        << title_stage.post_transition_dispatch_zero_branch_address << "/0x"
        << title_stage.post_transition_dispatch_positive_branch_address
        << " (sibling byte-combine route); zero's second state 0x"
        << title_stage.post_transition_dispatch_zero_variant_state_address
        << " selects 0x" << title_stage.post_transition_dispatch_zero_clear_variant_address
        << "/0x" << title_stage.post_transition_dispatch_zero_set_variant_address
        << "; negative path calls 0x" << title_stage.post_transition_dispatch_negative_service_address
        << " with D0/D1=0x" << title_stage.post_transition_dispatch_negative_service_d0 << "/0x"
        << title_stage.post_transition_dispatch_negative_service_d1 << " unless D0=0x"
        << title_stage.post_transition_dispatch_negative_suppress_value << ", delay 0x"
        << title_stage.post_transition_dispatch_negative_delay << std::dec << '\n';
    std::cout << "          Title exits: 0x" << std::hex
        << title_stage.title_exit_first_address << "/0x"
        << title_stage.title_exit_second_address << "/0x"
        << title_stage.title_exit_third_address << " select bootstrap profiles " << std::dec
        << title_stage.title_exit_first_profile << "/" << title_stage.title_exit_second_profile
        << "/" << title_stage.title_exit_third_profile << "; bootstrap 0x" << std::hex
        << title_stage.title_exit_controller_address << " resolves all to profile " << std::dec
        << title_stage.title_exit_resolved_profile << " main entry 0x" << std::hex
        << title_stage.title_exit_main_stage_entry_address << std::dec << '\n';
}

void report_millennium_dos(const eon::ReleaseArchive& release) {
    if (release.language == "es") {
        // The Spanish release is a genuine FAT12 floppy rather than a loose
        // file archive. Read the verified image in memory; never unpack it
        // into a runtime directory.
        constexpr auto spanish_image_sha256 =
            "1cb7d399ab22110317b1c7486a575c00895f12a17268d0c984ac264a5695961d";
        const auto image = eon::extract_verified_release_asset(release, spanish_image_sha256);
        if (!image) throw std::runtime_error("Verified Spanish Millennium floppy missing");
        const eon::Fat12Disk disk(*image);
        const auto* title_entry = disk.find("TITLE.LIB");
        const auto* static_entry = disk.find("2200AD4.BIN");
        const auto* ibm_entry = disk.find("IBM.COM");
        const auto* manual_entry = disk.find("MILL.BAT");
        const auto* titles_entry = disk.find("TITLES.EXE");
        const auto* game_entry = disk.find("2200AD.EXE");
        if (!title_entry || !static_entry || !ibm_entry || !manual_entry || !titles_entry || !game_entry) {
            throw std::runtime_error("Verified Spanish Millennium media missing title data");
        }
        const eon::MillenniumDosLib title_lib(disk.read(*title_entry));
        const auto* p00 = title_lib.find("P00");
        if (!p00) throw std::runtime_error("Verified Spanish TITLE.LIB has no P00 entry");
        const auto resource = title_lib.read(*p00);
        const auto bitmap = eon::decode_millennium_dos_bitmap(resource);
        const auto palette = eon::decode_millennium_dos_palette(resource, bitmap);
        const auto game_data = eon::parse_millennium_dos_game_data(disk.read(*static_entry));
        const auto launch_manual = eon::parse_millennium_dos_spanish_launch_manual(
            disk.read(*manual_entry));
        std::cout << "          Spanish FAT12: " << disk.root_entries().size()
            << " root files; TITLE.LIB P00 " << bitmap.width << 'x' << bitmap.height
            << ", RGB6 DAC entries 256, logical translation "
            << palette.logical_to_dac.size() << '\n';
        std::cout << "          Spanish 2200AD4.BIN: " << game_data.celestial_labels.size()
            << " original celestial labels (" << game_data.celestial_labels[4].text << ")\n";
        std::cout << "          Spanish MILL.BAT: " << launch_manual.original_text.size()
            << " original launcher-documentation bytes (SHA-256 " << launch_manual.sha256 << ")\n";
        const auto ibm_handoff = eon::parse_millennium_dos_spanish_ibm_handoff_evidence(
            disk.read(*ibm_entry), disk.read(*titles_entry), disk.read(*game_entry));
        const auto game_startup = eon::parse_millennium_dos_spanish_game_startup_evidence(
            disk.read(*game_entry));
        const auto game_startup_callees = eon::parse_millennium_dos_spanish_game_startup_callees(
            disk.read(*game_entry), game_startup);
        const auto game_startup_followups = eon::parse_millennium_dos_spanish_game_startup_followups(
            disk.read(*game_entry), game_startup_callees);
        std::cout << "          Spanish IBM.COM handoff: caller 0x" << std::hex
            << ibm_handoff.caller_entry_address << " names 0x" << ibm_handoff.title_name_address
            << "/0x" << ibm_handoff.game_name_address << "; calls 0x"
            << ibm_handoff.first_call_address << "/0x" << ibm_handoff.second_call_address
            << " -> 0x" << ibm_handoff.callee_address << "; JNE 0x"
            << ibm_handoff.first_nonzero_branch_address << "/0x"
            << ibm_handoff.second_nonzero_branch_address << "; DOS EXEC AX=0x"
            << ibm_handoff.exec_ax << " INT 0x" << static_cast<unsigned>(ibm_handoff.exec_interrupt)
            << " ES:BX=CS:0x" << ibm_handoff.exec_parameter_block_address
            << "; carry branch 0x" << ibm_handoff.carry_branch_address << " -> 0x"
            << ibm_handoff.carry_branch_target_address << "; SHA-256 "
            << ibm_handoff.ibm_sha256 << std::dec
            << " (static only; no DOS call, result, title, or game ABI executed)\n";
        std::cout << "          Spanish 2200AD startup: COM entry -> 0x" << std::hex
            << game_startup.startup_entry_address << "; SS:SP=CS:0x"
            << game_startup.stack_pointer << "; AX=0x" << game_startup.private_function
            << " ES:BX=CS:0x" << game_startup.private_record_address << "; CALL 0x"
            << game_startup.private_call_address << " -> 0x" << game_startup.private_wrapper_address
            << "; AL==0x" << static_cast<unsigned>(game_startup.compared_al_value)
            << " calls 0x" << game_startup.equal_call_target_address << ", otherwise 0x"
            << game_startup.other_call_target_address << "; SHA-256 " << game_startup.startup_sha256
            << std::dec << " (static only; no private result, branch, or game state is supplied)\n";
        std::cout << "          Spanish startup callees: AL==0x1 path 0x" << std::hex
            << game_startup_callees.equal_entry_address << " -> private 0x"
            << game_startup_callees.equal_private_target_address << ", local 0x"
            << game_startup_callees.equal_followup_target_address << ", then stores 0x1 at 0x"
            << game_startup_callees.equal_result_storage_address << "; other path 0x"
            << game_startup_callees.other_entry_address << " -> private 0x"
            << game_startup_callees.other_private_target_address << ", local 0x"
            << game_startup_callees.other_followup_target_address << ", compares 0x"
            << game_startup_callees.other_compare_value << " at 0x"
            << game_startup_callees.other_result_source_address << "; SHA-256 "
            << game_startup_callees.equal_sha256 << "/" << game_startup_callees.other_sha256
            << std::dec << " (call returns and predicates remain unmodeled)\n";
        std::cout << "          Spanish startup follow-ups: 0x" << std::hex
            << game_startup_followups.equal_entry_address << " stores 0x"
            << static_cast<unsigned>(game_startup_followups.equal_literal_value) << " at 0x"
            << game_startup_followups.equal_storage_address << "; 0x"
            << game_startup_followups.palette_entry_address << " initializes CX=0x" << std::hex
            << game_startup_followups.palette_initial_cx << " before original AX=0x"
            << game_startup_followups.bios_ax << " INT 0x"
            << game_startup_followups.bios_interrupt << " palette table 0x"
            << game_startup_followups.palette_table_address << "; SHA-256 "
            << game_startup_followups.palette_sha256 << "/"
            << game_startup_followups.palette_table_sha256 << std::dec
            << " (static only; no BIOS call, branch, or presentation is executed)\n";
        std::cout << "          Spanish isolation boundary: only this image's IBM.COM, TITLES.EXE, "
            << "and 2200AD.EXE are reported; no English executable, state, or title path is "
            << "substituted. Next trace inputs: hash-locked DOS child return/AL, file operations, "
            << "and CPU/interrupt events for this Spanish image.\n";
        return;
    }
    constexpr auto title_lib_sha256 =
        "6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678";
    const auto title_bytes = eon::extract_verified_release_asset(release, title_lib_sha256);
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
    const auto gx_bytes = eon::extract_verified_release_asset(release, gx_lib_sha256);
    if (!gx_bytes) throw std::runtime_error("Verified Millennium GX.LIB missing");
    const auto gx_canvas = eon::parse_millennium_dos_gameplay_screen(*gx_bytes);
    std::cout << "          GX.LIB IMG00 -> IMG01: " << gx_canvas.canvas.width << 'x'
        << gx_canvas.canvas.height << " original indexed canvas\n";
    constexpr auto last_lib_sha256 =
        "a3f5c0b447795881dd4cd5316a091ecc218b1bf563f567b6fe3f093f89781510";
    const auto last_bytes = eon::extract_verified_release_asset(release, last_lib_sha256);
    if (!last_bytes) throw std::runtime_error("Verified Millennium LAST.LIB missing");
    const auto last_screen = eon::parse_millennium_dos_last_screen(*last_bytes);
    std::cout << "          LAST.LIB last: " << last_screen.bitmap.width << 'x'
        << last_screen.bitmap.height << " original indexed screen, RGB6 DAC entries 256\n";
    constexpr auto titles_sha256 =
        "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
    constexpr auto launcher_sha256 =
        "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e";
    const auto titles = eon::extract_verified_release_asset(release, titles_sha256);
    const auto launcher = eon::extract_verified_release_asset(release, launcher_sha256);
    if (!titles || !launcher) throw std::runtime_error("Verified Millennium title flow assets missing");
    const auto flow = eon::parse_millennium_dos_title_flow(*titles, *launcher);
    const auto sound_selection = eon::parse_millennium_dos_sound_selection(*launcher);
    const auto sound_blaster = eon::extract_verified_release_asset(release,
        "be5a00e0b71d893a3aeaaa1127b1e5b870fe734dc876e636c6a933b6444f1b72");
    const auto covox = eon::extract_verified_release_asset(release,
        "99e110b91534206a6b83680a3e11cceadd0e5ddf863560aed53dcbd2c49df7c4");
    if (!sound_blaster || !covox) {
        throw std::runtime_error("Verified Millennium sound-driver leaves missing");
    }
    const auto sound_blaster_leaf = eon::admit_millennium_dos_sound_driver_leaf(*sound_blaster);
    const auto covox_leaf = eon::admit_millennium_dos_sound_driver_leaf(*covox);
    const auto title_exit = eon::parse_millennium_dos_title_exit_closure(*titles);
    const auto transition = eon::parse_millennium_dos_title_transition(title_lib, flow);
    std::cout << "          TITLES.EXE: resource " << flow.title_resource_index
        << ", " << flow.intro_transition_steps << " transition steps, key poll INT 0x"
        << std::hex << static_cast<unsigned>(flow.input_interrupt) << "; selection JLE 0x"
        << flow.title_selection_callee_branch_address << " -> 0x"
        << flow.title_selection_callee_branch_target << ", target CALL 0x"
        << flow.title_selection_callee_jle_target_call_address << " -> 0x"
        << flow.title_selection_callee_jle_target_call_target << std::dec
        << "; launcher hand-off " << flow.launcher_title_program << " -> "
        << flow.launcher_game_program << " (DX 0x" << std::hex
        << flow.launcher_title_program_address << " / 0x"
        << flow.launcher_game_program_address << ", near-call target 0x"
        << flow.launcher_common_call_target << ", JC 0x"
        << flow.launcher_common_branch_address << " -> 0x"
        << flow.launcher_common_branch_target << " (static boundary 0x"
        << flow.launcher_common_branch_target_static_boundary << "); pre-title JE 0x"
        << flow.launcher_pre_title_gate_address << " -> 0x"
        << flow.launcher_pre_title_gate_target << ", near-call 0x"
        << flow.launcher_pre_title_call_address << " -> 0x"
        << flow.launcher_pre_title_call_target << " (JNC 0x"
        << flow.launcher_pre_title_callee_branch_address << " -> 0x"
        << flow.launcher_pre_title_callee_branch_target << ", fallthrough JMP 0x"
        << flow.launcher_pre_title_callee_fallthrough_jump_address << " -> 0x"
        << flow.launcher_pre_title_callee_fallthrough_jump_target << "; target JC 0x"
        << flow.launcher_pre_title_callee_jnc_target_branch_address << " -> 0x"
        << flow.launcher_pre_title_callee_jnc_target_branch_target << "; JC target JMP 0x"
        << flow.launcher_pre_title_callee_jc_target_jump_address << " -> 0x"
        << flow.launcher_pre_title_callee_jc_target_jump_target << "; join JE 0x"
        << flow.launcher_pre_title_callee_join_branch_address << " -> 0x"
        << flow.launcher_pre_title_callee_join_branch_target << " (terminal opcode 0x"
        << flow.launcher_pre_title_callee_join_branch_terminal_address << "); private INT 0x"
        << static_cast<unsigned>(flow.launcher_private_interrupt_number) << " loader 0x"
        << flow.launcher_private_interrupt_loader_call_address << " -> 0x"
        << flow.launcher_private_interrupt_loader_call_target << ", setup 0x"
        << flow.launcher_private_interrupt_install_address << " offset 0x"
        << flow.launcher_private_interrupt_handler_offset << std::dec
        << " (loader reads " << flow.launcher_private_interrupt_handler_first_program
        << "/" << flow.launcher_private_interrupt_handler_other_program << " to DS:0x"
        << std::hex << flow.launcher_private_interrupt_handler_destination_offset
        << "; command-tail e/E/m/M scan 0x" << flow.launcher_video_selection_scan_address
        << " maps selectors 1/2, empty tail calls 0x"
        << flow.launcher_video_selection_default_detector_address
        << "; numeric segment unmodelled; raw save 0x"
        << flow.launcher_private_interrupt_saved_offset_cell << "/0x"
        << flow.launcher_private_interrupt_saved_segment_cell << ", restore 0x"
        << flow.launcher_private_interrupt_restore_address << std::dec << "))\n";
    std::cout << "          MILL.COM sound selection: routine 0x" << std::hex
        << sound_selection.selector_entry_address << " accepts 0/1/2 -> table slots "
        << static_cast<unsigned>(sound_selection.ibm_speaker_table_slot) << "/"
        << static_cast<unsigned>(sound_selection.sound_blaster_table_slot) << "/"
        << static_cast<unsigned>(sound_selection.covox_table_slot) << " ("
        << sound_selection.ibm_speaker_filename << "/" << sound_blaster_leaf.original_filename
        << "/" << covox_leaf.original_filename << "); admitted original leaves "
        << std::dec << sound_blaster_leaf.byte_size << "/" << covox_leaf.byte_size
        << " bytes; table slot " << static_cast<unsigned>(sound_selection.missing_srol_table_slot) << " "
        << sound_selection.missing_srol_filename
        << " is absent (no fallback or sound-driver execution)\n" << std::dec;
    std::cout << "          TITLES.EXE local exit: 0x" << std::hex
        << title_exit.nonzero_entry_address << " calls 0x"
        << title_exit.private_driver_target_address << "/0x"
        << title_exit.post_driver_target_address << ", clears 0x"
        << title_exit.status_storage_address << ", then jumps to 0x"
        << title_exit.exit_stub_address << " (pre-exit CALL 0x"
        << title_exit.exit_stub_preceding_call_target_address << ", INT 0x"
        << static_cast<unsigned>(title_exit.exit_interrupt) << " AH=0x"
        << static_cast<unsigned>(title_exit.exit_service)
        << "; static boundary only)\n" << std::dec;
    std::cout << "          TITLE.LIB P01-P25: " << transition.patches.size()
        << " decoded " << transition.patches.front().bitmap.width << 'x'
        << transition.patches.front().bitmap.height
        << " patches (static order only; no timing, composition, or frame claimed)\n";
    const auto ega640 = eon::extract_verified_release_asset(release,
        "ba003dd155fee868980f6ece933c33f9b22af68ed376cd64f4e027abd65baf6a");
    const auto mcga = eon::extract_verified_release_asset(release,
        "bb5106d7412a9f139b74ffdcacfc4f8dcdf25595aa90565eaec114a4301fb228");
    if (!ega640 || !mcga) throw std::runtime_error("Verified Millennium video driver missing");
    const auto ega_driver = eon::parse_millennium_dos_video_driver(*ega640,
        eon::MillenniumDosVideoDriverKind::ega640);
    const auto mcga_driver = eon::parse_millennium_dos_video_driver(*mcga,
        eon::MillenniumDosVideoDriverKind::mcga);
    std::cout << "          private INT 91 video ABI: EGA640 AX=0 -> BIOS mode 0x"
        << std::hex << static_cast<unsigned>(ega_driver.function_zero_video_mode)
        << " at 0x" << ega_driver.function_zero_set_mode_interrupt_site
        << "; MCGA AX=0 -> BIOS mode 0x"
        << static_cast<unsigned>(mcga_driver.function_zero_video_mode) << " at 0x"
        << mcga_driver.function_zero_set_mode_interrupt_site
        << "; AX=4 masks ES:BX[0] with 0x"
        << static_cast<unsigned>(ega_driver.function_four_input_mask)
        << "; AX=0x1f returns AH=0x"
        << static_cast<unsigned>(ega_driver.function_thirty_one_return_ah)
        << "/0x" << static_cast<unsigned>(mcga_driver.function_thirty_one_return_ah)
        << "; AX=0x13 polls VGA status 0x" << ega_driver.function_thirteen_status_port
        << " mask 0x" << static_cast<unsigned>(ega_driver.function_thirteen_retrace_mask)
        << " (read-only retrace wait, no host I/O)"
        << " with driver-local AL at 0x" << ega_driver.function_thirty_one_state_address
        << "/0x" << mcga_driver.function_thirty_one_state_address
        << " (validated ABI only; no BIOS/driver execution)\n" << std::dec;
    constexpr auto static_data_sha256 =
        "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d";
    const auto static_data = eon::extract_verified_release_asset(release, static_data_sha256);
    if (!static_data) throw std::runtime_error("Verified Millennium static game data missing");
    const auto game_data = eon::parse_millennium_dos_game_data(*static_data);
    const auto text_catalog = eon::parse_millennium_dos_static_text_catalog(*static_data);
    std::cout << "          2200AD4.BIN: " << game_data.celestial_labels.size()
        << " original celestial labels (" << game_data.celestial_labels[4].text << ")\n";
    std::cout << "          2200AD4.BIN static text: " << text_catalog.pointers.size()
        << " original pointers to " << text_catalog.records.size()
        << " raw records (read-only)\n";
    constexpr auto game_sha256 =
        "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57";
    const auto game = eon::extract_verified_release_asset(release, game_sha256);
    if (!game) throw std::runtime_error("Verified Millennium DOS executable missing");
    constexpr auto gx_overlay_sha256 =
        "093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb";
    const auto gx_overlay = eon::extract_verified_release_asset(release, gx_overlay_sha256);
    if (!gx_overlay) throw std::runtime_error("Verified Millennium DOS GX overlay missing");
    const auto game_flow = eon::parse_millennium_dos_game_flow(*game);
    const auto sound_effect_names = eon::parse_millennium_dos_sound_effect_name_table_evidence(*game);
    std::cout << "          2200AD.EXE SFX name bank: " << sound_effect_names.filenames.size()
        << " original VOC names at 0x" << std::hex << sound_effect_names.table_address
        << " (SHA-256 " << sound_effect_names.table_sha256
        << "; static only, no event mapping or playback)\n" << std::dec;
    // The executable names an exact original VOC family, but no recovered
    // caller selects an entry or invokes a sound-driver ABI. Decode it here
    // only as a hash-verified inspection diagnostic: SDL must not play a
    // voice merely because its bytes are available.
    const auto inventory = eon::inventory_verified_release(release);
    std::size_t decoded_voice_count = 0;
    std::size_t decoded_pcm_sample_count = 0;
    std::array<std::uint32_t, 2> voice_sample_rates{};
    std::size_t voice_sample_rate_count = 0;
    for (const auto& filename : sound_effect_names.filenames) {
        const auto asset = std::find_if(inventory.begin(), inventory.end(),
            [filename](const eon::ArchiveAsset& candidate) {
                const auto separator = candidate.path.find_last_of('/');
                const auto leaf = separator == std::string::npos
                    ? std::string_view(candidate.path)
                    : std::string_view(candidate.path).substr(separator + 1U);
                return candidate.kind == eon::AssetKind::audio && leaf == filename;
            });
        if (asset == inventory.end()) {
            throw std::runtime_error("Verified Millennium DOS VOC named by executable is missing");
        }
        const auto bytes = eon::extract_verified_release_asset(release, asset->sha256);
        if (!bytes) throw std::runtime_error("Verified Millennium DOS VOC cannot be read");
        const auto voice = eon::decode_creative_voice(*bytes);
        if (voice.unsigned_pcm.size() > std::numeric_limits<std::size_t>::max() - decoded_pcm_sample_count) {
            throw std::runtime_error("Millennium DOS VOC diagnostic sample count overflows");
        }
        decoded_pcm_sample_count += voice.unsigned_pcm.size();
        if (std::find(voice_sample_rates.begin(), voice_sample_rates.begin() + voice_sample_rate_count,
                voice.sample_rate) == voice_sample_rates.begin() + voice_sample_rate_count) {
            if (voice_sample_rate_count == voice_sample_rates.size()) {
                throw std::runtime_error("Unsupported Millennium DOS VOC sample-rate family");
            }
            voice_sample_rates[voice_sample_rate_count++] = voice.sample_rate;
        }
        ++decoded_voice_count;
    }
    std::cout << "          Original VOC bank: " << decoded_voice_count << " hash-verified voices, "
        << decoded_pcm_sample_count << " unsigned PCM samples at ";
    for (std::size_t index = 0; index < voice_sample_rate_count; ++index) {
        if (index != 0) std::cout << '/';
        std::cout << voice_sample_rates[index] << " Hz";
    }
    std::cout << " (inspection only; no event mapping, driver ABI, or playback)\n";
    const auto startup_allocation = eon::parse_millennium_dos_startup_allocation_boundary(*game);
    const auto startup_zero_path = eon::parse_millennium_dos_startup_zero_path_boundary(*game);
    const auto startup_zero_continuation =
        eon::parse_millennium_dos_startup_zero_continuation_boundary(*game);
    const auto startup_post_allocation =
        eon::parse_millennium_dos_startup_post_allocation_boundary(*game);
    const auto startup_post_release =
        eon::parse_millennium_dos_startup_post_release_continuation(*game);
    const auto startup_post_gx_loader =
        eon::parse_millennium_dos_startup_post_gx_loader_boundary(*game);
    const auto private_int91 = eon::parse_millennium_dos_private_int91_wrapper(*game);
    const auto post_int91_selector = eon::parse_millennium_dos_post_int91_caller_selector(*game);
    const auto post_overlay_adapter =
        eon::parse_millennium_dos_post_overlay_adapter_continuation(*game);
    const auto post_overlay_loop =
        eon::parse_millennium_dos_post_overlay_adapter_loop(*game);
    const auto post_overlay_dispatch =
        eon::parse_millennium_dos_post_overlay_dispatch_prefix(*game);
    const auto startup_nonzero_path = eon::parse_millennium_dos_startup_nonzero_path_boundary(*game);
    const auto gx_overlay_load = eon::parse_millennium_dos_gx_overlay_load_evidence(
        *game, *gx_overlay);
    const auto gx_overlay_adapter = eon::parse_millennium_dos_gx_overlay_adapter_evidence(
        *game, gx_overlay_load);
    const auto gx_overlay_dispatcher = eon::parse_millennium_dos_gx_overlay_dispatcher_evidence(
        *gx_overlay, gx_overlay_adapter);
    const auto gx_overlay_selector = eon::parse_millennium_dos_gx_overlay_selector_evidence(
        *game, *gx_overlay, gx_overlay_adapter, gx_overlay_dispatcher);
    const auto gx_overlay_startup_records =
        eon::parse_millennium_dos_gx_overlay_startup_record_evidence(
            *gx_overlay, gx_overlay_selector);
    const auto gx_overlay_dispatch13 = eon::parse_millennium_dos_gx_overlay_dispatch13_evidence(
        *gx_overlay, gx_overlay_dispatcher);
    std::cout << "          2200AD.EXE startup: entry 0x" << std::hex
        << game_flow.entry_address << ", SS=CS, SP=0x" << game_flow.startup_stack_pointer
        << ", first CALL 0x" << game_flow.startup_first_call_address
        << " -> INT 0x" << static_cast<unsigned>(game_flow.startup_first_call_interrupt)
        << ", static RET 0x" << game_flow.startup_first_call_return_address
        << " (return-site 0x" << game_flow.startup_first_call_return_site
        << ", AX -> 0x" << game_flow.startup_result_word_address
        << ", AH -> 0x" << game_flow.startup_result_high_byte_first_address << "/0x"
        << game_flow.startup_result_high_byte_second_address << ", SP -> 0x"
        << game_flow.startup_stack_snapshot_address << ")"
        << "; AL==$" << static_cast<unsigned>(game_flow.startup_mode_equal_value)
        << " -> 0x" << game_flow.startup_equal_call_address << ", otherwise 0x"
        << game_flow.startup_other_call_address << " (both re-enter INT 0x"
        << static_cast<unsigned>(game_flow.startup_first_call_interrupt) << " wrapper at 0x"
        << game_flow.startup_equal_path_private_call_site << "/0x"
        << game_flow.startup_other_path_private_call_site << "; equal follow-up writes $"
        << static_cast<unsigned>(game_flow.startup_equal_followup_write_value) << " -> 0x"
        << game_flow.startup_equal_followup_write_address << ", other first reaches INT 0x"
        << static_cast<unsigned>(game_flow.startup_other_followup_interrupt_number) << " at 0x"
        << game_flow.startup_other_followup_interrupt_site << " (AH=0x"
        << static_cast<unsigned>(game_flow.startup_other_followup_video_function) << ", AL=0x"
        << static_cast<unsigned>(game_flow.startup_other_followup_video_subfunction) << "); DX!=0 -> 0x"
        << game_flow.startup_nonzero_dx_branch_address << std::dec
        << " (validated boundary only; no native calls executed)\n";
    std::cout << "          2200AD startup continuation: 0x" << std::hex
        << startup_allocation.continuation_entry_address << " CALL 0x"
        << startup_allocation.allocator_entry_address << " reaches INT 0x"
        << static_cast<unsigned>(startup_allocation.allocator_first_external_interrupt)
        << " at 0x" << startup_allocation.allocator_first_external_interrupt_site
        << "; post-call DX==0 -> 0x" << startup_allocation.dx_zero_branch_target
        << ", DX!=0 -> 0x" << startup_allocation.dx_nonzero_jump_target << std::dec
        << " (static boundary only; no DOS result or branch chosen)\n";
    std::cout << "          2200AD DX-zero successor: 0x" << std::hex
        << startup_zero_path.zero_path_entry_address << " -> selector 0x"
        << startup_zero_path.selector_entry_address << " reads 0x"
        << startup_zero_path.selector_mode_byte_address << "; local CALL 0x"
        << startup_zero_path.selector_call_address << " -> 0x"
        << startup_zero_path.selector_call_target << " replaces DX with name 0x"
        << startup_zero_path.security_name_address << " before INT 0x"
        << static_cast<unsigned>(startup_zero_path.first_external_interrupt) << " at 0x"
        << startup_zero_path.first_external_interrupt_site << " (AH=0x"
        << static_cast<unsigned>(startup_zero_path.first_external_service) << ", AL=0x"
        << static_cast<unsigned>(startup_zero_path.first_external_access_mode) << std::dec
        << "; static boundary only, no mode or DOS result supplied)\n";
    std::cout << "          2200AD DX-nonzero successor: 0x" << std::hex
        << startup_nonzero_path.nonzero_entry_address << " loads AL=0x"
        << static_cast<unsigned>(startup_nonzero_path.immediate_al_value) << " then jumps to 0x"
        << startup_nonzero_path.continuation_entry_address << "; local CALL 0x"
        << startup_nonzero_path.continuation_first_call_address << " -> 0x"
        << startup_nonzero_path.continuation_first_call_target << " reaches INT 0x"
        << static_cast<unsigned>(startup_nonzero_path.first_external_interrupt) << " at 0x"
        << startup_nonzero_path.first_external_interrupt_site << " (AH=0x"
        << static_cast<unsigned>(startup_nonzero_path.first_external_service) << std::dec
        << "; static boundary only, no mouse state or return supplied)\n";
    std::cout << "          2200AD DX-zero return continuation: 0x" << std::hex
        << startup_zero_continuation.continuation_entry_address << " reads CS:0x"
        << startup_zero_continuation.source_byte_address << " then calls 0x"
        << startup_zero_continuation.first_local_call_target << "; reaches INT 0x"
        << static_cast<unsigned>(startup_zero_continuation.first_external_interrupt) << " at 0x"
        << startup_zero_continuation.first_external_interrupt_site << " (AH=0x"
        << static_cast<unsigned>(startup_zero_continuation.first_external_service)
        << ", BX=0x" << startup_zero_continuation.allocation_request_paragraphs << std::dec
        << "; conditional static boundary only, no DOS result or allocation supplied)\n";
    std::cout << "          2200AD post-allocation successor: 0x" << std::hex
        << startup_post_allocation.entry_address << " stores BX -> CS:0x"
        << startup_post_allocation.cs_override_store_target_address << ", moves AX -> ES at 0x"
        << startup_post_allocation.es_from_ax_address << ", then reaches INT 0x"
        << static_cast<unsigned>(startup_post_allocation.first_external_interrupt) << " at 0x"
        << startup_post_allocation.first_external_interrupt_site << " (AH=0x"
        << static_cast<unsigned>(startup_post_allocation.first_external_service) << std::dec
        << "; conditional static boundary only, no return or segment meaning supplied)\n";
    std::cout << "          2200AD post-release continuation: 0x" << std::hex
        << startup_post_release.entry_address << " restores DX, loads far cell 0x"
        << startup_post_release.far_pointer_address << " into DX/SI, then calls 0x"
        << startup_post_release.first_call_target << ", static-data loader 0x"
        << startup_post_release.static_data_call_target << ", and GX loader 0x"
        << startup_post_release.gx_loader_call_target << std::dec
        << " (conditional static boundary only; no DOS result, pointer, or calls supplied)\n";
    std::cout << "          2200AD post-GX-loader boundary: 0x" << std::hex
        << startup_post_gx_loader.entry_address << " restores ES from CS, loads BX=0x"
        << startup_post_gx_loader.bx_literal << "/AX=0x" << startup_post_gx_loader.ax_literal
        << ", then calls private wrapper 0x" << startup_post_gx_loader.private_call_target
        << " (INT 0x" << static_cast<unsigned>(startup_post_gx_loader.private_interrupt)
        << std::dec << "; conditional static boundary only, no calls or result supplied)\n";
    std::cout << "          2200AD private wrapper: 0x" << std::hex
        << private_int91.entry_address << " is called from 0x"
        << private_int91.caller_call_address << ", preserves raw stack span through INT 0x"
        << static_cast<unsigned>(private_int91.private_interrupt) << " at 0x"
        << private_int91.private_interrupt_site << ", then has RET at 0x"
        << private_int91.return_address << std::dec
        << " (static wrapper only; no private-interrupt ABI, return, or result supplied)\n";
    std::cout << "          2200AD post-wrapper caller selector: return site 0x" << std::hex
        << post_int91_selector.return_site_address << " compares CS:0x"
        << post_int91_selector.source_byte_address << " against 0x"
        << static_cast<unsigned>(post_int91_selector.first_compare_value) << "/0x"
        << static_cast<unsigned>(post_int91_selector.second_compare_value) << "/0x"
        << static_cast<unsigned>(post_int91_selector.third_compare_value)
        << ", writes DX to CS:0x" << post_int91_selector.shared_store_target_address
        << ", then CALLs 0x" << post_int91_selector.first_call_target << std::dec
        << " (conditional static prefix only; no wrapper return, state, or callee effect supplied)\n";
    std::cout << "          2200AD post-overlay-adapter continuation: return site 0x" << std::hex
        << post_overlay_adapter.return_site_address << " has CALL targets 0x"
        << post_overlay_adapter.initial_call_targets[0] << "/0x"
        << post_overlay_adapter.initial_call_targets[1] << "/0x"
        << post_overlay_adapter.initial_call_targets[2] << "/0x"
        << post_overlay_adapter.initial_call_targets[3] << "/0x"
        << post_overlay_adapter.initial_call_targets[4] << "/0x"
        << post_overlay_adapter.initial_call_targets[5] << "; CS:0x"
        << post_overlay_adapter.mode_byte_address << " == 0x"
        << static_cast<unsigned>(post_overlay_adapter.mode_equal_value) << " branches to 0x"
        << post_overlay_adapter.equal_branch_target << ", otherwise CALLs 0x"
        << post_overlay_adapter.other_call_target << ", converging at 0x"
        << post_overlay_adapter.convergence_address << std::dec
        << " (conditional static prefix only; no call return, state, or target effect supplied)\n";
    std::cout << "          2200AD post-overlay-adapter loop: 0x" << std::hex
        << post_overlay_loop.entry_address << " has " << std::dec
        << post_overlay_loop.call_addresses.size() << " hash-bound CALLs, tests AL at 0x"
        << std::hex << post_overlay_loop.first_al_test_address << "/0x"
        << post_overlay_loop.loop_al_test_address << ", and branches to 0x"
        << post_overlay_loop.loop_zero_branch_target << " before dispatcher 0x"
        << post_overlay_loop.following_dispatch_address << std::dec
        << " (conditional static span only; no calls, byte values, branches, or target effects supplied)\n";
    std::cout << "          2200AD post-overlay dispatch: action 0x" << std::hex
        << static_cast<unsigned>(post_overlay_dispatch.first_action_value) << " -> 0x"
        << post_overlay_dispatch.first_action_call_target << ", action 0x"
        << static_cast<unsigned>(post_overlay_dispatch.second_action_value) << " -> 0x"
        << post_overlay_dispatch.second_action_call_target << "; guard 0x"
        << post_overlay_dispatch.guard_byte_address << " and table 0x"
        << post_overlay_dispatch.table_base_address << " remain native; scaled CALL 0x"
        << post_overlay_dispatch.scaled_call_target << std::dec
        << " (validated dispatcher only; no input or call effects)\n";
    std::cout << "          2200GX.EXE overlay evidence: name 0x" << std::hex
        << gx_overlay_load.source_name_address << ", loader 0x"
        << gx_overlay_load.loader_entry_address << " reads segment cell 0x"
        << gx_overlay_load.loader_segment_cell_address << "; calls 0x"
        << gx_overlay_load.first_call_address << " -> 0x" << gx_overlay_load.first_call_target
        << "/0x" << gx_overlay_load.second_call_address << " -> 0x"
        << gx_overlay_load.second_call_target << "/0x" << gx_overlay_load.third_call_address
        << " -> 0x" << gx_overlay_load.third_call_target << "; caller 0x"
        << gx_overlay_load.caller_call_address << " -> 0x" << gx_overlay_load.caller_target
        << "; adapter 0x" << gx_overlay_adapter.entry_address << " RETF 0x"
        << gx_overlay_adapter.far_transfer_address << " to overlay offset 0x"
        << gx_overlay_adapter.overlay_entry_offset << "; SHA-256 "
        << gx_overlay_load.overlay_sha256 << std::dec
        << " (static only; no segment, calls, overlay code, or display executed)\n";
    std::cout << "          2200GX.EXE dispatcher: entry +0x" << std::hex
        << gx_overlay_dispatcher.entry_offset << ", table +0x"
        << gx_overlay_dispatcher.table_offset << "; selector 0xe/0xf/0x12/0x14 -> 0x"
        << gx_overlay_dispatcher.observed_selector_targets[0x0e] << "/0x"
        << gx_overlay_dispatcher.observed_selector_targets[0x0f] << "/0x"
        << gx_overlay_dispatcher.observed_selector_targets[0x12] << "/0x"
        << gx_overlay_dispatcher.observed_selector_targets[0x14] << "; near RET then far RETF +0x"
        << gx_overlay_dispatcher.far_return_offset << "; SHA-256 "
        << gx_overlay_dispatcher.dispatch_sha256 << std::dec
        << " (static only; no selector, handler, return, or display executed)\n";
    std::cout << "          2200GX selector prefix: 2200AD 0x" << std::hex
        << gx_overlay_selector.caller_entry_address << " reads 0x"
        << gx_overlay_selector.selector_source_address << "; 0x3/0x4/0x2/default -> AX 0x"
        << gx_overlay_selector.overlay_targets[0] << "/0x"
        << gx_overlay_selector.overlay_targets[1] << "/0x"
        << gx_overlay_selector.overlay_targets[2] << "/0x"
        << gx_overlay_selector.overlay_targets[3] << ", stores DX at 0x"
        << gx_overlay_selector.dx_storage_address << ", CALL 0x"
        << gx_overlay_selector.adapter_call_address << " -> 0x"
        << gx_overlay_selector.adapter_target << "; SHA-256 "
        << gx_overlay_selector.caller_sha256 << std::dec
        << " (static only; no selector value, records, returns, resources, or display executed)\n";
    std::cout << "          2200GX startup records: entries +0x" << std::hex
        << gx_overlay_startup_records.entry_offsets[0] << "/+0x"
        << gx_overlay_startup_records.entry_offsets[1] << "/+0x"
        << gx_overlay_startup_records.entry_offsets[2] << "/+0x"
        << gx_overlay_startup_records.entry_offsets[3] << " select original records +0x"
        << gx_overlay_startup_records.source_record_offsets[0] << "/+0x"
        << gx_overlay_startup_records.source_record_offsets[1] << "/+0x"
        << gx_overlay_startup_records.source_record_offsets[2] << "/+0x"
        << gx_overlay_startup_records.source_record_offsets[3] << "; shared +0x"
        << gx_overlay_startup_records.shared_copy_entry_offset << " copies " << std::dec
        << static_cast<unsigned>(gx_overlay_startup_records.copy_word_count)
        << " words to +0x" << std::hex << gx_overlay_startup_records.copy_destination_offset
        << "; SHA-256 " << gx_overlay_startup_records.entry_span_sha256 << std::dec
        << " (raw startup provenance only; no record meaning, state, or display inferred)\n";
    std::cout << "          2200GX dispatcher slot 13: entry +0x" << std::hex
        << gx_overlay_dispatch13.entry_offset << " resets words +0x"
        << gx_overlay_dispatch13.zeroed_word_storage_offsets[0] << "/+0x"
        << gx_overlay_dispatch13.zeroed_word_storage_offsets[1] << "/+0x"
        << gx_overlay_dispatch13.zeroed_word_storage_offsets[2] << ", compares AL with 0x"
        << static_cast<unsigned>(gx_overlay_dispatch13.first_result_compare_value)
        << ", and returns/loops via +0x" << gx_overlay_dispatch13.local_back_edge_target_offset
        << "; SHA-256 " << gx_overlay_dispatch13.span_sha256 << std::dec
        << " (conditional static span only; no selector, calls, results, or state supplied)\n";
    constexpr auto initial_save_sha256 =
        "a9b3d77534d3d575012f9553bfed9520edf92a83af408c977e7f0fd226a470e7";
    const auto initial_save = eon::extract_verified_release_asset(release, initial_save_sha256);
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
    std::size_t shared_resident_images = 0;
    std::optional<eon::MillenniumAmigaSharedResidentLayout> shared_resident;
    for (const auto& asset : eon::inventory_verified_release(release)) {
        if (asset.kind != eon::AssetKind::amiga_adf) continue;
        const auto image = eon::extract_verified_release_asset(release, asset.sha256);
        if (!image) throw std::runtime_error("Verified Millennium Amiga ADF is missing");
        const auto layout = eon::parse_millennium_amiga_shared_resident_layout(*image);
        if (shared_resident && (layout.disk_offset != shared_resident->disk_offset
            || layout.length != shared_resident->length
            || layout.destination != shared_resident->destination
            || layout.raw_sha256 != shared_resident->raw_sha256)) {
            throw std::runtime_error("Millennium Amiga shared resident layout differs between variants");
        }
        shared_resident = layout;
        ++shared_resident_images;
    }
    if (shared_resident) {
        std::cout << "          shared resident evidence: " << shared_resident_images
            << " original images, disk 0x" << std::hex << shared_resident->disk_offset
            << " + 0x" << shared_resident->length << " -> memory 0x"
            << shared_resident->destination << std::dec << "; SHA-256 "
            << shared_resident->raw_sha256 << '\n';
    }
    // Defjam's image contains the recovered raw-sector loader. Other supplied
    // crack variants preserve the shared media ranges but patch this stage.
    constexpr auto loader_adf_sha256 =
        "8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c";
    const auto image = eon::extract_verified_release_asset(release, loader_adf_sha256);
    if (!image) return;
    const eon::MillenniumAmigaBootstrapSession live_bootstrap(*image);
    const eon::AmigaAdf disk(*image);
    const auto& plan = live_bootstrap.plan();
    const auto& opaque_invocation = live_bootstrap.opaque_invocation_boundary();
    const auto& first_stage_source_anchors = live_bootstrap.first_stage_source_anchors();
    std::cout << "          bounded launcher bootstrap: resident entry 0x" << std::hex
        << live_bootstrap.resident_entry().entry_address << ", raw resident SHA-256 "
        << live_bootstrap.shared_resident().raw_sha256 << std::dec
        << " (opaque handoff validated; no raw-stage invocation)\n";
    const auto resident = eon::parse_millennium_amiga_resident_entry(disk, plan);
    const auto splitter = eon::parse_millennium_amiga_resident_word_splitter(disk, plan);
    const auto helper_boundary = eon::parse_millennium_amiga_resident_helper_raw_boundary(
        disk, plan, splitter);
    const auto setup_helper_boundary =
        eon::parse_millennium_amiga_resident_setup_helper_raw_boundary(disk, plan);
    const auto staging_callsites = eon::parse_millennium_amiga_resident_helper_staging_callsites(
        disk, plan, splitter);
    const auto first_post_helper_chain =
        eon::parse_millennium_amiga_resident_first_post_helper_static_chain(
            disk, plan, staging_callsites.front());
    const auto second_post_helper_chain =
        eon::parse_millennium_amiga_resident_second_post_helper_static_chain(
            disk, plan, staging_callsites.back());
    const auto staging_reachability =
        eon::parse_millennium_amiga_resident_staging_direct_reachability_boundary(
            disk, plan, staging_callsites);
    const auto separate_entry = eon::parse_millennium_amiga_resident_separate_entry_gate(disk, plan);
    const auto separate_branch = eon::parse_millennium_amiga_resident_separate_branch_boundary(
        disk, plan, separate_entry);
    const auto separate_post_call = eon::parse_millennium_amiga_resident_separate_post_call_boundary(
        disk, plan, separate_branch);
    const auto separate_post_call_tail =
        eon::parse_millennium_amiga_resident_separate_post_call_tail_boundary(
            disk, plan, separate_post_call);
    const auto separate_post_call_tail_branch =
        eon::parse_millennium_amiga_resident_separate_post_call_tail_branch_boundary(
            disk, plan, separate_post_call_tail);
    const auto separate_comparison =
        eon::parse_millennium_amiga_resident_separate_comparison_boundary(
            disk, plan, separate_post_call_tail_branch);
    const auto separate_byte_gate =
        eon::parse_millennium_amiga_resident_separate_byte_gate_boundary(
            disk, plan, separate_comparison);
    const auto separate_byte_gate_target =
        eon::parse_millennium_amiga_resident_separate_byte_gate_target_boundary(
            disk, plan, separate_byte_gate);
    const auto separate_byte_gate_convergence =
        eon::parse_millennium_amiga_resident_separate_byte_gate_convergence_boundary(
            disk, plan, separate_byte_gate_target);
    const auto separate_byte_gate_taken_branch =
        eon::parse_millennium_amiga_resident_separate_byte_gate_taken_branch_boundary(
            disk, plan, separate_byte_gate_convergence);
    const auto separate_byte_gate_fallthrough =
        eon::parse_millennium_amiga_resident_separate_byte_gate_fallthrough_boundary(
            disk, plan, separate_byte_gate_convergence);
    const auto independent_entry =
        eon::parse_millennium_amiga_resident_independent_entry_gate(disk, plan);
    const auto negative_d3 = eon::parse_millennium_amiga_resident_negative_d3_continuation(
        disk, plan, independent_entry);
    const auto negative_d3_terminal = eon::parse_millennium_amiga_resident_negative_d3_terminal(
        disk, plan, negative_d3);
    const auto post_negative_d3 = eon::parse_millennium_amiga_resident_post_negative_d3_terminal(
        disk, plan, negative_d3_terminal);
    const auto post_negative_d3_continuation =
        eon::parse_millennium_amiga_resident_post_negative_d3_continuation_boundary(
            disk, plan, post_negative_d3);
    std::cout << "          raw loader: disk 0x" << std::hex
        << plan.first_stage.disk_offset << " + 0x" << plan.first_stage.length
        << " -> memory 0x" << plan.first_stage.destination
        << "; disk 0x" << plan.resident_stage.disk_offset << " + 0x"
        << plan.resident_stage.length << " -> entry 0x" << plan.resident_entry
        << ", marker 0x" << plan.loader_magic << std::dec << '\n'
        << "          raw stage SHA-256: bootstrap " << plan.bootstrap_loader.raw_sha256
        << "; first " << plan.first_stage.raw_sha256
        << "; resident " << plan.resident_stage.raw_sha256 << '\n'
        << "          opaque loader handoff: " << std::dec
        << opaque_invocation.byte_count << " bytes at disk 0x" << std::hex
        << opaque_invocation.raw_disk_offset << " (SHA-256 " << opaque_invocation.sha256
        << "); JSR (A3) at 0x" << opaque_invocation.first_stage_invocation_address
        << " -> 0x" << opaque_invocation.first_stage_target
        << ", terminal JMP (A3) at 0x" << opaque_invocation.resident_stage_jump_address
        << " -> 0x" << opaque_invocation.resident_stage_target << std::dec
        << " (opaque; no invocation or return execution)\n"
        << "          first-stage source anchors: disk 0x" << std::hex
        << first_stage_source_anchors.raw_disk_offset << " + 0x"
        << first_stage_source_anchors.byte_count << " (SHA-256 "
        << first_stage_source_anchors.sha256 << "); offsets +0x"
        << first_stage_source_anchors.anchor_stage_offsets[0] << ", +0x"
        << first_stage_source_anchors.anchor_stage_offsets[1] << ", +0x"
        << first_stage_source_anchors.anchor_stage_offsets[2] << std::dec
        << " and two verified source windows (provenance only; no stage decoding)\n"
        << "          resident gate: entry 0x" << std::hex << resident.entry_address
        << " calls 0x" << resident.initializer_address << "; d3 != 0 ORs 0x"
        << resident.d3_nonzero_or_mask << " into d0, stores word at 0x"
        << resident.result_word_address << '\n'
        << "          resident word splitter: entry 0x" << splitter.entry_address
        << " reads A1+0x" << splitter.source_a1_offset << "; low 15-bit words -> 0x"
        << splitter.magnitude_word_addresses[0] << ", 0x" << splitter.magnitude_word_addresses[1]
        << ", 0x" << splitter.magnitude_word_addresses[2] << "; sign bytes -> 0x"
        << splitter.sign_byte_addresses[0] << ", 0x" << splitter.sign_byte_addresses[1]
        << ", 0x" << splitter.sign_byte_addresses[2] << "; helper 0x"
        << splitter.helper_address << std::dec << '\n'
        << "          splitter pre-helper transform is modeled in memory; no raw-resident "
           "caller or helper return effect is yet proven\n"
        << "          helper raw boundary: target 0x" << std::hex << helper_boundary.helper_address
        << " maps to disk 0x" << helper_boundary.raw_disk_offset << std::dec
        << "; 32-byte SHA-256 " << helper_boundary.raw_prefix_sha256
        << " (not treated as an executable helper)\n"
        << "          setup helper raw boundary: target 0x" << std::hex
        << setup_helper_boundary.helper_address << " maps to disk 0x"
        << setup_helper_boundary.raw_disk_offset << std::dec << "; 32-byte SHA-256 "
        << setup_helper_boundary.raw_prefix_sha256
        << " (not treated as an executable helper)\n";
    for (const auto& callsite : staging_callsites) {
        std::cout << "          staging caller 0x" << std::hex << callsite.entry_address
            << ": source 0x" << callsite.source_address << ", JSR setup 0x"
            << callsite.setup_helper_address << ", JSR helper 0x" << callsite.helper_address
            << "; static post-JSR bytes at 0x" << callsite.post_helper_return_address
            << " load 0x" << callsite.post_helper_source_address << " and 0x"
            << callsite.post_helper_magnitude_address << std::dec
            << " (byte boundary only; no helper-return execution)\n";
    }
    std::cout << "          first post-helper static chain: caller 0x" << std::hex
        << first_post_helper_chain.staging_entry_address << ", bytes at 0x"
        << first_post_helper_chain.static_start_address << " (disk 0x"
        << first_post_helper_chain.raw_disk_offset << ", " << std::dec
        << first_post_helper_chain.byte_count << " bytes, SHA-256 "
        << first_post_helper_chain.sha256 << "); static JSR 0x" << std::hex
        << first_post_helper_chain.next_setup_target << " at 0x"
        << first_post_helper_chain.next_setup_call_address << ", then 0x"
        << first_post_helper_chain.following_target << " at 0x"
        << first_post_helper_chain.following_call_address << std::dec
        << " (byte chain only; no helper-return or call execution)\n";
    std::cout << "          second post-helper static chain: caller 0x" << std::hex
        << second_post_helper_chain.staging_entry_address << ", bytes at 0x"
        << second_post_helper_chain.static_start_address << " (disk 0x"
        << second_post_helper_chain.raw_disk_offset << ", " << std::dec
        << second_post_helper_chain.byte_count << " bytes, SHA-256 "
        << second_post_helper_chain.sha256 << "); static JSR 0x" << std::hex
        << second_post_helper_chain.static_call_target << " at 0x"
        << second_post_helper_chain.static_call_address << std::dec
        << " (byte chain only; no helper-return or call execution)\n";
    std::cout << "          staging reachability boundary: raw disk 0x" << std::hex
        << staging_reachability.scanned_raw_disk_offset << " +0x"
        << staging_reachability.scanned_byte_count << "; direct absolute JSR/JMP counts to 0x"
        << staging_reachability.staging_entry_addresses[0] << " = " << std::dec
        << staging_reachability.absolute_jsr_counts[0] << '/' << staging_reachability.absolute_jmp_counts[0]
        << ", 0x" << std::hex << staging_reachability.staging_entry_addresses[1] << " = " << std::dec
        << staging_reachability.absolute_jsr_counts[1] << '/' << staging_reachability.absolute_jmp_counts[1]
        << "; PC-relative BSR.W counts = " << staging_reachability.pc_relative_bsr_word_counts[0]
        << '/' << staging_reachability.pc_relative_bsr_word_counts[1]
        << "; local MOVEA/JSR(An), JMP(An) counts = "
        << staging_reachability.local_immediate_register_jsr_counts[0] << '/'
        << staging_reachability.local_immediate_register_jsr_counts[1] << ", "
        << staging_reachability.local_immediate_register_jmp_counts[0] << '/'
        << staging_reachability.local_immediate_register_jmp_counts[1]
        << " (only these static encodings; indirect/transformed paths unproven)\n";
    std::cout << "          separate post-call boundary: entry 0x" << std::hex
        << separate_post_call.entry_address << " (disk 0x" << separate_post_call.raw_disk_offset
        << ", SHA-256 " << separate_post_call.sha256 << "); D0 #0x"
        << separate_post_call.d0_immediate << ", A5 source 0x"
        << separate_post_call.a5_source_address << ", store D0 0x"
        << separate_post_call.stored_d0_address << ", JSR 0x"
        << separate_post_call.following_call_target << " at 0x"
        << separate_post_call.following_call_address << " (target disk 0x"
        << separate_post_call.following_target_raw_disk_offset << ", SHA-256 "
        << separate_post_call.following_target_prefix_sha256 << std::dec
        << "; static only, no prior-return/RAM/call execution)\n";
    std::cout << "          separate post-call tail: six JSRs at 0x" << std::hex
        << separate_post_call_tail.entry_address << " (disk 0x"
        << separate_post_call_tail.raw_disk_offset << ", " << std::dec
        << separate_post_call_tail.byte_count << " bytes, SHA-256 "
        << separate_post_call_tail.sha256 << ")";
    for (std::size_t index = 0; index < separate_post_call_tail.call_targets.size(); ++index) {
        std::cout << "; 0x" << std::hex << separate_post_call_tail.call_targets[index]
            << " at 0x" << separate_post_call_tail.call_addresses[index]
            << " -> disk 0x" << separate_post_call_tail.target_raw_disk_offsets[index]
            << " (SHA-256 " << separate_post_call_tail.target_prefix_sha256[index] << ')';
    }
    std::cout << std::dec << " (static only, no prior-return/call execution)\n";
    std::cout << "          separate post-call tail branch: entry 0x" << std::hex
        << separate_post_call_tail_branch.entry_address << " (disk 0x"
        << separate_post_call_tail_branch.raw_disk_offset << ", SHA-256 "
        << separate_post_call_tail_branch.sha256 << "); CMP.B #0x"
        << static_cast<unsigned>(separate_post_call_tail_branch.compare_immediate)
        << ", 0x" << separate_post_call_tail_branch.compared_byte_address
        << "; BCS.W at 0x" << separate_post_call_tail_branch.conditional_branch_address
        << " -> 0x" << separate_post_call_tail_branch.conditional_branch_target
        << " (target disk 0x" << separate_post_call_tail_branch.target_raw_disk_offset
        << ", SHA-256 " << separate_post_call_tail_branch.target_prefix_sha256 << std::dec
        << "; static only, no prior-return/RAM/call execution)\n";
    std::cout << "          separate comparison boundary: BNE.W at 0x" << std::hex
        << separate_comparison.preceding_branch_address << " -> 0x"
        << separate_comparison.preceding_branch_target << "; entry 0x"
        << separate_comparison.entry_address << " (disk 0x"
        << separate_comparison.raw_disk_offset << ", SHA-256 "
        << separate_comparison.sha256 << ')';
    for (std::size_t index = 0;
         index < separate_comparison.conditional_branch_addresses.size(); ++index) {
        std::cout << "; branch 0x" << separate_comparison.conditional_branch_addresses[index]
            << " -> 0x" << separate_comparison.conditional_branch_targets[index];
    }
    std::cout << "; continuation disk 0x"
        << separate_comparison.continuation_raw_disk_offset << " (SHA-256 "
        << separate_comparison.continuation_prefix_sha256 << std::dec
        << "; static only, no register/flag/path execution)\n";
    std::cout << "          separate byte gate: entry 0x" << std::hex
        << separate_byte_gate.entry_address << " (disk 0x" << separate_byte_gate.raw_disk_offset
        << ", SHA-256 " << separate_byte_gate.sha256 << "); CMP.B D0,0x"
        << separate_byte_gate.compared_byte_address << "; BEQ.W at 0x"
        << separate_byte_gate.conditional_branch_address << " -> 0x"
        << separate_byte_gate.conditional_branch_target << " (target disk 0x"
        << separate_byte_gate.target_raw_disk_offset << ", SHA-256 "
        << separate_byte_gate.target_prefix_sha256 << "; fallthrough disk 0x"
        << separate_byte_gate.fallthrough_raw_disk_offset << ", SHA-256 "
        << separate_byte_gate.fallthrough_prefix_sha256 << std::dec
        << "; static only, no register/branch execution)\n";
    std::cout << "          taken byte-gate target: entry 0x" << std::hex
        << separate_byte_gate_target.entry_address << " (disk 0x"
        << separate_byte_gate_target.raw_disk_offset << ", SHA-256 "
        << separate_byte_gate_target.sha256 << ")";
    for (std::size_t index = 0;
         index < separate_byte_gate_target.conditional_branch_addresses.size(); ++index) {
        std::cout << "; BCC.W 0x"
            << separate_byte_gate_target.conditional_branch_addresses[index] << " -> 0x"
            << separate_byte_gate_target.conditional_branch_targets[index];
    }
    std::cout << "; convergence 0x" << separate_byte_gate_target.convergence_address
        << " (disk 0x" << separate_byte_gate_target.convergence_raw_disk_offset
        << ", SHA-256 " << separate_byte_gate_target.convergence_prefix_sha256 << std::dec
        << "; static only, no flag/cell/path execution)\n";
    std::cout << "          byte-gate convergence: entry 0x" << std::hex
        << separate_byte_gate_convergence.entry_address << " (disk 0x"
        << separate_byte_gate_convergence.raw_disk_offset << ", SHA-256 "
        << separate_byte_gate_convergence.sha256 << "); BEQ.W at 0x"
        << separate_byte_gate_convergence.conditional_branch_address << " -> 0x"
        << separate_byte_gate_convergence.conditional_branch_target << " (target disk 0x"
        << separate_byte_gate_convergence.target_raw_disk_offset << ", SHA-256 "
        << separate_byte_gate_convergence.target_prefix_sha256 << "; fallthrough disk 0x"
        << separate_byte_gate_convergence.fallthrough_raw_disk_offset << ", SHA-256 "
        << separate_byte_gate_convergence.fallthrough_prefix_sha256 << std::dec
        << "; static only, no register/branch execution)\n";
    std::cout << "          taken convergence branch: entry 0x" << std::hex
        << separate_byte_gate_taken_branch.entry_address << " (disk 0x"
        << separate_byte_gate_taken_branch.raw_disk_offset << ", SHA-256 "
        << separate_byte_gate_taken_branch.sha256 << ')';
    for (std::size_t index = 0;
         index < separate_byte_gate_taken_branch.conditional_branch_addresses.size(); ++index) {
        std::cout << "; BCC.W 0x"
            << separate_byte_gate_taken_branch.conditional_branch_addresses[index] << " -> 0x"
            << separate_byte_gate_taken_branch.conditional_branch_targets[index];
    }
    std::cout << "; external JSR 0x" << separate_byte_gate_taken_branch.external_call_address
        << " -> 0x" << separate_byte_gate_taken_branch.external_call_target << " (disk 0x"
        << separate_byte_gate_taken_branch.external_prefix_raw_disk_offset << ", SHA-256 "
        << separate_byte_gate_taken_branch.external_prefix_sha256 << std::dec
        << "; static boundary only, no call execution)\n";
    std::cout << "          byte-gate fallthrough: entry 0x" << std::hex
        << separate_byte_gate_fallthrough.entry_address << " (disk 0x"
        << separate_byte_gate_fallthrough.raw_disk_offset << ", SHA-256 "
        << separate_byte_gate_fallthrough.sha256 << "); BEQ.W at 0x"
        << separate_byte_gate_fallthrough.conditional_branch_address << " -> 0x"
        << separate_byte_gate_fallthrough.conditional_branch_target
        << "; alternate prefix 0x" << separate_byte_gate_fallthrough.other_path_entry_address
        << " (SHA-256 " << separate_byte_gate_fallthrough.other_path_sha256 << "); BRA.W at 0x"
        << separate_byte_gate_fallthrough.other_path_branch_address << " -> 0x"
        << separate_byte_gate_fallthrough.other_path_branch_target << std::dec
        << " (static only, no register/branch execution)\n";
    std::cout << "          independent resident gate: entry 0x" << std::hex
        << independent_entry.entry_address << "; negative-D3 branch 0x"
        << independent_entry.negative_d3_branch_address << " -> 0x"
        << independent_entry.negative_d3_target << "; fixed-byte test 0x"
        << independent_entry.flag_test_address << " at 0x" << independent_entry.flag_address
        << ", zero branch 0x" << independent_entry.flag_zero_branch_address << " -> 0x"
        << independent_entry.flag_zero_target << std::dec
        << " (static only, no flag/branch execution)\n"
        << "          negative-D3 continuation: entry 0x" << std::hex
        << negative_d3.entry_address << "; external JMP 0x"
        << negative_d3.external_jump_address << " -> 0x" << negative_d3.external_jump_target
        << "; terminal tail 0x" << negative_d3_terminal.entry_address
        << " has immediates 0x" << negative_d3_terminal.first_add_immediate << ", 0x"
        << negative_d3_terminal.second_add_immediate << " and RTS 0x"
        << negative_d3_terminal.return_address << std::dec
        << " (validated raw bytes only; no predicates, target, or effects executed)\n"
        << "          post-negative-D3 terminal: entry 0x" << std::hex
        << post_negative_d3.entry_address << "; byte stores 0x"
        << post_negative_d3.absolute_byte_store_addresses[0] << "/0x"
        << post_negative_d3.absolute_byte_store_addresses[1] << "; BNE 0x"
        << post_negative_d3.nonzero_branch_address << " -> 0x"
        << post_negative_d3.nonzero_branch_target << ", zero RTS 0x"
        << post_negative_d3.zero_return_address << "; BPL 0x"
        << post_negative_d3.nonnegative_branch_address << " -> boundary 0x"
        << post_negative_d3.nonnegative_branch_target << ", negative RTS 0x"
        << post_negative_d3.negative_return_address << "; SHA-256 "
        << post_negative_d3.raw_sha256 << std::dec
        << " (static only; no registers, stores, predicates, or continuation executed)\n"
        << "          post-negative-D3 continuation: entry 0x" << std::hex
        << post_negative_d3_continuation.entry_address << " (disk 0x"
        << post_negative_d3_continuation.raw_disk_offset << ", " << std::dec
        << post_negative_d3_continuation.byte_count << " bytes); BCC 0x" << std::hex
        << post_negative_d3_continuation.compare_branch_address << " -> 0x"
        << post_negative_d3_continuation.compare_branch_target << ", BCS 0x"
        << post_negative_d3_continuation.low_range_branch_address << " -> 0x"
        << post_negative_d3_continuation.low_range_branch_target << ", BMI 0x"
        << post_negative_d3_continuation.negative_range_branch_address << " -> 0x"
        << post_negative_d3_continuation.negative_range_branch_target << "; terminal JMP 0x"
        << post_negative_d3_continuation.terminal_jump_address << " -> 0x"
        << post_negative_d3_continuation.terminal_jump_target << "; SHA-256 "
        << post_negative_d3_continuation.raw_sha256 << std::dec
        << " (static only; no branch, restore, or jump execution)\n";
}

void report_millennium_atari_root_inventory(const eon::MillenniumAtariRootInventory& inventory) {
    // Keep the complete raw filesystem listing in a small separate function.
    // Besides making the provenance report easier to audit, this avoids
    // stressing compiler optimisation of the already deliberately detailed
    // Atari bootstrap report below.
    if (inventory.files.empty()) throw std::runtime_error("Verified Millennium Atari ST root is empty");
    std::cout << "          Equinox FAT12 root: " << inventory.files.size()
        << " original regular files; first " << inventory.files.front().name
        << " SHA-256 " << inventory.files.front().sha256 << ", final "
        << inventory.files.back().name << " SHA-256 "
        << inventory.files.back().sha256
        << " (complete read-only inventory; no file access or format semantics inferred)\n";
    for (const auto& file : inventory.files) {
        std::cout << "            " << file.name << " cluster " << file.first_cluster
            << ", " << file.size << " bytes, SHA-256 " << file.sha256 << '\n';
    }
}

void report_millennium_atari_st(const eon::ReleaseArchive& release) {
    constexpr auto equinox_disk_sha256 =
        "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7";
    constexpr auto disk1_stx_sha256 =
        "081d8bc102b8c7669c5cb21abace9b08532bc0b34164f11465d0c87b63a422fd";
    const auto image = eon::extract_verified_release_asset(release, equinox_disk_sha256);
    if (!image) return;
    const eon::Fat12Disk disk(*image);
    const auto* executable = disk.find("MILENIUM.TOS");
    if (!executable) throw std::runtime_error("Verified Millennium Atari ST disk has no MILENIUM.TOS");
    const auto executable_bytes = disk.read(*executable);
    const eon::MillenniumAtariBootstrapSession live_bootstrap(disk, executable_bytes);
    const auto prg = eon::parse_atari_st_prg(executable_bytes);
    const auto bootstrap = eon::parse_millennium_atari_bootstrap(executable_bytes, prg);
    const auto bss_entry = eon::parse_millennium_atari_bss_entry(executable_bytes, prg, bootstrap);
    const auto bss_source = eon::materialize_millennium_atari_bss_source(
        executable_bytes, prg, bootstrap, bss_entry);
    const auto target = eon::materialize_millennium_atari_target(bss_source, bss_entry);
    const auto trap_entry = eon::parse_millennium_atari_trap_entry(bss_source, target);
    const auto fopen_fallthrough = eon::parse_millennium_atari_fopen_fallthrough(target, trap_entry);
    const auto fread_config_transfer = eon::parse_millennium_atari_fread_config_transfer_boundary(
        target, fopen_fallthrough);
    std::cout << "          bounded launcher bootstrap: target 0x" << std::hex
        << live_bootstrap.target().target_address << ", Fopen boundary "
        << live_bootstrap.fopen_boundary().fopen_filename << std::dec
        << " (no GEMDOS call)\n";
    if (const auto physical_disk = eon::extract_verified_release_asset(release, disk1_stx_sha256)) {
        const eon::AtariStStxPhysicalDisk stx(*physical_disk);
        const auto boot = stx.sector(0, 0, 1);
        const auto loader = stx.sector(1, 0, 9);
        const auto boot_location = std::find_if(stx.sectors().begin(), stx.sectors().end(),
            [](const eon::AtariStStxSector& sector) {
                return sector.track == 0 && sector.side == 0 && sector.id == 1;
            });
        const auto loader_location = std::find_if(stx.sectors().begin(), stx.sectors().end(),
            [](const eon::AtariStStxSector& sector) {
                return sector.track == 1 && sector.side == 0 && sector.id == 9;
            });
        if (boot_location == stx.sectors().end() || loader_location == stx.sectors().end()) {
            throw std::runtime_error("Verified Millennium Atari STX has no required physical sector");
        }
        std::cout << "          physical Disk 1 STX: SHA-256 " << disk1_stx_sha256
            << "; " << stx.track_count() << " track records, " << stx.sectors().size()
            << " identified sectors; T0/H0/S1 +0x" << std::hex
            << boot_location->payload_offset << ", " << std::dec << boot.size()
            << " bytes, SHA-256 " << eon::to_hex(eon::sha256(boot))
            << "; T1/H0/S9 +0x" << std::hex << loader_location->payload_offset
            << ", " << std::dec << loader.size() << " bytes, SHA-256 "
            << eon::to_hex(eon::sha256(loader)) << "; literal +0xbe MILL22B.inf"
            << " (direct STX sector spans only; no flattened image, filesystem traversal,"
            << " boot semantics, or executable handoff)\n";
    }
    const auto equinox_config = eon::probe_millennium_atari_config(disk);
    if (!equinox_config.present) throw std::runtime_error("Verified Millennium Atari ST disk has no MILL22A.inf");
    const auto& root_inventory = live_bootstrap.root_inventory();
    const auto auxiliary_resource = eon::probe_millennium_atari_auxiliary_resource_name(disk);
    const auto config_entry = eon::parse_millennium_atari_config_entry(
        disk.read(*disk.find(equinox_config.requested_filename)));
    const auto config_load_address_boundary = eon::parse_millennium_atari_fread_config_load_address_boundary(
        fread_config_transfer, disk.read(*disk.find(equinox_config.requested_filename)), config_entry);
    const auto fread_mapped_config_prelude = eon::parse_millennium_atari_fread_mapped_config_prelude(
        fread_config_transfer, disk.read(*disk.find(equinox_config.requested_filename)), config_entry);
    const auto config_trap_argument_strings = eon::parse_millennium_atari_config_trap_argument_strings(
        disk.read(*disk.find(equinox_config.requested_filename)), config_entry);
    const auto config_first_jsr = eon::parse_millennium_atari_config_first_jsr(
        disk.read(*disk.find(equinox_config.requested_filename)), config_entry);
    const auto config_second_jsr = eon::parse_millennium_atari_config_second_jsr(
        disk.read(*disk.find(equinox_config.requested_filename)), config_entry);
    const auto config_join_jsr = eon::parse_millennium_atari_config_join_jsr(
        disk.read(*disk.find(equinox_config.requested_filename)), config_second_jsr);
    const auto config_forwarded_jsr = eon::parse_millennium_atari_config_forwarded_jsr(
        disk.read(*disk.find(equinox_config.requested_filename)), config_entry);
    const auto config_third_jsr = eon::parse_millennium_atari_config_third_jsr(
        disk.read(*disk.find(equinox_config.requested_filename)), config_entry);
    const auto config_fourth_jsr = eon::parse_millennium_atari_config_fourth_jsr(
        disk.read(*disk.find(equinox_config.requested_filename)), config_entry);
    const auto config_fourth_prelude = eon::parse_millennium_atari_config_fourth_prelude(
        disk.read(*disk.find(equinox_config.requested_filename)), config_fourth_jsr);
    const auto config_fourth_loop = eon::parse_millennium_atari_config_fourth_loop(
        disk.read(*disk.find(equinox_config.requested_filename)), config_fourth_jsr);
    const auto config_fourth_post_loop = eon::parse_millennium_atari_config_fourth_post_loop(
        disk.read(*disk.find(equinox_config.requested_filename)), config_fourth_loop);
    const auto config_fourth_outer_setup = eon::parse_millennium_atari_config_fourth_outer_setup(
        disk.read(*disk.find(equinox_config.requested_filename)), config_fourth_post_loop);
    const auto config_fourth_post_outer = eon::parse_millennium_atari_config_fourth_post_outer_boundary(
        disk.read(*disk.find(equinox_config.requested_filename)), config_fourth_post_loop);
    const auto config_fourth_post_outer_tail = eon::parse_millennium_atari_config_fourth_post_outer_tail(
        disk.read(*disk.find(equinox_config.requested_filename)), config_fourth_post_outer);
    const auto config_fourth_post_outer_recurrence = eon::parse_millennium_atari_config_fourth_post_outer_recurrence(
        disk.read(*disk.find(equinox_config.requested_filename)), config_fourth_post_outer_tail,
        config_fourth_loop);
    const auto config_jsr_inventory = eon::inventory_millennium_atari_config_absolute_jsrs(
        disk.read(*disk.find(equinox_config.requested_filename)));
    const auto config_residual_jsr_body = eon::parse_millennium_atari_config_residual_jsr_body(
        disk.read(*disk.find(equinox_config.requested_filename)), config_jsr_inventory);
    std::cout << "          auxiliary resource-name evidence: "
        << auxiliary_resource.container_filename << " cluster " << auxiliary_resource.first_cluster
        << ", +0x" << std::hex << auxiliary_resource.literal_file_offset << " = "
        << auxiliary_resource.literal_filename << "; SHA-256 " << auxiliary_resource.sha256 << std::dec
        << " (literal only; no open, decoder, or presentation claim)\n";
    std::cout << "          MILENIUM.TOS: text " << prg.text_bytes << ", data "
        << prg.data_bytes << ", BSS " << prg.bss_bytes << ", "
        << prg.relocation_count << " relocations (0x" << std::hex
        << prg.first_relocation_offset << "..0x" << prg.last_relocation_offset
        << std::dec << ")\n"
        << "          relocation values: [0x" << std::hex
        << prg.relocations.front().offset << "]=0x" << prg.relocations.front().original_value
        << ", [0x" << prg.relocations.back().offset << "]=0x"
        << prg.relocations.back().original_value << std::dec
        << " (unrelocated disk words)\n"
        << "          entry: BRA 0x" << std::hex << bootstrap.branch_target_offset
        << "; copies 0x" << bootstrap.stage_bytes << " bytes from 0x"
        << bootstrap.stage_source_offset << "..0x" << bootstrap.stage_last_longword_offset
        << " to BSS 0x" << bootstrap.stage_destination_offset
        << " and JMPs there" << std::dec << " (original bootstrap; not executed)\n"
        << "          BSS entry: copies " << bss_entry.copied_words << " words from 0x"
        << std::hex << bss_entry.copy_source_address << " to 0x"
        << bss_entry.copy_destination_address << "; JMP 0x" << bss_entry.jump_address
        << std::dec << " (original bootstrap; not executed)\n"
        << "          BSS source: PRG load base 0x" << std::hex << bss_source.load_base
        << "; " << std::dec << bss_source.original_data_bytes << " original DATA bytes at 0x"
        << std::hex << bss_source.source_data_offset << " plus " << std::dec
        << bss_source.bss_zero_bytes << " loader-zeroed BSS bytes (in-memory only)\n"
        << "          target 0x" << std::hex << target.target_address << ": opcode 0x"
        << target.first_opcode << " immediate word 0x" << target.first_immediate_word
        << ", immediate longword 0x" << target.first_immediate_longword << std::dec
        << " (validated original target; not executed)\n"
        << "          GEMDOS boundary: Fopen 0x" << std::hex << trap_entry.fopen_filename_address
        << " (\"" << trap_entry.fopen_filename << "\", mode " << std::dec
        << trap_entry.fopen_access_mode << ", function 0x" << std::hex
        << trap_entry.fopen_function << ") via TRAP #1 +0x" << trap_entry.fopen_trap_offset
        << "; next Fclose selector 0x" << trap_entry.following_fclose_function
        << " is prepared at +0x" << trap_entry.following_fclose_selector_offset << std::dec
        << "; Fopen negative D0 loops at +0x" << std::hex
        << trap_entry.fopen_result_negative_branch_target_offset << std::dec
        << " (reported only; no GEMDOS emulation)\n"
        << "          Fopen fall-through: target +0x" << std::hex << fopen_fallthrough.entry_offset
        << "; selector 0x" << fopen_fallthrough.fread_function << " reads D0 handle, 0x"
        << fopen_fallthrough.fread_byte_count << " bytes to 0x"
        << fopen_fallthrough.fread_buffer_address << " via TRAP #1 +0x"
        << fopen_fallthrough.fread_trap_offset << "; stack cleanup 0x"
        << fopen_fallthrough.stack_cleanup_bytes << "; SHA-256 " << fopen_fallthrough.sha256
        << std::dec << " (static call boundary only; no Fopen/Fread result or data is modeled)\n"
        << "          Fread-config transfer: target +0x" << std::hex
        << fread_config_transfer.entry_offset << "; TRAP #1, stack cleanup 0x"
        << fread_config_transfer.stack_cleanup_bytes << ", JSR 0x"
        << fread_config_transfer.config_buffer_address << "; SHA-256 "
        << fread_config_transfer.sha256 << std::dec
        << " (static edge only; no GEMDOS result, buffer fill, or JSR execution)\n"
        << "          requested config " << equinox_config.requested_filename << ": "
        << (equinox_config.present ? "present" : "absent") << " in Equinox FAT12 root ("
        << equinox_config.root_entry_count << " live entries)";
    if (equinox_config.present) {
        std::cout << "; cluster " << equinox_config.first_cluster << ", "
            << equinox_config.size << " bytes, SHA-256 " << equinox_config.sha256
            << ", leading words 0x" << std::hex << equinox_config.first_word << " 0x"
            << equinox_config.first_longword_operand << std::dec;
    }
    std::cout << " (metadata only; never generated or written)\n";
    report_millennium_atari_root_inventory(root_inventory);
    const auto* save_i = disk.find("2200SAVE.I");
    const auto* save_ii = disk.find("2200SAVE.II");
    const auto* save_iii = disk.find("2200SAVE.III");
    const auto* save_iv = disk.find("2200SAVE.IV");
    if (!save_i || !save_ii || !save_iii || !save_iv) {
        throw std::runtime_error("Verified Millennium Atari ST disk has incomplete save inventory");
    }
    const auto authenticated_save_i = eon::authenticate_millennium_save(
        eon::MillenniumSavePlatform::atari_st, "2200SAVE.I", disk.read(*save_i));
    const auto authenticated_save_ii = eon::authenticate_millennium_save(
        eon::MillenniumSavePlatform::atari_st, "2200SAVE.II", disk.read(*save_ii));
    const auto authenticated_save_iii = eon::authenticate_millennium_save(
        eon::MillenniumSavePlatform::atari_st, "2200SAVE.III", disk.read(*save_iii));
    const auto authenticated_save_iv = eon::authenticate_millennium_save(
        eon::MillenniumSavePlatform::atari_st, "2200SAVE.IV", disk.read(*save_iv));
    const auto save_i_ii = eon::compare_millennium_saves(authenticated_save_i,
        authenticated_save_ii);
    const auto save_iii_iv = eon::compare_millennium_saves(authenticated_save_iii,
        authenticated_save_iv);
    std::cout << "          Atari save byte comparison: I/II " << save_i_ii.equal_positions
        << "/" << save_i_ii.shared_bytes << " equal positions (prefix/suffix "
        << save_i_ii.common_prefix_bytes << "/" << save_i_ii.common_suffix_bytes
        << "); III/IV " << save_iii_iv.equal_positions << "/" << save_iii_iv.shared_bytes
        << " (prefix/suffix " << save_iii_iv.common_prefix_bytes << "/"
        << save_iii_iv.common_suffix_bytes
        << "; hash-gated byte facts only, no save-format or compatibility claim)\n";
    std::cout << "          MILL22A.inf candidate entry: JMP 0x" << std::hex << config_entry.entry_address
        << " resolves from independent static base 0x" << config_entry.proven_load_base
        << " to file +0x" << config_entry.entry_file_offset << "; TRAP #14 selectors 0x"
        << config_entry.initial_trap_selector << " (longword 0x"
        << config_entry.initial_trap_longword_argument << ") and 0x"
        << config_entry.second_trap_selector << " (longword 0x"
        << config_entry.second_trap_longword_argument << "); JSRs";
    for (const auto address : config_entry.jsr_targets) std::cout << " 0x" << address;
    std::cout << "; PEA 0x" << config_entry.final_pea_address << ", TRAP #14 selector 0x"
        << config_entry.final_trap_selector << ", RTS +0x" << config_entry.return_offset
        << std::dec << " (validated only; no TOS/XBIOS calls or config execution)\n";
    std::cout << "          Fread/config address boundary: JSR destination 0x" << std::hex
        << config_load_address_boundary.fread_destination_address << "; file JMP 0x"
        << config_load_address_boundary.payload_initial_jump_target_address << " would be +0x"
        << config_load_address_boundary.payload_initial_jump_target_file_offset_from_destination
        << " from that destination, while independent entry evidence is +0x"
        << config_load_address_boundary.independent_entry_file_offset << " (delta +0x"
        << config_load_address_boundary.independent_entry_offset_delta << "; SHA-256 "
        << config_load_address_boundary.payload_initial_jump_sha256 << std::dec
        << "; unresolved native load-address boundary, no mapping or execution inferred)\n";
    std::cout << "          conditional Fread-mapped config prelude: 0x" << std::hex
        << fread_mapped_config_prelude.mapped_entry_address << " = file +0x"
        << fread_mapped_config_prelude.mapped_entry_file_offset << " (" << std::dec
        << fread_mapped_config_prelude.byte_count << " bytes; SHA-256 "
        << fread_mapped_config_prelude.sha256 << "), branch -> 0x" << std::hex
        << fread_mapped_config_prelude.conditional_branch_target_address << ", converged JSR 0x"
        << fread_mapped_config_prelude.converged_jsr_address << " -> 0x"
        << fread_mapped_config_prelude.converged_jsr_target_address << ", fall-through 0x"
        << fread_mapped_config_prelude.continuation_address << std::dec
        << " (conditional bytes only; no GEMDOS return, branch, JSR, or platform state supplied)\n";
    std::cout << "          second literal TRAP argument: 0x" << std::hex
        << config_trap_argument_strings.argument_address << " (file +0x"
        << config_trap_argument_strings.file_offset << ") = "
        << config_trap_argument_strings.strings[0] << ", "
        << config_trap_argument_strings.strings[1] << "; SHA-256 "
        << config_trap_argument_strings.sha256 << std::dec
        << " (bytes only; no service or file access)\n";
    std::cout << "          MILL22A.inf first JSR target: 0x" << std::hex
        << config_first_jsr.target_address << " is file +0x" << config_first_jsr.target_file_offset
        << "; leading opcode 0x" << config_first_jsr.leading_opcode << ", then MOVEM.L opcode 0x"
        << config_first_jsr.movem_opcode << " mask 0x" << config_first_jsr.movem_register_mask
        << ", RTS 0x" << config_first_jsr.return_opcode << std::dec
        << " (validated only; no JSR execution or caller-state inference)\n";
    std::cout << "          MILL22A.inf second JSR target: 0x" << std::hex
        << config_second_jsr.target_address << " is file +0x" << config_second_jsr.target_file_offset
        << "; opcode 0x" << config_second_jsr.initial_opcode << " immediate 0x"
        << config_second_jsr.immediate_bit_number << ", branch 0x"
        << config_second_jsr.conditional_branch_opcode << " -> 0x"
        << config_second_jsr.conditional_branch_target_address << "; joined JSR at 0x"
        << config_second_jsr.join_jsr_address << " -> 0x" << config_second_jsr.join_jsr_target
        << ", following JSR -> 0x" << config_second_jsr.following_jsr_target << std::dec
        << " (validated only; no branch evaluation or calls)\n";
    std::cout << "          MILL22A.inf joined JSR target: 0x" << std::hex
        << config_join_jsr.target_address << " is file +0x" << config_join_jsr.target_file_offset
        << "; opcodes 0x" << config_join_jsr.initial_opcode << " 0x"
        << config_join_jsr.d0_word_store_opcode << " -> 0x"
        << config_join_jsr.d0_word_store_address << ", opaque Line-A 0x"
        << config_join_jsr.line_a_opcode << ", stores 0x"
        << config_join_jsr.first_longword_store_address << " and 0x"
        << config_join_jsr.second_longword_store_address << ", RTS 0x"
        << config_join_jsr.return_opcode << std::dec
        << " (validated only; no Line-A/OS emulation or RAM-state synthesis)\n";
    std::cout << "          MILL22A.inf forwarded JSR target: 0x" << std::hex
        << config_forwarded_jsr.entry_address << " is file +0x"
        << config_forwarded_jsr.entry_file_offset << "; JMP 0x"
        << config_forwarded_jsr.forwarded_address << " (file +0x"
        << config_forwarded_jsr.forwarded_file_offset << "), opcodes 0x"
        << config_forwarded_jsr.initial_opcode << ", TRAP #14 selector 0x"
        << config_forwarded_jsr.trap_selector << " via 0x" << config_forwarded_jsr.trap_opcode
        << ", cleanup 0x" << config_forwarded_jsr.stack_cleanup_opcode << ", RTS 0x"
        << config_forwarded_jsr.return_opcode << std::dec
        << " (validated only; no XBIOS/firmware calls or state synthesis)\n";
    std::cout << "          MILL22A.inf 0x2b2be target: file +0x" << std::hex
        << config_third_jsr.target_file_offset << "; opcodes 0x"
        << config_third_jsr.initial_opcode << " 0x" << config_third_jsr.gate_opcode
        << " immediate 0x" << config_third_jsr.gate_immediate << ", branch 0x"
        << config_third_jsr.branch_opcode << " +0x" << config_third_jsr.branch_displacement
        << " -> 0x" << config_third_jsr.branch_target_address << " (opcode 0x"
        << config_third_jsr.branch_target_opcode << " immediate 0x"
        << config_third_jsr.branch_target_immediate << ", branch 0x"
        << config_third_jsr.branch_target_branch_opcode << ')' << std::dec
        << " (validated only; no D0 branch choice or platform-state inference)\n";
    std::cout << "          MILL22A.inf 0x2b448 setup: file +0x" << std::hex
        << config_fourth_jsr.target_file_offset << "; D7 0x" << config_fourth_jsr.d7_initial_value
        << ", A5 0x" << config_fourth_jsr.a5_initial_address << ", A4 0x"
        << config_fourth_jsr.a4_initial_address << ", D6/D5/D4 0x"
        << config_fourth_jsr.d6_initial_value << "/0x" << config_fourth_jsr.d5_initial_value
        << "/0x" << config_fourth_jsr.d4_initial_value << std::dec
        << " (validated original setup only; no loop, trap, or data interpretation)\n";
    std::cout << "          MILL22A.inf static predecessor: 0x" << std::hex
        << config_fourth_prelude.prelude_address << " file +0x"
        << config_fourth_prelude.prelude_file_offset << " (" << std::dec
        << config_fourth_prelude.byte_count << " bytes; SHA-256 "
        << config_fourth_prelude.sha256 << "); D0/D1 0x" << std::hex
        << config_fourth_prelude.d0_initial_value << "/0x"
        << config_fourth_prelude.d1_initial_value << ", DBF 0x"
        << config_fourth_prelude.first_dbf_opcode << " -> 0x"
        << config_fourth_prelude.first_dbf_target_address << ", second D0 0x"
        << config_fourth_prelude.second_d0_initial_value << ", DBF 0x"
        << config_fourth_prelude.second_dbf_opcode << " -> 0x"
        << config_fourth_prelude.second_dbf_target_address << " -> 0x"
        << config_fourth_prelude.continuation_address << std::dec
        << " (static adjacency only; no callsite, loop execution, or data effect inferred)\n";
    std::cout << "          MILL22A.inf 0x2b448 first loop: 0x" << std::hex
        << config_fourth_loop.body_address << " file +0x" << config_fourth_loop.body_file_offset
        << " (" << std::dec << config_fourth_loop.body_bytes << " bytes), DBF opcode 0x"
        << std::hex << config_fourth_loop.backedge_opcode << " displacement " << std::dec
        << config_fourth_loop.backedge_displacement << " -> 0x" << std::hex
        << config_fourth_loop.backedge_target_address << "; setup D5 0x"
        << config_fourth_loop.setup_d5_value << std::dec
        << " (validated backedge only; no iteration or data-state execution)\n";
    std::cout << "          MILL22A.inf post-inner-loop: 0x" << std::hex
        << config_fourth_post_loop.post_loop_address << " file +0x"
        << config_fourth_post_loop.post_loop_file_offset << "; A5 opcode 0x"
        << config_fourth_post_loop.a5_advance_opcode << ", outer DBF 0x"
        << config_fourth_post_loop.outer_backedge_opcode << " displacement " << std::dec
        << config_fourth_post_loop.outer_backedge_displacement << " -> 0x" << std::hex
        << config_fourth_post_loop.outer_backedge_target_address << " (setup 0x"
        << config_fourth_post_loop.target_setup_opcode << " immediate 0x"
        << config_fourth_post_loop.target_setup_immediate << ')' << std::dec
        << " (validated control flow only; no loops, data, or native calls)\n";
    std::cout << "          MILL22A.inf outer-loop setup: 0x" << std::hex
        << config_fourth_outer_setup.setup_address << " file +0x"
        << config_fourth_outer_setup.setup_file_offset << "; D5 0x"
        << config_fourth_outer_setup.d5_initial_value << ", D4 0x"
        << config_fourth_outer_setup.d4_initial_value << " -> 0x"
        << config_fourth_outer_setup.continuation_address << std::dec
        << " (validated fall-through only; no loop iterations or data access)\n";
    std::cout << "          MILL22A.inf post-outer-loop boundary: 0x" << std::hex
        << config_fourth_post_outer.boundary_address << " file +0x"
        << config_fourth_post_outer.boundary_file_offset << "; pushes 0x"
        << config_fourth_post_outer.longword_argument << " and selector 0x"
        << config_fourth_post_outer.trap_selector << ", TRAP #14 opcode 0x"
        << config_fourth_post_outer.trap_opcode << std::dec
        << " (validated boundary only; no trap/native call or result inference)\n";
    std::cout << "          MILL22A.inf post-trap tail: 0x" << std::hex
        << config_fourth_post_outer_tail.tail_address << " file +0x"
        << config_fourth_post_outer_tail.tail_file_offset << std::dec << "; "
        << config_fourth_post_outer_tail.tail_bytes << " bytes, SHA-256 "
        << config_fourth_post_outer_tail.sha256
        << "; stack cleanup 0x" << std::hex
        << config_fourth_post_outer_tail.initial_stack_cleanup_opcode << ", D0 0x"
        << config_fourth_post_outer_tail.d0_initial_value << " decrement branch " << std::dec
        << static_cast<int>(config_fourth_post_outer_tail.d0_nonzero_branch_displacement)
        << " -> 0x" << std::hex << config_fourth_post_outer_tail.d0_nonzero_branch_target_address
        << "; DBF D7 " << std::dec << config_fourth_post_outer_tail.d7_backedge_displacement
        << " -> 0x" << std::hex << config_fourth_post_outer_tail.d7_backedge_target_address
        << "; selector 0x" << config_fourth_post_outer_tail.selector << ", TRAP #14 0x"
        << config_fourth_post_outer_tail.trap_opcode << ", cleanup/RTS 0x"
        << config_fourth_post_outer_tail.final_stack_cleanup_opcode << "/0x"
        << config_fourth_post_outer_tail.return_opcode << std::dec
        << " (validated only; no post-trap execution or state interpretation)\n";
    std::cout << "          MILL22A.inf post-trap recurrence: 0x" << std::hex
        << config_fourth_post_outer_recurrence.prefix_address << " file +0x"
        << config_fourth_post_outer_recurrence.prefix_file_offset << std::dec << "; "
        << config_fourth_post_outer_recurrence.prefix_bytes << " bytes, SHA-256 "
        << config_fourth_post_outer_recurrence.sha256 << ", falls through to 0x" << std::hex
        << config_fourth_post_outer_recurrence.continuation_address << std::dec
        << " (static linkage only; no trap return or loop execution)\n";
    std::cout << "          MILL22A.inf absolute-JSR encodings: "
        << config_jsr_inventory.encodings.size() << " (file +0x" << std::hex
        << config_jsr_inventory.encodings.front().first << " -> 0x"
        << config_jsr_inventory.encodings.front().second << ", final +0x"
        << config_jsr_inventory.encodings.back().first << " -> 0x"
        << config_jsr_inventory.encodings.back().second << std::dec
        << "; byte inventory only, not reachability claims)\n";
    std::cout << "          MILL22A.inf inventory-only JSR body: +0x" << std::hex
        << config_residual_jsr_body.callsite_file_offset << " -> 0x"
        << config_residual_jsr_body.target_address << " (file +0x"
        << config_residual_jsr_body.target_file_offset << ", " << std::dec
        << config_residual_jsr_body.byte_count << " bytes through RTS 0x" << std::hex
        << config_residual_jsr_body.return_opcode << "; SHA-256 "
        << config_residual_jsr_body.sha256 << std::dec
        << "; static body only, no reachability or execution claim)\n";

    // The outer archive is the supplied-media boundary.  Inspect every ST
    // leaf in memory so absence is not guessed from the one Equinox variant.
    std::size_t supplied_st_images = 0;
    std::size_t readable_fat12_images = 0;
    std::size_t config_files = 0;
    for (const auto& asset : eon::inventory_verified_release(release)) {
        if (asset.kind != eon::AssetKind::atari_st_disk) continue;
        ++supplied_st_images;
        const auto candidate = eon::extract_verified_release_asset(release, asset.sha256);
        if (!candidate) throw std::runtime_error("Verified Atari ST asset disappeared during scan");
        std::optional<eon::Fat12Disk> candidate_disk;
        try {
            candidate_disk.emplace(*candidate);
        } catch (const std::runtime_error&) {
            // The remaining supplied ST images may be raw/protected media.
            // They do not expose a FAT12 filename namespace to this parser.
            continue;
        }
        ++readable_fat12_images;
        if (eon::probe_millennium_atari_config(*candidate_disk).present) ++config_files;
    }
    std::cout << "          supplied ST config scan: " << supplied_st_images << " images, "
        << readable_fat12_images << " valid FAT12 volumes, " << config_files << " files named "
        << equinox_config.requested_filename
        << " (raw/protected images are not reinterpreted as files)\n";
    std::cout << "          Atari ST trace boundary: next evidence must identify GEMDOS Fopen D0/handle "
        << "behaviour, TRAP #14 and Line-A returns, configuration load address, and any codec, "
        << "palette, or planar destination. No alternate ST image or DOS/Amiga asset is substituted.\n";
}

void report_deuteros_atari_st(const eon::ReleaseArchive& release) {
    // The corpus has no pristine ST master: this hash identifies the supplied
    // Replicants Disk 1 whose boot code contains the recovered XBIOS stage.
    constexpr auto replicants_disk1_sha256 =
        "aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee";
    constexpr auto disk2_sha256 =
        "5501ce3fd79c9b37cf695692a8012267db23dacd8a2cc64c0c7b7e4305971193";
    const auto disk1_image = eon::extract_verified_release_asset(release, replicants_disk1_sha256);
    const auto disk2_image = eon::extract_verified_release_asset(release, disk2_sha256);
    if (!disk1_image || !disk2_image) return;
    const eon::DeuterosAtariBootstrapSession live_bootstrap(*disk1_image);
    const eon::DeuterosAtariDisk disk1(*disk1_image);
    const eon::DeuterosAtariDisk disk2(*disk2_image);
    const auto& stage = disk1.boot_profile();
    const auto& continuation = disk2.boot_profile();
    std::cout << "          protected ST media: " << stage.total_sectors << " sectors, "
        << stage.sectors_per_track << " sectors/track, boot checksum 0x" << std::hex
        << stage.boot_checksum << std::dec << "; FAT root intentionally unavailable\n";
    std::cout << "          bounded launcher bootstrap: first/second raw stages SHA-256 "
        << live_bootstrap.first_stage_sha256() << "/"
        << live_bootstrap.second_stage_sha256()
        << " (no XBIOS, callback, or state selection)\n";
    if (stage.has_recovered_first_stage) {
        const auto first_stage = disk1.read_sectors(stage.first_stage_track, stage.first_stage_side,
            stage.first_stage_sector, stage.first_stage_sector_count);
        const auto profile = eon::parse_deuteros_atari_first_stage(first_stage);
        const auto second_stage = disk1.read_sectors(profile.next_track, profile.next_side,
            profile.next_sector, profile.next_sector_count);
        const auto second_profile = eon::parse_deuteros_atari_second_stage(second_stage);
        const auto dispatch = eon::parse_deuteros_atari_dispatch(second_stage);
        const auto& state0_plan = live_bootstrap.state0_raw_load_plan();
        const auto& state1_plan = live_bootstrap.state1_raw_load_plan();
        const auto& state1_service = live_bootstrap.state1_service_boundary();
        const auto& state5_plan = live_bootstrap.state5_raw_load_plan();
        const auto& state5_return = live_bootstrap.state5_return();
        const auto& supervisor_callback = live_bootstrap.supervisor_callback();
        const auto& second_callee_continuation = live_bootstrap.second_callee_continuation();
        const auto& raw_reader_wrapper = live_bootstrap.raw_reader_wrapper();
        const auto& raw_reader_call = live_bootstrap.raw_reader_call_layout();
        const auto& direct_vector_callees = live_bootstrap.direct_vector_callees();
        const auto& direct_vector_transfer_loop = live_bootstrap.direct_vector_transfer_loop();
        const auto& direct_vector_transfer_tail = live_bootstrap.direct_vector_transfer_tail();
        const auto& state_selection = live_bootstrap.state_selection_layout();
        const auto& state_selection_continuation = live_bootstrap.state_selection_continuation();
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
        std::cout << "          Copied handoff provenance: RAM 0x" << std::hex
            << profile.copy_source << " + 0x" << second_profile.direct_entry_source_offset
            << " -> 0x" << second_profile.direct_entry << "; state 0x"
            << second_profile.dispatch_state_address << " indexes table 0x"
            << second_profile.dispatch_table_address << std::dec << '\n';
        std::cout << "          Static dispatch vectors: 0x" << std::hex
            << dispatch.vector_addresses[0] << ", 0x" << dispatch.vector_addresses[1]
            << "; state 0 raw args (RAM 0x" << dispatch.state0_destination << ", 0x"
            << dispatch.state0_byte_count << " bytes, sector 0x"
            << dispatch.state0_linear_sector << ")" << std::dec << '\n';
        std::cout << "          Static aliases: table slots 2/3/4 -> state-0 routine 0x"
            << std::hex << dispatch.vector_addresses[0] << std::dec << '\n';
        std::cout << "          State-1 vector service boundary: stage +0x" << std::hex
            << state1_service.callee_offset << " pushes 0x" << state1_service.longword_argument
            << " and XBIOS selector 0x" << state1_service.xbios_selector << ", TRAP #14; "
            << "ADDQ.L #" << std::dec << static_cast<unsigned>(state1_service.stack_cleanup_bytes)
            << ",A7 then returns raw args (RAM 0x" << std::hex << state1_service.destination
            << ", 0x" << state1_service.byte_count << " bytes, sector 0x"
            << state1_service.linear_sector << "); SHA-256 " << state1_service.callee_sha256
            << std::dec << " (no XBIOS execution, state selection, or media interpretation)\n";
        std::vector<std::uint8_t> state0_bytes;
        for (const auto& request : state0_plan.requests) {
            const auto chunk = disk1.read_sectors(request.track, request.side,
                request.first_sector, request.sector_count);
            state0_bytes.insert(state0_bytes.end(), chunk.begin(), chunk.end());
        }
        std::cout << "          Static state-0 raw-load plan: Disk 1 +0x" << std::hex
            << state0_plan.source_offset << " +0x" << state0_plan.byte_count
            << " -> RAM 0x" << state0_plan.destination << std::dec << " in "
            << state0_plan.requests.size() << " original nine-sector reads; SHA-256 "
            << eon::to_hex(eon::sha256(state0_bytes))
            << " (not selected or interpreted at runtime)\n";
        std::vector<std::uint8_t> state1_bytes;
        state1_bytes.reserve(state1_plan.byte_count);
        for (const auto& request : state1_plan.requests) {
            const auto chunk = disk1.read_sectors(request.track, request.side,
                request.first_sector, request.sector_count);
            state1_bytes.insert(state1_bytes.end(), chunk.begin(), chunk.end());
        }
        std::cout << "          Static state-1 raw-load plan: Disk 1 +0x" << std::hex
            << state1_plan.source_offset << " +0x" << state1_plan.byte_count
            << " -> RAM 0x" << state1_plan.destination << std::dec << " in "
            << state1_plan.requests.size() << " original reads; SHA-256 "
            << eon::to_hex(eon::sha256(state1_bytes))
            << " (not selected or interpreted at runtime)\n";
        const auto state1_skipped_ascii = eon::parse_deuteros_atari_state1_skipped_ascii_block(
            state1_bytes, state1_plan);
        std::cout << "          State-1 skipped ASCII evidence: Disk 1 +0x" << std::hex
            << state1_plan.source_offset + state1_skipped_ascii.branch_relative_offset
            << " BRA.W displacement 0x" << static_cast<std::uint16_t>(
                state1_skipped_ascii.branch_displacement)
            << "; block +0x" << state1_plan.source_offset
                + state1_skipped_ascii.ascii_relative_offset
            << " +0x" << state1_skipped_ascii.ascii_byte_count << std::dec << " ("
            << state1_skipped_ascii.printable_run_count << " printable runs), SHA-256 "
            << state1_skipped_ascii.ascii_sha256
            << "; marker offsets +0x" << std::hex
            << state1_skipped_ascii.presentation_marker_offset << "/+0x"
            << state1_skipped_ascii.game_name_marker_offsets[0] << "/+0x"
            << state1_skipped_ascii.game_name_marker_offsets[1] << std::dec
            << " (preservation metadata only; never rendered, translated, or interpreted)\n";
        const auto materialize_raw_range = [&disk1](const auto& plan) {
            std::vector<std::uint8_t> bytes;
            bytes.reserve(plan.byte_count);
            for (const auto& request : plan.requests) {
                const auto chunk = disk1.read_sectors(request.track, request.side,
                    request.first_sector, request.sector_count);
                bytes.insert(bytes.end(), chunk.begin(), chunk.end());
            }
            return bytes;
        };
        const auto state5_first_bytes = materialize_raw_range(state5_plan.first_read);
        const auto state5_second_bytes = materialize_raw_range(state5_plan.second_read);
        std::cout << "          Static vector-5 raw-load plans: Disk 1 +0x" << std::hex
            << state5_plan.first_read.source_offset << " +0x" << state5_plan.first_read.byte_count
            << " -> RAM 0x" << state5_plan.first_read.destination << std::dec << " in "
            << state5_plan.first_read.requests.size() << " original reads; SHA-256 "
            << eon::to_hex(eon::sha256(state5_first_bytes)) << "; copy RAM 0x" << std::hex
            << state5_plan.copy_source << " +0x" << state5_plan.copy_byte_count << " -> 0x"
            << state5_plan.copy_destination << std::dec << "; Disk 1 +0x" << std::hex
            << state5_plan.second_read.source_offset << " +0x" << state5_plan.second_read.byte_count
            << " -> RAM 0x" << state5_plan.second_read.destination << std::dec << " in "
            << state5_plan.second_read.requests.size() << " original reads; SHA-256 "
            << eon::to_hex(eon::sha256(state5_second_bytes))
            << " (not selected or interpreted at runtime)\n";
        std::cout << "          Static vector 5: raw args (RAM 0x" << std::hex
            << dispatch.state5_first_destination << ", 0x" << dispatch.state5_first_byte_count
            << " bytes, reader 0x" << dispatch.state5_first_reader_argument
            << "), copy 0x" << dispatch.state5_copy_source << " -> 0x"
            << dispatch.state5_copy_destination << " +0x" << dispatch.state5_copy_byte_count
            << ", then reader 0x" << dispatch.state5_second_reader_argument << std::dec << '\n';
        std::cout << "          Vector-5 return: stage +0x" << std::hex
            << state5_return.branch_offset << " BRA.W " << std::dec
            << state5_return.branch_displacement << " -> stage +0x" << std::hex
            << state5_return.branch_target_offset << "; SHA-256 " << state5_return.branch_sha256
            << "; tail MOVE.W $" << state5_return.state_word_address << ",D0 / RTS at +0x"
            << state5_return.dispatcher_tail_offset << "; SHA-256 "
            << state5_return.dispatcher_tail_sha256 << std::dec
            << " (validated only; no state selection, raw read, or XBIOS execution)\n";
        std::cout << "          Supervisor callback boundary: stage +0x" << std::hex
            << supervisor_callback.callsite_offset << " pushes callback 0x"
            << supervisor_callback.callback_address << " and XBIOS selector 0x"
            << supervisor_callback.xbios_selector << ", TRAP #14 0x"
            << supervisor_callback.trap_opcode << "; callsite SHA-256 "
            << supervisor_callback.callsite_sha256 << "; callback +0x"
            << supervisor_callback.callback_offset << " loads (A7),D0, sets A7=0x"
            << supervisor_callback.callback_stack_address << ", pushes D0, RTS; SHA-256 "
            << supervisor_callback.callback_sha256 << std::dec
            << " (ABI boundary only; no XBIOS/callback execution)\n";
        std::cout << "          Static raw-reader wrapper: stage +0x" << std::hex
            << raw_reader_wrapper.wrapper_offset << " +0x" << raw_reader_wrapper.wrapper_byte_count
            << " BSR.W -> +0x" << raw_reader_wrapper.raw_reader_bsr_target_offset
            << "; status-word branch -> +0x" << raw_reader_wrapper.nonzero_branch_target_offset
            << "; loop -> +0x" << raw_reader_wrapper.loop_branch_target_offset
            << "; SHA-256 " << raw_reader_wrapper.wrapper_sha256 << std::dec
            << " (layout only; no status value, XBIOS result, or reachability inferred)\n";
        std::cout << "          Raw-reader call layout: stage +0x" << std::hex
            << raw_reader_call.routine_offset << " +0x" << raw_reader_call.routine_byte_count
            << " count cap 0x" << raw_reader_call.count_compare_immediate
            << "; ABI opcode 0x" << raw_reader_call.abi_call_opcode << " selector 0x"
            << raw_reader_call.abi_selector << "; post-call store $"
            << raw_reader_call.post_call_store_address << ", RTS; SHA-256 "
            << raw_reader_call.routine_sha256 << std::dec
            << " (static bytes only; no ABI result or disk operation is performed)\n";
        std::cout << "          Direct dispatch-table callees: slots "
            << direct_vector_callees.distinct_callees[0].vector_slot << "/"
            << direct_vector_callees.distinct_callees[1].vector_slot << "/"
            << direct_vector_callees.distinct_callees[2].vector_slot << " at stage +0x"
            << std::hex << direct_vector_callees.distinct_callees[0].stage_offset << "/+0x"
            << direct_vector_callees.distinct_callees[1].stage_offset << "/+0x"
            << direct_vector_callees.distinct_callees[2].stage_offset << "; hashes "
            << direct_vector_callees.distinct_callees[0].sha256 << "/"
            << direct_vector_callees.distinct_callees[1].sha256 << "/"
            << direct_vector_callees.distinct_callees[2].sha256 << std::dec
            << "; alias BRA +0x" << std::hex << direct_vector_callees.alias_branch_offset
            << " -> +0x" << direct_vector_callees.alias_branch_target_offset << std::dec
            << " (table/linkage bytes only; no index, call, return, or state interpretation)\n";
        std::cout << "          Direct-vector literal transfer loop: stage +0x" << std::hex
            << direct_vector_transfer_loop.loop_block_offset << " +0x"
            << direct_vector_transfer_loop.loop_block_byte_count << " writes literal A0 0x"
            << direct_vector_transfer_loop.destination_pointer << " and A1 0x"
            << direct_vector_transfer_loop.source_pointer << "; DBF displacement " << std::dec
            << direct_vector_transfer_loop.dbf_displacement << " -> stage +0x" << std::hex
            << direct_vector_transfer_loop.dbf_target_offset << "; SHA-256 "
            << direct_vector_transfer_loop.loop_block_sha256 << std::dec
            << " (byte layout only; no vector selection, transfer, call, return, or media behavior)\n";
        std::cout << "          Direct-vector transfer tail: stage +0x" << std::hex
            << direct_vector_transfer_tail.tail_offset << " +0x"
            << direct_vector_transfer_tail.tail_byte_count << " BSR.W -> +0x"
            << direct_vector_transfer_tail.range_wrapper_bsr_target_offset << "; BRA.W -> +0x"
            << direct_vector_transfer_tail.dispatcher_return_bra_target_offset << "; SHA-256 "
            << direct_vector_transfer_tail.tail_sha256 << std::dec
            << " (static bytes only; no vector selection, call, return, transfer, or media behavior)\n";
        std::cout << "          State-selection layout: stage +0x" << std::hex
            << state_selection.input_capture_offset << " loads RAM $"
            << state_selection.source_longword_address << " then stores its low word at $"
            << state_selection.state_word_address << "; SHA-256 "
            << state_selection.input_capture_sha256 << "; later stage +0x"
            << state_selection.table_lookup_offset << " reads that word, shifts it by two, loads"
            << " table $" << state_selection.table_base_address << ", then JSR (A1); SHA-256 "
            << state_selection.table_lookup_sha256 << std::dec
            << " (static layout only; no input value, service return, bounds check, or vector"
            << " selection inferred)\n";
        std::cout << "          Post-indirect-call layout: stage +0x" << std::hex
            << state_selection_continuation.continuation_offset << " saves D1, advances D4 by 0x"
            << state_selection_continuation.raw_reader_argument_advance_bytes
            << ", transfers D2 to D7, then BSR.W -> +0x"
            << state_selection_continuation.raw_reader_wrapper_target_offset << "; SHA-256 "
            << state_selection_continuation.continuation_sha256 << std::dec
            << " (layout only; no callee return, register meaning, raw read, or reachability inferred)\n";
        std::cout << "          Post-raw-reader continuation: stage +0x" << std::hex
            << second_callee_continuation.continuation_offset << " -> XBIOS selector 0x"
            << second_callee_continuation.trap_selector << " with pointer 0x"
            << second_callee_continuation.trap_argument_address << "; SHA-256 "
            << second_callee_continuation.continuation_sha256 << "; post-service layout copies "
            << std::dec << (static_cast<std::uint32_t>(second_callee_continuation.copy_loop_counter) + 1U)
            << " longwords from RAM 0x" << std::hex << second_callee_continuation.copy_source
            << " through pointer at 0x" << second_callee_continuation.copy_destination_pointer_address
            << " then RTS (not reached or executed without raw-reader/service return evidence)\n";
    }
    std::cout << "          Disk 2 boot continuation: "
        << (continuation.killer_boot_signature ? "KILLER_BOOT signature" : "unclassified")
        << ", branch 0x" << std::hex << continuation.boot_branch_target << std::dec << '\n';
    if (continuation.has_killer_boot_continuation_profile) {
        const auto killer_handoff = eon::parse_deuteros_atari_killer_boot_handoff(
            disk2.read_sectors(0, 0, 1, 1), continuation);
        std::cout << "          Disk 2 relocated continuation: boot +0x" << std::hex
            << continuation.killer_boot_vector_source_offset << " +0x"
            << continuation.killer_boot_relocated_byte_count << " -> RAM 0x"
            << continuation.killer_boot_vector_destination << "; SHA-256 "
            << continuation.killer_boot_relocated_sha256 << "; clears 0x"
            << continuation.killer_boot_clear_start << " in 0x"
            << continuation.killer_boot_clear_stride << "-byte blocks (not executed)" << std::dec
            << '\n';
        std::cout << "          Disk 2 KILLER_BOOT handoff: setup +0x" << std::hex
            << killer_handoff.setup_offset << " +0x" << killer_handoff.setup_byte_count
            << " copies boot +0x" << killer_handoff.source_offset << " +0x"
            << killer_handoff.byte_count << " -> RAM 0x" << killer_handoff.destination
            << "; JMP 0x" << killer_handoff.continuation_address << " enters relocated +0x"
            << killer_handoff.continuation_relocated_offset << " opcode 0x"
            << killer_handoff.continuation_first_opcode << "; copied vector +0x"
            << killer_handoff.vector_jump_relocated_offset << " is JMP (A0) through RAM $"
            << killer_handoff.vector_jump_pointer_address << "; setup/relocated SHA-256 "
            << killer_handoff.setup_sha256 << "/" << killer_handoff.relocated_sha256 << std::dec
            << " (byte-proven relocation only; neither vector cell nor path executes)\n";
    }
    std::cout << "          Atari ST trace boundary: next evidence must identify the XBIOS Floprd result, "
        << "callback entry/return frame, dispatch word at RAM 0x1eaa, and selected vector D1/D2 "
        << "returns. Reported raw-load plans are not performed and no Amiga or synthetic screen is used.\n";
}

std::optional<MillenniumDosLaunchAssets> load_millennium_launch_assets(
    const std::vector<eon::ReleaseArchive>& releases,
    const std::optional<eon::Platform> requested_platform) {
    // The following executable and library profile is verified solely for
    // English DOS media. Never substitute it when the caller explicitly chose
    // the Amiga or Atari ST release.
    if (requested_platform && *requested_platform != eon::Platform::dos) return std::nullopt;
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
    const auto release = std::find_if(releases.begin(), releases.end(), [](const auto& candidate) {
        return candidate.game == eon::Game::millennium && candidate.platform == eon::Platform::dos
            && candidate.language == "en";
    });
    const auto spanish_release = std::find_if(releases.begin(), releases.end(), [](const auto& candidate) {
        return candidate.game == eon::Game::millennium && candidate.platform == eon::Platform::dos
            && candidate.language == "es";
    });
    if (release == releases.end() && spanish_release == releases.end()) return std::nullopt;
    try {
        if (release == releases.end()) {
            // The Spanish edition is an original FAT12 floppy. Its P00
            // resource is independently verified, but no executable handoff
            // ABI has been recovered, so expose only this authentic title.
            constexpr auto spanish_image_sha256 =
                "1cb7d399ab22110317b1c7486a575c00895f12a17268d0c984ac264a5695961d";
            const auto image = eon::extract_verified_release_asset(*spanish_release, spanish_image_sha256);
            if (!image) return std::nullopt;
            const eon::Fat12Disk disk(*image);
            const auto* title_entry = disk.find("TITLE.LIB");
            if (!title_entry) return std::nullopt;
            const eon::MillenniumDosLib title_lib(disk.read(*title_entry));
            const auto* p00 = title_lib.find("P00");
            if (!p00) return std::nullopt;
            const auto resource = title_lib.read(*p00);
            const auto bitmap = eon::decode_millennium_dos_bitmap(resource);
            const auto palette = eon::decode_millennium_dos_palette(resource, bitmap);
            return MillenniumDosLaunchAssets{
                .title = {bitmap.width, bitmap.height,
                    {eon::colorize_millennium_dos_bitmap(bitmap, palette)}},
                .language = "es",
                .gx_canvas = std::nullopt,
                .title_flow = std::nullopt,
                .game_flow = std::nullopt,
                .ega_video_driver = std::nullopt,
                .mcga_video_driver = std::nullopt,
                .initial_save = std::nullopt,
            };
        }
        const auto bytes = eon::extract_verified_release_asset(*release, title_lib_sha256);
        if (!bytes) return std::nullopt;
        const eon::MillenniumDosLib title_lib(*bytes);
        const auto* p00 = title_lib.find("P00");
        if (!p00) return std::nullopt;
        const auto resource = title_lib.read(*p00);
        const auto bitmap = eon::decode_millennium_dos_bitmap(resource);
        const auto palette = eon::decode_millennium_dos_palette(resource, bitmap);
        const auto gx_bytes = eon::extract_verified_release_asset(*release, gx_lib_sha256);
        const auto titles = eon::extract_verified_release_asset(*release, titles_sha256);
        const auto launcher = eon::extract_verified_release_asset(*release, launcher_sha256);
        const auto game = eon::extract_verified_release_asset(*release, game_sha256);
        const auto initial_save = eon::extract_verified_release_asset(*release, initial_save_sha256);
        const auto ega640 = eon::extract_verified_release_asset(*release, ega640_sha256);
        const auto mcga = eon::extract_verified_release_asset(*release, mcga_sha256);
        if (!gx_bytes || !titles || !launcher || !game || !initial_save || !ega640 || !mcga) {
            return std::nullopt;
        }
        const auto gx_canvas = eon::parse_millennium_dos_gameplay_screen(*gx_bytes);
        return MillenniumDosLaunchAssets{
            .title = {bitmap.width, bitmap.height,
                {eon::colorize_millennium_dos_bitmap(bitmap, palette)}},
            .language = "en",
            .gx_canvas = PreviewAnimation{gx_canvas.canvas.width, gx_canvas.canvas.height, {gx_canvas.rgba}},
            .title_flow = eon::parse_millennium_dos_title_flow(*titles, *launcher),
            .game_flow = eon::parse_millennium_dos_game_flow(*game),
            .ega_video_driver = eon::parse_millennium_dos_video_driver(*ega640,
                eon::MillenniumDosVideoDriverKind::ega640),
            .mcga_video_driver = eon::parse_millennium_dos_video_driver(*mcga,
                eon::MillenniumDosVideoDriverKind::mcga),
            .initial_save = eon::MillenniumDosSaveSession(*initial_save),
        };
    } catch (const std::exception& error) {
        std::cerr << "Unable to load Millennium DOS launch assets: " << error.what() << '\n';
        return std::nullopt;
    }
}

std::unique_ptr<eon::DeuterosAmigaOpening> load_deuteros_opening(
    const std::vector<eon::ReleaseArchive>& releases,
    std::optional<eon::Platform> requested_platform) {
    if (!eon::deuteros_amiga_opening_supported(requested_platform)) return {};
    constexpr auto clean_system_adf =
        "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38";
    const auto release = std::find_if(releases.begin(), releases.end(), [](const auto& candidate) {
        return candidate.game == eon::Game::deuteros && candidate.platform == eon::Platform::amiga;
    });
    if (release == releases.end()) return {};
    try {
        const auto image = eon::extract_verified_release_asset(*release, clean_system_adf);
        if (!image) return {};
        return std::make_unique<eon::DeuterosAmigaOpening>(std::move(*image));
    } catch (const std::exception& error) {
        std::cerr << "Unable to start Deuteros opening: " << error.what() << '\n';
        return {};
    }
}

std::unique_ptr<eon::MillenniumAtariBootstrapSession> load_millennium_atari_bootstrap(
    const std::vector<eon::ReleaseArchive>& releases,
    std::optional<eon::Platform> requested_platform) {
    if (requested_platform != eon::Platform::atari_st) return {};
    constexpr auto equinox_disk_sha256 =
        "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7";
    const auto release = std::find_if(releases.begin(), releases.end(), [](const auto& candidate) {
        return candidate.game == eon::Game::millennium
            && candidate.platform == eon::Platform::atari_st;
    });
    if (release == releases.end()) return {};
    try {
        const auto image = eon::extract_verified_release_asset(*release, equinox_disk_sha256);
        if (!image) return {};
        const eon::Fat12Disk disk(*image);
        const auto* executable = disk.find("MILENIUM.TOS");
        if (!executable) return {};
        return std::make_unique<eon::MillenniumAtariBootstrapSession>(
            disk, disk.read(*executable));
    } catch (const std::exception& error) {
        std::cerr << "Unable to start Millennium Atari ST bootstrap: " << error.what() << '\n';
        return {};
    }
}

std::unique_ptr<eon::MillenniumAmigaBootstrapSession> load_millennium_amiga_bootstrap(
    const std::vector<eon::ReleaseArchive>& releases,
    std::optional<eon::Platform> requested_platform) {
    if (requested_platform != eon::Platform::amiga) return {};
    constexpr auto defjam_adf_sha256 =
        "8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c";
    const auto release = std::find_if(releases.begin(), releases.end(), [](const auto& candidate) {
        return candidate.game == eon::Game::millennium && candidate.platform == eon::Platform::amiga;
    });
    if (release == releases.end()) return {};
    try {
        const auto image = eon::extract_verified_release_asset(*release, defjam_adf_sha256);
        if (!image) return {};
        return std::make_unique<eon::MillenniumAmigaBootstrapSession>(std::move(*image));
    } catch (const std::exception& error) {
        std::cerr << "Unable to start Millennium Amiga bootstrap: " << error.what() << '\n';
        return {};
    }
}

std::unique_ptr<eon::DeuterosAtariBootstrapSession> load_deuteros_atari_bootstrap(
    const std::vector<eon::ReleaseArchive>& releases,
    std::optional<eon::Platform> requested_platform) {
    if (requested_platform != eon::Platform::atari_st) return {};
    // This is the supplied Replicants Disk 1 whose boot code contains the
    // verified raw-stage sequence. Other protected ST disks remain detected,
    // but are never silently substituted for this bounded path.
    constexpr auto replicants_disk1_sha256 =
        "aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee";
    const auto release = std::find_if(releases.begin(), releases.end(), [](const auto& candidate) {
        return candidate.game == eon::Game::deuteros
            && candidate.platform == eon::Platform::atari_st;
    });
    if (release == releases.end()) return {};
    try {
        const auto image = eon::extract_verified_release_asset(*release, replicants_disk1_sha256);
        if (!image) return {};
        return std::make_unique<eon::DeuterosAtariBootstrapSession>(std::move(*image));
    } catch (const std::exception& error) {
        std::cerr << "Unable to start Deuteros Atari ST bootstrap: " << error.what() << '\n';
        return {};
    }
}

// Modern packs are intentionally a report-only preservation boundary.  This
// routine does not retain a pack object beyond inspection, nor does it create
// a default directory, decode an asset, or give a renderer any pack choice.
void report_modern_asset_packs(const std::filesystem::path& root,
                               const std::vector<eon::ReleaseArchive>& inspected_releases) {
    std::cout << "MODERN PACKS  read-only admission report; no pack is selected or rendered\n";
    std::error_code error;
    const auto status = std::filesystem::symlink_status(root, error);
    if (error || std::filesystem::is_symlink(status) || !std::filesystem::is_directory(status)) {
        std::cout << "MODERN PACK ROOT REJECTED  must be an existing non-symlink directory: "
            << root << '\n';
        return;
    }
    const auto validations = eon::discover_modern_asset_packs(root);
    if (validations.empty()) {
        std::cout << "MODERN PACKS  no direct-child pack.eonmodern candidates\n";
        return;
    }
    for (const auto& validation : validations) {
        if (!validation.accepted()) {
            std::cout << "MODERN PACK REJECTED  " << validation.manifest_path << '\n'
                << "          " << validation.error << '\n';
            continue;
        }
        const auto& pack = validation.pack;
        const bool source_is_inspected = std::any_of(inspected_releases.begin(), inspected_releases.end(),
            [&pack](const eon::ReleaseArchive& release) {
                return release.game == pack.game && release.platform == pack.platform
                    && release.sha256 == pack.source_release_sha256;
            });
        if (!source_is_inspected) {
            std::cout << "MODERN PACK REJECTED  " << validation.manifest_path << '\n'
                << "          pack source is not one of this inspection's reverified original releases\n";
            continue;
        }
        std::cout << "MODERN PACK ELIGIBLE  " << pack.id << " " << pack.version << '\n'
            << "          " << eon::name(pack.game) << " / " << eon::name(pack.platform)
            << " / source " << pack.source_release_sha256 << '\n'
            << "          " << pack.provenance << "; " << pack.assets.size()
            << " hash-verified external assets; admission only\n";
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
    const auto translator = eon::Translator::from_language(request.language,
        argc > 0 ? std::filesystem::path(argv[0]) : std::filesystem::path{});
    active_translator = &translator;
    const auto tr = [&translator](std::string_view message) {
        return std::string(translator.translate(message));
    };
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
    if (request.verify_game || request.inspect_data || request.game || request.reference_trace) {
        while (!scanner.advance(64)) {
        }
        releases = scanner.releases();
        if (releases.empty()) {
            std::cerr << "No recognised original release archives found.\n";
            return 3;
        }
    }
    if (request.reference_trace) {
        const auto validation = eon::validate_reference_trace(*request.reference_trace, releases,
            *request.game, *request.platform);
        if (!validation.trace) {
            std::cerr << "Reference trace rejected: " << validation.error << '\n';
            return 6;
        }
        const auto& trace = *validation.trace;
        try {
            // A trace is evidence about one immutable release. Re-read and
            // hash the current archive before reporting it so a post-scan
            // replacement cannot inherit the earlier admission result.
            eon::verify_release_archive(trace.source_release);
        } catch (const std::exception& error) {
            std::cerr << "Reference trace rejected: source release no longer verifies: "
                << error.what() << '\n';
            return 6;
        }
        std::cout << "REFERENCE TRACE VERIFIED  provenance-only; no replay performed\n"
            << "          " << eon::name(trace.source_release.game) << " / "
            << eon::name(trace.source_release.platform) << " / " << trace.source_release.language << '\n'
            << "          source " << trace.source_release.sha256 << '\n'
            << "          events " << trace.event_sha256 << " (" << trace.event_count << " ordered events)\n"
            << "          capture " << trace.capture_start_utc << " to " << trace.capture_end_utc << '\n'
            << "          emulator " << trace.emulator_name << " " << trace.emulator_version << '\n';
        if (!trace.adapter.empty()) {
            std::cout << "          adapter " << trace.adapter << " (";
            if (trace.adapter == "deuteros-atari-st-boot-v1") {
                std::cout << trace.adapter_trap_count << " TRAP, " << trace.adapter_callback_count
                    << " callback, " << trace.adapter_frame_count << " frame, "
                    << trace.adapter_state_count << " state, " << trace.adapter_table_count
                    << " table, " << trace.adapter_raw_reader_count
                    << " raw-reader observations; diagnostics only)\n";
            } else if (trace.adapter == "millennium-amiga-en-defjam-bootstrap-v1") {
                std::cout << trace.adapter_cpu_count << " CPU handoff observations; diagnostics only)\n";
            } else if (trace.adapter == "deuteros-amiga-en-title-stage-v1") {
                std::cout << trace.adapter_exec_count << " Exec, " << trace.adapter_open_library_count
                    << " OpenLibrary, " << trace.adapter_graphics_count << " graphics, "
                    << trace.adapter_custom_register_count << " custom-register, "
                    << trace.adapter_callback_count << " callback observations; diagnostics only)\n";
            } else {
                std::cout << trace.adapter_interrupt_count << " interrupt, " << trace.adapter_file_count
                    << " file, " << trace.adapter_exec_count << " EXEC observations; diagnostics only)\n";
            }
        }
        return 0;
    }
    if (request.verify_game || request.inspect_data) {
        if (request.inspect_data) {
            std::cout << "INSPECTION  read-only provenance scan; original media stays in place\n";
        }
        bool found = false;
        std::vector<eon::ReleaseArchive> inspected_releases;
        for (const auto& release : releases) {
            if (request.verify_game && release.game != *request.verify_game) continue;
            if (request.inspect_data && request.game && release.game != *request.game) continue;
            if (request.inspect_data && request.platform && release.platform != *request.platform) continue;
            try {
                eon::verify_release_archive(release);
            } catch (const std::exception& error) {
                std::cerr << "Recognised release changed after scan; refusing to inspect it: "
                    << error.what() << '\n';
                return 6;
            }
            found = true;
            inspected_releases.push_back(release);
            std::cout << "VERIFIED  " << eon::name(release.game) << " / "
                << eon::name(release.platform) << " / " << release.language << '\n'
                << "          " << release.sha256 << '\n'
                << "          " << release.path << '\n';
            report_recovery_map(release);
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
                && release.platform == eon::Platform::dos) {
                report_millennium_dos(release);
            }
            if (release.game == eon::Game::millennium
                && release.platform == eon::Platform::atari_st
                && release.language == "en") {
                report_millennium_atari_st(release);
            }
        }
        if (request.modern_pack_root) {
            report_modern_asset_packs(*request.modern_pack_root, inspected_releases);
        }
        if (request.inspect_data) {
            const auto& report = scanner.report();
            std::cout << "SCAN SUMMARY  " << report.candidates << " candidates; "
                << report.size_candidates << " manifest-size matches; "
                << report.hashed_candidates << " hashed; "
                << report.verified_occurrences << " verified occurrences; "
                << releases.size() << " unique releases; "
                << report.duplicate_occurrences << " duplicate occurrences; "
                << report.unreadable_candidates << " unreadable candidates\n";
        }
        if (!found) {
            if (request.inspect_data) {
                std::cerr << "No recognised original release matches the requested inspection filters.\n";
            } else {
                std::cerr << "No recognised original release matches the requested verification game.\n";
            }
        }
        return found ? 0 : 5;
    }
    if (request.game && !eon::release_available(releases, *request.game, request.platform)) {
        std::cerr << "Requested original release is not present for the selected platform. "
                     "Use --inspect to list hash-recognised releases; no platform fallback was selected.\n";
        return 4;
    }
    // A CLI platform request is fixed.  The start menu can otherwise choose
    // among the hash-verified platform releases it has actually discovered.
    std::optional<eon::Platform> active_platform = request.platform;
    auto millennium_assets = load_millennium_launch_assets(releases, active_platform);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Project Eon", request.display.width, request.display.height,
            SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        std::cerr << "SDL_CreateWindowAndRenderer failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderLogicalPresentation(renderer, 1280, 720, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(renderer, 1);
    if (const auto font_directory = find_font_directory()) {
        active_text_renderer = eon::UnicodeTextRenderer::create(renderer, *font_directory);
    }

    std::array<Card, 2> cards{{
        {eon::Game::millennium, "MILLENNIUM 2.2", "RETURN TO EARTH", "millennium.png", {64, 170, 552, 310}},
        {eon::Game::deuteros, "DEUTEROS", "THE NEXT MILLENNIUM", "deuteros.png", {664, 170, 552, 310}},
    }};
    for (auto& card : cards) card.texture = load_card(renderer, card.filename);
    std::array<PlatformCard, 3> platform_cards{{
        {eon::Platform::dos, "DOS", "dos-platform-v1.png", {64, 188, 352, 308}},
        {eon::Platform::amiga, "AMIGA", "amiga-platform-v1.png", {464, 188, 352, 308}},
        {eon::Platform::atari_st, "ATARI ST", "atari-st-platform-v1.png", {864, 188, 352, 308}},
    }};
    for (auto& card : platform_cards) card.texture = load_card(renderer, card.filename);
    std::array<ProfileCard, 3> profile_cards{{
        {ProfileChoice::original, "ORIGINAL", "PRESERVATION PROFILE", "original-profile-v1.png", {64, 188, 352, 308}},
        {ProfileChoice::modern, "MODERN", "ENHANCED PROFILE", "modern-profile-v1.png", {464, 188, 352, 308}},
        {ProfileChoice::custom, "CUSTOM", "TUNE MODERN SETTINGS", "custom-profile-v1.png", {864, 188, 352, 308}},
    }};
    for (auto& card : profile_cards) card.texture = load_card(renderer, card.filename);
    std::unique_ptr<eon::DeuterosAmigaOpening> deuteros_opening;
    std::unique_ptr<eon::DeuterosAmigaPaulaMixer> deuteros_paula;
    SDL_AudioStream* deuteros_audio_stream = nullptr;
    SDL_Texture* preview_texture = nullptr;
    // These transient textures are derived from decoded original pixels only.
    // They have no on-disk representation and are selected exclusively by
    // Modern at render time.
    SDL_Texture* modern_preview_texture = nullptr;
    // The opening VM advances at a verified 20 ms cadence. Retain its
    // decoded frame only until that cadence produces a new source frame, so
    // presentation refreshes never repeatedly colorize or reconstruct the
    // same original pixels. This remains renderer-only process memory.
    std::optional<std::vector<std::uint8_t>> deuteros_preview_rgba;
    std::optional<std::uint64_t> deuteros_preview_source_tick;
    std::optional<std::uint64_t> deuteros_modern_preview_attempted_tick;
    std::optional<std::uint64_t> deuteros_modern_preview_source_tick;
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
    SDL_Texture* millennium_modern_preview_texture = nullptr;
    SDL_Texture* millennium_gx_canvas_texture = nullptr;
    std::unique_ptr<eon::MillenniumAtariBootstrapSession> millennium_atari_session;
    std::unique_ptr<eon::MillenniumAmigaBootstrapSession> millennium_amiga_session;
    const auto discard_millennium_assets = [&] {
        if (millennium_preview_texture) SDL_DestroyTexture(millennium_preview_texture);
        if (millennium_modern_preview_texture) SDL_DestroyTexture(millennium_modern_preview_texture);
        if (millennium_gx_canvas_texture) SDL_DestroyTexture(millennium_gx_canvas_texture);
        millennium_preview_texture = nullptr;
        millennium_modern_preview_texture = nullptr;
        millennium_gx_canvas_texture = nullptr;
        millennium_assets.reset();
        millennium_atari_session.reset();
        millennium_amiga_session.reset();
    };
    const auto create_millennium_textures = [&] {
        if (!millennium_assets || millennium_preview_texture) return;
        millennium_preview_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STATIC, millennium_assets->title.width, millennium_assets->title.height);
        if (millennium_preview_texture) {
            SDL_UpdateTexture(millennium_preview_texture, nullptr,
                millennium_assets->title.rgba_frames.front().data(), millennium_assets->title.width * 4);
        }
        if (millennium_assets->gx_canvas) {
            millennium_gx_canvas_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                SDL_TEXTUREACCESS_STATIC, millennium_assets->gx_canvas->width,
                millennium_assets->gx_canvas->height);
            if (millennium_gx_canvas_texture) {
                SDL_UpdateTexture(millennium_gx_canvas_texture, nullptr,
                    millennium_assets->gx_canvas->rgba_frames.front().data(),
                    millennium_assets->gx_canvas->width * 4);
            }
        }
    };
    const auto millennium_texture_for = [&](const bool reconstruct) {
        if (!millennium_assets || !millennium_preview_texture || !reconstruct) {
            return millennium_preview_texture;
        }
        if (!millennium_modern_preview_texture) {
            const auto& title = millennium_assets->title;
            if (title.rgba_frames.empty()) return millennium_preview_texture;
            const auto enhanced = eon::reconstruct_rgba_scale2x(
                title.rgba_frames.front(), title.width, title.height);
            millennium_modern_preview_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                SDL_TEXTUREACCESS_STATIC, enhanced.width, enhanced.height);
            if (!millennium_modern_preview_texture || !SDL_UpdateTexture(
                    millennium_modern_preview_texture, nullptr, enhanced.rgba.data(), enhanced.width * 4)) {
                std::cerr << "Unable to upload transient Modern Millennium texture: " << SDL_GetError() << '\n';
                SDL_DestroyTexture(millennium_modern_preview_texture);
                millennium_modern_preview_texture = nullptr;
                return millennium_preview_texture;
            }
        }
        return millennium_modern_preview_texture;
    };
    create_millennium_textures();
    // Menu scanning is deliberately incremental.  Do not lock Millennium's
    // verified DOS resources to the empty pre-scan release list: load them
    // only when the scanner has actually found the selected original media.
    const auto load_millennium_assets_if_available = [&] {
        if (millennium_assets) return;
        millennium_assets = load_millennium_launch_assets(releases, active_platform);
        create_millennium_textures();
    };

    Screen screen = request.game ? Screen::launching : Screen::menu;
    LauncherPage launcher_page = LauncherPage::games;
    eon::Game selected = request.game.value_or(eon::Game::millennium);
    int focused = 0;
    int focused_platform_card = 0;
    int focused_profile_card = 0;
    bool custom_profile_ready = false;
    bool show_scanner = false;
    bool show_modern_graphics_settings = false;
    std::uint64_t deuteros_last_tick = SDL_GetTicks();
    bool deuteros_input_pressed = false;
    std::optional<std::uint32_t> deuteros_title_resource;
    std::unique_ptr<eon::DeuterosAtariBootstrapSession> deuteros_atari_session;
    std::unique_ptr<eon::MillenniumDosTitleSession> millennium_title_session;
    // This evidence-only object is intentionally never constructed by the
    // launcher until a genuine DOS return/startup path is recovered.
    std::unique_ptr<eon::MillenniumDosGameSession> millennium_game_session;
    std::size_t millennium_state_page = 0;
    const auto focus_menu_card = [&](const int next_focus) {
        focused = next_focus;
        if (request.platform) return;
        const auto next_platform = eon::select_available_platform(
            releases, cards[static_cast<std::size_t>(focused)].game, active_platform);
        if (next_platform != active_platform) {
            active_platform = next_platform;
            discard_millennium_assets();
        }
    };
    const auto start_millennium_title = [&] {
        millennium_atari_session = load_millennium_atari_bootstrap(releases, active_platform);
        millennium_amiga_session = load_millennium_amiga_bootstrap(releases, active_platform);
        millennium_title_session.reset();
        if (active_platform == eon::Platform::atari_st || active_platform == eon::Platform::amiga) return;
        load_millennium_assets_if_available();
        if (millennium_assets && millennium_assets->title_flow) {
            millennium_title_session = std::make_unique<eon::MillenniumDosTitleSession>(
                *millennium_assets->title_flow);
            millennium_state_page = 0;
        }
    };
    const auto start_deuteros = [&] {
        deuteros_atari_session = load_deuteros_atari_bootstrap(releases, active_platform);
        deuteros_opening = load_deuteros_opening(releases, active_platform);
        create_deuteros_opening_texture();
        start_deuteros_audio();
        deuteros_last_tick = SDL_GetTicks();
        deuteros_title_resource.reset();
    };
    const auto launch_menu_selection = [&] {
        const auto game = cards[static_cast<std::size_t>(focused)].game;
        if (!eon::release_available(releases, game, active_platform)) return;
        selected = game;
        screen = Screen::launching;
        if (selected == eon::Game::millennium) start_millennium_title();
        if (selected == eon::Game::deuteros) start_deuteros();
    };
    const auto choose_platform_card = [&](const int index) {
        focused_platform_card = std::clamp(index, 0, static_cast<int>(platform_cards.size() - 1U));
        const auto platform = platform_cards[static_cast<std::size_t>(focused_platform_card)].platform;
        const auto game = cards[static_cast<std::size_t>(focused)].game;
        if (!eon::release_available(releases, game, platform)) return false;
        if (!active_platform || *active_platform != platform) {
            active_platform = platform;
            discard_millennium_assets();
        }
        return true;
    };
    const auto choose_profile_card = [&](const ProfileChoice choice) {
        if (choice == ProfileChoice::custom) {
            // Custom is not a third runtime mode. It is the deliberate
            // launcher route for tuning Modern's renderer-only options.
            request.presentation = eon::Presentation::modern;
            custom_profile_ready = false;
            show_modern_graphics_settings = true;
            return;
        }
        request.presentation = choice == ProfileChoice::original
            ? eon::Presentation::original : eon::Presentation::modern;
        launch_menu_selection();
    };
    if (screen == Screen::launching && selected == eon::Game::millennium) {
        start_millennium_title();
    }
    if (screen == Screen::launching && selected == eon::Game::deuteros) start_deuteros();
    ModernGraphicsSettings modern_graphics_settings;
    modern_graphics_settings.output_resolution_index = output_resolution_index_for(request.display);
    modern_graphics_settings.aspect_ratio_index = request.display.aspect_ratio_index;
    const auto cycle_output_resolution = [&](const int direction) {
        const auto count = output_resolutions.size();
        const auto current = modern_graphics_settings.output_resolution_index;
        const auto next = direction < 0 ? (current + count - 1U) % count : (current + 1U) % count;
        modern_graphics_settings.output_resolution_index = next;
        const auto& resolution = output_resolutions.at(next);
        SDL_SetWindowSize(window, resolution.width, resolution.height);
    };
    const auto cycle_aspect_ratio = [&](const int direction) {
        const auto count = display_aspect_ratios.size();
        const auto current = modern_graphics_settings.aspect_ratio_index;
        modern_graphics_settings.aspect_ratio_index = direction < 0
            ? (current + count - 1U) % count : (current + 1U) % count;
    };
    const auto change_modern_graphics_option = [&](const int direction) {
        switch (modern_graphics_settings.focused_option) {
        case 0: cycle_output_resolution(direction); break;
        case 1: cycle_aspect_ratio(direction); break;
        case 2: modern_graphics_settings.pixel_reconstruction = !modern_graphics_settings.pixel_reconstruction; break;
        case 3: modern_graphics_settings.smooth_scaling = !modern_graphics_settings.smooth_scaling; break;
        case 4: modern_graphics_settings.scanlines = !modern_graphics_settings.scanlines; break;
        default: modern_graphics_settings.frame = !modern_graphics_settings.frame; break;
        }
    };
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F10 && !event.key.repeat) {
                // F10 is consumed by Project Eon's renderer chrome, never by
                // original DOS/Amiga input. It is available now and remains
                // the future in-game Modern-presentation entry point.
                request.presentation = eon::Presentation::modern;
                show_modern_graphics_settings = !show_modern_graphics_settings;
                if (!show_modern_graphics_settings && screen == Screen::menu
                    && launcher_page == LauncherPage::profiles
                    && focused_profile_card == 2) custom_profile_ready = true;
                continue;
            }
            if (show_modern_graphics_settings) {
                // This renderer-only dialog is a real input boundary. In
                // particular, Space, Enter and South/A must not leak into a
                // recovered opening or DOS availability poll behind it.
                if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                    if (event.key.key == SDLK_ESCAPE) {
                        show_modern_graphics_settings = false;
                        if (screen == Screen::menu && launcher_page == LauncherPage::profiles
                            && focused_profile_card == 2) custom_profile_ready = true;
                    } else if (event.key.key == SDLK_UP) {
                        modern_graphics_settings.focused_option =
                            (modern_graphics_settings.focused_option + 5) % 6;
                    } else if (event.key.key == SDLK_DOWN) {
                        modern_graphics_settings.focused_option =
                            (modern_graphics_settings.focused_option + 1) % 6;
                    } else if (event.key.key == SDLK_LEFT || event.key.key == SDLK_RIGHT) {
                        change_modern_graphics_option(event.key.key == SDLK_LEFT ? -1 : 1);
                    }
                } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                    if (event.gbutton.button == SDL_GAMEPAD_BUTTON_BACK) {
                        show_modern_graphics_settings = false;
                        if (screen == Screen::menu && launcher_page == LauncherPage::profiles
                            && focused_profile_card == 2) custom_profile_ready = true;
                    } else if (event.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_UP) {
                        modern_graphics_settings.focused_option =
                            (modern_graphics_settings.focused_option + 5) % 6;
                    } else if (event.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_DOWN) {
                        modern_graphics_settings.focused_option =
                            (modern_graphics_settings.focused_option + 1) % 6;
                    } else if (event.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_LEFT
                        || event.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) {
                        change_modern_graphics_option(
                            event.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_LEFT ? -1 : 1);
                    }
                }
                continue;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
                if (screen == Screen::launching && !request.game) screen = Screen::menu;
                else if (screen == Screen::menu && launcher_page != LauncherPage::games) {
                    launcher_page = launcher_page == LauncherPage::profiles
                        ? LauncherPage::platforms : LauncherPage::games;
                }
                else running = false;
            }
            if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN
                && event.gbutton.button == SDL_GAMEPAD_BUTTON_BACK) {
                if (screen == Screen::launching && !request.game) screen = Screen::menu;
                else running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F1 && !event.key.repeat) {
                request.presentation = request.presentation == eon::Presentation::original
                    ? eon::Presentation::modern : eon::Presentation::original;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat
                && screen == Screen::launching && selected == eon::Game::millennium
                && event.key.key != SDLK_ESCAPE && event.key.key != SDLK_F1 && event.key.key != SDLK_F10
                && millennium_title_session) {
                // TITLES.EXE uses DOS' non-blocking character availability
                // poll, rather than a game-specific action key.  SDL's key
                // event supplies that availability signal. Its original
                // process exit and the launcher/DOS return remain unexecuted.
                if (!millennium_title_session->handed_off()) {
                    static_cast<void>(millennium_title_session->poll_console(true));
                }
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
            if ((event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN
                    || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP)
                && screen == Screen::launching && selected == eon::Game::deuteros
                && event.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH) {
                // The sole gamepad route into the recovered Amiga opening is
                // the same physical held signal as Space/Enter. It does not
                // manufacture a title or gameplay action.
                deuteros_input_pressed = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
            }
            if (screen == Screen::menu && event.type == SDL_EVENT_KEY_DOWN
                && event.key.key == SDLK_D && !event.key.repeat) show_scanner = !show_scanner;
            if (screen == Screen::menu && event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                const bool previous = event.key.key == SDLK_LEFT || event.key.key == SDLK_UP;
                const bool next = event.key.key == SDLK_RIGHT || event.key.key == SDLK_DOWN;
                if (event.key.key == SDLK_ESCAPE && launcher_page != LauncherPage::games) {
                    launcher_page = launcher_page == LauncherPage::profiles
                        ? LauncherPage::platforms : LauncherPage::games;
                } else if (launcher_page == LauncherPage::games) {
                    if (previous || next) focus_menu_card(next ? 1 - focused : 1 - focused);
                    else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                        launcher_page = LauncherPage::platforms;
                    }
                } else if (launcher_page == LauncherPage::platforms) {
                    if (previous || next) {
                        focused_platform_card = (focused_platform_card + (previous ? 2 : 1)) % 3;
                    } else if (event.key.key == SDLK_HOME) focused_platform_card = 0;
                    else if (event.key.key == SDLK_END) focused_platform_card = 2;
                    else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                        if (choose_platform_card(focused_platform_card)) launcher_page = LauncherPage::profiles;
                    }
                } else {
                    if (previous || next) {
                        focused_profile_card = (focused_profile_card + (previous ? 2 : 1)) % 3;
                        custom_profile_ready = false;
                    } else if (event.key.key == SDLK_HOME) focused_profile_card = 0;
                    else if (event.key.key == SDLK_END) focused_profile_card = 2;
                    else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                        if (focused_profile_card == 2 && custom_profile_ready) launch_menu_selection();
                        else choose_profile_card(profile_cards[static_cast<std::size_t>(focused_profile_card)].choice);
                    }
                }
            }
            if (screen == Screen::menu && event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                const auto button = event.gbutton.button;
                if (button == SDL_GAMEPAD_BUTTON_DPAD_LEFT || button == SDL_GAMEPAD_BUTTON_DPAD_UP) {
                    if (launcher_page == LauncherPage::games) focus_menu_card(1 - focused);
                    else if (launcher_page == LauncherPage::platforms) focused_platform_card = (focused_platform_card + 2) % 3;
                    else { focused_profile_card = (focused_profile_card + 2) % 3; custom_profile_ready = false; }
                } else if (button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT || button == SDL_GAMEPAD_BUTTON_DPAD_DOWN) {
                    if (launcher_page == LauncherPage::games) focus_menu_card(1 - focused);
                    else if (launcher_page == LauncherPage::platforms) focused_platform_card = (focused_platform_card + 1) % 3;
                    else { focused_profile_card = (focused_profile_card + 1) % 3; custom_profile_ready = false; }
                } else if (button == SDL_GAMEPAD_BUTTON_SOUTH || button == SDL_GAMEPAD_BUTTON_START) {
                    if (launcher_page == LauncherPage::games) launcher_page = LauncherPage::platforms;
                    else if (launcher_page == LauncherPage::platforms) {
                        if (choose_platform_card(focused_platform_card)) launcher_page = LauncherPage::profiles;
                    } else if (focused_profile_card == 2 && custom_profile_ready) launch_menu_selection();
                    else choose_profile_card(profile_cards[static_cast<std::size_t>(focused_profile_card)].choice);
                }
            }
            if (screen == Screen::menu && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                float x = 0, y = 0;
                SDL_RenderCoordinatesFromWindow(renderer, event.button.x, event.button.y, &x, &y);
                if (launcher_page == LauncherPage::games) {
                    for (std::size_t index = 0; index < cards.size(); ++index) {
                        if (inside(cards[index].bounds, x, y)) {
                            focus_menu_card(static_cast<int>(index));
                            launcher_page = LauncherPage::platforms;
                        }
                    }
                } else if (launcher_page == LauncherPage::platforms) {
                    for (std::size_t index = 0; index < platform_cards.size(); ++index) {
                        if (inside(platform_cards[index].bounds, x, y)
                            && choose_platform_card(static_cast<int>(index))) launcher_page = LauncherPage::profiles;
                    }
                } else {
                    for (std::size_t index = 0; index < profile_cards.size(); ++index) {
                        if (!inside(profile_cards[index].bounds, x, y)) continue;
                        focused_profile_card = static_cast<int>(index);
                        if (index == 2 && custom_profile_ready) launch_menu_selection();
                        else choose_profile_card(profile_cards[index].choice);
                    }
                }
            }
        }

        if (!scanner.done()) {
            static_cast<void>(scanner.advance(show_scanner ? 32 : 1));
            releases = scanner.releases();
            if (screen == Screen::menu && !request.platform) focus_menu_card(focused);
        }
        if (screen == Screen::launching && selected == eon::Game::deuteros
            && !deuteros_opening && eon::deuteros_amiga_opening_supported(active_platform)) {
            deuteros_opening = load_deuteros_opening(releases, active_platform);
            create_deuteros_opening_texture();
            start_deuteros_audio();
            deuteros_last_tick = SDL_GetTicks();
        }
        if (screen == Screen::launching && selected == eon::Game::deuteros
            && active_platform == eon::Platform::atari_st && !deuteros_atari_session) {
            deuteros_atari_session = load_deuteros_atari_bootstrap(releases, active_platform);
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
                if (events.title_handoff) {
                    // The original opening returns to bootstrap here. Drop
                    // any host-side preview PCM rather than letting it play
                    // under the unexecuted title stage.
                    if (deuteros_audio_stream) {
                        static_cast<void>(SDL_ClearAudioStream(deuteros_audio_stream));
                    }
                    deuteros_paula.reset();
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
        if (modern) draw_modern_chrome(renderer);
        SDL_SetRenderDrawColor(renderer, 205, 225, 235, 255);

        if (screen == Screen::menu) {
            draw_text(renderer, 64, 56, tr("PROJECT EON"));
            const auto draw_card_border = [&](const SDL_FRect& bounds, const bool active, const bool enabled) {
                SDL_SetRenderDrawColor(renderer, active ? 255 : enabled ? 185 : 85,
                    active ? 195 : enabled ? 210 : 90, active ? 80 : enabled ? 135 : 90, 255);
                SDL_RenderRect(renderer, &bounds);
            };
            if (launcher_page == LauncherPage::games) {
                draw_text(renderer, 64, 82, tr("SELECT A GAME"));
                draw_text(renderer, 64, 108, tr("CLICK A GAME CARD OR USE LEFT/RIGHT, THEN ENTER"));
                for (std::size_t index = 0; index < cards.size(); ++index) {
                    auto& card = cards[index];
                    if (card.texture) SDL_RenderTexture(renderer, card.texture, nullptr, &card.bounds);
                    const bool available = eon::release_available(releases, card.game, std::nullopt);
                    draw_card_border(card.bounds, index == static_cast<std::size_t>(focused), available);
                    draw_text(renderer, card.bounds.x + 18, card.bounds.y + card.bounds.h - 45,
                        tr(card.title));
                    draw_text(renderer, card.bounds.x + 18, card.bounds.y + card.bounds.h - 25,
                        tr(card.subtitle));
                    draw_text(renderer, card.bounds.x + 18, card.bounds.y + card.bounds.h + 16,
                        available ? tr("VERIFIED ORIGINAL DATA") : scanner.done()
                        ? tr("ORIGINAL DATA NOT FOUND") : tr("SCANNING ORIGINAL DATA..."));
                }
            } else if (launcher_page == LauncherPage::platforms) {
                const auto game = cards[static_cast<std::size_t>(focused)].game;
                draw_text(renderer, 64, 82, tr("SELECT A VERIFIED PLATFORM"));
                draw_text(renderer, 64, 108, tr("UNAVAILABLE PLATFORM CARDS CANNOT START A GAME"));
                for (std::size_t index = 0; index < platform_cards.size(); ++index) {
                    auto& card = platform_cards[index];
                    const bool available = eon::release_available(releases, game, card.platform);
                    if (card.texture) SDL_RenderTexture(renderer, card.texture, nullptr, &card.bounds);
                    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                    if (!available) {
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 155);
                        SDL_RenderFillRect(renderer, &card.bounds);
                    }
                    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                    draw_card_border(card.bounds, index == static_cast<std::size_t>(focused_platform_card), available);
                    draw_text(renderer, card.bounds.x + 18, card.bounds.y + card.bounds.h - 46,
                        tr(card.title));
                    draw_text(renderer, card.bounds.x + 18, card.bounds.y + card.bounds.h - 22,
                        available ? tr("VERIFIED ORIGINAL DATA") : scanner.done()
                        ? tr("ORIGINAL DATA NOT FOUND") : tr("SCANNING ORIGINAL DATA..."));
                }
            } else {
                draw_text(renderer, 64, 82, tr("SELECT A PRESENTATION PROFILE"));
                draw_text(renderer, 64, 108, tr("ORIGINAL AND MODERN START DIRECTLY. CUSTOM TUNES MODERN FIRST."));
                for (std::size_t index = 0; index < profile_cards.size(); ++index) {
                    auto& card = profile_cards[index];
                    if (card.texture) SDL_RenderTexture(renderer, card.texture, nullptr, &card.bounds);
                    draw_card_border(card.bounds, index == static_cast<std::size_t>(focused_profile_card), true);
                    draw_text(renderer, card.bounds.x + 18, card.bounds.y + card.bounds.h - 46,
                        tr(card.title));
                    draw_text(renderer, card.bounds.x + 18, card.bounds.y + card.bounds.h - 22,
                        tr(card.subtitle));
                }
                if (focused_profile_card == 2 && custom_profile_ready) {
                    draw_text(renderer, 64, 530, tr("CUSTOM SETTINGS READY — ENTER / CLICK TO START MODERN"));
                }
            }
            if (show_scanner) {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 220);
                SDL_FRect overlay{64, 574, 1152, 104};
                SDL_RenderFillRect(renderer, &overlay);
                draw_text(renderer, 86, 596, tr("DATA SCANNER (content hashes, read-only)"));
                draw_text(renderer, 86, 620, tr("Files scanned: ") + std::to_string(scanner.scanned_count())
                    + " / " + std::to_string(scanner.candidate_count()));
                draw_text(renderer, 86, 644, scanner.done()
                    ? tr("Complete. Only hash-verified original releases are selectable.")
                    : tr("Scanning in progress. Press D to hide this progress panel."));
            }
        } else {
            draw_text(renderer, 64, 56, tr("LAUNCH REQUEST ACCEPTED"));
            draw_text(renderer, 64, 92, tr("Game: ") + tr(launcher_game_label(selected)));
            draw_text(renderer, 64, 116, tr("Platform: ")
                + (active_platform ? tr(launcher_platform_label(*active_platform)) : tr("AUTO")));
            draw_text(renderer, 64, 136, modern ? tr("Presentation: Modern") : tr("Presentation: Original"));
            draw_text(renderer, 64, 156, tr("Original data is present and selected."));
            draw_text(renderer, 64, 180, tr("The simulation is incomplete; no synthetic substitute will run."));
            if (selected == eon::Game::millennium && millennium_preview_texture && millennium_assets) {
                // Input availability proves only TITLES.EXE's local exit path.
                // Neither DOS EXEC return nor 2200AD startup is observed, so
                // a GX canvas must never replace the original title frame.
                constexpr bool millennium_game_execution_observed = false;
                SDL_Texture* texture = millennium_texture_for(
                    modern && modern_graphics_settings.pixel_reconstruction);
                if (millennium_game_execution_observed) {
                    draw_text(renderer, 64, 220,
                        tr("AUTHENTIC DOS HANDOFF - TITLES.EXE -> 2200ad.exe; GX.LIB IMG00 -> IMG01"));
                    draw_text(renderer, 64, 238,
                        tr("ORIGINAL GX CANVAS + READ-ONLY 2200SAVE.I POSITIONAL TABLE"));
                } else {
                    if (millennium_assets->language == "es") {
                        draw_text(renderer, 64, 220,
                            tr("AUTHENTIC SPANISH DOS TITLE - FAT12 TITLE.LIB P00 + VGA RGB6 DAC"));
                        draw_text(renderer, 64, 238,
                            tr("The simulation is incomplete; no synthetic substitute will run."));
                    } else {
                        draw_text(renderer, 64, 220, tr("AUTHENTIC DOS TITLE - P00 INDICES + VGA RGB6 DAC"));
                        draw_text(renderer, 64, 238, millennium_title_session
                                && millennium_title_session->handed_off()
                            ? tr("The simulation is incomplete; no synthetic substitute will run.")
                            : tr("PRESS ANY KEY: ORIGINAL INT 21h/AH=06h TITLE HANDOFF"));
                    }
                }
                SDL_SetTextureScaleMode(texture,
                    modern && modern_graphics_settings.smooth_scaling
                        ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
                const auto preview_bounds = aspect_viewport(64, 250, 576, 400,
                    modern_graphics_settings);
                if (modern && modern_graphics_settings.frame) draw_modern_surface_frame(renderer, preview_bounds);
                SDL_RenderTexture(renderer, texture, nullptr, &preview_bounds);
                if (modern && modern_graphics_settings.scanlines) draw_scanlines(renderer, preview_bounds);
                if (millennium_game_execution_observed) {
                    const auto& save = *millennium_assets->initial_save;
                    constexpr std::size_t records_per_page = 8;
                    const auto first_record = millennium_state_page * records_per_page;
                    std::ostringstream heading;
                    // This is a diagnostic view of original bytes, rather
                    // than game prose. Keep its variable part notation-only:
                    // it must not leak untranslated English into the launcher.
                    heading << "2200SAVE.I  SHA-256 " << save.sha256()
                            << "  v0x" << std::hex << save.layout().version << std::dec;
                    draw_text(renderer, 610, 270, heading.str());
                    draw_text(renderer, 610, 290,
                        tr("RECOVERED POSITIONAL WORDS ONLY; NO INFERRED GAME SEMANTICS"));
                    if (millennium_game_session) {
                        std::ostringstream dispatch;
                        dispatch << "F1-F10 -> [";
                        if (const auto index = millennium_game_session->last_function_key_index()) {
                            dispatch << *index << "]  (8 B)";
                        } else {
                            dispatch << "--]";
                        }
                        draw_text(renderer, 610, 310, dispatch.str());
                    }
                    if (millennium_game_session) {
                        if (const auto trace = millennium_game_session->last_first_function_key_trace()) {
                            std::ostringstream f1;
                            f1 << "F1: $" << std::hex << trace->handler_address
                               << " -> $" << trace->display_selector_call_address
                               << " -> $" << trace->setup_entry_address
                               << " (RET); $" << trace->selector_address
                               << "=0 -> $" << trace->selected_record_address << "  (+02=$"
                               << static_cast<unsigned>(trace->selected_record_byte_2) << ')';
                            draw_text(renderer, 610, 326, f1.str());
                        }
                        if (const auto trace = millennium_game_session->last_second_function_key_trace()) {
                            std::ostringstream f2;
                            f2 << "F2: [$" << std::hex
                               << trace->availability_address << "] >= $"
                               << static_cast<unsigned>(trace->minimum_availability)
                               << " -> $" << trace->handler_address
                               << " -> $" << trace->first_record_address
                               << " +$" << trace->record_stride;
                            draw_text(renderer, 610, 342, f2.str());
                        }
                        if (const auto trace = millennium_game_session->last_third_function_key_trace()) {
                            std::ostringstream f3;
                            f3 << "F3: [$" << std::hex
                               << trace->initialization_guard_address << "] == 0; [$"
                               << trace->availability_address << "] != 0 -> $"
                               << trace->handler_address << " -> $"
                               << trace->callback_address;
                            draw_text(renderer, 610, 358, f3.str());
                        }
                        if (const auto trace = millennium_game_session->last_fourth_function_key_trace()) {
                            std::ostringstream f4;
                            f4 << "F4: [$" << std::hex
                               << trace->initialization_guard_address << "] == 0 -> $"
                               << trace->handler_address << " -> $"
                               << trace->common_routine_address << " -> [$"
                               << trace->first_runtime_byte_address << "]=$"
                               << static_cast<unsigned>(trace->first_runtime_byte_value)
                               << " ; $" << trace->first_call_address
                               << " ; [$"
                               << trace->initialization_guard_clear_address << ']';
                            draw_text(renderer, 610, 374, f4.str());
                        }
                        if (const auto trace = millennium_game_session->last_fifth_function_key_trace()) {
                            std::ostringstream f5;
                            f5 << "F5: $" << std::hex << trace->handler_address
                               << " -> AL=$" << static_cast<unsigned>(trace->transfer_al_value)
                               << " -> $" << trace->first_call_address
                               << " -> $" << trace->first_call_initial_nested_call_address
                               << " ; $"
                               << trace->second_call_address << ",$" << trace->third_call_address
                               << ",$" << trace->fourth_call_address;
                            draw_text(renderer, 610, 390, f5.str());
                        }
                        if (const auto trace = millennium_game_session->last_sixth_function_key_trace()) {
                            std::ostringstream f6;
                            f6 << "F6: [$" << std::hex
                               << trace->initialization_guard_address << "] == 0 -> $"
                               << trace->handler_address << " -> [$"
                               << trace->first_byte_address << "]=$"
                               << static_cast<unsigned>(trace->first_byte_value)
                               << "; [$" << trace->second_byte_address << "]=$"
                               << static_cast<unsigned>(trace->second_byte_value)
                               << " -> $" << trace->wait_call_address;
                            draw_text(renderer, 610, 390, f6.str());
                        }
                        if (const auto trace = millennium_game_session->last_seventh_function_key_trace()) {
                            std::ostringstream f7;
                            f7 << "F7: [$" << std::hex
                               << trace->initialization_guard_address << "] == 0 -> $"
                               << trace->handler_address << " -> [$"
                               << trace->first_runtime_word_address << "],[$"
                               << trace->second_runtime_word_address << "],[$"
                               << trace->third_runtime_word_address << "] -> $"
                               << trace->terminal_call_address;
                            draw_text(renderer, 610, 390, f7.str());
                        }
                        if (const auto trace = millennium_game_session->last_eighth_function_key_trace()) {
                            std::ostringstream f8;
                            f8 << "F8: $" << std::hex << trace->handler_address
                               << " -> [$" << trace->reset_runtime_byte_address << "]=$"
                               << static_cast<unsigned>(trace->reset_runtime_byte_value)
                               << " -> $" << trace->local_preflight_address
                               << " -> $" << trace->repeated_call_address
                               << " (SHR BL,1 CARRY)";
                            draw_text(renderer, 610, 390, f8.str());
                            if (const auto effect = millennium_game_session->last_runtime_byte_effect()) {
                                std::ostringstream effect_text;
                                effect_text << "[$" << std::hex << effect->address << "] ";
                                if (effect->previous) {
                                    effect_text << '$' << static_cast<unsigned>(*effect->previous);
                                } else {
                                    effect_text << "?";
                                }
                                effect_text << " -> $" << static_cast<unsigned>(effect->value)
                                            << " ; !W, !C";
                                draw_text(renderer, 610, 406, effect_text.str());
                            }
                        }
                        if (const auto trace = millennium_game_session->last_ninth_function_key_trace()) {
                            std::ostringstream f9;
                            f9 << "F9: [$" << std::hex
                               << trace->initialization_guard_address << "] == 0 -> $"
                               << trace->handler_address << " -> [$"
                               << trace->first_reset_runtime_byte_address << "]=$"
                               << static_cast<unsigned>(trace->first_reset_runtime_byte_value)
                               << " -> [$" << trace->limit_runtime_byte_address << "] < $"
                               << static_cast<unsigned>(trace->limit_value)
                               << " -> $" << trace->local_preflight_address;
                            draw_text(renderer, 610, 390, f9.str());
                        }
                        if (const auto trace = millennium_game_session->last_tenth_function_key_trace()) {
                            std::ostringstream f10;
                            f10 << "F10: [$" << std::hex
                                << trace->initialization_guard_address << "] == 0 -> $"
                                << trace->handler_address << " -> [$"
                                << trace->limit_runtime_byte_address << "] < $"
                                << static_cast<unsigned>(trace->limit_value)
                                << " -> $" << trace->local_preflight_address
                                << " -> $" << trace->wait_call_address;
                            draw_text(renderer, 610, 390, f10.str());
                        }
                    }
                    for (std::size_t row = 0; row < records_per_page; ++row) {
                        const auto record_index = first_record + row;
                        if (record_index >= save.layout().state_table.size()) break;
                        const auto& record = save.state_record(record_index);
                        std::ostringstream line;
                        line << '[' << record_index << "] +00=0x" << std::hex
                             << record.runtime_offset_0 << " +04=0x" << record.runtime_offset_4
                             << " +06=0x" << record.runtime_offset_6 << " +08=0x"
                             << record.runtime_offset_8;
                        draw_text(renderer, 610, 406.0F + static_cast<float>(row) * 17.0F,
                            line.str());
                    }
                    std::ostringstream pager;
                    pager << "P " << (millennium_state_page + 1) << "/"
                          << ((eon::MillenniumDosSaveLayout::state_table_count + records_per_page - 1)
                              / records_per_page)
                          << "  <- / ->";
                    draw_text(renderer, 610, 496, pager.str());
                }
                draw_text(renderer, 64, 680, request.game ? tr("ESC: QUIT") : tr("ESC: BACK TO MENU"));
            } else if (selected == eon::Game::millennium && active_platform
                && *active_platform != eon::Platform::dos) {
                draw_text(renderer, 64, 220,
                    tr("VERIFIED NATIVE MILLENNIUM DATA - NO DOS RESOURCE SUBSTITUTION"));
                draw_text(renderer, 64, 244,
                    tr("INTERACTIVE AMIGA/ATARI ST FLOW IS NOT YET RECOVERED."));
                draw_text(renderer, 64, 268,
                    tr("NO SYNTHETIC SCREEN OR STATE WILL RUN FOR THIS PLATFORM."));
                draw_text(renderer, 64, 680, request.game ? tr("ESC: QUIT") : tr("ESC: BACK TO MENU"));
            } else if (selected == eon::Game::deuteros && active_platform
                && *active_platform == eon::Platform::atari_st) {
                draw_text(renderer, 64, 220,
                    tr("VERIFIED DEUTEROS ATARI ST MEDIA - PROTECTED BOOT CHAIN ONLY"));
                draw_text(renderer, 64, 244,
                    tr("INTERACTIVE ATARI ST PRESENTATION IS NOT YET RECOVERED."));
                draw_text(renderer, 64, 268,
                    tr("NO AMIGA PREVIEW OR SYNTHETIC STATE WILL RUN FOR THIS PLATFORM."));
                draw_text(renderer, 64, 680, request.game ? tr("ESC: QUIT") : tr("ESC: BACK TO MENU"));
            } else if (selected == eon::Game::deuteros && preview_texture && deuteros_opening) {
                const auto source_tick = deuteros_opening->ticks();
                if (!deuteros_preview_source_tick || *deuteros_preview_source_tick != source_tick) {
                    deuteros_preview_rgba = deuteros_opening->rgba_frame();
                    deuteros_preview_source_tick = source_tick;
                    if (deuteros_preview_rgba) {
                        SDL_UpdateTexture(preview_texture, nullptr, deuteros_preview_rgba->data(),
                            eon::DeuterosAmigaFrame::width * 4);
                    }
                }
                const auto& frame = deuteros_preview_rgba;
                draw_text(renderer, 64, 220, tr("AUTHENTIC AMIGA OPENING - ORIGINAL CHANNEL PROGRAM + PALETTE"));
                draw_text(renderer, 64, 238, tr("HOLD SPACE / ENTER: ORIGINAL INPUT SIGNAL"));
                draw_text(renderer, 64, 252, tr("PAULA: ORIGINAL PCM + PERIOD + VOLUME (FIRST DMA PASS)"));
                if (deuteros_title_resource) {
                    std::ostringstream handoff;
                    handoff << std::hex << *deuteros_title_resource;
                    draw_text(renderer, 64, 268, tr("ORIGINAL TITLE HANDOFF: RESOURCE 0x")
                        + handoff.str()
                        + "; "
                        + tr("TITLE-STAGE EXECUTION IS NOT YET RECOVERED; NO TITLE SCREEN IS FABRICATED"));
                }
                const auto& title_stage = deuteros_opening->title_stage_session();
                if (title_stage) {
                    std::ostringstream provenance;
                    provenance << tr("AUTHENTIC TITLE STAGE READY") << ": ADF +0x" << std::hex
                               << title_stage->stage().disk_offset << "; 0x"
                               << title_stage->stage().length << " -> RAM 0x"
                               << title_stage->stage().destination << "; 0x"
                               << title_stage->stage().entry_address;
                    draw_text(renderer, 64, 284, provenance.str());
                    draw_text(renderer, 64, 298,
                        tr("TITLE-STAGE EXECUTION IS NOT YET RECOVERED; NO TITLE SCREEN IS FABRICATED"));
                    draw_text(renderer, 64, 312,
                        tr("ORIGINAL TITLE STAGE SHA-256: ") + title_stage->original_sha256());
                }
                if (const auto& trace = deuteros_opening->alternate_renderer_trace()) {
                    draw_text(renderer, 64, title_stage ? 328 : 284, tr("ORIGINAL $20580 STREAM: +0x")
                        + [&] { std::ostringstream stream; stream << std::hex << trace->stream_offset;
                            return stream.str(); }()
                        + " - " + std::to_string(trace->glyph_codes.size()));
                }
                SDL_Texture* texture = preview_texture;
                if (modern && modern_graphics_settings.pixel_reconstruction && frame
                    && (!deuteros_modern_preview_attempted_tick
                        || *deuteros_modern_preview_attempted_tick != source_tick)) {
                    const auto enhanced = eon::reconstruct_rgba_scale2x(*frame,
                        eon::DeuterosAmigaFrame::width, eon::DeuterosAmigaFrame::height);
                    if (!modern_preview_texture) {
                        modern_preview_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                            SDL_TEXTUREACCESS_STREAMING, enhanced.width, enhanced.height);
                    }
                    if (modern_preview_texture && SDL_UpdateTexture(modern_preview_texture, nullptr,
                            enhanced.rgba.data(), enhanced.width * 4)) {
                        deuteros_modern_preview_source_tick = source_tick;
                    } else if (modern_preview_texture) {
                        std::cerr << "Unable to update transient Modern Deuteros texture: "
                                  << SDL_GetError() << '\n';
                    }
                    deuteros_modern_preview_attempted_tick = source_tick;
                }
                if (modern && modern_graphics_settings.pixel_reconstruction && modern_preview_texture
                    && deuteros_modern_preview_source_tick
                    && *deuteros_modern_preview_source_tick == source_tick) {
                    texture = modern_preview_texture;
                }
                SDL_SetTextureScaleMode(texture,
                    modern && modern_graphics_settings.smooth_scaling
                        ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
                // Keep the original pixels intact while allowing the extra
                // provenance boundary to remain visible after title handoff.
                const auto preview_bounds = aspect_viewport(64,
                    title_stage ? 350.0F : deuteros_title_resource ? 306.0F : 274.0F,
                    576, title_stage ? 350.0F : 400.0F, modern_graphics_settings);
                if (modern && modern_graphics_settings.frame) draw_modern_surface_frame(renderer, preview_bounds);
                SDL_RenderTexture(renderer, texture, nullptr, &preview_bounds);
                if (modern && modern_graphics_settings.scanlines) draw_scanlines(renderer, preview_bounds);
                draw_text(renderer, 64, 580, request.game ? tr("ESC: QUIT") : tr("ESC: BACK TO MENU"));
            } else {
                draw_text(renderer, 64, 220, request.game ? tr("ESC: QUIT") : tr("ESC: BACK TO MENU"));
            }
        }
        if (show_modern_graphics_settings) draw_modern_graphics_popup(renderer, modern_graphics_settings);
        SDL_RenderPresent(renderer);
    }

    for (auto& card : cards) SDL_DestroyTexture(card.texture);
    SDL_DestroyTexture(millennium_preview_texture);
    SDL_DestroyTexture(millennium_modern_preview_texture);
    SDL_DestroyTexture(millennium_gx_canvas_texture);
    SDL_DestroyTexture(preview_texture);
    SDL_DestroyTexture(modern_preview_texture);
    SDL_DestroyAudioStream(deuteros_audio_stream);
    active_text_renderer.reset();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
