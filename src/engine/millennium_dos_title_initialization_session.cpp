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
constexpr auto non_mode_one_return_sha =
    "66c5cf6c6a51f92ec93650c960546e562bd382e96d85f93ab15df8b5a82982b0";
constexpr auto post_library_call_sha =
    "802c3d3da0e9eebe7f5ccfaac938d6c4eba76d4dc6ffb190ef9d719d0a0c4044";
constexpr auto post_library_callee_sha =
    "687219b5f60eaec217f38860abfc29c8c407b95cb809caa61878e6db3a4ce454";
constexpr auto vector_query_prefix_sha =
    "a00fdf978777b8b563efc5c4d39f3e3fbafea0ef764f134c6ee308d9927b6e73";
constexpr auto vector_zero_install_sha =
    "2b274ecea07db05da2e4f091e648ba5bbec8132d34d60668d47fd57681ae854b";
constexpr auto vector_four_query_sha =
    "918d021be641065df0e5519ec984e3d556fb7300e6db321a79cc6b591a54c933";
constexpr auto vector_four_install_sha =
    "b78f3be0ba4b6067faaf00309ac1bf821468fae7ef2ec46e43c575de8f95860e";
constexpr auto vector_setup_return_sha =
    "f32140aa070695a63e56de66fdcdb32c78b2d378318715dc2d8da83a349f0787";
constexpr auto first_bios_setup_sha =
    "5531aa8efe777cde6344e051bee61deb3e45e685e91c345006caf34bf306b0a7";
constexpr auto setup_return_sha =
    "ae3f4619b0413d70d3004b9131c3752153074e45725be13b9a148978895e359e";
constexpr auto next_setup_call_sha =
    "8e9933fc8751a312d2c247e94987439ae91db1d7e288c14190035b2d6c3da1c8";
constexpr auto next_setup_callee_sha =
    "9c04e42a78762c9c76a807afa61b40ffc12e61e5aa5408fa11a252bdb81dba54";
constexpr auto followup_setup_prefix_sha =
    "00c5baf9b1d28d3216e6375b48f79ace14faac8b8037e6689703c6b510941d9d";
constexpr auto vector_hook_suffix_sha =
    "7744cad5e7d132e889a6b64095bd9a8c0d61726b46399e549c923ffa459603ff";
constexpr auto video_hook_prefix_sha =
    "31e40c32854737bd7eb5e63cfdf1da8d6a4b592793993f02ae1eef102f0d85b4";
constexpr auto video_hook_suffix_sha =
    "5f72f7b8f67574d774c5ba8e480cd8257accfab90651d94836d356edbe738861";
constexpr auto post_video_hook_branch_sha =
    "a111bf870ff60815e5d9f6a8c5d3a765335dcc8d77e1b0034b185b0872a3ec4d";
constexpr auto post_video_setup_sha =
    "c35f93db0d58443d76374684ed2c54ce78ddb7fc8e01ffa809026382450b4868";
constexpr auto post_video_setup_caller_sha =
    "b4b7b699e4630db6451b59ae2fe1eb6515b4c441ab5e60f2205746036efdf7fb";
constexpr auto graphics_request_sha =
    "d17cc200504c832c3062e1c6951c753a8819c0fd1255b7273c28b3fcf1f3e363";
constexpr auto descriptor_caller_sha =
    "646ada76ab8f0b370cd3e1f3001cf2e21a5105bbcf650cf6239bf801853754dd";
constexpr auto descriptor_prefix_sha =
    "f6be40d902e1d36bd640df417e6a3b8e813b4fce0c7bbf7801a33ae44d60a897";
constexpr auto descriptor_pointer_suffix_sha =
    "e8b21803c3739aac65b59a9919f03c97d0d55daf7fd2a35e7567973765724921";
constexpr auto descriptor_dimensions_sha =
    "787613791d00d3ae372e3ec9b7b02d56a0704b9e14b44e2d6874b125927befe6";
