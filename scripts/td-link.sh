#!/bin/bash
# Link the cross-built TDLib static libraries into one shared library for the app: /b/qnx/libtdjson.so
# (exports only tg_* from port/tdwrap.cpp; libc++, OpenSSL and sqlite are inside and hidden).
set -e
. /w/toolchain/qnx-env.sh
B=/b/work/td-qnx; O=/b/qnx; mkdir -p $O
$CLANG/clang++ $QNX_CXXFLAGS -D__unix__ -isystem /t/port/include -O2 -fvisibility=hidden -I/b/src/td -I$B -I$B/td/telegram -c /t/port/tdwrap.cpp -o $O/tdwrap.o
LIBS=$(find $B -name "*.a" | sort)
echo "$LIBS" | sed 's|.*/||' | tr '\n' ' '; echo
$QNX_LD $QNX_LDFLAGS_SO --gc-sections --exclude-libs ALL -soname libtdjson.so $O/tdwrap.o --start-group $LIBS /b/qnx/openssl/lib/libssl.a /b/qnx/openssl/lib/libcrypto.a --end-group \
  -lz -lsocket $QNX_CXX_LIBS $QNX_LIBS_SO -o $O/libtdjson.unstripped.so 2> $O/link.log || { grep -o "undefined symbol: .*" $O/link.log | sort | uniq -c | sort -rn | head -40; grep -v "undefined symbol\|referenced by\|^$" $O/link.log | head -10; exit 1; }
$CLANG/llvm-strip --strip-unneeded $O/libtdjson.unstripped.so -o $O/libtdjson.so
ls -la $O/libtdjson.so; $CLANG/llvm-readelf -d $O/libtdjson.so | grep NEEDED; $CLANG/llvm-nm -D --defined-only $O/libtdjson.so | grep -c . ; $CLANG/llvm-nm -D --defined-only $O/libtdjson.so | head -8
# small test program for the phone
$QNX_CC -O2 -c /t/port/tdtest.c -o $O/tdtest.o
$QNX_LD $QNX_LDFLAGS_EXE $O/tdtest.o -L$O -ltdjson $QNX_LIBS_EXE -o $O/tdtest
mkdir -p /t/build; cp $O/libtdjson.so $O/tdtest /t/build/; ls -la /t/build
