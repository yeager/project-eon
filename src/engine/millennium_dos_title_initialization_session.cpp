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
constexpr auto allocation_path_sha =
    "62bb857bf927ca3392900f9a8f26b9ab23f0780cd84c0ccf248f084e17c02ba7";
constexpr auto file_open_wrapper_sha =
    "06a31ffeae96544b136159050eabb961328023c749e073cd9e9e0b752a905884";
constexpr auto allocation_failure_sha =
    "d0f75b0f97509ff14ce1308b5a829de214523523b3fdc6ea7270df3a13e0ea5b";
constexpr auto allocation_caller_sha =
    "8f78c75697fe56993706c0b6ea69df78c90922b27775feaccb9f40071abbff1f";
constexpr auto title_library_name_sha =
    "62bfc3e4275f23097edf305a3e1144d3eac79b4a4c75cc35cfbb3eb0b9255aed";
constexpr auto file_helper_sha =
    "4fd3a9694c9ea36d7baf33607ed0b70ac764bb1f27bb6b686c3401bce5ef6b3d";
constexpr auto sized_allocation_prefix_sha =
    "b24d8fd1fa6200c9ea1cf43cfdd413e90089b63d0608efb3866beb8b782b5f3a";
constexpr auto allocation_tail_sha =
    "aa3738ee068dcdc02e63c90b3021d9da8672878ef0bae3af7a6ac35f50a3a578";
constexpr auto caller_to_library_sha =
    "e46d29382663f4876715eaf8aa2808956def21c90460f7b629c2adb213696f7c";
constexpr auto library_loader_sha =
    "63d5b5a645879a0a79ed0a7c880051e98ddf62b91f07616c0a72d035ee9581cf";
