# Small source fixes so libtgvoip builds for BB10 (QNX). Idempotent.  voip-patch.py <source dir>
import sys, os
D = sys.argv[1]
def patch(path, old, new, must=True):
    p = os.path.join(D, path); s = open(p).read()
    if new in s: return
    if old not in s:
        if must: raise SystemExit("patch: not found in %s: %r" % (path, old[:60]))
        return
    open(p, 'w').write(s.replace(old, new))
# QNX: alloca is a macro, so ::alloca does not parse
patch('webrtc_dsp/rtc_base/stringutils.h', 'static_cast<TYPE*>(::alloca((LEN) * sizeof(TYPE)))', 'static_cast<TYPE*>(__builtin_alloca((LEN) * sizeof(TYPE)))')
