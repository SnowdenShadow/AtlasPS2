# Architecture

This describes how AtlasPS2 is put together and, more usefully, *why* it
is put together that way. Most of the structure here exists to answer one
question: when something goes wrong on a console with no debugger, no
console output and no keyboard, what does the user see?

The answer that shaped everything below is "something readable, on a
screen that is already up". That is the reason video comes before
configuration, the reason the built-in theme cannot be removed, and the
reason half the modules are split in two.

## The two processors

A PlayStation 2 is two computers that talk over an RPC bridge.

The **Emotion Engine** (EE, a 300 MHz MIPS R5900 with 32 MB) runs
everything in `src/`. It draws, reads the pad, parses files, and decides
what to launch.

The **IOP** (a 37 MHz MIPS R3000 with 2 MB) owns every device. Memory
Cards, USB, the controller ports and the disc drive are all reached by
loading IRX modules onto it and calling them by RPC. The EE never touches
that hardware directly.

Two consequences run through the whole codebase:

- **The IOP is reset at start-up** ([src/boot/boot.c](../src/boot/boot.c)).
  Whatever launched AtlasPS2 — PS2BBL, OpenTuna, uLaunchELF — left its own
  modules resident, and 2 MB does not leave room to be generous. Resetting
  buys a module set we chose, at a version we know.
- **The modules are embedded in the ELF**, not loaded from a device. At
  reset time no filesystem is mounted, and the modules are precisely what
  makes filesystems work. `bin2c` turns each `.irx` into a C array at
  build time; the `IRX_LIST` in the [Makefile](../Makefile) is that list.

Module loading is grouped and every group except the file layer is
optional. A console with a dead USB port still reaches the Home screen
from its Memory Card, and the boot splash reports which groups came up so
that a missing device is a visible fact rather than a mystery.

## Boot order

[src/main.c](../src/main.c) is short on purpose, but its sequence is
load-bearing:

1. **Stash `argv[0]`.** It is the only record of where we were launched
   from, and it is gone the moment anything else uses `argv`.
2. **Reset the IOP.** Without it there is no pad and no filesystem.
   `ATLAS_EFATAL` here ends the program — drawing an error would require
   resetting the IOP again to get anywhere.
3. **Read the boot hotkeys.** L1+R1 is Recovery, R1 alone is safe video.
   Sampled over ~60 frames and latched, because libpad needs several
   frames to bring the port up while the user is already holding the
   buttons.
4. **Bring up video on safe defaults** — before reading any
   configuration. This costs one mode change and buys the property that
   every failure after this line has somewhere to be reported. A console
   that hangs probing a failing Memory Card hangs on a *picture*.
5. **Upload the fonts.** If they fail there is no way to explain anything,
   so the background flashes and we stop — visibly different from a hang.
6. **Splash**, reporting the module groups and the video mode.
7. **Start the device layer**, then read `ATLAS.INI`.
8. **Auto-launch**, if configured, cancellable with any button.
9. **Hand the frame loop to the screen stack.**

Step 7 is where the hotkeys are honoured, not inside the video module —
which has no idea a hotkey exists. Recovery reads no configuration at
all: a recovery mode that loads the file it exists to repair is not a
recovery mode.

## The screen stack

Every full-screen view is an `atlas_screen_t`
([include/atlas/screen.h](../include/atlas/screen.h)) with `enter`,
`leave`, `update` and `draw`. They form a stack: push shows a screen over
the current one, pop returns underneath. That is what makes Back
universal, and it means no screen has to know who opened it.

Screens are **static instances, not allocated**. The set is fixed at
build time and the depth is bounded at 8, so a menu system that cannot
fail to allocate is one fewer way to strand a user on a black screen.
Pushing past the limit is ignored rather than fatal — a navigation bug
should cost one unopened menu, not the session.

The loop in `atlas_screen_run()` does input, then **one device poll**,
then `update`, then draw. Polling one device per frame means a full sweep
costs four frames and no single frame pays for all four; it lives in the
loop rather than in each screen because the header indicators are drawn
everywhere, and **draw must never block on hardware**.

