# Modern presentation profiles

Project Eon's **Original** mode is the preservation contract. It uses the
verified original release, recovered renderer path and selected aspect ratio;
it does not use a Modern graphics profile, derived Scale2x surface, overlay,
external Modern pack or F10 setting.

**Modern** is an explicit, renderer-only opt-in. Its F10 panel starts at
**Clean** and has the following presentation profiles:

| Profile | Renderer-only selection |
| --- | --- |
| Clean | Scale2x reconstruction, smooth scaling and a Modern frame. |
| CRT | Nearest-neighbour original pixels, scanlines and a Modern frame. |
| Cinematic | Scale2x reconstruction, smooth scaling and a transient warm/vignette overlay. |
| High contrast | Crisp nearest scaling plus a transient high-separation keyline and surround. |
| Custom | The user has changed one of the individual renderer controls. |

The profile selector is only a named combination of F10 controls. Selecting a
profile does not read, write, patch, unpack or substitute original media; it
does not enter a recovered game VM, alter input sampling, timing, save data or
original texture bytes. Cinematic and High contrast are visual presentation
looks, not accessibility claims or alterations to original pixels.

Output resolution, aspect ratio and frame pacing also remain renderer controls.
The viewport is fitted and centered into the selected ratio, never cropped or
independently stretched. The shared geometry contract rejects non-finite or
non-positive bounds and ratios rather than passing malformed viewport values
to SDL; native tests cover the 4:3 and 16:9 F10 preview fits. Pacing offers display VSync (the
default), a precise 120-FPS presentation cap, or uncapped presentation. The
cap delays only SDL presentation after a frame has been rendered; it never
changes a recovered scheduler, original input poll, save byte, or game tick.
Any individual output, ratio, pacing, Scale2x/Scale4x, filtering, scanline or frame
change switches the panel to Custom. Original has no path to invoke these
settings.

The shipped Custom selector provides bounded transient **Scale4x** as well as
Scale2x and original-pixel output. Scale4x is exactly two Scale2x passes over
already decoded RGBA pixels, subject to a four-times-smaller source pixel
budget before its intermediate allocation. A derived SDL texture is keyed by
the admitted release hash, symbolic presentation source, source tick and
reconstruction mode; changing any of those values destroys the old transient
texture before upload. It has no file, disk cache, pack, media or simulation
API, and Original never instantiates it.

The F10 panel also contains a read-only **Developer diagnostics** page. It
reports only launcher-owned facts: the selected hash identity in abbreviated
form, game/platform/language, the number of declarative recovery-map
boundaries for that exact release, whether an external reference trace is
loaded, the active Modern preset and output controls, and the selected SDL
frame-pacing policy. GUI launches never load a trace: full trace admission remains
the separate hash-locked CLI verifier. The page consumes its own keyboard,
gamepad and touch events and cannot inspect or modify original media, guest
input, simulation state, or save bytes.

The diagnostics page can open a paged **Recovery function map**.  Each row is
an exact named function/dispatch boundary for the selected rehashed release:
CPU, source-leaf SHA-256, source offset, runtime address, evidence level,
uncertainty and runtime boundary. It is a preservation cross-reference
inspired by declarative recompilation maps, not a hook table or guest-code
browser. The renderer never executes an entry, opens a trace, or exposes an
original byte through this view.

When a metadata-only static control-flow aggregate has already been admitted,
the same page can show its document, range, and candidate counts. It never
reads a sidecar or exposes its name, path, decoded instructions, addresses, or
original media bytes. Current interactive launches deliberately load no
sidecar, so the row is explicitly unavailable; the bounded, hash-bound
sidecar route remains `--inspect-json` only.

Separately installed Modern asset packs retain their existing explicit
release-hash admission requirements; they are not discovered automatically and
are never selected in Original mode. See [MODERN_ASSET_PACK_FORMAT.md](MODERN_ASSET_PACK_FORMAT.md).
