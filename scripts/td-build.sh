#!/bin/bash
# Build the Telegram library (TDLib) for BB10.  Stages: src (copy + patch), gen (generated sources, host build),
# cross (static libraries for the phone).  td-build.sh [src] [gen] [cross]   (default: all)
set -e
. /w/toolchain/qnx-env.sh
CM=/b/host/cmake/bin/cmake
STAGES="${*:-src gen cross}"
for st in $STAGES; do case $st in
src)
  mkdir -p /b/src; rm -rf /b/src/td; cp -r /t/third_party/td /b/src/td; rm -rf /b/src/td/.git
  python3 /t/scripts/td-patch.py /b/src/td; echo "src: copied and patched" ;;
gen)
  mkdir -p /b/work/td-host; cd /b/work/td-host
  $CM -DCMAKE_BUILD_TYPE=Release -DTD_GENERATE_SOURCE_FILES=ON /b/src/td > cmake.log 2>&1 || { tail -30 cmake.log; exit 1; }
  $CM --build . -j12 > build.log 2>&1 || { grep -E "error|Error" -A3 build.log | head -40; exit 1; }
  echo "gen: done"; ls /b/src/td/td/generate/auto/td/telegram | head -8 ;;
cross)
  export CLANG QT
  export TG_CFLAGS="$QNX_TARGET_FLAGS -D__unix__ -D__unix -isystem /t/port/include $QNX_SYSINC -O2 -ffunction-sections -fdata-sections -fvisibility=hidden"
  export TG_CXXFLAGS="$QNX_CXXFLAGS -D__unix__ -D__unix -isystem /t/port/include -O2 -ffunction-sections -fdata-sections -fvisibility=hidden -fvisibility-inlines-hidden -Wno-everything"
  python3 /t/scripts/td-patch.py /b/src/td
  [ "$CLEAN" = 1 ] && rm -rf /b/work/td-qnx
  mkdir -p /b/work/td-qnx; cd /b/work/td-qnx
  $CM -DCMAKE_TOOLCHAIN_FILE=/t/scripts/qnx-toolchain.cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="$TG_CFLAGS" -DCMAKE_CXX_FLAGS="$TG_CXXFLAGS" \
    -DOPENSSL_ROOT_DIR=/b/qnx/openssl -DOPENSSL_USE_STATIC_LIBS=TRUE \
    -DOPENSSL_INCLUDE_DIR=/b/qnx/openssl/include -DOPENSSL_CRYPTO_LIBRARY=/b/qnx/openssl/lib/libcrypto.a -DOPENSSL_SSL_LIBRARY=/b/qnx/openssl/lib/libssl.a \
    -DZLIB_INCLUDE_DIR=$QT/usr/include -DZLIB_LIBRARY=$QT/armle-v7/usr/lib/libz.so \
    -DTD_INSTALL_SHARED_LIBRARIES=OFF /b/src/td > cmake.log 2>&1 || { tail -40 cmake.log; exit 1; }
  grep -E "Warning|WARNING|Could NOT|not found" cmake.log | head -10 || true
  $CM --build . --target tdjson_static -j${J:-12} -- -k > build.log 2>&1 || { grep -E "error:" -B2 -A6 build.log | head -${ERRLINES:-90}; echo "..."; grep -c "error:" build.log; exit 1; }
  echo "cross: done"; find . -name "*.a" | xargs ls -la ;;
esac; done
