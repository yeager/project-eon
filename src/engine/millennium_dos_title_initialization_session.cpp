#include "engine/millennium_dos_title_initialization_session.hpp"

#include "data/sha256.hpp"
#include "engine/native_runtime_memory.hpp"

#include <array>
#include <limits>
#include <map>
#include <stdexcept>

namespace eon {
namespace {
std::optional<std::uint16_t> latest_local_word(
    const std::vector<MillenniumDosTitleInitializationMemoryEffect>& effects,
    const std::uint16_t offset) {
    for (auto it=effects.rbegin();it!=effects.rend();++it)
        if (!it->explicit_segment
            && it->width==MillenniumDosTitleInitializationEffectWidth::word
            && it->offset==offset) return it->value;
    return std::nullopt;
}
}

void MillenniumDosTitleInitializationSession::advance_first_descriptor_mode_two_return() {
    if (state_ != MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_returned
        || continuation_address_ != 0x16e8)
        throw std::runtime_error("Detached Millennium DOS first-descriptor mode-two return");

    // The caller at $173d saved the instruction-derived descriptor-table
    // displacement (zero for this first invocation). Recover the two source
    // words and the callee-produced pair before making any mutation so a
    // missing native input cannot partially commit the caller suffix.
    const auto first_record_word = latest_local_word(memory_effects_, 0x1357);
    const auto second_record_word = latest_local_word(memory_effects_, 0x1359);
    if (!first_record_word || !second_record_word)
        throw std::runtime_error("Missing Millennium DOS first-descriptor caller words");

    memory_effects_.insert(memory_effects_.end(), {
        {0x174b, 0x1351, MillenniumDosTitleInitializationEffectWidth::word,
            first_descriptor_caller_word_},
        {0x1750, 0x134f, MillenniumDosTitleInitializationEffectWidth::word,
            first_descriptor_caller_second_word_},
        {0x175a, 0x133d, MillenniumDosTitleInitializationEffectWidth::word,
            *first_record_word},
        {0x175b, 0x133f, MillenniumDosTitleInitializationEffectWidth::word,
            *second_record_word}});
    effects_.insert(effects_.end(), {{0x1740,"AX",0},
        {0x1742,"DS",child_code_segment_},{0x1744,"ES",child_code_segment_},
        {0x1745,"SI",0x170c},{0x174a,"AX",first_descriptor_caller_word_},
        {0x174f,"AX",first_descriptor_caller_second_word_},
        {0x1754,"DI",0x133d},{0x1757,"SI",0x1357},
        {0x175d,"ES",child_code_segment_},{0x175e,"BX",0x1349},
        {0x1761,"AX",0x0006}});
    boundary_={0x1764,0x0122,0x0127,0x91,0x0006,
        child_code_segment_,0x1349,false,false};
    continuation_address_=0x0127;
    state_=MillenniumDosTitleInitializationState::graphics_record_private_interrupt_result_boundary;
}

MillenniumDosTitleModeTwoDriveResult
MillenniumDosTitleInitializationSession::drive_mode_two_from_owned_memory(
    NativeRuntimeMemory& runtime_memory,
    const MillenniumDosTitleModeTwoDriveRequest request) {
    MillenniumDosTitleModeTwoDriveResult result;
    if (state_ != MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_source_byte_boundary
        || continuation_address_ != 0x16b3 || last_sequence_ == std::numeric_limits<std::uint64_t>::max()
        || request.first_sequence != last_sequence_ + 1
        || request.maximum_observations == 0 || request.maximum_observations > 262144
        || request.first_sequence > std::numeric_limits<std::uint64_t>::max()
            - request.maximum_observations) {
        result.error = "Mode-two owned-memory drive requires its exact boundary, next sequence, and finite observation cap";
        return result;
    }

    auto next = *this;
    auto memory = runtime_memory;
    std::map<std::uint32_t,std::uint8_t> physical_bytes;
    for (const auto& cell : memory.checkpoint().initialized_bytes) {
        if (cell.location.address_space != NativeRuntimeAddressSpace::dos_segmented
            || !cell.location.segment) continue;
        const auto physical=static_cast<std::uint32_t>(*cell.location.segment)*16U
            + static_cast<std::uint32_t>(cell.location.offset);
        const auto [found,inserted]=physical_bytes.emplace(physical,cell.value);
        if (!inserted && found->second!=cell.value) {
            result.error = "Mode-two owned-memory drive found contradictory DOS segment aliases";
            return result;
        }
    }
    for (std::size_t count = 0; count < request.maximum_observations; ++count) {
        if (next.state_ == MillenniumDosTitleInitializationState::graphics_record_private_interrupt_result_boundary
            && next.boundary_.call_address == 0x1764) {
            result.accepted = true;
            result.returned = true;
            result.observation_count = count;
            *this = std::move(next);
            runtime_memory = std::move(memory);
            return result;
        }
        const auto mode_two_boundary =
            next.state_ == MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_source_byte_boundary
            || next.state_ == MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_first_lookup_byte_boundary
            || next.state_ == MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_second_source_byte_boundary
            || next.state_ == MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_second_lookup_byte_boundary;
        if (!mode_two_boundary) {
            result.error = "Mode-two owned-memory drive reached an unadmitted boundary";
            return result;
        }
        const auto boundary = next.far_byte_boundary_;
        const auto physical=static_cast<std::uint32_t>(boundary.source_segment)*16U
            + boundary.source_offset;
        const auto value=physical_bytes.find(physical);
        if (value==physical_bytes.end()) {
            result.error = "Mode-two owned-memory drive requires initialized source byte at segment "
                + std::to_string(boundary.source_segment) + " offset "
                + std::to_string(boundary.source_offset);
            return result;
        }
        const auto prior_effect_count = next.memory_effects_.size();
        try {
            next.observe_far_byte({request.first_sequence + count,boundary.instruction_address,
                boundary.source_segment,boundary.source_offset,value->second});
        } catch (const std::exception& error) {
            result.error = error.what();
            return result;
        }
        if (next.memory_effects_.size() > prior_effect_count) {
            NativeRuntimeEffectBatch batch{"millennium-dos-title-mode-two-owned-"
                + std::to_string(request.first_sequence + count),true,{}};
            for (std::size_t index=prior_effect_count;index<next.memory_effects_.size();++index) {
                const auto& effect=next.memory_effects_[index];
                batch.effects.push_back({batch.effects.size()+1,
                    {NativeRuntimeAddressSpace::dos_segmented,
                        effect.explicit_segment?effect.segment:next.child_code_segment_,effect.offset},
                    effect.width==MillenniumDosTitleInitializationEffectWidth::byte
                        ?MemoryTransferElementWidth::byte:MemoryTransferElementWidth::word,
                    NativeRuntimeByteOrder::little_endian,effect.value});
            }
            const auto applied=memory.apply(batch);
            if(!applied.accepted){result.error=applied.error;return result;}
            for(const auto& effect:batch.effects) {
                const auto effect_physical=static_cast<std::uint32_t>(*effect.location.segment)*16U
                    + static_cast<std::uint32_t>(effect.location.offset);
                physical_bytes[effect_physical]=static_cast<std::uint8_t>(effect.value);
            }
        }
        if (next.state_ == MillenniumDosTitleInitializationState::graphics_record_private_interrupt_result_boundary
            && next.boundary_.call_address == 0x1764) {
            result.accepted = true;
            result.returned = true;
            result.observation_count = count + 1;
            *this = std::move(next);
            runtime_memory = std::move(memory);
            return result;
        }
    }
    result.error = "Mode-two owned-memory drive exhausted its observation cap before return";
    return result;
}

void MillenniumDosTitleInitializationSession::advance_encoded_record_complete(){
    if(state_!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_record_complete
        ||continuation_address_!=0x1488)
        throw std::runtime_error("Detached Millennium DOS post-record dispatch");
    std::uint16_t source_offset=0,source_segment=0; bool found_offset=false,found_segment=false;
    for(auto it=memory_effects_.rbegin();it!=memory_effects_.rend();++it){
        if(!it->explicit_segment&&it->width==MillenniumDosTitleInitializationEffectWidth::word){
            if(!found_offset&&it->offset==0x138c){source_offset=it->value;found_offset=true;}
            if(!found_segment&&it->offset==0x138e){source_segment=it->value;found_segment=true;}
        }
        if(found_offset&&found_segment)break;
    }
    if(!found_offset||!found_segment)
        throw std::runtime_error("Missing Millennium DOS post-record source pointer");
    effects_.insert(effects_.end(),{{0x1489,"DS",child_code_segment_},
        {0x148c,"ES",child_code_segment_},{0x148d,"AL",selected_mode_}});
    if(selected_mode_==1){
        effects_.insert(effects_.end(),{{0x149f,"SI",source_offset},{0x149f,"DS",source_segment},
            {0x14a4,"FLAGS.DF",0},{0x14a5,"CH",0},{0x14a7,"DX",0}});
        far_byte_boundary_={0x14a9,source_segment,source_offset,0};
        continuation_address_=0x14a9;
        state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_one_header_byte_boundary;
    }else if(selected_mode_==2){
        effects_.insert(effects_.end(),{{0x163c,"ES",child_code_segment_},
            {0x163d,"SI",source_offset},{0x163d,"DS",source_segment},
            {0x1642,"FLAGS.DF",0},{0x1643,"CH",0},{0x1645,"DX",0}});
        far_byte_boundary_={0x1647,source_segment,source_offset,0};
        continuation_address_=0x1647;
        state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_header_byte_boundary;
    }else{
        effects_.insert(effects_.end(),{{0x14e5,"ES",child_code_segment_},
            {0x14e6,"SI",source_offset},{0x14e6,"DS",source_segment},
            {0x14eb,"FLAGS.DF",0},{0x14ec,"CH",0},{0x14ee,"DX",0}});
        far_byte_boundary_={0x14f0,source_segment,source_offset,0};
        continuation_address_=0x14f0;
        state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_other_header_byte_boundary;
    }
}
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
constexpr auto descriptor_first_byte_sha =
    "ed46676eb54a03e725cbb96371e4fd13852a350ba5b027e5c59dda07c78b8ecf";
constexpr auto descriptor_second_byte_sha =
    "172d30853354efec879699618dd36f3fbda28ddd07d8ea66bc2a23ace6ee6753";
constexpr auto descriptor_request_caller_sha =
    "d095399b2a968131f10112f1895b1449f6d1572052c032e48289218e5d07355b";
constexpr auto private_wrapper_epilogue_sha =
    "a6e3a351304f487a18bc22e460403bfcdb5e702831b037aa0a90a56bf3cf7baf";
constexpr auto post_descriptor_return_sha =
    "ae3f4619b0413d70d3004b9131c3752153074e45725be13b9a148978895e359e";
constexpr auto post_descriptor_caller_sha =
    "dcf069067320f72293bfab75c541e1685ad422f1f1270e5ef5f49543b0ca4a10";
constexpr auto first_title_loop_prefix_sha =
    "8ae5339224f631de9dbf852ab43c5553849b37ef00289e0a34055e73a760357a";
constexpr auto title_output_pointer_seed_sha =
    "b0f66adc83641586656866813fd9dd0b8ebb63796075661ba45d1aa8089e1d44";
constexpr auto function_001a_record_region_sha =
    "87bcb84e04957f217a47bf3c3b5fa19228ff60585fd60721daf861325a60227a";
constexpr auto encoded_record_prefix_sha =
    "a38148b66817871d8731829b2a0703e48b2e7fecb0fee51112be1e8e3b0332d0";
constexpr auto encoded_payload_prefix_sha =
    "912d067ef688829815594e9fdf4e2ae8f03051cd3be882dc482a02dae032d39b";
constexpr auto encoded_nibble_dispatch_sha =
    "dd7abdeaa64d537ee31fb6c4dffe319a7f824226ca44bb33e0f4cb3986560be7";
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
            != descriptor_product_adjust_sha
        || to_hex(sha256(titles_executable.subspan(0x12e9,9)))
            != descriptor_first_byte_sha
        || to_hex(sha256(titles_executable.subspan(0x12f2,20)))
            != descriptor_second_byte_sha
        || to_hex(sha256(titles_executable.subspan(0x1640,39)))
            != descriptor_request_caller_sha
        || to_hex(sha256(titles_executable.subspan(0x0029,6)))
            != private_wrapper_epilogue_sha
        || to_hex(sha256(titles_executable.subspan(0x0f14,1)))
            != post_descriptor_return_sha
        || to_hex(sha256(titles_executable.subspan(0x1b1d,3)))
            != post_descriptor_caller_sha
        || to_hex(sha256(titles_executable.subspan(0x1841,34)))
            != first_title_loop_prefix_sha
        || to_hex(sha256(titles_executable.subspan(0x000c,6)))
            != title_output_pointer_seed_sha
        || to_hex(sha256(titles_executable.subspan(0x0edf,20)))
            != function_001a_record_region_sha
        || to_hex(sha256(titles_executable.subspan(0x1306,19)))
            != encoded_record_prefix_sha
        || to_hex(sha256(titles_executable.subspan(0x1319,15)))
            != encoded_payload_prefix_sha
        || to_hex(sha256(titles_executable.subspan(0x1328,31)))
            != encoded_nibble_dispatch_sha) {
        throw std::runtime_error("Unsupported Millennium DOS title initialization media");
    }
    first_descriptor_caller_word_=static_cast<std::uint16_t>(titles_executable[0x160c])
        | static_cast<std::uint16_t>(static_cast<std::uint16_t>(titles_executable[0x160d]) << 8U);
    first_descriptor_caller_second_word_=static_cast<std::uint16_t>(titles_executable[0x160e])
        | static_cast<std::uint16_t>(static_cast<std::uint16_t>(titles_executable[0x160f]) << 8U);
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
                0x1000,static_cast<std::uint16_t>((static_cast<std::uint16_t>(value)<<8U)
                    |static_cast<std::uint16_t>(next_index)),
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
            &&boundary_state!=MillenniumDosTitleInitializationState::graphics_descriptor_far_read_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_far_read_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_second_loop_far_read_boundary)
        ||observation.sequence!=last_sequence_+1
        ||observation.instruction_address!=far_read_boundary_.instruction_address
        ||observation.source_segment!=far_read_boundary_.source_segment
        ||observation.source_offset!=far_read_boundary_.source_offset)
        throw std::runtime_error("Detached Millennium DOS vector far words");
    std::uint16_t descriptor_destination_segment=0;
    const bool descriptor_read=
        boundary_state==MillenniumDosTitleInitializationState::graphics_descriptor_far_read_boundary
        ||boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_far_read_boundary
        ||boundary_state==MillenniumDosTitleInitializationState::post_descriptor_second_loop_far_read_boundary;
    if(descriptor_read){
        const auto expected_first=static_cast<std::uint16_t>(
            boundary_state==MillenniumDosTitleInitializationState::graphics_descriptor_far_read_boundary
                ?0x0006:boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_far_read_boundary
                    ?0x0503:0xc800);
        const auto expected_second=static_cast<std::uint16_t>(
            boundary_state==MillenniumDosTitleInitializationState::graphics_descriptor_far_read_boundary
                ?0x0000:boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_far_read_boundary
                    ?0x1f02:0x4000);
        if(observation.first_word!=expected_first
            ||observation.second_word!=expected_second)
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
    if(descriptor_read){
        const auto destination_segment=descriptor_destination_segment;
        const auto normalized_offset=static_cast<std::uint16_t>(observation.first_word&0x000fU);
        const auto shifted_segment=static_cast<std::uint16_t>(
            (observation.second_word&0x00ffU)<<12U);
        const auto normalized_segment=static_cast<std::uint16_t>(destination_segment
            +shifted_segment+(observation.first_word>>4U));
        memory_effects_.push_back({0x13c0,0x138c,
            MillenniumDosTitleInitializationEffectWidth::word,normalized_offset});
        memory_effects_.push_back({0x13c3,0x138e,
            MillenniumDosTitleInitializationEffectWidth::word,normalized_segment});
        effects_.insert(effects_.end(),{{0x13ab,"BX",observation.first_word},
            {0x13ad,"AX",observation.second_word},
            {0x13ae,"AH",static_cast<std::uint16_t>(observation.second_word&0x00ffU)},
            {0x13b0,"AL",0},{0x13b2,"CX",4},{0x13b5,"AX",shifted_segment},
            {0x13b7,"DX",static_cast<std::uint16_t>(destination_segment+shifted_segment)},
            {0x13b9,"CX",observation.first_word},{0x013c,"AX",observation.first_word},
            {0x0141,"AX",static_cast<std::uint16_t>(observation.first_word>>4U)},
            {0x0149,"DX",normalized_segment},{0x13c4,"DS",child_code_segment_},
            {0x13c6,"SI",normalized_offset},{0x13c6,"DS",normalized_segment},
            {0x13cb,"BX",normalized_offset}});
        far_read_boundary_={0x13cd,normalized_segment,
            static_cast<std::uint16_t>(normalized_offset+0x0018),1,
            child_code_segment_,0x1359};
        last_sequence_=observation.sequence;
        continuation_address_=0x13cd;
        state_=boundary_state==MillenniumDosTitleInitializationState::graphics_descriptor_far_read_boundary
            ?MillenniumDosTitleInitializationState::graphics_record_word_read_boundary
            :boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_far_read_boundary
                ?MillenniumDosTitleInitializationState::post_descriptor_first_loop_record_word_read_boundary
                :MillenniumDosTitleInitializationState::post_descriptor_second_loop_record_word_read_boundary;
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
            &&boundary_state!=MillenniumDosTitleInitializationState::graphics_record_third_word_read_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_record_word_read_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_second_word_read_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_third_word_read_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_second_loop_record_word_read_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_second_loop_second_word_read_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_escape_word_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_escape_word_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_mode_two_word_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_mode_two_word_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_mode_two_escape_word_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_mode_two_escape_word_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_mode_two_second_escape_word_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_mode_two_second_escape_word_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_header_word_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_second_loop_third_word_read_boundary)
        ||observation.sequence!=last_sequence_+1
        ||observation.instruction_address!=far_read_boundary_.instruction_address
        ||observation.source_segment!=far_read_boundary_.source_segment
        ||observation.source_offset!=far_read_boundary_.source_offset
        ||(boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_record_word_read_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_second_word_read_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_third_word_read_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_second_loop_record_word_read_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_second_loop_second_word_read_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_escape_word_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_escape_word_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_mode_two_word_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_mode_two_word_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_mode_two_escape_word_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_mode_two_escape_word_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_mode_two_second_escape_word_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_mode_two_second_escape_word_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_header_word_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_second_loop_third_word_read_boundary
            &&observation.word!=(boundary_state==MillenniumDosTitleInitializationState::graphics_record_word_read_boundary?0x0140
                :boundary_state==MillenniumDosTitleInitializationState::graphics_record_second_word_read_boundary?0x00c8:0x0000)))
        throw std::runtime_error("Detached Millennium DOS record word");
    far_single_word_observations_.push_back(observation);
    if(boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_header_word_boundary){
        std::uint16_t header_count=0,header_dx=0; bool found_count=false,found_dx=false;
        for(auto it=effects_.rbegin();it!=effects_.rend();++it){
            if(!found_count&&it->register_name=="CX"){header_count=it->value;found_count=true;}
            if(!found_dx&&it->register_name=="DX"){header_dx=it->value;found_dx=true;}
            if(found_count&&found_dx)break;
        }
        if(!found_count||!found_dx){far_single_word_observations_.pop_back();throw std::runtime_error("Missing Millennium DOS mode-two header context");}
        const auto source=static_cast<std::uint16_t>(observation.source_offset-0x001aU+observation.word+0x001cU+header_dx+header_count);
        const auto owned_word=[this](const std::uint16_t offset,std::uint16_t& value){
            for(auto it=memory_effects_.rbegin();it!=memory_effects_.rend();++it)
                if(!it->explicit_segment&&it->width==MillenniumDosTitleInitializationEffectWidth::word&&it->offset==offset){value=it->value;return true;}
            return false;
        };
        std::uint16_t source_pointer=0,source_pointer_segment=0,destination=0,destination_segment=0,width=0,height=0;
        if(!owned_word(0x010c,source_pointer)||!owned_word(0x010e,source_pointer_segment)
            ||!owned_word(0x0110,destination)||!owned_word(0x0112,destination_segment)
            ||!owned_word(0x1357,width)||!owned_word(0x1359,height)){
            far_single_word_observations_.pop_back();throw std::runtime_error("Missing Millennium DOS mode-two pointer context");
        }
        const auto rows=static_cast<std::uint16_t>((height+7U)>>3U);
        const auto clear_words=static_cast<std::uint16_t>(static_cast<std::uint32_t>(rows)*width);
        memory_effects_.push_back({0x166b,0x14df,MillenniumDosTitleInitializationEffectWidth::word,source});
        memory_effects_.push_back({0x166e,0x14e1,MillenniumDosTitleInitializationEffectWidth::word,observation.source_segment});
        memory_effects_.push_back({0x169a,0x14dd,MillenniumDosTitleInitializationEffectWidth::word,clear_words});
        for(std::uint32_t i=0;i<clear_words;++i)
            memory_effects_.push_back({0x16a6,static_cast<std::uint16_t>(destination+i*2U),MillenniumDosTitleInitializationEffectWidth::word,0,destination_segment,true});
        effects_.insert(effects_.end(),{{0x1657,"AX",observation.word},{0x165a,"SI",source},
            {0x1664,"DX",observation.source_segment},{0x1666,"CX",source},{0x1668,"BX",0x14df},
            {0x1672,"SI",source_pointer},{0x1672,"DS",source_pointer_segment},
            {0x1677,"DI",destination},{0x1677,"ES",destination_segment},{0x167c,"BX",width},
            {0x1681,"DX",height},{0x1693,"AX",clear_words},{0x169e,"CX",clear_words},
            {0x16a0,"AX",0},{0x16aa,"AH",0x80},{0x16ac,"CL",0},{0x16ae,"DX",height}});
        far_byte_boundary_={0x16b3,source_pointer_segment,source_pointer,0};
        last_sequence_=observation.sequence;continuation_address_=0x16b3;
        state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_source_byte_boundary;
        return;
    }
    if(boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_mode_two_second_escape_word_boundary
        ||boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_mode_two_second_escape_word_boundary){
        const auto high_path=boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_mode_two_second_escape_word_boundary;
        std::uint16_t high=0,current_di=0,current_dx=0; std::uint8_t repeated=0;
        const auto output_segment=latest_local_word(memory_effects_,0x0112);
        bool found_high=false,found_di=false,found_dx=false,found_repeated=false;
        for(auto it=effects_.rbegin();it!=effects_.rend();++it){
            if(!found_high&&it->register_name=="CH"){high=it->value;found_high=true;}
            if(!found_di&&it->register_name=="DI"){current_di=it->value;found_di=true;}
            if(!found_dx&&it->register_name=="DX"){current_dx=it->value;found_dx=true;}
            if(found_high&&found_di&&found_dx)break;
        }
        for(auto it=memory_effects_.rbegin();it!=memory_effects_.rend();++it)
            if(output_segment&&it->explicit_segment&&it->segment==*output_segment
                &&it->width==MillenniumDosTitleInitializationEffectWidth::byte
                &&it->offset==static_cast<std::uint16_t>(current_di-1U)){
                repeated=static_cast<std::uint8_t>(it->value);found_repeated=true;break;
            }
        if(!found_high||!found_di||!found_dx||!found_repeated||!output_segment){
            far_single_word_observations_.pop_back();
            throw std::runtime_error("Missing Millennium DOS extended run context");
        }
        const auto shifted=boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_mode_two_second_escape_word_boundary
            ?observation.word:static_cast<std::uint16_t>(observation.word>>4U);
        const auto encoded_count=static_cast<std::uint16_t>((high<<8U)|static_cast<std::uint8_t>(shifted));
        const auto repeat_count=static_cast<std::uint16_t>(encoded_count+2U);
        const auto remaining=static_cast<std::uint16_t>(current_dx-repeat_count);
        for(std::uint32_t i=0;i<repeat_count;++i)
            memory_effects_.push_back({0x146b,static_cast<std::uint16_t>(current_di+i),
                MillenniumDosTitleInitializationEffectWidth::byte,repeated,*output_segment,true});
        effects_.insert(effects_.end(),{{0x1459,"SI",static_cast<std::uint16_t>(observation.source_offset+1U)},
            {0x145a,"AX",shifted},{0x1460,"CL",static_cast<std::uint8_t>(shifted)},
            {0x1466,"CX",repeat_count},{0x1469,"DX",remaining},
            {0x146b,"DI",static_cast<std::uint16_t>(current_di+repeat_count)}});
        last_sequence_=observation.sequence;
        if(remaining==0){continuation_address_=0x1488;state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_record_complete;advance_encoded_record_complete();}
        else {far_byte_boundary_={0x1428,observation.source_segment,static_cast<std::uint16_t>(observation.source_offset+1U),0};continuation_address_=0x1428;state_=high_path?MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_stream_byte_boundary:MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_nibble_byte_boundary;}
        return;
    }
    if(boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_mode_two_escape_word_boundary
        ||boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_mode_two_escape_word_boundary){
        const auto high_path=boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_mode_two_escape_word_boundary;
        const auto shifted=high_path?observation.word:static_cast<std::uint16_t>(observation.word>>4U);
        effects_.insert(effects_.end(),{{0x1453,"SI",static_cast<std::uint16_t>(observation.source_offset+1U)},
            {0x1454,"AX",shifted},{0x1456,"CH",static_cast<std::uint8_t>(shifted)}});
        far_read_boundary_={0x1458,observation.source_segment,
            static_cast<std::uint16_t>(observation.source_offset+1U),1,child_code_segment_,0};
        last_sequence_=observation.sequence;continuation_address_=0x1458;
        state_=high_path
            ?MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_mode_two_second_escape_word_boundary
            :MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_mode_two_second_escape_word_boundary;
        return;
    }
    if(boundary_state==MillenniumDosTitleInitializationState::post_descriptor_second_loop_third_word_read_boundary){
        std::uint16_t product_low=0; bool found=false;
        for(auto it=memory_effects_.rbegin();it!=memory_effects_.rend();++it)
            if(!it->explicit_segment&&it->offset==0x133b){product_low=it->value;found=true;break;}
        if(!found){far_single_word_observations_.pop_back();throw std::runtime_error("Missing Millennium DOS second-loop product");}
        const auto adjusted=static_cast<std::uint16_t>(product_low-observation.word);
        memory_effects_.push_back({0x13e5,0x138a,MillenniumDosTitleInitializationEffectWidth::word,adjusted});
        effects_.push_back({0x13e2,"AX",adjusted});
        far_byte_boundary_={0x13e9,observation.source_segment,
            static_cast<std::uint16_t>(observation.source_offset-0x0013U),0x1389};
        last_sequence_=observation.sequence;continuation_address_=0x13e9;
        state_=MillenniumDosTitleInitializationState::post_descriptor_second_loop_byte_read_boundary;
        return;
    }
    if(boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_escape_word_boundary
        ||boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_escape_word_boundary){
        const auto high_path=boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_escape_word_boundary;
        const auto shifted=high_path?observation.word:static_cast<std::uint16_t>(observation.word>>4U);
        const auto output=static_cast<std::uint8_t>(shifted);
        std::uint16_t current_di=0,current_dx=0; bool found_di=false,found_dx=false;
        const auto output_segment=latest_local_word(memory_effects_,0x0112);
        for(auto it=effects_.rbegin();it!=effects_.rend();++it){
            if(!found_di&&it->register_name=="DI"){current_di=it->value;found_di=true;}
            if(!found_dx&&it->register_name=="DX"){current_dx=it->value;found_dx=true;}
            if(found_di&&found_dx)break;
        }
        if(!found_di||!found_dx||!output_segment){far_single_word_observations_.pop_back();throw std::runtime_error("Missing Millennium DOS escape context");}
        const auto remaining=static_cast<std::uint16_t>(current_dx-1U);
        memory_effects_.push_back({0x1484,current_di,
            MillenniumDosTitleInitializationEffectWidth::byte,output,*output_segment,true});
        effects_.insert(effects_.end(),{{0x1438,"SI",static_cast<std::uint16_t>(observation.source_offset+1U)},
            {0x1439,"AX",shifted},{0x1482,"CH",output},{0x1485,"DI",static_cast<std::uint16_t>(current_di+1U)},
            {0x1485,"DX",remaining}});
        last_sequence_=observation.sequence;
        if(remaining==0){continuation_address_=0x1488;state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_record_complete;advance_encoded_record_complete();}
        else {far_byte_boundary_={0x1428,observation.source_segment,static_cast<std::uint16_t>(observation.source_offset+1U),0};continuation_address_=0x1428;state_=high_path?MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_stream_byte_boundary:MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_nibble_byte_boundary;}
        return;
    }
    if(boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_mode_two_word_boundary
        ||boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_mode_two_word_boundary){
        const auto high_path=boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_mode_two_word_boundary;
        const auto shifted=high_path?observation.word:static_cast<std::uint16_t>(observation.word>>4U);
        const auto run=static_cast<std::uint8_t>(shifted);
        effects_.insert(effects_.end(),{{0x144b,"SI",static_cast<std::uint16_t>(observation.source_offset+1U)},
            {0x144c,"AX",shifted}});
        last_sequence_=observation.sequence;
        if(run==0xff){
            far_read_boundary_={0x1452,observation.source_segment,
                static_cast<std::uint16_t>(observation.source_offset+1U),1,child_code_segment_,0};
            continuation_address_=0x1452;
            state_=high_path
                ?MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_mode_two_escape_word_boundary
                :MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_mode_two_escape_word_boundary;
            return;
        }
        std::uint16_t current_di=0,current_dx=0; std::uint8_t repeated=0;
        const auto output_segment=latest_local_word(memory_effects_,0x0112);
        bool found_di=false,found_dx=false,found_repeated=false;
        for(auto it=effects_.rbegin();it!=effects_.rend();++it){
            if(!found_di&&it->register_name=="DI"){current_di=it->value;found_di=true;}
            if(!found_dx&&it->register_name=="DX"){current_dx=it->value;found_dx=true;}
            if(found_di&&found_dx)break;
        }
        for(auto it=memory_effects_.rbegin();it!=memory_effects_.rend();++it){
            if(output_segment&&it->explicit_segment&&it->segment==*output_segment
                &&it->width==MillenniumDosTitleInitializationEffectWidth::byte
                &&it->offset==static_cast<std::uint16_t>(current_di-1U)){
                repeated=static_cast<std::uint8_t>(it->value);found_repeated=true;break;
            }
        }
        if(!found_di||!found_dx||!found_repeated||!output_segment){far_single_word_observations_.pop_back();throw std::runtime_error("Missing Millennium DOS run context");}
        const auto repeat_count=static_cast<std::uint16_t>(run+2U);
        const auto remaining=static_cast<std::uint16_t>(current_dx-repeat_count);
        for(std::uint16_t i=0;i<repeat_count;++i)
            memory_effects_.push_back({0x146b,static_cast<std::uint16_t>(current_di+i),
                MillenniumDosTitleInitializationEffectWidth::byte,repeated,*output_segment,true});
        effects_.insert(effects_.end(),{{0x145e,"CH",0},{0x1460,"CL",run},
            {0x1466,"CX",repeat_count},{0x1469,"DX",remaining},
            {0x146b,"DI",static_cast<std::uint16_t>(current_di+repeat_count)}});
        if(remaining==0){continuation_address_=0x1488;state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_record_complete;advance_encoded_record_complete();}
        else {far_byte_boundary_={0x1428,observation.source_segment,static_cast<std::uint16_t>(observation.source_offset+1U),0};continuation_address_=0x1428;state_=high_path?MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_stream_byte_boundary:MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_nibble_byte_boundary;}
        return;
    }
    if(boundary_state==MillenniumDosTitleInitializationState::post_descriptor_second_loop_second_word_read_boundary){
        if(far_single_word_observations_.size()<2)
            throw std::runtime_error("Missing Millennium DOS second-loop first record word");
        const auto first=far_single_word_observations_[far_single_word_observations_.size()-2].word;
        const auto product=static_cast<std::uint32_t>(first)*observation.word;
        memory_effects_.push_back({0x13d3,0x1357,MillenniumDosTitleInitializationEffectWidth::word,observation.word});
        memory_effects_.push_back({0x13d8,0x1359,MillenniumDosTitleInitializationEffectWidth::word,first});
        memory_effects_.push_back({0x13de,0x133b,MillenniumDosTitleInitializationEffectWidth::word,static_cast<std::uint16_t>(product)});
        effects_.insert(effects_.end(),{{0x13d0,"CX",observation.word},
            {0x13dc,"AX",static_cast<std::uint16_t>(product)},
            {0x13dc,"DX",static_cast<std::uint16_t>(product>>16U)}});
        far_read_boundary_={0x13e2,observation.source_segment,
            static_cast<std::uint16_t>(observation.source_offset-2U),1,child_code_segment_,0x138a};
        last_sequence_=observation.sequence;continuation_address_=0x13e2;
        state_=MillenniumDosTitleInitializationState::post_descriptor_second_loop_third_word_read_boundary;
        return;
    }
    if(boundary_state==MillenniumDosTitleInitializationState::post_descriptor_second_loop_record_word_read_boundary){
        effects_.push_back({0x13cd,"AX",observation.word});
        far_read_boundary_={0x13d0,observation.source_segment,
            static_cast<std::uint16_t>(observation.source_offset-2U),1,
            child_code_segment_,0x1357};
        last_sequence_=observation.sequence;
        continuation_address_=0x13d0;
        state_=MillenniumDosTitleInitializationState::post_descriptor_second_loop_second_word_read_boundary;
        return;
    }
    if(boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_third_word_read_boundary){
        std::uint16_t product_low=0;
        bool found_product=false;
        for(auto it=memory_effects_.rbegin();it!=memory_effects_.rend();++it)
            if(!it->explicit_segment&&it->offset==0x133b
                &&it->width==MillenniumDosTitleInitializationEffectWidth::word){
                product_low=it->value;found_product=true;break;
            }
        if(!found_product)
            throw std::runtime_error("Missing Millennium DOS first title-loop product");
        const auto adjusted=static_cast<std::uint16_t>(product_low-observation.word);
        memory_effects_.push_back({0x13e5,0x138a,
            MillenniumDosTitleInitializationEffectWidth::word,adjusted});
        effects_.push_back({0x13e2,"AX",adjusted});
        far_byte_boundary_={0x13e9,observation.source_segment,
            static_cast<std::uint16_t>(observation.source_offset-0x0013),0x1389};
        last_sequence_=observation.sequence;
        continuation_address_=0x13e9;
        state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_byte_read_boundary;
        return;
    }
    if(boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_second_word_read_boundary){
        if(far_single_word_observations_.size()<2)
            throw std::runtime_error("Missing Millennium DOS first title-loop record word");
        const auto first_word=far_single_word_observations_[
            far_single_word_observations_.size()-2].word;
        const auto product=static_cast<std::uint32_t>(first_word)*observation.word;
        const auto product_low=static_cast<std::uint16_t>(product);
        const auto product_high=static_cast<std::uint16_t>(product>>16U);
        memory_effects_.push_back({0x13d3,0x1357,
            MillenniumDosTitleInitializationEffectWidth::word,observation.word});
        memory_effects_.push_back({0x13d8,0x1359,
            MillenniumDosTitleInitializationEffectWidth::word,first_word});
        memory_effects_.push_back({0x13de,0x133b,
            MillenniumDosTitleInitializationEffectWidth::word,product_low});
        effects_.insert(effects_.end(),{{0x13d0,"CX",observation.word},
            {0x13dc,"AX",product_low},{0x13dc,"DX",product_high}});
        far_read_boundary_={0x13e2,observation.source_segment,
            static_cast<std::uint16_t>(observation.source_offset-2),1,
            child_code_segment_,0x138a};
        last_sequence_=observation.sequence;
        continuation_address_=0x13e2;
        state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_third_word_read_boundary;
        return;
    }
    if(boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_record_word_read_boundary){
        effects_.push_back({0x13cd,"AX",observation.word});
        far_read_boundary_={0x13d0,observation.source_segment,
            static_cast<std::uint16_t>(observation.source_offset-2),1,
            child_code_segment_,0x1357};
        last_sequence_=observation.sequence;
        continuation_address_=0x13d0;
        state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_second_word_read_boundary;
        return;
    }
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

