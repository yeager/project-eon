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
        || (selected_character == '1'
            && driver_.kind != MillenniumDosSoundDriverKind::sound_blaster)
        || (selected_character == '2'
            && driver_.kind != MillenniumDosSoundDriverKind::covox_sound_master)
        || (selected_character != '1' && selected_character != '2')) {
        throw std::runtime_error("Unsupported Millennium DOS selected sound-driver route");
    }
    const auto evidence = parse_millennium_dos_sound_selection(mill_com);
    filename_address_ = driver_.kind == MillenniumDosSoundDriverKind::sound_blaster
        ? 0x0645 : 0x064e;
    const auto slot = driver_.kind == MillenniumDosSoundDriverKind::sound_blaster
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
void MillenniumDosSoundDriverLoadSession::observe_vector_install(const std::uint16_t i,const std::uint16_t ax,const std::uint16_t dx){if(state_!=MillenniumDosSoundDriverLoadState::awaiting_vector_install||i!=0x0239||ax!=0x2595||dx!=0)throw std::runtime_error("Detached Millennium DOS INT 95h vector install");state_=MillenniumDosSoundDriverLoadState::awaiting_parent_stack;}
void MillenniumDosSoundDriverLoadSession::observe_parent_stack(const std::uint16_t i,const std::uint16_t address,const std::uint16_t value){if(state_!=MillenniumDosSoundDriverLoadState::awaiting_parent_stack||i!=0x032f||address!=0x05f7)throw std::runtime_error("Detached Millennium DOS EXEC parent stack");runtime_word_effects_.push_back({i,address,value});runtime_word_effects_.push_back({0x031e,0x067e,code_segment_});runtime_word_effects_.push_back({0x0322,0x0682,code_segment_});runtime_word_effects_.push_back({0x0326,0x0686,code_segment_});state_=MillenniumDosSoundDriverLoadState::title_exec_boundary;}
void MillenniumDosSoundDriverLoadSession::observe_title_exec_request(const std::uint16_t i,const std::uint16_t ax,const std::uint16_t dx,const std::uint16_t parameter_block){if(state_!=MillenniumDosSoundDriverLoadState::title_exec_boundary||i!=0x0336||ax!=0x4b00||dx!=0x068f||parameter_block!=0x067a)throw std::runtime_error("Detached Millennium DOS TITLES.EXE request");state_=MillenniumDosSoundDriverLoadState::title_exec_requested;}

} // namespace eon
