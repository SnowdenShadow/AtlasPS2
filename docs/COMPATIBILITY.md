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

Nothing below has been run on physical hardware yet. This table exists so
that the first person who does has somewhere to put the answer.

| Console model | ROM | Region | Boot method | AtlasPS2 | USB | HDD | MX4SIO | Video | Notes |
|---|---|---|---|---|---|---|---|---|---|
| SCPH-30000 series (fat, early) | UNTESTED | NTSC-J | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | Expansion bay present |
| SCPH-3900x (fat) | UNTESTED | NTSC-U | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | |
| SCPH-500xx (fat) | UNTESTED | PAL | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | |
| SCPH-700xx (slim) | UNTESTED | NTSC-U | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | No expansion bay; HDD not possible |
| SCPH-750xx (slim) | UNTESTED | PAL | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | |
| SCPH-770xx (slim) | UNTESTED | NTSC-U | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | |
| SCPH-900xx (slim, late) | UNTESTED | PAL | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | UNTESTED | Integrated PSU; late ROM revisions |
| PCSX2 (emulator) | n/a | both | ELF from folder | PARTIAL | UNTESTED | UNTESTED | n/a | PARTIAL | Development only; see below |

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
| Internal HDD | **not implemented** | No `ATAD`/`HDD`/`PFS` modules are loaded |
| MX4SIO (SD over the memory card port) | **not implemented** | Needs a driver this repository does not contain |
| Network (SMB, host:) | **not implemented** | No network stack is linked |
| ELF launching | implemented | `LoadExecPS2` after cleanup |
| Application discovery + `app.ini` | implemented | |
| Favorites and recently used | implemented | |
| File manager with confirmations | implemented | System paths warn twice |
| Configuration with `.BAK` recovery | implemented | |
| Themes | implemented | Built-in theme is compiled in and cannot be removed |
| French / English | implemented | 281 strings, French is the reference |
| Installer: install / update / repair | implemented | |
| Installer: backup / restore / uninstall | implemented | |
| Transactional boot swap with rollback | implemented | `BOOT.NEW` → verify → `BOOT.BAK` swap |
| Per-title profiles | implemented | Parsed, formatted and applied to video settings |
| Compatibility database (per-game) | implemented | Read from a user-editable file; ships empty |
| Disc image reading (ISO/ZSO) | implemented | Listed, identified, and read sector by sector |
| Booting a game from an image | written, **never run** | The IOP module builds and links; no console has executed it — see [DISC.md](DISC.md) |
| Bootstrap / exploit installation | **deliberately absent** | See below |

### Why HDD, MX4SIO and network are listed as not implemented

Because they are. Each needs IOP driver modules that are not in this
repository, and a menu entry that opens onto a device that cannot mount
is worse than an absent one — it reads as a fault in the user's hardware
rather than an absent feature. When the drivers are added, these rows
change and the device manager grows the entries.

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
