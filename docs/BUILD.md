# Building AtlasPS2

## With Docker (recommended)

The repository ships a `Dockerfile` that pins the toolchain, so you do not
have to install anything on your machine.

```sh
docker build -t atlasps2/build .
docker run --rm -v "$PWD:/src" -w /src atlasps2/build make
```

The result is `build/ATLASPS2.ELF`, around 640 KB stripped.

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

| Command       | Result                                                  |
|---------------|---------------------------------------------------------|
| `make`        | Release ELF, stripped, no debug strings                 |
| `make debug`  | Same plus `ATLAS_DEBUG` logging and DWARF symbols       |
| `make clean`  | Remove `build/`                                         |
| `make fonts`  | Re-bake the font atlases (needs Python, Pillow, DejaVu) |

`make fonts` is not part of a normal build: the baked atlases are
committed under `src/ui/assets/`, so building needs neither Python nor the
source TTF files. Regenerate only when changing the face or the pixel
size.

## With a native toolchain

If you already have ps2dev installed, only two environment variables
matter:

```sh
export PS2SDK=/usr/local/ps2dev/ps2sdk
export GSKIT=/usr/local/ps2dev/gsKit
export PATH="$PATH:/usr/local/ps2dev/ee/bin:/usr/local/ps2dev/iop/bin:$PS2SDK/bin"
make
```

The build needs the EE compiler (`mips64r5900el-ps2-elf-gcc`) and
`$(PS2SDK)/bin/bin2c`. It does not need the IOP compiler: AtlasPS2 embeds
prebuilt PS2SDK IRX modules rather than compiling its own.

Verified against ps2dev with gcc 15.2.0 and current PS2SDK/gsKit.

## What the build does

1. `bin2c` converts each IRX module from `$(PS2SDK)/iop/irx/` into a C
   array under `build/irx/`. These are embedded because at IOP-reset time
   no filesystem is mounted yet — these modules are what makes filesystems
   work.
2. The EE sources compile into `build/obj/`, mirroring the source tree.
3. Everything links against gsKit, dmaKit, libpatches, fileXio, padx, mc
   and poweroff, using the SDK's standard linkfile.
4. A release build then strips the ELF.

Objects live under `build/` rather than beside their sources, so `make
clean` is a single `rm -rf` and the tree stays readable.

## Running it

- **On hardware:** copy `ATLASPS2.ELF` to a USB stick or Memory Card and
  launch it from uLaunchELF, or install it as the boot ELF with the
  AtlasPS2 installer (not yet available — see the roadmap).
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
