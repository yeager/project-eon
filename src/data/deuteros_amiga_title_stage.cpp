#include "data/deuteros_amiga_title_stage.hpp"

#include "data/sha256.hpp"

#include <span>
#include <stdexcept>
#include <string_view>

namespace eon {
namespace {

std::uint16_t big16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) throw std::runtime_error("Truncated Deuteros title-stage word");
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U) | bytes[offset + 1]);
}

std::uint32_t big32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(big16(bytes, offset)) << 16U) | big16(bytes, offset + 2);
}

void require_word(std::span<const std::uint8_t> bytes, std::size_t offset, std::uint16_t expected) {
    const auto actual = big16(bytes, offset);
    if (actual != expected) {
        throw std::runtime_error("Unexpected Deuteros title-stage opcode at offset "
            + std::to_string(offset) + " (expected " + std::to_string(expected)
            + ", got " + std::to_string(actual) + ')');
    }
}

void require_long(std::span<const std::uint8_t> bytes, std::size_t offset, std::uint32_t expected) {
    if (big32(bytes, offset) != expected) {
        throw std::runtime_error("Unexpected Deuteros title-stage operand at offset " + std::to_string(offset));
    }
}

} // namespace

DeuterosAmigaTitleStageProfile parse_deuteros_amiga_title_stage(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan) {
    const auto& stage = plan.title_stage;
    if (stage.entry_address < stage.destination || stage.entry_address - stage.destination >= stage.length) {
        throw std::runtime_error("Deuteros title stage entry outside its loaded range");
    }
    const auto entry_offset = stage.disk_offset + stage.entry_address - stage.destination;
    const auto stage_code = [&](std::uint32_t address, std::size_t length) {
        if (address < stage.destination || address - stage.destination > stage.length
            || length > stage.length - (address - stage.destination)) {
            throw std::runtime_error("Deuteros title-stage helper outside its loaded range");
        }
        return disk.bytes(stage.disk_offset + address - stage.destination, length);
    };
    const auto bootstrap_code = [&](std::uint32_t address, std::size_t length) {
        const auto& bootstrap = plan.bootstrap_loader;
        if (address < bootstrap.destination || address - bootstrap.destination > bootstrap.length
            || length > bootstrap.length - (address - bootstrap.destination)) {
            throw std::runtime_error("Deuteros bootstrap helper outside its loaded range");
        }
        return disk.bytes(bootstrap.disk_offset + address - bootstrap.destination, length);
    };
    // This includes the loop's timer branch at $405b6, but deliberately does
    // not turn the remaining 68000 program into guessed gameplay semantics.
    // Covers the main-loop branch and the complete known timer-dispatch
    // prologue, while remaining inside the verified loaded stage.
    const auto code = disk.bytes(entry_offset, 0x400);

    // move.w d0,$4040e; cmp.b #5,d0; bne $40448
    require_word(code, 6, 0x33c0);
    require_long(code, 8, 0x0004040e);
    require_word(code, 12, 0xb03c);
    require_word(code, 14, 0x0005);
    require_word(code, 16, 0x6610); // bne.b $40448

    // mode 5: move.b d0,$3717e; move.w #$101,$38092; bra $40450
    require_word(code, 18, 0x13c0);
    require_long(code, 20, 0x0003717e);
    require_word(code, 24, 0x33fc);
    require_word(code, 26, 0x0101);
    require_long(code, 28, 0x00038092);
    require_word(code, 32, 0x6008);
    // default: move.b #1,$19d52
    require_word(code, 34, 0x13fc);
    require_word(code, 36, 0x0001);
    require_long(code, 38, 0x00019d52);

    // Both mode branches join at $40450. This startup prefix enters an
    // original stack, calls two Exec vectors, applies literal internal setup,
    // and programs four custom-chip words before reaching the recurring loop.
    // It is captured as a requirement profile; no external vector or hardware
    // effect is emulated here.
    constexpr std::size_t initialization = 0x2a; // $40450 - $40426
    require_word(code, initialization, 0x2e7c); // movea.l #$40b62,a7
    require_long(code, initialization + 2, 0x00040b62);
    require_word(code, initialization + 6, 0x2c78); // movea.l $4.w,a6
    require_word(code, initialization + 8, 0x0004);
    require_word(code, initialization + 10, 0x4eae); // jsr -$96(a6)
    require_word(code, initialization + 12, 0xff6a);
    require_word(code, initialization + 14, 0x203c); // move.l #$7fff0,d0
    require_long(code, initialization + 16, 0x0007fff0);
    require_word(code, initialization + 20, 0x2c78); // movea.l $4.w,a6
    require_word(code, initialization + 22, 0x0004);
    require_word(code, initialization + 24, 0x4eae); // jsr -$9c(a6)
    require_word(code, initialization + 26, 0xff64);
    const auto require_call = [&](const std::size_t offset, const std::uint32_t target) {
        require_word(code, offset, 0x4eb9);
        require_long(code, offset + 2, target);
    };
    require_call(initialization + 28, 0x1ed80);
    require_call(initialization + 34, 0x1f172);
    require_word(code, initialization + 40, 0x23f9); // $1f168 -> $1f974
    require_long(code, initialization + 42, 0x0001f168);
    require_long(code, initialization + 46, 0x0001f974);
    require_call(initialization + 50, 0x1f182);
    require_word(code, initialization + 56, 0x23f9); // $1f168 -> $410d8
    require_long(code, initialization + 58, 0x0001f168);
    require_long(code, initialization + 62, 0x000410d8);
    require_word(code, initialization + 66, 0x207c); // movea.l #$dff000,a0
    require_long(code, initialization + 68, 0x00dff000);
    constexpr std::array<std::uint16_t, 4> custom_offsets{{0x40, 0x42, 0x9a, 0x96}};
    constexpr std::array<std::uint16_t, 4> custom_values{{0x7fff, 0x7fff, 0xc000, 0x87ff}};
    for (std::size_t index = 0; index < custom_offsets.size(); ++index) {
        const auto offset = initialization + 72 + index * 6U;
        require_word(code, offset, 0x317c);
        require_word(code, offset + 2, custom_values[index]);
        require_word(code, offset + 4, custom_offsets[index]);
    }
    require_call(initialization + 96, 0x1ef74);
    require_call(initialization + 102, 0x206d4);
    require_call(initialization + 108, 0x206be);
    require_word(code, initialization + 114, 0x223c); // move.l #$13000,d1
    require_long(code, initialization + 116, 0x00013000);
    require_call(initialization + 120, 0x403e6);
    require_call(initialization + 126, 0x403f4);
    require_word(code, initialization + 132, 0x41f9); // lea $12ff4,a0
    require_long(code, initialization + 134, 0x00012ff4);
    require_word(code, initialization + 138, 0x2018);
    require_word(code, initialization + 140, 0x23c0);
    require_long(code, initialization + 142, 0x00037ef2);
    require_word(code, initialization + 146, 0x2018);
    require_word(code, initialization + 148, 0x23c0);
    require_long(code, initialization + 150, 0x00037ef6);
    require_call(initialization + 154, 0x204c8);
    require_call(initialization + 160, 0x389e2);
    require_word(code, initialization + 166, 0x7001);
    require_call(initialization + 168, 0x1fb9a);
    require_call(initialization + 174, 0x38912);
    require_call(initialization + 180, 0x2022a);
    require_word(code, initialization + 186, 0x303c);
    require_word(code, initialization + 188, 0x004d);
    require_call(initialization + 190, 0x41bb4);
    require_word(code, initialization + 196, 0x303c);
    require_word(code, initialization + 198, 0x004e);
    require_call(initialization + 200, 0x41bb4);
    require_word(code, initialization + 206, 0x23fc); // $2151a -> $222ae
    require_long(code, initialization + 208, 0x0002151a);
    require_long(code, initialization + 212, 0x000222ae);
    require_word(code, initialization + 216, 0x7000);
    require_call(initialization + 218, 0x20e18);
    require_call(initialization + 224, 0x20ba8);
    require_word(code, initialization + 230, 0x41f9);
    require_long(code, initialization + 232, 0x00020cfe);
    require_word(code, initialization + 236, 0x4e90); // jsr (a0)
    require_word(code, initialization + 238, 0x2039);
    require_long(code, initialization + 240, 0x00012fe4);
    require_word(code, initialization + 244, 0xe688);
    require_word(code, initialization + 246, 0x33c0);
    require_long(code, initialization + 248, 0x0001f42a);
    require_call(initialization + 252, 0x37180);
    require_word(code, initialization + 258, 0x23f9);
    require_long(code, initialization + 260, 0x0001378e);
    require_long(code, initialization + 264, 0x0001c26c);
    require_word(code, initialization + 268, 0x7005);
    require_word(code, initialization + 270, 0xb079);
    require_long(code, initialization + 272, 0x0004040e);
    require_word(code, initialization + 276, 0x6608);
    require_call(initialization + 278, 0x36a8c);
    require_word(code, initialization + 284, 0x6006);
    require_call(initialization + 286, 0x1fb9a);

    // The recurring service pair starts at $40574 after common startup. The
    // counter is reset on input mode change and runs to $ea60 before dispatch.
    constexpr std::size_t loop = 0x14e; // $40574 - $40426
    require_word(code, loop, 0x4eb9); // jsr absolute long
    require_long(code, loop + 2, 0x000222c0);
    require_word(code, loop + 6, 0x4eb9);
    require_long(code, loop + 8, 0x00023e4e);
    require_word(code, 0x16e, 0x23fc); // move.l #0,$40410
    require_long(code, 0x170, 0);
    require_long(code, 0x174, 0x00040410);
    require_word(code, 0x178, 0x2039); // move.l $40410,d0
    require_long(code, 0x17a, 0x00040410);
    require_word(code, 0x17e, 0xb0bc); // cmp.l #$ea60,d0
    require_long(code, 0x180, 0x0000ea60);
    require_word(code, 0x186, 0x0c79); // cmpi.w #$11,$22d34
    require_word(code, 0x188, 0x0011);
    require_long(code, 0x18a, 0x00022d34);
    require_word(code, 0x190, 0x4eb9); // jsr $4069a
    require_long(code, 0x192, 0x0004069a);
    // The only direct post-return action in this caller clears the elapsed
    // timer before returning to the recurring service loop.
    require_word(code, 0x196, 0x23fc); // move.l #0,$40410
    require_long(code, 0x198, 0);
    require_long(code, 0x19c, 0x00040410);

    // $4069a is the target of the elapsed-timer dispatch.  It first marks a
    // transition active, copies sixteen RGB4 words from $1ed24 into a private
    // work area while masking the low bit of every component, then invokes two
    // graphics-library vectors.  Keep the callable-vector values as machine
    // facts: their high-level effect is not named by this parser.
    constexpr std::size_t transition = 0x274; // $4069a - $40426
    require_word(code, transition, 0x13fc); // move.b #1,$202c6
    require_word(code, transition + 2, 0x0001);
    require_long(code, transition + 4, 0x000202c6);
    require_word(code, transition + 8, 0x41f9); // lea $1ed24,a0
    require_long(code, transition + 10, 0x0001ed24);
    require_word(code, transition + 14, 0x43f9); // lea $40678,a1
    require_long(code, transition + 16, 0x00040678);
    require_word(code, transition + 20, 0x7e0f); // moveq #15,d7
    require_word(code, transition + 24, 0x0240); // andi.w #$eee,d0
    require_word(code, transition + 26, 0x0eee);
    require_word(code, transition + 32, 0x51cf); // dbra d7,$406b0
    require_word(code, transition + 44, 0x13fc); // move.b #0,$202b8
    require_word(code, transition + 46, 0x0000);
    require_long(code, transition + 48, 0x000202b8);
    require_word(code, transition + 58, 0x43f9); // lea $40678,a1
    require_long(code, transition + 60, 0x00040678);
    require_word(code, transition + 66, 0x2c79); // movea.l $12fec,a6
    require_long(code, transition + 68, 0x00012fec);
    require_word(code, transition + 72, 0x4eae); // jsr -$c0(a6)
    require_word(code, transition + 74, 0xff40);
    require_word(code, transition + 106, 0x4eae); // jsr -$1a4(a6)
    require_word(code, transition + 108, 0xfe5c);
    require_word(code, transition + 110, 0x3039); // move.w $1ffc8,d0
    require_long(code, transition + 112, 0x0001ffc8);
    require_word(code, transition + 116, 0x3239); // move.w $1ffce,d1
    require_long(code, transition + 118, 0x0001ffce);
    require_word(code, transition + 122, 0x3439); // move.w $1ffd4,d2
    require_long(code, transition + 124, 0x0001ffd4);
    require_word(code, transition + 128, 0xb079); // cmp.w $1ffc8,d0
    require_long(code, transition + 130, 0x0001ffc8);
    require_word(code, transition + 136, 0xb279); // cmp.w $1ffce,d1
    require_long(code, transition + 138, 0x0001ffce);
    require_word(code, transition + 144, 0xb479); // cmp.w $1ffd4,d2
    require_long(code, transition + 146, 0x0001ffd4);
    // The original branch supplies three work addresses to -$1a4, then
    // clears the active byte, restores the saved display word, and returns.
    require_word(code, transition + 152, 0x41f9); // lea $12e12,a0
    require_long(code, transition + 154, 0x00012e12);
    require_word(code, transition + 158, 0x43f9); // lea $1ffda,a1
    require_long(code, transition + 160, 0x0001ffda);
    require_word(code, transition + 164, 0x45f9); // lea $1ffe6,a2
    require_long(code, transition + 166, 0x0001ffe6);
    require_word(code, transition + 170, 0x23ca); // move.l a2,$2008e
    require_long(code, transition + 172, 0x0002008e);
    require_word(code, transition + 182, 0x4eae); // jsr -$1a4(a6)
    require_word(code, transition + 184, 0xfe5c);
    require_word(code, transition + 186, 0x13fc); // move.b #0,$202c6
    require_word(code, transition + 188, 0x0000);
    require_long(code, transition + 190, 0x000202c6);
    require_word(code, transition + 194, 0x41f9); // lea $12e12,a0
    require_long(code, transition + 196, 0x00012e12);
    require_word(code, transition + 200, 0x43f9); // lea $1ed24,a1
    require_long(code, transition + 202, 0x0001ed24);
    require_word(code, transition + 206, 0x7010); // moveq #16,d0
    require_word(code, transition + 214, 0x4eae); // jsr -$c0(a6)
    require_word(code, transition + 216, 0xff40);
    require_word(code, transition + 218, 0x301f); // move.w (a7)+,d0
    require_word(code, transition + 220, 0x33c0); // move.w d0,$202b8
    require_long(code, transition + 222, 0x000202b8);
    require_word(code, transition + 226, 0x4e75); // rts

    // The stage continues immediately after the transition's return. Keep
    // this as literal control-flow evidence rather than assigning title-menu
    // or gameplay semantics to the control word or its response values.
    constexpr std::size_t post_transition = 0x358; // $4077e - $40426
    require_word(code, post_transition, 0x33fc); // move.w #0,$407e6
    require_word(code, post_transition + 2, 0x0000);
    require_long(code, post_transition + 4, 0x000407e6);
    require_word(code, post_transition + 8, 0x4eb9); // jsr $3f7a8
    require_long(code, post_transition + 10, 0x0003f7a8);
    require_word(code, post_transition + 14, 0x3039); // move.w $407e6,d0
    require_long(code, post_transition + 16, 0x000407e6);
    require_word(code, post_transition + 20, 0x3f00); // move.w d0,-(a7)
    require_word(code, post_transition + 22, 0x4eb9); // jsr $1f9a4
    require_long(code, post_transition + 24, 0x0001f9a4);
    // Preserve the opaque two-word instruction at +34 exactly; its inputs
    // have not yet been recovered, so it is not given a guessed meaning.
    require_word(code, post_transition + 34, 0x0000);
    require_word(code, post_transition + 36, 0x3017);
    require_word(code, post_transition + 38, 0x4eb9); // jsr $1fe7a
    require_long(code, post_transition + 40, 0x0001fe7a);
    require_word(code, post_transition + 44, 0x301f); // move.w (a7)+,d0
    require_word(code, post_transition + 46, 0x7202); // moveq #2,d1
    require_word(code, post_transition + 48, 0x4eb9); // jsr $3fbf8
    require_long(code, post_transition + 50, 0x0003fbf8);
    require_word(code, post_transition + 54, 0x4eb9); // jsr $1f238
    require_long(code, post_transition + 56, 0x0001f238);
    require_word(code, post_transition + 60, 0xb03c); // cmp.w #$1b,d0
    require_word(code, post_transition + 62, 0x001b);
    require_word(code, post_transition + 64, 0x6724); // beq.b $407e2
    require_word(code, post_transition + 66, 0x3239); // move.w $407e6,d1
    require_long(code, post_transition + 68, 0x000407e6);
    require_word(code, post_transition + 72, 0xb03c); // cmp.w #$20,d0
    require_word(code, post_transition + 74, 0x0020);
    require_word(code, post_transition + 78, 0xb03c); // cmp.w #$2e,d0
    require_word(code, post_transition + 80, 0x002e);
    require_word(code, post_transition + 84, 0x0c00); // cmpi.b #$2c,d0
    require_word(code, post_transition + 86, 0x002c);
    require_word(code, post_transition + 90, 0x5501); // subq.b #2,d1
    require_word(code, post_transition + 92, 0x5201); // addq.b #1,d1
    require_word(code, post_transition + 94, 0x33c1); // move.w d1,$407e6
    require_long(code, post_transition + 96, 0x000407e6);
    require_word(code, post_transition + 102, 0x4e75); // rts

    // The third helper reached above is a real control-flow boundary after
    // the post-transition loop. Its arithmetic and byte write are raw facts.
    // Its terminal absolute JMP stays in this title-stage image, so this path
    // has not reached the separately loaded main stage.
    constexpr std::uint32_t selector_address = 0x1fe7a;
    const auto selector = stage_code(selector_address, 46);
    require_word(selector, 0, 0x0280); // andi.l #$ffff,d0
    require_long(selector, 2, 0x0000ffff);
    require_word(selector, 6, 0x80fc); // divu.w #$64,d0
    require_word(selector, 8, 0x0064);
    require_word(selector, 10, 0x6100); // bsr.w $1feaa
    require_word(selector, 12, 0x0022);
    require_word(selector, 14, 0x0280); // andi.l #$ffff,d0
    require_long(selector, 16, 0x0000ffff);
    require_word(selector, 20, 0x80fc); // divu.w #$a,d0
    require_word(selector, 22, 0x000a);
    require_word(selector, 24, 0x6100); // bsr.w $1feaa
    require_word(selector, 26, 0x0014);
    require_word(selector, 28, 0x0640); // addi.w #$30,d0
    require_word(selector, 30, 0x0030);
    require_word(selector, 32, 0x13fc); // move.b #0,$1fe54
    require_word(selector, 34, 0x0000);
    require_long(selector, 36, 0x0001fe54);
    require_word(selector, 40, 0x4ef9); // jmp $1fbe6
    require_long(selector, 42, 0x0001fbe6);
    const auto dispatch = stage_code(0x1fbe6, 6);
    require_word(dispatch, 0, 0x4a39); // tst.b $1f98c
    require_long(dispatch, 2, 0x0001f98c);
    // The selector destination is not a generic return trampoline. The
    // original tests a signed state byte: zero goes to $1fc22, positive goes
    // to $1fc9e, and the negative fall-through preserves D0/D5, calls the
    // $1fc24 helper, then conditionally invokes $3fbf8 with literal D0/D1.
    // Record that concrete call boundary without assigning a UI/gameplay
    // meaning to its service or state bytes.
    const auto dispatch_flow = stage_code(0x1fbe6, 60);
    require_word(dispatch_flow, 6, 0x6734); // beq.b $1fc22
    require_word(dispatch_flow, 8, 0x6a00); // bpl.w $1fc9e
    require_word(dispatch_flow, 10, 0x00ac);
    require_word(dispatch_flow, 12, 0x2f00); // move.l d0,-(a7)
    require_word(dispatch_flow, 14, 0x6100); // bsr.w $1fc24
    require_word(dispatch_flow, 16, 0x002c);
    require_word(dispatch_flow, 18, 0x2f05); // move.l d5,-(a7)
    require_word(dispatch_flow, 20, 0xb03c); // cmpi.w #$20,d0
    require_word(dispatch_flow, 22, 0x0020);
    require_word(dispatch_flow, 24, 0x6712); // beq.b bypass service
    require_word(dispatch_flow, 26, 0x7013); // moveq #$13,d0
    require_word(dispatch_flow, 28, 0x720c); // moveq #$0c,d1
    require_word(dispatch_flow, 34, 0x4eb9); // jsr $3fbf8
    require_long(dispatch_flow, 36, 0x0003fbf8);
    require_word(dispatch_flow, 30, 0x2f08); // move.l a0,-(a7)
    require_word(dispatch_flow, 32, 0x2f09); // move.l a1,-(a7)
    require_word(dispatch_flow, 40, 0x225f); // movea.l (a7)+,a1
    require_word(dispatch_flow, 42, 0x205f); // movea.l (a7)+,a0
    require_word(dispatch_flow, 44, 0x203c); // move.l #$4e20,d0
    require_long(dispatch_flow, 46, 0x00004e20);
    require_word(dispatch_flow, 50, 0x5380); // subq.l #1,d0
    require_word(dispatch_flow, 52, 0x66fc); // bne.b delay loop
    require_word(dispatch_flow, 54, 0x2a1f); // move.l (a7)+,d5
    require_word(dispatch_flow, 56, 0x201f); // move.l (a7)+,d0
    require_word(dispatch_flow, 58, 0x4e75); // rts
    const auto zero_branch = stage_code(0x1fc22, 6);
    require_word(zero_branch, 0, 0x4a39); // tst.b $1f98e
    require_long(zero_branch, 2, 0x0001f98e);
    require_word(stage_code(0x1fc28, 4), 0, 0x6600); // bne.w $1fd0a
    require_word(stage_code(0x1fc28, 4), 2, 0x00e0);
    // 68000 Bcc.W displacement is relative to the extension word: BPL.W
    // lands at $1fc9c, the sibling route's tst.b (not its preceding RTS).
    const auto positive = stage_code(0x1fc9c, 108);
    require_word(positive, 0, 0x4a39); // tst.b $1f98e
    require_long(positive, 2, 0x0001f98e);
    require_word(positive, 6, 0x6600); // bne.w $1fd7a
    require_word(positive, 8, 0x00d6);
    const auto positive_clear = stage_code(0x1fca6, 100);
    require_word(positive_clear, 6, 0x7c28); // moveq #$28,d6
    require_word(positive_clear, 8, 0x3e3c); // move.w #$1f40,d7
    require_word(positive_clear, 10, 0x1f40);
    require_word(positive_clear, 22, 0x2079); // movea.l $1f99c,a0
    require_long(positive_clear, 24, 0x0001f99c);
    require_word(positive_clear, 30, 0x2879); // movea.l $1f974,a4
    require_long(positive_clear, 32, 0x0001f974);
    require_word(positive_clear, 36, 0x2479); // movea.l $1f96c,a2
    require_long(positive_clear, 38, 0x0001f96c);
    require_word(positive_clear, 42, 0x7a07); // moveq #7,d5
    require_word(positive_clear, 52, 0x7803); // moveq #3,d4
    require_word(positive_clear, 82, 0x51cd); // dbra d5
    require_word(positive_clear, 84, 0xffd8);
    require_word(positive_clear, 86, 0x52b9); // addq.l #1,$1f974
    require_long(positive_clear, 88, 0x0001f974);
    const auto positive_set = stage_code(0x1fd7a, 102);
    require_word(positive_set, 6, 0x2c39); // move.l $1f994,d6
    require_long(positive_set, 8, 0x0001f994);
    require_word(positive_set, 12, 0x2e39); // move.l $1f998,d7
    require_long(positive_set, 14, 0x0001f998);
    require_word(positive_set, 28, 0x2079); // movea.l $1f99c,a0
    require_long(positive_set, 30, 0x0001f99c);
    require_word(positive_set, 36, 0x2879); // movea.l $1f974,a4
    require_long(positive_set, 38, 0x0001f974);
    require_word(positive_set, 42, 0x2479); // movea.l $1f96c,a2
    require_long(positive_set, 44, 0x0001f96c);
    require_word(positive_set, 48, 0x7a07); // moveq #7,d5
    require_word(positive_set, 58, 0x7803); // moveq #3,d4
    const auto zero_clear = stage_code(0x1fc2c, 108);
    require_word(zero_clear, 6, 0x7c28); // moveq #$28,d6
    require_word(zero_clear, 8, 0x3e3c); // move.w #$1f40,d7
    require_word(zero_clear, 10, 0x1f40);
    require_word(zero_clear, 22, 0x2079); // movea.l $1f99c,a0
    require_long(zero_clear, 24, 0x0001f99c);
    require_word(zero_clear, 30, 0x2879); // movea.l $1f974,a4
    require_long(zero_clear, 32, 0x0001f974);
    require_word(zero_clear, 36, 0x2479); // movea.l $1f96c,a2
    require_long(zero_clear, 38, 0x0001f96c);
    require_word(zero_clear, 42, 0x2279); // movea.l $1f970,a1
    require_long(zero_clear, 44, 0x0001f970);
    require_word(zero_clear, 48, 0x7a07); // moveq #7,d5
    require_word(zero_clear, 58, 0x7803); // moveq #3,d4
    require_word(zero_clear, 92, 0x2039); // move.l $1f9a0,d0
    require_long(zero_clear, 94, 0x0001f9a0);
    require_word(zero_clear, 98, 0xd1b9); // add.l d0,$1f974
    require_long(zero_clear, 100, 0x0001f974);
    const auto zero_set = stage_code(0x1fd0a, 104);
    require_word(zero_set, 6, 0x2c39); // move.l $1f994,d6
    require_long(zero_set, 8, 0x0001f994);
    require_word(zero_set, 12, 0x2e39); // move.l $1f998,d7
    require_long(zero_set, 14, 0x0001f998);
    require_word(zero_set, 28, 0x2079); // movea.l $1f99c,a0
    require_long(zero_set, 30, 0x0001f99c);
    require_word(zero_set, 36, 0x2879); // movea.l $1f974,a4
    require_long(zero_set, 38, 0x0001f974);
    require_word(zero_set, 42, 0x2479); // movea.l $1f96c,a2
    require_long(zero_set, 44, 0x0001f96c);
    require_word(zero_set, 48, 0x2279); // movea.l $1f970,a1
    require_long(zero_set, 50, 0x0001f970);
    require_word(zero_set, 54, 0x7a07); // moveq #7,d5
    require_word(zero_set, 64, 0x7803); // moveq #3,d4
    require_word(zero_set, 98, 0x52b9); // addq.l #1,$1f974
    require_long(zero_set, 100, 0x0001f974);

    // These are the first recovered tails that leave the title image for the
    // bootstrap. Each saves the title entry's controller pointer, selects a
    // raw bootstrap profile, then resets through $12800. Do not attach names
    // to the routes: the original code here proves only their handoff values.
    const auto require_exit = [&](const std::uint32_t address, const std::size_t handoff_offset,
                                  const std::uint16_t profile) {
        const auto exit = stage_code(address, handoff_offset + 30);
        require_word(exit, handoff_offset, 0x2039); // move.l $206a0,d0
        require_long(exit, handoff_offset + 2, 0x000206a0);
        require_word(exit, handoff_offset + 6, 0x23c0); // move.l d0,$12ff8
        require_long(exit, handoff_offset + 8, 0x00012ff8);
        require_word(exit, handoff_offset + 12, 0x23fc); // move.l #profile,$12ffc
        require_long(exit, handoff_offset + 14, profile);
        require_long(exit, handoff_offset + 18, 0x00012ffc);
        require_word(exit, handoff_offset + 22, 0x4ef9); // jmp $12800
        require_long(exit, handoff_offset + 24, 0x00012800);
    };
    require_exit(0x37f56, 40, 2);
    require_exit(0x38038, 14, 4);
    require_exit(0x38068, 14, 3);

    // $12800 recreates its stack/Exec state then jumps into the same bootstrap
    // dispatcher used by the boot block. Table values 3 and 4 directly name
    // profile zero, while value 2 branches to it. Profile zero is the raw
    // main-stage read already recovered in DeuterosAmigaLoadPlan.
    const auto bootstrap_entry = bootstrap_code(0x12800, 34);
    require_word(bootstrap_entry, 0, 0x2e7c); // movea.l #$12dca,a7
    require_long(bootstrap_entry, 2, 0x00012dca);
    require_word(bootstrap_entry, 28, 0x4ef9); // jmp $12a4e
    require_long(bootstrap_entry, 30, 0x00012a4e);
    const auto profile_table = bootstrap_code(0x12a36, 24);
    require_long(profile_table, 0, 0x00012b1c); // profile zero
    require_long(profile_table, 4, 0x00012b30); // profile one
    require_long(profile_table, 8, 0x00012b44); // profile two
    require_long(profile_table, 12, 0x00012b1c); // profile three
    require_long(profile_table, 16, 0x00012b1c); // profile four
    require_long(profile_table, 20, 0x00012b46); // profile five
    const auto profile_two = bootstrap_code(0x12b44, 2);
    require_word(profile_two, 0, 0x60d6); // bra.b $12b1c
    const auto profile_zero = bootstrap_code(0x12b1c, 20);
    require_word(profile_zero, 0, 0x223c); // move.l #$20000,d1
    require_long(profile_zero, 2, plan.main_stage.destination);
    require_word(profile_zero, 6, 0x203c); // move.l #$4200,d0
    require_long(profile_zero, 8, plan.main_stage.length);
    require_word(profile_zero, 12, 0x243c); // move.l #$4,d2
    require_long(profile_zero, 14, 4);
    require_word(profile_zero, 18, 0x4e75);
    const auto profile_five = bootstrap_code(0x12b46, 6);
    require_word(profile_five, 0, 0x6100); // bsr.w $12932
    require_word(profile_five, 2, 0xfdea);
    // The helper itself has a straight-line, local prefix. Stop exactly at
    // its first library vector; neither its return nor that vector's effect
    // is part of this preservation profile.
    const auto profile_five_helper = bootstrap_code(0x12932, 34);
    require_word(profile_five_helper, 0, 0x2279); // movea.l $12822,a1
    require_long(profile_five_helper, 2, 0x00012822);
    require_word(profile_five_helper, 6, 0x237c); // move.l #1,$24(a1)
    require_long(profile_five_helper, 8, 1);
    require_word(profile_five_helper, 12, 0x0024);
    require_word(profile_five_helper, 14, 0x337c); // move.w #9,$1c(a1)
    require_word(profile_five_helper, 16, 9);
    require_word(profile_five_helper, 18, 0x001c);
    require_word(profile_five_helper, 20, 0x137c); // move.b #0,$1e(a1)
    require_word(profile_five_helper, 22, 0);
    require_word(profile_five_helper, 24, 0x001e);
    require_word(profile_five_helper, 26, 0x2c78); // movea.l $4,a6
    require_word(profile_five_helper, 28, 4);
    require_word(profile_five_helper, 30, 0x4eae); // jsr -$1c8(a6)
    require_word(profile_five_helper, 32, 0xfe38);

    DeuterosAmigaTitleStageProfile result{stage.entry_address, 0x4040e, 5, 0x3717e, 0x38092, 0x101,
        0x19d52, 1, 0x40574, 0x222c0, 0x23e4e, 0x40410, 0xea60, 0x4069a,
        0x22d34, 0x11, 0x40410,
        0x202c6, 0x202b8, 0x1ed24, 0x40678, 16, 0x0eee, 0x12fec,
        static_cast<std::int16_t>(-0xc0), static_cast<std::int16_t>(-0x1a4),
        0x12e12, 0x1ffda, 0x1ffe6, 0x2008e, 0x1ffc8, 0x1ffce, 0x1ffd4,
        0x4077c,
        0x407e6, 0, 0x3f7a8, 0x1f9a4, 0x1fe7a, 0x1f238,
        0x1b, 0x20, 0x2e, 0x2c, 0x407e4,
        selector_address, 0x0000ffff, 0x0064, 0x000a, 0x0030,
        0x1fe54, 0x1fbe6,
        0x1f98c, 0x1fc22, 0x1fc9c,
        0x1f98e, 0x1fc2c, 0x1fd0a, 0x1f99c, 0x1f974, 0x1f970, 0x1f96c,
        0x1f9a0, 0x28, 0x1f40, 8, 4,
        0x3fbf8, 0x13, 0x0c, 0x20, 0x4e20,
        true, true, 0x1fc20,
        0x37f56, 2, 0x38038, 4, 0x38068, 3,
        0x12800, 0x12ffc, 0x12a36, 0, plan.main_stage.entry_address,
        {0x12b1c, 0x12b30, 0x12b44, 0x12b1c, 0x12b1c, 0x12b46},
        0x12b46, 0x12932,
        0x12822, 0x24, 1, 0x1c, 9, 0x1e, 0, 4, static_cast<std::int16_t>(-0x1c8)};
    result.initialization_stack_address = 0x40b62;
    result.initialization_exec_base_address = 4;
    result.initialization_exec_vectors = {static_cast<std::int16_t>(-0x96), static_cast<std::int16_t>(-0x9c)};
    result.initialization_exec_allocation_size = 0x7fff0;
    result.initialization_internal_calls = {0x1ed80, 0x1f172, 0x1f182, 0x1ef74,
        0x206d4, 0x206be, 0x403e6, 0x403f4, 0x204c8, 0x389e2, 0x37180};
    result.initialization_copy_source_address = 0x1f168;
    result.initialization_copy_destinations = {0x1f974, 0x410d8};
    result.initialization_custom_base_address = 0xdff000;
    result.initialization_custom_offsets = custom_offsets;
    result.initialization_custom_values = custom_values;
    result.initialization_mode_five_call_address = 0x36a8c;
    result.initialization_normal_call_address = 0x1fb9a;
    return result;
}

