#include "engine/deuteros_amiga_title_stage_session.hpp"

#include "data/sha256.hpp"

#include <stdexcept>
#include <string_view>

namespace eon {

DeuterosAmigaTitleStageSession::DeuterosAmigaTitleStageSession(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    const std::uint16_t incoming_profile)
    : disk_(&disk), load_plan_(plan), stage_(plan.title_stage),
      profile_(parse_deuteros_amiga_title_stage(disk, plan)),
      entry_prefix_(incoming_profile == 1
          ? execute_deuteros_amiga_title_entry_prefix(disk, plan, incoming_profile)
          : DeuterosAmigaTitleEntryPrefix{incoming_profile, 0x206a0, 0x4040e,
                incoming_profile, 0, 0, 0x40450}),
      entry_prefix_state_(materialize_deuteros_amiga_title_entry_prefix_state(
          disk, plan, incoming_profile)),
      exec_prelude_(execute_deuteros_amiga_title_exec_prelude(disk, plan, incoming_profile)),
      graphics_setup_(parse_deuteros_amiga_title_graphics_setup_profile(disk, plan)),
      display_clear_(parse_deuteros_amiga_title_display_clear_profile(disk, plan)),
      callback_registration_(
          parse_deuteros_amiga_title_callback_registration_profile(disk, plan)),
      exec_boundary_session_(disk, plan, exec_prelude_, profile_),
      service_setup_boundary_session_(disk, plan),
      service_batch_boundary_session_(disk, plan) {
    if (stage_.length == 0 || stage_.disk_offset > AmigaAdf::standard_size
        || stage_.length > AmigaAdf::standard_size - stage_.disk_offset
        || stage_.entry_address < stage_.destination
        || stage_.entry_address - stage_.destination >= stage_.length) {
        throw std::runtime_error("Invalid Deuteros Amiga title-stage provenance");
    }
    original_sha256_ = to_hex(sha256(original_bytes()));
    constexpr std::string_view clean_title_stage_sha256 =
        "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03";
    if (original_sha256_ != clean_title_stage_sha256) {
        throw std::runtime_error("Unexpected Deuteros Amiga title-stage media");
    }
    // Keep the caller-proven bootstrap profile joined to the exact loaded
    // stage. The helper models only local writes and stops before Exec.
    // Validate the two immediately following caller-connected local helpers
    // as part of admitting this live title-stage boundary.  They describe
    // original bytes and operands only: neither helper is executed and no
    // display memory is allocated from their externally supplied pointer.
    const bool profile_one = incoming_profile == 1;
    if (entry_prefix_state_.incoming_profile != entry_prefix_.incoming_profile
        || entry_prefix_state_.stop_before_exec_address != entry_prefix_.stop_before_exec_address
        || entry_prefix_state_.write_count != (profile_one ? 2U : 3U)
        || entry_prefix_state_.writes[0].address != entry_prefix_.mode_word_address
        || entry_prefix_state_.writes[0].width_bytes != 2
        || entry_prefix_state_.writes[0].value != entry_prefix_.mode_word_value
        || (profile_one && (entry_prefix_state_.writes[1].address != entry_prefix_.normal_mode_byte_address
            || entry_prefix_state_.writes[1].width_bytes != 1
            || entry_prefix_state_.writes[1].value != entry_prefix_.normal_mode_byte_value))
        || (!profile_one && (entry_prefix_state_.writes[1].address != 0x3717e
            || entry_prefix_state_.writes[1].width_bytes != 1
            || entry_prefix_state_.writes[1].value != 5
            || entry_prefix_state_.writes[2].address != 0x38092
            || entry_prefix_state_.writes[2].width_bytes != 2
            || entry_prefix_state_.writes[2].value != 0x0101))) {
        throw std::runtime_error("Deuteros title prefix state detached from original entry evidence");
    }
    if (exec_prelude_.incoming_profile != entry_prefix_.incoming_profile
        || exec_prelude_.entry_address != entry_prefix_.stop_before_exec_address
        || exec_prelude_.stack_pointer_value != 0x40b62
        || exec_prelude_.stop_before_exec_base_read_address != 0x40456) {
        throw std::runtime_error("Deuteros title Exec prelude detached from original entry evidence");
    }
    if (graphics_setup_.palette_source_address != profile_.transition_source_palette_address
        || graphics_setup_.palette_words.size() != 20
        || graphics_setup_.palette_destination_address != 0x12ecc
        || display_clear_.destination_pointer_address
            != graphics_setup_.external_display_base_destinations[0]
        || display_clear_.iteration_count != 0x1f40
        || display_clear_.write_width_bytes != 4) {
        throw std::runtime_error("Deuteros title graphics setup detached from original stage evidence");
    }
}

std::span<const std::uint8_t> DeuterosAmigaTitleStageSession::original_bytes() const {
    if (!disk_) throw std::runtime_error("Missing Deuteros Amiga title-stage source disk");
    return disk_->bytes(stage_.disk_offset, stage_.length);
}

std::span<const std::uint8_t> DeuterosAmigaTitleStageSession::title_disk1_read_chunk(
    const std::uint32_t ordinal) const {
    constexpr std::uint32_t first_offset = 0x14000U;
    constexpr std::uint32_t chunk_bytes = 0x1600U;
    constexpr std::array<std::string_view, 4> expected_hashes{
        "21e34a4162953998ab45dc1469b280f1d48dd9288596393fcd445e2aabd8a10d",
        "63306d78650d5958dcf8e093b10ea37b9c7721b559a09ff51407d3dee64ce9d6",
        "08505e304e73338409a0a28ffca915e6d75c8dc5955ab70281bf74b6258a5ddd",
        "579c42ce56eb5b88321e93e32f6e817a29ee0936b66124e2e5d25db01fe3347c",
    };
    if (!disk_ || ordinal >= expected_hashes.size()) {
        throw std::runtime_error("Deuteros title read chunk is outside the recovered Disk 1 profile");
    }
    const auto offset = first_offset + ordinal * chunk_bytes;
    const auto payload = disk_->bytes(offset, chunk_bytes);
    if (to_hex(sha256(payload)) != expected_hashes[ordinal]) {
        throw std::runtime_error("Deuteros title Disk 1 read chunk differs from its source hash");
    }
    return payload;
}

std::span<const std::uint8_t> DeuterosAmigaTitleStageSession::callback_code() const {
    constexpr std::uint32_t callback_address = 0x1f056;
    constexpr std::size_t callback_length = 0xfa;
    if (callback_address < stage_.destination
        || callback_address - stage_.destination > stage_.length
        || callback_length > stage_.length - (callback_address - stage_.destination)) {
        throw std::runtime_error("Deuteros title callback is outside its hash-locked stage");
    }
    const auto bytes = original_bytes().subspan(
        callback_address - stage_.destination, callback_length);
    if (to_hex(sha256(bytes)) != callback_registration_.callback_sha256) {
        throw std::runtime_error("Deuteros title callback bytes no longer match their admitted hash");
    }
    return bytes;
}

std::array<RgbColor, 16> DeuterosAmigaTitleStageSession::transition_palette_evidence() const {
    constexpr std::size_t word_count = 16;
    constexpr std::size_t byte_count = word_count * 2U;
    const auto address = profile_.transition_source_palette_address;
    if (address < stage_.destination || address - stage_.destination > stage_.length
        || byte_count > stage_.length - (address - stage_.destination)) {
        throw std::runtime_error("Deuteros title palette evidence lies outside original stage");
    }
    const auto bytes = original_bytes().subspan(address - stage_.destination, byte_count);
    std::array<RgbColor, word_count> colors{};
    for (std::size_t index = 0; index < colors.size(); ++index) {
        const auto rgb4 = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(bytes[index * 2U]) << 8U) | bytes[index * 2U + 1U]);
        if ((rgb4 & 0xf000U) != 0) {
            throw std::runtime_error("Invalid Deuteros title RGB4 palette evidence");
        }
        colors[index] = {static_cast<std::uint8_t>(((rgb4 >> 8U) & 0xfU) * 17U),
            static_cast<std::uint8_t>(((rgb4 >> 4U) & 0xfU) * 17U),
            static_cast<std::uint8_t>((rgb4 & 0xfU) * 17U)};
    }
    return colors;
}

