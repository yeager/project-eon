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

} // namespace eon