DeuterosAmigaTitleTransitionPrefix execute_deuteros_amiga_title_transition_prefix(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    const std::uint16_t input_display_word) {
    const auto& stage = plan.title_stage;
    constexpr std::uint32_t entry_address = 0x4069a;
    constexpr std::uint32_t source_palette_address = 0x1ed24;
    constexpr std::uint32_t work_palette_address = 0x40678;
    constexpr std::uint32_t saved_display_word_address = 0x202b8;
    constexpr std::uint32_t active_flag_address = 0x202c6;
    constexpr std::string_view title_stage_hash =
        "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03";
    constexpr std::array<std::uint8_t, 72> prefix_bytes{{
        0x13, 0xfc, 0x00, 0x01, 0x00, 0x02, 0x02, 0xc6,
        0x41, 0xf9, 0x00, 0x01, 0xed, 0x24, 0x43, 0xf9,
        0x00, 0x04, 0x06, 0x78, 0x7e, 0x0f, 0x30, 0x18,
        0x02, 0x40, 0x0e, 0xee, 0xe2, 0x48, 0x32, 0xc0,
        0x51, 0xcf, 0xff, 0xf4, 0x30, 0x39, 0x00, 0x02,
        0x02, 0xb8, 0x3f, 0x00, 0x13, 0xfc, 0x00, 0x00,
        0x00, 0x02, 0x02, 0xb8, 0x41, 0xf9, 0x00, 0x01,
        0x2e, 0x12, 0x43, 0xf9, 0x00, 0x04, 0x06, 0x78,
        0x70, 0x10, 0x2c, 0x79, 0x00, 0x01, 0x2f, 0xec,
    }};
    constexpr std::string_view prefix_hash =
        "fda01edebbc2e99372cb22a858269202343f98d31bee1e473f751048666759ca";
    constexpr std::string_view source_hash =
        "6920018538a18ca186ef36431678de4fc8f7bc68ac6b481e82086dbda54ff1e1";
    if (stage.length == 0 || entry_address < stage.destination
        || source_palette_address < stage.destination
        || entry_address - stage.destination > stage.length
        || prefix_bytes.size() > stage.length - (entry_address - stage.destination)
        || 32U > stage.length - (source_palette_address - stage.destination)) {
        throw std::runtime_error("Deuteros title transition prefix lies outside original stage");
    }
    const auto stage_bytes = disk.bytes(stage.disk_offset, stage.length);
    const auto prefix = stage_bytes.subspan(entry_address - stage.destination, prefix_bytes.size());
    const auto source = stage_bytes.subspan(source_palette_address - stage.destination, 32);
    if (to_hex(sha256(stage_bytes)) != title_stage_hash
        || !std::equal(prefix_bytes.begin(), prefix_bytes.end(), prefix.begin())
        || to_hex(sha256(prefix)) != prefix_hash
        || to_hex(sha256(source)) != source_hash) {
        throw std::runtime_error("Unsupported Deuteros title transition prefix");
    }
    DeuterosAmigaTitleTransitionPrefix result;
    result.entry_address = entry_address;
    result.active_flag_address = active_flag_address;
    result.active_flag_value = 1;
    result.saved_display_word_address = saved_display_word_address;
    result.saved_display_word = input_display_word;
    result.cleared_display_word = 0;
    result.source_palette_address = source_palette_address;
    result.work_palette_address = work_palette_address;
    for (std::size_t index = 0; index < result.work_palette_words.size(); ++index) {
        result.work_palette_words[index] = static_cast<std::uint16_t>(
            (big16(source, index * 2U) & 0x0eeeU) >> 1U);
    }
    result.graphics_library_base_address = 0x12fec;
    result.graphics_library_vector = -0xc0;
    result.graphics_source_address = 0x12e12;
    result.graphics_destination_address = work_palette_address;
    result.graphics_word_count = 16;
    return result;
}

