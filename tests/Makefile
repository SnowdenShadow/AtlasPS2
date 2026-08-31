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

TESTS = test_utf8 test_path test_ini test_i18n test_config test_hash

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

clean:
	rm -f $(TESTS)
