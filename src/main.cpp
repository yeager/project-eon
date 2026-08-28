#include "launcher.hpp"
#include "i18n.hpp"
#include "engine/deuteros_amiga_opening.hpp"
#include "engine/deuteros_amiga_paula.hpp"
#include "engine/deuteros_atari_bootstrap_session.hpp"
#include "engine/millennium_dos_title_session.hpp"
#include "engine/millennium_dos_game_session.hpp"
#include "engine/millennium_dos_save_session.hpp"
#include "engine/millennium_atari_bootstrap_session.hpp"
#include "engine/millennium_amiga_bootstrap_session.hpp"
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
#include "data/millennium_dos_game_flow.hpp"
#include "data/millennium_dos_gameplay_screen.hpp"
#include "data/millennium_dos_last_screen.hpp"
#include "data/millennium_amiga_loader.hpp"
#include "data/millennium_dos_lib.hpp"
#include "data/millennium_dos_title_flow.hpp"
#include "data/millennium_dos_video_driver.hpp"
#include "data/sha256.hpp"
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
#include <vector>

namespace {

enum class Screen { menu, launching };

const eon::Translator* active_translator = nullptr;

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
    SDL_RenderDebugText(renderer, x, y, localized.c_str());
}

// Modern presentation is deliberately a renderer-only treatment. It frames
// the same decoded original surfaces with scalable SDL primitives; it neither
// substitutes an asset nor changes an input, simulation, or saved-state byte.
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
        const auto image = eon::extract_asset_by_sha256(release.path, spanish_image_sha256);
        if (!image) throw std::runtime_error("Verified Spanish Millennium floppy missing");
        const eon::Fat12Disk disk(*image);
        const auto* title_entry = disk.find("TITLE.LIB");
        const auto* static_entry = disk.find("2200AD4.BIN");
        if (!title_entry || !static_entry) {
            throw std::runtime_error("Verified Spanish Millennium media missing title data");
        }
        const eon::MillenniumDosLib title_lib(disk.read(*title_entry));
        const auto* p00 = title_lib.find("P00");
        if (!p00) throw std::runtime_error("Verified Spanish TITLE.LIB has no P00 entry");
        const auto resource = title_lib.read(*p00);
        const auto bitmap = eon::decode_millennium_dos_bitmap(resource);
        const auto palette = eon::decode_millennium_dos_palette(resource, bitmap);
        const auto game_data = eon::parse_millennium_dos_game_data(disk.read(*static_entry));
        std::cout << "          Spanish FAT12: " << disk.root_entries().size()
            << " root files; TITLE.LIB P00 " << bitmap.width << 'x' << bitmap.height
            << ", RGB6 DAC entries 256, logical translation "
            << palette.logical_to_dac.size() << '\n';
        std::cout << "          Spanish 2200AD4.BIN: " << game_data.celestial_labels.size()
            << " original celestial labels (" << game_data.celestial_labels[4].text << ")\n";
        return;
    }
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
    constexpr auto last_lib_sha256 =
        "a3f5c0b447795881dd4cd5316a091ecc218b1bf563f567b6fe3f093f89781510";
    const auto last_bytes = eon::extract_asset_by_sha256(release.path, last_lib_sha256);
    if (!last_bytes) throw std::runtime_error("Verified Millennium LAST.LIB missing");
    const auto last_screen = eon::parse_millennium_dos_last_screen(*last_bytes);
    std::cout << "          LAST.LIB last: " << last_screen.bitmap.width << 'x'
        << last_screen.bitmap.height << " original indexed screen, RGB6 DAC entries 256\n";
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
        << "; numeric segment unmodelled; raw save 0x"
        << flow.launcher_private_interrupt_saved_offset_cell << "/0x"
        << flow.launcher_private_interrupt_saved_segment_cell << ", restore 0x"
        << flow.launcher_private_interrupt_restore_address << std::dec << "))\n";
    const auto ega640 = eon::extract_asset_by_sha256(release.path,
        "ba003dd155fee868980f6ece933c33f9b22af68ed376cd64f4e027abd65baf6a");
    const auto mcga = eon::extract_asset_by_sha256(release.path,
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
        << " (validated ABI only; no BIOS/driver execution)\n" << std::dec;
    constexpr auto static_data_sha256 =
        "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d";
    const auto static_data = eon::extract_asset_by_sha256(release.path, static_data_sha256);
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
    const auto game = eon::extract_asset_by_sha256(release.path, game_sha256);
    if (!game) throw std::runtime_error("Verified Millennium DOS executable missing");
    const auto game_flow = eon::parse_millennium_dos_game_flow(*game);
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
    std::size_t shared_resident_images = 0;
    std::optional<eon::MillenniumAmigaSharedResidentLayout> shared_resident;
    for (const auto& asset : eon::inventory_zip(release.path)) {
        if (asset.kind != eon::AssetKind::amiga_adf) continue;
        const auto image = eon::extract_asset_by_sha256(release.path, asset.sha256);
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
    const auto image = eon::extract_asset_by_sha256(release.path, loader_adf_sha256);
    if (!image) return;
    const eon::MillenniumAmigaBootstrapSession live_bootstrap(*image);
    const eon::AmigaAdf disk(*image);
    const auto plan = eon::parse_millennium_amiga_load_plan(disk);
    std::cout << "          bounded launcher bootstrap: resident entry 0x" << std::hex
        << live_bootstrap.resident_entry().entry_address << ", raw resident SHA-256 "
        << live_bootstrap.shared_resident().raw_sha256 << std::dec
        << " (no transformed-stage call)\n";
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
    std::cout << "          raw loader: disk 0x" << std::hex
        << plan.first_stage.disk_offset << " + 0x" << plan.first_stage.length
        << " -> memory 0x" << plan.first_stage.destination
        << "; disk 0x" << plan.resident_stage.disk_offset << " + 0x"
        << plan.resident_stage.length << " -> entry 0x" << plan.resident_entry
        << ", marker 0x" << plan.loader_magic << std::dec << '\n'
        << "          raw stage SHA-256: bootstrap " << plan.bootstrap_loader.raw_sha256
        << "; first " << plan.first_stage.raw_sha256
        << "; resident " << plan.resident_stage.raw_sha256 << '\n'
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
        << " (static only; no registers, stores, predicates, or continuation executed)\n";
}

void report_millennium_atari_st(const eon::ReleaseArchive& release) {
    constexpr auto equinox_disk_sha256 =
        "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7";
    const auto image = eon::extract_asset_by_sha256(release.path, equinox_disk_sha256);
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
    std::cout << "          bounded launcher bootstrap: target 0x" << std::hex
        << live_bootstrap.target().target_address << ", Fopen boundary "
        << live_bootstrap.fopen_boundary().fopen_filename << std::dec
        << " (no GEMDOS call)\n";
    const auto equinox_config = eon::probe_millennium_atari_config(disk);
    if (!equinox_config.present) throw std::runtime_error("Verified Millennium Atari ST disk has no MILL22A.inf");
    const auto auxiliary_resource = eon::probe_millennium_atari_auxiliary_resource_name(disk);
    const auto config_entry = eon::parse_millennium_atari_config_entry(
        disk.read(*disk.find(equinox_config.requested_filename)));
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
    std::cout << "          MILL22A.inf entry: JMP 0x" << std::hex << config_entry.entry_address
        << " resolves from proven load base 0x" << config_entry.proven_load_base
        << " to file +0x" << config_entry.entry_file_offset << "; TRAP #14 selectors 0x"
        << config_entry.initial_trap_selector << " (longword 0x"
        << config_entry.initial_trap_longword_argument << ") and 0x"
        << config_entry.second_trap_selector << " (longword 0x"
        << config_entry.second_trap_longword_argument << "); JSRs";
    for (const auto address : config_entry.jsr_targets) std::cout << " 0x" << address;
    std::cout << "; PEA 0x" << config_entry.final_pea_address << ", TRAP #14 selector 0x"
        << config_entry.final_trap_selector << ", RTS +0x" << config_entry.return_offset
        << std::dec << " (validated only; no TOS/XBIOS calls or config execution)\n";
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

    // The outer archive is the supplied-media boundary.  Inspect every ST
    // leaf in memory so absence is not guessed from the one Equinox variant.
    std::size_t supplied_st_images = 0;
    std::size_t readable_fat12_images = 0;
    std::size_t config_files = 0;
    for (const auto& asset : eon::inventory_zip(release.path)) {
        if (asset.kind != eon::AssetKind::atari_st_disk) continue;
        ++supplied_st_images;
        const auto candidate = eon::extract_asset_by_sha256(release.path, asset.sha256);
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
        const auto& state5_plan = live_bootstrap.state5_raw_load_plan();
        const auto& state5_return = live_bootstrap.state5_return();
        const auto& supervisor_callback = live_bootstrap.supervisor_callback();
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
    }
    std::cout << "          Disk 2 boot continuation: "
        << (continuation.killer_boot_signature ? "KILLER_BOOT signature" : "unclassified")
        << ", branch 0x" << std::hex << continuation.boot_branch_target << std::dec << '\n';
    if (continuation.has_killer_boot_continuation_profile) {
        std::cout << "          Disk 2 relocated continuation: boot +0x" << std::hex
            << continuation.killer_boot_vector_source_offset << " +0x"
            << continuation.killer_boot_relocated_byte_count << " -> RAM 0x"
            << continuation.killer_boot_vector_destination << "; SHA-256 "
            << continuation.killer_boot_relocated_sha256 << "; clears 0x"
            << continuation.killer_boot_clear_start << " in 0x"
            << continuation.killer_boot_clear_stride << "-byte blocks (not executed)" << std::dec
            << '\n';
    }
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
            const auto image = eon::extract_asset_by_sha256(spanish_release->path, spanish_image_sha256);
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
        const auto game = eon::extract_asset_by_sha256(release->path, game_sha256);
        const auto initial_save = eon::extract_asset_by_sha256(release->path, initial_save_sha256);
        const auto ega640 = eon::extract_asset_by_sha256(release->path, ega640_sha256);
        const auto mcga = eon::extract_asset_by_sha256(release->path, mcga_sha256);
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
        const auto image = eon::extract_asset_by_sha256(release->path, clean_system_adf);
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
        const auto image = eon::extract_asset_by_sha256(release->path, equinox_disk_sha256);
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
        const auto image = eon::extract_asset_by_sha256(release->path, defjam_adf_sha256);
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
        const auto image = eon::extract_asset_by_sha256(release->path, replicants_disk1_sha256);
        if (!image) return {};
        return std::make_unique<eon::DeuterosAtariBootstrapSession>(std::move(*image));
    } catch (const std::exception& error) {
        std::cerr << "Unable to start Deuteros Atari ST bootstrap: " << error.what() << '\n';
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
    if (request.verify_game || request.inspect_data || request.game) {
        while (!scanner.advance(64)) {
        }
        releases = scanner.releases();
        if (releases.empty()) {
            std::cerr << "No recognised original release archives found.\n";
            return 3;
        }
    }
    if (request.verify_game || request.inspect_data) {
        bool found = false;
        for (const auto& release : releases) {
            if (request.verify_game && release.game != *request.verify_game) continue;
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
                && release.platform == eon::Platform::dos) {
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
    std::unique_ptr<eon::MillenniumAtariBootstrapSession> millennium_atari_session;
    std::unique_ptr<eon::MillenniumAmigaBootstrapSession> millennium_amiga_session;
    const auto discard_millennium_assets = [&] {
        if (millennium_preview_texture) SDL_DestroyTexture(millennium_preview_texture);
        if (millennium_gx_canvas_texture) SDL_DestroyTexture(millennium_gx_canvas_texture);
        millennium_preview_texture = nullptr;
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
    eon::Game selected = request.game.value_or(eon::Game::millennium);
    int focused = 0;
    std::uint64_t deuteros_last_tick = SDL_GetTicks();
    bool deuteros_input_pressed = false;
    std::optional<std::uint32_t> deuteros_title_resource;
    std::unique_ptr<eon::DeuterosAtariBootstrapSession> deuteros_atari_session;
    std::unique_ptr<eon::MillenniumDosTitleSession> millennium_title_session;
    std::unique_ptr<eon::MillenniumDosGameSession> millennium_game_session;
    std::size_t millennium_state_page = 0;
    const auto menu_platforms_for = [&](const eon::Game game) {
        return eon::available_platforms(releases, game);
    };
    const auto start_millennium_title = [&] {
        millennium_atari_session = load_millennium_atari_bootstrap(releases, active_platform);
        millennium_amiga_session = load_millennium_amiga_bootstrap(releases, active_platform);
        millennium_title_session.reset();
        millennium_game_session.reset();
        if (active_platform == eon::Platform::atari_st || active_platform == eon::Platform::amiga) return;
        load_millennium_assets_if_available();
        if (millennium_assets && millennium_assets->title_flow && millennium_assets->game_flow) {
            millennium_title_session = std::make_unique<eon::MillenniumDosTitleSession>(
                *millennium_assets->title_flow);
            millennium_game_session = std::make_unique<eon::MillenniumDosGameSession>(
                *millennium_assets->game_flow);
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
    if (screen == Screen::launching && selected == eon::Game::millennium) {
        start_millennium_title();
    }
    if (screen == Screen::launching && selected == eon::Game::deuteros) start_deuteros();
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
            if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN
                && event.gbutton.button == SDL_GAMEPAD_BUTTON_BACK) {
                if (screen == Screen::launching && !request.game) screen = Screen::menu;
                else running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F1 && !event.key.repeat
                && !(screen == Screen::launching && selected == eon::Game::millennium
                    && millennium_title_session && millennium_title_session->handed_off())) {
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
                if (millennium_title_session->handed_off() && millennium_game_session
                    && event.key.key >= SDLK_F1 && event.key.key <= SDLK_F10) {
                    const auto raw_action = static_cast<std::uint8_t>(0x3b
                        + static_cast<unsigned>(event.key.key - SDLK_F1));
                    static_cast<void>(millennium_game_session->observe_action(raw_action));
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
                && event.key.key == SDLK_D && !event.key.repeat) {
                show_scanner = !show_scanner;
            }
            if (screen == Screen::menu && event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_LEFT || event.key.key == SDLK_RIGHT) focused = 1 - focused;
                if (!request.platform
                    && (event.key.key == SDLK_UP || event.key.key == SDLK_DOWN)) {
                    const auto game = cards[static_cast<std::size_t>(focused)].game;
                    const auto platforms = menu_platforms_for(game);
                    if (!platforms.empty()) {
                        const auto current = std::find(platforms.begin(), platforms.end(), active_platform);
                        const auto index = current == platforms.end()
                            ? 0U : static_cast<unsigned>(std::distance(platforms.begin(), current));
                        const auto next = event.key.key == SDLK_UP
                            ? (index + platforms.size() - 1U) % platforms.size()
                            : (index + 1U) % platforms.size();
                        if (!active_platform || *active_platform != platforms[next]) {
                            active_platform = platforms[next];
                            discard_millennium_assets();
                        }
                    }
                }
                if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE) {
                    const auto game = cards[static_cast<std::size_t>(focused)].game;
                    if (eon::release_available(releases, game, active_platform)) {
                        selected = game;
                        screen = Screen::launching;
                        if (selected == eon::Game::millennium) start_millennium_title();
                        if (selected == eon::Game::deuteros) start_deuteros();
                    }
                }
            }
            if (screen == Screen::menu && event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                const auto button = event.gbutton.button;
                if (button == SDL_GAMEPAD_BUTTON_DPAD_LEFT
                    || button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) {
                    focused = 1 - focused;
                }
                if (!request.platform && (button == SDL_GAMEPAD_BUTTON_DPAD_UP
                    || button == SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
                    const auto game = cards[static_cast<std::size_t>(focused)].game;
                    const auto platforms = menu_platforms_for(game);
                    if (!platforms.empty()) {
                        const auto current = std::find(platforms.begin(), platforms.end(), active_platform);
                        const auto index = current == platforms.end()
                            ? 0U : static_cast<unsigned>(std::distance(platforms.begin(), current));
                        const auto next = button == SDL_GAMEPAD_BUTTON_DPAD_UP
                            ? (index + platforms.size() - 1U) % platforms.size()
                            : (index + 1U) % platforms.size();
                        if (!active_platform || *active_platform != platforms[next]) {
                            active_platform = platforms[next];
                            discard_millennium_assets();
                        }
                    }
                }
                if (button == SDL_GAMEPAD_BUTTON_SOUTH || button == SDL_GAMEPAD_BUTTON_START) {
                    const auto game = cards[static_cast<std::size_t>(focused)].game;
                    if (eon::release_available(releases, game, active_platform)) {
                        selected = game;
                        screen = Screen::launching;
                        if (selected == eon::Game::millennium) start_millennium_title();
                        if (selected == eon::Game::deuteros) start_deuteros();
                    }
                }
            }
            if (screen == Screen::menu && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                float x = 0, y = 0;
                SDL_RenderCoordinatesFromWindow(renderer, event.button.x, event.button.y, &x, &y);
                for (std::size_t index = 0; index < cards.size(); ++index) {
                    if (inside(cards[index].bounds, x, y)) {
                        focused = static_cast<int>(index);
                        if (eon::release_available(releases, cards[index].game, active_platform)) {
                            selected = cards[index].game;
                            screen = Screen::launching;
                            if (selected == eon::Game::millennium) start_millennium_title();
                            if (selected == eon::Game::deuteros) start_deuteros();
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
            draw_text(renderer, 64, 82, tr(
                "SELECT GAME   |   UP/DOWN: PLATFORM   |   D: DATA SCAN   |   F1: ORIGINAL / MODERN   |   ESC: QUIT"));
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
                draw_text(renderer, card.bounds.x + 18, card.bounds.y + card.bounds.h - 45, tr(card.title));
                draw_text(renderer, card.bounds.x + 18, card.bounds.y + card.bounds.h - 25, tr(card.subtitle));
                const auto available = eon::release_available(releases, card.game, std::nullopt);
                draw_text(renderer, card.bounds.x + 18, card.bounds.y + card.bounds.h + 16,
                    available ? tr("VERIFIED ORIGINAL DATA") : scanner.done()
                    ? tr("ORIGINAL DATA NOT FOUND") : tr("SCANNING ORIGINAL DATA..."));
            }
            draw_text(renderer, 64, 530, tr("ENTER / CLICK: START"));
            const auto focused_game = cards[static_cast<std::size_t>(focused)].game;
            const auto menu_platforms = menu_platforms_for(focused_game);
            std::string platform_text = tr("PLATFORM: ");
            if (active_platform) {
                platform_text += eon::name(*active_platform);
            } else {
                platform_text += tr("AUTO");
            }
            if (!menu_platforms.empty()) {
                platform_text += "  (";
                for (std::size_t index = 0; index < menu_platforms.size(); ++index) {
                    if (index != 0) platform_text += ", ";
                    platform_text += eon::name(menu_platforms[index]);
                }
                platform_text += ')';
            }
            draw_text(renderer, 64, 552, platform_text);
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
            draw_text(renderer, 64, 92, tr("Game: ") + eon::name(selected));
            draw_text(renderer, 64, 116, tr("Platform: ")
                + (active_platform ? eon::name(*active_platform) : tr("AUTO")));
            draw_text(renderer, 64, 136, modern ? tr("Presentation: Modern") : tr("Presentation: Original"));
            draw_text(renderer, 64, 156, tr("Original data is present and selected."));
            draw_text(renderer, 64, 180, tr("The simulation is incomplete; no synthetic substitute will run."));
            if (selected == eon::Game::millennium && millennium_preview_texture && millennium_assets) {
                const bool millennium_handed_off = millennium_title_session
                    && millennium_title_session->handed_off()
                    && millennium_gx_canvas_texture && millennium_assets->gx_canvas;
                SDL_Texture* texture = millennium_handed_off ? millennium_gx_canvas_texture
                                                              : millennium_preview_texture;
                const auto& image = millennium_handed_off ? *millennium_assets->gx_canvas
                                                            : millennium_assets->title;
                if (millennium_handed_off) {
                    draw_text(renderer, 64, 220,
                        tr("AUTHENTIC DOS HANDOFF - TITLES.EXE -> 2200ad.exe; GX.LIB IMG00 -> IMG01"));
                    draw_text(renderer, 64, 238,
                        tr("ORIGINAL GX CANVAS + READ-ONLY 2200SAVE.I POSITIONAL TABLE"));
                } else {
                    if (millennium_assets->language == "es") {
                        draw_text(renderer, 64, 220,
                            tr("AUTHENTIC SPANISH DOS TITLE - FAT12 TITLE.LIB P00 + VGA RGB6 DAC"));
                        draw_text(renderer, 64, 238,
                            tr("EXECUTABLE HANDOFF NOT YET RECOVERED; NO ENGLISH STATE IS SUBSTITUTED"));
                    } else {
                        draw_text(renderer, 64, 220, tr("AUTHENTIC DOS TITLE - P00 INDICES + VGA RGB6 DAC"));
                        draw_text(renderer, 64, 238,
                            tr("PRESS ANY KEY: ORIGINAL INT 21h/AH=06h TITLE HANDOFF"));
                    }
                }
                SDL_SetTextureScaleMode(texture,
                    modern ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
                const float scale = millennium_handed_off ? 1.65F : 2.0F;
                SDL_FRect preview_bounds{64, 250,
                    static_cast<float>(image.width) * scale,
                    static_cast<float>(image.height) * scale};
                if (modern) draw_modern_surface_frame(renderer, preview_bounds);
                SDL_RenderTexture(renderer, texture, nullptr, &preview_bounds);
                if (millennium_handed_off) {
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
                const auto frame = deuteros_opening->rgba_frame();
                if (frame) SDL_UpdateTexture(preview_texture, nullptr, frame->data(),
                    eon::DeuterosAmigaFrame::width * 4);
                draw_text(renderer, 64, 220, tr("AUTHENTIC AMIGA OPENING - ORIGINAL CHANNEL PROGRAM + PALETTE"));
                draw_text(renderer, 64, 238, tr("HOLD SPACE / ENTER: ORIGINAL INPUT SIGNAL"));
                draw_text(renderer, 64, 252, tr("PAULA: ORIGINAL PCM + PERIOD + VOLUME (FIRST DMA PASS)"));
                if (deuteros_title_resource) {
                    std::ostringstream handoff;
                    handoff << std::hex << *deuteros_title_resource;
                    draw_text(renderer, 64, 268, tr("ORIGINAL TITLE HANDOFF: RESOURCE 0x")
                        + handoff.str()
                        + " -> STAGE ENTRY 0x40426 (REIMPLEMENTATION IN PROGRESS)");
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
                SDL_SetTextureScaleMode(preview_texture,
                    modern ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
                // Keep the original pixels intact while allowing the extra
                // provenance boundary to remain visible after title handoff.
                const float scale = title_stage ? 1.75F : 2.0F;
                SDL_FRect preview_bounds{64, title_stage ? 350.0F : deuteros_title_resource ? 306.0F : 274.0F,
                    static_cast<float>(eon::DeuterosAmigaFrame::width) * scale,
                    static_cast<float>(eon::DeuterosAmigaFrame::height) * scale};
                if (modern) draw_modern_surface_frame(renderer, preview_bounds);
                SDL_RenderTexture(renderer, preview_texture, nullptr, &preview_bounds);
                draw_text(renderer, 64, 580, request.game ? tr("ESC: QUIT") : tr("ESC: BACK TO MENU"));
            } else {
                draw_text(renderer, 64, 220, request.game ? tr("ESC: QUIT") : tr("ESC: BACK TO MENU"));
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
