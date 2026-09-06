#include "data/millennium_dos_title_flow.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace eon {
namespace {

template <std::size_t Size>
struct ExecutableByteAnchor {
    std::string_view sha256;
    static constexpr std::size_t size() { return Size; }
    bool matches(const std::span<const std::uint8_t> bytes) const {
        return bytes.size() == Size && to_hex(eon::sha256(bytes)) == sha256;
    }
};

template <std::size_t Size>
bool has_bytes(std::span<const std::uint8_t> bytes, std::size_t offset,
               const ExecutableByteAnchor<Size>& expected) {
    return offset <= bytes.size() && Size <= bytes.size() - offset
        && expected.matches(bytes.subspan(offset, Size));
}

template <std::size_t Size>
std::size_t require_unique(std::span<const std::uint8_t> bytes,
                           const ExecutableByteAnchor<Size>& expected,
                           const char* description) {
    std::size_t result = bytes.size();
    for (std::size_t offset = 0; offset + Size <= bytes.size(); ++offset) {
        if (!has_bytes(bytes, offset, expected)) continue;
        if (result != bytes.size()) {
            throw std::runtime_error(std::string("Ambiguous Millennium DOS ") + description);
        }
        result = offset;
    }
    if (result == bytes.size()) {
        throw std::runtime_error(std::string("Missing Millennium DOS ") + description);
    }
    return result;
}

} // namespace

MillenniumDosSpanishTitleBoundary parse_millennium_dos_spanish_title_boundary(
    const std::span<const std::uint8_t> titles_executable) {
    constexpr auto spanish_sha256 =
        "02082c35e18cee330f7d1b88098f502e68011f7e47a3a649961f6f03d1d14fe7";
    constexpr ExecutableByteAnchor<7> entry{"f68952a9bbb82fa876f35aa293b010e2fb0be9f2814c77d2f8604391716ccd07"};
    constexpr ExecutableByteAnchor<13> wrapper{"5d17daad68e9062dc6852ae76740db4afdcb81555ba9fb7d15d4e4aa8d088175"};
    constexpr ExecutableByteAnchor<16> post_title_loop{"5eebd21fcd4da98b08e7b29a27c3ee7b88405949ff3c1838b31aecc3ebf596fb"};
    constexpr ExecutableByteAnchor<7> input_poll{"96715350de2c4a159dd994b1c40dafb21c038f38f52937aa1a54491485146dc6"};
    // The local exit begins at loaded $1c54.  Its preceding condition is the
    // original `AND AL,AL` / `JNZ`, so this anchor establishes the title's
    // availability-only hand-off without interpreting a DOS character.
    constexpr ExecutableByteAnchor<8> input_nonzero_exit{"6802b4587f2f3f458752384d2397dc54fda71ecd0515d5b45fc854959d15ac96"};
    if (titles_executable.size() != 7022 || to_hex(sha256(titles_executable)) != spanish_sha256
        || !has_bytes(titles_executable, 0, entry)
        || !has_bytes(titles_executable, 0x0122 - 0x100, wrapper)
        || !has_bytes(titles_executable, 0x1931 - 0x100, post_title_loop)
        || require_unique(titles_executable, input_poll, "Spanish title input poll") != 0x0d0a - 0x100
        || !has_bytes(titles_executable, 0x1c2b - 0x100, input_nonzero_exit)) {
        throw std::runtime_error("Unsupported Millennium Spanish DOS title boundary");
    }
    return {spanish_sha256, 0x1b80, 0x21, 0x06, 0xff, 0x1c54,
        0x0122, 0x1968, 0x0013, 5, 0x1917};
}

