# AtlasPS2 build environment.
#
# ps2dev/ps2dev is Alpine-based and ships the cross-compilers, PS2SDK and
# gsKit, but no make and no shell tooling. This adds exactly what the
# build needs and nothing else.
#
#   docker build -t atlasps2/build .
#   docker run --rm -v "$PWD:/src" -w /src atlasps2/build make
#
# py3-pillow and the fonts are only needed by `make fonts`, which
# regenerates the committed atlases; a normal build does not use them.
# gcc/musl-dev build the host-side self-checks in tests/, which run on
# the build machine rather than the console.
#
# py3-lz4 is the REFERENCE compressor, and it is here for exactly that
# reason: tools/genimage.py and tools/genlz4vec.py use it to produce the
# data our own decoder is checked against. A decoder validated only
# against its own encoder can be uniformly wrong and still agree with
# itself, which is not a check at all.

FROM ps2dev/ps2dev:latest

RUN apk add --no-cache \
        make \
        bash \
        gcc \
        musl-dev \
        git \
        zip \
        python3 \
        py3-pillow \
        py3-lz4 \
        font-dejavu

WORKDIR /src
