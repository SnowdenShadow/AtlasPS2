# Building AtlasPS2

## With Docker (recommended)

The repository ships a `Dockerfile` that pins the toolchain, so you do not
have to install anything on your machine.

```sh
docker build -t atlasps2/build .
docker run --rm -v "$PWD:/src" -w /src atlasps2/build make
```

The result is `build/ATLASPS2.ELF`, around 780 KB stripped. Most of that
is the embedded IRX modules, not code — they are carried inside the ELF
because at IOP-reset time there is no filesystem to load them from.

### Windows

Run this from Git Bash, PowerShell or cmd. **From Git Bash you must
prefix the command:**

```sh
MSYS_NO_PATHCONV=1 docker run --rm -v "$PWD:/src" -w /src atlasps2/build make
```

Without it, MSYS rewrites the container path `/src` into a Windows path
and Docker fails with `the working directory 'C:/Program Files/Git/src'
is invalid`.

From PowerShell:

```powershell
docker run --rm -v "${PWD}:/src" -w /src atlasps2/build make
```

## Targets

| Command                | Result                                                  |
|------------------------|---------------------------------------------------------|
| `make`                 | Release ELF, stripped, no debug strings                 |
| `make debug`           | Same plus `ATLAS_DEBUG` logging and DWARF symbols       |
| `make installer`       | `build/ATLAS_INSTALLER.ELF`, the second program         |
| `make installer DEBUG=1` | The installer with logging (there is no `installer-debug`) |
| `make all-elf`         | Both ELFs, one command — what a release needs           |
| `make check`           | The host self-checks; no toolchain required             |
| `make lang`            | Regenerate `lang/en.ini` and `lang/fr.ini`              |
| `make release`         | Assemble `release/AtlasPS2-vX.Y.Z/` and its checksums   |
| `make clean`           | Remove `build/`                                         |
| `make fonts`           | Re-bake the font atlases (needs Python, Pillow, DejaVu) |

`make fonts` is not part of a normal build: the baked atlases are
committed under `src/ui/assets/`, so building needs neither Python nor the
source TTF files. Regenerate only when changing the face or the pixel
size.

The installer is deliberately not part of `all`. It is what a user runs
once from a USB stick, and rebuilding it on every launcher build doubles
the compile time of a program that changes far less often.

## With a native toolchain

If you already have ps2dev installed, only two environment variables
matter:

```sh
export PS2SDK=/usr/local/ps2dev/ps2sdk
export GSKIT=/usr/local/ps2dev/gsKit
export PATH="$PATH:/usr/local/ps2dev/ee/bin:/usr/local/ps2dev/iop/bin:$PS2SDK/bin"
make
```

The build needs the EE compiler (`mips64r5900el-ps2-elf-gcc`),
`$(PS2SDK)/bin/bin2c`, **and** the IOP compiler
(`mipsel-none-elf-gcc`). Most of the IRX modules are prebuilt ones taken
from the SDK, but `iop/atlascdvd/` is ours and is compiled from source —
it is the module a game reads its disc through, and it cannot be anything
but built here. `python3` is needed too, for the export-ordinal check
that runs before that module links; it is not needed for anything else.

Verified against ps2dev with gcc 15.2.0 and current PS2SDK/gsKit.

## What the build does

1. `iop/atlascdvd/` builds with the IOP compiler. Its export table is
   checked against the cdvdman ordinals by `tools/checkexports.py`
   **before** the link — a table that has drifted stops the whole build,
   because the alternative is a module that loads and answers the wrong
   call.
2. `bin2c` converts that module, and each prebuilt IRX from
   `$(PS2SDK)/iop/irx/`, into a C array under `build/irx/`. These are
   embedded because at IOP-reset time no filesystem is mounted yet —
   these modules are what makes filesystems work.
3. The EE sources compile into `build/obj/$(MODE)/`, mirroring the source
   tree. Release and debug objects live in separate trees: an object
   records nothing about the flags that built it, and a shared tree
   would let `make` after `make debug` relink debug objects into an ELF
   called a release build.
4. Everything links against gsKit, dmaKit, elf-loader, libpatches,
   fileXio, padx, mc and poweroff, using the SDK's standard linkfile.
5. A release build then strips the ELF.

Objects live under `build/` rather than beside their sources, so `make
clean` is a single `rm -rf` and the tree stays readable.

## Running it

- **On hardware:** copy `ATLASPS2.ELF` to a USB stick or Memory Card and
  launch it from uLaunchELF, or install it as the boot ELF with
  `ATLAS_INSTALLER.ELF` (`make installer`). The step-by-step version of
  the second path is in [INSTALL_EN.md](INSTALL_EN.md).
- **In PCSX2:** works for UI iteration, but USB and Memory Card behaviour
  differ enough from a console that anything device-related must be
  confirmed on real hardware before it counts as tested.

## Troubleshooting

**`sh: make: not found`** — you are using `ps2dev/ps2dev` directly instead
of the image built from this repository's `Dockerfile`. The upstream image
is Alpine and ships no make.

**`bin2c: not found`** — `$PS2SDK/bin` is not on your `PATH`, or `PS2SDK`
points at the wrong directory.

**Link errors mentioning `gsKit_*`** — `GSKIT` is unset. gsKit installs
outside `PS2SDK` and the Makefile adds `-L$(GSKIT)/lib` from that
variable.
