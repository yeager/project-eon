#include "engine/millennium_dos_title_initialization_session.hpp"

#include "data/sha256.hpp"

#include <array>
#include <stdexcept>

namespace eon {
namespace {
constexpr auto titles_sha =
    "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
constexpr auto startup_sha =
    "6bb7c15471e42155d44449cf6e814a538f3a0ee686126f7c2befa91cfb0d08d7";
constexpr auto wrapper_request_sha =
    "f7dee937ac756b0aa6c9b287ba8dcf985d7a6fe539612de66cd4871184d85680";
constexpr auto result_continuation_sha =
    "4ffa7a86b6e398183f251b7de848cefe76ed4e10fd9ddd95b5c8548539fb2704";
constexpr auto mode_one_callee_sha =
    "a4db63f6cc6d8ba1004340b3f25b1d21299bd14a3466189d0bb495434c5849a2";
constexpr auto other_mode_callee_sha =
    "0dab61c355813642910e49ec8fecc80def19a584a51a8323b3ad0e644468a5fe";
constexpr auto mode_one_followup_sha =
    "1c2afa83de99564ceb8e9168f7d6fa586ef7ba21ec2b7d1bdaad9291ec3efc0a";
constexpr auto other_mode_followup_sha =
    "111aabbae0194a132060f1acd6cc5d6c100ccb9c64facdb64c90785a845e6c6b";
constexpr auto mode_one_palette_sha =
    "9d1fdeadf710e7f0a6736f172415e15d7db87480588ec771327f30128afb43e9";
constexpr auto other_mode_palette_sha =
    "ce46bce999708ea5109a857b0b6ecc02ece34eaf431cd148ef1aa1c0e80aed0a";
}

MillenniumDosTitleInitializationSession::MillenniumDosTitleInitializationSession(
    const std::span<const std::uint8_t> titles_executable,
    const std::uint16_t child_code_segment, const std::uint64_t entry_sequence)
    : last_sequence_(entry_sequence), child_code_segment_(child_code_segment) {
    constexpr std::size_t startup_offset = 0x1a80;
    constexpr std::size_t startup_size = 24;
    constexpr std::size_t wrapper_offset = 0x22;
    constexpr std::size_t wrapper_request_size = 7;
    constexpr std::size_t result_continuation_offset = 0x1a98;
    constexpr std::size_t result_continuation_size = 29;
    constexpr std::size_t mode_one_callee_offset = 0x19c6;
    constexpr std::size_t other_mode_callee_offset = 0x19da;
    constexpr std::size_t selected_callee_prefix_size = 11;
    constexpr std::size_t mode_one_followup_offset = 0x034c;
    constexpr std::size_t mode_one_followup_size = 35;
    constexpr std::size_t other_mode_followup_offset = 0x0387;
    constexpr std::size_t other_mode_followup_size = 18;
    if (titles_executable.size() != 7022
        || to_hex(sha256(titles_executable)) != titles_sha
        || child_code_segment == 0 || entry_sequence == 0
        || to_hex(sha256(titles_executable.subspan(startup_offset, startup_size)))
            != startup_sha
        || to_hex(sha256(titles_executable.subspan(
               wrapper_offset, wrapper_request_size))) != wrapper_request_sha
        || to_hex(sha256(titles_executable.subspan(result_continuation_offset,
               result_continuation_size))) != result_continuation_sha
        || to_hex(sha256(titles_executable.subspan(mode_one_callee_offset,
               selected_callee_prefix_size))) != mode_one_callee_sha
        || to_hex(sha256(titles_executable.subspan(other_mode_callee_offset,
               selected_callee_prefix_size))) != other_mode_callee_sha
        || to_hex(sha256(titles_executable.subspan(mode_one_followup_offset,
               mode_one_followup_size))) != mode_one_followup_sha
        || to_hex(sha256(titles_executable.subspan(other_mode_followup_offset,
               other_mode_followup_size))) != other_mode_followup_sha
        || to_hex(sha256(titles_executable.subspan(0x004c,48)))
            != mode_one_palette_sha
        || to_hex(sha256(titles_executable.subspan(0x0377,16)))
            != other_mode_palette_sha) {
        throw std::runtime_error("Unsupported Millennium DOS title initialization media");
    }
}

void MillenniumDosTitleInitializationSession::
observe_selected_callee_private_interrupt_result(
    const MillenniumDosTitleSelectedCalleeResultObservation& observation) {
    const auto expected_return=static_cast<std::uint16_t>(
        selected_call_target_==0x1ac6?0x1ad1:0x1ae5);
    if(state_!=MillenniumDosTitleInitializationState::
            selected_callee_private_interrupt_result_boundary
        ||observation.sequence!=last_sequence_+1
        ||observation.interrupt_address!=0x0127
        ||observation.wrapper_return_address!=0x0129
        ||observation.selected_callee_return_address!=expected_return){
        throw std::runtime_error(
            "Detached Millennium DOS selected-callee private-interrupt result");
    }
    selected_callee_observed_ax_=observation.ax;
    selected_callee_observed_flags_=observation.flags;
    selected_followup_call_address_=expected_return;
    selected_followup_call_target_=static_cast<std::uint16_t>(
        selected_call_target_==0x1ac6?0x044c:0x0487);
    selected_callee_boundary_.result_observed=true;
    last_sequence_=observation.sequence;
    state_=MillenniumDosTitleInitializationState::selected_followup_call_boundary;
}

void MillenniumDosTitleInitializationSession::execute_selected_followup_start(
    const std::uint64_t sequence,
    const std::uint16_t selected_followup_call_address,
    const std::uint16_t selected_followup_call_target) {
    if(state_!=MillenniumDosTitleInitializationState::selected_followup_call_boundary
        ||sequence!=last_sequence_+1
        ||selected_followup_call_address!=selected_followup_call_address_
        ||selected_followup_call_target!=selected_followup_call_target_){
        throw std::runtime_error("Detached Millennium DOS title BIOS continuation");
    }
    if(selected_followup_call_target_==0x044c){
        memory_effects_.push_back({0x044e,0x0107,
            MillenniumDosTitleInitializationEffectWidth::byte,1});
        effects_.insert(effects_.end(),{
            {0x044c,"AL",1},{0x0452,"DS",child_code_segment_},
            {0x0454,"SI",0x014c},{0x0457,"CX",0x0010},
            {0x045b,"BX",0x0010},{0x045e,"BX",0x0000},
            {0x0461,"DH",0},{0x0464,"CH",0},{0x0467,"CL",0},
            {0x0469,"AH",0x10},{0x046b,"AL",0x10}});
        bios_boundary_={selected_followup_call_address_,0x044c,0x046d,0x10,
            0x1010,0,0,0xff00,0,0x014c,false};
    } else if(selected_followup_call_target_==0x0487){
        effects_.insert(effects_.end(),{
            {0x0487,"DS",child_code_segment_},{0x0489,"SI",0x0477},
            {0x048c,"CX",0x0010},{0x048f,"BL",0},
            {0x0492,"BH",0},{0x0494,"AX",0x1000}});
        bios_boundary_={selected_followup_call_address_,0x0487,0x0497,0x10,
            0x1000,0,0x0010,0,0,0x0477,false};
    } else {
        throw std::runtime_error("Unsupported Millennium DOS title BIOS continuation");
    }
    last_sequence_=sequence;
    state_=MillenniumDosTitleInitializationState::bios_palette_interrupt_boundary;
}

void MillenniumDosTitleInitializationSession::observe_bios_palette_result(
    const MillenniumDosTitleBiosResultObservation& observation,
    const std::span<const std::uint8_t> titles_executable) {
    if(state_!=MillenniumDosTitleInitializationState::bios_palette_interrupt_boundary
        ||observation.sequence!=last_sequence_+1
        ||observation.interrupt_address!=bios_boundary_.interrupt_address
        ||observation.return_address
            !=static_cast<std::uint16_t>(
                selected_followup_call_target_==0x044c?0x046f:0x0499)
        ||titles_executable.size()!=7022
        ||to_hex(sha256(titles_executable))!=titles_sha
        ||bios_results_.size()>=16){
        throw std::runtime_error("Detached Millennium DOS title BIOS result");
    }
    bios_results_.push_back({observation.sequence,observation.interrupt_address,
        observation.return_address,observation.ax,observation.flags});
    bios_boundary_.result_observed=true;
    last_sequence_=observation.sequence;
    const auto next_index=bios_results_.size();
    if(next_index<16){
        if(selected_followup_call_target_==0x044c){
            const auto source=static_cast<std::uint16_t>(0x014c+next_index*3);
            const auto file_offset=static_cast<std::size_t>(source-0x0100);
            const auto red=titles_executable[file_offset];
            const auto green=titles_executable[file_offset+1];
            const auto blue=titles_executable[file_offset+2];
            bios_boundary_={selected_followup_call_address_,0x044c,0x046d,0x10,
                0x1010,static_cast<std::uint16_t>(next_index),
                static_cast<std::uint16_t>((green<<8U)|blue),0xff00,
                static_cast<std::uint16_t>(red<<8U),source,false};
        } else {
            const auto source=static_cast<std::uint16_t>(0x0477+next_index);
            const auto value=titles_executable[source-0x0100];
            bios_boundary_={selected_followup_call_address_,0x0487,0x0497,0x10,
                0x1000,static_cast<std::uint16_t>((value<<8U)|next_index),
                static_cast<std::uint16_t>(16-next_index),0,0,source,false};
        }
        return;
    }

    if(selected_followup_call_target_==0x044c){
        effects_.push_back({0x0472,"BL",0x000f});
        effects_.push_back({0x1ad4,"AL",1});
        memory_effects_.push_back({0x1ad6,0x0107,
            MillenniumDosTitleInitializationEffectWidth::byte,1});
    } else {
        effects_.push_back({0x0499,"BL",0x0010});
        effects_.push_back({0x1ae8,"AL",selected_mode_});
        if(selected_mode_==2){
            effects_.push_back({0x1aef,"AX",0xb800});
            memory_effects_.push_back({0x1af2,0x010a,
                MillenniumDosTitleInitializationEffectWidth::word,0xb800});
        }
    }
    effects_.push_back({0x1bb6,"DS",child_code_segment_});
    title_main_call_address_=0x1bb8;
    title_main_call_target_=0x1b1f;
    last_sequence_=observation.sequence;
    state_=MillenniumDosTitleInitializationState::title_main_allocation_call_boundary;
}

void MillenniumDosTitleInitializationSession::execute_selected_callee_start(
    const std::uint64_t sequence, const std::uint16_t selected_call_address,
    const std::uint16_t selected_call_target) {
    if(state_!=MillenniumDosTitleInitializationState::selected_local_call_boundary
        ||sequence!=last_sequence_+1||selected_call_address!=selected_call_address_
        ||selected_call_target!=selected_call_target_){
        throw std::runtime_error("Detached Millennium DOS selected title callee");
    }
    const bool mode_one=selected_call_target_==0x1ac6;
    if((mode_one&&(selected_call_address_!=0x1bad))
        ||(!mode_one&&(selected_call_address_!=0x1bb2
            ||selected_call_target_!=0x1ada))){
        throw std::runtime_error("Unsupported Millennium DOS selected title callee");
    }
    const auto entry=selected_call_target_;
    const auto wrapper_call=static_cast<std::uint16_t>(mode_one?0x1ace:0x1ae2);
    effects_.push_back({entry,"AX",0x0004});
    effects_.push_back({static_cast<std::uint16_t>(entry+3),"ES",
        child_code_segment_});
    effects_.push_back({static_cast<std::uint16_t>(entry+5),"BX",0x1ac5});
    selected_callee_boundary_={wrapper_call,0x0122,0x0127,0x91,0x0004,
        child_code_segment_,0x1ac5,false,false};
    last_sequence_=sequence;
    state_=MillenniumDosTitleInitializationState::
        selected_callee_private_interrupt_result_boundary;
}

void MillenniumDosTitleInitializationSession::observe_private_interrupt_result(
    const MillenniumDosTitlePrivateInterruptResultObservation& observation) {
    if (state_
            != MillenniumDosTitleInitializationState::private_interrupt_result_boundary
        || observation.sequence != last_sequence_ + 1
        || observation.interrupt_address != 0x0127
        || observation.return_address != 0x0129) {
        throw std::runtime_error("Detached Millennium DOS title private-interrupt result");
    }
    observed_ax_ = observation.ax;
    observed_flags_ = observation.flags;
    selected_mode_ = static_cast<std::uint8_t>(observation.ax >> 8U);
    memory_effects_ = {
        {0x1b98,0x1a9c,MillenniumDosTitleInitializationEffectWidth::word,
            observation.ax},
        {0x1b9e,0x1aaa,MillenniumDosTitleInitializationEffectWidth::byte,
            selected_mode_},
        {0x1ba2,0x0107,MillenniumDosTitleInitializationEffectWidth::byte,
            selected_mode_},
        {0x1ba5,0x1aa0,MillenniumDosTitleInitializationEffectWidth::word,
            0xda00},
    };
    if (selected_mode_ == 1) {
        selected_call_address_ = 0x1bad;
        selected_call_target_ = 0x1ac6;
    } else {
        selected_call_address_ = 0x1bb2;
        selected_call_target_ = 0x1ada;
    }
    boundary_.result_observed = true;
    last_sequence_ = observation.sequence;
    state_ = MillenniumDosTitleInitializationState::selected_local_call_boundary;
}

void MillenniumDosTitleInitializationSession::execute_exact_startup(
    const std::uint64_t sequence, const std::uint16_t entry_address,
    const std::uint16_t call_address, const std::uint16_t wrapper_address,
    const std::uint8_t interrupt) {
    if (state_ != MillenniumDosTitleInitializationState::awaiting_entry
        || sequence != last_sequence_ + 1 || entry_address != 0x1b80
        || call_address != 0x1b95 || wrapper_address != 0x0122
        || interrupt != 0x91) {
        throw std::runtime_error("Detached Millennium DOS title initialization");
    }

    effects_ = {
        {0x1b80, "DS", child_code_segment_},
        {0x1b82, "ES", child_code_segment_},
        {0x1b84, "AX", child_code_segment_},
        {0x1b86, "SS", child_code_segment_},
        {0x1b88, "AX", 0xda00},
        {0x1b8b, "SP", 0xda00},
        {0x1b8d, "AX", 0x0000},
        {0x1b90, "ES", child_code_segment_},
        {0x1b92, "BX", 0x1ac4},
    };
    boundary_ = {call_address, wrapper_address, 0x0127, interrupt, 0x0000,
        child_code_segment_, 0x1ac4, false, false};
    last_sequence_ = sequence;
    state_ = MillenniumDosTitleInitializationState::private_interrupt_result_boundary;
}

MillenniumDosTitleInitializationCheckpoint
MillenniumDosTitleInitializationSession::checkpoint() const {
    return {state_,last_sequence_,child_code_segment_,effects_,memory_effects_,
        boundary_,observed_ax_,observed_flags_,selected_mode_,
        selected_call_address_,selected_call_target_,selected_callee_boundary_,
        selected_callee_observed_ax_,selected_callee_observed_flags_,
        selected_followup_call_address_,selected_followup_call_target_,
        bios_boundary_,bios_results_,title_main_call_address_,
        title_main_call_target_};
}

} // namespace eon
