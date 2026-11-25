#!/bin/bash
# Link libtgvoip + port/voip.cpp (+ Opus, OpenSSL crypto, libc++) into /t/build/libgramvoip.so (exports only gv_*),
# plus build/voiptest.
set -e
. /w/toolchain/qnx-env.sh
O=/b/qnx/voip; S=/b/work/voip/libtgvoip-public
$CLANG/clang++ $QNX_CXXFLAGS -mfpu=neon -DTGVOIP_USE_CALLBACK_AUDIO_IO -DWEBRTC_POSIX -D_HAS_C9X=1 -D__unix__ -isystem /t/port/include -O2 -fvisibility=hidden \
  -I$S -I$S/webrtc_dsp -c /t/port/voip.cpp -o $O/voip.o
$QNX_LD $QNX_LDFLAGS_SO --gc-sections -z defs --exclude-libs ALL -soname libgramvoip.so $O/voip.o --start-group $O/libtgvoip.a /b/qnx/media/lib/libopus.a \
  /b/qnx/openssl/lib/libcrypto.a --end-group -lasound -laudio_manager -lsocket -lm $QNX_CXX_LIBS $QNX_LIBS_SO -o $O/libgramvoip.unstripped.so 2> $O/link.log \
  || { grep -o "undefined symbol: .*" $O/link.log | sort | uniq -c | sort -rn | head -30; grep -v "undefined symbol\|referenced by\|^$\|>>>" $O/link.log | head -8; exit 1; }
$CLANG/llvm-strip --strip-unneeded $O/libgramvoip.unstripped.so -o /t/build/libgramvoip.so
$QNX_CC -O2 -I/t/port -c /t/port/voiptest.c -o $O/voiptest.o
$QNX_LD $QNX_LDFLAGS_EXE $O/voiptest.o -L/t/build -lgramvoip $QNX_LIBS_EXE -o /t/build/voiptest
ls -la /t/build/libgramvoip.so; $CLANG/llvm-nm -D --defined-only /t/build/libgramvoip.so | grep -c " gv_"; $CLANG/llvm-readelf -d /t/build/libgramvoip.so | grep NEEDED