Recovery is a different *root*, not a screen pushed over Home. Home is
drawn from a configuration that was deliberately not read this boot, and
a Back that fell through to it would land the user inside the thing they
held two buttons to escape.

## The split pattern

Seven modules exist as a pair of files:

| Pure data | Device half |
|---|---|
| `core/config.c` | `core/config_io.c` |
| `core/fav.c` | `core/fav_io.c` |
| `core/fs_path.c` | `core/fs.c` |
| `ui/theme.c` | `ui/theme_io.c` |
| `video/video_cfg.c` | `video/video.c` |
| `disc/compat.c` | `disc/compat_io.c` |
| `disc/profile.c` | `disc/profile_io.c` |

The first column is arithmetic over bytes: parsing, clamping, path
joining, defaults. It has no fileXio, no gsKit and no PS2SDK dependency
beyond `tamtypes.h`, so it **compiles with the system compiler on the
build machine** and is covered by the suites in [tests/](../tests).
`make check` runs seventeen of them.

The second column is the part that opens files and talks to the GS. It
cannot be tested off-console and is therefore kept as thin as the split
allows.

This is not tidiness. The bugs that hurt on a console are the silent
ones — a game ID truncated into another game's compatibility entry, a
sector extent one short that returns real data from the wrong place. Both
are pure-data mistakes, and both are caught on the build machine in under
a second instead of on a television.

`src/disc/disc.c` takes the idea further and reads through a **callback**
rather than a path, so identification runs over a synthetic image in a
test and over a real ISO on hardware without either side knowing.

## Storage

[src/device/device.c](../src/device/device.c) presents every kind of
storage as a numbered slot with a mount path — `mc0:`, `mc1:`, `mass:`,
`hdd0:` — so nothing above it knows that a Memory Card is polled through
libmc while a USB stick appears as a mounted FAT volume.

Detection is slow (an empty card slot costs milliseconds; USB needs
seconds to enumerate after power-on), so the module is a **cache**, and
refreshing it is an `update`-half operation. `hdd0:` is enumerated but
not mounted.

Applications are **discovered, never registered**
([src/apps/app.c](../src/apps/app.c)): the user copies a folder or an ELF
onto a device and it appears. A registry is one more thing that goes
stale when a card moves between consoles. The catalogue is a fixed array
holding no pointers into scanned data, so a stick pulled mid-scan costs
the entries from that stick and nothing else.

## Handing the console over

`atlas_launch_elf()` is the one operation AtlasPS2 cannot take back. Once
the target ELF is running our code is gone from memory — no return, no
error screen, no recovery short of the power switch. So
`atlas_launch_check()` opens the file and reads its ELF header *first*,
while we are still on screen and able to say why not.

Booting a disc image is the same handover plus a drive to emulate, and it
gets its own document: [DISC.md](DISC.md). The short version is that
`iop/atlascdvd/` is an IOP module that answers a game's disc calls out of
a file, and everything that can fail while the user is still looking at a
screen is done before it is installed. It has never run on a console, and
the Games screen says so before it starts anything.

## Text, themes and the recovery guarantee

Strings live in an X-macro table in
[include/atlas/i18n.h](../include/atlas/i18n.h): one row carries the key,
the name it has in a file, and **both** translations. A key cannot be
added without a French string, because there is nowhere to put it that
the compiler accepts. Lookup is an array index, so drawing a screen costs
no string comparisons.

`lang/en.ini` and `lang/fr.ini` are generated from that table by
`make lang` and ship as **editable overrides, not as the source**. The
built-in copy is what Recovery draws with when the card that would have
held those files is exactly what failed.

Themes work the same way: fifteen colours, an X-macro table in
`theme.h`, a built-in palette compiled in that cannot be removed. A theme
file that is missing or broken costs colours and never the console.

## Configuration

`ATLAS.INI` is text because a binary blob is smaller, faster to parse,
and unrecoverable by the person holding the console. When the settings
are what stops AtlasPS2 from booting — a video mode this television
cannot show is the obvious case — the fix has to be something a user can
do with a card reader and Notepad.

