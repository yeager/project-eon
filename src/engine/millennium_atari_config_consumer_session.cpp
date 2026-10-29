#include "engine/millennium_atari_config_consumer_session.hpp"
#include "data/sha256.hpp"

#include <array>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace eon {
namespace {

template <std::size_t Size>
struct ExecutableByteAnchor {
    std::string_view sha256;
    static constexpr std::size_t size() { return Size; }
};

std::uint8_t require_byte(const NativeRuntimeMemory& memory, const std::uint32_t address) {
    const auto value = memory.read_byte(
        {NativeRuntimeAddressSpace::linear, std::nullopt, address});
    if (!value) throw std::runtime_error("Millennium Atari config byte is absent from native memory");
    return *value;
}

template <std::size_t Size>
bool memory_matches(const NativeRuntimeMemory& memory, const std::uint32_t address,
    const ExecutableByteAnchor<Size>& anchor) {
    std::array<std::uint8_t, Size> observed{};
    for (std::size_t index = 0; index < Size; ++index) {
        observed[index] = require_byte(memory,
            address + static_cast<std::uint32_t>(index));
    }
    return to_hex(sha256(observed)) == anchor.sha256;
}

} // namespace

MillenniumAtariConfigConsumerSession::MillenniumAtariConfigConsumerSession(
    const std::uint64_t generation, const NativeRuntimeMemory& memory,
    const MillenniumAtariReadOnlyGemdosCheckpoint& gemdos,
    const MillenniumAtariFreadConfigLoadAddressBoundary& load_boundary,
    const MillenniumAtariFreadMappedConfigPrelude& prelude) {
    constexpr std::uint32_t jsr_instruction = 0x7703c;
    constexpr std::uint32_t jsr_return = 0x77042;
    constexpr std::uint32_t jsr_target = 0x2a500;
    constexpr std::uint32_t jump_target = 0x2aa88;
    constexpr std::uint16_t move_sr_d0 = 0x40c0;
    constexpr std::string_view prelude_sha256 =
        "dede20eddbd8015da1d1a4f2f5e53424c2bc2195bff238d830ea24c9f522ea59";
    const auto require_span = [&memory]<std::size_t Size>(const std::uint32_t address,
        const ExecutableByteAnchor<Size>& anchor, const char* message) {
        if (!memory_matches(memory, address, anchor)) throw std::runtime_error(message);
    };
    constexpr ExecutableByteAnchor<20> xbios_prefix{"751915c217471e4763ebeef2928dc4cca68bc481dae3113adabb441c2446ee2f"};
    require_span(0x2a51cU, xbios_prefix, "Unexpected Millennium Atari XBIOS continuation bytes");
    constexpr ExecutableByteAnchor<16> selector_three_continuation{"f4a7b019591ccff43e4478ac1549e262387ebfb22c16ded18457fe2aca6bbcc2"};
    require_span(0x2a52eU, selector_three_continuation, "Unexpected Millennium Atari selector-3 continuation bytes");
    constexpr ExecutableByteAnchor<12> selector_four_continuation{"42c6d7ede7609ced9c859e6222d678edf861018b86ee80be2cfe6f8a23010e44"};
    require_span(0x2a53cU, selector_four_continuation, "Unexpected Millennium Atari selector-4 continuation bytes");
    constexpr ExecutableByteAnchor<24> line_a_continuation{"1705523f57debe7644c3a874cd76e42464f1f34f227c9ee1247026afdb2f3539"};
    require_span(0x2a546U, line_a_continuation, "Unexpected Millennium Atari Line-A continuation bytes");
    constexpr ExecutableByteAnchor<8> caller_continuation{"37f9fb95e45dc6c4807821ac79189a2d764fffe6bbbef6196ee17f3ad1a18684"};
    require_span(0x2aaaaU, caller_continuation, "Unexpected Millennium Atari Line-A caller bytes");
    constexpr ExecutableByteAnchor<16> selector_21_continuation{"de3f0996c3b76c20c1e83a686f9a97f7a5ad8f9575a03d8f01b7f4cadf45a233"};
    require_span(0x2aab0U, selector_21_continuation, "Unexpected Millennium Atari selector-21 continuation bytes");
    constexpr ExecutableByteAnchor<10> selector_6_continuation{"ba614a28f861921a263225ef85209b20dc2673ea3444cb556b88ca29b2b23163"};
    require_span(0x2aabeU, selector_6_continuation, "Unexpected Millennium Atari selector-6 continuation bytes");
    constexpr ExecutableByteAnchor<8> jsr_prefix{"b1b4328c9f54737553994259dac4dfb0247bf422414ed05a1c5c6166ec37ba62"};
    require_span(0x2b55aU, jsr_prefix, "Unexpected $2b55a prefix");
    constexpr ExecutableByteAnchor<16> bsr_prefix{"967cb0022c8e29e0bef0dae618b95750fff3afa255094f9356210f1c89686fa3"};
    require_span(0x2b59aU, bsr_prefix, "Unexpected $2b59a prefix");
    constexpr ExecutableByteAnchor<12> indexed_writes{"e87859079e18a266cc359d7e0be47667c5cfe79dbffa05daad80ee951fa777d7"};
    require_span(0x2b5a6U, indexed_writes, "Unexpected indexed writes");
    constexpr ExecutableByteAnchor<48> a1_setup{"4345389397550c90280802d10a3f03b3e181745bcb98f8c693a2c0980722a1ef"};
    require_span(0x2b5b2U, a1_setup, "Unexpected A1 setup bytes");
    constexpr ExecutableByteAnchor<18> indexed_word{"6fae36f2f65050ca3ff99c8cb73f43a8c130dd4d252d4b7d38d0be9118eeba78"};
    require_span(0x2b5deU, indexed_word, "Unexpected indexed word bytes");
    constexpr ExecutableByteAnchor<20> tail{"82379ace33d5464b74e03aa0669f8a1097498fd21ce3639c180ab5e21cac810b"};
    require_span(0x2b5ecU, tail, "Unexpected indexed tail");
    constexpr ExecutableByteAnchor<42> loop_setup{"9efa7511411f3ca6698746d8bac484420a14e67e35467be2909f3647b0612034"};
    require_span(0x2b5b8U, loop_setup, "Unexpected loop setup");
    constexpr ExecutableByteAnchor<28> epilogue{"51ea54e46ad38380435c7a367889825fce566b4f33036fb5dd38846dafdf4ab7"};
    require_span(0x2b600U, epilogue, "Unexpected loop epilogue");
    constexpr ExecutableByteAnchor<6> movem_rts{"7f09b538ef863cae65b4a16e1301251bde1fed37c1dba591dd4ec9f4b34106b1"};
    constexpr ExecutableByteAnchor<6> caller_jsr{"fd41f7c5a0cdb684768c3da230cb9ca56bac136abd2254b55090d6b1cf58da78"};
    require_span(0x2b562U, movem_rts, "Unexpected MOVEM return continuation");
    require_span(0x2aac8U, caller_jsr, "Unexpected MOVEM caller continuation");
    constexpr ExecutableByteAnchor<12> aa68_prefix{"fd6e1ace58bbc4108fcc0b8a7f75103c04337c41d24b2c9de5907f9538aaf439"};
    require_span(0x2aa68U, aa68_prefix, "Unexpected $2aa68 prefix");
    constexpr ExecutableByteAnchor<6> selector38_return{"59f7345ed980fd79117e7ad10db1a93c3872cafca3000afb7ef3f7eda5603adc"};
    constexpr ExecutableByteAnchor<12> selector38_caller{"7218804023c2ec3e694e19b581efeb17703f7bfe78d77b6da330354cc23a18f2"};
    require_span(0x2aa72U, selector38_return, "Unexpected selector-38 return");
    require_span(0x2aaceU, selector38_caller, "Unexpected selector-38 caller");
    constexpr ExecutableByteAnchor<6> jsr_2a5aa{"25939d2a8a98420749b181f742081cc576f302cffd0bea5b8008765af3b5d9f0"};
    constexpr ExecutableByteAnchor<12> gemdos61{"bdfb77219a19903ee730f3361af0958841aae3570ef3ed0d2ea60c3b56a3491e"};
    require_span(0x2aa0cU, jsr_2a5aa, "Unexpected $2aa0c caller");
    require_span(0x2a5aaU, gemdos61, "Unexpected GEMDOS 61 prefix");
    constexpr ExecutableByteAnchor<12> gemdos61_return{"dfe4c3bc4466d6d8772f3633cb125f64ea7a9114d3d0be45aca5be3daf28b30b"};
    constexpr ExecutableByteAnchor<12> fopen_branch{"fc103b11f1dfc5ee90e07376b76549242e52fd0823e6c6775f090cfd8ef7204d"};
    constexpr ExecutableByteAnchor<18> fopen_positive{"84dffae90b11dbe9032b435f839fb2f0afe57565b2052c7dcf177c06410b6d32"};
    require_span(0x2a5b6U, gemdos61_return, "Unexpected GEMDOS 61 return");
    require_span(0x2aa12U, fopen_branch, "Unexpected Fopen caller branch");
    require_span(0x2aa1cU, fopen_positive, "Unexpected Fopen positive continuation");
    if(require_byte(memory,0x2a632U)!=0x60||require_byte(memory,0x2a633U)!=0xfe)throw std::runtime_error("Unexpected Fopen failure spin");
    constexpr ExecutableByteAnchor<16> gemdos63{"6d2ddd7da4866769c78162433427fb37fe2f885926f429c098fca3062e282921"};
    require_span(0x2a5c2U, gemdos63, "Unexpected GEMDOS 63 prefix");
    constexpr ExecutableByteAnchor<10> gemdos63_return{"9f590fdbc6197d898da37312cddcb27a0411bf687877778f77320cb5c61f8ed3"};
    constexpr ExecutableByteAnchor<6> fread_caller_jump{"a3bf89946746662879548e7a74f8f77c8d107c234cae2908c9b94abe94b19f89"};
    constexpr ExecutableByteAnchor<12> gemdos62{"e815352850ca1cb7dffb7fa6d7e46d7775e82146695009e790e525daac17a2e9"};
    require_span(0x2a5d2U, gemdos63_return, "Unexpected GEMDOS 63 return");
    require_span(0x2aa2eU, fread_caller_jump, "Unexpected Fread caller jump");
    require_span(0x2a5dcU, gemdos62, "Unexpected GEMDOS 62 prefix");
    constexpr ExecutableByteAnchor<6> gemdos62_return{"1653b046f59ffdf7cdcdae81914ab08b45f9fd09915e21b1c27ea8c6021e0b2f"};
    constexpr ExecutableByteAnchor<18> fclose_caller{"b57a65f987273c544a3b4bb8826792332a01aa3fe3ba019798bbb437ba2eca1f"};
    constexpr ExecutableByteAnchor<6> jsr_2b2be{"2454ed31c410499746fd0817e23556106ddbb3a6089f52720385a9d815567450"};
    require_span(0x2a5e8U, gemdos62_return, "Unexpected GEMDOS 62 return");
    require_span(0x2aadaU, fclose_caller, "Unexpected Fclose caller continuation");
    require_span(0x2aaecU, jsr_2b2be, "Unexpected $2b2be caller");
    constexpr ExecutableByteAnchor<34> game_init_setup{"8086ab4a24f6f30bc4c267337bf872a4d258efae4ac89d0726392167ebd86de5"};
    require_span(0x2b2beU, game_init_setup, "Unexpected game-init setup");
    constexpr ExecutableByteAnchor<12> game_init_source_dispatch{"948e269d0e24d6ec05013d07ffe3d3ba66400189b98a30d676b44e5b39683fe6"};
    constexpr ExecutableByteAnchor<16> game_init_nonzero_dispatch{"4b98ca43cbf9af758b5d56087a8d113f23fedf107e1320a2a6ee137d6cfe92c3"};
    require_span(0x2b2deU, game_init_source_dispatch, "Unexpected game-init source dispatch");
    require_span(0x2b322U, game_init_nonzero_dispatch, "Unexpected game-init nonzero dispatch");
    constexpr ExecutableByteAnchor<48> game_init_zero_counter_continuation{"9b3476f5d2ecb028149eec6ee575cd79c7c9f94589a7e7398d794ecd176f04ef"};
    require_span(0x2b2f2U, game_init_zero_counter_continuation, "Unexpected game-init zero-counter continuation");
    constexpr ExecutableByteAnchor<68> game_init_replicated_run{"6429d7b0634cff176ec01486b3f4e05bd648e3de11a67edd151f8345724b6701"};
    constexpr ExecutableByteAnchor<66> game_init_swapped_run{"dbf80460ade3c9cc5fba8b4a62937920cc9e131052d3a48bfc8b0981e150a9b9"};
    constexpr ExecutableByteAnchor<14> game_init_extended_prefix{"72fa63385edc5122cd3fe1c4031d0a0089a187d498c04ff1f8be912f4462b0c5"};
    if(!memory_matches(memory,0x2b332U,game_init_replicated_run))throw std::runtime_error("Unexpected replicated-run continuation");
    if(!memory_matches(memory,0x2b376U,game_init_swapped_run))throw std::runtime_error("Unexpected swapped-run continuation");
    require_span(0x2b3b8U, game_init_extended_prefix, "Unexpected extended-run prefix");
    constexpr ExecutableByteAnchor<18> game_init_caller_2b448{"155575e295ad1e7831c0eef9809316db6f68321beb0661c03b7c14bb141f793e"};
    constexpr ExecutableByteAnchor<62> game_init_palette_copy_prefix{"748d9b2df05839b68583069e29ff34954477ce7a367b0a88ef9e9bad7abfa0ca"};
    constexpr ExecutableByteAnchor<40> game_init_palette_arithmetic{"0866601f1a271ee74b399dd544b5b1ced15693e600c30034531a094dbc41d746"};
    constexpr ExecutableByteAnchor<16> game_init_palette_post_xbios{"9e3fd4aeca606c5560b204d12a20a77de12552ded7fa64a0677cca56c4676bf1"};
    constexpr ExecutableByteAnchor<10> game_init_palette_terminal{"876ea72e7f61e2604ffa34d0fae7a6c1b3f880aa43e88006af18e1f67677c967"};
    constexpr ExecutableByteAnchor<12> game_init_palette_caller{"ae672762da7616abc67d0a1e5a5aaf3ab540b96b94b9689b31f8a11a8de256d7"};
    constexpr ExecutableByteAnchor<22> game_init_second_config_caller{"eea2683953b1fe18e3e7b88e1744fa10a9684444fe183d283efee9f54302c1a0"};
    require_span(0x2aaf2U, game_init_caller_2b448, "Unexpected post-game-init caller bytes");
    require_span(0x2b448U, game_init_palette_copy_prefix, "Unexpected palette-copy prefix");
    require_span(0x2b486U, game_init_palette_arithmetic, "Unexpected palette arithmetic");
    require_span(0x2b4aeU, game_init_palette_post_xbios, "Unexpected post-XBIOS palette continuation");
    require_span(0x2b4beU, game_init_palette_terminal, "Unexpected terminal palette continuation");
    require_span(0x2ab04U, game_init_palette_caller, "Unexpected palette caller continuation");
    require_span(0x2ab10U, game_init_second_config_caller, "Unexpected second-config caller continuation");
    if (generation == 0 || gemdos.generation != generation
        || gemdos.state != MillenniumAtariReadOnlyGemdosState::config_jsr_boundary
        || gemdos.config_jsr_instruction_address != jsr_instruction
        || gemdos.config_jsr_target_address != jsr_target
        || load_boundary.fread_destination_address != jsr_target
        || load_boundary.payload_initial_jump_opcode != 0x4ef9
        || load_boundary.payload_initial_jump_target_address != jump_target
        || load_boundary.payload_initial_jump_target_file_offset_from_destination != 0x588
        || prelude.fread_destination_address != jsr_target
        || prelude.mapped_entry_address != jump_target
        || prelude.mapped_entry_file_offset != 0x588
        || prelude.initial_opcode != move_sr_d0 || prelude.sha256 != prelude_sha256
        || require_byte(memory, jsr_target) != 0x4e
        || require_byte(memory, jsr_target + 1U) != 0xf9
        || (static_cast<std::uint32_t>(require_byte(memory, jsr_target + 2U)) << 24U
            | static_cast<std::uint32_t>(require_byte(memory, jsr_target + 3U)) << 16U
            | static_cast<std::uint32_t>(require_byte(memory, jsr_target + 4U)) << 8U
            | require_byte(memory, jsr_target + 5U)) != jump_target
        || (static_cast<std::uint16_t>(require_byte(memory, jump_target)) << 8U
            | require_byte(memory, jump_target + 1U)) != move_sr_d0) {
        throw std::runtime_error("Unexpected Millennium Atari config consumer entry");
    }
    checkpoint_.generation = generation;
    checkpoint_.state = MillenniumAtariConfigConsumerState::status_register_boundary;
    checkpoint_.jsr_instruction_address = jsr_instruction;
    checkpoint_.jsr_return_address = jsr_return;
    checkpoint_.jsr_target_address = jsr_target;
    checkpoint_.entry_jump_opcode = load_boundary.payload_initial_jump_opcode;
    checkpoint_.entry_jump_target_address = jump_target;
    checkpoint_.entry_jump_file_offset =
        load_boundary.payload_initial_jump_target_file_offset_from_destination;
    checkpoint_.boundary_instruction_address = jump_target;
    checkpoint_.boundary_opcode = move_sr_d0;
    checkpoint_.boundary_dependency = "68000 SR privilege/status value";
    checkpoint_.mapped_prelude_sha256 = std::string(prelude_sha256);
    checkpoint_.selector_two_continuation_sha256 =
        "751915c217471e4763ebeef2928dc4cca68bc481dae3113adabb441c2446ee2f";
    checkpoint_.selector_three_continuation_sha256 =
        "f4a7b019591ccff43e4478ac1549e262387ebfb22c16ded18457fe2aca6bbcc2";
    checkpoint_.selector_four_continuation_sha256 =
        "42c6d7ede7609ced9c859e6222d678edf861018b86ee80be2cfe6f8a23010e44";
    checkpoint_.line_a_continuation_sha256 =
        "1705523f57debe7644c3a874cd76e42464f1f34f227c9ee1247026afdb2f3539";
    checkpoint_.line_a_caller_continuation_sha256 =
        "37f9fb95e45dc6c4807821ac79189a2d764fffe6bbbef6196ee17f3ad1a18684";
    checkpoint_.selector_21_continuation_sha256 =
        "de3f0996c3b76c20c1e83a686f9a97f7a5ad8f9575a03d8f01b7f4cadf45a233";
    checkpoint_.selector_6_continuation_sha256 =
        "ba614a28f861921a263225ef85209b20dc2673ea3444cb556b88ca29b2b23163";
    checkpoint_.jsr_2b55a_prefix_sha256 =
        "b1b4328c9f54737553994259dac4dfb0247bf422414ed05a1c5c6166ec37ba62";
    checkpoint_.bsr_2b59a_prefix_sha256 =
        "967cb0022c8e29e0bef0dae618b95750fff3afa255094f9356210f1c89686fa3";
    checkpoint_.indexed_write_sha256 =
        "e87859079e18a266cc359d7e0be47667c5cfe79dbffa05daad80ee951fa777d7";
    checkpoint_.a1_setup_sha256 =
        "4345389397550c90280802d10a3f03b3e181745bcb98f8c693a2c0980722a1ef";
    checkpoint_.indexed_word_sha256="6fae36f2f65050ca3ff99c8cb73f43a8c130dd4d252d4b7d38d0be9118eeba78";
    checkpoint_.a0_indexed_tail_sha256="82379ace33d5464b74e03aa0669f8a1097498fd21ce3639c180ab5e21cac810b";
    checkpoint_.loop_setup_sha256="9efa7511411f3ca6698746d8bac484420a14e67e35467be2909f3647b0612034";
    checkpoint_.loop_epilogue_sha256="51ea54e46ad38380435c7a367889825fce566b4f33036fb5dd38846dafdf4ab7";
    checkpoint_.movem_rts_sha256="7f09b538ef863cae65b4a16e1301251bde1fed37c1dba591dd4ec9f4b34106b1";
    checkpoint_.caller_jsr_2aa68_sha256="fd41f7c5a0cdb684768c3da230cb9ca56bac136abd2254b55090d6b1cf58da78";
    checkpoint_.jsr_2aa68_prefix_sha256="fd6e1ace58bbc4108fcc0b8a7f75103c04337c41d24b2c9de5907f9538aaf439";
    checkpoint_.selector_38_return_sha256="59f7345ed980fd79117e7ad10db1a93c3872cafca3000afb7ef3f7eda5603adc";
    checkpoint_.selector_38_caller_sha256="7218804023c2ec3e694e19b581efeb17703f7bfe78d77b6da330354cc23a18f2";
    checkpoint_.caller_jsr_2a5aa_sha256="25939d2a8a98420749b181f742081cc576f302cffd0bea5b8008765af3b5d9f0";
    checkpoint_.gemdos_61_prefix_sha256="bdfb77219a19903ee730f3361af0958841aae3570ef3ed0d2ea60c3b56a3491e";
    std::vector<std::uint8_t> palette_source_bytes;
    palette_source_bytes.reserve(checkpoint_.game_init_palette_source_longs.size()*4U);
    for(std::size_t i=0;i<checkpoint_.game_init_palette_source_longs.size();++i){
        const auto address=0x2a66cU+static_cast<std::uint32_t>(i*4U);
        const auto first=require_byte(memory,address),second=require_byte(memory,address+1U),third=require_byte(memory,address+2U),fourth=require_byte(memory,address+3U);
        palette_source_bytes.insert(palette_source_bytes.end(),{first,second,third,fourth});
        checkpoint_.game_init_palette_source_longs[i]=(static_cast<std::uint32_t>(first)<<24U)|(static_cast<std::uint32_t>(second)<<16U)|(static_cast<std::uint32_t>(third)<<8U)|fourth;
    }
    checkpoint_.game_init_palette_source_sha256=to_hex(sha256(palette_source_bytes));
    if(checkpoint_.game_init_palette_source_sha256!="a2263d35c251e787a9a5705a5277bcf641321817f825e7689081280fbd157dfe")
        throw std::runtime_error("Unexpected palette-copy source SHA-256 "+checkpoint_.game_init_palette_source_sha256);
    checkpoint_.local_control_transfers_executed = 2;
}