std::array<RgbColor, 20> DeuterosAmigaTitleStageSession::graphics_setup_palette_evidence() const {
    constexpr std::size_t word_count = 20;
    constexpr std::size_t byte_count = word_count * 2U;
    const auto address = graphics_setup_.palette_source_address;
    if (address < stage_.destination || address - stage_.destination > stage_.length
        || byte_count > stage_.length - (address - stage_.destination)) {
        throw std::runtime_error("Deuteros title graphics-setup palette lies outside original stage");
    }
    const auto bytes = original_bytes().subspan(address - stage_.destination, byte_count);
    std::array<RgbColor, word_count> colors{};
    for (std::size_t index = 0; index < colors.size(); ++index) {
        const auto rgb4 = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(bytes[index * 2U]) << 8U) | bytes[index * 2U + 1U]);
        if ((rgb4 & 0xf000U) != 0) {
            throw std::runtime_error("Invalid Deuteros title graphics-setup RGB4 palette");
        }
        colors[index] = {static_cast<std::uint8_t>(((rgb4 >> 8U) & 0xfU) * 17U),
            static_cast<std::uint8_t>(((rgb4 >> 4U) & 0xfU) * 17U),
            static_cast<std::uint8_t>((rgb4 & 0xfU) * 17U)};
        if (rgb4 != graphics_setup_.palette_words[index]) {
            throw std::runtime_error("Deuteros title palette evidence detached from graphics setup");
        }
    }
    return colors;
}

