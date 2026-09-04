#pragma once
#include <cstdint>
#include <span>
#include <vector>
namespace eon{
enum class MillenniumDosBdfModeTwoState{awaiting_toggle,awaiting_segment,awaiting_source_word,nonzero_boundary,returned};
struct MillenniumDosBdfModeTwoBoundary{std::uint16_t instruction_address=0,runtime_address=0;constexpr bool operator==(const MillenniumDosBdfModeTwoBoundary&)const=default;};
struct MillenniumDosBdfModeTwoFarEffect{std::uint16_t instruction_address=0,segment=0,offset=0,value=0;constexpr bool operator==(const MillenniumDosBdfModeTwoFarEffect&)const=default;};
class MillenniumDosBdfModeTwoSession{public:MillenniumDosBdfModeTwoSession(std::span<const std::uint8_t>,std::uint16_t entry_di);auto state()const{return state_;}MillenniumDosBdfModeTwoBoundary boundary()const;const auto&far_effects()const{return effects_;}void observe_runtime_byte(std::uint16_t,std::uint16_t,std::uint8_t);void observe_runtime_word(std::uint16_t,std::uint16_t,std::uint16_t);private:static std::uint16_t next_plane(std::uint16_t);MillenniumDosBdfModeTwoState state_=MillenniumDosBdfModeTwoState::awaiting_toggle;std::uint16_t segment_=0,di_=0,source_=0x07fa;std::uint8_t word_=0,row_=0;std::vector<MillenniumDosBdfModeTwoFarEffect>effects_;};
}
