# Testing checklist

Three lanes, in increasing cost and increasing value:

1. **Host self-checks** — `make -C tests check`. Run on every change.
2. **PCSX2** — fast iteration on the parts that behave the same.
3. **Real hardware** — the only lane that can move a row in
   [COMPATIBILITY.md](COMPATIBILITY.md) off `UNTESTED`.

> **Emulator success alone does not prove hardware compatibility.** PCSX2
> is forgiving about IOP timing, module load order, Memory Card write
> behaviour and USB enumeration — which is most of what this program
> does. Every item below marked **hardware** must be done on a physical
> console before it counts.

## Lane 1 — Host self-checks

```sh
make -C tests check
```

Eighteen suites covering the pure-data half of every module that has
one: UTF-8, paths, INI grammar, the string table, configuration,
hashing, themes, favorites, protected paths, disc identification, LZ4,
ZSO indexing, per-game compatibility, fragment lists, sector arithmetic,
per-title profiles, the IOPRP module list, and screen layout.

These exist because the modules were split for them. A theme's colours, a
protected-path decision, a game ID, the meaning of a profile key — each
is a value that is wrong invisibly, and each would otherwise need a
television to check. They run in about a second and catch the class of
mistake that costs an afternoon on hardware.

`test_layout` is the newest and was written after the fact. Each list
screen used to carry a hand-counted row constant — eight here, nine
there — chosen against one video mode and never revisited when the row
height grew or when PAL handed the screen sixty-four more lines. A list
that asks for more rows than fit does not fail; it draws the extra ones
over the footer. The suite checks the invariant rather than the number:
for both fields and every reserve any screen uses, the last row must end
above the footer.

They are **not** a substitute for any item below. Nothing here touches
the GS, the pad, the IOP or a filesystem.

`test_image` compares the same disc as an ISO and as a ZSO, sector by
sector. Those two files are generated rather than committed — 200 KB of
data with no history worth keeping — so `check` builds them first, into
`build/testimg/`. That needs Python's `lz4` module, which is the
*reference* compressor and is the whole point: a decoder checked only
against its own encoder can be uniformly wrong and still agree with
itself. The Docker image carries it; on a native toolchain,
`pip install lz4`.

Without it the check prints a note and `test_image` reports `SKIPPED`.
That is a gap in coverage, not a pass — the ISO/ZSO pairing is what
found two real bugs in the ZSO reader that no fixed expected value could
have, because a wrong index entry returns real data, correctly
decompressed, from the wrong place.

## Lane 2 — Build checks

```sh
make            # release ELF, zero warnings expected
make debug      # same with ATLAS_DEBUG logging and symbols
make installer  # the separate installer ELF
make installer DEBUG=1
make all-elf    # everything at once
make lang       # regenerate lang/en.ini and lang/fr.ini from the table
```

A warning is a failure. The release build is expected to be silent.

## Lane 3 — The hardware checklist

Each item says what to do and what should happen. Anything that does not
match is a result worth reporting, including the ones that merely look
untidy.

### Cold boot

- [ ] **hardware** Console off for at least a minute, then on with no
      button held. AtlasPS2 appears after the Sony logo, without a black
      gap longer than a couple of seconds.
- [ ] **hardware** Immediately power-cycle and boot again. Same result —
      a first boot that works and a second that does not points at
      something written during the first.
- [ ] **hardware** Boot with the USB stick already inserted, and again
      with it inserted after the menu appears. Both should reach the
      menu; the second may need **Devices** → refresh to show the stick.

### Controller

- [ ] **hardware** Unplug the controller at the menu. The interface stays
      up and stays drawn; it must not hang or reboot.
- [ ] **hardware** Plug it back in. Input resumes without a restart.
- [ ] **hardware** Boot with no controller plugged in at all, then plug
      one in. It should be picked up.
- [ ] **hardware** Controller in port 2 only. Note the result — port 1 is
      what is assumed.

### Devices

- [ ] **hardware** No USB stick present. **Devices** lists the Memory
      Cards and says the USB slot is empty. No error screen, no hang.
- [ ] **hardware** Memory Card in slot 1 only, then slot 2 only, then
      both. Each card is listed with its free space, and the free space
      is plausible (a nearly-full 8 MB card should not read as empty).
- [ ] **hardware** An empty, freshly formatted Memory Card. Browsing it
      shows an empty card, not an error.
