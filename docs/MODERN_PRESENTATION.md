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

Output resolution and aspect ratio also remain renderer controls. The viewport
is centered into the selected ratio, avoiding unintended independent width and
height stretching. Any individual output, ratio, Scale2x, filtering, scanline
or frame change switches the panel to Custom. Original has no path to invoke
these settings.

Separately installed Modern asset packs retain their existing explicit
release-hash admission requirements; they are not discovered automatically and
are never selected in Original mode. See [MODERN_ASSET_PACK_FORMAT.md](MODERN_ASSET_PACK_FORMAT.md).
