# Booting a disc image

This describes what AtlasPS2 does with an ISO or ZSO today, and — in the
second half — the exact contract the missing piece has to satisfy before
a game will actually boot from one.

Read the second half before writing any of it. It is a list of the ways
this goes wrong on hardware, and every one of them was expensive for
somebody else first.

## What is implemented

Everything above the IOP boundary, and it is covered by self-checks that
run on the build machine (`make -C tests`):

| Module | What it does | Check |
|---|---|---|
| `src/disc/disc.c` | ISO9660 volume descriptors, `SYSTEM.CNF`, game ID, region | `test_disc` |
| `src/disc/lz4.c` | LZ4 block decompression | `test_lz4` |
| `src/disc/image.c` | ISO and ZSO as a flat array of 2048-byte sectors | `test_image` |
| `src/disc/compat.c` | Per-game workarounds and video mode, from a file the user edits | `test_compat` |

`atlas_image_read()` has the same signature as `atlas_disc_read_fn`, so
identification runs over an image file without either module knowing
about the other.

### Formats

**ISO** is the whole disc, byte for byte. Nothing to do.

**ZSO** is the same disc cut into blocks, each LZ4-compressed, with a
u32 index of block offsets. LZ4 decompression is a byte-copy loop with
no entropy decoding and no window state, which is why this format and
not another: the IOP is a 37 MHz R3000 that has to keep a game's audio
streaming fed while it decompresses.

**CSO is not supported.** Same idea, DEFLATE instead of LZ4. Huffman
decoding on the IOP is too slow to stream through, and a format that
works until a cutscene needs audio is worse than one that is absent.

**CHD is not supported.** Its hunk map, mixed codecs and CD framing are
a larger job than the rest of this put together, for a format the PS2
scene does not use.

The format is decided by the file's magic, never by its extension. A
renamed file is an ordinary thing to find on somebody's drive.

### Identification

`atlas_disc_probe()` scans LBA 16 through 31 for the primary volume
descriptor rather than assuming 16 — boot records and Joliet supplementary
descriptors push it along — then reads `SYSTEM.CNF` through the project's
existing INI parser, since that is the grammar it is written in.

The game ID is normalised: `cdrom0:\SLUS_209.02;1` becomes `SLUS-20902`.
An ID that does not fit the buffer is **refused, not truncated**: a
truncated ID silently matches another game's compatibility entry, and
the user has no way to see that happen.

Region comes from the four-letter prefix, and `VMODE=` in `SYSTEM.CNF`
overrides it — except that `VMODE=NTSC` never downgrades a prefix already
known to be PAL, which is a mastering artefact rather than an intent.

### Per-game settings

`ATLAS/CONFIG/COMPAT.INI`, one section per game:

```ini
[SLUS-20902]
force_dvd = 1
vmode = pal

[SLES-50490]
hide_tray = yes
slow_first_read = true
```

Booleans may be written `1`/`0`, `yes`/`no`, `true`/`false`, `on`/`off`,
in any case. Values are matched as whole words: `perhaps` is not a
boolean and `purple` is not a video mode, and both are logged and
skipped rather than being taken for their first letter.

Section names go through the same normaliser the disc does, so
`[SLUS_209.02]` and `[slus-20902]` both reach the entry the user meant.
A name that is not a game ID is rejected and logged — otherwise it
becomes an entry that never matches, and the user edits it forever with
no effect.

An unknown key is skipped, not fatal. A list shared between launchers
carries keys AtlasPS2 does not implement, and dropping the file over one
of them loses every entry that would have worked.

AtlasPS2 ships **no entries of its own**. A table baked into the ELF is
a compatibility list that can only be corrected by rebuilding, for a body
of knowledge that players collect and we do not.

| Flag | Meaning |
|---|---|
| `force_dvd` | Report the disc as a DVD even when the image is CD-sized. A few titles ask for DVD layer information and stop if they do not get it. |
| `hide_tray` | Never report the tray as having been opened. Some games poll tray status while streaming and treat any change as the disc being removed; with no physical tray, the honest answer is the one they abort on. |
| `slow_first_read` | Delay the first read. A few titles issue one before their own interrupt handlers are installed, assuming a physical drive takes tens of milliseconds; an emulated one answers before anything is ready to receive it. |
| `low_modules` | Leave the IOP memory map alone. A small number of games assume all of IOP RAM is theirs and write over whatever is there — which, with an emulated drive, is the driver they are reading through. |
| `no_disc_patch` | Do not patch the game's disc-detection routine. A few titles use the same routine for something else. |

## What is not implemented, and why

A game does not read files. It calls `cdvdman`, which talks to the
drive. To boot from a file, that module has to be replaced with one that
answers from the file instead — and it runs on the IOP, in the IOP's
address space, exporting an ABI that a hundred games' worth of code
already links against by ordinal.

