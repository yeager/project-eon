#pragma once
#include <cstdint>
#include <span>
#include <vector>
namespace eon {
enum class MillenniumDosSharedHelperState { awaiting_segment,awaiting_table_word,awaiting_helper_return,returned };
struct MillenniumDosSharedHelperBoundary { std::uint16_t instruction_address=0;std::uint16_t address=0;bool far_memory=false;std::uint16_t segment=0;constexpr bool operator==(const MillenniumDosSharedHelperBoundary&)const=default; };
struct MillenniumDosSharedHelperEffect { std::uint16_t instruction_address=0,address=0;std::uint8_t value=0;constexpr bool operator==(const MillenniumDosSharedHelperEffect&)const=default; };
class MillenniumDosSharedHelperSession {
public:
    MillenniumDosSharedHelperSession(std::span<const std::uint8_t>,std::uint16_t caller_ax);
    auto state()const{return state_;} MillenniumDosSharedHelperBoundary boundary()const;const auto&effects()const{return effects_;}
    void observe_runtime_word(std::uint16_t,std::uint16_t,std::uint16_t);
    void observe_far_word(std::uint16_t,std::uint16_t,std::uint16_t,std::uint16_t);
    void observe_call_return(std::uint16_t,std::uint16_t);
    std::uint16_t selected_offset()const{return selected_offset_;}
private:
    MillenniumDosSharedHelperState state_=MillenniumDosSharedHelperState::awaiting_segment;std::uint16_t segment_=0,table_offset_=0,selected_offset_=0;std::vector<MillenniumDosSharedHelperEffect>effects_;
};
}
