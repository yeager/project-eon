#include "engine/millennium_dos_shared_helper_session.hpp"
#include "data/sha256.hpp"
#include <stdexcept>
namespace eon {
namespace {
std::optional<std::uint16_t> read_owned_word(const NativeRuntimeMemory& memory,
    const std::uint16_t segment, const std::uint16_t offset) {
    const NativeRuntimeLocation low_location{NativeRuntimeAddressSpace::dos_segmented,
        segment, offset};
    auto high_location = low_location;
    ++high_location.offset;
    const auto low = memory.read_byte(low_location);
    const auto high = memory.read_byte(high_location);
    if (!low && !high) return std::nullopt;
    if (!low || !high) throw std::runtime_error("Owned shared-helper operand is only partially initialized");
    return static_cast<std::uint16_t>(*low | (static_cast<std::uint16_t>(*high) << 8U));
}
}

NativeRuntimeMemoryApplyResult commit_millennium_dos_shared_helper_effect(
    NativeRuntimeMemory& memory, const std::uint64_t generation,
    const std::size_t effect_ordinal, const std::uint16_t child_code_segment,
    const MillenniumDosSharedHelperEffect& effect) {
    if (generation == 0 || effect_ordinal == 0 || child_code_segment == 0
        || effect.instruction_address != 0x066e || effect.address != 0x05c8
        || effect.value != 0) {
        return {false, "Shared-helper effect is detached from the hash-bound CS:$05c8 store"};
    }
    auto next = memory;
    const NativeRuntimeLocation location{NativeRuntimeAddressSpace::dos_segmented,
        child_code_segment, effect.address};
    NativeRuntimeEffectBatch batch{
        "millennium-dos-shared-helper-" + std::to_string(generation)
            + "-" + std::to_string(effect_ordinal)
            + "-memory-revision-" + std::to_string(memory.revision()),
        true,
        {{1, location, MemoryTransferElementWidth::byte,
            NativeRuntimeByteOrder::little_endian, effect.value}}};
    const auto applied = next.apply(batch);
    if (!applied.accepted) return applied;
    memory = std::move(next);
    return {true, {}};
}

NativeRuntimeMemoryApplyResult commit_millennium_dos_shared_helper_table_word(
    NativeRuntimeMemory& memory, const std::uint64_t generation,
    const std::uint16_t segment, const std::uint16_t offset,
    const std::uint16_t value) {
    if (generation == 0 || (offset & 1U) != 0U) {
        return {false, "Shared-helper table word is not an exact aligned runtime read"};
    }
    auto next = memory;
    const NativeRuntimeLocation location{NativeRuntimeAddressSpace::dos_segmented,
        segment, offset};
    auto high_location = location;
    ++high_location.offset;
    const auto low = next.read_byte(location);
    const auto high = next.read_byte(high_location);
    if (low || high) {
        if (!low || !high
            || static_cast<std::uint16_t>(*low
                | (static_cast<std::uint16_t>(*high) << 8U)) != value) {
            return {false, "Shared-helper table word contradicts owned DOS memory"};
        }
        return {true, {}};
    }
    NativeRuntimeEffectBatch batch{
        "millennium-dos-shared-helper-table-" + std::to_string(generation)
            + "-" + std::to_string(segment) + "-" + std::to_string(offset),
        true,
        {{1, location, MemoryTransferElementWidth::word,
            NativeRuntimeByteOrder::little_endian, value}}};
    const auto applied = next.apply(batch);
    if (!applied.accepted) return applied;
    memory = std::move(next);
    return {true, {}};
}

NativeRuntimeMemoryApplyResult validate_millennium_dos_shared_helper_segment_word(
    const NativeRuntimeMemory& memory, const std::uint16_t child_code_segment,
    const std::uint16_t value) {
    if (child_code_segment == 0) {
        return {false, "Shared-helper segment read has no active child segment"};
    }
    const NativeRuntimeLocation low_location{NativeRuntimeAddressSpace::dos_segmented,
        child_code_segment, 0x0116};
    auto high_location = low_location;
    ++high_location.offset;
    const auto low = memory.read_byte(low_location);
    const auto high = memory.read_byte(high_location);
    if (!low && !high) return {true, {}};
    if (!low || !high
        || static_cast<std::uint16_t>(*low
            | (static_cast<std::uint16_t>(*high) << 8U)) != value) {
        return {false, "Shared-helper segment observation contradicts owned CS:$0116"};
    }
    return {true, {}};
}

MillenniumDosSharedHelperSession::MillenniumDosSharedHelperSession(std::span<const std::uint8_t>b,std::uint16_t ax):table_offset_(std::uint16_t(ax<<1)){if(to_hex(sha256(b))!="427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57")throw std::runtime_error("Unsupported Millennium DOS $0666 helper profile");effects_.push_back({0x066e,0x05c8,0});}
MillenniumDosSharedHelperBoundary MillenniumDosSharedHelperSession::boundary()const{switch(state_){case MillenniumDosSharedHelperState::awaiting_segment:return{0x0669,0x0116};case MillenniumDosSharedHelperState::awaiting_table_word:return{0x0678,table_offset_,true,segment_};case MillenniumDosSharedHelperState::awaiting_helper_return:return{0x067b,0x05f7};case MillenniumDosSharedHelperState::returned:return{0x0681,0};}throw std::runtime_error("Invalid $0666 helper state");}
void MillenniumDosSharedHelperSession::observe_runtime_word(std::uint16_t i,std::uint16_t a,std::uint16_t v){if(state_!=MillenniumDosSharedHelperState::awaiting_segment||i!=0x0669||a!=0x0116)throw std::runtime_error("Detached $0666 segment");segment_=v;state_=MillenniumDosSharedHelperState::awaiting_table_word;}
void MillenniumDosSharedHelperSession::observe_far_word(std::uint16_t i,std::uint16_t s,std::uint16_t o,std::uint16_t v){if(state_!=MillenniumDosSharedHelperState::awaiting_table_word||i!=0x0678||s!=segment_||o!=table_offset_)throw std::runtime_error("Detached $0666 table word");selected_offset_=v;state_=MillenniumDosSharedHelperState::awaiting_helper_return;}
void MillenniumDosSharedHelperSession::observe_call_return(std::uint16_t i,std::uint16_t r){if(state_!=MillenniumDosSharedHelperState::awaiting_helper_return||i!=0x067b||r!=0x067e)throw std::runtime_error("Detached $0666 call return");state_=MillenniumDosSharedHelperState::returned;}

MillenniumDosSharedHelperOwnedAdvanceResult
advance_millennium_dos_shared_helper_from_owned_memory(
    MillenniumDosSharedHelperSession& helper, NativeRuntimeMemory& memory,
    const std::uint64_t generation, const std::uint16_t child_code_segment) {
    MillenniumDosSharedHelperOwnedAdvanceResult result;
    if (generation == 0 || child_code_segment == 0
        || helper.state() != MillenniumDosSharedHelperState::awaiting_segment) {
        result.error = "Owned shared-helper advance requires its initial active boundary";
        return result;
    }
    try {
        auto next_helper = helper;
        auto next_memory = memory;
        const auto data_segment = read_owned_word(next_memory, child_code_segment, 0x0116);
        if (!data_segment) throw std::runtime_error("Owned DOS child CS:$0116 is absent");
        next_helper.observe_runtime_word(0x0669, 0x0116, *data_segment);

        const auto boundary = next_helper.boundary();
        const auto table_word = read_owned_word(next_memory, boundary.segment, boundary.address);
        if (!table_word) throw std::runtime_error("Owned shared-helper far table word is absent");
        next_helper.observe_far_word(0x0678, boundary.segment, boundary.address, *table_word);

        const auto& effects = next_helper.effects();
        if (effects.size() != 1U) {
            throw std::runtime_error("Owned shared-helper effect count is not source-exact");
        }
        const auto committed = commit_millennium_dos_shared_helper_effect(
            next_memory, generation, effects.size(), child_code_segment, effects.front());
        if (!committed.accepted) throw std::runtime_error(committed.error);

        helper = std::move(next_helper);
        memory = std::move(next_memory);
        result.boundary = helper.boundary();
        result.accepted = true;
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}
}
