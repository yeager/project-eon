#include "data/function_map.hpp"

#include "data/release_manifest.hpp"
#include "data/sha256.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdint>

namespace eon {
namespace {

// Keep this table in exact source order with docs/function-map.json.  Every
// source hash names an existing, separately hash-checked original leaf or
// stage.  The descriptions deliberately retain unknown ABI/state boundaries.
constexpr std::array<FunctionMapEntry, 54> entries{{
    {"millennium-atari-en-prg-entry", "ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01",
     "millennium-atari-equinox-prg-chain", Game::millennium, Platform::atari_st, "en", "m68000",
     "4584ddc459e3bf03e642f3156fbedb74aa33a847db4937beb5635eb492e93686",
     "MILENIUM.TOS+0x001c", "+0x0000", "verified-static",
     "GEMDOS relocation, runtime load base, TOS/XBIOS results, and execution remain unproven",
     "diagnostics only", "PRESERVATION.md#millennium-atari-st-relocation-evidence",
     "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7",
     "image-relative-unrelocated"},
    {"millennium-amiga-en-resident-independent-entry", "2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400",
     "millennium-amiga-shared-resident", Game::millennium, Platform::amiga, "en", "m68000",
     "d144abc05f891710dc99b30d87f020bd6e2ff7796ef86a847f07b8d97d55d18e",
     "ADF+0x16908", "$68508", "verified-static",
     "D3, the tested runtime byte, both branch outcomes, and external targets remain unproven",
     "diagnostics only", "PRESERVATION.md#millennium-amiga-raw-loader-evidence",
     "d144abc05f891710dc99b30d87f020bd6e2ff7796ef86a847f07b8d97d55d18e"},
    {"millennium-dos-en-launcher-driver-request", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-launcher", Game::millennium, Platform::dos, "en", "i8086",
     "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e",
     "MILL.COM+0x01cf", "$02cf", "verified-static",
     "the requested video driver, private interrupt result, and subsequent branch remain unproven",
     "diagnostics only", "PRESERVATION.md#english-millennium-dos-reference-trace-adapter",
     "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e"},
    {"millennium-dos-en-title-entry", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-title-flow", Game::millennium, Platform::dos, "en", "i8086",
     "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6",
     "TITLES.EXE+0x1a80", "$1b80", "verified-static",
     "entry conditions, resource routine results, input, and title presentation remain unproven",
     "diagnostics only", "PRESERVATION.md#title-to-game-hand-off",
     "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6"},
    {"millennium-dos-en-title-timer-vector-hook", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-title-flow", Game::millennium, Platform::dos, "en", "i8086",
     "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6",
     "TITLES.EXE+0x104e", "$114e", "verified-static",
     "the two words at $0000:$0070 require a typed runtime observation",
     "native vector hook", "PRESERVATION.md#title-to-game-hand-off",
     "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6"},
    {"millennium-dos-en-title-video-vector-hook", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "millennium-dos-title-flow", Game::millennium, Platform::dos, "en", "i8086", "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6", "TITLES.EXE+0x11a0", "$12a0", "verified-static", "the setup callee at $1c0e->$135e remains unproven", "native repeated mode setup", "PRESERVATION.md#title-to-game-hand-off", "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6"},
    {"millennium-dos-en-title-buffer-selection", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "millennium-dos-title-flow", Game::millennium, Platform::dos, "en", "i8086", "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6", "TITLES.EXE+0x125e", "$135e", "verified-static", "the setup call at $1c11->$0ff3 remains unproven", "native buffer selection", "PRESERVATION.md#title-to-game-hand-off", "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6"},
    {"millennium-dos-en-title-graphics-request", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "millennium-dos-title-flow", Game::millennium, Platform::dos, "en", "i8086", "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6", "TITLES.EXE+0x0ef3", "$0ff3", "verified-static", "the callee at $1c17->$1725 remains unproven", "native observed private result", "PRESERVATION.md#title-to-game-hand-off", "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6"},
    {"millennium-dos-en-title-descriptor-setup", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "millennium-dos-title-flow", Game::millennium, Platform::dos, "en", "i8086", "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6", "TITLES.EXE+0x1290", "$1390", "verified-static", "the two relocated source words at $13aa remain unproven", "native far-read boundary", "PRESERVATION.md#title-to-game-hand-off", "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6"},
    {"millennium-dos-en-title-private-wrapper", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-title-flow", Game::millennium, Platform::dos, "en", "i8086",
     "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6",
     "TITLES.EXE+0x0127", "$0227", "verified-static",
     "the private INT 91h ABI and observed raw returns are not runtime inputs",
     "diagnostics only", "PRESERVATION.md#english-millennium-dos-reference-trace-adapter",
     "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6"},
    {"millennium-dos-en-title-availability-poll", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-title-flow", Game::millennium, Platform::dos, "en", "i8086",
     "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6",
     "TITLES.EXE+0x0c0a", "$0d0a", "verified-static",
     "the DOS result, character semantics, title exit, child launch, rendering, and game state remain unproven",
     "availability boundary only", "PRESERVATION.md#required-dynamic-trace-contract-for-the-next-playable-dos-increment",
     "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6"},
    {"millennium-dos-en-title-nonzero-branch", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-title-flow", Game::millennium, Platform::dos, "en", "i8086",
     "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6",
     "TITLES.EXE+0x1b28", "$1c28", "verified-static",
     "only the zero/nonzero branch shape is verified; the DOS result, character, later calls, process exit, rendering, and game state remain unproven",
     "availability boundary only", "PRESERVATION.md#required-dynamic-trace-contract-for-the-next-playable-dos-increment",
     "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6"},
    {"millennium-dos-en-title-exit-closure", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-title-flow", Game::millennium, Platform::dos, "en", "i8086",
     "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6",
     "TITLES.EXE+0x1b54", "$1c54", "verified-static",
     "local call returns, process termination, parent return, rendering, and game startup remain unproven",
     "diagnostics only", "PRESERVATION.md#required-dynamic-trace-contract-for-the-next-playable-dos-increment",
     "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6"},
    {"millennium-dos-en-title-private-driver-setup", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-title-flow", Game::millennium, Platform::dos, "en", "i8086",
     "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6",
     "TITLES.EXE+0x1868", "$1968", "verified-static",
     "the five private-driver ABI calls, helper effects, destinations, composition, BIOS output, and title exit remain unproven",
     "diagnostics only", "PRESERVATION.md#required-dynamic-trace-contract-for-the-next-playable-dos-increment",
     "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6"},
    {"millennium-dos-en-title-private-driver-helper", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-title-flow", Game::millennium, Platform::dos, "en", "i8086",
     "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6",
     "TITLES.EXE+0x1817", "$1917", "verified-static",
     "selector state, resource reads, private-driver effects, display output, timing, and game state remain unproven",
     "diagnostics only", "PRESERVATION.md#required-dynamic-trace-contract-for-the-next-playable-dos-increment",
     "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6"},
    {"millennium-dos-en-action-poll", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-game-flow", Game::millennium, Platform::dos, "en", "i8086",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57",
     "2200AD.EXE+0xd2db", "$d3db", "verified-static",
     "raw action return; input producer and handler semantics remain unproven",
     "diagnostics only", "PRESERVATION.md#main-loop-action-dispatch",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57"},
    {"millennium-dos-en-f1-handler", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-game-flow", Game::millennium, Platform::dos, "en", "i8086",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57",
     "2200AD.EXE+0x6e9a", "$6f9a", "verified-static",
     "action production, native call returns, state effects, rendering, and handler semantics remain unproven",
     "diagnostics only", "PRESERVATION.md#main-loop-action-dispatch",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57"},
    {"millennium-dos-en-f2-handler", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-game-flow", Game::millennium, Platform::dos, "en", "i8086",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57",
     "2200AD.EXE+0x70ca", "$71ca", "verified-static",
     "action production, native call returns, state effects, rendering, and handler semantics remain unproven",
     "diagnostics only", "PRESERVATION.md#main-loop-action-dispatch",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57"},
    {"millennium-dos-en-f3-handler", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-game-flow", Game::millennium, Platform::dos, "en", "i8086",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57",
     "2200AD.EXE+0x6eaa", "$6faa", "verified-static",
     "action production, native call returns, state effects, rendering, and handler semantics remain unproven",
     "diagnostics only", "PRESERVATION.md#main-loop-action-dispatch",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57"},
    {"millennium-dos-en-f4-handler", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-game-flow", Game::millennium, Platform::dos, "en", "i8086",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57",
     "2200AD.EXE+0x71f9", "$72f9", "verified-static",
     "action production, native call returns, state effects, rendering, and handler semantics remain unproven",
     "diagnostics only", "PRESERVATION.md#main-loop-action-dispatch",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57"},
    {"millennium-dos-en-f5-handler", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-game-flow", Game::millennium, Platform::dos, "en", "i8086",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57",
     "2200AD.EXE+0x7497", "$7597", "verified-static",
     "action production, native call returns, state effects, rendering, and handler semantics remain unproven",
     "diagnostics only", "PRESERVATION.md#main-loop-action-dispatch",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57"},
    {"millennium-dos-en-f6-handler", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-game-flow", Game::millennium, Platform::dos, "en", "i8086",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57",
     "2200AD.EXE+0x7315", "$7415", "verified-static",
     "action production, native call returns, state effects, rendering, and handler semantics remain unproven",
     "diagnostics only", "PRESERVATION.md#main-loop-action-dispatch",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57"},
    {"millennium-dos-en-f7-handler", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-game-flow", Game::millennium, Platform::dos, "en", "i8086",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57",
     "2200AD.EXE+0x7421", "$7521", "verified-static",
     "action production, native call returns, state effects, rendering, and handler semantics remain unproven",
     "diagnostics only", "PRESERVATION.md#main-loop-action-dispatch",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57"},
    {"millennium-dos-en-f8-prefix", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-game-flow", Game::millennium, Platform::dos, "en", "i8086",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57",
     "2200AD.EXE+0x7206", "$7306", "verified-static",
     "only the pre-call private-overlay write is proven; later native calls are opaque",
     "isolated transient overlay only", "PRESERVATION.md#main-loop-action-dispatch",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57"},
    {"millennium-dos-en-f9-handler", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-game-flow", Game::millennium, Platform::dos, "en", "i8086",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57",
     "2200AD.EXE+0x7239", "$7339", "verified-static",
     "action production, native call returns, state effects, rendering, and handler semantics remain unproven",
     "diagnostics only", "PRESERVATION.md#main-loop-action-dispatch",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57"},
    {"millennium-dos-en-f10-handler", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-game-flow", Game::millennium, Platform::dos, "en", "i8086",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57",
     "2200AD.EXE+0x7284", "$7384", "verified-static",
     "action production, native call returns, state effects, rendering, and handler semantics remain unproven",
     "diagnostics only", "PRESERVATION.md#main-loop-action-dispatch",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57"},
    {"millennium-dos-en-game-entry", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-game-flow", Game::millennium, Platform::dos, "en", "i8086",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57",
     "2200AD.EXE+0xd1b0", "$d2b0", "verified-static",
     "the first private wrapper result, BIOS results, game state, and action loop remain unproven",
     "diagnostics only", "PRESERVATION.md#english-millennium-dos-startup-prefix",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57"},
    {"millennium-dos-en-gx-dispatcher", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-gx-overlay", Game::millennium, Platform::dos, "en", "i8086",
     "093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb",
     "2200GX.EXE+0x0000", "$0100", "verified-static",
     "selector policy, overlay segment, handler results, resource order, and display effects remain unproven",
     "trace-gated sparse GX startup session", "PRESERVATION.md#millennium-dos-execution-model",
     "093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb"},
    {"millennium-dos-es-title-entry", "b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4",
     "millennium-dos-spanish-title-boundary", Game::millennium, Platform::dos, "es", "i8086",
     "02082c35e18cee330f7d1b88098f502e68011f7e47a3a649961f6f03d1d14fe7",
     "TITLES.EXE+0x1a80", "$1b80", "verified-static",
     "private-driver results, DOS character semantics, child status, frames, and game state remain unproven",
     "diagnostics only", "PRESERVATION.md#millennium-spanish-dos-floppy-evidence",
     "1cb7d399ab22110317b1c7486a575c00895f12a17268d0c984ac264a5695961d"},
    {"deuteros-amiga-en-main-entry", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
     "deuteros-amiga-clean-main-stage", Game::deuteros, Platform::amiga, "en", "m68000",
     "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",
     "ADF+0x06f34", "$21734", "verified-static",
     "decoded disk-read results, Exec/graphics ABI, input, timing, and game state remain unproven",
     "diagnostics only", "PRESERVATION.md#deuteros-amiga-execution-chain",
     "a82c0d6a12e156e0832d632a6c40dd58713a00b611dbcba7289aa16b0969a0a6"},
    {"deuteros-amiga-en-channel-request-continuation", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
     "deuteros-amiga-clean-main-stage", Game::deuteros, Platform::amiga, "en", "m68000",
     "120fba90e0b4fa9e96d8a6cf95fbac512d67d7daa42c3776ce0d3066b3f02ee9",
     "ADF+0x7092", "$21892", "verified-static",
     "caller state, branch choices, service results, input, and later destination remain unproven",
     "diagnostics only", "PRESERVATION.md#deuteros-amiga-execution-chain",
     "a82c0d6a12e156e0832d632a6c40dd58713a00b611dbcba7289aa16b0969a0a6"},
    {"deuteros-amiga-en-channel-request-first-callee", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
     "deuteros-amiga-clean-main-stage", Game::deuteros, Platform::amiga, "en", "m68000",
     "d1a162af50f92b60d03b1da4ab186a547e46d145b0599cfbbeff7fb5af324ac1",
     "ADF+0x7a9c", "$2229c", "verified-static",
     "custom-register poll, ABI calls, state writes, and return-dependent paths remain unproven",
     "diagnostics only", "PRESERVATION.md#deuteros-amiga-execution-chain",
     "a82c0d6a12e156e0832d632a6c40dd58713a00b611dbcba7289aa16b0969a0a6"},
    {"deuteros-amiga-en-channel-request-second-callee", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
     "deuteros-amiga-clean-main-stage", Game::deuteros, Platform::amiga, "en", "m68000",
     "d4e9a1ee0065537a627cdd9ee8827f11d5fa28e0f860aacb21bbdc7e11784bd1",
     "ADF+0x7ca2", "$224a2", "verified-static",
     "low-memory and custom-register values, writes, and return semantics remain unproven",
     "diagnostics only", "PRESERVATION.md#deuteros-amiga-execution-chain",
     "a82c0d6a12e156e0832d632a6c40dd58713a00b611dbcba7289aa16b0969a0a6"},
    {"deuteros-amiga-en-channel-request-following-service", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
     "deuteros-amiga-clean-main-stage", Game::deuteros, Platform::amiga, "en", "m68000",
     "d5fdbdacd004d2cf377ea0dbaefb9d8b308ba23b568cfb3785456622bde49d19",
     "ADF+0x825a", "$22a5a", "verified-static",
     "descriptor values, flag state, runtime-cell writes, and caller results remain unproven",
     "diagnostics only", "PRESERVATION.md#deuteros-amiga-execution-chain",
     "a82c0d6a12e156e0832d632a6c40dd58713a00b611dbcba7289aa16b0969a0a6"},
    {"deuteros-amiga-en-channel-request-adjacent-entry", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
     "deuteros-amiga-clean-main-stage", Game::deuteros, Platform::amiga, "en", "m68000",
     "10ed8be15c107dbb56ca98eb8d17ffd2bce3910dd169d67ba058447c9031b1ff",
     "ADF+0x838a", "$22b8a", "verified-static",
     "caller registers, branch state, descriptor pointers, reads, writes, and return remain unproven",
     "diagnostics only", "PRESERVATION.md#deuteros-amiga-execution-chain",
     "a82c0d6a12e156e0832d632a6c40dd58713a00b611dbcba7289aa16b0969a0a6"},
    {"deuteros-amiga-en-opening-title-command", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
     "deuteros-amiga-clean-title-handoff", Game::deuteros, Platform::amiga, "en", "m68000",
     "9f3880bf72d32f0fc119b941527dfe6004e18ad7e0fdfc40fe87eb6a13fe9c41",
     "ADF+0x1c28a", "$3355c", "verified-static",
     "the pointer is caller-bound, but title execution, input, graphics, and mode semantics remain unproven",
     "diagnostics only", "PRESERVATION.md#deuteros-amiga-title-input-and-bootstrap-handoff",
     "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03"},
    {"deuteros-amiga-en-title-negative-service", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
     "deuteros-amiga-clean-title-handoff", Game::deuteros, Platform::amiga, "en", "m68000",
     "cbddd93eb43c498079e7e2175f8f7d6178c357aa6b5241631e717f9037cff414",
     "ADF+0x7abe6", "$1fbe6", "verified-static",
     "the nested $3fbf8 service effect remains opaque and requires an exact return observation",
     "native trace-gated command completion", "PRESERVATION.md#deuteros-amiga-title-input-and-bootstrap-handoff",
     "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03"},
    {"deuteros-amiga-en-title-post-command-pointer-route", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
     "deuteros-amiga-clean-title-handoff", Game::deuteros, Platform::amiga, "en", "m68000",
     "a7f7c0c3efa60284b3d292249b3560da4d832ff0c5dfa34711b72604760b39a9",
     "ADF+0x7b22a", "$2022a", "verified-static",
     "the zero-flag graphics wrapper remains an explicit nested-call boundary",
     "native trace-gated post-command state", "PRESERVATION.md#deuteros-amiga-title-input-and-bootstrap-handoff",
     "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03"},
    {"deuteros-amiga-en-title-post-command-graphics-return", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
     "deuteros-amiga-clean-title-handoff", Game::deuteros, Platform::amiga, "en", "m68000",
     "6e36c860c280c651947ad0ea6ef868759fbc7bfac67d89af219135e4751e6e6f",
     "ADF+0x7b0dc", "$200dc", "verified-static",
     "the graphics vector effect and following $41bb4 service remain opaque",
     "native trace-gated post-command state", "PRESERVATION.md#deuteros-amiga-title-input-and-bootstrap-handoff",
     "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03"},
    {"deuteros-amiga-en-title-first-paired-dispatch", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
     "deuteros-amiga-clean-title-handoff", Game::deuteros, Platform::amiga, "en", "m68000",
     "fba4dff4da954290d970f5ec129220c179a2ef73f010def6512401380b8640cc",
     "ADF+0x9cbb4", "$41bb4", "verified-static",
     "preexisting destination words require observation and decoded output is not yet a renderer surface",
     "native complete first merge loop", "PRESERVATION.md#deuteros-amiga-title-input-and-bootstrap-handoff",
     "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03"},
    {"deuteros-amiga-en-title-second-paired-dispatch", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
     "deuteros-amiga-clean-title-handoff", Game::deuteros, Platform::amiga, "en", "m68000",
     "dabcb6ee4feb0022f3232bcab1ffccb6657448e8602c39ef248da996e57a5666",
     "ADF runtime $78c76..$78d5a", "$41bb4", "verified-static",
     "64 final destination words require typed observation; execution stops before the next caller effect",
     "native complete second merge loop", "PRESERVATION.md#deuteros-amiga-title-input-and-bootstrap-handoff",
     "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03"},
    {"deuteros-amiga-en-title-post-command-service-prefix", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
     "deuteros-amiga-clean-title-handoff", Game::deuteros, Platform::amiga, "en", "m68000",
     "037c48dd824e064d3734fb4b72b6e649bfda6b9a7a764147a76690f4ce9506e0",
     "ADF+0x7be18", "$20e18", "verified-static",
     "the selected low-height stream header and payload remain separate boundaries",
     "native selector-5c dispatch setup", "PRESERVATION.md#deuteros-amiga-title-input-and-bootstrap-handoff",
     "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03"},
    {"deuteros-amiga-en-title-planar-zero-route", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
     "deuteros-amiga-clean-title-handoff", Game::deuteros, Platform::amiga, "en", "m68000",
     "14bad66df34c5d4200afe7ba9cef8ac114afaf31d9be133d428c1af727c0fe89",
     "ADF+0x7ac22", "$1fc22", "verified-static",
     "runtime pointer values and nonzero mode routes require ordered observations",
     "native trace-gated planar writes", "PRESERVATION.md#deuteros-amiga-title-input-and-bootstrap-handoff",
     "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03"},
    {"deuteros-amiga-en-title-planar-positive-clear", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
     "deuteros-amiga-clean-title-handoff", Game::deuteros, Platform::amiga, "en", "m68000",
     "806ad8916bbcdd2b6e01806f56cde2905cd8f9d2af63c877c2242371e2659141",
     "ADF+0x7aca6", "$1fca6", "verified-static",
     "mode, glyph, destination and blend-word values require ordered observations",
     "native trace-gated planar writes", "PRESERVATION.md#deuteros-amiga-title-input-and-bootstrap-handoff",
     "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03"},
    {"deuteros-amiga-en-title-planar-zero-set", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
     "deuteros-amiga-clean-title-handoff", Game::deuteros, Platform::amiga, "en", "m68000",
     "13e86b16e732da32a9cbdcd1b0b387c042b6a6bedd9b46cb5664d0a4a121318a",
     "ADF+0x7ad0a", "$1fd0a", "verified-static",
     "mode, dynamic strides, glyph and source-word values require ordered observations",
     "native trace-gated planar writes", "PRESERVATION.md#deuteros-amiga-title-input-and-bootstrap-handoff",
     "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03"},
    {"deuteros-amiga-en-title-planar-positive-set", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
     "deuteros-amiga-clean-title-handoff", Game::deuteros, Platform::amiga, "en", "m68000",
     "bd6bfbd42d3b6471a8166e14228fe177f5afe3a7ff3c8372cf291b0c37c44f82",
     "ADF+0x7ad7a", "$1fd7a", "verified-static",
     "mode, dynamic strides, glyph, destination and blend values require ordered observations",
     "native trace-gated planar writes", "PRESERVATION.md#deuteros-amiga-title-input-and-bootstrap-handoff",
     "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03"},
    {"deuteros-amiga-en-title-entry-prefix", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
     "deuteros-amiga-clean-title-handoff", Game::deuteros, Platform::amiga, "en", "m68000",
     "833374022042225f1bfeeedd56c05d7011168531fa121494cef04174453e5387",
     "title-stage+0x0426", "$40426", "verified-static",
     "incoming A1, mode meaning, Exec base, graphics calls, input, and title state remain unobserved",
     "diagnostics only", "PRESERVATION.md#deuteros-amiga-title-input-and-bootstrap-handoff",
     "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03"},
    {"deuteros-atari-en-copied-dispatcher", "c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653",
     "deuteros-atari-replicants-first-stage", Game::deuteros, Platform::atari_st, "en", "m68000",
     "2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7",
     "track-2+0x00c4", "$1ec4", "verified-static",
     "the preceding raw-read result, dispatcher state word, vector choice, callback ABI, and XBIOS effects remain unproven",
     "diagnostics only", "PRESERVATION.md#deuteros-atari-st-protected-media-boot-chain",
     "d20784600c5fe3c8fb2005ec5d162d68ffa8f5a0f65d29fcd8a1d9ede2bafddc"},
    {"deuteros-atari-en-raw-reader-wrapper", "c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653",
     "deuteros-atari-replicants-first-stage", Game::deuteros, Platform::atari_st, "en", "m68000",
     "a5bec9d04daa8ce600add594f6325030acd2ad8535910dee62497da90d572c90",
     "track-2+0x0060", "$1e60", "verified-static",
     "XBIOS Floprd inputs/results, status word, caller registers, and physical reads remain unproven",
     "diagnostics only", "PRESERVATION.md#deuteros-atari-st-protected-media-boot-chain",
     "d20784600c5fe3c8fb2005ec5d162d68ffa8f5a0f65d29fcd8a1d9ede2bafddc"},
    {"deuteros-atari-en-supervisor-callback-boundary", "c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653",
     "deuteros-atari-replicants-first-stage", Game::deuteros, Platform::atari_st, "en", "m68000",
     "11b26d5900e614547617a9c95611515e8238184756a0a18c7ff18b1ec372657b",
     "track-2+0x00d2", "$1ed2", "verified-static",
     "TRAP #14 selector 0x26, callback frame, XBIOS result, callback return, and state source remain unproven",
     "diagnostics only", "PRESERVATION.md#deuteros-atari-st-protected-media-boot-chain",
     "d20784600c5fe3c8fb2005ec5d162d68ffa8f5a0f65d29fcd8a1d9ede2bafddc"},
    {"deuteros-atari-en-state-selection-site", "c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653",
     "deuteros-atari-replicants-first-stage", Game::deuteros, Platform::atari_st, "en", "m68000",
     "03cf620d981a775fd1adabe55deea940e08760e3e49c62cd0643c22b5aa08082",
     "track-2+0x00c4", "$1ec4", "verified-static",
     "RAM $25fc provenance, selected word, bounds, table target, indirect-call result, and reachability remain unproven",
     "diagnostics only", "PRESERVATION.md#deuteros-atari-st-protected-media-boot-chain",
     "d20784600c5fe3c8fb2005ec5d162d68ffa8f5a0f65d29fcd8a1d9ede2bafddc"},
    {"deuteros-atari-en-post-raw-reader-service", "c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653",
     "deuteros-atari-replicants-first-stage", Game::deuteros, Platform::atari_st, "en", "m68000",
     "5b1480495df8defe3e1264dd083ec1c91134c01e56d3d94e060c583ee9b54a89",
     "track-2+0x1138", "$2f38", "verified-static",
     "raw-reader return, XBIOS selector 0x6 result, pointer provenance, copy outcome, and execution remain unproven",
     "diagnostics only", "PRESERVATION.md#deuteros-atari-st-protected-media-boot-chain",
     "d20784600c5fe3c8fb2005ec5d162d68ffa8f5a0f65d29fcd8a1d9ede2bafddc"},
    {"deuteros-atari-en-first-callee-post-service", "c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653",
     "deuteros-atari-replicants-first-stage", Game::deuteros, Platform::atari_st, "en", "m68000",
     "8778c08ae16a5f66009dda8d60a0dacba267cca4d29211a11fd2e30c40a7796b",
     "track-2+0x1116", "$2f16", "verified-static",
     "the selector-5 service return, branch reachability, RAM meanings, and execution remain unproven",
     "diagnostics only", "PRESERVATION.md#deuteros-atari-st-protected-media-boot-chain",
     "d20784600c5fe3c8fb2005ec5d162d68ffa8f5a0f65d29fcd8a1d9ede2bafddc"},
    {"deuteros-amiga-en-title-exec-boundary", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
     "deuteros-amiga-clean-title-handoff", Game::deuteros, Platform::amiga, "en", "m68000",
     "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03",
     "title-stage+0x0450", "$40450", "verified-static",
     "Exec base/vector result, callback ABI, input and title state remain unobserved",
     "diagnostics only", "PRESERVATION.md#deuteros-amiga-title-input-and-bootstrap-handoff",
     "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03"},
}};

bool is_lower_hex(const std::string_view value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](const unsigned char character) {
        return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
    });
}

bool parse_declared_address(const FunctionMapEntry& entry, std::uint64_t& address) {
    const std::string_view prefix = entry.address_space == "runtime" ? "$" : "+0x";
    if (!entry.runtime_address.starts_with(prefix)) return false;
    const auto digits = entry.runtime_address.substr(prefix.size());
    if (!is_lower_hex(digits)) return false;
    const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), address, 16);
    return result.ec == std::errc{} && result.ptr == digits.data() + digits.size();
}

} // namespace

std::span<const FunctionMapEntry> function_map() { return entries; }

std::vector<FunctionMapEntry> function_map_for_release(const std::string_view release_sha256) {
    std::vector<FunctionMapEntry> result;
    result.reserve(entries.size());
    for (const auto& entry : entries) {
        if (entry.release_sha256 == release_sha256) result.push_back(entry);
    }
    return result;
}

bool function_map_entry_is_well_formed(const FunctionMapEntry& entry) {
    if (entry.id.empty() || entry.parser_profile_id.empty() || entry.release_sha256.size() != 64U
        || !is_lower_hex(entry.release_sha256) || entry.source_asset_sha256.size() != 64U
        || !is_lower_hex(entry.source_asset_sha256) || entry.source_span_sha256.size() != 64U
        || !is_lower_hex(entry.source_span_sha256) || entry.source_offset.empty()
        || entry.uncertainty.empty() || entry.runtime_status.empty()
        || !entry.documentation_anchor.starts_with("PRESERVATION.md#")
        || entry.evidence_level != "verified-static") return false;
    if (entry.cpu != "i8086" && entry.cpu != "m68000") return false;
    if (entry.address_space == "runtime") {
        std::uint64_t address = 0;
        return parse_declared_address(entry, address);
    }
    return entry.address_space == "image-relative-unrelocated"
        && entry.runtime_address.size() > 3U && entry.runtime_address.starts_with("+0x")
        && is_lower_hex(entry.runtime_address.substr(3));
}

bool function_map_manifest_is_valid() {
    const auto releases = release_manifest();
    const auto profiles = parser_profile_manifest();
    for (const auto& entry : entries) {
        if (!function_map_entry_is_well_formed(entry)) return false;
        if ((entry.platform == Platform::dos && entry.cpu != "i8086")
            || ((entry.platform == Platform::amiga || entry.platform == Platform::atari_st)
                && entry.cpu != "m68000")) return false;
        const auto release_matches = std::count_if(releases.begin(), releases.end(), [&entry](const auto& release) {
            return release.sha256 == entry.release_sha256 && release.game == entry.game
                && release.platform == entry.platform && release.language == entry.language;
        });
        const auto id_matches = std::count_if(entries.begin(), entries.end(), [&entry](const auto& candidate) {
            return candidate.id == entry.id;
        });
        const auto profile_matches = std::count_if(profiles.begin(), profiles.end(), [&entry](const auto& profile) {
                return profile.release_sha256 == entry.release_sha256
                    && profile.id == entry.parser_profile_id;
            });
        if (release_matches != 1 || id_matches != 1 || profile_matches != 1) return false;
    }
    return true;
}

bool release_has_function_map_entry(const std::string_view release_sha256,
    const std::string_view entry_id) {
    const auto entry = std::find_if(entries.begin(), entries.end(), [&](const auto& candidate) {
        return candidate.release_sha256 == release_sha256 && candidate.id == entry_id;
    });
    return entry != entries.end()
        && release_has_parser_profile(release_sha256, entry->parser_profile_id);
}

std::optional<std::uint64_t> function_map_runtime_address_for(
    const std::string_view release_sha256, const std::string_view entry_id) {
    const auto entry = std::find_if(entries.begin(), entries.end(), [&](const auto& candidate) {
        return candidate.release_sha256 == release_sha256 && candidate.id == entry_id;
    });
    if (entry == entries.end() || entry->address_space != "runtime"
        || !function_map_entry_is_well_formed(*entry)) return std::nullopt;
    std::uint64_t address = 0;
    if (!parse_declared_address(*entry, address)) return std::nullopt;
    return address;
}

bool function_map_entries_are_attested_by_media(const VerifiedReleaseMedia& media) {
    // A function entry's source-span digest is deliberately the digest of the
    // parser-profile interval, rather than an executable-code assertion. A
    // profile can cover an ADF/PRG/COM range containing data, relocation
    // records, or unresolved calls as well as the named static site.
    const auto profiles = parser_profile_manifest();
    for (const auto& entry : function_map_for_release(media.release().sha256)) {
        if (!function_map_entry_is_well_formed(entry)) return false;
        const auto profile = std::find_if(profiles.begin(), profiles.end(), [&entry](const auto& candidate) {
            return candidate.release_sha256 == entry.release_sha256
                && candidate.id == entry.parser_profile_id;
        });
        if (profile == profiles.end() || profile->offset > profile->leaf_size
            || profile->length > profile->leaf_size - profile->offset) return false;
        const auto leaf = media.borrow(profile->leaf_sha256);
        if (!leaf || leaf->size() != profile->leaf_size
            || profile->offset > leaf->size()
            || profile->length > leaf->size() - profile->offset) return false;
        const auto span = leaf->subspan(
            static_cast<std::size_t>(profile->offset), static_cast<std::size_t>(profile->length));
        if (to_hex(sha256(span)) != entry.source_span_sha256) return false;
    }
    return true;
}

FunctionMapSidecarCoverage function_map_sidecar_coverage(const StaticControlFlowSummary& sidecar) {
    FunctionMapSidecarCoverage result;
    for (const auto& entry : entries) {
        std::uint64_t address = 0;
        if (!function_map_entry_is_well_formed(entry) || !parse_declared_address(entry, address)) continue;
        bool sidecar_names_release = false;
        bool bound = false;
        for (std::size_t index = 0; index < sidecar.documents.size(); ++index) {
            const auto& document = sidecar.documents[index];
            if (document.release_sha256 != entry.release_sha256) continue;
            sidecar_names_release = true;
            if (document.cpu != entry.cpu || document.address_space != entry.address_space) continue;
            for (const auto& range : sidecar.declared_ranges) {
                if (range.document_index != index || range.sha256 != entry.source_span_sha256
                    || address < range.address || address - range.address >= range.length) continue;
                bound = true;
                break;
            }
            if (bound) break;
        }
        if (!sidecar_names_release) continue;
        ++result.function_entry_count;
        if (bound) ++result.direct_range_binding_count;
        else ++result.not_declared_by_sidecar_count;
    }
    return result;
}

} // namespace eon