MillenniumAtariConfigConsumerResult
MillenniumAtariConfigConsumerSession::observe_status_register(
    const MillenniumAtariStatusRegisterObservation& observation) {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::status_register_boundary) {
        return {false, "Millennium Atari config consumer is not at the SR boundary"};
    }
    if (observation.generation != checkpoint_.generation || observation.sequence == 0
        || observation.sequence <= checkpoint_.last_sequence
        || observation.instruction_address != checkpoint_.boundary_instruction_address) {
        return {false, "Millennium Atari SR observation is stale or at the wrong instruction"};
    }
    constexpr std::uint16_t supervisor_mask = 0x2000;
    const bool supervisor = (observation.status_register & supervisor_mask) != 0;
    if (supervisor != (observation.privilege == MillenniumAtariObservedPrivilege::supervisor)) {
        return {false, "Millennium Atari SR value contradicts observed privilege"};
    }

    auto next = checkpoint_;
    next.state = MillenniumAtariConfigConsumerState::xbios_trap_boundary;
    next.last_sequence = observation.sequence;
    next.status_register_read = true;
    next.observed_status_register = observation.status_register;
    next.observed_privilege = observation.privilege;
    next.supervisor_bit_was_set = supervisor;
    // BCLR #13,D0 sets Z when the observed S bit was clear. BEQ therefore
    // bypasses hardware setup for an observed user-mode SR.
    next.branch_taken = !supervisor;
    next.converged_jsr_address = 0x2aaa4;
    next.converged_jsr_target = 0x2a51c;
    next.converged_jsr_return_address = 0x2aaaa;
    next.xbios_trap_address = 0x2a520;
    next.xbios_selector = 2;
    next.local_instruction_count = supervisor ? 10U : 5U;
    next.local_control_transfers_executed = supervisor ? 3U : 4U;
    if (supervisor) {
        next.hardware_write_executed = true;
        next.resulting_status_register = 0x0300;
        next.hardware_writes = {
            {1, 0x2aa98, 0xffff8800U, 0x07},
            {2, 0x2aa98, 0xffff8802U, 0xff},
            {3, 0x2aa9c, 0xffff8800U, 0x0e},
        };
    } else {
        // BCLR found a clear bit and therefore sets CCR.Z before BEQ.
        next.resulting_status_register =
            static_cast<std::uint16_t>(observation.status_register | 0x0004U);
    }
    checkpoint_ = std::move(next);
    return {true, {}};
}

