#pragma once
#include "engine/native_runtime_memory.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>
namespace eon {
enum class MillenniumDosSharedHelperState { awaiting_segment,awaiting_table_word,awaiting_helper_return,returned };
struct MillenniumDosSharedHelperBoundary { std::uint16_t instruction_address=0;std::uint16_t address=0;bool far_memory=false;std::uint16_t segment=0;constexpr bool operator==(const MillenniumDosSharedHelperBoundary&)const=default; };
struct MillenniumDosSharedHelperEffect { std::uint16_t instruction_address=0,address=0;std::uint8_t value=0;constexpr bool operator==(const MillenniumDosSharedHelperEffect&)const=default; };
[[nodiscard]] NativeRuntimeMemoryApplyResult
commit_millennium_dos_shared_helper_effect(NativeRuntimeMemory& memory,
    std::uint64_t generation, std::size_t effect_ordinal,
    std::uint16_t child_code_segment, const MillenniumDosSharedHelperEffect& effect);
[[nodiscard]] NativeRuntimeMemoryApplyResult
commit_millennium_dos_shared_helper_table_word(NativeRuntimeMemory& memory,
    std::uint64_t generation, std::uint16_t segment, std::uint16_t offset,
    std::uint16_t value);
[[nodiscard]] NativeRuntimeMemoryApplyResult
validate_millennium_dos_shared_helper_segment_word(
    const NativeRuntimeMemory& memory, std::uint16_t child_code_segment,
    std::uint16_t value);
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

struct MillenniumDosSharedHelperOwnedAdvanceResult {
    bool accepted = false;
    std::string error;
    std::optional<MillenniumDosSharedHelperBoundary> boundary;
};

// Executes the two hash-bound helper reads only when their complete operands
// already exist in the active child/DS images. Applies the proven $066e store
// atomically and stops before the unresolved $05f7 helper call.
[[nodiscard]] MillenniumDosSharedHelperOwnedAdvanceResult
advance_millennium_dos_shared_helper_from_owned_memory(
    MillenniumDosSharedHelperSession& helper, NativeRuntimeMemory& memory,
    std::uint64_t generation, std::uint16_t child_code_segment);
}
