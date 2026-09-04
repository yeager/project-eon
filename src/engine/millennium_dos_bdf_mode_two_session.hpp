#pragma once
#include <cstdint>
#include <span>
#include <vector>
namespace eon{
enum class MillenniumDosBdfModeTwoState{awaiting_toggle,awaiting_zero_segment,awaiting_zero_source_word,awaiting_nonzero_segment,awaiting_nonzero_far_word,transform_boundary,returned};
struct MillenniumDosBdfModeTwoBoundary{std::uint16_t instruction_address=0,runtime_address=0;bool far_memory=false;std::uint16_t segment=0;constexpr bool operator==(const MillenniumDosBdfModeTwoBoundary&)const=default;};
struct MillenniumDosBdfModeTwoFarEffect{std::uint16_t instruction_address=0,segment=0,offset=0,value=0;constexpr bool operator==(const MillenniumDosBdfModeTwoFarEffect&)const=default;};
struct MillenniumDosBdfModeTwoRuntimeEffect{std::uint16_t instruction_address=0,runtime_address=0,value=0;constexpr bool operator==(const MillenniumDosBdfModeTwoRuntimeEffect&)const=default;};
class MillenniumDosBdfModeTwoSession{public:MillenniumDosBdfModeTwoSession(std::span<const std::uint8_t>,std::uint16_t entry_di);auto state()const{return state_;}MillenniumDosBdfModeTwoBoundary boundary()const;const auto&far_effects()const{return far_effects_;}const auto&runtime_effects()const{return runtime_effects_;}void observe_runtime_byte(std::uint16_t,std::uint16_t,std::uint8_t);void observe_runtime_word(std::uint16_t,std::uint16_t,std::uint16_t);void observe_far_word(std::uint16_t,std::uint16_t,std::uint16_t,std::uint16_t);private:static std::uint16_t next_plane(std::uint16_t);void advance_zero();void advance_nonzero();MillenniumDosBdfModeTwoState state_=MillenniumDosBdfModeTwoState::awaiting_toggle;std::uint16_t segment_=0,entry_di_=0,current_offset_=0,source_=0x07fa,destination_=0;std::uint8_t word_=0,row_=0;std::vector<MillenniumDosBdfModeTwoFarEffect>far_effects_;std::vector<MillenniumDosBdfModeTwoRuntimeEffect>runtime_effects_;};
}
