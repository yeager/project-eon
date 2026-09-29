#include "engine/millennium_dos_video_function_13_session.hpp"

#include "data/sha256.hpp"

#include <stdexcept>

namespace eon {

MillenniumDosVideoFunction13Session::MillenniumDosVideoFunction13Session(
    const std::span<const std::uint8_t> english_driver,
    const MillenniumDosVideoDriverKind kind)
    : driver_(parse_millennium_dos_video_driver(english_driver, kind)) {
    constexpr auto span_size = static_cast<std::size_t>(14);
    const auto span_offset = static_cast<std::size_t>(driver_.function_thirteen_address);
    constexpr auto span_hash =
        "3bc140d91abbde582da6b63df063ad9ca92aeea2fd74d285d5eda8e9aa24f440";
    if (span_offset > english_driver.size()
        || english_driver.size() - span_offset < span_size
        || to_hex(sha256(english_driver.subspan(span_offset, span_size))) != span_hash
        || driver_.function_thirteen_status_port != 0x03da
        || driver_.function_thirteen_retrace_mask != 0x08
        || driver_.function_thirteen_first_poll_address != driver_.function_thirteen_address + 3
        || driver_.function_thirteen_second_poll_address != driver_.function_thirteen_address + 8) {
        throw std::runtime_error("Unsupported Millennium English function-$13 poll loop");
    }
}

std::optional<MillenniumDosVideoFunction13Boundary>
MillenniumDosVideoFunction13Session::boundary() const {
    switch (state_) {
    case MillenniumDosVideoFunction13State::awaiting_retrace_clear:
        return MillenniumDosVideoFunction13Boundary{
            driver_.function_thirteen_first_poll_address,
            driver_.function_thirteen_status_port};
    case MillenniumDosVideoFunction13State::awaiting_retrace_set:
        return MillenniumDosVideoFunction13Boundary{
            driver_.function_thirteen_second_poll_address,
            driver_.function_thirteen_status_port};
    case MillenniumDosVideoFunction13State::ret_boundary:
        return std::nullopt;
    }
    return std::nullopt;
}

void MillenniumDosVideoFunction13Session::observe_status(
    const MillenniumDosVideoFunction13PortRead& read) {
    const auto current = boundary();
    if (!current || read.sequence <= last_sequence_
        || read.instruction_address != current->instruction_address
        || read.port_address != current->port_address) {
        throw std::runtime_error("Millennium DOS function-$13 status observation mismatch");
    }

    reads_.push_back(read);
    last_sequence_ = read.sequence;
    const bool retrace = (read.value & driver_.function_thirteen_retrace_mask) != 0;
    if (state_ == MillenniumDosVideoFunction13State::awaiting_retrace_clear && !retrace) {
        state_ = MillenniumDosVideoFunction13State::awaiting_retrace_set;
    } else if (state_ == MillenniumDosVideoFunction13State::awaiting_retrace_set && retrace) {
        state_ = MillenniumDosVideoFunction13State::ret_boundary;
        outcome_ = MillenniumDosVideoFunction13Outcome{
            static_cast<std::uint16_t>(driver_.function_thirteen_address + 13), reads_};
    }
}

} // namespace eon
