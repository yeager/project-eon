#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
namespace eon {
enum class MillenniumDosBdfServiceState{awaiting_active_byte,returned_by_active,awaiting_segment_word,awaiting_source_word,awaiting_toggle_byte,awaiting_poll_return,awaiting_mode_byte,awaiting_mapping_return,awaiting_second_mode_byte,external_mode_two_jump,external_other_mode_jump,awaiting_copy_toggle_byte,far_memory_nonzero_boundary,far_memory_zero_boundary};
enum class MillenniumDosBdfServiceBoundaryKind{runtime_byte,runtime_word,call_return,local_return,external_jump,far_memory};
struct MillenniumDosBdfServiceBoundary{MillenniumDosBdfServiceBoundaryKind kind;std::uint16_t instruction_address;std::optional<std::uint16_t> runtime_address;std::optional<std::uint16_t> call_target;std::optional<std::uint16_t> known_ax;constexpr bool operator==(const MillenniumDosBdfServiceBoundary&)const=default;};
struct MillenniumDosBdfServiceEffect{std::uint16_t instruction_address;std::uint16_t runtime_address;std::uint8_t width;std::uint16_t value;constexpr bool operator==(const MillenniumDosBdfServiceEffect&)const=default;};
class MillenniumDosBdfServiceSession{public:explicit MillenniumDosBdfServiceSession(std::span<const std::uint8_t>);[[nodiscard]]auto state()const{return state_;}[[nodiscard]]MillenniumDosBdfServiceBoundary boundary()const;[[nodiscard]]const auto&effects()const{return effects_;}void observe_runtime_byte(std::uint16_t,std::uint16_t,std::uint8_t);void observe_runtime_word(std::uint16_t,std::uint16_t,std::uint16_t);void observe_poll_return(std::uint16_t,std::uint16_t,std::uint16_t,std::uint16_t);void observe_mapping_return(std::uint16_t,std::uint16_t,std::uint16_t);private:MillenniumDosBdfServiceState state_=MillenniumDosBdfServiceState::awaiting_active_byte;std::uint8_t initial_toggle_=0;std::uint16_t observed_cx_=0,computed_ax_=0;std::vector<MillenniumDosBdfServiceEffect>effects_;};
}
