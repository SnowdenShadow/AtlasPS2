# Compatibility

What has actually been tested, on what, with what result.

## How to read this

Every cell is one of four words, and only one of them means anything good:

| Value | Meaning |
|---|---|
| `WORKS` | Someone ran this, on this hardware, and it did what it should. |
| `PARTIAL` | It runs, with a specific limitation named in the Notes column. |
| `BROKEN` | It was tried on this hardware and it failed. |
| `UNTESTED` | **Nobody has tried it.** Not "probably fine". |

`UNTESTED` is the default and it is not a placeholder for optimism. A row
moves off it when a person reports a result from a physical console —
not from a build that compiled, not from an emulator, and not from
reasoning about the code.

> **An emulator passing proves nothing about hardware.** PCSX2 is far more
> forgiving than a real console about IOP timing, module load order,
> Memory Card behaviour and USB enumeration — which is precisely the set
> of things this program spends its time on. A row tested only under
> PCSX2 stays `UNTESTED` here and is noted as such.

## Console and feature matrix

One physical console has now run the launcher; the model was not
recorded, so it is a row of its own rather than a claim about any of the
models below. Everything else here is still `UNTESTED`.

| Console model | ROM | Region | Boot method | AtlasPS2 | USB | HDD | MX4SIO | Video | Notes |
|---|---|---|---|---|---|---|---|---|---|
| SCPH-30000 series (fat, early) | UNTESTED | NTSC-J | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | Expansion bay present |
| SCPH-3900x (fat) | UNTESTED | NTSC-U | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | |
| SCPH-500xx (fat) | UNTESTED | PAL | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | |
| SCPH-700xx (slim) | UNTESTED | NTSC-U | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | No expansion bay; HDD not possible |
| SCPH-750xx (slim) | UNTESTED | PAL | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | |
| SCPH-770xx (slim) | UNTESTED | NTSC-U | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | |
| SCPH-900xx (slim, late) | UNTESTED | PAL | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | Integrated PSU; late ROM revisions |
| Unrecorded model (reporter's console) | UNTESTED | UNTESTED | uLaunchELF | PARTIAL | UNTESTED | UNTESTED | UNTESTED | WORKS | Boots to the interface; see below |
| PCSX2 (emulator) | n/a | both | ELF from folder | PARTIAL | UNTESTED | UNTESTED | n/a | PARTIAL | Development only; see below |

### The one hardware result

Launched from uLaunchELF. The first attempt was a black screen: the IOP
reset wait was bounded in loop iterations rather than in time, and a real
IOP takes hundreds of milliseconds where PCSX2 takes none, so the wait
expired and `main()` returned before video was ever brought up. With
unbounded waits the console reaches the interface and draws it.

`PARTIAL` and not `WORKS`, because that is all that has been confirmed.
Nobody has yet reported a Memory Card write, a USB stick enumerating, an
application launching, or a game booting on that console.

The second report from the same console reached the first-boot wizard
and **no button did anything**, which is a separate fault from the black
screen and was fixed separately: `padInit()` was rejected on a success
code the header does not document, `padPortOpen()` was tried once where
PADMAN needs a moment after an IOP reset, `padGetState()` returning
`DISCONN` on the first sample was read as "no pad" rather than "not
awake yet", and pad initialisation was skipped entirely whenever the
module load reported failure — which it does when a previous loader
left PADMAN resident. **Not yet confirmed on hardware.**

### What "PCSX2 PARTIAL" means here

The ELF loads under PCSX2, brings up the GS, reads the controller, walks
`mc0:` and draws the interface. That is what "development only" covers.
It says nothing about whether a physical Memory Card write completes,
whether a real USB stick enumerates, or whether a given television
accepts a given video mode — the three areas where hardware and emulator
diverge most.

## Feature status, independent of console

This is about what is implemented, not about what has been verified on
hardware. A feature can be `implemented` here and `UNTESTED` above at the
same time, and most are.

| Feature | Status | Notes |
|---|---|---|
| Boot, IOP reset, module loading | implemented | |
| Video: AUTO / NTSC / PAL / 480p | implemented | 480p is progressive output, marked experimental in the UI |
| Safe video mode (hold R1) | implemented | Forces NTSC 4:3, no offsets |
| Recovery mode (hold L1+R1) | implemented | No theme, no config read, minimal drawing |
| Memory Card browsing (mc0, mc1) | implemented | |
| USB mass storage | implemented | `bdm` + `bdmfs_fatfs` + `usbmass_bd` |
| Internal HDD: `__common` browsing | implemented, read-only | `ps2dev9`/`ps2atad`/`ps2hdd`/`ps2fs`; only the `__common` PFS partition is mounted, read-only |
| Internal HDD: HDL game partitions (listing) | implemented, **UNTESTED** | Detected via `hdd0:` APA enumeration (`APA_TYPE_HDL`) and `fileXioIoctl2()`; see below |
| Internal HDD: HDL game partitions (booting) | written, **never run** | Raw `sceAtaDmaTransfer()` reads, no filesystem; see below and [DISC.md](DISC.md) |
| MX4SIO (SD over the memory card port) | **not implemented** | Needs a driver this repository does not contain |
| Network (SMB, host:) | **not implemented** | No network stack is linked |
| ELF launching | implemented | `LoadExecPS2` after cleanup |
| Application discovery + `app.ini` | implemented | |
| Favorites and recently used | implemented | |
| File manager with confirmations | implemented | System paths warn twice |
| Configuration with `.BAK` recovery | implemented | |
| Themes | implemented | Built-in theme is compiled in and cannot be removed |
| French / English | implemented | 283 strings, French is the reference |
| Installer: install / update / repair | implemented | |
| Installer: backup / restore / uninstall | implemented | |
| Transactional boot swap with rollback | implemented | `BOOT.NEW` → verify → `BOOT.BAK` swap |
| Per-title profiles | implemented | Parsed, formatted and applied to video settings |
| Compatibility database (per-game) | implemented | Read from a user-editable file; ships empty |
| Disc image reading (ISO/ZSO) | implemented | Listed, identified, and read sector by sector |
| Booting a game from an image (USB) | written, **never run** | The IOP module builds and links; no console has executed it — see [DISC.md](DISC.md) |
| Booting an HDL game (internal HDD) | written, **never run** | Same IOP module, raw ATA reads instead of a filesystem — see [DISC.md](DISC.md) |
| Bootstrap / exploit installation | **deliberately absent** | See below |

### Why MX4SIO and network are listed as not implemented

Because they are. Each needs IOP driver modules that are not in this
repository, and a menu entry that opens onto a device that cannot mount
is worse than an absent one — it reads as a fault in the user's hardware
rather than an absent feature. When the drivers are added, these rows
change and the device manager grows the entries.

### What the internal HDD support does and does not do

`ps2fs.irx` is ps2sdk's own PFS driver, not a Sony binary — the same
posture as `atlascdvd` for disc emulation. Only the `__common` partition
is mounted, and only read-only (`FIO_MT_RDONLY` at the `fileXioMount()`
call, not just "nothing calls write"). Writing to the HDD or formatting
it are **not implemented** — not attempted here given the real data on
a user's physical drive.

A drive holding HDLoader-style game installs (as made by "HDLB"/HDL
Batch and similar tools) has one APA partition per game, type
`0x1337` (`APA_TYPE_HDL`), separate from `__common` and never a PFS
filesystem — so listing these needed a second code path, added
alongside `__common` browsing rather than replacing it:

- **Detection and listing.** Independent of whether `__common` mounts -
  a drive written entirely by HDLoader-style tools never has one, and
  that alone must not hide every game on it. `hdd0:` is enumerated
  directly (`fileXioDopen`/`fileXioDread`), filtering on `stat.mode ==
  APA_TYPE_HDL`. For each match, `fileXioIoctl2()` with
  `HIOCGETPARTSTART`/`HIOCGETSIZE` recovers the partition's start
  sector and size; the game data itself starts `0x2000` (512-byte ATA)
  sectors into the partition — a fixed 4 MB header every HDL installer
  writes — confirmed from `hdl-dump`'s source, not guessed. Enumerating
  `hdd0:` returns a dirent for every partition in the chain, mains and
  subs alike, and a sub's dirent carries its *main's* name rather than
  its own (`hddDread()`/`fioGetStatFiller()` in ps2sdk's
  `iop/hdd/apa/src/hdd_fio.c`) — so a game split into a main plus two
  subs would surface as three identical-looking rows if subs were not
  filtered out. `stat.attr & APA_FLAG_SUB` is what tells a sub's dirent
  from its main's, and is checked first, before anything else runs.
  What's left after that filter is one row per real install; a main
  with subs (`stat.private_0 > 0` on that surviving dirent, now
  unambiguous) is a multi-slice install for a very large game, listed
  but marked unstartable, since the boot side only knows how to read
  one contiguous run. **Never run on a console** — if a game does not
  show up, or shows with "format not recognised", that is the first
  thing to report; it means one of the two ioctl2 assumptions above
  needs correcting.
- **Booting.** Reuses the exact same `atlascdvd` IOP module that boots
  a USB image, with one more branch in `bd_read()`: instead of reading
  through `bdm`, it calls `sceAtaDmaTransfer()` directly — the same
  function, same signature, `ps2hdd.irx`/PFS not involved at all — the
  same way OPL's own HDD backend does it. No filesystem is walked; the
  game's data is treated as one contiguous run of sectors, known
  entirely from the listing step above. **Never run on a console.**

### Why bootstrap installation is deliberately absent

Which exploit variant a console needs depends on its ROM version and
region, there is no way to determine that reliably from software on every
model, and installing the wrong one can leave a Memory Card the console
refuses to boot from. Guessing costs the user their card. So AtlasPS2's
installer stops and says so instead, and installing AtlasPS2 onto a
console that already runs homebrew — which is safe — is the only
operation offered.

## Per-game compatibility

Per-game workarounds live in a **user-editable file**, not in this
document and not compiled into the program:

```
mc0:/ATLAS/CONFIG/COMPAT.INI
```

AtlasPS2 ships **no entries of its own**. That is deliberate: a table
baked into the ELF would be a compatibility list correctable only by
rebuilding, for a body of knowledge that players collect and we do not.
The format is one section per game:

```ini
[SLUS-20902]
force_dvd = 1
vmode = pal
```

See [DISC.md](DISC.md) for what each flag means. These entries are read
and applied, but no game has been booted on a console yet, so none of
them has been confirmed to do anything on hardware.

### Results

One row per title actually attempted. Empty is the honest state: nobody
has tried.

| Game ID | Title | Format | Console | Result | Notes |
|---|---|---|---|---|---|
| — | — | — | — | — | No attempts recorded |

`Format` is `ISO` or `ZSO`. `Result` uses the same four words as above.
The pair matters: the same title as an ISO and as a ZSO must behave
identically, and a difference between them is an indexing bug rather
than a compatibility one — worth its own row and its own note.

The steps that produce a row, in the order that makes a black screen
diagnosable, are in [TESTING.md](TESTING.md#games-and-disc-images).

## Reporting a result

What is needed to fill in one row:

1. The console model from the label underneath (`SCPH-xxxxx`).
2. The ROM string, which the installer shows on its **Console** line.
3. The region and how the console boots homebrew.
4. Which of the columns you exercised, and what happened.
5. Whether this was physical hardware or an emulator. **Say which.**

A `BROKEN` report is more useful than a missing row. It is also more
useful than a `WORKS` that was actually an emulator.