DeuterosAmigaTitleTimerGate evaluate_deuteros_amiga_title_timer_gate(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    const std::uint32_t elapsed_counter, const std::uint16_t inhibit_word) {
    const auto& stage = plan.title_stage;
    constexpr std::uint32_t entry_address = 0x4059e;
    constexpr std::uint32_t skipped_target_address = 0x405c6;
    constexpr std::uint32_t transition_address = 0x4069a;
    constexpr std::string_view title_stage_hash =
        "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03";
    constexpr std::array<std::uint8_t, 40> gate_bytes{{
        0x20, 0x39, 0x00, 0x04, 0x04, 0x10, 0xb0, 0xbc,
        0x00, 0x00, 0xea, 0x60, 0x65, 0x1a, 0x0c, 0x79,
        0x00, 0x11, 0x00, 0x02, 0x2d, 0x34, 0x67, 0x10,
        0x4e, 0xb9, 0x00, 0x04, 0x06, 0x9a, 0x23, 0xfc,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x10,
    }};
    constexpr std::string_view gate_hash =
        "47c56a2ad892d973cc967bca2a8c3b34338ffbdbff3b1b57ecef63cc6d8d7200";
    if (stage.length == 0 || entry_address < stage.destination
        || entry_address - stage.destination > stage.length
        || gate_bytes.size() > stage.length - (entry_address - stage.destination)) {
        throw std::runtime_error("Deuteros title timer gate lies outside original stage");
    }
    const auto stage_bytes = disk.bytes(stage.disk_offset, stage.length);
    const auto gate = stage_bytes.subspan(entry_address - stage.destination, gate_bytes.size());
    if (to_hex(sha256(stage_bytes)) != title_stage_hash
        || !std::equal(gate_bytes.begin(), gate_bytes.end(), gate.begin())
        || to_hex(sha256(gate)) != gate_hash) {
        throw std::runtime_error("Unsupported Deuteros title timer gate");
    }
    DeuterosAmigaTitleTimerGate result;
    result.entry_address = entry_address;
    result.elapsed_counter_address = 0x40410;
    result.elapsed_threshold = 0xea60;
    result.inhibit_word_address = 0x22d34;
    result.inhibit_word_value = 0x0011;
    result.skipped_target_address = skipped_target_address;
    result.transition_address = transition_address;
    result.dispatches_transition = elapsed_counter >= result.elapsed_threshold
        && inhibit_word != result.inhibit_word_value;
    result.counter_reset_after_transition_return = result.dispatches_transition;
    return result;
}

