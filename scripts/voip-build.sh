#!/bin/bash
# Cross-build libtgvoip 2.5 (Telegram's voice-call engine, grishka/libtgvoip master, with its bundled WebRTC echo
# canceller / noise suppression, NEON) as a static library: /b/qnx/voip/libtgvoip.a.  Audio goes through callbacks
# (TGVOIP_USE_CALLBACK_AUDIO_IO); the phone's audio is done in port/voip.cpp.  Files: scripts/voip-files.txt.
set -e
. /w/toolchain/qnx-env.sh
W=/b/work/voip; O=/b/qnx/voip; mkdir -p $O
if [ "$KEEP" != 1 ]; then rm -rf $W; mkdir -p $W; cd $W; tar xzf /t/third_party/dl/libtgvoip.tar.gz; python3 /t/scripts/voip-patch.py $W/libtgvoip-public; fi
cd $W/libtgvoip-public; mkdir -p obj
DEFS="-mfpu=neon -DWEBRTC_HAS_NEON -DTGVOIP_USE_CALLBACK_AUDIO_IO -DWEBRTC_APM_DEBUG_DUMP=0 -DWEBRTC_NS_FLOAT -DWEBRTC_POSIX \
 -D__unix__ -D_HAS_C9X=1 -D__STDC_LIMIT_MACROS -isystem /t/port/include -I. -Iwebrtc_dsp -I/b/qnx/media/include -I/b/qnx/openssl/include"
export CXXF="$QNX_CXXFLAGS $DEFS -O2 -ffunction-sections -fdata-sections -fvisibility=hidden -fvisibility-inlines-hidden -Wno-everything"
export CF="$DEFS -O2 -ffunction-sections -fdata-sections -fvisibility=hidden -w"
export CLANG QNX_CC
comp() { f=$1; o=obj/$(echo $f | tr / _).o; [ -f $o ] && return 0
  case $f in *.c) $QNX_CC $CF -c $f -o $o ;; *) $CLANG/clang++ $CXXF -c $f -o $o ;; esac > obj/$(echo $f | tr / _).log 2>&1 || { echo "FAIL $f"; return 0; } }
export -f comp
xargs -P ${J:-12} -I{} bash -c 'comp {}' < /t/scripts/voip-files.txt | sort > fails.txt || true
if [ -s fails.txt ]; then cat fails.txt | head -40; echo "$(wc -l < fails.txt) failed"; for f in $(cut -d' ' -f2 fails.txt | head -${SHOW:-3}); do echo "== $f"; grep -E "error" -A3 obj/$(echo $f | tr / _).log | head -20; done; exit 1; fi
rm -f $O/libtgvoip.a; $CLANG/llvm-ar rcs $O/libtgvoip.a obj/*.o; ls -la $O/libtgvoip.a
mkdir -p $O/include; cp -r *.h *.hpp audio $O/include/ 2>/dev/null || true