MillenniumDosSpanishTitlePresentationEvidence
parse_millennium_dos_spanish_title_presentation_evidence(
    const std::span<const std::uint8_t> titles_executable,
    const std::span<const std::uint8_t> title_library) {
    constexpr std::string_view titles_sha256 =
        "02082c35e18cee330f7d1b88098f502e68011f7e47a3a649961f6f03d1d14fe7";
    constexpr std::string_view title_library_sha256 =
        "30d6ccb95e7f501d59e72fc2e34583302116bd88f6eceaae989f6ad986ef7f19";
    constexpr ExecutableByteAnchor<12> selection{"056142489c8d70d88640c5dd0dea385fd4a3d561efe95cb57625773840ca1327"};
    // Revalidate the independent input profile too. This prevents a generic
    // similarly-shaped COM image from being used to associate a title library
    // with the Spanish release.
    static_cast<void>(parse_millennium_dos_spanish_title_boundary(titles_executable));
    constexpr std::size_t selection_offset = 0x1c14 - 0x100;
    if (to_hex(sha256(titles_executable)) != titles_sha256
        || to_hex(sha256(title_library)) != title_library_sha256
        || !has_bytes(titles_executable, selection_offset, selection)) {
        throw std::runtime_error("Unsupported Millennium Spanish DOS title presentation evidence");
    }
    // The complete library identity pins the directory as well as P00. The
    // first resource's fixed span is read directly from the caller's view so
    // this evidence parser neither extracts nor owns a copy of game data.
    constexpr std::size_t p00_offset = 6;
    constexpr std::size_t p00_size = 10'555;
    if (title_library.size() < p00_offset + p00_size) {
        throw std::runtime_error("Unsupported Millennium Spanish DOS P00 title resource");
    }
    const auto resource = title_library.subspan(p00_offset, p00_size);
    const auto resource_sha256 = to_hex(sha256(resource));
    if (resource_sha256 != "91c315133e58634d7327c7d3a3e95ecaa035580200f609f161db6b044261b43b") {
        throw std::runtime_error("Unsupported Millennium Spanish DOS P00 title hash");
    }
    return {std::string(titles_sha256), std::string(title_library_sha256), 0x1c14, 0,
        0x1725, 0x1c1a, 0x1004, 0x1c1d, 0x1941, "P00", p00_offset, p00_size,
        resource_sha256};
}

