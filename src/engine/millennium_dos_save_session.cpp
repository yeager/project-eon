#include "engine/millennium_dos_save_session.hpp"

#include "data/sha256.hpp"

#include <stdexcept>

namespace eon {

MillenniumDosSaveSession::MillenniumDosSaveSession(
    const std::span<const std::uint8_t> serialized)
    : serialized_(serialized.begin(), serialized.end())
    , layout_(parse_millennium_dos_save_layout(serialized_))
    , sha256_(to_hex(eon::sha256(serialized_))) {
}

const MillenniumDosStateTableEntry& MillenniumDosSaveSession::state_record(
    const std::size_t index) const {
    if (index >= layout_.state_table.size()) {
        throw std::out_of_range("Millennium DOS state-table index out of range");
    }
    return layout_.state_table[index];
}

std::span<const std::uint8_t> MillenniumDosSaveSession::opaque_bytes(
    const std::size_t offset, const std::size_t length) const {
    if (offset > serialized_.size() || length > serialized_.size() - offset) {
        throw std::out_of_range("Millennium DOS save range out of bounds");
    }
    return std::span<const std::uint8_t>(serialized_).subspan(offset, length);
}

} // namespace eon