DeuterosAmigaTitleZeroResponseLoop evaluate_deuteros_amiga_title_zero_response_loop(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    const std::span<const std::uint8_t> helper_response_low_bytes) {
    const auto& stage = plan.title_stage;
    constexpr std::uint32_t entry_address = 0x405c6;
    constexpr std::uint32_t response_loop_address = 0x40638;
    constexpr std::uint32_t state_word_address = 0x1bf36;
    constexpr std::uint32_t helper_address = 0x1f238;
    constexpr std::uint32_t custom_address = 0xdff180;
    constexpr std::uint32_t return_loop_address = 0x40574;
    constexpr std::string_view title_stage_hash =
        "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03";
    constexpr std::array<std::uint8_t, 10> common_gate_bytes{{
        0x4a, 0x39, 0x00, 0x01, 0xbf, 0x36, 0x67, 0x00, 0x00, 0x6a,
    }};
    constexpr std::string_view common_gate_hash =
        "68ccbd8edf32800e43fe55c47356e162896b8500b01d2e9fd461191ba1760736";
    constexpr std::string_view response_loop_hash =
        "b47192ea229873ef1ae47f841d044393bfd3e7e1a7fc0ca92308a555c2eb84d0";
    constexpr std::string_view initial_state_hash =
        "96a296d224f285c67bee93c30f8a309157f0daa35dc5b87e410b78630a09cfc7";
    if (stage.length == 0 || entry_address < stage.destination
        || response_loop_address < stage.destination || state_word_address < stage.destination
        || entry_address - stage.destination > stage.length
        || common_gate_bytes.size() > stage.length - (entry_address - stage.destination)
        || 60U > stage.length - (response_loop_address - stage.destination)
        || 2U > stage.length - (state_word_address - stage.destination)) {
        throw std::runtime_error("Deuteros title response loop lies outside original stage");
    }
    const auto stage_bytes = disk.bytes(stage.disk_offset, stage.length);
    const auto common_gate = stage_bytes.subspan(
        entry_address - stage.destination, common_gate_bytes.size());
    const auto response_loop = stage_bytes.subspan(response_loop_address - stage.destination, 60);
    const auto initial_state = stage_bytes.subspan(state_word_address - stage.destination, 2);
    if (to_hex(sha256(stage_bytes)) != title_stage_hash
        || !std::equal(common_gate_bytes.begin(), common_gate_bytes.end(), common_gate.begin())
        || to_hex(sha256(common_gate)) != common_gate_hash
        || to_hex(sha256(response_loop)) != response_loop_hash
        || to_hex(sha256(initial_state)) != initial_state_hash
        || big16(initial_state, 0) != 0) {
        throw std::runtime_error("Unsupported Deuteros title zero response loop");
    }
    if (helper_response_low_bytes.empty()) {
        throw std::runtime_error("Deuteros title response loop needs original helper response");
    }
    DeuterosAmigaTitleZeroResponseLoop result;
    result.entry_address = entry_address;
    result.state_word_address = state_word_address;
    result.initial_state_word = 0;
    result.final_state_word = 0;
    result.helper_address = helper_address;
    result.response_match_value = 0x43;
    result.custom_address = custom_address;
    result.return_loop_address = return_loop_address;
    if (helper_response_low_bytes.front() != result.response_match_value) {
        if (helper_response_low_bytes.size() != 1) {
            throw std::runtime_error("Unexpected extra Deuteros title helper response");
        }
        return result;
    }
    result.final_state_word ^= 0x0101;
    for (std::size_t index = 1; index < helper_response_low_bytes.size(); ++index) {
        // Starting from the validated zero route, EOR.W #$0101 makes the
        // state nonzero and the original chooses this literal custom word.
        result.custom_write_words.push_back(0x0f00);
        if (helper_response_low_bytes[index] == result.response_match_value) {
            if (index + 1 != helper_response_low_bytes.size()) {
                throw std::runtime_error("Unexpected trailing Deuteros title helper response");
            }
            return result;
        }
    }
    throw std::runtime_error("Deuteros title response loop lacks terminating helper response");
}

