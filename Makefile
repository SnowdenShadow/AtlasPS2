# AtlasPS2 - root Makefile
#
# Build:
#     make            release ELF in build/
#     make debug      same, with ATLAS_DEBUG logging to the console
#     make clean
#
# Requires the ps2dev toolchain (PS2SDK, GSKIT). See docs/BUILD.md for the
# Docker one-liner that provides it without installing anything locally.

EE_BIN = build/ATLASPS2.ELF

# ------------------------------------------------------------------ #
# IRX modules                                                         #
#                                                                     #
# Embedded in the ELF rather than loaded from a device: at IOP-reset   #
# time no filesystem is mounted, and these modules are precisely what  #
# makes filesystems work. bin2c turns each .irx into a C array.        #
# ------------------------------------------------------------------ #

IRX_LIST = iomanX fileXio sio2man padman mcman mcserv \
           usbd bdm bdmfs_fatfs usbmass_bd poweroff

IRX_C   = $(IRX_LIST:%=build/irx/%_irx.c)
IRX_OBJ = $(IRX_LIST:%=build/irx/%_irx.o)

# ------------------------------------------------------------------ #
# Sources                                                             #
# ------------------------------------------------------------------ #

EE_SRC = \
	src/main.c \
	src/core/err.c \
	src/core/log.c \
	src/core/utf8.c \
	src/core/path.c \
	src/core/ini.c \
	src/core/i18n.c \
	src/core/file.c \
	src/core/hash.c \
	src/core/config.c \
	src/core/config_io.c \
	src/core/fav.c \
	src/core/fav_io.c \
	src/core/fs.c \
	src/core/fs_path.c \
	src/core/power.c \
	src/core/install.c \
	src/boot/boot.c \
	src/video/video.c \
	src/video/video_cfg.c \
	src/input/input.c \
	src/device/device.c \
	src/apps/app.c \
	src/apps/launch.c \
	src/ui/font.c \
	src/ui/theme.c \
	src/ui/theme_io.c \
	src/ui/ui.c \
	src/ui/screen.c \
	src/ui/screen_home.c \
	src/ui/screen_devices.c \
	src/ui/screen_apps.c \
	src/ui/screen_files.c \
	src/ui/screen_sysinfo.c \
	src/ui/screen_video.c \
	src/ui/screen_theme.c \
	src/ui/screen_power.c \
	src/ui/screen_recovery.c \
	src/ui/screen_install_run.c \
	src/ui/screen_todo.c \
	src/ui/assets/font_ui.c \
	src/ui/assets/font_title.c

# Release and debug objects live in separate trees. An object records
# nothing about the flags that built it, so a shared tree would let
# `make` after `make debug` relink debug objects into an ELF called a
# release build - one that still carries every ATLAS_LOG string.
ifeq ($(DEBUG),1)
MODE = debug
else
MODE = release
endif

OBJ_DIR = build/obj/$(MODE)

EE_OBJS = $(EE_SRC:%.c=$(OBJ_DIR)/%.o) $(IRX_OBJ)

# The ELF is one path for both modes, so switching mode has to force a
# relink: otherwise make finds the existing ELF newer than the other
# tree's objects and leaves it alone, and `make` after `make debug`
# reports a release build that still carries every log string. The
# stamp rule itself is down in Rules, below `all`, so that it cannot
# become the default goal.
MODE_STAMP = build/.mode.$(MODE)

EE_INCS    += -Iinclude -Isrc -Ibuild/irx -I$(PS2SDK)/ports/include -I$(GSKIT)/include
EE_LDFLAGS += -L$(GSKIT)/lib -L$(PS2SDK)/ports/lib
# -lelf-loader carries the loader that reads an ELF into EE RAM, resets
# the IOP and jumps. It must come before -lpatches: it calls the SBV
# patch helpers, and the linker resolves left to right.
EE_LIBS    += -lgskit -ldmakit -lelf-loader -lpatches -lfileXio -lpadx \
              -lmc -lpoweroff

EE_CFLAGS  += -Wall -Wextra -Wno-unused-parameter

# `make debug` turns on ATLAS_LOG; release builds carry no debug strings.
ifeq ($(DEBUG),1)
EE_CFLAGS  += -DATLAS_DEBUG=1
endif

.PHONY: all debug installer all-elf check clean fonts lang