std::vector<NativeRuntimeEffectBatch>
MillenniumAtariConfigConsumerSession::make_hardware_effect_batches(
    std::string id_prefix) const {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::xbios_trap_boundary
        || checkpoint_.generation == 0 || id_prefix.empty()) {
        throw std::runtime_error("Millennium Atari hardware effects are not admitted");
    }
    if (!checkpoint_.hardware_write_executed) return {};
    if (checkpoint_.observed_privilege != MillenniumAtariObservedPrivilege::supervisor
        || checkpoint_.hardware_writes.size() != 3) {
        throw std::runtime_error("Millennium Atari hardware effects lack supervisor evidence");
    }
    // MOVEP writes two non-contiguous bytes. MOVE.B then intentionally
    // overwrites the first address, so it is a separate atomic batch.
    NativeRuntimeEffectBatch movep{id_prefix + "-movep", true, {}};
    movep.effects = {
        {1, {NativeRuntimeAddressSpace::linear, std::nullopt, 0xffff8800U},
            MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian, 0x07},
        {2, {NativeRuntimeAddressSpace::linear, std::nullopt, 0xffff8802U},
            MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian, 0xff},
    };
    NativeRuntimeEffectBatch move_byte{id_prefix + "-move-byte", true, {{1,
        {NativeRuntimeAddressSpace::linear, std::nullopt, 0xffff8800U},
        MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian, 0x0e}}};
    return {std::move(movep), std::move(move_byte)};
}

MillenniumAtariConfigConsumerResult
MillenniumAtariConfigConsumerSession::observe_xbios_selector_two(
    const MillenniumAtariXbiosSelectorTwoObservation& observation) {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::xbios_trap_boundary) {
        return {false, "Millennium Atari config consumer is not at XBIOS selector 2"};
    }
    if (observation.generation != checkpoint_.generation
        || observation.sequence <= checkpoint_.last_sequence
        || observation.trap_address != 0x2a520 || observation.selector != 2) {
        return {false, "Millennium Atari XBIOS selector-2 observation is stale or mismatched"};
    }
    auto next = checkpoint_;
    next.state = MillenniumAtariConfigConsumerState::xbios_selector_three_boundary;
    next.last_sequence = observation.sequence;
    next.selector_two_result_observed = true;
    next.selector_two_result_d0 = observation.result_d0;
    next.selector_two_store_address = 0x2a50a;
    next.selector_two_stack_cleanup_bytes = 2;
    next.xbios_trap_address = 0x2a52e;
    next.xbios_selector = 3;
    next.local_instruction_count += 3;
    checkpoint_ = std::move(next);
    return {true, {}};
}

NativeRuntimeEffectBatch
MillenniumAtariConfigConsumerSession::make_selector_two_result_effect_batch(
    std::string id) const {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::xbios_selector_three_boundary
        || !checkpoint_.selector_two_result_observed || checkpoint_.generation == 0
        || checkpoint_.selector_two_store_address != 0x2a50a || id.empty()) {
        throw std::runtime_error("Millennium Atari selector-2 result effect is not admitted");
    }
    return {std::move(id), true, {{1,
        {NativeRuntimeAddressSpace::linear, std::nullopt,
            checkpoint_.selector_two_store_address},
        MemoryTransferElementWidth::longword, NativeRuntimeByteOrder::big_endian,
        checkpoint_.selector_two_result_d0}}};
}

MillenniumAtariConfigConsumerResult
MillenniumAtariConfigConsumerSession::observe_xbios_selector_three(
    const MillenniumAtariXbiosSelectorThreeObservation& observation) {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::xbios_selector_three_boundary) {
        return {false, "Millennium Atari config consumer is not at XBIOS selector 3"};
    }
    if (observation.generation != checkpoint_.generation
        || observation.sequence <= checkpoint_.last_sequence
        || observation.trap_address != 0x2a52e || observation.selector != 3) {
        return {false, "Millennium Atari XBIOS selector-3 observation is stale or mismatched"};
    }
    auto next = checkpoint_;
    next.state = MillenniumAtariConfigConsumerState::xbios_selector_four_boundary;
    next.last_sequence = observation.sequence;
    next.selector_three_result_observed = true;
    next.selector_three_result_d0 = observation.result_d0;
    next.selector_three_store_address = 0x2a50e;
    next.selector_four_stack_cleanup_bytes = 2;
    next.xbios_trap_address = 0x2a53c;
    next.xbios_selector = 4;
    next.local_instruction_count += 3;
    checkpoint_ = std::move(next);
    return {true, {}};
}

NativeRuntimeEffectBatch
MillenniumAtariConfigConsumerSession::make_selector_three_result_effect_batch(
    std::string id) const {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::xbios_selector_four_boundary
        || !checkpoint_.selector_three_result_observed || checkpoint_.generation == 0
        || checkpoint_.selector_three_store_address != 0x2a50e || id.empty()) {
        throw std::runtime_error("Millennium Atari selector-3 result effect is not admitted");
    }
    return {std::move(id), true, {{1,
        {NativeRuntimeAddressSpace::linear, std::nullopt,
            checkpoint_.selector_three_store_address},
        MemoryTransferElementWidth::longword, NativeRuntimeByteOrder::big_endian,
        checkpoint_.selector_three_result_d0}}};
}

MillenniumAtariConfigConsumerResult
MillenniumAtariConfigConsumerSession::observe_xbios_selector_four(
    const MillenniumAtariXbiosSelectorFourObservation& observation) {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::xbios_selector_four_boundary) {
        return {false, "Millennium Atari config consumer is not at XBIOS selector 4"};
    }
    if (observation.generation != checkpoint_.generation
        || observation.sequence <= checkpoint_.last_sequence
        || observation.trap_address != 0x2a53c || observation.selector != 4) {
        return {false, "Millennium Atari XBIOS selector-4 observation is stale or mismatched"};
    }
    auto next = checkpoint_;
    next.state = MillenniumAtariConfigConsumerState::line_a_init_boundary;
    next.last_sequence = observation.sequence;
    next.selector_four_result_observed = true;
    next.selector_four_result_d0_word = static_cast<std::uint16_t>(observation.result_d0);
    next.selector_four_store_address = 0x2a512;
    next.selector_three_stack_cleanup_bytes = 2;
    next.line_a_init_address = 0x2a546;
    next.line_a_init_opcode = 0xa000;
    next.local_instruction_count += 2;
    checkpoint_ = std::move(next);
    return {true, {}};
}

NativeRuntimeEffectBatch
MillenniumAtariConfigConsumerSession::make_selector_four_result_effect_batch(
    std::string id) const {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::line_a_init_boundary
        || !checkpoint_.selector_four_result_observed || checkpoint_.generation == 0
        || checkpoint_.selector_four_store_address != 0x2a512 || id.empty()) {
        throw std::runtime_error("Millennium Atari selector-4 result effect is not admitted");
    }
    return {std::move(id), true, {{1,
        {NativeRuntimeAddressSpace::linear, std::nullopt,
            checkpoint_.selector_four_store_address},
        MemoryTransferElementWidth::word, NativeRuntimeByteOrder::big_endian,
        checkpoint_.selector_four_result_d0_word}}};
}

MillenniumAtariConfigConsumerResult
MillenniumAtariConfigConsumerSession::observe_line_a(
    const MillenniumAtariLineAObservation& observation) {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::line_a_init_boundary) {
        return {false, "Millennium Atari config consumer is not at Line-A init"};
    }
    if (observation.generation != checkpoint_.generation
        || observation.sequence <= checkpoint_.last_sequence
        || observation.instruction_address != 0x2a546
        || observation.returned_a0 > 0xfffffff3U) {
        return {false, "Millennium Atari Line-A observation is stale or mismatched"};
    }
    auto next = checkpoint_;
    next.state = MillenniumAtariConfigConsumerState::xbios_selector_21_boundary;
    next.last_sequence = observation.sequence;
    next.line_a_result_observed = true;
    next.line_a_returned_a0 = observation.returned_a0;
    next.line_a_result_a3 = observation.value_at_a0_plus_8;
    next.line_a_result_a4 = observation.value_at_a0_plus_12;
    next.line_a_a3_store_address = 0x2a514;
    next.line_a_a4_store_address = 0x2a518;
    next.xbios_trap_address = 0x2aab0;
    next.xbios_selector = 0x15;
    next.local_instruction_count += 7;
    checkpoint_ = std::move(next);
    return {true, {}};
}

NativeRuntimeEffectBatch MillenniumAtariConfigConsumerSession::make_line_a_result_effect_batch(
    std::string id) const {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::xbios_selector_21_boundary
        || !checkpoint_.line_a_result_observed || checkpoint_.generation == 0 || id.empty()) {
        throw std::runtime_error("Millennium Atari Line-A result effect is not admitted");
    }
    return {std::move(id), true, {
        {1, {NativeRuntimeAddressSpace::linear, std::nullopt, 0x2a514},
            MemoryTransferElementWidth::longword, NativeRuntimeByteOrder::big_endian,
            checkpoint_.line_a_result_a3},
        {2, {NativeRuntimeAddressSpace::linear, std::nullopt, 0x2a518},
            MemoryTransferElementWidth::longword, NativeRuntimeByteOrder::big_endian,
            checkpoint_.line_a_result_a4},
    }};
}

MillenniumAtariConfigConsumerResult
MillenniumAtariConfigConsumerSession::observe_xbios_selector_21(
    const MillenniumAtariXbiosSelector21Observation& observation) {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::xbios_selector_21_boundary) {
        return {false, "Millennium Atari config consumer is not at XBIOS selector 21"};
    }
    if (observation.generation != checkpoint_.generation
        || observation.sequence <= checkpoint_.last_sequence
        || observation.trap_address != 0x2aab0 || observation.selector != 0x15) {
        return {false, "Millennium Atari XBIOS selector-21 observation is stale or mismatched"};
    }
    auto next = checkpoint_;
    next.state = MillenniumAtariConfigConsumerState::xbios_selector_6_boundary;
    next.last_sequence = observation.sequence;
    next.selector_21_result_observed = true;
    next.selector_21_result_d0 = observation.result_d0;
    next.selector_21_stack_cleanup_bytes = 6;
    next.selector_6_pointer_argument = 0x2a612;
    next.xbios_trap_address = 0x2aabe;
    next.xbios_selector = 6;
    next.local_instruction_count += 3;
    checkpoint_ = std::move(next);
    return {true, {}};
}

MillenniumAtariConfigConsumerResult
MillenniumAtariConfigConsumerSession::observe_xbios_selector_6(
    const MillenniumAtariXbiosSelector6Observation& observation) {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::xbios_selector_6_boundary) {
        return {false, "Millennium Atari config consumer is not at XBIOS selector 6"};
    }
    if (observation.generation != checkpoint_.generation
        || observation.sequence <= checkpoint_.last_sequence
        || observation.trap_address != 0x2aabe || observation.selector != 6) {
        return {false, "Millennium Atari XBIOS selector-6 observation is stale or mismatched"};
    }
    auto next = checkpoint_;
    next.state = MillenniumAtariConfigConsumerState::jsr_2b55a_boundary;
    next.last_sequence = observation.sequence;
    next.selector_6_result_observed = true;
    next.selector_6_result_d0 = observation.result_d0;
    next.selector_6_stack_cleanup_bytes = 6;
    next.next_jsr_address = 0x2aac2;
    next.next_jsr_target = 0x2b55a;
    next.local_instruction_count += 1;
    checkpoint_ = std::move(next);
    return {true, {}};
}

MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_bchg_2b55a(
    const MillenniumAtariBchgObservation& observation) {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::jsr_2b55a_boundary) {
        return {false, "Millennium Atari consumer is not at JSR $2b55a"};
    }
    static_cast<void>(observation);
    return {false, "JSR $2b55a is outside the exact $2a500-loaded config mapping"};
}

NativeRuntimeEffectBatch MillenniumAtariConfigConsumerSession::make_bchg_effect_batch(
    std::string id) const {
    static_cast<void>(id);
    throw std::runtime_error(
        "Millennium Atari JSR $2b55a has no admitted memory effect");
}

MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::execute_jsr_2b55a() {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::jsr_2b55a_boundary) {
        return {false, "Millennium Atari consumer is not at JSR $2b55a"};
    }
    checkpoint_.state = MillenniumAtariConfigConsumerState::bsr_2b59a_boundary;
    checkpoint_.bsr_instruction_address = 0x2b55e;
    checkpoint_.bsr_target = 0x2b59a;
    checkpoint_.local_instruction_count += 2;
    return {true, {}};
}

MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::execute_bsr_2b59a() {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::bsr_2b59a_boundary) {
        return {false, "Millennium Atari consumer is not at BSR $2b59a"};
    }
    checkpoint_.state = MillenniumAtariConfigConsumerState::d0_indexed_write_boundary;
    checkpoint_.bsr_return_address = 0x2b562;
    checkpoint_.callee_a3 = 0x2b0e8;
    checkpoint_.callee_clear_address = 0x2b6b8;
    checkpoint_.indexed_instruction_address = 0x2b5a6;
    checkpoint_.local_instruction_count += 3; // BSR, LEA, CLR.B
    return {true, {}};
}

NativeRuntimeEffectBatch MillenniumAtariConfigConsumerSession::make_bsr_2b59a_effect_batch(
    std::string id) const {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::d0_indexed_write_boundary
        || id.empty()) throw std::runtime_error("Millennium Atari $2b59a effect is not admitted");
    return {std::move(id), true, {{1,
        {NativeRuntimeAddressSpace::linear, std::nullopt, checkpoint_.callee_clear_address},
        MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian, 0}}};
}

MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_d0_indexed_byte(
    const MillenniumAtariD0IndexedByteObservation& observation) {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::d0_indexed_write_boundary) return {false, "Not at D0-indexed write"};
    const auto displacement = static_cast<std::int16_t>(observation.d0 & 0xffffU);
    const auto expected = static_cast<std::uint32_t>(0x2bdfdLL + displacement);
    if (observation.generation != checkpoint_.generation || observation.sequence <= checkpoint_.last_sequence
        || observation.instruction_address != 0x2b5a6 || observation.source_address != expected) return {false, "D0-indexed observation mismatch"};
    checkpoint_.state = MillenniumAtariConfigConsumerState::a1_setup_boundary;
    checkpoint_.last_sequence = observation.sequence;
    checkpoint_.indexed_source_base = 0x2bdfc;
    checkpoint_.indexed_source_address = expected;
    checkpoint_.indexed_source_byte = observation.source_byte;
    checkpoint_.indexed_first_destination = 0x2b6b0;
    checkpoint_.indexed_second_destination = 0x2b6b1;
    checkpoint_.indexed_instruction_address = 0x2b5b2;
    checkpoint_.local_instruction_count += 2;
    return {true, {}};
}

NativeRuntimeEffectBatch MillenniumAtariConfigConsumerSession::make_d0_indexed_effect_batch(std::string id) const {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::a1_setup_boundary || id.empty()) throw std::runtime_error("Indexed effect unavailable");
    return {std::move(id), true, {
        {1, {NativeRuntimeAddressSpace::linear, std::nullopt, checkpoint_.indexed_first_destination}, MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian, checkpoint_.indexed_source_byte},
        {2, {NativeRuntimeAddressSpace::linear, std::nullopt, checkpoint_.indexed_second_destination}, MemoryTransferElementWidth::byte, NativeRuntimeByteOrder::big_endian, checkpoint_.indexed_source_byte}}};
}

MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::execute_a1_setup() {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::a1_setup_boundary) return {false, "Not at A1 setup"};
    checkpoint_.state = MillenniumAtariConfigConsumerState::d0_indexed_word_boundary;
    checkpoint_.setup_a1 = 0x2b61e;
    checkpoint_.setup_a0_first = 0x2bdcc;
    checkpoint_.setup_a0_second = 0x2bdfc;
    checkpoint_.setup_d7 = 2;
    checkpoint_.indexed_word_instruction_address = 0x2b5de;
    checkpoint_.local_instruction_count += 11;
    return {true, {}};
}

NativeRuntimeEffectBatch MillenniumAtariConfigConsumerSession::make_a1_setup_effect_batch(std::string id) const {
    if (checkpoint_.state != MillenniumAtariConfigConsumerState::d0_indexed_word_boundary || id.empty()) throw std::runtime_error("A1 setup effect unavailable");
    return {std::move(id), true, {
        {1,{NativeRuntimeAddressSpace::linear,std::nullopt,0x2b639},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,1},
        {2,{NativeRuntimeAddressSpace::linear,std::nullopt,0x2b61e},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,0},
        {3,{NativeRuntimeAddressSpace::linear,std::nullopt,0x2b64a},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,0},
        {4,{NativeRuntimeAddressSpace::linear,std::nullopt,0x2b64b},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,0},
        {5,{NativeRuntimeAddressSpace::linear,std::nullopt,0x2b64c},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,0},
        {6,{NativeRuntimeAddressSpace::linear,std::nullopt,0x2b62e},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,0x2bdcc},
        {7,{NativeRuntimeAddressSpace::linear,std::nullopt,0x2b632},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,0x2bdcc}}};
}

MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_d0_indexed_word(const MillenniumAtariD0IndexedWordObservation& o){
    if(checkpoint_.state!=MillenniumAtariConfigConsumerState::d0_indexed_word_boundary)return{false,"Not at D0-indexed word"};
    const auto disp=static_cast<std::int16_t>(o.d0&0xffffU); const auto expected=static_cast<std::uint32_t>(0x2bdfeLL+disp);
    if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.instruction_address!=0x2b5de||o.source_address!=expected||(checkpoint_.loop_iteration>0&&(o.d0&0xffffU)!=checkpoint_.loop_d0_value))return{false,"D0-indexed word mismatch"};
    checkpoint_.state=MillenniumAtariConfigConsumerState::a0_indexed_word_boundary;checkpoint_.last_sequence=o.sequence;checkpoint_.indexed_word_source_address=expected;checkpoint_.indexed_word_value=o.source_word;checkpoint_.a0_indexed_instruction_address=0x2b5ec;checkpoint_.local_instruction_count+=3;return{true,{}};
}
NativeRuntimeEffectBatch MillenniumAtariConfigConsumerSession::make_d0_indexed_word_effect_batch(std::string id)const{
    if(checkpoint_.state!=MillenniumAtariConfigConsumerState::a0_indexed_word_boundary||id.empty())throw std::runtime_error("Indexed word effect unavailable");
    const auto a1=checkpoint_.loop_iteration==0?0x2b61e:checkpoint_.loop_current_a1;
    return{std::move(id),true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,a1+6U},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,0xbdfc},{2,{NativeRuntimeAddressSpace::linear,std::nullopt,a1+10U},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,2}}};
}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_a0_indexed_word(const MillenniumAtariA0IndexedWordObservation&o){if(checkpoint_.state!=MillenniumAtariConfigConsumerState::a0_indexed_word_boundary)return{false,"Not at A0-indexed word"};const auto expected=static_cast<std::uint32_t>(static_cast<std::int64_t>(checkpoint_.callee_a3)+static_cast<std::int16_t>(checkpoint_.indexed_word_value));if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.instruction_address!=0x2b5ec||o.source_address!=expected)return{false,"A0-indexed word mismatch"};const auto prior=checkpoint_.loop_iteration==0?static_cast<std::uint16_t>(checkpoint_.setup_d7):checkpoint_.loop_d7_value;checkpoint_.state=MillenniumAtariConfigConsumerState::loop_branch_boundary;checkpoint_.last_sequence=o.sequence;checkpoint_.a0_indexed_source_address=expected;checkpoint_.a0_indexed_word_value=o.source_word;checkpoint_.loop_a0_value=0x56eee4;checkpoint_.loop_d0_value=static_cast<std::uint16_t>(o.source_word+2U);checkpoint_.loop_d7_value=static_cast<std::uint16_t>(prior-1U);checkpoint_.loop_branch_target=checkpoint_.loop_d7_value==0xffffU?0x2b600:0x2b5b8;checkpoint_.local_instruction_count+=6;return{true,{}};}
NativeRuntimeEffectBatch MillenniumAtariConfigConsumerSession::make_a0_indexed_tail_effect_batch(std::string id)const{if(checkpoint_.state!=MillenniumAtariConfigConsumerState::loop_branch_boundary||id.empty())throw std::runtime_error("Tail effect unavailable");return{std::move(id),true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,0x2b620},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,checkpoint_.loop_a0_value}}};}

MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::execute_loop_iteration_setup(){
if(checkpoint_.state!=MillenniumAtariConfigConsumerState::loop_branch_boundary||checkpoint_.loop_branch_target!=0x2b5b8||checkpoint_.loop_d7_value==0xffffU)return{false,"Loop iteration setup unavailable"};
checkpoint_.state=MillenniumAtariConfigConsumerState::d0_indexed_word_boundary;++checkpoint_.loop_iteration;checkpoint_.loop_current_a1=0x2b61eU + checkpoint_.loop_iteration*0x30U;checkpoint_.setup_a0_first=0x2bdcc;checkpoint_.setup_a0_second=0x2bdfc;checkpoint_.indexed_word_instruction_address=0x2b5de;checkpoint_.local_instruction_count+=10;return{true,{}};}
NativeRuntimeEffectBatch MillenniumAtariConfigConsumerSession::make_loop_iteration_setup_effect_batch(std::string id)const{
if(checkpoint_.state!=MillenniumAtariConfigConsumerState::d0_indexed_word_boundary||checkpoint_.loop_iteration<1||id.empty())throw std::runtime_error("Loop setup effect unavailable");
const auto a1=checkpoint_.loop_current_a1;
return{std::move(id),true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,a1+0x1b},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,1},{2,{NativeRuntimeAddressSpace::linear,std::nullopt,a1},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,0},{3,{NativeRuntimeAddressSpace::linear,std::nullopt,a1+0x2c},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,0},{4,{NativeRuntimeAddressSpace::linear,std::nullopt,a1+0x2d},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,0},{5,{NativeRuntimeAddressSpace::linear,std::nullopt,a1+0x2e},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,0},{6,{NativeRuntimeAddressSpace::linear,std::nullopt,a1+0x10},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,0x2bdcc},{7,{NativeRuntimeAddressSpace::linear,std::nullopt,a1+0x14},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,0x2bdcc}}};}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::execute_loop_epilogue(){if(checkpoint_.state!=MillenniumAtariConfigConsumerState::loop_branch_boundary||checkpoint_.loop_branch_target!=0x2b600||checkpoint_.loop_d7_value!=0xffffU)return{false,"Loop epilogue unavailable"};checkpoint_.state=MillenniumAtariConfigConsumerState::movem_restore_boundary;checkpoint_.movem_instruction_address=0x2b562;checkpoint_.movem_register_mask=0x7fff;checkpoint_.local_instruction_count+=7;return{true,{}};}
NativeRuntimeEffectBatch MillenniumAtariConfigConsumerSession::make_loop_epilogue_effect_batch(std::string id)const{if(checkpoint_.state!=MillenniumAtariConfigConsumerState::movem_restore_boundary||id.empty())throw std::runtime_error("Loop epilogue effect unavailable");return{std::move(id),true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,0x2b6b2},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,0},{2,{NativeRuntimeAddressSpace::linear,std::nullopt,0x2b6af},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,0x0f},{3,{NativeRuntimeAddressSpace::linear,std::nullopt,0x2b6b9},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,0},{4,{NativeRuntimeAddressSpace::linear,std::nullopt,0x2b6be},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,0xff},{5,{NativeRuntimeAddressSpace::linear,std::nullopt,0x2b6b7},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,0xff},{6,{NativeRuntimeAddressSpace::linear,std::nullopt,0x2b6ae},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,0xff}}};}

MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_movem_frame(const MillenniumAtariMovemFrameObservation&o){
if(checkpoint_.state!=MillenniumAtariConfigConsumerState::movem_restore_boundary)return{false,"Not at MOVEM restore"};
if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.instruction_address!=0x2b562||o.frame_address>0xffffffbfU||o.rts_return_address!=0x2aac8)return{false,"MOVEM frame mismatch"};
checkpoint_.state=MillenniumAtariConfigConsumerState::jsr_2aa68_boundary;checkpoint_.last_sequence=o.sequence;checkpoint_.movem_frame_observed=true;checkpoint_.movem_frame_address=o.frame_address;checkpoint_.restored_registers=o.registers;checkpoint_.restored_stack_address=o.frame_address+64U;checkpoint_.rts_return_address=o.rts_return_address;checkpoint_.next_jsr_address=0x2aac8;checkpoint_.next_jsr_target=0x2aa68;checkpoint_.local_instruction_count+=3;return{true,{}};}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::execute_jsr_2aa68(){if(checkpoint_.state!=MillenniumAtariConfigConsumerState::jsr_2aa68_boundary)return{false,"Not at JSR $2aa68"};checkpoint_.state=MillenniumAtariConfigConsumerState::xbios_selector_38_boundary;checkpoint_.selector_38_pointer_argument=0x2aa42;checkpoint_.xbios_trap_address=0x2aa72;checkpoint_.xbios_selector=0x26;checkpoint_.local_instruction_count+=3;return{true,{}};}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_xbios_selector_38(const MillenniumAtariXbiosSelector38Observation&o){if(checkpoint_.state!=MillenniumAtariConfigConsumerState::xbios_selector_38_boundary)return{false,"Not at XBIOS selector 38"};if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.trap_address!=0x2aa72||o.selector!=0x26)return{false,"Selector-38 observation mismatch"};checkpoint_.state=MillenniumAtariConfigConsumerState::jsr_2aa0c_boundary;checkpoint_.last_sequence=o.sequence;checkpoint_.selector_38_result_observed=true;checkpoint_.selector_38_result_d0=o.result_d0;checkpoint_.selector_38_stack_cleanup_bytes=6;checkpoint_.caller_d7=0x2a640;checkpoint_.next_jsr_address=0x2aad4;checkpoint_.next_jsr_target=0x2aa0c;checkpoint_.local_instruction_count+=4;return{true,{}};}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::execute_jsr_2aa0c(){if(checkpoint_.state!=MillenniumAtariConfigConsumerState::jsr_2aa0c_boundary)return{false,"Not at JSR $2aa0c"};checkpoint_.state=MillenniumAtariConfigConsumerState::gemdos_selector_61_boundary;checkpoint_.next_jsr_address=0x2aa0c;checkpoint_.next_jsr_target=0x2a5aa;checkpoint_.gemdos_trap_address=0x2a5b4;checkpoint_.gemdos_selector=0x3d;checkpoint_.gemdos_open_mode=2;checkpoint_.gemdos_filename_pointer=checkpoint_.caller_d7;checkpoint_.local_instruction_count+=5;return{true,{}};}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_gemdos_selector_61(const MillenniumAtariGemdosSelector61Observation&o){
if(checkpoint_.state!=MillenniumAtariConfigConsumerState::gemdos_selector_61_boundary)return{false,"Not at GEMDOS selector 61"};
if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.trap_address!=0x2a5b4||o.selector!=0x3d)return{false,"GEMDOS selector-61 observation mismatch"};
checkpoint_.last_sequence=o.sequence;checkpoint_.gemdos_61_result_observed=true;checkpoint_.gemdos_61_result_d0=o.result_d0;checkpoint_.gemdos_handle_store_address=0x2a5fa;checkpoint_.gemdos_stack_cleanup_bytes=6;checkpoint_.fopen_branch_address=0x2aa12;checkpoint_.gemdos_61_return_sha256="dfe4c3bc4466d6d8772f3633cb125f64ea7a9114d3d0be45aca5be3daf28b30b";checkpoint_.fopen_caller_branch_sha256="55dcd9fa27242e6bf6bc6f1f019a8fc086215fd0629c33a88a0a6f7e623517dc";
if(o.result_d0<0){checkpoint_.state=MillenniumAtariConfigConsumerState::fopen_failure_spin;checkpoint_.fopen_branch_target=0x2a632;checkpoint_.fopen_caller_branch_sha256="3a06cb0af877cc363d5ad25b670d680c77b4abcd00955b260c2139270b57426c";checkpoint_.local_instruction_count+=7;}else{checkpoint_.state=MillenniumAtariConfigConsumerState::jsr_2a5c2_boundary;checkpoint_.fopen_branch_target=0x2aa1c;checkpoint_.fopen_positive_d0=0x7d42;checkpoint_.fopen_positive_d1=0x2c24a;checkpoint_.next_jsr_address=0x2aa28;checkpoint_.next_jsr_target=0x2a5c2;checkpoint_.fopen_caller_branch_sha256="e3c9dfa674089f687e0042be07645d2d57bf321a76d53b0276f86ba8316f06f4";checkpoint_.local_instruction_count+=7;}return{true,{}};}
NativeRuntimeEffectBatch MillenniumAtariConfigConsumerSession::make_gemdos_selector_61_effect_batch(std::string id)const{if((checkpoint_.state!=MillenniumAtariConfigConsumerState::jsr_2a5c2_boundary&&checkpoint_.state!=MillenniumAtariConfigConsumerState::fopen_failure_spin)||id.empty())throw std::runtime_error("GEMDOS selector-61 effect unavailable");return{std::move(id),true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,0x2a5fa},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,static_cast<std::uint16_t>(checkpoint_.gemdos_61_result_d0)}}};}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::execute_jsr_2a5c2(){if(checkpoint_.state!=MillenniumAtariConfigConsumerState::jsr_2a5c2_boundary)return{false,"Not at JSR $2a5c2"};checkpoint_.state=MillenniumAtariConfigConsumerState::gemdos_selector_63_boundary;checkpoint_.gemdos_63_trap_address=0x2a5d0;checkpoint_.gemdos_63_selector=0x3f;checkpoint_.gemdos_63_handle=static_cast<std::uint16_t>(checkpoint_.gemdos_61_result_d0);checkpoint_.gemdos_63_buffer=checkpoint_.fopen_positive_d1;checkpoint_.gemdos_63_count=checkpoint_.fopen_positive_d0;checkpoint_.gemdos_63_prefix_sha256="6d2ddd7da4866769c78162433427fb37fe2f885926f429c098fca3062e282921";checkpoint_.local_instruction_count+=5;return{true,{}};}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_gemdos_selector_63(const MillenniumAtariGemdosSelector63Observation&o){if(checkpoint_.state!=MillenniumAtariConfigConsumerState::gemdos_selector_63_boundary)return{false,"Not at GEMDOS selector 63"};if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.trap_address!=0x2a5d0||o.selector!=0x3f)return{false,"GEMDOS selector-63 observation mismatch"};checkpoint_.state=MillenniumAtariConfigConsumerState::gemdos_selector_62_boundary;checkpoint_.last_sequence=o.sequence;checkpoint_.gemdos_63_result_observed=true;checkpoint_.gemdos_63_result_d0=o.result_d0;checkpoint_.gemdos_63_stack_cleanup_bytes=12;checkpoint_.gemdos_63_return_sha256="9f590fdbc6197d898da37312cddcb27a0411bf687877778f77320cb5c61f8ed3";checkpoint_.fread_caller_jump_sha256="a3bf89946746662879548e7a74f8f77c8d107c234cae2908c9b94abe94b19f89";checkpoint_.gemdos_62_trap_address=0x2a5e6;checkpoint_.gemdos_62_selector=0x3e;checkpoint_.gemdos_62_handle=checkpoint_.gemdos_63_handle;checkpoint_.gemdos_62_prefix_sha256="e815352850ca1cb7dffb7fa6d7e46d7775e82146695009e790e525daac17a2e9";checkpoint_.local_instruction_count+=7;return{true,{}};}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_gemdos_selector_62(const MillenniumAtariGemdosSelector62Observation&o){if(checkpoint_.state!=MillenniumAtariConfigConsumerState::gemdos_selector_62_boundary)return{false,"Not at GEMDOS selector 62"};if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.trap_address!=0x2a5e6||o.selector!=0x3e)return{false,"GEMDOS selector-62 observation mismatch"};checkpoint_.state=MillenniumAtariConfigConsumerState::fread_prefix_boundary;checkpoint_.last_sequence=o.sequence;checkpoint_.gemdos_62_result_observed=true;checkpoint_.gemdos_62_result_d0=o.result_d0;checkpoint_.gemdos_62_stack_cleanup_bytes=4;checkpoint_.gemdos_62_return_sha256="1653b046f59ffdf7cdcdae81914ab08b45f9fd09915e21b1c27ea8c6021e0b2f";checkpoint_.fread_prefix_a4=0x2c24c;checkpoint_.local_instruction_count+=5;return{true,{}};}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_fread_prefix(const MillenniumAtariFreadPrefixObservation&o){if(checkpoint_.state!=MillenniumAtariConfigConsumerState::fread_prefix_boundary)return{false,"Not at Fread prefix"};if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.first_address!=0x2c24c||o.second_address!=0x2c24e||checkpoint_.gemdos_63_result_d0<4)return{false,"Fread prefix observation mismatch"};checkpoint_.state=MillenniumAtariConfigConsumerState::jsr_2b2be_boundary;checkpoint_.last_sequence=o.sequence;checkpoint_.fread_prefix_observed=true;checkpoint_.fread_prefix_d6=o.first_word;checkpoint_.fread_prefix_d7=o.second_word;checkpoint_.caller_a5=checkpoint_.selector_three_result_d0;checkpoint_.next_jsr_address=0x2aaec;checkpoint_.next_jsr_target=0x2b2be;checkpoint_.fread_caller_prefix_sha256="06aca8d014e4064f17c8dba3c9b19ed705214dcb63cc41b0b3d9f8da7a2cd782";checkpoint_.local_instruction_count+=4;return{true,{}};}
NativeRuntimeEffectBatch MillenniumAtariConfigConsumerSession::make_fread_prefix_effect_batch(std::string id)const{if(checkpoint_.state!=MillenniumAtariConfigConsumerState::jsr_2b2be_boundary||id.empty())throw std::runtime_error("Fread prefix effect unavailable");return{std::move(id),true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,0x2c24c},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,checkpoint_.fread_prefix_d6},{2,{NativeRuntimeAddressSpace::linear,std::nullopt,0x2c24e},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,checkpoint_.fread_prefix_d7}}};}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::execute_jsr_2b2be(){if(checkpoint_.state!=MillenniumAtariConfigConsumerState::jsr_2b2be_boundary)return{false,"Not at JSR $2b2be"};checkpoint_.state=MillenniumAtariConfigConsumerState::game_init_source_byte_boundary;checkpoint_.game_init_a3=0x2b2ba;checkpoint_.game_init_d7=static_cast<std::uint16_t>(checkpoint_.fread_prefix_d7&0x00ffU);const auto decremented=static_cast<std::uint16_t>(checkpoint_.fread_prefix_d6-1U);checkpoint_.game_init_d6=static_cast<std::uint16_t>((decremented>>2U)+1U);checkpoint_.game_init_a6=checkpoint_.caller_a5;checkpoint_.game_init_a0=checkpoint_.caller_a5;checkpoint_.game_init_d5=4;checkpoint_.game_init_d2=0;checkpoint_.game_init_source_address=0x2c250;checkpoint_.game_init_dispatch_sha256="73fd6f3a91efb666e74c8022cd546f20457e599c36268691b0a27f43b22dc2bd";checkpoint_.local_instruction_count+=11;return{true,{}};}
NativeRuntimeEffectBatch MillenniumAtariConfigConsumerSession::make_game_init_setup_effect_batch(std::string id)const{if(checkpoint_.state!=MillenniumAtariConfigConsumerState::game_init_source_byte_boundary||id.empty())throw std::runtime_error("Game-init setup effect unavailable");return{std::move(id),true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,0x2b2ba},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,checkpoint_.game_init_d6},{2,{NativeRuntimeAddressSpace::linear,std::nullopt,0x2b2bc},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,checkpoint_.game_init_d7}}};}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_game_init_source_byte(const MillenniumAtariGameInitSourceByteObservation&o){if(checkpoint_.state!=MillenniumAtariConfigConsumerState::game_init_source_byte_boundary)return{false,"Not at game-init source byte"};if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.instruction_address!=0x2b2de||o.source_address!=checkpoint_.game_init_source_address)return{false,"Game-init source-byte observation mismatch"};checkpoint_.last_sequence=o.sequence;checkpoint_.game_init_source_observed=true;checkpoint_.game_init_source_byte=o.source_byte;checkpoint_.game_init_d2=o.source_byte;checkpoint_.game_init_source_address=o.source_address+1U;checkpoint_.game_init_source_dispatch_sha256="948e269d0e24d6ec05013d07ffe3d3ba66400189b98a30d676b44e5b39683fe6";const auto masked=static_cast<std::uint8_t>(o.source_byte&0xc0U);if(masked==0){checkpoint_.state=MillenniumAtariConfigConsumerState::game_init_zero_copy_boundary;checkpoint_.game_init_next_instruction=0x2b2ea;checkpoint_.local_instruction_count+=4;}else{checkpoint_.game_init_nonzero_dispatch_sha256="4b98ca43cbf9af758b5d56087a8d113f23fedf107e1320a2a6ee137d6cfe92c3";if((o.source_byte&0x40U)==0){checkpoint_.state=MillenniumAtariConfigConsumerState::game_init_bit6_clear_boundary;checkpoint_.game_init_next_instruction=0x2b3b8;checkpoint_.local_instruction_count+=6;}else if((o.source_byte&0x80U)!=0){checkpoint_.state=MillenniumAtariConfigConsumerState::game_init_bit7_set_boundary;checkpoint_.game_init_next_instruction=0x2b376;checkpoint_.local_instruction_count+=7;}else{checkpoint_.state=MillenniumAtariConfigConsumerState::game_init_second_source_boundary;checkpoint_.game_init_d2=static_cast<std::uint16_t>(o.source_byte&0x3fU);checkpoint_.game_init_next_instruction=0x2b338;checkpoint_.local_instruction_count+=9;}}return{true,{}};}
NativeRuntimeEffectBatch MillenniumAtariConfigConsumerSession::make_game_init_source_byte_effect_batch(std::string id)const{if(!checkpoint_.game_init_source_observed||checkpoint_.game_init_source_address==0||id.empty())throw std::runtime_error("Game-init source-byte effect unavailable");return{std::move(id),true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,checkpoint_.game_init_source_address-1U},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,checkpoint_.game_init_source_byte}}};}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_game_init_zero_pair(const MillenniumAtariGameInitZeroPairObservation&o){
if(checkpoint_.state!=MillenniumAtariConfigConsumerState::game_init_zero_copy_boundary)return{false,"Not at game-init zero pair"};
if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.instruction_address!=0x2b2ea||o.first_source_address!=checkpoint_.game_init_source_address||o.second_source_address!=checkpoint_.game_init_source_address+1U)return{false,"Game-init zero-pair observation mismatch"};
checkpoint_.state=MillenniumAtariConfigConsumerState::game_init_zero_counter_branch_boundary;checkpoint_.last_sequence=o.sequence;checkpoint_.game_init_zero_pair_observed=true;checkpoint_.game_init_zero_first_byte=o.first_source_byte;checkpoint_.game_init_zero_second_byte=o.second_source_byte;checkpoint_.game_init_zero_destination_address=checkpoint_.caller_a5;checkpoint_.game_init_source_address+=2U;checkpoint_.caller_a5+=8U;checkpoint_.game_init_d6=static_cast<std::uint16_t>(checkpoint_.game_init_d6-1U);checkpoint_.game_init_next_instruction=0x2b2f2;checkpoint_.game_init_zero_pair_prefix_sha256="8b97786735b1f1be41f931a62098f2f1080b5067b2db2a9835125619ad3b7623";checkpoint_.local_instruction_count+=3;return{true,{}};}
NativeRuntimeEffectBatch MillenniumAtariConfigConsumerSession::make_game_init_zero_pair_effect_batch(std::string id)const{if(checkpoint_.state!=MillenniumAtariConfigConsumerState::game_init_zero_counter_branch_boundary||!checkpoint_.game_init_zero_pair_observed||id.empty())throw std::runtime_error("Game-init zero-pair effect unavailable");return{std::move(id),true,{{1,{NativeRuntimeAddressSpace::linear,std::nullopt,checkpoint_.game_init_source_address-2U},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,checkpoint_.game_init_zero_first_byte},{2,{NativeRuntimeAddressSpace::linear,std::nullopt,checkpoint_.game_init_source_address-1U},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,checkpoint_.game_init_zero_second_byte},{3,{NativeRuntimeAddressSpace::linear,std::nullopt,checkpoint_.game_init_zero_destination_address},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,checkpoint_.game_init_zero_first_byte},{4,{NativeRuntimeAddressSpace::linear,std::nullopt,checkpoint_.game_init_zero_destination_address+1U},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,checkpoint_.game_init_zero_second_byte}}};}

MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::execute_game_init_zero_counter_branch(){
    if(checkpoint_.state!=MillenniumAtariConfigConsumerState::game_init_zero_counter_branch_boundary)return{false,"Not at game-init zero counter branch"};
    checkpoint_.game_init_zero_counter_continuation_sha256="9b3476f5d2ecb028149eec6ee575cd79c7c9f94589a7e7398d794ecd176f04ef";
    ++checkpoint_.local_instruction_count;
    if(checkpoint_.game_init_d6==0){
        checkpoint_.game_init_d7=static_cast<std::uint16_t>(checkpoint_.game_init_d7-1U); checkpoint_.local_instruction_count+=2;
        if(checkpoint_.game_init_d7!=0){
            checkpoint_.game_init_d6=static_cast<std::uint16_t>((checkpoint_.fread_prefix_d6-1U)>>2U)+1U;
            checkpoint_.game_init_a6+=0xa0U; checkpoint_.caller_a5=checkpoint_.game_init_a6; checkpoint_.local_instruction_count+=3;
        }else{
            checkpoint_.game_init_d5=static_cast<std::uint16_t>(checkpoint_.game_init_d5-1U); checkpoint_.local_instruction_count+=2;
            if(checkpoint_.game_init_d5==0){checkpoint_.state=MillenniumAtariConfigConsumerState::game_init_complete;checkpoint_.game_init_next_instruction=0x2b3c6;checkpoint_.game_init_completed_planes=4;return{true,{}};}
            checkpoint_.game_init_a0+=2U;checkpoint_.game_init_a6=checkpoint_.game_init_a0;
            checkpoint_.game_init_d6=static_cast<std::uint16_t>((checkpoint_.fread_prefix_d6-1U)>>2U)+1U;
            checkpoint_.game_init_d7=static_cast<std::uint16_t>(checkpoint_.fread_prefix_d7&0x00ffU);
            checkpoint_.caller_a5=checkpoint_.game_init_a6; checkpoint_.game_init_completed_planes=4U-checkpoint_.game_init_d5; checkpoint_.local_instruction_count+=6;
        }
    }
    checkpoint_.game_init_d2=static_cast<std::uint16_t>((checkpoint_.game_init_d2&0xff00U)|static_cast<std::uint8_t>(checkpoint_.game_init_d2-1U));checkpoint_.local_instruction_count+=2;
    if(static_cast<std::uint8_t>(checkpoint_.game_init_d2)!=0){checkpoint_.state=MillenniumAtariConfigConsumerState::game_init_zero_copy_boundary;checkpoint_.game_init_next_instruction=0x2b2ea;}
    else{checkpoint_.state=MillenniumAtariConfigConsumerState::game_init_source_byte_boundary;checkpoint_.game_init_next_instruction=0x2b2de;++checkpoint_.local_instruction_count;}
    return{true,{}};
}

void MillenniumAtariConfigConsumerSession::execute_game_init_alternate_run(const std::uint16_t value,const std::uint32_t source_advance_after_run){
    checkpoint_.game_init_alternate_writes.clear();
    for(;;){
        checkpoint_.game_init_alternate_writes.push_back({checkpoint_.caller_a5,value});
        checkpoint_.caller_a5+=8U;
        checkpoint_.game_init_d6=static_cast<std::uint16_t>(checkpoint_.game_init_d6-1U);
        checkpoint_.local_instruction_count+=4;
        if(checkpoint_.game_init_d6==0){
            checkpoint_.game_init_d7=static_cast<std::uint16_t>(checkpoint_.game_init_d7-1U);checkpoint_.local_instruction_count+=2;
            if(checkpoint_.game_init_d7!=0){
                checkpoint_.game_init_d6=static_cast<std::uint16_t>(((checkpoint_.fread_prefix_d6-1U)>>2U)+1U);
                checkpoint_.game_init_a6+=0xa0U;checkpoint_.caller_a5=checkpoint_.game_init_a6;checkpoint_.local_instruction_count+=3;
            }else{
                checkpoint_.game_init_d5=static_cast<std::uint16_t>(checkpoint_.game_init_d5-1U);checkpoint_.local_instruction_count+=2;
                if(checkpoint_.game_init_d5==0){checkpoint_.state=MillenniumAtariConfigConsumerState::game_init_complete;checkpoint_.game_init_next_instruction=0x2b3c6;checkpoint_.game_init_completed_planes=4;return;}
                checkpoint_.game_init_a0+=2U;checkpoint_.game_init_a6=checkpoint_.game_init_a0;
                checkpoint_.game_init_d6=static_cast<std::uint16_t>(((checkpoint_.fread_prefix_d6-1U)>>2U)+1U);
                checkpoint_.game_init_d7=static_cast<std::uint16_t>(checkpoint_.fread_prefix_d7&0x00ffU);
                checkpoint_.caller_a5=checkpoint_.game_init_a6;checkpoint_.game_init_completed_planes=4U-checkpoint_.game_init_d5;checkpoint_.local_instruction_count+=6;
            }
        }
        checkpoint_.game_init_d2=static_cast<std::uint16_t>(checkpoint_.game_init_d2-1U);checkpoint_.local_instruction_count+=2;
        if(checkpoint_.game_init_d2!=0)continue;
        checkpoint_.game_init_source_address+=source_advance_after_run;
        checkpoint_.state=MillenniumAtariConfigConsumerState::game_init_source_byte_boundary;
        checkpoint_.game_init_next_instruction=0x2b2de;
        return;
    }
}

MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_game_init_replicated_byte(const MillenniumAtariGameInitReplicatedByteObservation&o){
    if(checkpoint_.state!=MillenniumAtariConfigConsumerState::game_init_second_source_boundary)return{false,"Not at replicated-byte source"};
    if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.instruction_address!=0x2b338||o.source_address!=checkpoint_.game_init_source_address)return{false,"Replicated-byte observation mismatch"};
    checkpoint_.last_sequence=o.sequence;checkpoint_.game_init_alternate_source_addresses={o.source_address};checkpoint_.game_init_alternate_source_bytes={o.source_byte};checkpoint_.game_init_source_address+=1U;checkpoint_.game_init_alternate_run_sha256="6429d7b0634cff176ec01486b3f4e05bd648e3de11a67edd151f8345724b6701";
    execute_game_init_alternate_run(static_cast<std::uint16_t>((static_cast<std::uint16_t>(o.source_byte)<<8U)|o.source_byte),0U);return{true,{}};
}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_game_init_swapped_pair(const MillenniumAtariGameInitSwappedPairObservation&o){
    if(checkpoint_.state!=MillenniumAtariConfigConsumerState::game_init_bit7_set_boundary)return{false,"Not at swapped-pair source"};
    if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.instruction_address!=0x2b37a||o.first_source_address!=checkpoint_.game_init_source_address||o.second_source_address!=o.first_source_address+1U)return{false,"Swapped-pair observation mismatch"};
    checkpoint_.last_sequence=o.sequence;checkpoint_.game_init_d2=static_cast<std::uint16_t>(checkpoint_.game_init_d2&0x3fU);checkpoint_.game_init_alternate_source_addresses={o.first_source_address,o.second_source_address};checkpoint_.game_init_alternate_source_bytes={o.first_source_byte,o.second_source_byte};checkpoint_.game_init_alternate_run_sha256="dbf80460ade3c9cc5fba8b4a62937920cc9e131052d3a48bfc8b0981e150a9b9";
    execute_game_init_alternate_run(static_cast<std::uint16_t>((static_cast<std::uint16_t>(o.second_source_byte)<<8U)|o.first_source_byte),2U);return{true,{}};
}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_game_init_extended_run(const MillenniumAtariGameInitExtendedRunObservation&o){
    if(checkpoint_.state!=MillenniumAtariConfigConsumerState::game_init_bit6_clear_boundary)return{false,"Not at extended-run source"};
    if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.instruction_address!=0x2b3c0||o.count_source_address!=checkpoint_.game_init_source_address||o.first_value_address!=o.count_source_address+1U||o.second_value_address!=o.count_source_address+2U)return{false,"Extended-run observation mismatch"};
    checkpoint_.last_sequence=o.sequence;checkpoint_.game_init_d2=static_cast<std::uint16_t>(((checkpoint_.game_init_d2&0x3fU)<<8U)|o.count_low_byte);checkpoint_.game_init_alternate_source_addresses={o.count_source_address,o.first_value_address,o.second_value_address};checkpoint_.game_init_alternate_source_bytes={o.count_low_byte,o.first_value_byte,o.second_value_byte};checkpoint_.game_init_source_address+=1U;checkpoint_.game_init_alternate_run_sha256="72fa63385edc5122cd3fe1c4031d0a0089a187d498c04ff1f8be912f4462b0c5+dbf80460ade3c9cc5fba8b4a62937920cc9e131052d3a48bfc8b0981e150a9b9";
    execute_game_init_alternate_run(static_cast<std::uint16_t>((static_cast<std::uint16_t>(o.second_value_byte)<<8U)|o.first_value_byte),2U);return{true,{}};
}
NativeRuntimeEffectBatch MillenniumAtariConfigConsumerSession::make_game_init_alternate_effect_batch(std::string id)const{
    if(id.empty()||checkpoint_.game_init_alternate_source_addresses.size()!=checkpoint_.game_init_alternate_source_bytes.size()||checkpoint_.game_init_alternate_source_addresses.empty())throw std::runtime_error("Game-init alternate effect unavailable");
    NativeRuntimeEffectBatch batch{std::move(id),true,{}};std::size_t order=1;
    for(std::size_t i=0;i<checkpoint_.game_init_alternate_source_addresses.size();++i)batch.effects.push_back({order++,{NativeRuntimeAddressSpace::linear,std::nullopt,checkpoint_.game_init_alternate_source_addresses[i]},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,checkpoint_.game_init_alternate_source_bytes[i]});
    for(const auto&w:checkpoint_.game_init_alternate_writes)batch.effects.push_back({order++,{NativeRuntimeAddressSpace::linear,std::nullopt,w.address},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,w.value});
    return batch;
}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::execute_game_init_return(){if(checkpoint_.state!=MillenniumAtariConfigConsumerState::game_init_complete)return{false,"Game-init return is unavailable"};checkpoint_.state=MillenniumAtariConfigConsumerState::game_init_jsr_2b448_boundary;checkpoint_.game_init_a3=0x2a64c;checkpoint_.game_init_a0=0x2a66c;checkpoint_.next_jsr_address=0x2aafe;checkpoint_.next_jsr_target=0x2b448;checkpoint_.game_init_caller_2b448_sha256="155575e295ad1e7831c0eef9809316db6f68321beb0661c03b7c14bb141f793e";checkpoint_.local_instruction_count+=4;return{true,{}};}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::execute_game_init_palette_copy_prefix(){if(checkpoint_.state!=MillenniumAtariConfigConsumerState::game_init_jsr_2b448_boundary)return{false,"Palette-copy prefix is unavailable"};checkpoint_.state=MillenniumAtariConfigConsumerState::game_init_palette_transform_boundary;checkpoint_.game_init_palette_clear_destination=checkpoint_.caller_a5;checkpoint_.game_init_palette_copy_destination=0x2b3c8;checkpoint_.caller_a5=0x2b428;checkpoint_.game_init_a0=0x2b3c8;checkpoint_.game_init_d7=6;checkpoint_.game_init_d6=15;checkpoint_.game_init_d5=2;checkpoint_.game_init_palette_copy_prefix_sha256="748d9b2df05839b68583069e29ff34954477ce7a367b0a88ef9e9bad7abfa0ca";checkpoint_.game_init_next_instruction=0x2b486;checkpoint_.local_instruction_count+=41;return{true,{}};}
NativeRuntimeEffectBatch MillenniumAtariConfigConsumerSession::make_game_init_palette_copy_effect_batch(std::string id)const{if(checkpoint_.state!=MillenniumAtariConfigConsumerState::game_init_palette_transform_boundary||id.empty())throw std::runtime_error("Palette-copy effects are unavailable");NativeRuntimeEffectBatch batch{std::move(id),true,{}};std::size_t order=1;for(std::size_t i=0;i<8;++i)batch.effects.push_back({order++,{NativeRuntimeAddressSpace::linear,std::nullopt,checkpoint_.game_init_palette_clear_destination+static_cast<std::uint32_t>(i*4U)},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,0});for(std::size_t i=0;i<checkpoint_.game_init_palette_source_longs.size();++i)batch.effects.push_back({order++,{NativeRuntimeAddressSpace::linear,std::nullopt,checkpoint_.game_init_palette_copy_destination+static_cast<std::uint32_t>(i*4U)},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,checkpoint_.game_init_palette_source_longs[i]});return batch;}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_game_init_palette_words(const MillenniumAtariGameInitPaletteWordsObservation&o){if(checkpoint_.state!=MillenniumAtariConfigConsumerState::game_init_palette_transform_boundary)return{false,"Palette arithmetic is unavailable"};if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.instruction_address!=0x2b486||o.source_address!=0x2b3c8||o.destination_address!=0x2b428)return{false,"Palette-word observation mismatch"};checkpoint_.last_sequence=o.sequence;std::array<std::uint8_t,96> source{};for(std::size_t i=0;i<checkpoint_.game_init_palette_source_longs.size();++i){const auto value=checkpoint_.game_init_palette_source_longs[i];source[i*4U]=static_cast<std::uint8_t>(value>>24U);source[i*4U+1U]=static_cast<std::uint8_t>(value>>16U);source[i*4U+2U]=static_cast<std::uint8_t>(value>>8U);source[i*4U+3U]=static_cast<std::uint8_t>(value);}checkpoint_.game_init_palette_result_words=o.destination_words;std::size_t carry_count=0;for(std::size_t group=0;group<16;++group){std::uint16_t weight=0x0100;for(std::size_t lane=0;lane<3;++lane){const auto source_index=group*6U+lane*2U;const std::uint16_t sum=static_cast<std::uint16_t>(static_cast<std::uint16_t>(source[source_index])+source[source_index+1U]);if(sum>0xffU){checkpoint_.game_init_palette_result_words[group]=static_cast<std::uint16_t>(checkpoint_.game_init_palette_result_words[group]+weight);++carry_count;}checkpoint_.game_init_palette_result_bytes[group*3U+lane]=static_cast<std::uint8_t>(sum);weight=static_cast<std::uint16_t>(weight>>4U);}}checkpoint_.state=MillenniumAtariConfigConsumerState::game_init_palette_xbios_selector_6_boundary;checkpoint_.game_init_palette_arithmetic_sha256="0866601f1a271ee74b399dd544b5b1ced15693e600c30034531a094dbc41d746";checkpoint_.game_init_palette_xbios_trap_address=0x2b4ac;checkpoint_.game_init_palette_xbios_selector=6;checkpoint_.game_init_palette_xbios_pointer=0x2b428;checkpoint_.game_init_palette_completed_passes=1;checkpoint_.game_init_next_instruction=0x2b4ac;checkpoint_.local_instruction_count+=419U+carry_count;return{true,{}};}
NativeRuntimeEffectBatch MillenniumAtariConfigConsumerSession::make_game_init_palette_arithmetic_effect_batch(std::string id)const{if(checkpoint_.state!=MillenniumAtariConfigConsumerState::game_init_palette_xbios_selector_6_boundary||id.empty())throw std::runtime_error("Palette arithmetic effects are unavailable");NativeRuntimeEffectBatch batch{std::move(id),true,{}};std::size_t order=1;for(std::size_t group=0;group<16;++group)for(std::size_t lane=0;lane<3;++lane)batch.effects.push_back({order++,{NativeRuntimeAddressSpace::linear,std::nullopt,0x2b3c8U+static_cast<std::uint32_t>(group*6U+lane*2U)},MemoryTransferElementWidth::byte,NativeRuntimeByteOrder::big_endian,checkpoint_.game_init_palette_result_bytes[group*3U+lane]});for(std::size_t i=0;i<checkpoint_.game_init_palette_result_words.size();++i)batch.effects.push_back({order++,{NativeRuntimeAddressSpace::linear,std::nullopt,0x2b428U+static_cast<std::uint32_t>(i*2U)},MemoryTransferElementWidth::word,NativeRuntimeByteOrder::big_endian,checkpoint_.game_init_palette_result_words[i]});return batch;}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_game_init_palette_xbios_selector_6(const MillenniumAtariGameInitPaletteXbios6Observation&o){const auto recurrent=checkpoint_.state==MillenniumAtariConfigConsumerState::game_init_palette_xbios_selector_6_boundary;const auto terminal=checkpoint_.state==MillenniumAtariConfigConsumerState::game_init_palette_terminal_xbios_selector_6_boundary;if(!recurrent&&!terminal)return{false,"Palette XBIOS selector-6 result is unavailable"};const auto expected_trap=terminal?0x2b4c2U:0x2b4acU;if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.trap_address!=expected_trap||o.selector!=6)return{false,"Palette XBIOS selector-6 observation mismatch"};auto next=checkpoint_;next.last_sequence=o.sequence;if(terminal){next.state=MillenniumAtariConfigConsumerState::game_init_palette_rts_boundary;next.game_init_palette_terminal_result_d0=o.result_d0;next.game_init_palette_xbios_stack_cleanup_bytes=6;next.game_init_palette_terminal_sha256="876ea72e7f61e2604ffa34d0fae7a6c1b3f880aa43e88006af18e1f67677c967";next.game_init_palette_rts_address=0x2b4c6;next.game_init_next_instruction=0x2b4c6;next.local_instruction_count+=2;checkpoint_=std::move(next);return{true,{}};}next.game_init_palette_xbios_result_observed=true;next.game_init_palette_xbios_result_d0=o.result_d0;next.game_init_palette_xbios_stack_cleanup_bytes=6;next.game_init_palette_post_xbios_sha256="9e3fd4aeca606c5560b204d12a20a77de12552ded7fa64a0677cca56c4676bf1";next.game_init_palette_delay_initial_d0=0x4e20;next.game_init_palette_delay_iterations=0x4e20;next.game_init_palette_delay_final_d0=0;next.game_init_palette_outer_backedge_address=0x2b46e;next.local_instruction_count+=40003U;if(next.game_init_d7==0){next.game_init_d7=0xffff;next.state=MillenniumAtariConfigConsumerState::game_init_palette_terminal_xbios_selector_6_boundary;next.game_init_palette_terminal_trap_address=0x2b4c2;next.game_init_next_instruction=0x2b4c2;next.local_instruction_count+=2;}else{next.game_init_d7=static_cast<std::uint16_t>(next.game_init_d7-1U);next.state=MillenniumAtariConfigConsumerState::game_init_palette_outer_recurrence_boundary;next.game_init_next_instruction=0x2b46e;}checkpoint_=std::move(next);return{true,{}};}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_game_init_palette_recurrence(const MillenniumAtariGameInitPaletteRecurrenceObservation&o){if(checkpoint_.state!=MillenniumAtariConfigConsumerState::game_init_palette_outer_recurrence_boundary)return{false,"Recurrent palette pass is unavailable"};if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.instruction_address!=0x2b46e||o.source_address!=0x2b3c8||o.destination_address!=0x2b428)return{false,"Recurrent palette observation mismatch"};auto next=checkpoint_;next.last_sequence=o.sequence;next.game_init_palette_result_words=o.destination_words;std::size_t carry_count=0;for(std::size_t group=0;group<16;++group){std::uint16_t weight=0x0100;for(std::size_t lane=0;lane<3;++lane){const auto source_index=group*6U+lane*2U;const auto sum=static_cast<std::uint16_t>(static_cast<std::uint16_t>(o.source_bytes[source_index])+o.source_bytes[source_index+1U]);if(sum>0xffU){next.game_init_palette_result_words[group]=static_cast<std::uint16_t>(next.game_init_palette_result_words[group]+weight);++carry_count;}next.game_init_palette_result_bytes[group*3U+lane]=static_cast<std::uint8_t>(sum);weight=static_cast<std::uint16_t>(weight>>4U);}}next.state=MillenniumAtariConfigConsumerState::game_init_palette_xbios_selector_6_boundary;next.game_init_palette_recurrence_sha256="a50d1864336da9b76c9594f94b2eb736108d738d0aefc6443a91c8e8fdd7088b";++next.game_init_palette_completed_passes;next.game_init_next_instruction=0x2b4ac;next.local_instruction_count+=424U+carry_count;checkpoint_=std::move(next);return{true,{}};}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_game_init_palette_rts(const MillenniumAtariGameInitPaletteRtsObservation&o){if(checkpoint_.state!=MillenniumAtariConfigConsumerState::game_init_palette_rts_boundary)return{false,"Palette RTS destination is unavailable"};if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.instruction_address!=0x2b4c6||o.stack_address>0xfffffffbU||o.return_address!=0x2ab04)return{false,"Palette RTS observation mismatch"};auto next=checkpoint_;next.state=MillenniumAtariConfigConsumerState::jsr_2aa0c_boundary;next.last_sequence=o.sequence;next.game_init_palette_rts_stack_address=o.stack_address;next.game_init_palette_rts_return_address=o.return_address;next.game_init_palette_caller_continuation_sha256="ae672762da7616abc67d0a1e5a5aaf3ab540b96b94b9689b31f8a11a8de256d7";next.game_init_second_config_open=true;next.caller_d7=0x2a634;next.next_jsr_address=0x2ab0a;next.next_jsr_target=0x2aa0c;next.local_instruction_count+=3;checkpoint_=std::move(next);return{true,{}};}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_game_init_second_config_fopen(const MillenniumAtariGemdosSelector61Observation&o){if(!checkpoint_.game_init_second_config_open||checkpoint_.state!=MillenniumAtariConfigConsumerState::gemdos_selector_61_boundary||checkpoint_.gemdos_filename_pointer!=0x2a634)return{false,"Second config Fopen result is unavailable"};return observe_gemdos_selector_61(o);}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_game_init_second_config_fread(const MillenniumAtariGemdosSelector63Observation&o){if(!checkpoint_.game_init_second_config_open||checkpoint_.state!=MillenniumAtariConfigConsumerState::gemdos_selector_63_boundary||checkpoint_.gemdos_63_buffer!=0x2c24a||checkpoint_.gemdos_63_count!=0x7d42)return{false,"Second config Fread result is unavailable"};return observe_gemdos_selector_63(o);}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_game_init_second_config_fclose(const MillenniumAtariGemdosSelector62Observation&o){if(!checkpoint_.game_init_second_config_open||checkpoint_.state!=MillenniumAtariConfigConsumerState::gemdos_selector_62_boundary)return{false,"Second config Fclose result is unavailable"};if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.trap_address!=0x2a5e6||o.selector!=0x3e)return{false,"Second config Fclose observation mismatch"};auto next=checkpoint_;next.state=MillenniumAtariConfigConsumerState::game_init_second_config_rts_boundary;next.last_sequence=o.sequence;next.gemdos_62_result_observed=true;next.gemdos_62_result_d0=o.result_d0;next.gemdos_62_stack_cleanup_bytes=4;next.gemdos_62_return_sha256="1653b046f59ffdf7cdcdae81914ab08b45f9fd09915e21b1c27ea8c6021e0b2f";next.game_init_second_config_rts_address=0x2a5ec;next.local_instruction_count+=3;checkpoint_=std::move(next);return{true,{}};}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_game_init_second_config_rts(const MillenniumAtariGameInitSecondConfigRtsObservation&o){if(checkpoint_.state!=MillenniumAtariConfigConsumerState::game_init_second_config_rts_boundary)return{false,"Second config RTS destination is unavailable"};if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.instruction_address!=0x2a5ec||o.stack_address>0xfffffffbU||o.return_address!=0x2ab10)return{false,"Second config RTS observation mismatch"};auto next=checkpoint_;next.state=MillenniumAtariConfigConsumerState::game_init_post_second_config_xbios_38_boundary;next.last_sequence=o.sequence;next.game_init_second_config_rts_stack_address=o.stack_address;next.game_init_second_config_rts_return_address=o.return_address;next.game_init_second_config_caller_sha256="eea2683953b1fe18e3e7b88e1744fa10a9684444fe183d283efee9f54302c1a0";next.game_init_a3=0x2a64c;next.game_init_a0=0x2a66c;next.game_init_second_config_xbios_pointer=0x2ab2c;next.xbios_trap_address=0x2ab24;next.xbios_selector=0x26;next.local_instruction_count+=6;checkpoint_=std::move(next);return{true,{}};}
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_game_init_second_config_xbios_38(const MillenniumAtariXbiosSelector38Observation&o){if(checkpoint_.state!=MillenniumAtariConfigConsumerState::game_init_post_second_config_xbios_38_boundary)return{false,"Second config XBIOS selector-38 result is unavailable"};if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.trap_address!=0x2ab24||o.selector!=0x26)return{false,"Second config XBIOS selector-38 observation mismatch"};auto next=checkpoint_;next.state=MillenniumAtariConfigConsumerState::game_init_config_final_rts_boundary;next.last_sequence=o.sequence;next.game_init_second_config_xbios_result_d0=o.result_d0;next.game_init_second_config_xbios_cleanup_bytes=6;next.game_init_second_config_xbios_return_sha256="2b1d33a613d225ccb932ee7c7ad5efb29dcdd736ba28ad3c4b75162694bc09ed";next.game_init_config_final_rts_address=0x2ab28;next.local_instruction_count+=2;checkpoint_=std::move(next);return{true,{}};}
// $77042 belongs to the separately hash-admitted staged PRG profile, not the
// resident $2a500 image supplied to this session's constructor.  Validate its
// typed entry/return here without demanding those nonresident bytes at launch.
MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_game_init_config_final_rts(const MillenniumAtariGameInitSecondConfigRtsObservation&o){if(checkpoint_.state!=MillenniumAtariConfigConsumerState::game_init_config_final_rts_boundary)return{false,"Final config RTS destination is unavailable"};if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.instruction_address!=0x2ab28||o.stack_address>0xfffffffbU||o.return_address!=0x77042)return{false,"Final config RTS observation mismatch"};auto next=checkpoint_;next.state=MillenniumAtariConfigConsumerState::game_init_post_config_gemdos_61_boundary;next.last_sequence=o.sequence;next.game_init_config_final_rts_stack_address=o.stack_address;next.game_init_config_final_rts_return_address=o.return_address;next.game_init_post_config_caller_sha256="dc2a50400e22fdbe4870f790d4f70c7446caa379dc68281a0445db4ee027fe4d";next.game_init_post_config_preserved_stack_long=0x11e00;next.gemdos_trap_address=0x77056;next.gemdos_selector=0x3d;next.gemdos_open_mode=2;next.gemdos_filename_pointer=0x1d6d8;next.local_instruction_count+=6;checkpoint_=std::move(next);return{true,{}};}

MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_game_init_post_config_fopen(const MillenniumAtariGemdosSelector61Observation&o){if(checkpoint_.state!=MillenniumAtariConfigConsumerState::game_init_post_config_gemdos_61_boundary)return{false,"Post-config Fopen result is unavailable"};if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.trap_address!=0x77056||o.selector!=0x3d)return{false,"Post-config Fopen observation mismatch"};auto next=checkpoint_;next.last_sequence=o.sequence;next.game_init_post_config_fopen_result_d0=o.result_d0;next.game_init_post_config_handle_word=static_cast<std::uint16_t>(o.result_d0);if(o.result_d0<0){next.state=MillenniumAtariConfigConsumerState::game_init_post_config_fopen_failure_spin;next.game_init_post_config_fopen_branch_sha256="d124b586e52a783689925186d8cc93366870526fd894567b7c55761a617807c7";next.game_init_post_config_failure_spin_address=0x77060;next.local_instruction_count+=4;}else{next.state=MillenniumAtariConfigConsumerState::game_init_post_config_gemdos_63_boundary;next.game_init_post_config_fopen_branch_sha256="2ceb9e3c6a8c2882f13708d64367b0a9f8bf18ee7456ea396a3e600734825476";next.gemdos_trap_address=0x77074;next.gemdos_selector=0x3f;next.game_init_post_config_fread_buffer=0x11e00;next.game_init_post_config_fread_count=0x20000;next.local_instruction_count+=9;}checkpoint_=std::move(next);return{true,{}};}

MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_game_init_post_config_fread(const MillenniumAtariGemdosSelector63Observation&o){if(checkpoint_.state!=MillenniumAtariConfigConsumerState::game_init_post_config_gemdos_63_boundary)return{false,"Post-config Fread result is unavailable"};if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.trap_address!=0x77074||o.selector!=0x3f)return{false,"Post-config Fread observation mismatch"};auto next=checkpoint_;next.state=MillenniumAtariConfigConsumerState::game_init_post_config_gemdos_62_boundary;next.last_sequence=o.sequence;next.game_init_post_config_fread_result_d0=o.result_d0;next.game_init_post_config_fread_cleanup_bytes=12;next.game_init_post_config_fread_return_sha256="368338a18784d37b5867fa551121703b2fb0ab613db51cbc5b2c08e14f474558";next.gemdos_trap_address=0x7707c;next.gemdos_selector=0x3e;next.local_instruction_count+=2;checkpoint_=std::move(next);return{true,{}};}

MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_game_init_post_config_fclose(const MillenniumAtariGemdosSelector62Observation&o){if(checkpoint_.state!=MillenniumAtariConfigConsumerState::game_init_post_config_gemdos_62_boundary)return{false,"Post-config Fclose result is unavailable"};if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.trap_address!=0x7707c||o.selector!=0x3e)return{false,"Post-config Fclose observation mismatch"};auto next=checkpoint_;next.state=MillenniumAtariConfigConsumerState::game_init_post_config_rts_boundary;next.last_sequence=o.sequence;next.game_init_post_config_fclose_result_d0=o.result_d0;next.game_init_post_config_fclose_cleanup_bytes=12;next.game_init_post_config_fclose_return_sha256="aa177208872c4125af13601feb4566003e5fb01c851c44f8b7f4904fb5f52b52";next.game_init_post_config_write_addresses={0x2ab2c,0x11dfc};next.game_init_post_config_write_value=0x361436a7;next.game_init_post_config_rts_address=0x770ba;next.local_instruction_count+=5;checkpoint_=std::move(next);return{true,{}};}

NativeRuntimeEffectBatch MillenniumAtariConfigConsumerSession::make_game_init_post_config_effect_batch(std::string id)const{if(checkpoint_.state!=MillenniumAtariConfigConsumerState::game_init_post_config_rts_boundary||id.empty())throw std::runtime_error("Post-config effects are unavailable");NativeRuntimeEffectBatch batch{std::move(id),true,{}};for(std::size_t i=0;i<checkpoint_.game_init_post_config_write_addresses.size();++i)batch.effects.push_back({i+1,{NativeRuntimeAddressSpace::linear,std::nullopt,checkpoint_.game_init_post_config_write_addresses[i]},MemoryTransferElementWidth::longword,NativeRuntimeByteOrder::big_endian,checkpoint_.game_init_post_config_write_value});return batch;}

MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::observe_game_init_post_config_rts(const MillenniumAtariGameInitSecondConfigRtsObservation&o){if(checkpoint_.state!=MillenniumAtariConfigConsumerState::game_init_post_config_rts_boundary)return{false,"Post-config RTS destination is unavailable"};if(o.generation!=checkpoint_.generation||o.sequence<=checkpoint_.last_sequence||o.instruction_address!=0x770ba||o.stack_address>0xfffffffbU||o.return_address>0xffffffU||(o.return_address&1U)!=0U)return{false,"Post-config RTS observation mismatch"};auto next=checkpoint_;next.state=MillenniumAtariConfigConsumerState::game_init_post_config_complete;next.last_sequence=o.sequence;next.game_init_post_config_rts_stack_address=o.stack_address;next.game_init_post_config_rts_return_address=o.return_address;next.local_instruction_count+=1;checkpoint_=std::move(next);return{true,{}};}

MillenniumAtariConfigConsumerResult MillenniumAtariConfigConsumerSession::revoke(
    const std::uint64_t generation) {
    if (checkpoint_.state == MillenniumAtariConfigConsumerState::revoked) {
        return {false, "Millennium Atari config consumer generation is already revoked"};
    }
    if (generation == 0 || generation != checkpoint_.generation) {
        return {false, "Millennium Atari config consumer revocation generation is stale"};
    }
    checkpoint_.state = MillenniumAtariConfigConsumerState::revoked;
    return {true, {}};
}

} // namespace eon