std::optional<DeuterosAmigaTitleStageSession::LocalPrefixAdvance>
DeuterosAmigaTitleStageSession::execute_local_prefix() {
    if (local_prefix_executed_) return std::nullopt;
    local_prefix_executed_ = true;
    const auto exec_boundary = exec_boundary_session_.enter_after_local_prefix(
        exec_prelude_.stack_pointer_value);
    if (!exec_boundary
        || exec_boundary->state
            != DeuterosAmigaTitleExecBoundaryState::awaiting_exec_base_read) {
        throw std::runtime_error("Deuteros title Exec boundary did not advance");
    }
    return LocalPrefixAdvance{entry_prefix_state_.writes, entry_prefix_state_.write_count,
        exec_prelude_.stack_pointer_value,
        exec_prelude_.stop_before_exec_base_read_address};
}

std::optional<DeuterosAmigaTitleExecBoundaryCheckpoint>
DeuterosAmigaTitleStageSession::observe_exec_return(
    const DeuterosAmigaObservedExecReturn& observation) {
    if (!local_prefix_executed_) return std::nullopt;
    const auto advanced = exec_boundary_session_.observe_exec_return(observation);
    if (advanced && advanced->state
            == DeuterosAmigaTitleExecBoundaryState::before_open_library_boundary) {
        open_library_boundary_session_.emplace(
            *advanced, graphics_setup_, profile_, display_clear_);
    }
    return advanced;
}

std::optional<DeuterosAmigaTitleNativePrivilegeTransition>
DeuterosAmigaTitleStageSession::execute_native_privilege_transition() {
    if (!local_prefix_executed_) return std::nullopt;
    const auto transition = exec_boundary_session_.execute_native_privilege_transition();
    if (!transition) return std::nullopt;
    open_library_boundary_session_.emplace(
        exec_boundary_session_.checkpoint(), graphics_setup_, profile_, display_clear_);
    return transition;
}

std::optional<DeuterosAmigaTitleDisplayLocalAdvance>
DeuterosAmigaTitleStageSession::observe_display_base_and_advance(
    const DeuterosAmigaObservedDisplayBaseRead& observation) {
    if (!open_library_boundary_session_) return std::nullopt;
    const auto advanced =
        open_library_boundary_session_->observe_display_base_and_advance(observation);
    if (advanced) {
        custom_chip_boundary_session_.emplace(
            observation.trace_sequence, profile_, callback_registration_);
    }
    return advanced;
}