# The SDK's default flags carry DWARF info, which triples the ELF for no
# benefit on a console with no debugger attached. A release build strips
# it; `make debug` keeps it alongside the ATLAS_LOG output.
all: $(EE_BIN)
ifneq ($(DEBUG),1)
	$(EE_STRIP) $(EE_BIN)
endif
	@ls -l $(EE_BIN)

debug:
	@$(MAKE) DEBUG=1

# ------------------------------------------------------------------ #
# Installer                                                           #
#                                                                     #
# A second ELF from a second Makefile, invoked with -f so it runs in  #
# this directory: the two programs share build/, build/irx and most   #
# of src/, and a sub-make run from installer/ would generate a second #
# copy of every IRX array. `make installer DEBUG=1` for the debug     #
# build, the same as the launcher.                                    #
#                                                                     #
# It is deliberately not part of `all`. The installer is what a user  #
# runs once from a USB stick; rebuilding it on every launcher build    #
# doubles compile time for a program that changes far less often.      #
# ------------------------------------------------------------------ #

installer:
	@$(MAKE) -f installer/Makefile

# What release packaging needs: both ELFs, same mode, one command.
all-elf: all
	@$(MAKE) -f installer/Makefile

# ------------------------------------------------------------------ #
# Rules                                                               #
#                                                                     #
# Objects go under build/ rather than beside their sources, so a      #
# clean is one rm and the tree stays readable. These pattern rules    #
# are more specific than the SDK's %.o: %.c and therefore win.         #
# ------------------------------------------------------------------ #

build/irx/%_irx.c: $(PS2SDK)/iop/irx/%.irx
	$(DIR_GUARD)
	$(PS2SDK)/bin/bin2c $< $@ $*_irx

$(OBJ_DIR)/%.o: %.c
	$(DIR_GUARD)
	$(EE_C_COMPILE) -c $< -o $@

# Only one stamp exists at a time, so a mode change always creates a
# new one and the ELF, which depends on it, is always relinked.
$(MODE_STAMP):
	$(DIR_GUARD)
	@rm -f build/.mode.*
	@touch $@

$(EE_BIN): $(MODE_STAMP)

# The generated IRX sources must exist before any object is compiled,
# not merely before the ones that include them: make expands the whole
# prerequisite list first.
$(EE_OBJS): | $(IRX_C)

build/irx/%_irx.o: build/irx/%_irx.c
	$(DIR_GUARD)
	$(EE_C_COMPILE) -c $< -o $@

# ------------------------------------------------------------------ #
# Font assets                                                         #
#                                                                     #
# The baked atlases are committed, so a normal build needs neither    #
# Python nor the TTFs. Regenerate only when changing size or face.    #
# ------------------------------------------------------------------ #

FONT_DIR ?= /usr/share/fonts/dejavu

fonts:
	python3 tools/genfont.py $(FONT_DIR)/DejaVuSans.ttf 16 \
		src/ui/assets/font_ui --name=ui
	python3 tools/genfont.py $(FONT_DIR)/DejaVuSans-Bold.ttf 24 \
		src/ui/assets/font_title --name=title

# ------------------------------------------------------------------ #
# Translation files                                                   #
#                                                                     #
# lang/en.ini and lang/fr.ini are complete copies of the tables built #
# into the ELF, generated from them so the two cannot drift. They     #
# ship as editable overrides, not as the source of the strings: the   #
# built-in copy is what Recovery draws with when the card that would  #
# have held these files is exactly what failed.                       #
# ------------------------------------------------------------------ #

# Built with the HOST compiler, not the EE one: the generator runs here,
# on the build machine. tests/host supplies the one PS2SDK type header
# atlas.h needs, the same way the self-checks get it.
HOSTCC ?= cc

lang:
	@mkdir -p build lang
	$(HOSTCC) -Wall -Wextra -O1 -Iinclude -Itests/host \
		-o build/genlang tools/genlang.c src/core/i18n.c
	./build/genlang lang

# ------------------------------------------------------------------ #
# Self-checks                                                         #
#                                                                     #
# Run on the build machine with the host compiler, not on the EE:     #
# they cover the pure data handling (UTF-8, parsing) that behaves the #
# same off-console. Anything touching the GS, pad or IOP is checked   #
# on hardware against docs/TESTING.md.                                #
# ------------------------------------------------------------------ #

check:
	$(MAKE) -C tests check

clean:
	$(MAKE) -C tests clean
	rm -rf build

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