So the parser ignores what it does not understand rather than rejecting
it, and `atlas_config_load()` **always** leaves a usable configuration
behind: a missing file, an unreadable device or a file full of nonsense
each ends in the safe defaults plus a note about what happened. There is
no path that hands the caller a half-populated struct, because the caller
is boot and it has nothing to fall back to.

An origin of `ATLAS_CFG_DEFAULTS` — no file found anywhere — is what
starts the first-boot wizard. A file that was found and repaired from its
`.BAK` reports `RECOVERED`, not `DEFAULTS`: asking that user to set their
language up again would treat a repaired file as an absent one.

## Memory

Nothing here allocates in a loop. The catalogues are fixed arrays, the
screens are static, the strings are `const char *` into rodata. Two
places call `malloc`, both bounded and both freed on the same path:
`ui/font.c` (one atlas upload) and `disc/image.c` (the ZSO block index
and decompression buffers, sized from the file's own header).

The budget that matters is not the EE's 32 MB, it is the GS's 4 MB of
VRAM: two framebuffers, a Z-buffer we do not use, and the font atlas.
That is why the font is an 8-bit alpha texture with a greyscale CLUT —
one atlas serves every colour in the interface, tinted at draw time.

## Two ELFs

The installer is a separate program
([installer/](../installer)) and it has to be: it writes the file the
console boots, so repairing a broken installation must not mean running
the broken installation. A launcher that cannot start is exactly when the
installer is needed.

But Recovery, inside the launcher, needs the same operations — reinstall
and rollback are the two reasons to be in Recovery at all. Two copies of
a transaction that swaps `BOOT.ELF` is two chances to get the rollback
wrong, and only one of them would ever be exercised on a given run. So
the engine is [src/core/install.c](../src/core/install.c), both ELFs link
it, and there is exactly one implementation of the swap. What differs
between the programs is where the new ELF comes *from*, not what happens
to it.

Two backups exist and they are not the same thing:

- `BOOT/BOOT.BAK` — the rollback slot for one transaction, overwritten by
  every install.
- `ATLAS/BACKUP/BOOT.ELF` — whatever booted the console *before*
  AtlasPS2 existed. Written once, never overwritten, because after the
  first update the rollback slot holds one of our own builds — and an
  uninstall that restored *that* would "remove" AtlasPS2 by reinstalling
  it.

Neither program installs a bootstrap or an exploit. Which variant a
console needs depends on its ROM version and region, getting it wrong can
leave a card the console refuses to boot from, and there is no software
method that determines it reliably on every model. So it stops and says
so.

## The build

One [Makefile](../Makefile) at the root builds the launcher; it invokes
`installer/Makefile` with `-f` so both programs share `build/` and the
generated IRX arrays rather than producing two copies of each.

Release and debug objects live in **separate trees** with a mode stamp
forcing a relink, because an object file records nothing about the flags
that built it — a shared tree would let `make` after `make debug` relink
debug objects into an ELF called a release build, still carrying every
log string.

`iop/atlascdvd/` is built by the IOP toolchain, a different compiler from
the one that builds everything else, and its export table is checked
against the cdvdman ordinals by `tools/checkexports.py` **before** the
link. An export table that has drifted stops the whole build, because the
alternative is a module that loads and answers the wrong call.

The font atlases under `src/ui/assets/` are generated and **committed**,
so an ordinary build needs neither Python nor the source TTFs.

## Where to start reading

| If you want to know | Read |
|---|---|
| How the console gets to a menu | [src/main.c](../src/main.c), [src/boot/boot.c](../src/boot/boot.c) |
| How a screen is written | [src/ui/screen_devices.c](../src/ui/screen_devices.c) — the smallest complete one |
| How a game boots from an image | [DISC.md](DISC.md), then [src/apps/discboot.c](../src/apps/discboot.c) |
| What is checked without hardware | [tests/](../tests) and `make check` |
| What must be checked *with* hardware | [TESTING.md](TESTING.md) |
