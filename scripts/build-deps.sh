#!/bin/sh
# Builds the third party libraries the plug-in is linked against, in the layout
# the makefiles expect:
#
#   oflto152/   libopus objects, compiled into the DLL
#   logg/       libogg sources, compiled into the DLL
#   opus/       libopus and libopusfile headers   (#include <opus/opusfile.h>)
#   ogg/        libogg headers                    (#include <ogg/ogg.h>)
#
# None of these directories is committed: .gitignore covers all of them.
#
# usage: scripts/build-deps.sh
#   environment: OPUS_VERSION, OGG_VERSION, CC, WORK, JOBS
set -eu

OPUS_VERSION=${OPUS_VERSION:-1.5.2}
OGG_VERSION=${OGG_VERSION:-1.3.2}
CC=${CC:-gcc}
WORK=${WORK:-.deps}

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"

for tool in "$CC" curl tar; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "build-deps: missing $tool" >&2
        exit 1
    }
done

mkdir -p "$WORK" opus ogg logg oflto152

# --- sources ----------------------------------------------------------------
if [ ! -d "$WORK/opus-$OPUS_VERSION" ]; then
    echo "build-deps: downloading libopus $OPUS_VERSION"
    curl -fsSL -o "$WORK/opus.tar.gz" \
        "https://downloads.xiph.org/releases/opus/opus-$OPUS_VERSION.tar.gz"
    tar -xzf "$WORK/opus.tar.gz" -C "$WORK"
fi
if [ ! -d "$WORK/libogg-$OGG_VERSION" ]; then
    echo "build-deps: downloading libogg $OGG_VERSION"
    curl -fsSL -o "$WORK/ogg.tar.gz" \
        "https://downloads.xiph.org/releases/ogg/libogg-$OGG_VERSION.tar.gz"
    tar -xzf "$WORK/ogg.tar.gz" -C "$WORK"
fi

# --- headers and the libogg sources that the makefiles compile themselves ----
cp "$WORK/opus-$OPUS_VERSION"/include/*.h opus/
cp opusfile/opusfile.h opus/
cp "$WORK/libogg-$OGG_VERSION"/include/ogg/*.h ogg/
cp "$WORK/libogg-$OGG_VERSION"/src/*.c logg/

# --- libopus objects --------------------------------------------------------
OPUS_DIR="$WORK/opus-$OPUS_VERSION"
OPUS_CFLAGS="-c -Os -flto -w -fno-stack-check -fno-stack-protector -mno-stack-arg-probe
 -DOPUS_BUILD -DFLOATING_POINT -DUSE_ALLOCA
 -I$OPUS_DIR/include -I$OPUS_DIR/celt -I$OPUS_DIR/silk -I$OPUS_DIR/silk/float"

count=0
for src in "$OPUS_DIR"/src/*.c "$OPUS_DIR"/celt/*.c "$OPUS_DIR"/silk/*.c "$OPUS_DIR"/silk/float/*.c; do
    case "$src" in
        *demo*|*compare*) continue ;;
    esac
    $CC $OPUS_CFLAGS "$src" -o "oflto152/$(basename "$src" .c).o"
    count=$((count + 1))
done

echo "build-deps: $count libopus objects in oflto152/, libogg sources in logg/"
echo "build-deps: headers in opus/ and ogg/ (libopus $OPUS_VERSION, libogg $OGG_VERSION)"