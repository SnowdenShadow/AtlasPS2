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
	src/boot/boot.c \
	src/video/video.c \
	src/input/input.c \
	src/ui/font.c \
	src/ui/assets/font_ui.c \
	src/ui/assets/font_title.c

EE_OBJS = $(EE_SRC:%.c=build/obj/%.o) $(IRX_OBJ)

EE_INCS    += -Iinclude -Isrc -Ibuild/irx -I$(PS2SDK)/ports/include -I$(GSKIT)/include
EE_LDFLAGS += -L$(GSKIT)/lib -L$(PS2SDK)/ports/lib
EE_LIBS    += -lgskit -ldmakit -lpatches -lfileXio -lpadx -lmc -lpoweroff

EE_CFLAGS  += -Wall -Wextra -Wno-unused-parameter

# `make debug` turns on ATLAS_LOG; release builds carry no debug strings.
ifeq ($(DEBUG),1)
EE_CFLAGS  += -DATLAS_DEBUG=1
endif

.PHONY: all debug clean fonts

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
# Rules                                                               #
#                                                                     #
# Objects go under build/ rather than beside their sources, so a      #
# clean is one rm and the tree stays readable. These pattern rules     #
# are more specific than the SDK's %.o: %.c and therefore win.         #
# ------------------------------------------------------------------ #

build/irx/%_irx.c: $(PS2SDK)/iop/irx/%.irx
	$(DIR_GUARD)
	$(PS2SDK)/bin/bin2c $< $@ $*_irx

build/obj/%.o: %.c
	$(DIR_GUARD)
	$(EE_C_COMPILE) -c $< -o $@

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

clean:
	rm -rf build

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
