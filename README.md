# AtlasPS2

A modern boot and menu environment for the PlayStation 2, built for real
hardware.

AtlasPS2 is what loads when the console powers on: it brings up the IOP,
finds your devices, lists the homebrew applications it discovers on them,
and launches them. It is a spiritual successor to FreeMcBoot's menu, with
a readable dark interface, an unbreakable Recovery mode, and an update
path that cannot brick the installation.

> **Status: pre-release, and untested on hardware.** All ten milestones
> are built: the launcher, the file manager, the installer, Recovery,
> themes, the first-boot wizard and the disc-image path. What has *not*
> happened is a console running any of it — every hardware row in
> [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md) reads `UNTESTED`, and
> that is a statement of fact rather than a placeholder. Booting a game
> from an image is the least verified part of all; the Games screen says
> so before it hands the console over. See [CHANGELOG.md](CHANGELOG.md).

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
with on-screen logging and debug symbols, `make installer` builds the
second program, and `make release` assembles a directory a user can
install from without ever opening this repository. Full instructions,
including a native toolchain setup, are in
[docs/BUILD.md](docs/BUILD.md).

`make check` runs the host self-checks — seventeen suites over the
pure-data half of every module that has one. They need no PS2 toolchain
and take about a second.

On Windows, run Docker from Git Bash with `MSYS_NO_PATHCONV=1` prefixed,
or MSYS rewrites the container paths.

## Layout

```
include/atlas/   public headers, one per subsystem
src/core/        strings, paths, INI, config, favourites, install
src/boot/        IOP reset and IRX module loading
src/video/       AtlasVideo: GS setup, modes, safe area
src/input/       controller handling
src/device/      device detection: MC0, MC1, USB
src/apps/        application discovery, ELF launch, disc boot
src/disc/        disc images: ISO/ZSO reading, ISO9660, per-game settings
src/ui/          font rendering and every screen
src/ui/assets/   baked font atlases (generated, committed)
iop/atlascdvd/   the IOP module a game reads its disc through
installer/       the second ELF, run once from a USB stick
tests/           host-side self-checks (`make check`)
lang/            generated FR/EN string files, editable overrides
assets/themes/   an example theme, ready to copy to a card
tools/           build-time asset and translation generation
docs/            architecture, build, install, video, discs, recovery,
                 themes, testing, compatibility
```

A game boots through `iop/atlascdvd/`, an IOP module that answers the
drive calls a disc game makes and serves the sectors out of a file on a
USB stick instead. It is written and it links, and it has never run on a
console — [docs/DISC.md](docs/DISC.md) says exactly what it covers, what
it refuses, and in what order to bring it up.

Themes are fifteen colours in a `theme.ini`; see
[docs/THEMES.md](docs/THEMES.md). The built-in theme is compiled in and
cannot be removed, so a theme file that is missing or broken costs you
colours and never the console.

[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) explains why the code is
shaped the way it is, and is the place to start if you intend to change
any of it.

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
