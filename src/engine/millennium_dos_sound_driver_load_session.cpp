#include "engine/millennium_dos_sound_driver_load_session.hpp"

#include "data/millennium_dos_title_flow.hpp"
#include "data/sha256.hpp"

#include <stdexcept>

namespace eon {

MillenniumDosSoundDriverLoadSession::MillenniumDosSoundDriverLoadSession(
    const std::span<const std::uint8_t> mill_com,
    const std::span<const std::uint8_t> selected_driver,
    const char selected_character, const std::uint16_t code_segment)
    : driver_(admit_millennium_dos_sound_driver_leaf(selected_driver)),
      driver_bytes_(selected_driver.begin(), selected_driver.end()),
      code_segment_(code_segment) {
    if (to_hex(sha256(mill_com))
            != "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e"
        || code_segment_ == 0
        || (selected_character == '0'
            && driver_.kind != MillenniumDosSoundDriverKind::ibm_speaker)
        || (selected_character == '1'
            && driver_.kind != MillenniumDosSoundDriverKind::sound_blaster)
        || (selected_character == '2'
            && driver_.kind != MillenniumDosSoundDriverKind::covox_sound_master)
        || (selected_character != '0' && selected_character != '1' && selected_character != '2')) {
        throw std::runtime_error("Unsupported Millennium DOS selected sound-driver route");
    }
    const auto evidence = parse_millennium_dos_sound_selection(mill_com);
    filename_address_ = driver_.kind == MillenniumDosSoundDriverKind::ibm_speaker
        ? 0x062a : driver_.kind == MillenniumDosSoundDriverKind::sound_blaster
            ? 0x0645 : 0x064e;
    const auto slot = driver_.kind == MillenniumDosSoundDriverKind::ibm_speaker
        ? evidence.ibm_speaker_table_slot : driver_.kind == MillenniumDosSoundDriverKind::sound_blaster
            ? evidence.sound_blaster_table_slot : evidence.covox_table_slot;
    runtime_byte_effects_.push_back({0x0221,0x068a,
        static_cast<std::uint8_t>('0' + slot)});
}

MillenniumDosSoundDriverLoadBoundary MillenniumDosSoundDriverLoadSession::boundary() const {
    switch (state_) {
    case MillenniumDosSoundDriverLoadState::awaiting_open_result:
        return {MillenniumDosSoundDriverLoadBoundaryKind::dos_result,0x02d2,0x3d00,filename_address_,0};
    case MillenniumDosSoundDriverLoadState::awaiting_seek_end_result:
        return {MillenniumDosSoundDriverLoadBoundaryKind::dos_result,0x02eb,0x4202,0,0};
    case MillenniumDosSoundDriverLoadState::awaiting_allocation_result:
        return {MillenniumDosSoundDriverLoadBoundaryKind::dos_result,0x02fa,0x4800,0,
            static_cast<std::uint16_t>((driver_bytes_.size()+15)/16)};
    case MillenniumDosSoundDriverLoadState::awaiting_seek_start_result:
        return {MillenniumDosSoundDriverLoadBoundaryKind::dos_result,0x0309,0x4200,0,0};
    case MillenniumDosSoundDriverLoadState::awaiting_read_result:
        return {MillenniumDosSoundDriverLoadBoundaryKind::dos_result,0x0313,0x3f00,0,
            static_cast<std::uint16_t>(driver_bytes_.size())};
    case MillenniumDosSoundDriverLoadState::awaiting_close_result:
        return {MillenniumDosSoundDriverLoadBoundaryKind::dos_result,0x0319,0x3e00,0,0};
    case MillenniumDosSoundDriverLoadState::awaiting_vector_install:
        return {MillenniumDosSoundDriverLoadBoundaryKind::interrupt_observation,0x0239,0x2595,0,0};
    case MillenniumDosSoundDriverLoadState::awaiting_parent_stack:
        return {MillenniumDosSoundDriverLoadBoundaryKind::runtime_word,0x032f,0,0x05f7,0};
    case MillenniumDosSoundDriverLoadState::title_exec_boundary:
    case MillenniumDosSoundDriverLoadState::title_exec_requested:
        return {MillenniumDosSoundDriverLoadBoundaryKind::dos_exec,0x0336,0x4b00,0x068f,0x067a};
    }
    throw std::runtime_error("Invalid Millennium DOS sound-driver load state");
}

bool MillenniumDosSoundDriverLoadSession::has_eon_sibm_int95() const noexcept {
    return state_ == MillenniumDosSoundDriverLoadState::title_exec_requested
        && driver_.kind == MillenniumDosSoundDriverKind::ibm_speaker
        && driver_.sha256
            == "f3224caa43c1149907f852fa98816ed68c489b70f1ba795592d684d4e51f31b1"
        && installed_vector_
        && installed_vector_->interrupt_number == 0x95
        && installed_vector_->offset == 0
        && installed_vector_->segment == load_segment_
        && vector_install_provenance_
            == MillenniumDosSoundDriverVectorInstallProvenance::eon_dos_compatibility_service;
}

void MillenniumDosSoundDriverLoadSession::observe_open_result(const std::uint16_t i,
    const bool carry, const std::uint16_t ax) {
    if(state_!=MillenniumDosSoundDriverLoadState::awaiting_open_result||i!=0x02d2||carry)
        throw std::runtime_error("Millennium DOS sound-driver open failed or detached");
    file_handle_=ax; state_=MillenniumDosSoundDriverLoadState::awaiting_seek_end_result;
}
void MillenniumDosSoundDriverLoadSession::observe_seek_end_result(const std::uint16_t i,
    const bool carry, const std::uint16_t bx, const std::uint16_t ax,
    const std::uint16_t dx) {
    if (state_ != MillenniumDosSoundDriverLoadState::awaiting_seek_end_result
        || i != 0x02eb || carry || bx != file_handle_ || dx != 0
        || ax != driver_bytes_.size()) {
        throw std::runtime_error("Millennium DOS sound-driver length differs from leaf");
    }
    state_ = MillenniumDosSoundDriverLoadState::awaiting_allocation_result;
}
void MillenniumDosSoundDriverLoadSession::observe_allocation_result(const std::uint16_t i,const bool carry,const std::uint16_t ax){if(state_!=MillenniumDosSoundDriverLoadState::awaiting_allocation_result||i!=0x02fa||carry||ax==0)throw std::runtime_error("Millennium DOS sound-driver allocation failed or detached");load_segment_=ax;state_=MillenniumDosSoundDriverLoadState::awaiting_seek_start_result;}
void MillenniumDosSoundDriverLoadSession::observe_seek_start_result(const std::uint16_t i,const bool carry,const std::uint16_t bx,const std::uint16_t ax,const std::uint16_t dx){if(state_!=MillenniumDosSoundDriverLoadState::awaiting_seek_start_result||i!=0x0309||carry||bx!=file_handle_||ax!=0||dx!=0)throw std::runtime_error("Millennium DOS sound-driver rewind failed or detached");state_=MillenniumDosSoundDriverLoadState::awaiting_read_result;}
void MillenniumDosSoundDriverLoadSession::observe_read_result(const std::uint16_t i,const bool carry,const std::uint16_t bx,const std::uint16_t ax){if(state_!=MillenniumDosSoundDriverLoadState::awaiting_read_result||i!=0x0313||carry||bx!=file_handle_||ax!=driver_bytes_.size())throw std::runtime_error("Millennium DOS sound-driver read was incomplete or detached");memory_effects_.reserve(driver_bytes_.size());for(std::size_t n=0;n<driver_bytes_.size();++n)memory_effects_.push_back({0x0313,load_segment_,static_cast<std::uint16_t>(n),driver_bytes_[n]});state_=MillenniumDosSoundDriverLoadState::awaiting_close_result;}
void MillenniumDosSoundDriverLoadSession::observe_close_result(const std::uint16_t i,const bool carry,const std::uint16_t bx){if(state_!=MillenniumDosSoundDriverLoadState::awaiting_close_result||i!=0x0319||carry||bx!=file_handle_)throw std::runtime_error("Millennium DOS sound-driver close failed or detached");state_=MillenniumDosSoundDriverLoadState::awaiting_vector_install;}
void MillenniumDosSoundDriverLoadSession::observe_vector_install(
    const std::uint16_t post_instruction, const std::uint16_t flags,
    const std::uint16_t ivt_offset, const std::uint16_t ivt_segment) {
    // INT 21h/AH=25 has no status result. Preserve flags in the observation
    // contract, but do not interpret CF as success or failure.
    static_cast<void>(flags);
    if (state_ != MillenniumDosSoundDriverLoadState::awaiting_vector_install
        || post_instruction != 0x023b
        || ivt_offset != 0
        || ivt_segment != load_segment_) {
        throw std::runtime_error("Detached Millennium DOS INT 95h SetVect result");
    }
    installed_vector_ = MillenniumDosSoundDriverIvtVector{0x95, ivt_offset, ivt_segment};
    vector_install_provenance_ =
        MillenniumDosSoundDriverVectorInstallProvenance::observed;
    state_ = MillenniumDosSoundDriverLoadState::awaiting_parent_stack;
}
void MillenniumDosSoundDriverLoadSession::install_compatibility_set_vector(
    const std::uint16_t instruction, const std::uint16_t ax,
    const std::uint16_t dx, const std::uint16_t ds) {
    // The exact MILL.COM request is INT 21h/AH=25h, AL=95h, DS:DX=driver:0.
    // Model DOS's deterministic IVT write while preserving its compatibility
    // provenance; this is not an observation from an original DOS process.
    if (state_ != MillenniumDosSoundDriverLoadState::awaiting_vector_install
        || instruction != 0x0239 || ax != 0x2595 || dx != 0
        || ds != load_segment_) {
        throw std::runtime_error("Detached Millennium DOS compatibility SetVect request");
    }
    installed_vector_ = MillenniumDosSoundDriverIvtVector{0x95, dx, ds};
    vector_install_provenance_ =
        MillenniumDosSoundDriverVectorInstallProvenance::eon_dos_compatibility_service;
    state_ = MillenniumDosSoundDriverLoadState::awaiting_parent_stack;
}
void MillenniumDosSoundDriverLoadSession::observe_parent_stack(
    const std::uint16_t instruction, const std::uint16_t address,
    const std::uint16_t value) {
    if (state_ != MillenniumDosSoundDriverLoadState::awaiting_parent_stack
        || instruction != 0x032f || address != 0x05f7) {
        throw std::runtime_error("Detached Millennium DOS EXEC parent stack");
    }
    runtime_word_effects_.push_back({instruction, code_segment_, address, value});
    runtime_word_effects_.push_back({0x031e, code_segment_, 0x067e, code_segment_});
    runtime_word_effects_.push_back({0x0322, code_segment_, 0x0682, code_segment_});
    runtime_word_effects_.push_back({0x0326, code_segment_, 0x0686, code_segment_});
    parent_stack_provenance_ =
        MillenniumDosSoundDriverParentStackProvenance::observed;
    state_ = MillenniumDosSoundDriverLoadState::title_exec_boundary;
}
void MillenniumDosSoundDriverLoadSession::install_compatibility_parent_stack(
    const std::uint16_t instruction, const std::uint16_t address,
    const std::uint16_t sp, const std::uint16_t ds) {
    // The exact COM entry sets SP=$0100. The selected-driver call at $0240
    // pushes its return address before entering $031c, so MOV [$05f7],SP at
    // $032f stores $00fe. DS was set to CS at $023b and remains CS here.
    if (state_ != MillenniumDosSoundDriverLoadState::awaiting_parent_stack
        || instruction != 0x032f || address != 0x05f7 || sp != 0x00fe
        || ds != code_segment_) {
        throw std::runtime_error(
            "Detached Millennium DOS compatibility parent-stack store");
    }
    runtime_word_effects_.push_back({instruction, code_segment_, address, sp});
    runtime_word_effects_.push_back({0x031e, code_segment_, 0x067e, code_segment_});
    runtime_word_effects_.push_back({0x0322, code_segment_, 0x0682, code_segment_});
    runtime_word_effects_.push_back({0x0326, code_segment_, 0x0686, code_segment_});
    parent_stack_provenance_ =
        MillenniumDosSoundDriverParentStackProvenance::eon_dos_compatibility_service;
    state_ = MillenniumDosSoundDriverLoadState::title_exec_boundary;
}
void MillenniumDosSoundDriverLoadSession::observe_title_exec_request(const std::uint16_t i,const std::uint16_t ax,const std::uint16_t dx,const std::uint16_t parameter_block){if(state_!=MillenniumDosSoundDriverLoadState::title_exec_boundary||i!=0x0336||ax!=0x4b00||dx!=0x068f||parameter_block!=0x067a)throw std::runtime_error("Detached Millennium DOS TITLES.EXE request");state_=MillenniumDosSoundDriverLoadState::title_exec_requested;}

} // namespace eon
