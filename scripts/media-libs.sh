#!/bin/bash
# Cross-build static libopus 1.4 (float) and the WebP decoder (libwebp 1.3.2) for BB10 -> /b/qnx/media/{lib,include}
# Sources: /t/third_party/dl/*.tar.gz (official releases, versions and sha256 in README.md). No configure: plain file lists.
set -e
. /w/toolchain/qnx-env.sh
W=/b/work/media; O=/b/qnx/media; rm -rf $W; mkdir -p $W $O/lib $O/include
cd $W; tar xzf /t/third_party/dl/opus-1.4.tar.gz; tar xzf /t/third_party/dl/libwebp-1.3.2.tar.gz
CF="-O2 -ffunction-sections -fdata-sections -fvisibility=hidden -w"
# --- opus
cd $W/opus-1.4
SRC=$(python3 - <<'PY'
import re
def lst(f, var):
    s = open(f).read().replace("\\\n", " ")
    m = re.search(r"^" + var + r"\s*=(.*)$", s, re.M)
    return m.group(1).split() if m else []
print(" ".join(lst("celt_sources.mk", "CELT_SOURCES") + lst("silk_sources.mk", "SILK_SOURCES") + lst("silk_sources.mk", "SILK_SOURCES_FLOAT")
               + lst("opus_sources.mk", "OPUS_SOURCES") + lst("opus_sources.mk", "OPUS_SOURCES_FLOAT")))
PY
)
mkdir -p obj; n=0
for f in $SRC; do o=obj/$(echo $f | tr / _).o; $QNX_CC $CF -DOPUS_BUILD -DUSE_ALLOCA -DHAVE_LRINT -DHAVE_LRINTF -Iinclude -Icelt -Isilk -Isilk/float -c $f -o $o & n=$((n+1)); [ $((n % 12)) = 0 ] && wait; done; wait
$CLANG/llvm-ar rcs $O/lib/libopus.a obj/*.o
cp include/opus*.h $O/include/
echo "opus: $(ls obj | wc -l) objects"
# --- webp (decoder side)
cd $W/libwebp-1.3.2
mkdir -p obj; n=0
for f in src/dec/*.c src/dsp/*.c src/utils/*.c; do o=obj/$(echo $f | tr / _).o; $QNX_CC $CF -I. -Isrc -c $f -o $o & n=$((n+1)); [ $((n % 12)) = 0 ] && wait; done; wait
$CLANG/llvm-ar rcs $O/lib/libwebpdec.a obj/*.o
mkdir -p $O/include/webp; cp src/webp/decode.h src/webp/types.h $O/include/webp/
echo "webp: $(ls obj | wc -l) objects"
ls -la $O/lib