constexpr auto dos_io_helpers_sha =
    "d74f413ecf61f099d786f957f3f7a17e0044a78027bac844912b087e33d27b27";
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
            != other_mode_palette_sha
        || to_hex(sha256(titles_executable.subspan(0x1a1f,67)))
            != allocation_path_sha
        || to_hex(sha256(titles_executable.subspan(0x19f6,5)))
            != file_open_wrapper_sha
        || to_hex(sha256(titles_executable.subspan(0x1a7c,4)))
            != allocation_failure_sha
        || to_hex(sha256(titles_executable.subspan(0x1abb,10)))
            != allocation_caller_sha
        || to_hex(sha256(titles_executable.subspan(0x0d4e,10)))
            != title_library_name_sha
        || to_hex(sha256(titles_executable.subspan(0x19f6,41)))
            != file_helper_sha
        || to_hex(sha256(titles_executable.subspan(0x1a62,6)))
            != sized_allocation_prefix_sha
        || to_hex(sha256(titles_executable.subspan(0x1a62,30)))
            != allocation_tail_sha
        || to_hex(sha256(titles_executable.subspan(0x1abb,52)))
            != caller_to_library_sha
        || to_hex(sha256(titles_executable.subspan(0x0d5f,268)))
            != library_loader_sha
        || to_hex(sha256(titles_executable.subspan(0x0436,109)))
            != dos_io_helpers_sha) {
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

void MillenniumDosTitleInitializationSession::execute_title_main_allocation_start(
    const std::uint64_t sequence,const std::uint16_t call_address,
    const std::uint16_t call_target){
    if(state_!=MillenniumDosTitleInitializationState::title_main_allocation_call_boundary
        ||sequence!=last_sequence_+1||call_address!=0x1bb8||call_target!=0x1b1f
        ||call_address!=title_main_call_address_||call_target!=title_main_call_target_){
        throw std::runtime_error("Detached Millennium DOS title allocation entry");
    }
    effects_.insert(effects_.end(),{{0x1b1f,"ES",child_code_segment_},
        {0x1b21,"BX",0x1000},{0x1b24,"AH",0x004a}});
    const auto initial_al=static_cast<std::uint16_t>(
        selected_mode_==2?0:selected_mode_);
    dos_boundary_={0x1b26,0x1b28,0x21,0x4a,0xffff,
        static_cast<std::uint16_t>(0x4a00|initial_al),0x1000,
        child_code_segment_,0,false};
    last_sequence_=sequence;
    state_=MillenniumDosTitleInitializationState::dos_resize_result_boundary;
}

void MillenniumDosTitleInitializationSession::observe_dos_memory_result(
    const MillenniumDosTitleDosResultObservation& observation){
    const auto memory_boundary=
        state_>=MillenniumDosTitleInitializationState::dos_resize_result_boundary
            &&state_<=MillenniumDosTitleInitializationState::
                dos_second_buffer_allocation_result_boundary;
    const auto library_allocation_boundary=
        state_==MillenniumDosTitleInitializationState::
                dos_file_sized_allocation_result_boundary
            ||state_==MillenniumDosTitleInitializationState::
                dos_single_paragraph_allocation_result_boundary
            ||state_==MillenniumDosTitleInitializationState::
                dos_scratch_allocation_result_boundary
            ||state_==MillenniumDosTitleInitializationState::
                dos_scratch_free_result_boundary;
    if((!memory_boundary&&!library_allocation_boundary)
        ||observation.sequence!=last_sequence_+1
        ||observation.interrupt_address!=dos_boundary_.interrupt_address
        ||observation.return_address!=dos_boundary_.return_address
        ||observation.carry!=((observation.flags&0x0001U)!=0)){
        throw std::runtime_error("Detached Millennium DOS title DOS-memory result");
    }
    dos_results_.push_back({observation.sequence,observation.interrupt_address,
        observation.return_address,observation.carry,observation.ax,
        observation.bx,observation.flags});
    dos_boundary_.result_observed=true;
    last_sequence_=observation.sequence;
    const auto low=static_cast<std::uint16_t>(observation.ax&0x00ff);
    switch(state_){
    case MillenniumDosTitleInitializationState::dos_resize_result_boundary:
        effects_.push_back({0x1b28,"BX",0xfa00});
        effects_.push_back({0x1b2b,"AH",0x0048});
        dos_boundary_={0x1b2d,0x1b2f,0x21,0x48,0xffff,
            static_cast<std::uint16_t>(0x4800|low),0xfa00,0,0,false};
        state_=MillenniumDosTitleInitializationState::
            dos_large_allocation_result_boundary;
        return;
    case MillenniumDosTitleInitializationState::dos_large_allocation_result_boundary:
        memory_effects_.push_back({0x1b2f,0x1aa2,
            MillenniumDosTitleInitializationEffectWidth::word,observation.bx});
        effects_.push_back({0x1b34,"ES",observation.ax});
        effects_.push_back({0x1b36,"AH",0x0049});
        dos_boundary_={0x1b38,0x1b3a,0x21,0x49,0xffff,
            static_cast<std::uint16_t>(0x4900|low),observation.bx,
            observation.ax,0,false};
        state_=MillenniumDosTitleInitializationState::dos_free_result_boundary;
        return;
    case MillenniumDosTitleInitializationState::dos_free_result_boundary:
        effects_.push_back({0x1b3a,"BX",0x1000});
        effects_.push_back({0x1b3d,"AH",0x0048});
        dos_boundary_={0x1b3f,0x1b41,0x21,0x48,0xffff,
            static_cast<std::uint16_t>(0x4800|low),0x1000,0,0,false};
        state_=MillenniumDosTitleInitializationState::
            dos_first_buffer_allocation_result_boundary;
        return;
    case MillenniumDosTitleInitializationState::dos_first_buffer_allocation_result_boundary:
        memory_effects_.push_back({0x1b44,0x010e,
            MillenniumDosTitleInitializationEffectWidth::word,observation.ax});
        if(observation.carry){
            effects_.push_back({0x1b7c,"DX",1});
            memory_effects_.push_back({0x1bbb,0x1a9c,
                MillenniumDosTitleInitializationEffectWidth::word,observation.ax});
            failure_address_=0x1c6a;
            state_=MillenniumDosTitleInitializationState::allocation_failure_boundary;
            return;
        }
        effects_.push_back({0x1b4a,"BX",0x0fa1});
        effects_.push_back({0x1b4d,"AH",0x0048});
        dos_boundary_={0x1b4f,0x1b51,0x21,0x48,0xffff,
            static_cast<std::uint16_t>(0x4800|low),0x0fa1,0,0,false};
        state_=MillenniumDosTitleInitializationState::
            dos_second_buffer_allocation_result_boundary;
        return;
    case MillenniumDosTitleInitializationState::dos_second_buffer_allocation_result_boundary:
        memory_effects_.push_back({0x1b54,0x0112,
            MillenniumDosTitleInitializationEffectWidth::word,observation.ax});
        if(observation.carry){
            effects_.push_back({0x1b7c,"DX",1});
            memory_effects_.push_back({0x1bbb,0x1a9c,
                MillenniumDosTitleInitializationEffectWidth::word,observation.ax});
            failure_address_=0x1c6a;
            state_=MillenniumDosTitleInitializationState::allocation_failure_boundary;
            return;
        }
        effects_.push_back({0x1b5a,"DS",child_code_segment_});
        effects_.push_back({0x1b5c,"DX",0x0e4e});
        effects_.push_back({0x1af6,"AX",0x3d00});
        dos_boundary_={0x1af9,0x1afb,0x21,0x3d,0xffff,0x3d00,
            observation.bx,child_code_segment_,0x0e4e,false};
        dos_boundary_.source_address=0x0e4e;
        dos_boundary_.source_size=10;
        state_=MillenniumDosTitleInitializationState::dos_file_open_result_boundary;
        return;
    case MillenniumDosTitleInitializationState::dos_file_sized_allocation_result_boundary:
        if(observation.carry){
            effects_.push_back({0x1b7c,"DX",1});
            memory_effects_.push_back({0x1bbb,0x1a9c,
                MillenniumDosTitleInitializationEffectWidth::word,observation.ax});
            failure_address_=0x1c6a;
            state_=MillenniumDosTitleInitializationState::allocation_failure_boundary;
            return;
        }
        title_library_segment_=observation.ax;
        title_library_paragraphs_=dos_boundary_.bx;
        memory_effects_.push_back({0x1b6b,0x0e48,
            MillenniumDosTitleInitializationEffectWidth::word,observation.ax});
        effects_.push_back({0x1b6f,"BX",1});
        effects_.push_back({0x1b72,"AH",0x0048});
        dos_boundary_={0x1b74,0x1b76,0x21,0x48,0xffff,
            static_cast<std::uint16_t>(0x4800|low),1,
            child_code_segment_,0,false};
        state_=MillenniumDosTitleInitializationState::
            dos_single_paragraph_allocation_result_boundary;
        return;
    case MillenniumDosTitleInitializationState::dos_single_paragraph_allocation_result_boundary:
        memory_effects_.push_back({0x1b76,0x1a9e,
            MillenniumDosTitleInitializationEffectWidth::word,observation.ax});
        effects_.push_back({0x1b79,"DX",0});
        memory_effects_.push_back({0x1bbb,0x1a9c,
            MillenniumDosTitleInitializationEffectWidth::word,observation.ax});
        effects_.push_back({0x1bc5,"BX",0xfa00});
        effects_.push_back({0x1bc8,"AH",0x0048});
        dos_boundary_={0x1bca,0x1bcc,0x21,0x48,0xffff,
            static_cast<std::uint16_t>(0x4800|low),0xfa00,
            child_code_segment_,0,false};
        state_=MillenniumDosTitleInitializationState::
            dos_scratch_allocation_result_boundary;
        return;
    case MillenniumDosTitleInitializationState::dos_scratch_allocation_result_boundary:
        memory_effects_.push_back({0x1bcc,0x1aa4,
            MillenniumDosTitleInitializationEffectWidth::word,observation.bx});
        effects_.push_back({0x1bd1,"ES",observation.ax});
        effects_.push_back({0x1bd3,"AH",0x0049});
        dos_boundary_={0x1bd5,0x1bd7,0x21,0x49,0xffff,
            static_cast<std::uint16_t>(0x4900|low),observation.bx,
            observation.ax,0,false};
        state_=MillenniumDosTitleInitializationState::dos_scratch_free_result_boundary;
        return;
    case MillenniumDosTitleInitializationState::dos_scratch_free_result_boundary:
        effects_.insert(effects_.end(),{{0x1bd7,"DS",child_code_segment_},
            {0x1bda,"DS",title_library_segment_},{0x1bda,"DX",0},
            {0x1bde,"CX",0xffff},{0x1be2,"DS",child_code_segment_},
            {0x1be3,"DS",title_library_segment_},{0x1be3,"SI",0},
            {0x1be7,"AX",0x0a00},{0x1bea,"DS",child_code_segment_},
            {0x0e5f,"FLAGS.DF",0},{0x0e60,"DX",0x0e4e},
            {0x0545,"AL",2},{0x0547,"AH",0x003d}});
        dos_boundary_={0x0549,0x054b,0x21,0x3d,0xffff,0x3d02,
            observation.bx,child_code_segment_,0x0e4e,false};
        dos_boundary_.source_address=0x0e4e;
        dos_boundary_.source_size=10;
        state_=MillenniumDosTitleInitializationState::dos_library_open_result_boundary;
        return;
    default: break;
    }
    throw std::runtime_error("Unsupported Millennium DOS title DOS-memory state");
}

void MillenniumDosTitleInitializationSession::observe_dos_file_result(
    const MillenniumDosTitleDosFileResultObservation& observation,
    const std::span<const std::uint8_t> title_library){
    if((state_!=MillenniumDosTitleInitializationState::dos_file_open_result_boundary
            &&state_!=MillenniumDosTitleInitializationState::dos_file_seek_result_boundary
            &&state_!=MillenniumDosTitleInitializationState::dos_file_close_result_boundary
            &&state_!=MillenniumDosTitleInitializationState::dos_library_open_result_boundary
            &&state_!=MillenniumDosTitleInitializationState::dos_library_read_result_boundary
            &&state_!=MillenniumDosTitleInitializationState::dos_library_close_result_boundary)
        ||observation.sequence!=last_sequence_+1
        ||observation.interrupt_address!=dos_boundary_.interrupt_address
        ||observation.return_address!=dos_boundary_.return_address
        ||observation.carry!=((observation.flags&0x0001U)!=0)){
        throw std::runtime_error("Detached Millennium DOS title file result");
    }
    const auto library_state=
        state_==MillenniumDosTitleInitializationState::dos_library_open_result_boundary
        ||state_==MillenniumDosTitleInitializationState::dos_library_read_result_boundary
        ||state_==MillenniumDosTitleInitializationState::dos_library_close_result_boundary;
    if(library_state&&(title_library.size()!=18907
        ||to_hex(sha256(title_library))
            !="6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678")){
        throw std::runtime_error("Exact Millennium DOS TITLE.LIB is unavailable");
    }
    if(state_==MillenniumDosTitleInitializationState::dos_library_read_result_boundary
        &&!observation.carry){
        const auto count=static_cast<std::uint32_t>(observation.ax);
        const auto capacity=static_cast<std::uint32_t>(title_library_paragraphs_)*16U;
        const auto displacement=
            static_cast<std::uint32_t>(dos_boundary_.segment-title_library_segment_)*16U
            +dos_boundary_.dx;
        if(count>dos_boundary_.cx||title_library_cursor_+count>title_library.size()
            ||(count!=0&&displacement+count>capacity)){
            throw std::runtime_error("Millennium DOS TITLE.LIB read exceeds source or buffer");
        }
    }
    if(state_==MillenniumDosTitleInitializationState::dos_library_close_result_boundary
        &&title_library_first_read_count_<6){
        throw std::runtime_error("Millennium DOS TITLE.LIB header was not loaded");
    }
    dos_file_results_.push_back({observation.sequence,observation.interrupt_address,
        observation.return_address,observation.carry,observation.ax,observation.bx,
        observation.cx,observation.dx,observation.flags});
    dos_boundary_.result_observed=true;
    last_sequence_=observation.sequence;
    switch(state_){
    case MillenniumDosTitleInitializationState::dos_file_open_result_boundary:
        if(observation.carry){
            failure_address_=0x05a3;
            state_=MillenniumDosTitleInitializationState::dos_file_failure_boundary;
            return;
        }
        dos_file_handle_=observation.ax;
        effects_.insert(effects_.end(),{{0x1b01,"BX",observation.ax},
            {0x1b02,"DX",0},{0x1b04,"CX",0},{0x1b06,"AX",0x4202}});
        dos_boundary_={0x1b09,0x1b0b,0x21,0x42,0xffff,0x4202,
            dos_file_handle_,child_code_segment_,0,false};
        state_=MillenniumDosTitleInitializationState::dos_file_seek_result_boundary;
        return;
    case MillenniumDosTitleInitializationState::dos_file_seek_result_boundary:
        if(observation.carry){
            failure_address_=0x05a3;
            state_=MillenniumDosTitleInitializationState::dos_file_failure_boundary;
            return;
        }
        dos_file_length_low_=observation.ax;
        effects_.push_back({0x1b0d,"BX",dos_file_handle_});
        effects_.push_back({0x1b0f,"AX",0x3e00});
        dos_boundary_={0x1b12,0x1b14,0x21,0x3e,0xffff,0x3e00,
            dos_file_handle_,child_code_segment_,observation.dx,false};
        dos_boundary_.cx=observation.cx;
        state_=MillenniumDosTitleInitializationState::dos_file_close_result_boundary;
        return;
    case MillenniumDosTitleInitializationState::dos_file_close_result_boundary: {
        const auto rounded=static_cast<std::uint16_t>(dos_file_length_low_+0x000f);
        const auto paragraphs=static_cast<std::uint16_t>(rounded>>4U);
        effects_.insert(effects_.end(),{{0x1b14,"AX",dos_file_length_low_},
            {0x1b15,"AX",rounded},{0x1b18,"CL",4},
            {0x1b1a,"AX",paragraphs},{0x1b1c,"BX",paragraphs},
            {0x1b62,"AH",0x0048}});
        dos_boundary_={0x1b64,0x1b66,0x21,0x48,0xffff,
            static_cast<std::uint16_t>(0x4800|(paragraphs&0x00ff)),
            paragraphs,child_code_segment_,observation.dx,false};
        dos_boundary_.cx=static_cast<std::uint16_t>(
            (observation.cx&0xff00U)|0x0004U);
        state_=MillenniumDosTitleInitializationState::
            dos_file_sized_allocation_result_boundary;
        return;
    }
    case MillenniumDosTitleInitializationState::dos_library_open_result_boundary:
        memory_effects_.push_back({0x054b,0x19c6,
            MillenniumDosTitleInitializationEffectWidth::word,observation.ax});
        if(observation.carry){
            failure_address_=0x0e6a;
            state_=MillenniumDosTitleInitializationState::dos_file_failure_boundary;
            return;
        }
        title_library_handle_=observation.ax;
        effects_.insert(effects_.end(),{{0x0e80,"CX",0x8000},
            {0x0e83,"DS",title_library_segment_},{0x0e83,"DX",0},
            {0x0575,"BX",title_library_handle_},{0x057a,"AH",0x003f}});
        dos_boundary_={0x057c,0x057e,0x21,0x3f,0xff00,0x3f00,
            title_library_handle_,title_library_segment_,0,false};
        dos_boundary_.cx=0x8000;
        title_library_cursor_=0;
        title_library_read_index_=0;
        state_=MillenniumDosTitleInitializationState::dos_library_read_result_boundary;
        return;
    case MillenniumDosTitleInitializationState::dos_library_read_result_boundary: {
        if(!observation.carry){
            if(title_library_read_index_==0)
                title_library_first_read_count_=observation.ax;
            for(std::uint32_t index=0;index<observation.ax;++index){
                memory_effects_.push_back({0x057c,
                    static_cast<std::uint16_t>(dos_boundary_.dx+index),
                    MillenniumDosTitleInitializationEffectWidth::byte,
                    title_library[title_library_cursor_+index],dos_boundary_.segment});
            }
            title_library_cursor_+=observation.ax;
        }
        ++title_library_read_index_;
        if(title_library_read_index_<9){
            constexpr std::array<std::uint16_t,9> segment_add{{
                0,0,0x1000,0x1000,0x2000,0x2000,0x3000,0x3000,0x4000}};
            constexpr std::array<std::uint16_t,9> offsets{{
                0,0x8000,0,0x8000,0,0x8000,0,0x8000,0}};
            const auto index=title_library_read_index_;
            const auto segment=static_cast<std::uint16_t>(
                title_library_segment_+segment_add[index]);
            const auto count=static_cast<std::uint16_t>(index==8?0xa000:0x8000);
            effects_.insert(effects_.end(),{{0x0575,"BX",title_library_handle_},
                {0x057a,"AH",0x003f}});
            dos_boundary_={0x057c,0x057e,0x21,0x3f,0xff00,0x3f00,
                title_library_handle_,segment,offsets[index],false};
            dos_boundary_.cx=count;
            return;
        }
        effects_.push_back({0x0597,"BX",title_library_handle_});
        effects_.push_back({0x059c,"AH",0x003e});
        dos_boundary_={0x059e,0x05a0,0x21,0x3e,0xffff,
            static_cast<std::uint16_t>(0x3e00|(observation.ax&0x00ffU)),
            title_library_handle_,child_code_segment_,observation.dx,false};
        dos_boundary_.cx=observation.cx;
        state_=MillenniumDosTitleInitializationState::dos_library_close_result_boundary;
        return;
    }
    case MillenniumDosTitleInitializationState::dos_library_close_result_boundary: {
        const auto word=[](const std::span<const std::uint8_t> bytes,
                           const std::size_t offset){
            return static_cast<std::uint16_t>(bytes[offset]
                |static_cast<std::uint16_t>(bytes[offset+1])<<8U);
        };
        const auto entry_count=word(title_library,0);
        const auto directory_offset=word(title_library,2);
        const auto segment_delta=static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(title_library[4])<<8U);
        const auto unnormalised=static_cast<std::uint16_t>(directory_offset);
        const auto relocated_offset=static_cast<std::uint16_t>(unnormalised&0x000fU);
        const auto relocated_segment=static_cast<std::uint16_t>(
            title_library_segment_+segment_delta+(unnormalised>>4U));
        memory_effects_.push_back({0x0f2c,0x0e5d,
            MillenniumDosTitleInitializationEffectWidth::word,entry_count});
        memory_effects_.push_back({0x0f56,0x0e4a,
            MillenniumDosTitleInitializationEffectWidth::word,relocated_offset});
        memory_effects_.push_back({0x0f59,0x0e4c,
            MillenniumDosTitleInitializationEffectWidth::word,relocated_segment});
        effects_.insert(effects_.end(),{{0x0f25,"DS",title_library_segment_},
            {0x0f25,"SI",0},{0x0f2a,"AX",entry_count},
            {0x0f30,"CX",0},{0x0f32,"DX",title_library_segment_},
            {0x0f37,"AX",directory_offset},{0x0f3d,"BX",directory_offset},
            {0x0f3f,"AX",title_library[4]},
            {0x0f4c,"DX",static_cast<std::uint16_t>(title_library_segment_+segment_delta)},
            {0x0f4e,"CX",directory_offset},{0x0f5d,"DS",child_code_segment_},
            {0x0f60,"ES",child_code_segment_},{0x0f62,"DS",child_code_segment_}});
        continuation_address_=selected_mode_==1?0x0f6b:0x0f6a;
        state_=MillenniumDosTitleInitializationState::library_relocation_complete;
        return;
    }
    default: break;
    }
    throw std::runtime_error("Unsupported Millennium DOS title file state");
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
        title_main_call_target_,dos_boundary_,dos_results_,dos_file_results_,
        failure_address_,continuation_address_};
}

} // namespace eon