DeuterosAmigaTitlePostTransitionResponseLoop
evaluate_deuteros_amiga_title_post_transition_response_loop(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    const std::span<const std::uint8_t> helper_response_low_bytes) {
    // $4077e clears the local word, then unresolved calls eventually yield a
    // low-byte D0 response to the fully local $407ba feedback tail. This
    // evaluator follows only that tail; it never invokes the helpers.
    constexpr std::uint32_t entry_address = 0x4077e;
    constexpr std::uint32_t feedback_tail_address = 0x407ba;
    constexpr std::uint32_t control_word_address = 0x407e6;
    constexpr std::uint32_t helper_address = 0x1f238;
    constexpr std::uint32_t helper_loop_address = 0x4078c;
    constexpr std::uint32_t return_address = 0x407e4;
    constexpr std::size_t feedback_tail_bytes = 44;
    constexpr std::string_view title_stage_hash =
        "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03";
    constexpr std::string_view feedback_tail_hash =
        "b4212844a9f0fb4008caad00950e613b70581a5552cacabc253ea0966ed16df3";
    const auto& stage = plan.title_stage;
    if (stage.length == 0 || entry_address < stage.destination
        || feedback_tail_address < stage.destination || control_word_address < stage.destination
        || feedback_tail_address - stage.destination > stage.length
        || feedback_tail_bytes > stage.length - (feedback_tail_address - stage.destination)
        || 2U > stage.length - (control_word_address - stage.destination)) {
        throw std::runtime_error("Deuteros post-transition response tail lies outside original stage");
    }
    const auto stage_bytes = disk.bytes(stage.disk_offset, stage.length);
    const auto tail = stage_bytes.subspan(feedback_tail_address - stage.destination,
        feedback_tail_bytes);
    if (to_hex(sha256(stage_bytes)) != title_stage_hash
        || to_hex(sha256(tail)) != feedback_tail_hash) {
        throw std::runtime_error("Unsupported Deuteros post-transition response tail");
    }
    if (helper_response_low_bytes.empty()) {
        throw std::runtime_error("Deuteros post-transition response tail needs helper response");
    }
    DeuterosAmigaTitlePostTransitionResponseLoop result;
    result.entry_address = entry_address;
    result.feedback_tail_address = feedback_tail_address;
    result.control_word_address = control_word_address;
    result.helper_address = helper_address;
    result.return_response = 0x1b;
    result.loop_response = 0x20;
    result.increment_response = 0x2e;
    result.decrement_response = 0x2c;
    result.helper_loop_address = helper_loop_address;
    result.return_address = return_address;
    std::uint8_t control_low_byte = 0;
    for (std::size_t index = 0; index < helper_response_low_bytes.size(); ++index) {
        const auto response = helper_response_low_bytes[index];
        if (response == result.return_response) {
            if (index + 1 != helper_response_low_bytes.size()) {
                throw std::runtime_error("Unexpected trailing Deuteros post-transition response");
            }
            result.final_control_word = control_low_byte;
            return result;
        }
        if (response == result.increment_response) {
            ++control_low_byte;
            result.control_low_byte_writes.push_back(control_low_byte);
        } else if (response == result.decrement_response) {
            control_low_byte = static_cast<std::uint8_t>(control_low_byte - 1U);
            result.control_low_byte_writes.push_back(control_low_byte);
        }
        // $20 and every unmatched byte branch back to the unresolved helper
        // with no local write, so the caller supplies the next response.
    }
    throw std::runtime_error("Deuteros post-transition response tail lacks return response");
}