constexpr auto descriptor_product_adjust_sha =
    "0653c7fb33f8d3c60d973b7c038f4c724ffd194abd7f21990762340477246ed4";
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
            != dos_io_helpers_sha
        || to_hex(sha256(titles_executable.subspan(0x0e5d,14)))
            != non_mode_one_return_sha
        || to_hex(sha256(titles_executable.subspan(0x1aef,3)))
            != post_library_call_sha
        || to_hex(sha256(titles_executable.subspan(0x19ac,7)))
            != post_library_callee_sha
        || to_hex(sha256(titles_executable.subspan(0x0fec,10)))
            != vector_query_prefix_sha
        || to_hex(sha256(titles_executable.subspan(0x0ff6,18)))
            != vector_zero_install_sha
        || to_hex(sha256(titles_executable.subspan(0x1008,5)))
            != vector_four_query_sha
        || to_hex(sha256(titles_executable.subspan(0x100d,18)))
            != vector_four_install_sha
        || to_hex(sha256(titles_executable.subspan(0x101f,5)))
            != vector_setup_return_sha
        || to_hex(sha256(titles_executable.subspan(0x19b3,8)))
            != first_bios_setup_sha
        || to_hex(sha256(titles_executable.subspan(0x19c3,1)))
            != setup_return_sha
        || to_hex(sha256(titles_executable.subspan(0x1af2,3)))
            != next_setup_call_sha
        || to_hex(sha256(titles_executable.subspan(0x10a7,49)))
            != next_setup_callee_sha
        || to_hex(sha256(titles_executable.subspan(0x104e,15)))
            != followup_setup_prefix_sha
        || to_hex(sha256(titles_executable.subspan(0x105d,27)))
            != vector_hook_suffix_sha
        || to_hex(sha256(titles_executable.subspan(0x11a0,13)))
            != video_hook_prefix_sha
        || to_hex(sha256(titles_executable.subspan(0x11ad,19)))
            != video_hook_suffix_sha
        || to_hex(sha256(titles_executable.subspan(0x1afb,10)))
            != post_video_hook_branch_sha
        || to_hex(sha256(titles_executable.subspan(0x125e,42)))
            != post_video_setup_sha
        || to_hex(sha256(titles_executable.subspan(0x1b0a,10)))
            != post_video_setup_caller_sha
        || to_hex(sha256(titles_executable.subspan(0x0ef3,16)))
            != graphics_request_sha
        || to_hex(sha256(titles_executable.subspan(0x1625,27)))
            != descriptor_caller_sha
        || to_hex(sha256(titles_executable.subspan(0x1290,26)))
            != descriptor_prefix_sha
        || to_hex(sha256(titles_executable.subspan(0x12aa,35)))
            != descriptor_pointer_suffix_sha
        || to_hex(sha256(titles_executable.subspan(0x12d0,18)))
            != descriptor_dimensions_sha
        || to_hex(sha256(titles_executable.subspan(0x12e2,7)))
            != descriptor_product_adjust_sha) {
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
        ||bios_results_.size()>=(post_video_repeat_?32U:16U)){
        throw std::runtime_error("Detached Millennium DOS title BIOS result");
    }
    bios_results_.push_back({observation.sequence,observation.interrupt_address,
        observation.return_address,observation.ax,observation.flags});
    bios_boundary_.result_observed=true;
    last_sequence_=observation.sequence;
    const auto next_index=bios_results_.size()%16U;
    if(next_index!=0){
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
    if(post_video_repeat_){
        effects_.insert(effects_.end(),{{0x1c0b,"DS",child_code_segment_},
            {0x1c0d,"ES",child_code_segment_}});
        last_sequence_=observation.sequence;
        continuation_address_=0x1c0e;
        state_=MillenniumDosTitleInitializationState::post_video_setup_call_boundary;
        return;
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

void MillenniumDosTitleInitializationSession::execute_post_relocation(
    const std::uint64_t sequence,
    const std::span<const std::uint8_t> title_library){
    if(state_!=MillenniumDosTitleInitializationState::library_relocation_complete
        ||sequence!=last_sequence_+1||title_library.size()!=18907
        ||to_hex(sha256(title_library))
            !="6bc6484fbea66a8e4eaf61b53d7eeab62a358b2c76a40897cca9f80c861b7678")
        throw std::runtime_error("Detached Millennium DOS TITLE.LIB palette setup");
    if(selected_mode_!=1){
        if(continuation_address_!=0x0f6a)
            throw std::runtime_error("Detached Millennium DOS TITLE.LIB return");
        effects_.insert(effects_.end(),{{0x0f5d,"DS",child_code_segment_},
            {0x0f60,"ES",child_code_segment_},{0x0f62,"DS",child_code_segment_},
            {0x0f63,"AL",selected_mode_}});
        last_sequence_=sequence;
        continuation_address_=0x1bef;
        state_=MillenniumDosTitleInitializationState::post_library_setup_call_boundary;
        return;
    }
    if(continuation_address_!=0x0f6b)
        throw std::runtime_error("Detached Millennium DOS TITLE.LIB palette setup");
    constexpr std::size_t directory=0x4813;
    const auto directory_delta=static_cast<std::uint16_t>(
        title_library[directory]|title_library[directory+1]<<8U);
    const auto palette_delta=static_cast<std::uint16_t>(
        title_library[directory+4+0x1a]|title_library[directory+4+0x1b]<<8U);
    memory_effects_.push_back({0x0f81,0x0e59,
        MillenniumDosTitleInitializationEffectWidth::word,directory_delta});
    memory_effects_.push_back({0x0f8e,0x0e5b,
        MillenniumDosTitleInitializationEffectWidth::word,title_library_segment_});
    for(std::size_t index=0;index<0x300;++index)
        memory_effects_.push_back({0x0f9c,static_cast<std::uint16_t>(0x014c+index),
            MillenniumDosTitleInitializationEffectWidth::byte,0});
    effects_.insert(effects_.end(),{{0x0f93,"DI",0x014c},{0x0f97,"AX",0},
        {0x0f99,"CX",0x0180},{0x0fba,"AX",palette_delta},
        {0x0fbe,"SI",static_cast<std::uint16_t>(0x4817+palette_delta)},
        {0x0fc0,"SI",static_cast<std::uint16_t>(0x4833+palette_delta)},
        {0x0fc3,"CX",0x0180}});
    last_sequence_=sequence;
    continuation_address_=0x0fc6;
    state_=MillenniumDosTitleInitializationState::library_palette_copy_boundary;
}

void MillenniumDosTitleInitializationSession::execute_post_library_setup(
    const std::uint64_t sequence,const std::uint16_t call_address,
    const std::uint16_t call_target){
    if(state_!=MillenniumDosTitleInitializationState::post_library_setup_call_boundary
        ||sequence!=last_sequence_+1||call_address!=0x1bef||call_target!=0x1aac
        ||continuation_address_!=call_address)
        throw std::runtime_error("Detached Millennium DOS post-library setup");
    effects_.insert(effects_.end(),{{0x1aac,"DS",child_code_segment_},
        {0x1aae,"ES",child_code_segment_},{0x10f0,"FLAGS.DF",0},
        {0x10f1,"AX",0x3500}});
    dos_boundary_={0x10f4,0x10f6,0x21,0x35,0xffff,0x3500,
        0,child_code_segment_,0,false};
    last_sequence_=sequence;
    continuation_address_=0x10f4;
    state_=MillenniumDosTitleInitializationState::dos_get_vector_zero_result_boundary;
}

void MillenniumDosTitleInitializationSession::observe_dos_vector_result(
    const MillenniumDosTitleDosVectorResultObservation& observation){
    if((state_!=MillenniumDosTitleInitializationState::dos_get_vector_zero_result_boundary
            &&state_!=MillenniumDosTitleInitializationState::dos_set_vector_zero_result_boundary
            &&state_!=MillenniumDosTitleInitializationState::dos_get_vector_four_result_boundary
            &&state_!=MillenniumDosTitleInitializationState::dos_set_vector_four_result_boundary)
        ||observation.sequence!=last_sequence_+1
        ||observation.interrupt_address!=dos_boundary_.interrupt_address
        ||observation.return_address!=dos_boundary_.return_address)
        throw std::runtime_error("Detached Millennium DOS vector-zero result");
    dos_vector_results_.push_back({observation.sequence,observation.interrupt_address,
        observation.return_address,observation.ax,observation.bx,observation.es,
        observation.flags});
    last_sequence_=observation.sequence;
    if(state_==MillenniumDosTitleInitializationState::dos_set_vector_four_result_boundary){
        effects_.insert(effects_.end(),{{0x1ab3,"AH",1},{0x1ab5,"AL",0x1b},
            {0x1ab7,"BL",0x46}});
        setup_bios_boundary_={0x1ab9,0x1abb,0x15,0xffff,0x011b,0xffff,
            static_cast<std::uint16_t>((observation.bx&0xff00U)|0x0046U),false};
        continuation_address_=0x1ab9;
        state_=MillenniumDosTitleInitializationState::bios_int15_first_result_boundary;
        return;
    }
    if(state_==MillenniumDosTitleInitializationState::dos_get_vector_four_result_boundary){
        memory_effects_.push_back({0x110d,0x10e8,
            MillenniumDosTitleInitializationEffectWidth::word,observation.bx});
        memory_effects_.push_back({0x1111,0x10ea,
            MillenniumDosTitleInitializationEffectWidth::word,observation.es});
        effects_.insert(effects_.end(),{{0x1115,"DS",child_code_segment_},
            {0x1117,"AX",0x2504},{0x111a,"DX",0x1124}});
        dos_boundary_={0x111d,0x111f,0x21,0x25,0xffff,0x2504,
            observation.bx,child_code_segment_,0x1124,false};
        continuation_address_=0x111d;
        state_=MillenniumDosTitleInitializationState::dos_set_vector_four_result_boundary;
        return;
    }
    if(state_==MillenniumDosTitleInitializationState::dos_set_vector_zero_result_boundary){
        effects_.push_back({0x1108,"AX",0x3504});
        dos_boundary_={0x110b,0x110d,0x21,0x35,0xffff,0x3504,
            observation.bx,observation.es,0,false};
        continuation_address_=0x110b;
        state_=MillenniumDosTitleInitializationState::dos_get_vector_four_result_boundary;
        return;
    }
    memory_effects_.push_back({0x10f6,0x10e4,
        MillenniumDosTitleInitializationEffectWidth::word,observation.bx});
    memory_effects_.push_back({0x10fa,0x10e6,
        MillenniumDosTitleInitializationEffectWidth::word,observation.es});
    effects_.insert(effects_.end(),{{0x10fe,"DS",child_code_segment_},
        {0x1100,"AX",0x2500},{0x1103,"DX",0x1124}});
    dos_boundary_={0x1106,0x1108,0x21,0x25,0xffff,0x2500,
        observation.bx,child_code_segment_,0x1124,false};
    continuation_address_=0x1106;
    state_=MillenniumDosTitleInitializationState::dos_set_vector_zero_result_boundary;
}

void MillenniumDosTitleInitializationSession::observe_setup_bios_result(
    const MillenniumDosTitleSetupBiosResultObservation& observation){
    if((state_!=MillenniumDosTitleInitializationState::bios_int15_first_result_boundary
            &&state_!=MillenniumDosTitleInitializationState::bios_int15_second_result_boundary)
        ||observation.sequence!=last_sequence_+1
        ||observation.interrupt_address!=setup_bios_boundary_.interrupt_address
        ||observation.return_address!=setup_bios_boundary_.return_address)
        throw std::runtime_error("Detached Millennium DOS setup BIOS result");
    setup_bios_results_.push_back(observation);
    setup_bios_boundary_.result_observed=true;
    last_sequence_=observation.sequence;
    if(state_==MillenniumDosTitleInitializationState::bios_int15_second_result_boundary){
        continuation_address_=0x1bf2;
        state_=MillenniumDosTitleInitializationState::post_library_next_setup_call_boundary;
        return;
    }
    effects_.insert(effects_.end(),{{0x1abb,"AH",1},{0x1abd,"AL",0x1c},
        {0x1abf,"BL",0x46}});
    setup_bios_boundary_={0x1ac1,0x1ac3,0x15,0xffff,0x011c,0xffff,
        static_cast<std::uint16_t>((observation.bx&0xff00U)|0x0046U),false};
    continuation_address_=0x1ac1;
    state_=MillenniumDosTitleInitializationState::bios_int15_second_result_boundary;
}

void MillenniumDosTitleInitializationSession::execute_next_setup(
    const std::uint64_t sequence,const std::uint16_t call_address,
    const std::uint16_t call_target){
    if(state_!=MillenniumDosTitleInitializationState::post_library_next_setup_call_boundary
        ||sequence!=last_sequence_+1||call_address!=0x1bf2||call_target!=0x11a7
        ||continuation_address_!=call_address)
        throw std::runtime_error("Detached Millennium DOS next setup call");
    memory_effects_.push_back({0x11a7,0x118d,
        MillenniumDosTitleInitializationEffectWidth::byte,0});
    memory_effects_.push_back({0x11cd,0x1181,
        MillenniumDosTitleInitializationEffectWidth::word,0});
    memory_effects_.push_back({0x11d1,0x1183,
        MillenniumDosTitleInitializationEffectWidth::word,0x0444});
    memory_effects_.push_back({0x11d1,0x1185,
        MillenniumDosTitleInitializationEffectWidth::word,0x1178});
    effects_.insert(effects_.end(),{{0x11ac,"AX",0},{0x11af,"SI",0x1179},
        {0x11c5,"DS",child_code_segment_},{0x11c7,"ES",child_code_segment_},
        {0x11c9,"FLAGS.DF",0},{0x11ca,"DI",0x1181},{0x11ce,"CX",2}});
    last_sequence_=sequence;
    continuation_address_=0x1bf5;
    state_=MillenniumDosTitleInitializationState::post_library_followup_call_boundary;
}

void MillenniumDosTitleInitializationSession::execute_followup_setup(
    const std::uint64_t sequence,const std::uint16_t call_address,
    const std::uint16_t call_target){
    if(state_!=MillenniumDosTitleInitializationState::post_library_followup_call_boundary
        ||sequence!=last_sequence_+1||call_address!=0x1bf5||call_target!=0x114e
        ||continuation_address_!=call_address)
        throw std::runtime_error("Detached Millennium DOS followup setup call");
    effects_.insert(effects_.end(),{{0x1152,"DS",child_code_segment_},
        {0x1154,"ES",child_code_segment_},{0x1156,"DI",0x10dc},
        {0x1159,"DS",0},{0x1159,"SI",0x0070}});
    far_read_boundary_={0x115d,0,0x0070,2,child_code_segment_,0x10dc};
    last_sequence_=sequence;
    continuation_address_=0x115d;
    state_=MillenniumDosTitleInitializationState::timer_vector_far_read_boundary;
}

void MillenniumDosTitleInitializationSession::observe_far_words(
    const MillenniumDosTitleFarWordsObservation& observation){
    const auto boundary_state=state_;
    if((boundary_state!=MillenniumDosTitleInitializationState::timer_vector_far_read_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::video_vector_far_read_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::graphics_descriptor_far_read_boundary)
        ||observation.sequence!=last_sequence_+1
        ||observation.instruction_address!=far_read_boundary_.instruction_address
        ||observation.source_segment!=far_read_boundary_.source_segment
        ||observation.source_offset!=far_read_boundary_.source_offset)
        throw std::runtime_error("Detached Millennium DOS vector far words");
    std::uint16_t descriptor_destination_segment=0;
    if(boundary_state==MillenniumDosTitleInitializationState::graphics_descriptor_far_read_boundary){
        if(observation.first_word!=0x0006||observation.second_word!=0x0000)
            throw std::runtime_error("Contradictory Millennium DOS TITLE.LIB descriptor words");
        for(auto it=memory_effects_.rbegin();it!=memory_effects_.rend();++it)
            if(!it->explicit_segment&&it->offset==0x0e48
                &&it->width==MillenniumDosTitleInitializationEffectWidth::word){
                descriptor_destination_segment=it->value;break;
            }
        if(descriptor_destination_segment==0)
            throw std::runtime_error("Missing Millennium DOS descriptor destination");
    }
    far_word_observations_.push_back(observation);
    if(boundary_state==MillenniumDosTitleInitializationState::graphics_descriptor_far_read_boundary){
        const auto destination_segment=descriptor_destination_segment;
        memory_effects_.push_back({0x13c0,0x138c,
            MillenniumDosTitleInitializationEffectWidth::word,0x0006});
        memory_effects_.push_back({0x13c3,0x138e,
            MillenniumDosTitleInitializationEffectWidth::word,destination_segment});
        effects_.insert(effects_.end(),{{0x13ab,"BX",0x0006},
            {0x13ad,"AX",0},{0x13ae,"AH",0},{0x13b0,"AL",0},
            {0x13b2,"CX",4},{0x13b5,"AX",0},{0x13b7,"DX",destination_segment},
            {0x13b9,"CX",0x0006},{0x013c,"AX",0x0006},
            {0x0141,"AX",0},{0x0149,"DX",destination_segment},
            {0x13c4,"DS",child_code_segment_},{0x13c6,"SI",0x0006},
            {0x13c6,"DS",destination_segment},{0x13cb,"BX",0x0006}});
        far_read_boundary_={0x13cd,destination_segment,0x001e,1,
            child_code_segment_,0x1359};
        last_sequence_=observation.sequence;
        continuation_address_=0x13cd;
        state_=MillenniumDosTitleInitializationState::graphics_record_word_read_boundary;
        return;
    }
    if(boundary_state==MillenniumDosTitleInitializationState::video_vector_far_read_boundary){
        memory_effects_.push_back({0x12ad,0x1266,
            MillenniumDosTitleInitializationEffectWidth::word,observation.first_word});
        memory_effects_.push_back({0x12ae,0x1268,
            MillenniumDosTitleInitializationEffectWidth::word,observation.second_word});
        memory_effects_.push_back({0x12b9,0x0024,
            MillenniumDosTitleInitializationEffectWidth::word,0x126a,0,true});
        memory_effects_.push_back({0x12bc,0x0026,
            MillenniumDosTitleInitializationEffectWidth::word,child_code_segment_,0,true});
        effects_.insert(effects_.end(),{{0x12af,"ES",0},{0x12b1,"DS",child_code_segment_},
            {0x12b3,"DI",0x0024},{0x12b6,"AX",0x126a},
            {0x12ba,"AX",child_code_segment_},{0x12bd,"ES",child_code_segment_}});
        last_sequence_=observation.sequence;
        continuation_address_=0x1c02;
        state_=MillenniumDosTitleInitializationState::post_video_hook_mode_call_boundary;
        return;
    }
    memory_effects_.push_back({0x115d,0x10dc,
        MillenniumDosTitleInitializationEffectWidth::word,observation.first_word});
    memory_effects_.push_back({0x115e,0x10de,
        MillenniumDosTitleInitializationEffectWidth::word,observation.second_word});
    memory_effects_.push_back({0x1168,0x0070,
        MillenniumDosTitleInitializationEffectWidth::word,0x11d8,0,true});
    memory_effects_.push_back({0x116b,0x0072,
        MillenniumDosTitleInitializationEffectWidth::word,child_code_segment_,0,true});
    memory_effects_.push_back({0x1170,0x112c,
        MillenniumDosTitleInitializationEffectWidth::byte,1});
    effects_.insert(effects_.end(),{{0x115f,"DS",child_code_segment_},
        {0x1161,"ES",0},{0x1161,"DI",0x0070},{0x1165,"AX",0x11d8},
        {0x1169,"AX",child_code_segment_},{0x116c,"ES",child_code_segment_},
        {0x116e,"AL",1}});
    last_sequence_=observation.sequence;
    continuation_address_=0x1bf8;
    state_=MillenniumDosTitleInitializationState::post_vector_hook_call_boundary;
}

void MillenniumDosTitleInitializationSession::execute_video_hook_setup(
    const std::uint64_t sequence,const std::uint16_t call_address,
    const std::uint16_t call_target){
    if(state_!=MillenniumDosTitleInitializationState::post_vector_hook_call_boundary
        ||sequence!=last_sequence_+1||call_address!=0x1bf8||call_target!=0x12a0
        ||continuation_address_!=call_address)
        throw std::runtime_error("Detached Millennium DOS video-hook setup");
    effects_.insert(effects_.end(),{{0x12a0,"FLAGS.DF",0},
        {0x12a1,"ES",child_code_segment_},{0x12a3,"DI",0x1266},
        {0x12a6,"AX",0},{0x12a8,"DS",0},{0x12aa,"SI",0x0024}});
    far_read_boundary_={0x12ad,0,0x0024,2,child_code_segment_,0x1266};
    last_sequence_=sequence;
    continuation_address_=0x12ad;
    state_=MillenniumDosTitleInitializationState::video_vector_far_read_boundary;
}

void MillenniumDosTitleInitializationSession::observe_far_word(
    const MillenniumDosTitleFarWordObservation& observation){
    const auto boundary_state=state_;
    if((boundary_state!=MillenniumDosTitleInitializationState::graphics_record_word_read_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::graphics_record_second_word_read_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::graphics_record_third_word_read_boundary)
        ||observation.sequence!=last_sequence_+1
        ||observation.instruction_address!=far_read_boundary_.instruction_address
        ||observation.source_segment!=far_read_boundary_.source_segment
        ||observation.source_offset!=far_read_boundary_.source_offset
        ||observation.word!=(boundary_state==MillenniumDosTitleInitializationState::graphics_record_word_read_boundary?0x0140
            :boundary_state==MillenniumDosTitleInitializationState::graphics_record_second_word_read_boundary?0x00c8:0x0000))
        throw std::runtime_error("Detached Millennium DOS record word");
    far_single_word_observations_.push_back(observation);
    if(boundary_state==MillenniumDosTitleInitializationState::graphics_record_third_word_read_boundary){
        memory_effects_.push_back({0x13e5,0x138a,
            MillenniumDosTitleInitializationEffectWidth::word,0xfa00});
        effects_.push_back({0x13e2,"AX",0xfa00});
        far_byte_boundary_={0x13e9,observation.source_segment,0x0007,0x1389};
        last_sequence_=observation.sequence;
        continuation_address_=0x13e9;
        state_=MillenniumDosTitleInitializationState::graphics_record_byte_read_boundary;
        return;
    }
    if(boundary_state==MillenniumDosTitleInitializationState::graphics_record_second_word_read_boundary){
        memory_effects_.push_back({0x13d3,0x1357,
            MillenniumDosTitleInitializationEffectWidth::word,observation.word});
        memory_effects_.push_back({0x13d8,0x1359,
            MillenniumDosTitleInitializationEffectWidth::word,0x0140});
        memory_effects_.push_back({0x13de,0x133b,
            MillenniumDosTitleInitializationEffectWidth::word,0xfa00});
        effects_.insert(effects_.end(),{{0x13d0,"CX",observation.word},
            {0x13dc,"AX",0xfa00}});
        far_read_boundary_={0x13e2,observation.source_segment,0x001a,1,
            child_code_segment_,0x138a};
        last_sequence_=observation.sequence;
        continuation_address_=0x13e2;
        state_=MillenniumDosTitleInitializationState::graphics_record_third_word_read_boundary;
        return;
    }
    effects_.push_back({0x13cd,"AX",observation.word});
    far_read_boundary_={0x13d0,observation.source_segment,0x001c,1,
        child_code_segment_,0x1357};
    last_sequence_=observation.sequence;
    continuation_address_=0x13d0;
    state_=MillenniumDosTitleInitializationState::graphics_record_second_word_read_boundary;
}

void MillenniumDosTitleInitializationSession::execute_post_video_mode_call(
    const std::uint64_t sequence,const std::uint16_t call_address,
    const std::uint16_t call_target){
    if(state_!=MillenniumDosTitleInitializationState::post_video_hook_mode_call_boundary
        ||sequence!=last_sequence_+1||call_address!=0x1c02||call_target!=0x1ada
        ||continuation_address_!=call_address||selected_mode_==1)
        throw std::runtime_error("Detached Millennium DOS post-video mode call");
    effects_.insert(effects_.end(),{{0x1ada,"AX",0x0004},
        {0x1add,"ES",child_code_segment_},{0x1adf,"BX",0x1ac5}});
    selected_callee_boundary_={0x1ae2,0x0122,0x0127,0x91,0x0004,
        child_code_segment_,0x1ac5,false};
    post_video_repeat_=true;
    last_sequence_=sequence;
    continuation_address_=0x0127;
    state_=MillenniumDosTitleInitializationState::selected_callee_private_interrupt_result_boundary;
}

void MillenniumDosTitleInitializationSession::execute_post_video_setup(
    const std::uint64_t sequence,const std::uint16_t call_address,
    const std::uint16_t call_target){
    if(state_!=MillenniumDosTitleInitializationState::post_video_setup_call_boundary
        ||sequence!=last_sequence_+1||call_address!=0x1c0e||call_target!=0x135e
        ||continuation_address_!=call_address||selected_mode_==1)
        throw std::runtime_error("Detached Millennium DOS post-video setup");
    const auto pointer_cell=static_cast<std::uint16_t>(selected_mode_==1?0x010c:0x0110);
    std::uint16_t source_segment=0;
    for(auto it=memory_effects_.rbegin();it!=memory_effects_.rend();++it)
        if(!it->explicit_segment&&it->offset==static_cast<std::uint16_t>(pointer_cell+2)
            &&it->width==MillenniumDosTitleInitializationEffectWidth::word){
            source_segment=it->value;break;
        }
    if(source_segment==0)
        throw std::runtime_error("Missing Millennium DOS post-video source segment");
    memory_effects_.push_back({0x1376,0x1341,
        MillenniumDosTitleInitializationEffectWidth::word,0});
    memory_effects_.push_back({0x137c,0x1343,
        MillenniumDosTitleInitializationEffectWidth::word,source_segment});
    memory_effects_.push_back({0x1384,0x134b,
        MillenniumDosTitleInitializationEffectWidth::word,child_code_segment_});
    effects_.insert(effects_.end(),{{0x135f,"DS",child_code_segment_},
        {0x1361,"ES",child_code_segment_},{0x1362,"SI",0},
        {0x1362,"DS",source_segment},{0x1367,"AL",selected_mode_},
        {0x136f,"SI",0},{0x136f,"DS",source_segment},
        {0x1374,"AX",0},{0x137a,"AX",source_segment},
        {0x1380,"AX",child_code_segment_},{0x1382,"DS",child_code_segment_}});
    last_sequence_=sequence;
    continuation_address_=0x1c11;
    state_=MillenniumDosTitleInitializationState::post_video_graphics_call_boundary;
}

void MillenniumDosTitleInitializationSession::execute_post_video_graphics_call(
    const std::uint64_t sequence,const std::uint16_t call_address,
    const std::uint16_t call_target){
    if(state_!=MillenniumDosTitleInitializationState::post_video_graphics_call_boundary
        ||sequence!=last_sequence_+1||call_address!=0x1c11||call_target!=0x0ff3
        ||continuation_address_!=call_address||selected_mode_==1)
        throw std::runtime_error("Detached Millennium DOS graphics request");
    effects_.insert(effects_.end(),{{0x0ff3,"AX",child_code_segment_},
        {0x0ff5,"ES",child_code_segment_},{0x0ff7,"BX",0x0fe9},
        {0x0ffd,"AX",0x0019}});
    memory_effects_.push_back({0x0ffa,0x0ff1,
        MillenniumDosTitleInitializationEffectWidth::word,child_code_segment_});
    boundary_={0x1000,0x0122,0x0127,0x91,0x0019,
        child_code_segment_,0x0fe9,false,false};
    last_sequence_=sequence;
    continuation_address_=0x0127;
    state_=MillenniumDosTitleInitializationState::post_video_private_interrupt_result_boundary;
}

void MillenniumDosTitleInitializationSession::execute_post_video_followup(
    const std::uint64_t sequence,const std::uint16_t call_address,
    const std::uint16_t call_target){
    if(state_!=MillenniumDosTitleInitializationState::post_video_followup_call_boundary
        ||sequence!=last_sequence_+1||call_address!=0x1c17||call_target!=0x1725
        ||continuation_address_!=call_address||selected_mode_==1)
        throw std::runtime_error("Detached Millennium DOS descriptor setup");
    const auto owned_word=[this](const std::uint16_t offset){
        for(auto it=memory_effects_.rbegin();it!=memory_effects_.rend();++it)
            if(!it->explicit_segment&&it->offset==offset
                &&it->width==MillenniumDosTitleInitializationEffectWidth::word)
                return it->value;
        throw std::runtime_error("Missing Millennium DOS descriptor pointer");
    };
    const auto entry_count=owned_word(0x0e5d);
    if(static_cast<std::int16_t>(0)>static_cast<std::int16_t>(entry_count))
        throw std::runtime_error("Unsupported Millennium DOS descriptor count");
    const auto source_offset=owned_word(0x0e4a);
    const auto source_segment=owned_word(0x0e4c);
    constexpr std::uint16_t destination_offset=0; // hash-bound TITLES.EXE word
    const auto destination_segment=owned_word(0x0e48);
    effects_.insert(effects_.end(),{{0x1726,"DS",child_code_segment_},
        {0x1728,"ES",child_code_segment_},{0x1729,"CX",entry_count},
        {0x1733,"AX",0},{0x1735,"CX",0},{0x1737,"AX",0},
        {0x1390,"CX",12},{0x1393,"AX",0},{0x1395,"SI",source_offset},
        {0x1395,"DS",source_segment},{0x139a,"DI",destination_offset},
        {0x139a,"ES",destination_segment},{0x139f,"DX",destination_segment},
        {0x13a1,"BX",destination_offset},{0x13a4,"ES",child_code_segment_},
        {0x13a5,"DI",0x138c}});
    far_read_boundary_={0x13aa,source_segment,source_offset,2,
        child_code_segment_,0x138c};
    last_sequence_=sequence;
    continuation_address_=0x13aa;
    state_=MillenniumDosTitleInitializationState::graphics_descriptor_far_read_boundary;
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
    const auto result_state=state_;
    if ((result_state
            != MillenniumDosTitleInitializationState::private_interrupt_result_boundary
            &&result_state!=MillenniumDosTitleInitializationState::post_video_private_interrupt_result_boundary)
        || observation.sequence != last_sequence_ + 1
        || observation.interrupt_address != 0x0127
        || observation.return_address != 0x0129) {
        throw std::runtime_error("Detached Millennium DOS title private-interrupt result");
    }
    if(result_state==MillenniumDosTitleInitializationState::post_video_private_interrupt_result_boundary){
        post_video_observed_ax_=observation.ax;
        post_video_observed_flags_=observation.flags;
        boundary_.result_observed=true;
        effects_.push_back({0x1c14,"AX",0});
        last_sequence_=observation.sequence;
        continuation_address_=0x1c17;
        state_=MillenniumDosTitleInitializationState::post_video_followup_call_boundary;
        return;
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
        dos_vector_results_,setup_bios_boundary_,setup_bios_results_,
        far_read_boundary_,far_word_observations_,far_single_word_observations_,far_byte_boundary_,failure_address_,
        continuation_address_,post_video_observed_ax_,post_video_observed_flags_};
}

} // namespace eon
