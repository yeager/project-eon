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
    if (big16(bytes, offset) != expected) {
        throw std::runtime_error("Unexpected Deuteros title-stage opcode at offset " + std::to_string(offset));
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
    require_word(code, 0x190, 0x4eb9); // jsr $4069a
    require_long(code, 0x192, 0x0004069a);

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

    return {stage.entry_address, 0x4040e, 5, 0x3717e, 0x38092, 0x101,
        0x19d52, 1, 0x40574, 0x222c0, 0x23e4e, 0x40410, 0xea60, 0x4069a,
        0x202c6, 0x202b8, 0x1ed24, 0x40678, 16, 0x0eee, 0x12fec,
        static_cast<std::int16_t>(-0xc0), static_cast<std::int16_t>(-0x1a4),
        0x12e12, 0x1ffda, 0x1ffe6, 0x2008e, 0x1ffc8, 0x1ffce, 0x1ffd4,
        0x40776};
}

} // namespace eon