DeuterosAmigaTitleEntryPrefix execute_deuteros_amiga_title_entry_prefix(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    const std::uint16_t incoming_profile) {
    if (incoming_profile != 1) {
        throw std::runtime_error("Unsupported Deuteros title entry profile");
    }
    constexpr std::string_view adf_hash =
        "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38";
    constexpr std::array<std::uint8_t, 14> bootstrap_return{{
        0x22, 0x79, 0x00, 0x01, 0x28, 0x22, 0x30, 0x39,
        0x00, 0x01, 0x2a, 0x34, 0x4e, 0x75,
    }};
    constexpr std::array<std::uint8_t, 18> mode_prefix{{
        0x23, 0xc9, 0x00, 0x02, 0x06, 0xa0, 0x33, 0xc0,
        0x00, 0x04, 0x04, 0x0e, 0xb0, 0x3c, 0x00, 0x05,
        0x66, 0x10,
    }};
    constexpr std::array<std::uint8_t, 8> normal_prefix{{
        0x13, 0xfc, 0x00, 0x01, 0x00, 0x01, 0x9d, 0x52,
    }};
    constexpr std::array<std::uint8_t, 16> exec_boundary{{
        0x2e, 0x7c, 0x00, 0x04, 0x0b, 0x62, 0x2c, 0x78,
        0x00, 0x04, 0x4e, 0xae, 0xff, 0x6a, 0x20, 0x3c,
    }};
    if (to_hex(sha256(disk.bytes(0, AmigaAdf::standard_size))) != adf_hash
        || plan.bootstrap_loader.destination > 0x12b0e
        || 0x12b0e - plan.bootstrap_loader.destination > plan.bootstrap_loader.length
        || bootstrap_return.size() > plan.bootstrap_loader.length
            - (0x12b0e - plan.bootstrap_loader.destination)
        || plan.title_stage.destination > 0x40426
        || 0x40450 - plan.title_stage.destination > plan.title_stage.length
        || exec_boundary.size() > plan.title_stage.length - (0x40450 - plan.title_stage.destination)) {
        throw std::runtime_error("Deuteros title entry prefix lies outside original stages");
    }
    const auto bootstrap = disk.bytes(plan.bootstrap_loader.disk_offset
        + 0x12b0e - plan.bootstrap_loader.destination, bootstrap_return.size());
    const auto mode = disk.bytes(plan.title_stage.disk_offset
        + 0x40426 - plan.title_stage.destination, mode_prefix.size());
    const auto normal = disk.bytes(plan.title_stage.disk_offset
        + 0x40448 - plan.title_stage.destination, normal_prefix.size());
    const auto exec = disk.bytes(plan.title_stage.disk_offset
        + 0x40450 - plan.title_stage.destination, exec_boundary.size());
    if (!std::equal(bootstrap_return.begin(), bootstrap_return.end(), bootstrap.begin())
        || to_hex(sha256(bootstrap)) != "858d0a08e8d6fe8200fb71a0866731feabffcadc232bfdeff5be669446bae0fd"
        || !std::equal(mode_prefix.begin(), mode_prefix.end(), mode.begin())
        || to_hex(sha256(mode)) != "833374022042225f1bfeeedd56c05d7011168531fa121494cef04174453e5387"
        || !std::equal(normal_prefix.begin(), normal_prefix.end(), normal.begin())
        || to_hex(sha256(normal)) != "8d15b73f389c05fc214b9440c0a0b77df33782c6400d455cef96f338aa5f1211"
        || !std::equal(exec_boundary.begin(), exec_boundary.end(), exec.begin())
        || to_hex(sha256(exec)) != "f0c847a4d443e26fc08f6c6864afeca3b33da514f8708f76f2f05314a4c88067") {
        throw std::runtime_error("Unsupported Deuteros title entry prefix");
    }
    return {1, 0x206a0, 0x4040e, 1, 0x19d52, 1, 0x40450};
}

