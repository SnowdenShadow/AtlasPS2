# AtlasPS2 - host-side self-checks
#
#     make -C tests
#
# These run on the build machine with the system compiler, not the EE
# toolchain: they cover the parts that are pure string and data handling
# and so behave identically off-console. Anything touching the GS, the
# pad or the IOP cannot be tested this way and is checked on hardware.

CC     ?= cc
CFLAGS += -Wall -Wextra -O1 -I../include

TESTS = test_utf8

.PHONY: check clean

check: $(TESTS)
	@for t in $(TESTS); do ./$$t || exit 1; done

test_utf8: test_utf8.c ../src/core/utf8.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TESTS)