std::optional<DeuterosAmigaTitlePostCallbackRegistrationAdvance>
DeuterosAmigaTitleStageSession::observe_callback_exec_return(
    const DeuterosAmigaObservedCallbackExecReturn& observation) {
    if (!custom_chip_boundary_session_) return std::nullopt;
    const auto advanced = custom_chip_boundary_session_->observe_exec_return(observation);
    if (advanced) {
        last_callback_producer_sequence_ = observation.trace_sequence;
        service_setup_boundary_session_.enter(observation.trace_sequence);
    }
    return advanced;
}

std::optional<DeuterosAmigaTitleCallbackProducerRuntimePlan>
DeuterosAmigaTitleStageSession::observe_callback_producer(
    const DeuterosAmigaObservedTitleCallbackProducer& observation) {
    if (!custom_chip_boundary_session_
        || !custom_chip_boundary_session_->observed_exec_return()) {
        return std::nullopt;
    }
    const auto& registration_return =
        *custom_chip_boundary_session_->observed_exec_return();
    if (observation.entry_trace_sequence <= last_callback_producer_sequence_
        || observation.entry_trace_sequence <= registration_return.trace_sequence
        || observation.return_trace_sequence <= observation.entry_trace_sequence
        || observation.entry_address != callback_registration_.callback_address
        || observation.return_sample_address != 0x1f14e
        || observation.caller_event_byte != 1) {
        throw std::runtime_error(
            "Deuteros callback producer observations do not match the registered byte-one route");
    }

    const auto producer = evaluate_deuteros_amiga_title_callback_producer(
        *disk_, load_plan_, {observation.caller_word_at_6,
            observation.caller_word_at_8, observation.pending_count_before});
    const auto slot = static_cast<std::size_t>(producer.destination_offset);
    if (slot >= observation.queue_before.size()
        || observation.mirrored_event_after != producer.mirrored_event_value
        || observation.selector_word_after != producer.selector_word_value
        || observation.pending_count_after != producer.pending_count_after) {
        throw std::runtime_error(
            "Deuteros callback producer result differs from the observed return snapshot");
    }
    auto expected_queue = observation.queue_before;
    expected_queue[slot] = producer.queued_byte;
    if (observation.queue_after != expected_queue) {
        throw std::runtime_error(
            "Deuteros callback producer queue snapshot has unexpected byte changes");
    }
    last_callback_producer_sequence_ = observation.return_trace_sequence;
    return DeuterosAmigaTitleCallbackProducerRuntimePlan{observation, producer};
}

std::optional<DeuterosAmigaTitleMoveSpriteRuntimePlan>
DeuterosAmigaTitleStageSession::observe_callback_move_sprite(
    const DeuterosAmigaObservedTitleMoveSpriteCall& observation) {
    if (!custom_chip_boundary_session_
        || !custom_chip_boundary_session_->observed_exec_return()) {
        return std::nullopt;
    }
    constexpr std::array<std::uint32_t, 5> expected_addresses{
        0x20118, 0x201b6, 0x201ba, 0x201be, 0x1f0be};
    if (observation.sequence == 0 || observation.sequence > 32
        || observation.input_ordinal == 0 || observation.input_ordinal > 256
        || observation.instruction_addresses != expected_addresses
        || observation.caller_return_address != 0x1f0be
        || (last_move_sprite_sequence_
            && observation.sequence <= *last_move_sprite_sequence_)
        || (last_move_sprite_input_ordinal_
            && observation.input_ordinal <= *last_move_sprite_input_ordinal_)) {
        throw std::runtime_error(
            "Deuteros MoveSprite samples do not match the ordered callback route");
    }
    for (std::size_t index = 1; index < observation.sample_cycles.size(); ++index) {
        if (observation.sample_cycles[index] <= observation.sample_cycles[index - 1]) {
            throw std::runtime_error(
                "Deuteros MoveSprite sample cycles are not strictly increasing");
        }
    }

    const auto expected = evaluate_deuteros_amiga_title_move_sprite(
        *disk_, load_plan_, observation.input);
    const bool arguments_match =
        static_cast<std::uint16_t>(observation.d0) == expected.d0
        && static_cast<std::uint16_t>(observation.d1) == expected.d1
        && observation.a0 == expected.viewport_address
        && observation.a1 == expected.sprite_address;
    last_move_sprite_sequence_ = observation.sequence;
    last_move_sprite_input_ordinal_ = observation.input_ordinal;
    return DeuterosAmigaTitleMoveSpriteRuntimePlan{
        observation, expected, arguments_match};
}