DeuterosAmigaTitleEntryModeFivePrefix execute_deuteros_amiga_title_entry_mode_five_prefix(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    const std::uint16_t incoming_profile) {
    if (static_cast<std::uint8_t>(incoming_profile) != 5) {
        throw std::runtime_error("Unsupported Deuteros title entry low byte");
    }
    constexpr std::string_view stage_hash =
        "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03";
    constexpr std::array<std::uint8_t, 16> mode_five{{
        0x13, 0xc0, 0x00, 0x03, 0x71, 0x7e, 0x33, 0xfc,
        0x01, 0x01, 0x00, 0x03, 0x80, 0x92, 0x60, 0x08,
    }};
    const auto& stage = plan.title_stage;
    constexpr std::uint32_t branch_entry = 0x40426;
    constexpr std::uint32_t entry = 0x40438;
    if (branch_entry < stage.destination || branch_entry - stage.destination > stage.length
        || 42U > stage.length - (branch_entry - stage.destination)
        || entry < stage.destination || entry - stage.destination > stage.length
        || mode_five.size() > stage.length - (entry - stage.destination)) {
        throw std::runtime_error("Deuteros mode-five title prefix lies outside original stage");
    }
    const auto bytes = disk.bytes(stage.disk_offset, stage.length);
    const auto branch = bytes.subspan(branch_entry - stage.destination, 42);
    const auto prefix = bytes.subspan(entry - stage.destination, mode_five.size());
    if (to_hex(sha256(bytes)) != stage_hash
        || to_hex(sha256(branch)) != "8fbe2ad1f1ad9de8d8edf02fa792faf88938dc4415f40db614e9e1399cf36fba"
        || !std::equal(mode_five.begin(), mode_five.end(), prefix.begin())
        || to_hex(sha256(prefix)) != "c4f5b0fa571dc0c932e9bb3df9f48e4c4336840d49ae2368e69fffa8c05c87a7") {
        throw std::runtime_error("Unsupported Deuteros mode-five title prefix");
    }
    return {incoming_profile, 0x206a0, 0x4040e, incoming_profile, 0x3717e,
        static_cast<std::uint8_t>(incoming_profile), 0x38092, 0x0101, 0x40450};
}

DeuterosAmigaFirstTitleExitCopy evaluate_deuteros_amiga_first_title_exit_copy(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan) {
    // $37f56 reaches this copy only if the two preceding original calls
    // return. Preserve that condition as an ABI boundary; the bytes below are
    // the complete local MOVE.B/DBRA transfer through the following BSR.
    constexpr std::uint32_t entry_address = 0x37f56;
    constexpr std::uint32_t source_address = 0x13006;
    constexpr std::uint32_t destination_address = 0x66000;
    constexpr std::uint32_t byte_count = 0x9392;
    constexpr std::uint32_t stop_before_subroutine_address = 0x37f7a;
    constexpr std::string_view title_stage_hash =
        "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03";
    constexpr std::array<std::uint8_t, 40> copy_prefix_bytes{{
        0x4e, 0xb9, 0x00, 0x03, 0x88, 0x0a,
        0x4e, 0xb9, 0x00, 0x02, 0x04, 0xfa,
        0x41, 0xf9, 0x00, 0x06, 0x60, 0x00,
        0x22, 0x7c, 0x00, 0x01, 0x30, 0x06,
        0x30, 0x3c, 0x93, 0x92,
        0x53, 0x40,
        0x10, 0xd9,
        0x51, 0xc8, 0xff, 0xfc,
        0x61, 0x00, 0x00, 0x1e,
    }};
    constexpr std::string_view copy_prefix_hash =
        "51b8d6875ea6d0c35557c358d4fe22e4cac6cff79ead9df604d213cab1adfe1c";
    constexpr std::string_view source_hash =
        "2951d0ae6dd01f84c1fb9b6cbb766c15378af1abb9a91fa5ded748d70b3e90eb";
    const auto& stage = plan.title_stage;
    if (stage.length == 0 || entry_address < stage.destination
        || source_address < stage.destination
        || entry_address - stage.destination > stage.length
        || source_address - stage.destination > stage.length
        || copy_prefix_bytes.size() > stage.length - (entry_address - stage.destination)
        || byte_count > stage.length - (source_address - stage.destination)) {
        throw std::runtime_error("Deuteros first title exit copy lies outside original stage");
    }
    const auto stage_bytes = disk.bytes(stage.disk_offset, stage.length);
    const auto copy_prefix = stage_bytes.subspan(
        entry_address - stage.destination, copy_prefix_bytes.size());
    const auto source = stage_bytes.subspan(source_address - stage.destination, byte_count);
    if (to_hex(sha256(stage_bytes)) != title_stage_hash
        || !std::equal(copy_prefix_bytes.begin(), copy_prefix_bytes.end(), copy_prefix.begin())
        || to_hex(sha256(copy_prefix)) != copy_prefix_hash
        || to_hex(sha256(source)) != source_hash) {
        throw std::runtime_error("Unsupported Deuteros first title exit copy");
    }
    DeuterosAmigaFirstTitleExitCopy result;
    result.entry_address = entry_address;
    result.preceding_helper_addresses = {0x3880a, 0x204fa};
    result.source_address = source_address;
    result.source_disk_offset = stage.disk_offset + source_address - stage.destination;
    result.destination_address = destination_address;
    result.byte_count = byte_count;
    result.source_sha256 = std::string(source_hash);
    result.copied_bytes.assign(source.begin(), source.end());
    result.stop_before_subroutine_address = stop_before_subroutine_address;
    return result;
}

DeuterosAmigaFirstTitleExitReturnTail evaluate_deuteros_amiga_first_title_exit_return_tail(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    const bool subroutine_returned) {
    // The BSR itself is intentionally not invoked. Its return is a required
    // caller-provided fact before this following straight-line tail exists.
    if (!subroutine_returned) {
        throw std::runtime_error("Deuteros first title exit subroutine did not return");
    }
    constexpr std::uint32_t entry_address = 0x37f7e;
    constexpr std::uint32_t preceding_subroutine_address = 0x37f7a;
    constexpr std::string_view title_stage_hash =
        "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03";
    constexpr std::array<std::uint8_t, 28> return_tail_bytes{{
        0x20, 0x39, 0x00, 0x02, 0x06, 0xa0,
        0x23, 0xc0, 0x00, 0x01, 0x2f, 0xf8,
        0x23, 0xfc, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01, 0x2f, 0xfc,
        0x4e, 0xf9, 0x00, 0x01, 0x28, 0x00,
    }};
    constexpr std::string_view return_tail_hash =
        "bacc75771f84068878d031ad87b0708c08911e85b605436c29d8d4c1faa2884c";
    const auto& stage = plan.title_stage;
    if (stage.length == 0 || entry_address < stage.destination
        || entry_address - stage.destination > stage.length
        || return_tail_bytes.size() > stage.length - (entry_address - stage.destination)) {
        throw std::runtime_error("Deuteros first title exit return tail lies outside original stage");
    }
    const auto stage_bytes = disk.bytes(stage.disk_offset, stage.length);
    const auto return_tail = stage_bytes.subspan(
        entry_address - stage.destination, return_tail_bytes.size());
    if (to_hex(sha256(stage_bytes)) != title_stage_hash
        || !std::equal(return_tail_bytes.begin(), return_tail_bytes.end(), return_tail.begin())
        || to_hex(sha256(return_tail)) != return_tail_hash) {
        throw std::runtime_error("Unsupported Deuteros first title exit return tail");
    }
    return {entry_address, preceding_subroutine_address, 0x206a0, 0x12ff8,
        0x12ffc, 2, 0x12800};
}

