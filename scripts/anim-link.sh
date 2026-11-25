#!/bin/bash
# Link rlottie + libvpx + port/anim.cpp into /t/build/libgramanim.so (exports only gm_*), plus build/animtest.
set -e
. /w/toolchain/qnx-env.sh
O=/b/qnx/anim
$CLANG/clang++ $QNX_CXXFLAGS -D_HAS_C9X=1 -D__unix__ -O2 -fvisibility=hidden -I$O/include -I$QT/usr/include -c /t/port/anim.cpp -o $O/anim.o
$QNX_LD $QNX_LDFLAGS_SO --gc-sections --exclude-libs ALL -soname libgramanim.so $O/anim.o --start-group $O/lib/librlottie.a $O/lib/libvpx.a --end-group \
  -lz -lm $QNX_CXX_LIBS $QNX_LIBS_SO -o $O/libgramanim.unstripped.so 2> $O/link.log || { grep -o "undefined symbol: .*" $O/link.log | sort | uniq -c | sort -rn | head -30; head -5 $O/link.log; exit 1; }
$CLANG/llvm-strip --strip-unneeded $O/libgramanim.unstripped.so -o /t/build/libgramanim.so
$QNX_CC -O2 -I/t/port -c /t/port/animtest.c -o $O/animtest.o
$QNX_LD $QNX_LDFLAGS_EXE $O/animtest.o -L/t/build -lgramanim $QNX_LIBS_EXE -o /t/build/animtest
ls -la /t/build/libgramanim.so /t/build/animtest; $CLANG/llvm-nm -D --defined-only /t/build/libgramanim.so | grep gm_; $CLANG/llvm-readelf -d /t/build/libgramanim.so | grep NEEDED
