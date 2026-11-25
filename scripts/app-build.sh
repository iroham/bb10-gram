#!/bin/bash
# Build the app binary: moc, compile the C++ side against the BB10 SDK (libstdc++ 4.8.3, Qt 4.8, Cascades), link
# with libtdjson.so.  -> /t/build/gram
set -e
. /w/toolchain/qnx-env.sh
cd /t; mkdir -p build/obj
GXX=$QT/usr/include/c++/4.8.3
INC="-nostdinc++ -isystem $GXX -isystem $GXX/arm-unknown-nto-qnx8.0.0eabi -isystem $GXX/backward $QNX_SYSINC -isystem $QT/usr/include/qt4 -isystem $QT/usr/include/qt4/QtCore -isystem $QT/usr/include/qt4/QtDeclarative -isystem $QT/usr/include/qt4/QtGui -isystem $QT/usr/include/qt4/QtNetwork -isystem $QT/usr/include/qt4/QtSensors -isystem $QT/usr/include/qt4/QtMobility -Isrc -Ibuild/obj"
FL="-std=gnu++98 -O2 -Wall -Wno-pragma-pack -fvisibility=hidden -DQT_NO_DEBUG -DQT_DECLARATIVE_LIB -DQT_CORE_LIB"
OBJS=""
for h in src/*.h; do
  if grep -q Q_OBJECT $h; then n=$(basename ${h%.h}); /b/host/moc -DQ_OS_BLACKBERRY $h -o build/obj/moc_$n.cpp; $QNX_CXX $FL $INC -c build/obj/moc_$n.cpp -o build/obj/moc_$n.o & OBJS="$OBJS build/obj/moc_$n.o"; fi
done
for c in src/main.cpp service/gramd.cpp; do n=$(basename ${c%.cpp}); /b/host/moc -DQ_OS_BLACKBERRY $c -o build/obj/$n.moc; done
for c in src/*.c; do n=$(basename ${c%.c}); $QNX_CC -O2 -Wall -I/b/qnx/media/include -c $c -o build/obj/c_$n.o & OBJS="$OBJS build/obj/c_$n.o"; done
for c in src/*.cpp; do n=$(basename ${c%.cpp}); $QNX_CXX $FL $INC -c $c -o build/obj/$n.o & OBJS="$OBJS build/obj/$n.o"; done
wait
for o in $OBJS; do [ -f $o ] || { echo "compile failed: $o"; exit 1; }; done
$QNX_LD $QNX_LDFLAGS_EXE -L$QT/armle-v7/usr/lib/qt4/lib -L$QT/armle-v7/lib/gcc/4.8.3 -rpath-link=$QT/armle-v7/usr/lib/qt4/lib --disable-new-dtags -rpath app/native/lib:/usr/lib/qt4/lib $OBJS \
  -Lbuild -ltdjson -lgramanim -lgramvoip /b/qnx/media/lib/libopus.a /b/qnx/media/lib/libwebpdec.a -lz -lbb -lbbcascades -lbbdata -lbbsystem -lbbplatform -lbbmultimedia -lbbcascadespickers -lbbdevice -lcamapi -lQtSensors -lQtDeclarative -lQtNetwork -lQtCore -lstdc++ -lm $QNX_LIBS_EXE -o build/gram
$CLANG/llvm-strip --strip-unneeded build/gram
# the headless service
$QNX_CXX $FL $INC -c service/gramd.cpp -o build/obj/gramd.o
$QNX_LD $QNX_LDFLAGS_EXE -L$QT/armle-v7/usr/lib/qt4/lib -L$QT/armle-v7/lib/gcc/4.8.3 -rpath-link=$QT/armle-v7/usr/lib/qt4/lib --disable-new-dtags -rpath app/native/lib:/usr/lib/qt4/lib build/obj/gramd.o \
  -Lbuild -ltdjson -lbb -lbbdata -lbbsystem -lbbplatform -lQtNetwork -lQtCore -lstdc++ -lm $QNX_LIBS_EXE -o build/gramd
$CLANG/llvm-strip --strip-unneeded build/gramd
ls -la build/gramd
ls -la build/gram; $CLANG/llvm-readelf -d build/gram | grep -E "NEEDED|RPATH|RUNPATH"