DeuterosAmigaFirstTitleExitSubroutineProfile
parse_deuteros_amiga_first_title_exit_subroutine_profile(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan) {
    constexpr std::uint32_t entry_address = 0x37f9a;
    constexpr std::string_view title_stage_hash =
        "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03";
    constexpr std::array<std::uint8_t, 152> routine_bytes{{
        0x22, 0x3c, 0x00, 0x01, 0x28, 0x00, 0x2e, 0x3c, 0x00, 0x00, 0x2c, 0x00,
        0x20, 0x3c, 0x00, 0x00, 0x06, 0x00, 0x4e, 0xb9, 0x00, 0x02, 0x08, 0xc0,
        0x43, 0xf9, 0x00, 0x01, 0xee, 0xfa, 0x33, 0x7c, 0x00, 0x0a, 0x00, 0x1c,
        0x23, 0x7c, 0x00, 0x01, 0xef, 0x48, 0x00, 0x28, 0x2c, 0x78, 0x00, 0x04,
        0x4e, 0xae, 0xfe, 0x32, 0x43, 0xf9, 0x00, 0x01, 0xee, 0xfa, 0x2c, 0x78,
        0x00, 0x04, 0x4e, 0xae, 0xfe, 0x3e, 0x43, 0xf9, 0x00, 0x01, 0xee, 0xd8,
        0x2c, 0x78, 0x00, 0x04, 0x4e, 0xae, 0xfe, 0x98, 0x20, 0x39, 0x00, 0x02,
        0x06, 0x98, 0xb0, 0xb9, 0x00, 0x02, 0x06, 0x9c, 0x67, 0x1c, 0x43, 0xf9,
        0x00, 0x02, 0x06, 0x3e, 0x2c, 0x78, 0x00, 0x04, 0x4e, 0xae, 0xfe, 0x3e,
        0x43, 0xf9, 0x00, 0x02, 0x06, 0x76, 0x2c, 0x78, 0x00, 0x04, 0x4e, 0xae,
        0xfe, 0x98, 0x43, 0xf9, 0x00, 0x02, 0x05, 0xe4, 0x2c, 0x78, 0x00, 0x04,
        0x4e, 0xae, 0xfe, 0x3e, 0x43, 0xf9, 0x00, 0x02, 0x06, 0x1c, 0x2c, 0x78,
        0x00, 0x04, 0x4e, 0xae, 0xfe, 0x98, 0x4e, 0x75,
    }};
    constexpr std::string_view routine_hash =
        "b076611efd33354e311dc9f64b57454e31cddd69c0749a05034f0d828a5b36c1";
    const auto& stage = plan.title_stage;
    if (stage.length == 0 || entry_address < stage.destination
        || entry_address - stage.destination > stage.length
        || routine_bytes.size() > stage.length - (entry_address - stage.destination)) {
        throw std::runtime_error("Deuteros first title exit subroutine lies outside original stage");
    }
    const auto stage_bytes = disk.bytes(stage.disk_offset, stage.length);
    const auto routine = stage_bytes.subspan(entry_address - stage.destination, routine_bytes.size());
    if (to_hex(sha256(stage_bytes)) != title_stage_hash
        || !std::equal(routine_bytes.begin(), routine_bytes.end(), routine.begin())
        || to_hex(sha256(routine)) != routine_hash) {
        throw std::runtime_error("Unsupported Deuteros first title exit subroutine");
    }
    return {entry_address, 0x12800, 0x2c00, 0x600, 0x208c0, 0x1eefa, 0x1c, 0x000a,
        0x28, 0x1ef48, {0x1eefa, 0x1eefa, 0x1eed8, 0x2063e, 0x20676},
        {-0x1ce, -0x1c2, -0x168, -0x1c2, -0x168}, 0x20698, 0x2069c, 0x38014, 0x38030};
}

DeuterosAmigaSecondTitleExitReturnTail evaluate_deuteros_amiga_second_title_exit_return_tail(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    const bool preceding_calls_returned) {
    // The four preceding calls are deliberately not invoked. Their collective
    // return is a caller-provided ABI fact before this local tail can exist.
    if (!preceding_calls_returned) {
        throw std::runtime_error("Deuteros second title exit calls did not return");
    }
    constexpr std::uint32_t entry_address = 0x38046;
    constexpr std::string_view title_stage_hash =
        "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03";
    constexpr std::array<std::uint8_t, 28> return_tail_bytes{{
        0x20, 0x39, 0x00, 0x02, 0x06, 0xa0,
        0x23, 0xc0, 0x00, 0x01, 0x2f, 0xf8,
        0x23, 0xfc, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x2f, 0xfc,
        0x4e, 0xf9, 0x00, 0x01, 0x28, 0x00,
    }};
    constexpr std::string_view return_tail_hash =
        "cf80103d5a580dc1e59f1090169c769a66a5d34c1112f14456e00713f1d078da";
    const auto& stage = plan.title_stage;
    if (stage.length == 0 || entry_address < stage.destination
        || entry_address - stage.destination > stage.length
        || return_tail_bytes.size() > stage.length - (entry_address - stage.destination)) {
        throw std::runtime_error("Deuteros second title exit return tail lies outside original stage");
    }
    const auto stage_bytes = disk.bytes(stage.disk_offset, stage.length);
    const auto return_tail = stage_bytes.subspan(
        entry_address - stage.destination, return_tail_bytes.size());
    if (to_hex(sha256(stage_bytes)) != title_stage_hash
        || !std::equal(return_tail_bytes.begin(), return_tail_bytes.end(), return_tail.begin())
        || to_hex(sha256(return_tail)) != return_tail_hash) {
        throw std::runtime_error("Unsupported Deuteros second title exit return tail");
    }
    return {entry_address, {0x3880a, 0x204fa, 0x37efa, 0x37f9a}, 0x206a0,
        0x12ff8, 0x12ffc, 4, 0x12800};
}

DeuterosAmigaThirdTitleExitReturnTail evaluate_deuteros_amiga_third_title_exit_return_tail(
    const AmigaAdf& disk, const DeuterosAmigaLoadPlan& plan,
    const bool preceding_calls_returned) {
    if (!preceding_calls_returned) {
        throw std::runtime_error("Deuteros third title exit calls did not return");
    }
    constexpr std::uint32_t entry_address = 0x38076;
    constexpr std::string_view title_stage_hash =
        "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03";
    constexpr std::array<std::uint8_t, 28> return_tail_bytes{{
        0x20, 0x39, 0x00, 0x02, 0x06, 0xa0,
        0x23, 0xc0, 0x00, 0x01, 0x2f, 0xf8,
        0x23, 0xfc, 0x00, 0x00, 0x00, 0x03, 0x00, 0x01, 0x2f, 0xfc,
        0x4e, 0xf9, 0x00, 0x01, 0x28, 0x00,
    }};
    constexpr std::string_view return_tail_hash =
        "25c2f6bf241a863d0b16359553dfae9a82953dfbc25035db71634a0b369df217";
    const auto& stage = plan.title_stage;
    if (stage.length == 0 || entry_address < stage.destination
        || entry_address - stage.destination > stage.length
        || return_tail_bytes.size() > stage.length - (entry_address - stage.destination)) {
        throw std::runtime_error("Deuteros third title exit return tail lies outside original stage");
    }
    const auto stage_bytes = disk.bytes(stage.disk_offset, stage.length);
    const auto return_tail = stage_bytes.subspan(
        entry_address - stage.destination, return_tail_bytes.size());
    if (to_hex(sha256(stage_bytes)) != title_stage_hash
        || !std::equal(return_tail_bytes.begin(), return_tail_bytes.end(), return_tail.begin())
        || to_hex(sha256(return_tail)) != return_tail_hash) {
        throw std::runtime_error("Unsupported Deuteros third title exit return tail");
    }
    return {entry_address, {0x3880a, 0x204fa, 0x37efa, 0x37f9a}, 0x206a0,
        0x12ff8, 0x12ffc, 3, 0x12800};
}

} // namespace eon
