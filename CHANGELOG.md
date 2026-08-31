# Changelog

All notable changes to AtlasPS2 are recorded here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Until 1.0.0 the minor number carries breaking changes: 0.x releases are
pre-release milestones, not a stable interface.

## [Unreleased]

### Added

- **Milestone 2 - graphical interface.** A dark theme, drawing primitives,
  a screen stack, and the Home, System Info, Power and placeholder
  screens.
- `atlas_theme_*`: the default dark palette is compiled into the ELF and
  cannot be removed, so a corrupt or missing theme file still leaves a
  readable interface rather than an invisible one.
- `atlas_ui_*`: rectangles, gradients, notched panels, separators, header
  and footer bars, menu rows, and a modal message box. Every coordinate
  is safe-area-relative and the 16:9 narrowing is applied in one place,
  so no screen has to know about overscan or aspect.
- `atlas_screen_*`: a fixed-depth stack of statically allocated screens.
  Push, pop and replace take effect between frames, so a screen's
  `enter()` always runs before it first draws.
- Home, System Info and Power screens; Power builds its entry list from
  what is actually available and asks for confirmation before anything
  that ends the session. Unfinished entries open an honest placeholder
  rather than a black screen.
- `atlas_utf8_*`: the UTF-8 decoder, split out of the font renderer so it
  can be checked on the host. `make check` builds and runs the self-check
  in `tests/`, which covers the French accented characters, replacement
  of anything outside Latin-1, and the guarantee that malformed input
  always advances - a decoder that could stall would hang the render loop
  with no way out.
- **Milestone 1 - bootable ELF.** IOP reset, embedded IRX module loading,
  GS bring-up, controller input, bitmap font rendering, and a splash screen
  reporting which module groups came up.
- `atlas_boot_*`: IOP reset with bounded retries, SBV patches, and the
  module set (iomanX, fileXio, sio2man, padman, mcman, mcserv, usbd, bdm,
  bdmfs_fatfs, usbmass_bd, poweroff) embedded in the ELF. Every group is
  optional except the file layer; a failure degrades features instead of
  aborting the boot.
- `atlas_video_*` (AtlasVideo): AUTO/NTSC/PAL/480p modes with region
  detection via `gsKit_check_rom()`, 4:3 and 16:9, screen offsets, an
  overscan-aware safe area, and a CT24 framebuffer.
- `atlas_input_*`: both controller ports merged, logical buttons so no UI
  code hardcodes cross-as-confirm, edge detection, and direction repeat.
- `atlas_font_*`: T8 atlas plus greyscale CLUT uploaded to VRAM, so one
  atlas draws in any colour; UTF-8 decoding covering Latin-1 for the
  French translation; width measurement and ellipsis clipping.
- `tools/genfont.py`: build-time TTF-to-atlas baker. Avoids gsKit's FONTM
  helper, which reads Sony ROM data.
- Boot hotkeys latched before any configuration is parsed: L1+R1 for
  Recovery, R1 alone for safe video.
