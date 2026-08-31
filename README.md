# AtlasPS2

A modern boot and menu environment for the PlayStation 2, built for real
hardware.

AtlasPS2 is what loads when the console powers on: it brings up the IOP,
finds your devices, lists the homebrew applications it discovers on them,
and launches them. It is a spiritual successor to FreeMcBoot's menu, with
a readable dark interface, an unbreakable Recovery mode, and an update
path that cannot brick the installation.

> **Status: pre-release.** Milestone 1 of 10 is complete: the ELF boots on
> hardware, brings up video and the controller, and reports which module
> groups loaded. The device browser, application launcher and installer
> are not implemented yet. See [CHANGELOG.md](CHANGELOG.md).

## What it is not

AtlasPS2 contains no Sony code, BIOS, firmware, keys or artwork, and ships
no commercial games. It does not read `rom0:FONTM` or any other ROM
resource — the interface font is baked from DejaVu Sans at build time.
Third-party homebrew such as Open PS2 Loader stays a separate component
under its own licence; AtlasPS2 launches it, it does not embed it.

## Building

You do not need to install a toolchain. With Docker:

```sh
docker build -t atlasps2/build .
docker run --rm -v "$PWD:/src" -w /src atlasps2/build make
```

The ELF lands in `build/ATLASPS2.ELF`. `make debug` builds the same thing
with on-screen logging and debug symbols. Full instructions, including a
native toolchain setup, are in [docs/BUILD.md](docs/BUILD.md).

On Windows, run Docker from Git Bash with `MSYS_NO_PATHCONV=1` prefixed,
or MSYS rewrites the container paths.

## Layout

```
include/atlas/   public headers, one per subsystem
src/boot/        IOP reset and IRX module loading
src/video/       AtlasVideo: GS setup, modes, safe area
src/input/       controller handling
src/ui/          font rendering and the interface
src/ui/assets/   baked font atlases (generated, committed)
tools/           build-time asset generation
docs/            architecture, build, install, recovery
```

## Safety

The rules below are structural, not aspirational — they are why the design
looks the way it does.

- The working `BOOT.ELF` is never overwritten in place. Updates write
  `BOOT.NEW`, keep `BOOT.BAK`, and roll back on failure.
- The installer backs a Memory Card up before touching it, and never
  blindly deletes `BOOT`, `APPS` or `SYS-CONF`.
- No bootstrap variant is installed by guessing. If the right one cannot
  be determined safely, installation stops with an explanation.
- Recovery mode (hold **L1+R1** at boot) draws with no theme, no
  configuration and minimal graphics, so a bad theme or a corrupt
  `ATLAS.INI` cannot lock you out.
- Safe video mode (hold **R1** at boot) forces NTSC 4:3 with no offsets,
  for when a video setting produced a picture your TV cannot show.
- Destructive file operations confirm first; system folders warn twice.

## Licence

MIT, with third-party components under their own terms. See
[LICENSE](LICENSE).
