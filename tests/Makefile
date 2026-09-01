# AtlasPS2 - host-side self-checks
#
#     make -C tests
#
# These run on the build machine with the system compiler, not the EE
# toolchain: they cover the parts that are pure string and data handling
# and so behave identically off-console. Anything touching the GS, the
# pad or the IOP cannot be tested this way and is checked on hardware.

# host/ holds a stand-in for the one PS2SDK header atlas.h pulls in, so
# these build without the EE toolchain. It is searched after ../include,
# and only ever supplies types the SDK would otherwise define.
CC     ?= cc
CFLAGS += -Wall -Wextra -O1 -I../include -Ihost

# The headers are prerequisites too: i18n.h carries the string table
# itself, so a binary that ignored it would keep reporting the count it
# was built with and quietly pass a check on strings it never saw.
HDRS = $(wildcard ../include/atlas/*.h host/*.h)

TESTS = test_utf8 test_path test_ini test_i18n test_config test_hash \
        test_theme test_fav test_fs_path test_disc test_lz4 test_image \
        test_compat test_frag test_sector

.PHONY: check clean

check: $(TESTS)
	@for t in $(TESTS); do ./$$t || exit 1; done

test_utf8: test_utf8.c ../src/core/utf8.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^)

test_path: test_path.c ../src/core/path.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^)

test_ini: test_ini.c ../src/core/ini.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^)

test_i18n: test_i18n.c ../src/core/i18n.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^)

# config.c pulls in video.h for the [video] section. video_cfg.c holds
# the parts of that module with no gsKit dependency, which is exactly
# why it is a separate file.
test_config: test_config.c ../src/core/config.c ../src/core/ini.c ../src/core/i18n.c ../src/video/video_cfg.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^)

# hash.c is pure arithmetic, so the host answer and the console answer
# are the same value - which is the point: the installer compares a
# checksum computed on the PS2 against one this check pinned here.
test_hash: test_hash.c ../src/core/hash.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^)

# theme.c holds the palette and the colour parser and touches no
# device; theme_io.c is the half that reads Memory Cards, and is not
# here. That split exists for this check: a theme file's colours are
# exactly what is wrong when a theme looks wrong, and a check that
# needs a television is not a check.
test_theme: test_theme.c ../src/ui/theme.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^)

# fav.c is the lists themselves; fav_io.c reads and writes the card and
# is not here. Order is the whole behaviour of a recently-used list, and
# order is exactly what a check can pin without hardware.
test_fav: test_fav.c ../src/core/fav.c ../src/core/ini.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^)

# fs_path.c is why the split in fs.c exists. atlas_fs_is_protected()
# decides whether a user is warned before deleting the file their
# console boots from, and the only other way to find out it was wrong
# is to wreck a Memory Card. fs.c itself calls fileXio and is not here.
test_fs_path: test_fs_path.c ../src/core/fs_path.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^)

# disc.c takes its sectors through a callback and never opens a file,
# which is what makes this check possible at all: the images below are
# built byte by byte in memory. The game ID it extracts is the key every
# compatibility decision is made on, and a wrong one does not fail
# loudly - it matches another game's entry. ini.c comes along because
# SYSTEM.CNF is INI grammar and is parsed with the same reader.
test_disc: test_disc.c ../src/disc/disc.c ../src/core/ini.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^)

# lz4_vectors.h is committed and comes from the reference compressor
# (tools/genlz4vec.py). A decoder checked only against its own encoder
# can be uniformly wrong and still agree with itself, and this one runs
# over every sector a compressed image is read through.
test_lz4: test_lz4.c ../src/disc/lz4.c lz4_vectors.h $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^)

# image.c calls fileXio, so host/ carries a stdio-backed stand-in for
# it rather than the module taking a seam it would not have on the
# console. What is being checked is the ZSO index arithmetic - which
# block holds sector N - and a seam would let the check run against
# different arithmetic from the one that ships.
test_image: test_image.c ../src/disc/image.c ../src/disc/disc.c \
            ../src/disc/lz4.c ../src/core/ini.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^)

# compat.c is the settings and what they mean; compat_io.c is the read
# off the card and is not here. Everything a wrong entry costs happens
# in the parsing: a workaround that silently failed to parse looks
# exactly like a game that needed a different one, and neither says so
# until a television is in front of you. disc.c comes along for
# atlas_disc_id_normalize(), which every section header goes through.
test_compat: test_compat.c ../src/disc/compat.c ../src/disc/disc.c ../src/core/ini.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^)

# frag.c reads its sectors through a callback, so the volumes it is
# checked against are built in memory here rather than needing a card.
test_frag: test_frag.c ../src/disc/frag.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^)

test_sector: test_sector.c ../src/disc/sector.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^)

# ------------------------------------------------------------------ #
# Disc image fixtures                                                 #
#                                                                     #
# test_image compares the same disc as an ISO and as a ZSO, sector by #
# sector. That pairing is the whole value of the check: a wrong index #
# entry returns real data, correctly decompressed, from the wrong     #
# place - which passes against any fixed expected value and cannot    #
# pass against the same disc in another format.                       #
#                                                                     #
# The images are generated rather than committed (200 KB of data with #
# no history worth keeping), and building the ZSO needs Python's lz4  #
# module. `check` therefore does not depend on this: without the      #
# images test_image reports SKIPPED, so a contributor who lacks the   #
# module still gets a useful run out of every other check.            #
# ------------------------------------------------------------------ #

images:
	python3 ../tools/genimage.py ../build/testimg

clean:
	rm -f $(TESTS)