void MillenniumDosTitleInitializationSession::observe_far_byte(
    const MillenniumDosTitleFarByteObservation& observation){
    const auto boundary_state=state_;
    if((boundary_state!=MillenniumDosTitleInitializationState::graphics_record_byte_read_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::graphics_record_second_byte_read_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_byte_read_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_second_byte_read_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_payload_byte_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_stream_byte_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_nibble_byte_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_xlat_byte_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_xlat_byte_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_header_byte_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_header_second_byte_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_source_byte_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_first_lookup_byte_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_second_source_byte_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_second_lookup_byte_boundary)
        ||observation.sequence!=last_sequence_+1
        ||observation.instruction_address!=far_byte_boundary_.instruction_address
        ||observation.source_segment!=far_byte_boundary_.source_segment
        ||observation.source_offset!=far_byte_boundary_.source_offset
        ||(boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_byte_read_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_second_byte_read_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_payload_byte_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_stream_byte_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_nibble_byte_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_xlat_byte_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_xlat_byte_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_header_byte_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_header_second_byte_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_source_byte_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_first_lookup_byte_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_second_source_byte_boundary
            &&boundary_state!=MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_second_lookup_byte_boundary
            &&observation.byte!=(boundary_state==MillenniumDosTitleInitializationState::graphics_record_byte_read_boundary?0x23:0x00)))
        throw std::runtime_error("Detached Millennium DOS record byte");
    far_byte_observations_.push_back(observation);
    if(boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_source_byte_boundary
        ||boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_second_source_byte_boundary){
        std::uint16_t table_offset=0,table_segment=0; bool found_offset=false,found_segment=false;
        for(auto it=memory_effects_.rbegin();it!=memory_effects_.rend();++it){
            if(!it->explicit_segment&&it->width==MillenniumDosTitleInitializationEffectWidth::word){
                if(!found_offset&&it->offset==0x14df){table_offset=it->value;found_offset=true;}
                if(!found_segment&&it->offset==0x14e1){table_segment=it->value;found_segment=true;}
            }
            if(found_offset&&found_segment)break;
        }
        if(!found_offset||!found_segment){far_byte_observations_.pop_back();throw std::runtime_error("Missing Millennium DOS mode-two lookup pointer");}
        const auto second=boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_second_source_byte_boundary;
        const auto source_instruction=static_cast<std::uint16_t>(second?0x16c8:0x16b3);
        effects_.insert(effects_.end(),{{source_instruction,"AL",observation.byte},
            {source_instruction,"SI",static_cast<std::uint16_t>(observation.source_offset+1U)}});
        far_byte_boundary_={static_cast<std::uint16_t>(second?0x16d0:0x16bb),table_segment,
            static_cast<std::uint16_t>(table_offset+observation.byte),0};
        last_sequence_=observation.sequence;continuation_address_=far_byte_boundary_.instruction_address;
        state_=second?MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_second_lookup_byte_boundary
            :MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_first_lookup_byte_boundary;
        return;
    }
    if(boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_first_lookup_byte_boundary
        ||boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_second_lookup_byte_boundary){
        const auto second=boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_second_lookup_byte_boundary;
        std::uint16_t cl=0,dx=0,di=0,bx=0,si=0,ch=0,height=0,destination_segment=0;
        bool fcl=false,fdx=false,fdi=false,fbx=false,fsi=false,fch=!second,fh=false,fes=false;
        for(auto it=effects_.rbegin();it!=effects_.rend();++it){
            if(!fcl&&it->register_name=="CL"){cl=it->value;fcl=true;}
            if(!fdx&&it->register_name=="DX"){dx=it->value;fdx=true;}
            if(!fdi&&it->register_name=="DI"){di=it->value;fdi=true;}
            if(!fbx&&it->register_name=="BX"){bx=it->value;fbx=true;}
            if(!fsi&&it->register_name=="SI"){si=it->value;fsi=true;}
            if(!fch&&it->register_name=="CH"){ch=it->value;fch=true;}
            if(!fes&&it->register_name=="ES"){destination_segment=it->value;fes=true;}
            if(fcl&&fdx&&fdi&&fbx&&fsi&&fch&&fes)break;
        }
        for(auto it=memory_effects_.rbegin();it!=memory_effects_.rend();++it)
            if(!it->explicit_segment&&it->offset==0x1359){height=it->value;fh=true;break;}
        if(!fcl||!fdx||!fdi||!fbx||!fsi||!fch||!fh||!fes){far_byte_observations_.pop_back();throw std::runtime_error("Missing Millennium DOS mode-two loop context");}
        const auto promoted_byte=static_cast<std::uint16_t>(observation.byte);
        auto output=second?static_cast<std::uint8_t>(((promoted_byte>>cl)&0x0fU)|ch)
            :static_cast<std::uint8_t>((static_cast<std::uint32_t>(promoted_byte)<<cl)&0xf0U);
        auto remaining=static_cast<std::uint16_t>(dx-1U);
        effects_.insert(effects_.end(),{{static_cast<std::uint16_t>(second?0x16d0:0x16bb),"AL",observation.byte},
            {static_cast<std::uint16_t>(second?0x16d4:0x16bf),"AL",output},
            {static_cast<std::uint16_t>(second?0x16da:0x16c3),"DX",remaining}});
        if(!second&&remaining!=0){
            effects_.push_back({0x16c6,"CH",output});
            far_byte_boundary_={0x16c8,far_byte_observations_[far_byte_observations_.size()-2].source_segment,si,0};
            last_sequence_=observation.sequence;continuation_address_=0x16c8;
            state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_second_source_byte_boundary;return;
        }
        memory_effects_.push_back({0x16db,di,MillenniumDosTitleInitializationEffectWidth::byte,output,destination_segment,true});
        effects_.push_back({0x16db,"DI",static_cast<std::uint16_t>(di+1U)});
        if(remaining!=0){
            far_byte_boundary_={0x16b3,far_byte_observations_[far_byte_observations_.size()-2].source_segment,si,0};
            last_sequence_=observation.sequence;continuation_address_=0x16b3;
            state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_source_byte_boundary;return;
        }
        const auto next_cl=static_cast<std::uint16_t>(cl^4U);
        const auto next_bx=static_cast<std::uint16_t>(bx-1U);
        effects_.insert(effects_.end(),{{0x16de,"CL",next_cl},{0x16e1,"BX",next_bx}});
        last_sequence_=observation.sequence;
        if(next_bx==0){effects_.insert(effects_.end(),{{0x16e5,"DS",child_code_segment_},{0x16e7,"ES",child_code_segment_}});continuation_address_=0x16e8;state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_returned;advance_first_descriptor_mode_two_return();}
        else {effects_.push_back({0x16ae,"DX",height});far_byte_boundary_={0x16b3,far_byte_observations_[far_byte_observations_.size()-2].source_segment,si,0};continuation_address_=0x16b3;state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_source_byte_boundary;}
        return;
    }
    if(boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_header_byte_boundary){
        const auto dx=static_cast<std::uint16_t>((observation.byte&1U)!=0?0x0300:0);
        effects_.insert(effects_.end(),{{0x1647,"CL",observation.byte},{0x164e,"DX",dx},{0x1651,"CH",0}});
        far_byte_boundary_={0x1653,observation.source_segment,static_cast<std::uint16_t>(observation.source_offset+1U),0};
        last_sequence_=observation.sequence;continuation_address_=0x1653;
        state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_header_second_byte_boundary;
        return;
    }
    if(boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_header_second_byte_boundary){
        const auto count=static_cast<std::uint16_t>(observation.byte+1U);
        effects_.insert(effects_.end(),{{0x1653,"CL",observation.byte},{0x1656,"CX",count}});
        far_read_boundary_={0x1657,observation.source_segment,static_cast<std::uint16_t>(observation.source_offset+0x0019U),1,child_code_segment_,0};
        last_sequence_=observation.sequence;continuation_address_=0x1657;
        state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_mode_two_header_word_boundary;
        return;
    }
    if(boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_nibble_byte_boundary){
        const auto nibble=static_cast<std::uint8_t>(observation.byte>>4U);
        effects_.insert(effects_.end(),{{0x1428,"AL",observation.byte},
            {0x1429,"AL",nibble},{0x142b,"CL",0},
            {0x1433,"SI",static_cast<std::uint16_t>(observation.source_offset+1U)}});
        last_sequence_=observation.sequence;
        if(nibble==0x0f){
            far_read_boundary_={0x1437,observation.source_segment,
                static_cast<std::uint16_t>(observation.source_offset+1U),1,child_code_segment_,0};
            continuation_address_=0x1437;
            state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_escape_word_boundary;
        }else if(selected_mode_==2&&nibble==0x0e){
            far_read_boundary_={0x144a,observation.source_segment,
                static_cast<std::uint16_t>(observation.source_offset+1U),1,child_code_segment_,0};
            continuation_address_=0x144a;
            state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_mode_two_word_boundary;
        }else{
            far_byte_boundary_={0x1470,observation.source_segment,
                static_cast<std::uint16_t>(0x0008+nibble),0};
            continuation_address_=0x1470;
            state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_xlat_byte_boundary;
        }
        return;
    }
    if(boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_xlat_byte_boundary
        ||boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_xlat_byte_boundary){
        if(far_byte_observations_.size()<3){far_byte_observations_.pop_back();throw std::runtime_error("Missing Millennium DOS encoded lookup context");}
        const auto dispatch=far_byte_observations_[far_byte_observations_.size()-2];
        std::uint16_t limit=0,current_di=0,current_dx=0,prior=0;
        const auto output_segment=latest_local_word(memory_effects_,0x0112);
        bool found_di=false,found_dx=false,found_ch=false;
        for(auto it=memory_effects_.rbegin();it!=memory_effects_.rend();++it){
            if(!it->explicit_segment&&it->offset==0x1389&&limit==0)limit=it->value;
        }
        for(auto it=effects_.rbegin();it!=effects_.rend();++it){
            if(!found_di&&it->register_name=="DI"){current_di=it->value;found_di=true;}
            if(!found_dx&&it->register_name=="DX"){current_dx=it->value;found_dx=true;}
            if(!found_ch&&it->register_name=="CH"){prior=it->value;found_ch=true;}
            if(found_di&&found_dx&&found_ch)break;
        }
        if(!output_segment){far_byte_observations_.pop_back();throw std::runtime_error("Missing Millennium DOS encoded output segment");}
        const auto sum=static_cast<std::uint16_t>(observation.byte+prior);
        auto output=static_cast<std::uint8_t>(sum);
        if(sum>0xffU||output>=static_cast<std::uint8_t>(limit))
            output=static_cast<std::uint8_t>(output-static_cast<std::uint8_t>(limit));
        const auto remaining=static_cast<std::uint16_t>(current_dx-1U);
        memory_effects_.push_back({0x1484,current_di,MillenniumDosTitleInitializationEffectWidth::byte,output,*output_segment,true});
        effects_.insert(effects_.end(),{{0x1470,"AL",observation.byte},{0x1472,"AL",static_cast<std::uint8_t>(sum)},
            {0x1482,"CH",output},{0x1485,"DI",static_cast<std::uint16_t>(current_di+1U)},{0x1485,"DX",remaining}});
        last_sequence_=observation.sequence;
        if(remaining==0){continuation_address_=0x1488;state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_record_complete;advance_encoded_record_complete();}
        else if(boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_xlat_byte_boundary){far_byte_boundary_={0x1428,dispatch.source_segment,dispatch.source_offset,0};continuation_address_=0x1428;state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_high_nibble_byte_boundary;}
        else {far_byte_boundary_={0x1428,dispatch.source_segment,static_cast<std::uint16_t>(dispatch.source_offset+1U),0};continuation_address_=0x1428;state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_stream_byte_boundary;}
        return;
    }
    if(boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_stream_byte_boundary){
        const auto nibble=static_cast<std::uint8_t>(observation.byte&0x0fU);
        effects_.insert(effects_.end(),{{0x1428,"AL",observation.byte},
            {0x142b,"CL",4},{0x1430,"AL",nibble},
            {0x1432,"SI",observation.source_offset}});
        last_sequence_=observation.sequence;
        if(nibble==0x0f){
            far_read_boundary_={0x1437,observation.source_segment,
                observation.source_offset,1,child_code_segment_,0};
            continuation_address_=0x1437;
            state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_escape_word_boundary;
        }else if(selected_mode_==2&&nibble==0x0e){
            far_read_boundary_={0x144a,observation.source_segment,
                observation.source_offset,1,child_code_segment_,0};
            continuation_address_=0x144a;
            state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_mode_two_word_boundary;
        }else{
            far_byte_boundary_={0x1470,observation.source_segment,
                static_cast<std::uint16_t>(0x0008+nibble),0};
            continuation_address_=0x1470;
            state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_xlat_byte_boundary;
        }
        return;
    }
    if(boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_payload_byte_boundary){
        std::uint16_t record_count=0;
        bool found_count=false;
        for(auto it=memory_effects_.rbegin();it!=memory_effects_.rend();++it)
            if(!it->explicit_segment&&it->offset==0x138a
                &&it->width==MillenniumDosTitleInitializationEffectWidth::word){
                record_count=it->value;found_count=true;break;
            }
        const auto output_segment=latest_local_word(memory_effects_,0x0112);
        if(!found_count||!output_segment){
            far_byte_observations_.pop_back();
            throw std::runtime_error("Missing Millennium DOS encoded record count");
        }
        const auto remaining=static_cast<std::uint16_t>(record_count-1U);
        memory_effects_.push_back({0x141c,0x0170,
            MillenniumDosTitleInitializationEffectWidth::byte,observation.byte,*output_segment,true});
        effects_.insert(effects_.end(),{{0x1419,"AL",observation.byte},
            {0x141a,"CH",observation.byte},{0x141d,"DI",0x0171},
            {0x1422,"DX",record_count},{0x1425,"BX",0x0008},
            {0x1426,"DX",remaining}});
        last_sequence_=observation.sequence;
        if(remaining==0){
            continuation_address_=0x1488;
            state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_record_complete;
            advance_encoded_record_complete();
        }else{
            far_byte_boundary_={0x1428,observation.source_segment,
                static_cast<std::uint16_t>(observation.source_offset+1U),0};
            continuation_address_=0x1428;
            state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_stream_byte_boundary;
        }
        return;
    }
    if(boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_second_byte_read_boundary){
        memory_effects_.push_back({0x13f5,0x1388,
            MillenniumDosTitleInitializationEffectWidth::byte,observation.byte});
        effects_.push_back({0x13f2,"AL",observation.byte});
        last_sequence_=observation.sequence;
        if(observation.byte==1||observation.byte==2){
            effects_.insert(effects_.end(),{{0x1401,"AH",0},
                {0x1404,"DS",child_code_segment_},{0x1407,"ES",child_code_segment_},
                {0x1407,"DI",0x0170},{0x140c,"AH",0},{0x140e,"SI",0x001f},
                {0x140e,"DS",observation.source_segment},{0x1417,"CL",0}});
            far_byte_boundary_={0x1419,observation.source_segment,0x001f,0};
            continuation_address_=0x1419;
            state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_encoded_payload_byte_boundary;
            return;
        }
        const auto owned_word=[this](const std::uint16_t offset){
            for(auto it=memory_effects_.rbegin();it!=memory_effects_.rend();++it)
                if(!it->explicit_segment&&it->offset==offset
                    &&it->width==MillenniumDosTitleInitializationEffectWidth::word)
                    return it->value;
            throw std::runtime_error("Missing Millennium DOS second title-loop pointer");
        };
        const auto source_offset=owned_word(0x0e4a);
        const auto source_segment=owned_word(0x0e4c);
        const auto destination_segment=owned_word(0x0e48);
        constexpr std::uint16_t second_output=0x02e0;
        memory_effects_.push_back({0x1950,0x010c,
            MillenniumDosTitleInitializationEffectWidth::word,second_output});
        memory_effects_.push_back({0x1959,0x0110,
            MillenniumDosTitleInitializationEffectWidth::word,second_output});
        effects_.insert(effects_.end(),{{0x1401,"AH",0},
            {0x1404,"DS",child_code_segment_},{0x1963,"DX",0x0170},
            {0x1964,"CX",0x0025},{0x1965,"CX",0x0024},
            {0x1949,"SI",0x010c},{0x194c,"AX",second_output},
            {0x1952,"SI",0x0110},{0x1955,"AX",second_output},
            {0x195b,"AX",0x0026},{0x195e,"AX",0x0002},
            {0x1390,"CX",0x000c},{0x1393,"AX",0x0018},
            {0x1395,"SI",static_cast<std::uint16_t>(source_offset+0x0018)},
            {0x1395,"DS",source_segment},{0x139a,"DI",0},
            {0x139a,"ES",destination_segment},{0x139f,"DX",destination_segment},
            {0x13a1,"BX",0},{0x13a4,"ES",child_code_segment_},
            {0x13a5,"DI",0x138c}});
        far_read_boundary_={0x13aa,source_segment,
            static_cast<std::uint16_t>(source_offset+0x0018),2,
            child_code_segment_,0x138c};
        continuation_address_=0x13aa;
        state_=MillenniumDosTitleInitializationState::post_descriptor_second_loop_far_read_boundary;
        return;
    }
    if(boundary_state==MillenniumDosTitleInitializationState::post_descriptor_first_loop_byte_read_boundary){
        const auto incremented=static_cast<std::uint8_t>(observation.byte+1U);
        effects_.insert(effects_.end(),{{0x13e9,"AL",observation.byte},
            {0x13ec,"AL",incremented}});
        memory_effects_.push_back({0x13ee,0x1389,
            MillenniumDosTitleInitializationEffectWidth::byte,incremented});
        far_byte_boundary_={0x13f2,observation.source_segment,
            static_cast<std::uint16_t>(observation.source_offset+3),0x1388};
        last_sequence_=observation.sequence;
        continuation_address_=0x13f2;
        state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_second_byte_read_boundary;
        return;
    }
    if(boundary_state==MillenniumDosTitleInitializationState::graphics_record_second_byte_read_boundary){
        memory_effects_.insert(memory_effects_.end(),{
            {0x13f5,0x1388,MillenniumDosTitleInitializationEffectWidth::byte,0},
            {0x174b,0x1351,MillenniumDosTitleInitializationEffectWidth::word,0},
            {0x1750,0x134f,MillenniumDosTitleInitializationEffectWidth::word,0},
            {0x175a,0x133d,MillenniumDosTitleInitializationEffectWidth::word,0x00c8},
            {0x175b,0x133f,MillenniumDosTitleInitializationEffectWidth::word,0x0140}});
        effects_.insert(effects_.end(),{{0x13f2,"AL",0},{0x1401,"AH",0},
            {0x1404,"DS",child_code_segment_},{0x1740,"AX",0},
            {0x1742,"DS",child_code_segment_},{0x1744,"ES",child_code_segment_},
            {0x1745,"SI",0x170c},{0x174a,"AX",0},{0x174f,"AX",0},
            {0x1754,"DI",0x133d},{0x1757,"SI",0x1357},
            {0x175d,"ES",child_code_segment_},{0x175e,"BX",0x1349},
            {0x1761,"AX",0x0006}});
        boundary_={0x1764,0x0122,0x0127,0x91,0x0006,
            child_code_segment_,0x1349,false,false};
        last_sequence_=observation.sequence;
        continuation_address_=0x0127;
        state_=MillenniumDosTitleInitializationState::graphics_record_private_interrupt_result_boundary;
        return;
    }
    effects_.insert(effects_.end(),{{0x13e9,"AL",observation.byte},{0x13ec,"AL",0x24}});
    memory_effects_.push_back({0x13ee,0x1389,
        MillenniumDosTitleInitializationEffectWidth::byte,0x24});
    far_byte_boundary_={0x13f2,observation.source_segment,0x000a,0x1388};
    last_sequence_=observation.sequence;
    continuation_address_=0x13f2;
    state_=MillenniumDosTitleInitializationState::graphics_record_second_byte_read_boundary;
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
            &&result_state!=MillenniumDosTitleInitializationState::post_video_private_interrupt_result_boundary
            &&result_state!=MillenniumDosTitleInitializationState::graphics_record_private_interrupt_result_boundary
            &&result_state!=MillenniumDosTitleInitializationState::post_descriptor_private_interrupt_result_boundary)
        || observation.sequence != last_sequence_ + 1
        || observation.interrupt_address != 0x0127
        || observation.return_address != 0x0129) {
        throw std::runtime_error("Detached Millennium DOS title private-interrupt result");
    }
    if(result_state==MillenniumDosTitleInitializationState::post_descriptor_private_interrupt_result_boundary){
        if(observation.record_segment!=child_code_segment_
            ||observation.record_offset!=0x0fdf
            ||observation.record_bytes.size()!=10)
            throw std::runtime_error("Millennium DOS function-001a result needs exact CS:0fdf record");
        const auto owned_word=[this](const std::uint16_t offset){
            for(auto it=memory_effects_.rbegin();it!=memory_effects_.rend();++it)
                if(!it->explicit_segment&&it->offset==offset
                    &&it->width==MillenniumDosTitleInitializationEffectWidth::word)
                    return it->value;
            throw std::runtime_error("Missing Millennium DOS first-loop pointer");
        };
        const auto source_offset=owned_word(0x0e4a);
        const auto source_segment=owned_word(0x0e4c);
        // Both output offsets are hash-bound zero words in TITLES.EXE. No
        // admitted native prefix writes them before this path.
        constexpr std::uint16_t destination_offset=0;
        const auto destination_segment=owned_word(0x0e48);
        constexpr std::uint16_t first_output=0x0170;
        constexpr std::uint16_t second_output=0x0170;
        post_descriptor_observed_ax_=observation.ax;
        post_descriptor_observed_flags_=observation.flags;
        post_descriptor_observed_record_=observation.record_bytes;
        boundary_.result_observed=true;
        for(std::size_t i=0;i<observation.record_bytes.size();++i)
            memory_effects_.push_back({0x0127,
                static_cast<std::uint16_t>(0x0fdf+i),
                MillenniumDosTitleInitializationEffectWidth::byte,
                observation.record_bytes[i]});
        memory_effects_.push_back({0x1950,0x010c,
            MillenniumDosTitleInitializationEffectWidth::word,first_output});
        memory_effects_.push_back({0x1959,0x0110,
            MillenniumDosTitleInitializationEffectWidth::word,second_output});
        effects_.insert(effects_.end(),{{0x1941,"CX",0x0025},
            {0x1944,"DX",0x0170},{0x1949,"SI",0x010c},
            {0x194c,"AX",first_output},{0x1952,"SI",0x0110},
            {0x1955,"AX",second_output},{0x195b,"AX",0x0026},
            {0x195e,"AX",0x0001},{0x1390,"CX",0x000c},
            {0x1393,"AX",0x000c},{0x1395,"SI",
                static_cast<std::uint16_t>(source_offset+0x000c)},
            {0x1395,"DS",source_segment},{0x139a,"DI",destination_offset},
            {0x139a,"ES",destination_segment},{0x139f,"DX",destination_segment},
            {0x13a1,"BX",destination_offset},{0x13a4,"ES",child_code_segment_},
            {0x13a5,"DI",0x138c}});
        far_read_boundary_={0x13aa,source_segment,
            static_cast<std::uint16_t>(source_offset+0x000c),2,
            child_code_segment_,0x138c};
        last_sequence_=observation.sequence;
        continuation_address_=0x13aa;
        state_=MillenniumDosTitleInitializationState::post_descriptor_first_loop_far_read_boundary;
        return;
    }
    if(!observation.record_bytes.empty()||observation.record_segment!=0
        ||observation.record_offset!=0)
        throw std::runtime_error("Unexpected Millennium DOS private-ABI record observation");
    if(result_state==MillenniumDosTitleInitializationState::graphics_record_private_interrupt_result_boundary){
        graphics_record_observed_ax_=observation.ax;
        graphics_record_observed_flags_=observation.flags;
        boundary_.result_observed=true;
        // The wrapper epilogue $0129..$012e and helper RET at $1767 preserve
        // the raw result. The caller then enters the exact $1004 prefix,
        // which writes CS into the function-$001a request record.
        effects_.insert(effects_.end(),{{0x1004,"AX",child_code_segment_},
            {0x1006,"ES",child_code_segment_},{0x1008,"BX",0x0fdf},
            {0x100e,"AX",0x001a}});
        memory_effects_.push_back({0x100b,0x0fe7,
            MillenniumDosTitleInitializationEffectWidth::word,child_code_segment_});
        boundary_={0x1011,0x0122,0x0127,0x91,0x001a,
            child_code_segment_,0x0fdf,false,false};
        last_sequence_=observation.sequence;
        continuation_address_=0x0127;
        state_=MillenniumDosTitleInitializationState::post_descriptor_private_interrupt_result_boundary;
        return;
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
        far_read_boundary_,far_word_observations_,far_single_word_observations_,far_byte_boundary_,far_byte_observations_,failure_address_,
        continuation_address_,post_video_observed_ax_,post_video_observed_flags_,
        graphics_record_observed_ax_,graphics_record_observed_flags_,
        post_descriptor_observed_ax_,post_descriptor_observed_flags_,
        post_descriptor_observed_record_};
}

} // namespace eon