MillenniumDosTitleFlow parse_millennium_dos_title_flow(
    std::span<const std::uint8_t> titles_executable,
    std::span<const std::uint8_t> mill_launcher) {
    // The local call/branch anchors below identify a recovered boundary, not
    // a generic DOS format. Bind both participating leaves in full before a
    // similarly-shaped executable can contribute any title/launcher fact.
    constexpr std::string_view titles_sha256 =
        "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6";
    constexpr std::string_view launcher_sha256 =
        "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e";
    if (titles_executable.size() != 7022 || mill_launcher.size() != 1445
        || to_hex(sha256(titles_executable)) != titles_sha256
        || to_hex(sha256(mill_launcher)) != launcher_sha256) {
        throw std::runtime_error("Unsupported Millennium English DOS title-flow leaves");
    }
    // Flat DOS files are loaded at offset 0x100.  The entry jump at file 0
    // lands at 0x1b80, where the title program establishes its stack.
    constexpr ExecutableByteAnchor<7> entry_jump{"f68952a9bbb82fa876f35aa293b010e2fb0be9f2814c77d2f8604391716ccd07"};
    constexpr ExecutableByteAnchor<6> title_selection{"f0805af54bb804b270a11e08547332a0601eecd904f9e8e42f11c9ea79930cd7"}; // AX=0; call 0x1725
    constexpr ExecutableByteAnchor<13> title_selection_callee_prefix{"7ac29ca8f2e9685a8285698f967d1e6f0e0fda76842bb4166d0cc15fa46cc2d1"};
    constexpr ExecutableByteAnchor<16> title_selection_callee_jle_target_prefix{"e26bb9b183aee42e91251408dc3b876838e0472fe9f3516e3c6715f0f3461067"};
    constexpr ExecutableByteAnchor<46> title_selection_nested_callee_prefix{"10a8d366b8fbc911960b5b800a1651b6e30ad3144008519a86dfefd4f48abbaf"};
    constexpr ExecutableByteAnchor<16> title_selection_nested_leaf_prefix{"0c3e92ab20959a3c185dd4ebd7a9ff731ddef8bb63a18a7625eb5802adcfdfa5"};
    constexpr ExecutableByteAnchor<13> transition_setup{"98245c2d0f86d2037a032c43898944758f68aeaac515c23109f9ecb88de7274b"};
    constexpr ExecutableByteAnchor<7> input_poll{"96715350de2c4a159dd994b1c40dafb21c038f38f52937aa1a54491485146dc6"};
    // All nonzero DOS poll results take this one exit path. The returned AL
    // is only tested, never decoded as a scan code or a named control. The
    // path subsequently reaches a private INT 91h wrapper, so it remains
    // static evidence rather than a host-side loading animation.
    constexpr ExecutableByteAnchor<66> input_branch{"d08916b10f92fe78e643a3335680c341f2347c361cded2419954422c0c37e6dd"};
    constexpr ExecutableByteAnchor<16> input_exit_loading_text{"1db412f70481caefc39f1fd0d330b05cef0638ce34735f44d6ec56e7b2318594"};
    constexpr ExecutableByteAnchor<7> input_exit_private_driver_entry{"964b6704df7b2a452669a3a4acbf934eaea023fc29343430e70b02b62c0d888f"};
    constexpr ExecutableByteAnchor<16> input_exit_private_driver_loop{"5eebd21fcd4da98b08e7b29a27c3ee7b88405949ff3c1838b31aecc3ebf596fb"};
    constexpr ExecutableByteAnchor<25> input_exit_helper_loop{"17910a31c3aa8cbbb788c95e0b303828ea210fec847fa658502f3e32728207b4"};
    constexpr ExecutableByteAnchor<29> input_exit_helper_selector{"a08610633a7f4b202daa5c51417097e73f40ebd59b8f97b1f67454e1016ec9d4"};
    constexpr ExecutableByteAnchor<13> input_exit_helper_patch_offset_builder{"6fa75475170547fb9a9018b3960df7fa64b1cad87311bba3bbf19eb3c3451bf0"};
    constexpr ExecutableByteAnchor<24> input_exit_helper_position_dispatch{"62de989e3d07e863425a27d71d402ac07d6c7e7cf27c4b712ceb176566b604b5"};
    constexpr ExecutableByteAnchor<8> input_exit_helper_position_table_first{"c7c869480e7d21e18110f4780b7e4dec5de3fb4a0ff81b7ef51bc244a577390b"};
    constexpr ExecutableByteAnchor<4> input_exit_helper_position_table_last{"82322cbc7e3ab2ff060c3f38d3afe92be3895b5b08f90054e9642c088b269a35"};
    constexpr ExecutableByteAnchor<34> title_buffer_setup{"1d09fc8745da25f46945c0cf038c156d6a66eb7f7c5d7fde452804a8255d42ac"};
    constexpr ExecutableByteAnchor<10> clean_exit{"04ed07830e7ac6deedf3d93b3e78fe5edff092c6b8758c8e4bffd89ba19bcb69"};
    constexpr ExecutableByteAnchor<8> dos_exit{"71853cdd1fe4b6f587a7012f7278054bf03969b608331a3ab17fac0a2157ab49"};
    constexpr ExecutableByteAnchor<40> title_driver_setup{"e6e014d7c03f9efbd7e9bde67686c281cf66acca809b306cc29dfb45d614b535"};
    constexpr ExecutableByteAnchor<13> private_wrapper{"5d17daad68e9062dc6852ae76740db4afdcb81555ba9fb7d15d4e4aa8d088175"};
    constexpr ExecutableByteAnchor<2> title_driver_record{"47dc540c94ceb704a23875c11273e16bb0b8a87aed84de911f2133568115f254"};

    if (!has_bytes(titles_executable, 0, entry_jump)) {
        throw std::runtime_error("Unsupported Millennium DOS title entry");
    }
    constexpr std::size_t file_to_load_bias = 0x100;
    constexpr std::size_t title_selection_offset = 0x1c14 - file_to_load_bias;
    constexpr std::size_t input_branch_offset = 0x1c28 - file_to_load_bias;
    constexpr std::size_t input_poll_call_address = 0x1c28;
    constexpr std::size_t input_poll_helper_address = 0x0d0a;
    constexpr std::size_t clean_exit_offset = 0x1c5a - file_to_load_bias;
    constexpr std::size_t dos_exit_offset = 0x1a12 - file_to_load_bias;
    if (!has_bytes(titles_executable, title_selection_offset, title_selection)
        || !has_bytes(titles_executable, input_branch_offset, input_branch)
        || !has_bytes(titles_executable, clean_exit_offset, clean_exit)
        || !has_bytes(titles_executable, dos_exit_offset, dos_exit)
        || !has_bytes(titles_executable, 0x1b80 - file_to_load_bias, title_driver_setup)
        || !has_bytes(titles_executable, 0x0122 - file_to_load_bias, private_wrapper)
        || !has_bytes(titles_executable, 0x1ac4 - file_to_load_bias, title_driver_record)
        || !has_bytes(titles_executable, 0x1884 - file_to_load_bias, input_exit_loading_text)
        || !has_bytes(titles_executable, 0x1968 - file_to_load_bias, input_exit_private_driver_entry)
        || !has_bytes(titles_executable, 0x1931 - file_to_load_bias, input_exit_private_driver_loop)
        || !has_bytes(titles_executable, 0x1917 - file_to_load_bias, input_exit_helper_loop)
        || !has_bytes(titles_executable, 0x18f9 - file_to_load_bias, input_exit_helper_selector)
        || !has_bytes(titles_executable, 0x1712 - file_to_load_bias, input_exit_helper_patch_offset_builder)
        || !has_bytes(titles_executable, 0x174a - file_to_load_bias, input_exit_helper_position_dispatch)
        || !has_bytes(titles_executable, 0x1768 - file_to_load_bias, input_exit_helper_position_table_first)
        || !has_bytes(titles_executable, 0x17a0 - file_to_load_bias, input_exit_helper_position_table_last)
        || !has_bytes(titles_executable, 0x135e - file_to_load_bias, title_buffer_setup)) {
        throw std::runtime_error("Unsupported Millennium DOS title control flow");
    }
    constexpr std::size_t title_selection_callee_address = 0x1725;
    if (!has_bytes(titles_executable, title_selection_callee_address - file_to_load_bias,
                   title_selection_callee_prefix)) {
        throw std::runtime_error("Unsupported Millennium DOS title selection callee");
    }
    constexpr std::size_t title_selection_callee_jle_target = 0x1732;
    if (!has_bytes(titles_executable, title_selection_callee_jle_target - file_to_load_bias,
                   title_selection_callee_jle_target_prefix)) {
        throw std::runtime_error("Unsupported Millennium DOS title selection JLE target");
    }
    if (!has_bytes(titles_executable, 0x1390 - file_to_load_bias,
                   title_selection_nested_callee_prefix)) {
        throw std::runtime_error("Unsupported Millennium DOS title selection nested callee");
    }
    if (!has_bytes(titles_executable, 0x13c - file_to_load_bias,
                   title_selection_nested_leaf_prefix)) {
        throw std::runtime_error("Unsupported Millennium DOS title selection nested leaf");
    }
    static_cast<void>(require_unique(titles_executable, transition_setup, "title transition loop"));
    if (require_unique(titles_executable, input_poll, "title input poll")
        != input_poll_helper_address - file_to_load_bias) {
        throw std::runtime_error("Unsupported Millennium DOS title input-poll helper");
    }

    // This is a caller-side fact only.  MILL.COM loads DX with each adjacent
    // program string, makes two near calls to the same local target, and
    // tests AL between them.  We deliberately do not assign a meaning to
    // the target or to either post-call status test.
    constexpr ExecutableByteAnchor<22> launcher_call_chain{"829b3d096d593d1ff4f1028eb05af1ccf8ca0b8ead98a5edcb523dba4cd725cf"};
    // The shared local target is preserved as raw control flow.  Its first
    // local branch is JC +5 at 0x345: the non-taken bytes end in RET at
    // 0x34b, while the taken destination starts at 0x34c.  No interrupt or
    // return semantics are inferred here.
    constexpr ExecutableByteAnchor<50> launcher_common_routine{"635407dc237538c96bc7cbd1f34f7fd3f83dfe0051ca2da2e64dfc7063f4c080"};
    constexpr ExecutableByteAnchor<14> launcher_branch_target_bytes{"eef67bc7e389dab3d03ba67b93c9e690ea971ed52e5a6e27a056ef021a32d62a"};
    // This static caller-side range is immediately before the DX=0x68f
    // setup.  It records a post-call JE and the later local near call without
    // claiming either call's return behavior or assigning meaning to AL.
    constexpr ExecutableByteAnchor<45> launcher_pre_title_chain{"f85abb17f9eb88ca911788b00af4719c50ec2307c1f18b85bdbbe9cab35be3be"};
    // The local target of the near call at 0x231 is only profiled through its
    // first conditional split.  The conditional's meaning and all interrupt
    // effects remain deliberately unmodelled.
    constexpr ExecutableByteAnchor<19> launcher_pre_title_callee_prefix{"f5e4b8fec5ab5875ff7ab15c75a4486338914c869a0f80952110f419f2fa9712"};
    constexpr ExecutableByteAnchor<14> launcher_pre_title_callee_jnc_target_prefix{"1d20ec61c3489df31b8c8c55760c715a4359d8e84c7adb74f6c81eda4f485321"};
    constexpr ExecutableByteAnchor<12> launcher_pre_title_callee_jc_target_prefix{"84ad630af41e41c04728eedf9769121b9062224ebbf5aa1baa8df000f4f19f9a"};
    constexpr ExecutableByteAnchor<16> launcher_pre_title_callee_join_prefix{"082750245e349594d03c9169369a8d15335996185ba0f168434b6478b7e681f6"};
    constexpr ExecutableByteAnchor<10> launcher_pre_title_callee_join_branch{"ee072ddd5db21ea6d44aaf0b93376ebc112df370899691c8a0f06d4cbaf3be2f"};
    constexpr ExecutableByteAnchor<48> launcher_exec_helper{"62cee56837e015eecc218906046c1e1c19a7ad9ba87e6580f99674eac0976b58"};
    constexpr ExecutableByteAnchor<14> launcher_exec_param_block{"e2b2aa089d2c6a23b14055f3721c6b53836268070c2a727d2d7fa1a75461869b"};
    // The raw private-vector installation is preceded by a local loader call.
    // The call's DOS effects are intentionally not evaluated: this byte range
    // establishes only the direct call target and the literal DX=0 / AX=$2591
    // setup at the following interrupt instruction.
    constexpr ExecutableByteAnchor<12> launcher_private_interrupt_install{"df3b878ca7eb13ae3509d271eb6f0dced3c534180dbe8935b399dfd56b0bf811"};
    constexpr ExecutableByteAnchor<13> launcher_private_interrupt_query{"f549729b17553fe940573af7c7b4baccca4509c97641c87fd309438f8135c27a"};
    constexpr ExecutableByteAnchor<10> launcher_private_interrupt_restore{"806a50b1d7fcb55eca56a0de70e74f6d8e7bb4b9e4ab73702c85e08e98885eed"};
    // Before the first call to $02cf, AL==1 keeps DX=$0617; the other path
    // loads DX=$05f9. $02cf opens the selected original file, seeks to its
    // end, rounds the length to paragraphs, allocates a segment, rewinds,
    // reads CX bytes at DS:0000, closes, and returns. The code proves this
    // transfer ABI but deliberately does not assign any DOS result, segment,
    // or handler execution semantics.
    constexpr ExecutableByteAnchor<12> launcher_private_interrupt_handler_selection{"c92d7e6e830b7785d1749250a4879d70cee7a87a147d41e4cb465481a6d7f76d"};
    constexpr ExecutableByteAnchor<45> launcher_video_selection_scan{"157c83c6cdef55dfb7531bceee1759884f68237f445437553d36c12f167d6eba"};
    constexpr ExecutableByteAnchor<77> launcher_private_interrupt_handler_loader{"5c7f7ec03aa3109d4df2fbdec457ca7e2be412fc9d818bd07252a039fb8c6671"};
    constexpr ExecutableByteAnchor<7> launcher_pre_title_callee_join_target_prefix{"b5f36b3aede6a55d03f540f96de1e31eb3459fb167be4a75c744548e4f7f1560"};
    constexpr ExecutableByteAnchor<11> title_name{"8591f84bc4f9a8c6cc2134853a800147de6e11d64f5d363a0645a701a386fce2"};
    constexpr ExecutableByteAnchor<11> game_name{"06b11f376f111baff60775a1986a1473d07d9e1788587c66e991e10e74b0dfdb"};
    constexpr ExecutableByteAnchor<10> ega640_name{"9183b6686ed145c0be0045cc0412c3f92776eea35600597f4bbb1aa2a1de588e"};
    constexpr ExecutableByteAnchor<8> mcga_name{"2c2626a5700aac6b83cf8efd8632ab9bebc5b5b0ec93b0b175d4b5b375271241"};
    constexpr std::size_t mill_load_bias = 0x100;
    const auto launcher_chain_offset = require_unique(
        mill_launcher, launcher_call_chain, "launcher caller-side call chain");
    constexpr std::size_t title_call_in_chain = 3;
    constexpr std::size_t game_call_in_chain = 15;
    const auto title_call_offset = launcher_chain_offset + title_call_in_chain;
    const auto game_call_offset = launcher_chain_offset + game_call_in_chain;
    const auto title_call_address = title_call_offset + mill_load_bias;
    const auto game_call_address = game_call_offset + mill_load_bias;
    const auto title_call_target = title_call_address + 3 + 0x00d9;
    const auto game_call_target = game_call_address + 3 + 0x00cd;
    if (title_call_target != game_call_target) {
        throw std::runtime_error("Invalid Millennium DOS launcher common call target");
    }
    if (title_call_target < mill_load_bias
        || !has_bytes(mill_launcher, title_call_target - mill_load_bias, launcher_common_routine)) {
        throw std::runtime_error("Unsupported Millennium DOS launcher common routine");
    }
    if (!has_bytes(mill_launcher, 0x031c - mill_load_bias, launcher_exec_helper)
        || !has_bytes(mill_launcher, 0x067a - mill_load_bias, launcher_exec_param_block)) {
        throw std::runtime_error("Unsupported Millennium DOS launcher EXEC boundary");
    }
    constexpr std::size_t common_branch_target = 0x34c;
    if (!has_bytes(mill_launcher, common_branch_target - mill_load_bias,
                   launcher_branch_target_bytes)) {
        throw std::runtime_error("Unsupported Millennium DOS launcher branch target");
    }
    constexpr std::size_t pre_title_chain_address = 0x210;
    if (!has_bytes(mill_launcher, pre_title_chain_address - mill_load_bias,
                   launcher_pre_title_chain)) {
        throw std::runtime_error("Unsupported Millennium DOS launcher pre-title chain");
    }
    constexpr std::size_t pre_title_callee_address = 0x2cf;
    if (!has_bytes(mill_launcher, pre_title_callee_address - mill_load_bias,
                   launcher_pre_title_callee_prefix)) {
        throw std::runtime_error("Unsupported Millennium DOS launcher pre-title callee prefix");
    }
    constexpr std::size_t pre_title_callee_jnc_target = 0x2e2;
    if (!has_bytes(mill_launcher, pre_title_callee_jnc_target - mill_load_bias,
                   launcher_pre_title_callee_jnc_target_prefix)) {
        throw std::runtime_error("Unsupported Millennium DOS launcher JNC-target prefix");
    }
    constexpr std::size_t pre_title_callee_jc_target = 0x2d6;
    if (!has_bytes(mill_launcher, pre_title_callee_jc_target - mill_load_bias,
                   launcher_pre_title_callee_jc_target_prefix)) {
        throw std::runtime_error("Unsupported Millennium DOS launcher JC-target prefix");
    }
    constexpr std::size_t pre_title_callee_join = 0x269;
    if (!has_bytes(mill_launcher, pre_title_callee_join - mill_load_bias,
                   launcher_pre_title_callee_join_prefix)
        || !has_bytes(mill_launcher, 0x2aa - mill_load_bias,
                      launcher_pre_title_callee_join_branch)) {
        throw std::runtime_error("Unsupported Millennium DOS launcher join branch");
    }
    constexpr std::size_t pre_title_callee_join_target = 0x2c8;
    if (!has_bytes(mill_launcher, pre_title_callee_join_target - mill_load_bias,
                   launcher_pre_title_callee_join_target_prefix)) {
        throw std::runtime_error("Unsupported Millennium DOS launcher join target");
    }
    constexpr std::size_t private_interrupt_loader_call_address = 0x204;
    constexpr std::size_t private_interrupt_loader_call_target = 0x2cf;
    constexpr std::size_t private_interrupt_install_address = 0x209;
    if (!has_bytes(mill_launcher, private_interrupt_loader_call_address - mill_load_bias,
                   launcher_private_interrupt_install)) {
        throw std::runtime_error("Unsupported Millennium DOS private interrupt installation");
    }
    constexpr std::size_t private_interrupt_query_address = 0x167;
    constexpr std::size_t private_interrupt_restore_address = 0x269;
    if (!has_bytes(mill_launcher, private_interrupt_query_address - mill_load_bias,
                   launcher_private_interrupt_query)
        || !has_bytes(mill_launcher, private_interrupt_restore_address - mill_load_bias,
                      launcher_private_interrupt_restore)) {
        throw std::runtime_error("Unsupported Millennium DOS private interrupt preservation chain");
    }
    constexpr std::size_t private_interrupt_handler_selection_address = 0x1de;
    constexpr std::size_t video_selection_scan_address = 0x19d;
    if (!has_bytes(mill_launcher, video_selection_scan_address - mill_load_bias,
                   launcher_video_selection_scan)
        || !has_bytes(mill_launcher, private_interrupt_handler_selection_address - mill_load_bias,
                   launcher_private_interrupt_handler_selection)
        || !has_bytes(mill_launcher, private_interrupt_loader_call_target - mill_load_bias,
                      launcher_private_interrupt_handler_loader)
        || !has_bytes(mill_launcher, 0x0617 - mill_load_bias, ega640_name)
        || !has_bytes(mill_launcher, 0x05f9 - mill_load_bias, mcga_name)) {
        throw std::runtime_error("Unsupported Millennium DOS private interrupt handler loader");
    }
    const auto title_offset = require_unique(mill_launcher, title_name, "launcher title program");
    const auto game_offset = require_unique(mill_launcher, game_name, "launcher game program");
    if (title_offset >= game_offset || game_offset != title_offset + title_name.size()) {
        throw std::runtime_error("Invalid Millennium DOS launcher hand-off order");
    }

    return {
        .title_entry_address = 0x1b80,
        .title_selection_callee_entry_address = 0x1725,
        .title_selection_callee_branch_address = 0x172f,
        .title_selection_callee_branch_target = 0x1732,
        .title_selection_callee_fallthrough_return = 0x1731,
        .title_selection_callee_jle_target_call_address = 0x173d,
        .title_selection_callee_jle_target_call_target = 0x1390,
        .title_selection_nested_callee_call_address = 0x13bb,
        .title_selection_nested_callee_call_target = 0x13c,
        .title_selection_nested_callee_terminal_address = 0x14b,
        .title_resource_index = 0,
        .intro_transition_steps = 0x25,
        .intro_step_stride = 0x170,
        .input_interrupt = 0x21,
        .input_service = 0x06,
        .input_parameter = 0xff,
        .input_poll_call_address = input_poll_call_address,
        .input_poll_helper_address = input_poll_helper_address,
        .input_nonzero_exit_address = 0x1c54,
        .input_exit_first_call_address = 0x1c54,
        .input_exit_first_call_target = 0x1968,
        .input_exit_loading_text_address = 0x1884,
        .input_exit_loading_text = "    LOADING    2",
        .input_exit_private_driver_entry_address = 0x1968,
        .input_exit_private_driver_loop_address = 0x1931,
        .input_exit_private_driver_wrapper_address = 0x0122,
        .input_exit_private_driver_function = 0x0013,
        .input_exit_private_driver_call_count = 5,
        .input_exit_private_driver_helper_address = 0x1917,
        .input_exit_helper_selector_iterations = 15,
        .input_exit_helper_selector_state_address = 0x1181,
        .input_exit_helper_selector_accumulator_address = 0x18f7,
        .input_exit_helper_selector_mask = 0x03ff,
        .input_exit_helper_selector_range = 0x24,
        .input_exit_helper_selector_subtract = 0x18,
        .input_exit_helper_resource_index_bias = 1,
        .input_exit_helper_resource_loader_address = 0x1712,
        .input_exit_helper_patch_offset_builder_address = 0x1712,
        .input_exit_helper_patch_offset_stride = 0x0170,
        .input_exit_helper_patch_offset_cell_address = 0x1341,
        .input_exit_helper_position_table_address = 0x1768,
        .input_exit_helper_position_count = 15,
        .input_exit_helper_position_stride = 4,
        .input_exit_helper_private_driver_function = 6,
        .input_exit_helper_driver_record_address = 0x1349,
        .input_exit_helper_driver_record_source_offset_cell_address = 0x1349,
        .input_exit_helper_driver_record_segment_cell_address = 0x134b,
        .input_exit_helper_driver_record_table_second_cell_address = 0x134f,
        .input_exit_helper_driver_record_table_first_cell_address = 0x1351,
        .input_exit_helper_decoded_height_cell_address = 0x1357,
        .input_exit_helper_decoded_width_cell_address = 0x1359,
        .title_buffer_setup_address = 0x135e,
        .title_buffer_source_offset_cell_address = 0x1341,
        .title_buffer_source_segment_cell_address = 0x1343,
        .exit_code = 0,
        .title_private_interrupt_wrapper_address = 0x0122,
        .title_private_interrupt_record_address = 0x1ac4,
        .title_private_interrupt_function = 0,
        .title_private_interrupt_result_word_address = 0x1a9c,
        .title_private_interrupt_result_low_byte_address = 0x1aaa,
        .title_private_interrupt_result_high_byte_address = 0x0107,
        .title_private_interrupt_equal_branch_target = 0x1ac6,
        .title_private_interrupt_other_branch_target = 0x1ada,
        .launcher_title_program_address = static_cast<std::uint16_t>(title_offset + mill_load_bias),
        .launcher_game_program_address = static_cast<std::uint16_t>(game_offset + mill_load_bias),
        .launcher_title_call_address = static_cast<std::uint16_t>(title_call_address),
        .launcher_game_call_address = static_cast<std::uint16_t>(game_call_address),
        .launcher_common_call_target = static_cast<std::uint16_t>(title_call_target),
        .launcher_common_branch_address = 0x345,
        .launcher_common_branch_target = common_branch_target,
        .launcher_common_fallthrough_return = 0x34b,
        .launcher_common_branch_target_static_boundary = 0x35a,
        .launcher_exec_helper_address = 0x031c,
        .launcher_exec_param_block_address = 0x067a,
        .launcher_exec_saved_stack_address = 0x05f7,
        .launcher_exec_interrupt_site = 0x0337,
        .launcher_exec_result_interrupt_site = 0x0348,
        .launcher_exec_carry_branch_address = 0x0345,
        .launcher_exec_noncarry_return_address = 0x034b,
        .launcher_pre_title_gate_address = 0x215,
        .launcher_pre_title_gate_target = 0x21a,
        .launcher_pre_title_call_address = 0x231,
        .launcher_pre_title_call_target = 0x2cf,
        .launcher_pre_title_callee_branch_address = 0x2d4,
        .launcher_pre_title_callee_branch_target = 0x2e2,
        .launcher_pre_title_callee_fallthrough_jump_address = 0x2e0,
        .launcher_pre_title_callee_fallthrough_jump_target = 0x269,
        .launcher_pre_title_callee_jnc_target_branch_address = 0x2ed,
        .launcher_pre_title_callee_jnc_target_branch_target = 0x2d6,
        .launcher_pre_title_callee_jc_target_jump_address = 0x2e0,
        .launcher_pre_title_callee_jc_target_jump_target = 0x269,
        .launcher_pre_title_callee_join_branch_address = 0x2b2,
        .launcher_pre_title_callee_join_branch_target = 0x2c8,
        .launcher_pre_title_callee_join_branch_terminal_address = 0x2ce,
        .launcher_private_interrupt_loader_call_address = private_interrupt_loader_call_address,
        .launcher_private_interrupt_loader_call_target = private_interrupt_loader_call_target,
        .launcher_private_interrupt_install_address = private_interrupt_install_address,
        .launcher_private_interrupt_number = 0x91,
        .launcher_private_interrupt_handler_offset = 0,
        .launcher_private_interrupt_saved_offset_cell = 0x5e7,
        .launcher_private_interrupt_saved_segment_cell = 0x5e9,
        .launcher_private_interrupt_restore_address = private_interrupt_restore_address,
        .launcher_private_interrupt_handler_loader_entry = private_interrupt_loader_call_target,
        .launcher_private_interrupt_handler_destination_offset = 0,
        .launcher_private_interrupt_handler_open_service = 0x3d,
        .launcher_private_interrupt_handler_seek_end_service = 0x42,
        .launcher_private_interrupt_handler_allocate_service = 0x48,
        .launcher_private_interrupt_handler_rewind_service = 0x42,
        .launcher_private_interrupt_handler_read_service = 0x3f,
        .launcher_private_interrupt_handler_close_service = 0x3e,
        .launcher_video_selection_scan_address = static_cast<std::uint16_t>(video_selection_scan_address),
        .launcher_video_selection_default_detector_address = 0x05a1,
        .launcher_video_selection_map_address = static_cast<std::uint16_t>(private_interrupt_handler_selection_address),
        .launcher_private_interrupt_handler_first_selector = 1,
        .launcher_private_interrupt_handler_first_program_address = 0x0617,
        .launcher_private_interrupt_handler_other_selector = 2,
        .launcher_private_interrupt_handler_other_program_address = 0x05f9,
        .launcher_private_interrupt_handler_first_program = "ega640.bin",
        .launcher_private_interrupt_handler_other_program = "mcga.bin",
        .launcher_title_offset = title_offset,
        .launcher_game_offset = game_offset,
        .launcher_title_program = "TITLES.EXE",
        .launcher_game_program = "2200ad.exe",
    };
}

} // namespace eon
