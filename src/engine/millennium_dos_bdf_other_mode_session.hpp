#pragma once
#include <cstdint>
#include <span>
#include <vector>
namespace eon {
enum class MillenniumDosBdfOtherModeState { awaiting_toggle, awaiting_segment, awaiting_source_word, awaiting_mode_four_toggle, mode_four_nonzero_boundary, awaiting_mode_four_segment, awaiting_mode_four_source_word, awaiting_mode_four_source_byte, returned };
struct MillenniumDosBdfOtherModeBoundary { std::uint16_t instruction_address=0,runtime_address=0; constexpr bool operator==(const MillenniumDosBdfOtherModeBoundary&)const=default; };
struct MillenniumDosBdfOtherModeFarEffect { std::uint16_t instruction_address=0,segment=0,offset=0,value=0; constexpr bool operator==(const MillenniumDosBdfOtherModeFarEffect&)const=default; };
struct MillenniumDosBdfOtherModePortEffect { std::uint16_t instruction_address=0,port=0;std::uint8_t value=0; constexpr bool operator==(const MillenniumDosBdfOtherModePortEffect&)const=default; };
struct MillenniumDosBdfOtherModeFarByteEffect { std::uint16_t instruction_address=0,segment=0,offset=0;std::uint8_t value=0; constexpr bool operator==(const MillenniumDosBdfOtherModeFarByteEffect&)const=default; };
class MillenniumDosBdfOtherModeSession {
public:
    MillenniumDosBdfOtherModeSession(std::span<const std::uint8_t>,std::uint8_t entry_dl,std::uint16_t entry_di);
    auto state()const{return state_;} MillenniumDosBdfOtherModeBoundary boundary()const;
    const auto& far_effects()const{return far_effects_;} const auto& far_byte_effects()const{return far_byte_effects_;} const auto& port_effects()const{return port_effects_;}
    void observe_runtime_byte(std::uint16_t,std::uint16_t,std::uint8_t);
    void observe_runtime_word(std::uint16_t,std::uint16_t,std::uint16_t);
private:
    MillenniumDosBdfOtherModeState state_; std::uint16_t entry_di_=0,segment_=0,source_=0x07fa,return_instruction_=0;std::uint8_t plane_=0,row_=0;
    std::vector<MillenniumDosBdfOtherModeFarEffect> far_effects_; std::vector<MillenniumDosBdfOtherModeFarByteEffect> far_byte_effects_; std::vector<MillenniumDosBdfOtherModePortEffect> port_effects_;
};
}
