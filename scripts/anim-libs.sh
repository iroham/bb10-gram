#!/bin/bash
# Cross-build the decoders for animated stickers -> /b/qnx/anim/{lib,include}
#   rlottie 0.2 (Lottie / .tgs renderer, C++ with the project's libc++), libvpx 1.13.1 (VP9 decoder for .webm).
# Sources: /t/third_party/dl/*.tar.gz (versions and sha256 in README.md).  anim-libs.sh [rlottie] [vpx]
set -e
. /w/toolchain/qnx-env.sh
CM=/b/host/cmake/bin/cmake
W=/b/work/anim; O=/b/qnx/anim; mkdir -p $W $O/lib $O/include
for st in ${*:-rlottie vpx}; do case $st in
rlottie)
  cd $W; rm -rf rlottie-0.2 rl-build; tar xzf /t/third_party/dl/rlottie-0.2.tar.gz; mkdir rl-build; cd rl-build
  export CLANG QT
  export TG_CFLAGS="$QNX_TARGET_FLAGS -D__unix__ -D__unix -isystem /t/port/include $QNX_SYSINC -O2 -ffunction-sections -fdata-sections -fvisibility=hidden"
  export TG_CXXFLAGS="$QNX_CXXFLAGS -D_HAS_C9X=1 -D__unix__ -D__unix -isystem /t/port/include -O2 -ffunction-sections -fdata-sections -fvisibility=hidden -fvisibility-inlines-hidden -Wno-everything"
  $CM -DCMAKE_TOOLCHAIN_FILE=/t/scripts/qnx-toolchain.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
      -DLOTTIE_MODULE=OFF -DLOTTIE_THREAD=OFF -DLOTTIE_CACHE=OFF -DLOTTIE_TEST=OFF ../rlottie-0.2 > cmake.log 2>&1 || { tail -30 cmake.log; exit 1; }
  $CM --build . --target rlottie -j12 > build.log 2>&1 || { grep -E "error" -A4 build.log | head -40; exit 1; }
  cp $(find . -name "librlottie*.a") $O/lib/librlottie.a; cp ../rlottie-0.2/inc/*.h $O/include/
  ls -la $O/lib ;;
vpx)
  cd $W; rm -rf libvpx-1.13.1; tar xzf /t/third_party/dl/libvpx-1.13.1.tar.gz; cd libvpx-1.13.1
  # configure links small test programs: give it a linker command that can make QNX executables
  cat > $W/qnxcc <<EOS
#!/bin/bash
case " \$* " in *" -c "*|*" -E "*|*" -S "*) exec $QNX_CC "\$@";; esac
objs=(); out=a.out; while [ \$# -gt 0 ]; do case "\$1" in -o) out=\$2; shift;; *.o|*.a) objs+=("\$1");; -l*) objs+=("\$1");; *.c) $QNX_CC -c "\$1" -o "\$1.o"; objs+=("\$1.o");; esac; shift; done
exec $QNX_LD $QNX_LDFLAGS_EXE "\${objs[@]}" -lm $QNX_LIBS_EXE -o "\$out"
EOS
  chmod +x $W/qnxcc
  printf '#!/bin/bash\nexec %s -c "$@"\n' "$QNX_CC" > $W/qnxas; chmod +x $W/qnxas
  T=${VPX_TARGET:-armv7-linux-gcc}
  CC=$W/qnxcc CXX="$QNX_CXX" AR=$CLANG/llvm-ar NM=$CLANG/llvm-nm STRIP=$CLANG/llvm-strip AS=$W/qnxas LD=$W/qnxcc CROSS= \
  ./configure --target=$T --disable-vp8 --disable-vp9-encoder --enable-vp9-decoder --disable-examples --disable-tools --disable-docs \
     --disable-unit-tests --disable-webm-io --disable-libyuv --enable-static --disable-shared --enable-pic --disable-runtime-cpu-detect \
     --extra-cflags="-O2 -ffunction-sections -fdata-sections -fvisibility=hidden" > configure.log 2>&1 || { tail -30 configure.log; exit 1; }
  make -j12 libvpx.a > make.log 2>&1 || { grep -E "error" -A4 make.log | head -40; exit 1; }
  cp libvpx.a $O/lib/; mkdir -p $O/include/vpx; cp vpx/vpx_decoder.h vpx/vpx_codec.h vpx/vpx_image.h vpx/vp8dx.h vpx/vp8.h vpx/vpx_integer.h vpx/vpx_frame_buffer.h vpx/vpx_ext_ratectrl.h $O/include/vpx/ 2>/dev/null || true
  ls -la $O/lib ;;
esac; done
