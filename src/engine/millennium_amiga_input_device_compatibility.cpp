#include "engine/millennium_amiga_input_device_compatibility.hpp"

#include <algorithm>
#include <stdexcept>

namespace eon {
namespace {

std::uint16_t big16(const std::span<const std::uint8_t> bytes,
    const std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2U)
        throw std::runtime_error("Truncated Millennium Amiga input state");
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(bytes[offset]) << 8U)
        | static_cast<std::uint16_t>(bytes[offset + 1U]));
}

std::uint8_t byte_at(const std::span<const std::uint8_t> bytes,
    const std::size_t offset) {
    if (offset >= bytes.size())
        throw std::runtime_error("Truncated Millennium Amiga input state");
    return bytes[offset];
}

} // namespace

MillenniumAmigaInputDeviceCompatibilitySession::
MillenniumAmigaInputDeviceCompatibilitySession(const AmigaAdf& disk,
    const MillenniumAmigaLoadPlan& plan, const std::uint64_t runtime_generation)
    : runtime_generation_(runtime_generation),
      source_boundary_(parse_millennium_amiga_input_device_open_boundary(disk, plan)) {
    if (runtime_generation_ == 0)
        throw std::invalid_argument("Millennium Amiga input generation must be nonzero");

    const auto stage = disk.bytes(plan.first_stage.disk_offset, plan.first_stage.length);
    const auto stage_offset = [&plan](const std::uint32_t address) -> std::size_t {
        if (address < plan.first_stage.destination)
            throw std::runtime_error("Millennium Amiga input address precedes its source stage");
        return static_cast<std::size_t>(address - plan.first_stage.destination);
    };
    const auto table_offset = stage_offset(source_boundary_.key_translation_table_address);
    if (source_boundary_.key_translation_table_byte_count != 0xa0U
        || table_offset > stage.size()
        || stage.size() - table_offset < source_boundary_.key_translation_table_byte_count) {
        throw std::runtime_error("Millennium Amiga key table is outside the admitted stage");
    }
    key_translation_table_ = stage.subspan(table_offset,
        source_boundary_.key_translation_table_byte_count);

    // Preserve the exact initial local state from the source image. The
    // complete first-stage hash was checked by the parser above.
    previous_raw_key_code_ = big16(stage, stage_offset(0x41b44));
    previous_qualifier_ = big16(stage, stage_offset(0x41b46));
    queued_count_ = big16(stage, stage_offset(0x41c06));
    last_event_class_ = byte_at(stage, stage_offset(0x41d04));
    event_flag_ = byte_at(stage, stage_offset(0x41e20));
    const auto queue_offset = stage_offset(0x41bf0);
    if (queue_offset > stage.size() || stage.size() - queue_offset < input_queue_.size())
        throw std::runtime_error("Millennium Amiga input queue is outside the admitted stage");
    std::copy_n(stage.begin() + static_cast<std::ptrdiff_t>(queue_offset),
        input_queue_.size(), input_queue_.begin());
    if (queued_count_ > 0x14U)
        throw std::runtime_error("Millennium Amiga source queue count exceeds its consumer bound");
}

bool MillenniumAmigaInputDeviceCompatibilitySession::
register_native_compatibility_handler(
    const MillenniumAmigaInputDeviceRegistrationOutcome& o) noexcept {
    if (registered_ || o.runtime_generation != runtime_generation_
        || o.origin != MillenniumAmigaInputDeviceCompatibilityOrigin::eon_native_compatibility
        || o.open_device_call_address != source_boundary_.call_address
        || o.open_device_vector != source_boundary_.vector
        || o.open_device_return_address != source_boundary_.return_address
        || !o.open_device_succeeded
        || o.reset_doio_call_address != source_boundary_.reset_doio_call_address
        || o.reset_doio_vector != source_boundary_.reset_doio_vector
        || o.reset_doio_return_address != source_boundary_.reset_doio_return_address
        || o.reset_doio_command != source_boundary_.reset_doio_command
        || !o.reset_doio_succeeded
        || o.add_handler_call_address != source_boundary_.add_handler_doio_call_address
        || o.add_handler_vector != source_boundary_.add_handler_doio_vector
        || o.add_handler_return_address != source_boundary_.add_handler_doio_return_address
        || o.add_handler_command != source_boundary_.add_handler_doio_command
        || o.handler_structure_address != source_boundary_.input_handler_struct_address
        || !o.add_handler_succeeded) {
        return false;
    }
    registered_ = true;
    return true;
}

MillenniumAmigaInputDeviceCompatibilitySession::DispatchResult
MillenniumAmigaInputDeviceCompatibilitySession::dispatch(const RawInputEvent& event,
    const std::uint64_t runtime_generation) noexcept {
    DispatchResult result;
    if (!registered_ || runtime_generation != runtime_generation_)
        return result;
    result.accepted = true;

    // The handler's first branch returns D0 unchanged for IECLASS_TIMER (6).
    // The other two special classes update $41e20 and fall through to the
    // source's register-clear/RTS epilogue, returning D0=0.
    if (event.event_class == 6U) {
        result.returned_event_list_address = event.event_list_address;
        result.last_event_class = last_event_class_;
        result.event_flag = event_flag_;
        result.queued_count = queued_count_;
        return result;
    }
    if (event.event_class == 0x0fU) {
        event_flag_ = 1;
    } else if (event.event_class == 0x10U) {
        event_flag_ = 0;
    } else {
        last_event_class_ = event.event_class;
        if (event.event_class == 1U) {
            previous_qualifier_ = event.qualifier;
            const auto duplicate = event.code == previous_raw_key_code_;
            previous_raw_key_code_ = event.code;
            if (!duplicate && event.code < 0x50U && queued_count_ < 0x14U) {
                const auto qualifier_row = (event.qualifier & 0x0007U) != 0 ? 0x50U : 0U;
                const auto table_index = static_cast<std::size_t>(qualifier_row + event.code);
                if (table_index < key_translation_table_.size()) {
                    const auto character = key_translation_table_[table_index];
                    input_queue_[queued_count_] = character;
                    ++queued_count_;
                    result.queued_character = true;
                    result.translated_character = character;
                }
            }
        }
    }
    result.consumed = true;
    result.returned_event_list_address = 0;
    result.last_event_class = last_event_class_;
    result.event_flag = event_flag_;
    result.queued_count = queued_count_;
    return result;
}

std::optional<std::uint8_t>
MillenniumAmigaInputDeviceCompatibilitySession::take_next_character(
    const std::uint64_t runtime_generation) noexcept {
    if (!registered_ || runtime_generation != runtime_generation_ || queued_count_ == 0)
        return std::nullopt;
    const auto character = input_queue_[0];
    for (std::size_t index = 0; index < 0x14U; ++index)
        input_queue_[index] = input_queue_[index + 1U];
    --queued_count_;
    return character;
}

} // namespace eon
