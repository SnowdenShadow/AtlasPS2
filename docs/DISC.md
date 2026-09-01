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

## The drive emulation, and what it does not cover yet

A game does not read files. It calls `cdvdman`, which talks to the
drive. To boot from a file, that module has to be replaced with one that
answers from the file instead — and it runs on the IOP, in the IOP's
address space, exporting an ABI that a hundred games' worth of code
already links against by ordinal.

That module is `iop/atlascdvd/`. It is built by the IOP toolchain, its
ordinals are checked against the SDK header by `tools/checkexports.py`
on every build, and `bin2c` embeds the resulting `.irx` in the launcher
ELF — there is no filesystem left to load it from at the point it is
installed. `src/apps/discboot.c` is the EE half: it identifies the image
while the user is still looking at a screen, then resets the IOP without
the stock drive modules, loads the device stack and this module, and
hands over.

**None of that is testable on a build machine, and getting it wrong
gives a black screen.** That is recoverable — the console is one power
cycle from the menu — but it is indistinguishable from a bad dump, and a
user cannot tell which they have. So the module is written to keep the
untestable part small: the FAT walk and the extent arithmetic live in
`src/disc/frag.c`, the sector framing in `src/disc/sector.c` and the
boot-list filter in `src/disc/btconf.c`, all three with host self-checks
over them, and what remains in the IOP module is sequencing and
hardware.

Its scope today is deliberately one thing: **ISO images on a USB block
device.** ZSO, HDD and SMB are each a change to that module's `bd_read()`
and nothing else, which is why that function is the only place in it
that knows where bytes come from.

The rest of this section is the contract, including the parts not yet
met. Every hardware row in [COMPATIBILITY.md](COMPATIBILITY.md) is
`UNTESTED`; nothing below should be read as a claim that it works.

### 1. The module the game calls

Replace `cdvdman`. Load it from EE RAM after the IOP reset, before the
game's own modules, with the real one excluded — a module cannot
register a library name that is already registered, so if the stock
`cdvdman` boots, ours loads and is never called.

Excluding it means changing the module list the IOP boots with, and
that list lives in `rom0:IOPBTCONF`. `discboot.c` reads the console's
own copy, `src/disc/btconf.c` removes the lines naming `CDVDMAN`,
`CDVDFSV` and `CDVDSTM`, and the result is packed into an IOPRP image
and handed to `SifIopRebootBuffer()`. The list is read rather than
written down because it differs between console revisions: a list baked
into this program would be a guess about somebody else's machine, and a
wrong guess is an IOP booting without a module it needed, after the last
screen.

That filter is the one part of this handover a build machine can check,
and `tests/test_btconf.c` checks it — including the near misses, where
a revision's `CDVDMAN2` must survive a filter that removes `CDVDMAN`.

`cdvdfsv` is what the EE's own `sceCdRead` calls arrive through, and it
imports `cdvdman`, so it cannot load in the window where `cdvdman` is
missing. It comes out of the boot list with the others and is loaded
again from `rom0:` once our module holds the name — at which point what
it links to is ours.

The export table must match the real module **by ordinal, not by name**.
Games bind to the numbers. A table with the right functions in the wrong
order links cleanly and calls the wrong one at runtime. This is what
`tools/checkexports.py` exists to catch: it compares
`iop/atlascdvd/exports.tab` against the SDK's own ordinals and fails the
build on a mismatch, so a drift cannot reach a console.

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
- `sceCdReadClock()` — the real RTC, reached through the mechacon; games
  use it for save timestamps. The module **fails this rather than
  answering**, along with the console ID, the NVM and the disc key: a
  fabricated clock timestamps every save the player makes, and a
  fabricated console ID lies to a game about the machine it is on. A
  title that genuinely needs one stops, which is visible, instead of
  misbehaving later, which is not.
- The stream calls (`sceCdStRead` and its family) for titles that stream
  audio through the drive interface. Exported, not yet implemented.

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
   moves until a game boots. **← the module is written to here, and is
   unverified on hardware.**
2. ZSO on top of that. One function, `bd_read()`, plus the block cache
   sizing in the section above.
3. IGR and poweroff — before wide testing, not after, because everything
   else is unpleasant to test without them.
4. Sub-sector modes, then streaming titles.
5. HDD and SMB.

Test on hardware at each step. There is no emulator whose `cdvdman`
behaviour is close enough to be evidence.

## Reading the code

| File | Contents |
|---|---|
| `src/disc/disc.c` | Identifying an image: `SYSTEM.CNF`, the game ID, the region |
| `src/disc/image.c` | ISO and ZSO reading on the EE, with the block cache |
| `src/disc/lz4.c` | ZSO block decompression |
| `src/disc/frag.c` | The FAT walk and the extent list — host-checked |
| `src/disc/sector.c` | 2048/2340/2352 framing — host-checked, and linked into the IOP module rather than copied |
| `src/disc/btconf.c` | Removing the drive modules from the IOP boot list — host-checked |
| `src/disc/compat.c` | What a compatibility flag means |
| `src/apps/discboot.c` | The EE half: identify, reset the IOP, install, hand over |
| `iop/atlascdvd/main.c` | The IOP module the game calls |
| `iop/atlascdvd/exports.tab` | The ordinals, checked against the SDK on every build |