std::optional<DeuterosAmigaTitleResponseQueueRuntimePlan>
DeuterosAmigaTitleStageSession::observe_response_queue(
    const DeuterosAmigaObservedTitleResponseQueue& observation) {
    if (!custom_chip_boundary_session_
        || !custom_chip_boundary_session_->observed_exec_return()) {
        return std::nullopt;
    }

    constexpr std::array<std::uint32_t, 6> expected_addresses{
        0x407b4, 0x1f230, 0x1f23e, 0x1f258, 0x407ba, 0x407e4};
    if (observation.instruction_addresses != expected_addresses
        || observation.sequence == 0
        || (last_response_queue_sequence_
            && observation.sequence <= *last_response_queue_sequence_)
        || (last_response_queue_cycle_
            && observation.sample_cycles.front() <= *last_response_queue_cycle_)
        || observation.stack_return_addresses
            != std::array<std::uint32_t, 3>{0x407ba, 0x407ba, 0x407ba}) {
        throw std::runtime_error(
            "Deuteros response-queue observations do not match the ordered caller/helper path");
    }
    for (std::size_t index = 1; index < observation.sample_cycles.size(); ++index) {
        if (observation.sample_cycles[index] <= observation.sample_cycles[index - 1]
            || observation.input_ordinals[index] == 0
            || observation.input_ordinals[index] < observation.input_ordinals[index - 1]) {
            throw std::runtime_error(
                "Deuteros response-queue observations are not monotonic");
        }
    }
    if (observation.input_ordinals.front() == 0) {
        throw std::runtime_error("Deuteros response-queue sample is not linked to host input");
    }

    const auto pending = observation.pending_counts[0];
    if (pending == 0 || observation.pending_counts[1] != pending
        || observation.pending_counts[2] != pending
        || observation.d0_values[0] != pending
        || ((observation.second_read_sr & 0x0004U) != 0)) {
        throw std::runtime_error(
            "Deuteros response-queue observation does not prove the nonempty second-read path");
    }

    const auto dequeue = evaluate_deuteros_amiga_title_response_queue_once(
        *disk_, load_plan_, {pending, observation.queue_snapshots[0]});
    if (observation.queue_snapshots[1] != observation.queue_snapshots[0]
        || observation.queue_snapshots[2] != observation.queue_snapshots[0]
        || observation.queue_snapshots[3] != dequeue.shifted_bytes
        || observation.queue_snapshots[4] != dequeue.shifted_bytes
        || observation.queue_snapshots[5] != dequeue.shifted_bytes
        || observation.pending_counts[3] != dequeue.pending_count_after
        || observation.pending_counts[4] != dequeue.pending_count_after
        || observation.pending_counts[5] != dequeue.pending_count_after
        || (observation.d0_values[1] & 0x00ffU) != dequeue.response_low_byte) {
        throw std::runtime_error(
            "Deuteros response-queue snapshots disagree with the original dequeue instructions");
    }

    last_response_queue_sequence_ = observation.sequence;
    last_response_queue_cycle_ = observation.sample_cycles.back();
    return DeuterosAmigaTitleResponseQueueRuntimePlan{observation, dequeue};
}

std::optional<DeuterosAmigaTitleControllerPointerSeedPlan>
DeuterosAmigaTitleStageSession::advance_controller_pointer_seed() {
    const auto advanced = service_setup_boundary_session_.advance_controller_pointer_seed();
    if (advanced) {
        const auto* library = open_library_boundary();
        if (!library
            || (library->observed_return && library->native_service_return)
            || (!library->observed_return && !library->native_service_return)
            || (library->observed_return && library->observed_return->result_d0 == 0)
            || (library->native_service_return
                && library->native_service_return->native_service_token == 0)) {
            throw std::runtime_error("Missing Deuteros graphics-library observation");
        }
        service_batch_boundary_session_.enter(
            advanced->preceding_trace_sequence,
            library->observed_return ? library->observed_return->result_d0
                : library->native_service_return->native_service_token);
    }
    return advanced;
}

} // namespace eon
