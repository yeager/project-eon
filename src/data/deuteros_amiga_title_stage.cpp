#include "data/deuteros_amiga_title_stage.hpp"

#include <span>
#include <stdexcept>

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
    require_word(dispatch_flow, 44, 0x203c); // move.l #$4e20,d0
    require_long(dispatch_flow, 46, 0x00004e20);
    require_word(dispatch_flow, 50, 0x5380); // subq.l #1,d0
    require_word(dispatch_flow, 52, 0x66fc); // bne.b delay loop
    const auto zero_branch = stage_code(0x1fc22, 6);
    require_word(zero_branch, 0, 0x4a39); // tst.b $1f98e
    require_long(zero_branch, 2, 0x0001f98e);

    return {stage.entry_address, 0x4040e, 5, 0x3717e, 0x38092, 0x101,
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
        0x1f98c, 0x1fc22, 0x1fc9e, 0x3fbf8, 0x13, 0x0c, 0x20, 0x4e20};
}

} // namespace eon
