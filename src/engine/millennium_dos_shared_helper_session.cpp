#include "engine/millennium_dos_shared_helper_session.hpp"
#include "data/sha256.hpp"
#include <stdexcept>
namespace eon {
MillenniumDosSharedHelperSession::MillenniumDosSharedHelperSession(std::span<const std::uint8_t>b,std::uint16_t ax):table_offset_(std::uint16_t(ax<<1)){if(to_hex(sha256(b))!="427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57")throw std::runtime_error("Unsupported Millennium DOS $0666 helper profile");effects_.push_back({0x066e,0x05c8,0});}
MillenniumDosSharedHelperBoundary MillenniumDosSharedHelperSession::boundary()const{switch(state_){case MillenniumDosSharedHelperState::awaiting_segment:return{0x0669,0x0116};case MillenniumDosSharedHelperState::awaiting_table_word:return{0x0678,table_offset_,true,segment_};case MillenniumDosSharedHelperState::awaiting_helper_return:return{0x067b,0x05f7};case MillenniumDosSharedHelperState::returned:return{0x0681,0};}throw std::runtime_error("Invalid $0666 helper state");}
void MillenniumDosSharedHelperSession::observe_runtime_word(std::uint16_t i,std::uint16_t a,std::uint16_t v){if(state_!=MillenniumDosSharedHelperState::awaiting_segment||i!=0x0669||a!=0x0116)throw std::runtime_error("Detached $0666 segment");segment_=v;state_=MillenniumDosSharedHelperState::awaiting_table_word;}
void MillenniumDosSharedHelperSession::observe_far_word(std::uint16_t i,std::uint16_t s,std::uint16_t o,std::uint16_t v){if(state_!=MillenniumDosSharedHelperState::awaiting_table_word||i!=0x0678||s!=segment_||o!=table_offset_)throw std::runtime_error("Detached $0666 table word");selected_offset_=v;state_=MillenniumDosSharedHelperState::awaiting_helper_return;}
void MillenniumDosSharedHelperSession::observe_call_return(std::uint16_t i,std::uint16_t r){if(state_!=MillenniumDosSharedHelperState::awaiting_helper_return||i!=0x067b||r!=0x067e)throw std::runtime_error("Detached $0666 call return");state_=MillenniumDosSharedHelperState::returned;}
}
