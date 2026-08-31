# AtlasPS2 - installer Makefile
#
# Builds build/ATLAS_INSTALLER.ELF, the standalone program that puts
# AtlasPS2 onto a Memory Card. Driven from the root Makefile:
#
#     make installer
#
# Paths here are relative to the repository root, not to this file: the
# root Makefile invokes it with -f so that both ELFs share one build/
# tree and one set of generated IRX sources.

EE_BIN = build/ATLAS_INSTALLER.ELF

# ------------------------------------------------------------------ #
# IRX modules                                                         #
#                                                                     #
# Exactly the launcher's set, poweroff included. The installer never   #
# turns the console off, so that module is dead weight - but the boot  #
# sequence that loads it is src/boot/boot.c, shared verbatim with the  #
# launcher, and an installer-only list would mean an #ifdef inside the #
# one file whose job is to behave identically in both programs. A few  #
# KB of IOP memory is the cheaper price.                               #
# ------------------------------------------------------------------ #

IRX_LIST = iomanX fileXio sio2man padman mcman mcserv \
           usbd bdm bdmfs_fatfs usbmass_bd poweroff

IRX_C   = $(IRX_LIST:%=build/irx/%_irx.c)
IRX_OBJ = $(IRX_LIST:%=build/irx/%_irx.o)

# ------------------------------------------------------------------ #
# Sources                                                             #
#                                                                     #
# The core, video, input and UI modules are the launcher's own files,  #
# compiled again rather than linked from a library. A static library   #
# would be the tidier arrangement and is not worth it for two programs #
# in one tree: this way there is exactly one copy of each source and   #
# no chance of an installer built against a stale libatlas.a.          #
# ------------------------------------------------------------------ #

EE_SRC = \
	installer/src/main.c \
	installer/src/install.c \
	installer/src/ins_menu.c \
	installer/src/ins_run.c \
	installer/src/ins_bootstrap.c \
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
	src/core/power.c \
	src/boot/boot.c \
	src/video/video.c \
	src/video/video_cfg.c \
	src/input/input.c \
	src/device/device.c \
	src/ui/font.c \
	src/ui/theme.c \
	src/ui/ui.c \
	src/ui/screen.c \
	src/ui/assets/font_ui.c \
	src/ui/assets/font_title.c

# Its own object tree, under the same mode split as the launcher's. The
# two programs compile the same sources with the same flags, but an
# object built for one must not be silently reused by the other: they
# have different entry points and different link sets, and a shared
# tree would make "which ELF is stale" unanswerable.
ifeq ($(DEBUG),1)
MODE = debug
else
MODE = release
endif

OBJ_DIR = build/obj/installer-$(MODE)

EE_OBJS = $(EE_SRC:%.c=$(OBJ_DIR)/%.o) $(IRX_OBJ)

MODE_STAMP = build/.imode.$(MODE)

# installer/include comes first so install.h and ins_screen.h are found
# by their bare names; -Isrc is what lets the shared UI sources reach
# their asset headers.
EE_INCS    += -Iinstaller/include -Iinclude -Isrc -Ibuild/irx \
              -I$(PS2SDK)/ports/include -I$(GSKIT)/include
EE_LDFLAGS += -L$(GSKIT)/lib -L$(PS2SDK)/ports/lib

# No -lelf-loader here: the installer launches nothing. -lpatches stays,
# because the shared boot sequence applies the SBV patches before loading
# any module, and -lpoweroff because it embeds the same module list.
EE_LIBS    += -lgskit -ldmakit -lpatches -lfileXio -lpadx -lmc -lpoweroff

EE_CFLAGS  += -Wall -Wextra -Wno-unused-parameter

ifeq ($(DEBUG),1)
EE_CFLAGS  += -DATLAS_DEBUG=1
endif

.PHONY: all clean

all: $(EE_BIN)
ifneq ($(DEBUG),1)
	$(EE_STRIP) $(EE_BIN)
endif
	@ls -l $(EE_BIN)

$(OBJ_DIR)/%.o: %.c
	$(DIR_GUARD)
	$(EE_C_COMPILE) -c $< -o $@

$(MODE_STAMP):
	$(DIR_GUARD)
	@rm -f build/.imode.*
	@touch $@

$(EE_BIN): $(MODE_STAMP)

$(EE_OBJS): | $(IRX_C)

build/irx/%_irx.c: $(PS2SDK)/iop/irx/%.irx
	$(DIR_GUARD)
	$(PS2SDK)/bin/bin2c $< $@ $*_irx

build/irx/%_irx.o: build/irx/%_irx.c
	$(DIR_GUARD)
	$(EE_C_COMPILE) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(EE_BIN)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
