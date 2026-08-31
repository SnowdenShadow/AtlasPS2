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

FROM ps2dev/ps2dev:latest

RUN apk add --no-cache \
        make \
        bash \
        git \
        zip \
        python3 \
        py3-pillow \
        font-dejavu

WORKDIR /src