- [ ] **hardware** A **full** Memory Card. The installer must refuse with
      an insufficient-space message rather than starting and failing
      partway.
- [ ] **hardware** A USB stick the PS2 does not like (large, or a brand
      known to be awkward). The menu still comes up; the stick is simply
      absent.
- [ ] **hardware** Remove the USB stick while browsing it. The file
      manager reports the failure and returns; it must not hang.

### Configuration

- [ ] **hardware** Corrupt `ATLAS/CONFIG/ATLAS.INI` deliberately (random
      bytes, or truncate it). AtlasPS2 recovers from `ATLAS.INI.BAK`,
      says so, and reaches the menu.
- [ ] **hardware** Delete both the config and its `.BAK`. The first-boot
      wizard appears — that is what "no file at all" means.
- [ ] **hardware** A config file with unknown keys and a couple of
      nonsense values. The known keys apply, the rest are ignored, and
      the console reaches the menu.
- [ ] **hardware** Make the Memory Card read-only in practice (fill it),
      then change a setting. The failure is reported and the console
      keeps running.

### Themes

- [ ] **hardware** Delete the selected theme's folder. The built-in theme
      is used, the menu is readable, and it says the theme is missing.
- [ ] **hardware** A `theme.ini` with a malformed colour. The remaining
      colours load; the bad one falls back.
- [ ] **hardware** A theme folder that exists but is empty.

### Applications

- [ ] **hardware** An `ATLAS/APPS` folder with several ELFs. All are
      listed; those with an `app.ini` show its name and description.
- [ ] **hardware** Delete an ELF that is in the favorites list, then open
      **Favorites**. The entry reports that it is missing rather than
      launching into nothing.
- [ ] **hardware** Launch an application and return from it (by powering
      off and on). The favorites and recently-used lists survive.
- [ ] **hardware** Set an application as `default_app` with a timeout,
      reboot, and let the countdown run out. It launches.
- [ ] **hardware** Same again, but press a button during the countdown.
      It cancels and the menu appears.
- [ ] **hardware** Set `default_app` to a path that no longer exists.
      There must be no countdown at all — the check happens first.

### Video

- [ ] **hardware, PAL console, PAL TV** Each mode: AUTO, PAL, NTSC, 480p.
      Note which ones the television accepts.
- [ ] **hardware, NTSC console** The same four.
- [ ] **hardware** 4:3 and 16:9 on a 4:3 television, then on a widescreen
      one. The safe area must stay inside the visible picture in all
      four combinations.
- [ ] **hardware** Screen offsets at their limits (±32) and overscan at
      its limit (64). The interface stays on screen and nothing is cut.
- [ ] **hardware** Change to a mode the television cannot display, and do
      **not** confirm. It must revert on its own within fifteen seconds.
- [ ] **hardware** Do the same, and power-cycle before the countdown
      finishes. The saved setting must still be the old one.
- [ ] **hardware** Hold **R1** at boot. Safe video: NTSC, 4:3, no
      offsets, on a television that showed nothing before.

### Recovery

- [ ] **hardware** Hold **L1 + R1** at boot. Recovery appears, with no
      theme applied and no configuration read.
- [ ] **hardware** Reach Recovery with a *corrupt* config and a *broken*
      theme in place. This is the case it exists for: it must still draw
      and still be navigable.
- [ ] **hardware** Recovery → **Reset settings**, then continue. Defaults
      are in effect.
- [ ] **hardware** Recovery → **Disable theme**, then continue.
- [ ] **hardware** Recovery → **Change card**, with a card in slot 2.
- [ ] **hardware** Recovery → **Return to browser**. The console's own
      menu appears.

### Installer

- [ ] **hardware** Install onto a card that has never had AtlasPS2. All
      five steps complete; the console boots into AtlasPS2 afterwards.
- [ ] **hardware** Confirm `ATLAS/BACKUP/BOOT.ELF` now holds what booted
      the console before, and that `ATLAS/BACKUP/ORIGINAL.TXT` is there.
- [ ] **hardware** Install again over an existing installation (update).
      `BOOT.BAK` holds the previous AtlasPS2 build; `ATLAS/BACKUP` is
      **unchanged** — it must not have been rewritten.
- [ ] **hardware** Repair on a good installation. It completes and
      changes nothing visible.
- [ ] **hardware** Uninstall. The console boots as it did before
      AtlasPS2 existed.
- [ ] **hardware** Uninstall on a card with no `ATLAS/BACKUP`. The entry
      is unavailable and says why, rather than doing something.
