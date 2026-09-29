#include "engine/millennium_dos_video_function_31_session.hpp"

#include "data/sha256.hpp"

#include <stdexcept>

namespace eon {

MillenniumDosVideoFunction31Session::MillenniumDosVideoFunction31Session(
    const std::span<const std::uint8_t> english_driver,
    const MillenniumDosVideoDriverKind kind)
    : driver_(parse_millennium_dos_video_driver(english_driver, kind)) {
    const bool ega = kind == MillenniumDosVideoDriverKind::ega640;
    constexpr auto span_size = static_cast<std::size_t>(6);
    const auto span_offset = static_cast<std::size_t>(driver_.function_thirty_one_address);
    const auto expected_hash = ega
        ? "94333f834cb173cd0d147abb963ac8363d3cc8a1420317660eb75024b04abda2"
        : "2fb9fb052bcfb36d2dd2becda30bc6537ae31b6d1631493fbf290d39bbb85e93";
    if (span_offset > english_driver.size()
        || english_driver.size() - span_offset < span_size
        || to_hex(sha256(english_driver.subspan(span_offset, span_size))) != expected_hash
        || driver_.function_thirty_one_state_address != (ega ? 0x008a : 0x00ac)
        || driver_.function_thirty_one_return_ah != (ega ? 0x04 : 0x01)) {
        throw std::runtime_error("Unsupported Millennium English function-$1f prefix");
    }
}

void MillenniumDosVideoFunction31Session::observe_local_state_read(
    const MillenniumDosVideoFunction31LocalRead& read) {
    if (state_ != MillenniumDosVideoFunction31State::awaiting_local_state_read
        || read.sequence <= last_sequence_
        || read.instruction_address != driver_.function_thirty_one_address
        || read.driver_local_address != driver_.function_thirty_one_state_address) {
        throw std::runtime_error("Millennium DOS function-$1f local byte observation mismatch");
    }
    outcome_ = MillenniumDosVideoFunction31Outcome{
        static_cast<std::uint16_t>(driver_.function_thirty_one_address + 5),
        read,
        static_cast<std::uint16_t>((driver_.function_thirty_one_return_ah << 8)
            | read.value)};
    last_sequence_ = read.sequence;
    state_ = MillenniumDosVideoFunction31State::ret_boundary;
}

} // namespace eon
