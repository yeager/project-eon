#include "engine/millennium_dos_sound_selection_session.hpp"

#include <stdexcept>
#include <utility>

namespace eon {

MillenniumDosSoundSelectionSession::MillenniumDosSoundSelectionSession(
    MillenniumDosSoundSelectionEvidence evidence)
    : MillenniumDosSoundSelectionSession(std::move(evidence), std::nullopt, std::nullopt) {}

MillenniumDosSoundSelectionSession::MillenniumDosSoundSelectionSession(
    MillenniumDosSoundSelectionEvidence evidence,
    std::optional<MillenniumDosSoundDriverLeaf> sound_blaster_driver,
    std::optional<MillenniumDosSoundDriverLeaf> covox_driver)
    : evidence_(std::move(evidence)), sound_blaster_driver_(std::move(sound_blaster_driver)),
      covox_driver_(std::move(covox_driver)) {
    // Do not allow this input mapping to drift independently of the parser's
    // content-locked source evidence.  In particular, the table's missing
    // SROL leaf is not a selectable fallback.
    if (evidence_.launcher_sha256
            != "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e"
        || evidence_.selector_entry_address != 0x511 || evidence_.selector_byte_count != 100
        || evidence_.ibm_speaker_table_slot != 0 || evidence_.sound_blaster_table_slot != 3
        || evidence_.covox_table_slot != 4 || evidence_.ibm_speaker_filename != "sibm.drv"
        || evidence_.sound_blaster_filename != "ssbl.drv" || evidence_.covox_filename != "scvx.drv") {
        throw std::runtime_error("Unsupported Millennium DOS sound-selection session evidence");
    }
    const auto require_driver = [](const std::optional<MillenniumDosSoundDriverLeaf>& driver,
        const MillenniumDosSoundDriverKind kind, const std::string_view filename) {
        if (driver && (driver->kind != kind || driver->original_filename != filename
                || driver->sha256.size() != 64 || driver->byte_size == 0)) {
            throw std::runtime_error("Mismatched Millennium DOS sound-driver admission");
        }
    };
    require_driver(sound_blaster_driver_, MillenniumDosSoundDriverKind::sound_blaster, "ssbl.drv");
    require_driver(covox_driver_, MillenniumDosSoundDriverKind::covox_sound_master, "scvx.drv");
}

bool MillenniumDosSoundSelectionSession::accept_ascii_character(const char ascii_character) {
    if (choice_) return false;
    switch (ascii_character) {
    case '0': choice_ = MillenniumDosSoundEffectChoice::ibm_speaker; return true;
    case '1': choice_ = MillenniumDosSoundEffectChoice::sound_blaster; return true;
    case '2': choice_ = MillenniumDosSoundEffectChoice::covox_sound_master; return true;
    default: return false;
    }
}

std::string_view MillenniumDosSoundSelectionSession::selected_original_filename() const {
    if (!choice_) return {};
    switch (*choice_) {
    case MillenniumDosSoundEffectChoice::ibm_speaker: return evidence_.ibm_speaker_filename;
    case MillenniumDosSoundEffectChoice::sound_blaster: return evidence_.sound_blaster_filename;
    case MillenniumDosSoundEffectChoice::covox_sound_master: return evidence_.covox_filename;
    }
    return {};
}

std::uint8_t MillenniumDosSoundSelectionSession::selected_table_slot() const {
    if (!choice_) throw std::runtime_error("Millennium DOS sound effect has not been selected");
    switch (*choice_) {
    case MillenniumDosSoundEffectChoice::ibm_speaker: return evidence_.ibm_speaker_table_slot;
    case MillenniumDosSoundEffectChoice::sound_blaster: return evidence_.sound_blaster_table_slot;
    case MillenniumDosSoundEffectChoice::covox_sound_master: return evidence_.covox_table_slot;
    }
    throw std::runtime_error("Unknown Millennium DOS sound-effect choice");
}

std::optional<MillenniumDosSoundDriverLeaf> MillenniumDosSoundSelectionSession::selected_driver() const {
    if (!choice_) return std::nullopt;
    switch (*choice_) {
    case MillenniumDosSoundEffectChoice::ibm_speaker: return std::nullopt;
    case MillenniumDosSoundEffectChoice::sound_blaster: return sound_blaster_driver_;
    case MillenniumDosSoundEffectChoice::covox_sound_master: return covox_driver_;
    }
    return std::nullopt;
}

bool MillenniumDosSoundSelectionSession::selected_driver_is_admitted() const {
    return selected_driver().has_value();
}

} // namespace eon