**None of that is testable on a build machine, and getting it wrong
gives a black screen.** That is recoverable — the console is one power
cycle from the menu — but it is indistinguishable from a bad dump, and a
user cannot tell which they have.

So the IRX is not in this repository. Shipping an unverified one would
contradict the thing the rest of this project is built around, which is
that a user should never be unable to tell what went wrong. What follows
is the contract it has to meet, written down while it is fresh, so that
whoever has a console in hand is not rediscovering it.

### 1. The module the game calls

Replace `cdvdman` and `cdvdfsd`. Load them from EE RAM after
`IOP reset`, before the game's own modules, with the real ones excluded.

The export table must match the real module **by ordinal, not by name**.
Games bind to the numbers. A table with the right functions in the wrong
order links cleanly and calls the wrong one at runtime.

The calls that matter:

- `sceCdRead(lsn, sectors, buf, mode)` — asynchronous. Returns
  immediately; completion is signalled through the callback registered
  with `sceCdCallback()` and through `sceCdSync()`. A synchronous
  implementation that returns when the data is there will work in
  testing and deadlock in a game that waits on the callback.
- `sceCdGetDiskType()` — must answer `SCECdPS2DVD` or `SCECdPS2CD`. This
  is what `force_dvd` overrides.
- `sceCdDiskReady()`, `sceCdTrayReq()` — where `hide_tray` lives.
- `sceCdStatus()`, `sceCdGetError()`, `sceCdSync()`, `sceCdInit()`.
- `sceCdReadClock()` — from the real RTC; games use it for save
  timestamps.
- The stream calls (`sceCdStRead` and its family) for titles that stream
  audio through the drive interface.

### 2. Reads must survive the game's DMA

This is the part that turns a working prototype into an intermittent
one.

The game owns IOP RAM and programs DMA channels while your read is in
flight. Concretely:

- **Never DMA into a buffer the game gave you while the game may also be
  using it.** Read into your own, then copy.
- **Your buffers must be in memory the game will not reuse.** This is
  what `low_modules` is about: the default is to place modules high in
  IOP RAM, and a few games write over all of it.
- **The device stack is yours, not the game's.** ATA/HDD, `usbhdfsd` and
  SMB all run on the IOP alongside the game. Their interrupt handlers
  and yours must not assume the game has left the IOP in any particular
  state.
- **A read that fails must be retried, not reported.** A USB stick that
  NAKs once is normal; a game told the disc is unreadable is a crash.

The failure mode when this is wrong is not a black screen. It is a game
that runs for twenty minutes and then corrupts one texture, which is far
harder to attribute.

### 3. Sub-sector reads

The PS2 drive can be asked for 2048, 2340 or 2352 bytes per sector, and
some titles ask for the larger forms to get the subheader. An image
holds 2048-byte sectors, so the rest has to be synthesised: the mode
bytes and the subheader are derivable from the LSN.

The relevant part for **streaming**: a game reading audio expects the
drive to keep up. Decompressing a ZSO block per sector will not, so the
block cache is not an optimisation here — it is the difference between
audio playing and audio stalling. `src/disc/image.c` already caches the
most recently decompressed block for exactly this reason. On the IOP,
size it so a streaming read never crosses more blocks per second than
LZ4 can decompress at 37 MHz, and measure that rather than assuming it.

### 4. Video mode

`atlas_compat_vmode_for()` decides. An unknown region stays `AUTO` rather
than guessing NTSC: a PAL title told it is NTSC runs a fifth too fast
with the bottom of the picture cut off, and that reads as a bad dump
rather than as a setting the user can change.

Applying it means patching the GS mode the game sets, on the EE, after
the ELF is loaded and before it is entered.

### 5. In-game reset and poweroff

IGR is a pad handler that stays resident: on the button combination it
resets the IOP, restores the real `cdvdman`, and reloads AtlasPS2.
Without it, the only way out of a launched game is the power switch.

The poweroff hook must be re-registered after the IOP reset — the
game's reset drops it, and a console that will not power off from the
front button is a bug users report as a brick.

### 6. Patching the game

The disc-detection patch rewrites the routine a game uses to check for a
real disc. `no_disc_patch` turns it off for titles that use the same
routine for something else. This is per-game work, found by trying, and
belongs in `COMPAT.INI` — not in the ELF.

## Order of work

1. ISO only, from USB, one known-good game, no compression. Nothing else
   moves until a game boots.
2. ZSO on top of that.
3. IGR and poweroff — before wide testing, not after, because everything
   else is unpleasant to test without them.
4. Sub-sector modes, then streaming titles.
5. HDD and SMB.

Test on hardware at each step. There is no emulator whose `cdvdman`
behaviour is close enough to be evidence.