- [ ] **hardware** Existing user files on the card — an unrelated `APPS`
      folder, saves, another launcher's config — are still there after
      an install.

### Failed update and rollback

The point of the transaction is that the failure cases are safe, so they
have to be *provoked*, not waited for.

- [ ] **hardware** Start an install and **pull the Memory Card during the
      copy step**. The console must still boot as it did before: the live
      `BOOT.ELF` was never opened for writing.
- [ ] **hardware** Do the same during the verify step. Same requirement.
- [ ] **hardware** Corrupt `BOOT.NEW` between a copy and a verify (if you
      can arrange it). The verify must fail and the swap must not happen.
- [ ] **hardware** After a successful update, Recovery → **Roll back**.
      The previous AtlasPS2 build boots.
- [ ] **hardware** Roll back twice. It returns to where it started — it
      is a swap, not a one-way move.
- [ ] **hardware** Update from `ATLAS_UPDATE/ATLASPS2.ELF` on USB via
      Recovery, with the USB stick removed. It reports no source rather
      than doing anything.

### Games and disc images

This is the least verified part of the program, and the checklist is
written to separate two failures that look identical on a television: the
*list* being wrong, and the *drive emulation* being wrong. Do the first
five items before attempting the sixth — a black screen is much easier to
reason about when the rows above it are known good.

- [ ] **hardware** No USB stick. **Games** says there is no USB stick
      connected, and says nothing about images. It must not read as "no
      games found" — those are different problems with different fixes.
- [ ] **hardware** A USB stick with no images. It says no images were
      found and names the folders it looked in.
- [ ] **hardware** Images at the stick's root, in `DVD/`, in `CD/`, in
      `ISO/` and in `ATLAS/GAMES/`. All are listed, each exactly once —
      the roots overlap deliberately and a file must not appear twice.
- [ ] **hardware** A file named `game.ISO` in capitals, and one named
      `.iso`. Both are listed; the suffix test is case-insensitive.
- [ ] **hardware** A folder *inside* a folder inside the root. It is
      **not** listed — the scan is one level deep on purpose.
- [ ] **hardware** A file renamed to `.iso` that is not a disc image at
      all (a text file will do). It appears in the list, and choosing it
      reports an unrecognised format in the dialog **before** anything
      is handed to the IOP.
- [ ] **hardware** Remove the stick, then choose a row that was listed
      from it. The dialog reports the file is gone; the console stays up.
- [ ] **hardware** Choose a real PS2 image. The dialog shows the game ID,
      the region and the compatibility entry if there is one, plus the
      warning that this path has never run on hardware. **Circle must
      cancel and return to the list with the console untouched.**
- [ ] **hardware** Now confirm. Record what happens — a booting game, a
      black screen, a reset loop, or the console returning to its own
      browser are all results worth writing down. Note the model, the
      image format (ISO or ZSO) and the game ID in
      [COMPATIBILITY.md](COMPATIBILITY.md).
- [ ] **hardware** If it boots: play for several minutes, including
      something that streams (a cutscene, in-game music). Sector sizes
      other than 2048 are read by exactly that kind of code, and a wrong
      answer there plays every audio channel at once rather than
      failing outright.
- [ ] **hardware** A ZSO of the same title as an ISO that worked. Both
      must behave identically; a difference is an indexing bug, not a
      compatibility one.
- [ ] **hardware** An image on a Memory Card. It must **not** be listed —
      the drive emulation reads USB only, and a row that always refuses
      is worse than no row.

### Profiles and per-game settings

- [ ] **hardware** A profile naming only `aspect_ratio`. The screen
      offsets set globally must be **unchanged** afterwards.
- [ ] **hardware** A profile with `offset_x = 0`. That must centre the
      screen, not be treated as "unset".
- [ ] **hardware** A profile file full of nonsense keys. It behaves
      exactly like no profile.

### Long-running

- [ ] **hardware** Leave AtlasPS2 at the menu for an hour. It is still
      drawing, still responsive, and the frame rate has not changed.
- [ ] **hardware** Navigate every screen in turn, twenty times over.
      Memory use must not grow — screens are static instances, so a
      leak here would be a real bug.

## Recording the results

Fill in the row in [COMPATIBILITY.md](COMPATIBILITY.md), and say whether
it was hardware or an emulator. A `BROKEN` with a model number and a
description of what happened is worth more than any number of untested
rows that look reassuring.
