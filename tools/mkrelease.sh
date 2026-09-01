#!/bin/sh
#
# AtlasPS2 - assemble a release directory.
#
# Run through `make release`, which builds both ELFs and the language
# files first and then calls this. Running it by hand works too, as long
# as build/ already holds what it expects.
#
# WHY A SHELL SCRIPT AND NOT MORE MAKEFILE
# ----------------------------------------
# This is a sequence of copies and one checksum pass. Expressing it as
# make rules would mean a target per file for no incremental benefit -
# nothing here is expensive enough to want rebuilding separately, and a
# half-assembled release directory is worse than one that is rebuilt
# whole every time. So the directory is removed and rebuilt.
#
# WHAT THE ARCHIVE HAS TO ACHIEVE
# -------------------------------
# Someone who downloads it and never opens the source repository must be
# able to install AtlasPS2 and understand what they installed. That is
# why the install guides are copied in as .txt rather than linked, why
# LICENSES/ carries the actual licence text rather than a URL, and why
# README.txt repeats the safety rules instead of pointing at them.

set -e

VERSION="$1"
OUT="$2"

if [ -z "$VERSION" ] || [ -z "$OUT" ]; then
    echo "usage: mkrelease.sh <version> <output-dir>" >&2
    exit 1
fi

DIR="$OUT/AtlasPS2-v$VERSION"

# Both ELFs must exist before anything is copied. Assembling a release
# around a missing file and finding out from the checksum list is the
# kind of mistake that ships.
for f in build/ATLASPS2.ELF build/ATLAS_INSTALLER.ELF; do
    if [ ! -f "$f" ]; then
        echo "mkrelease: $f is missing - run 'make all-elf' first" >&2
        exit 1
    fi
done

rm -rf "$DIR"
mkdir -p "$DIR/USB/ATLAS/APPS" \
         "$DIR/USB/ATLAS/LANG" \
         "$DIR/USB/ATLAS/THEMES" \
         "$DIR/LICENSES"

# ---------------------------------------------------------------- #
# The programs                                                      #
# ---------------------------------------------------------------- #

cp build/ATLASPS2.ELF        "$DIR/"
cp build/ATLAS_INSTALLER.ELF "$DIR/"

# ---------------------------------------------------------------- #
# USB/ - copied to the root of a FAT32 stick, exactly as it stands  #
#                                                                   #
# The two ELFs appear a second time here on purpose. The install     #
# guide tells the user to copy the ELFs and the ATLAS folder to the  #
# stick; having a directory that IS the stick removes the step where  #
# they get that wrong.                                               #
# ---------------------------------------------------------------- #

cp build/ATLASPS2.ELF        "$DIR/USB/"
cp build/ATLAS_INSTALLER.ELF "$DIR/USB/"

cp lang/en.ini "$DIR/USB/ATLAS/LANG/"
cp lang/fr.ini "$DIR/USB/ATLAS/LANG/"

cp -r assets/themes/* "$DIR/USB/ATLAS/THEMES/"

# APPS is empty and has to survive the copy anyway: an empty folder is
# what tells a user where their homebrew goes.
cat > "$DIR/USB/ATLAS/APPS/PUT-YOUR-ELF-FILES-HERE.txt" <<'EOF'
Copy homebrew .ELF files into this folder.

They appear under Applications in AtlasPS2. A folder with an app.ini
next to the ELF shows the name and description from that file instead
of the filename; see the documentation for the keys.

This file itself does nothing and can be deleted.
EOF

# ---------------------------------------------------------------- #
# Documentation                                                     #
#                                                                   #
# .txt rather than .md: the reader is on Windows, double-clicking.   #
# ---------------------------------------------------------------- #

cp docs/INSTALL_FR.md "$DIR/INSTALL_FR.txt"
cp docs/INSTALL_EN.md "$DIR/INSTALL_EN.txt"
cp CHANGELOG.md       "$DIR/CHANGELOG.txt"

# ---------------------------------------------------------------- #
# LICENSES/                                                         #
# ---------------------------------------------------------------- #

cp LICENSE "$DIR/LICENSES/AtlasPS2-MIT.txt"

cat > "$DIR/LICENSES/README.txt" <<'EOF'
AtlasPS2 itself is MIT licensed; the text is in AtlasPS2-MIT.txt, which
also lists every third-party component and its licence.

The short version:

  AtlasPS2 code        MIT
  PS2SDK IRX modules   Academic Free License 2.0, embedded unmodified
  gsKit                Academic Free License 2.0
  DejaVu Sans          DejaVu Fonts License; the interface font atlas is
                       baked from it at build time. No font file is
                       redistributed as such.

AtlasPS2 contains no Sony code, BIOS data, firmware, encryption keys or
graphical assets, and no commercial games. All interface artwork is
original to this project.
EOF

# ---------------------------------------------------------------- #
# README.txt                                                        #
# ---------------------------------------------------------------- #

sed -e "s/@VERSION@/$VERSION/g" tools/release_readme.txt > "$DIR/README.txt"

# ---------------------------------------------------------------- #
# CHECKSUMS.txt                                                     #
#                                                                   #
# Over every file in the release except the checksum list itself.    #
# Paths are relative to the release directory so that a user can     #
# verify from inside it without editing anything.                    #
# ---------------------------------------------------------------- #

(
    cd "$DIR"
    find . -type f ! -name CHECKSUMS.txt | sed 's|^\./||' | sort \
        | xargs sha256sum > CHECKSUMS.txt
)

echo "release assembled: $DIR"
