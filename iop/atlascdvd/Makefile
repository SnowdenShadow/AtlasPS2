# AtlasPS2 - atlascdvd
#
# Built by the root Makefile, not by hand. The IOP toolchain is a
# different compiler from the one that builds everything else, so this
# lives in its own file rather than as a rule in the root one.
#
# The ordinal check runs before the link, not after: an export table
# with the right functions in the wrong order links cleanly and calls
# the wrong function at runtime, so the build must not get as far as
# producing an .irx from one. See tools/checkexports.py.

IOP_BIN  = atlascdvd.irx

# frag.o and sector.o are src/disc/frag.c and src/disc/sector.c, the EE
# side's FAT reader and sector framing, compiled a second time by this
# toolchain. They are shared rather than copied because
# tests/test_frag.c and tests/test_sector.c are run against them on
# every build - see the note in atlascdvd.h about why two copies of that
# arithmetic would be worse than none.
IOP_OBJS = exports.o imports.o main.o frag.o sector.o

IOP_INCS += -I../../include

# -Wextra on top of the SDK's -Wall: this module runs in the game's
# address space and a warning here is cheaper than a black screen.
IOP_CFLAGS += -Wextra -Wno-unused-parameter

# The one warning that is not ours: the SDK's own DECLARE_IMPORT_TABLE
# stores a library name in an 8-byte field with no room for the
# terminator, which is the format loadcore reads. GCC ignores an
# unrecognised -Wno- option unless something else warns, so this stays
# harmless on a toolchain too old to know the name.
IOP_CFLAGS += -Wno-unterminated-string-initialization

all: check $(IOP_BIN)

# The shared source, compiled here. The pattern rule in the SDK's
# Makefile.iopglobal only looks in this directory, so this one rule is
# what makes the file reachable.
obj/frag.o: ../../src/disc/frag.c
	$(DIR_GUARD)
	$(IOP_C_COMPILE) -c $< -o $@

# sector.c, for the same reason. The 2340 layout puts a BCD address in
# the first three bytes and the data at offset 12; a second copy of that
# in main.c was the one place this module had arithmetic no test could
# reach, and being wrong there is a game reading twelve bytes of header
# as the start of its own structure.
obj/sector.o: ../../src/disc/sector.c
	$(DIR_GUARD)
	$(IOP_C_COMPILE) -c $< -o $@

# Checked against the SDK's own header. If that file and exports.tab
# disagree, exports.tab is wrong.
check:
	@python3 ../../tools/checkexports.py exports.tab \
		$(PS2SDK)/iop/include/cdvdman.h

clean:
	rm -rf obj $(IOP_BIN) atlascdvd.notiopmod.elf \
		atlascdvd.notiopmod.stripped.elf

.PHONY: all check clean

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
