#!/bin/bash
# OpenSSL 1.1.1w, static, for BB10 (the phone's own is 1.0.1i: too old for the Telegram library and current TLS).
# -> /b/qnx/openssl/{include,lib}
set -e
. /w/toolchain/qnx-env.sh
rm -rf /b/work/openssl; mkdir -p /b/work/openssl /b/qnx/openssl; cd /b/work/openssl
tar xzf /b/dl/openssl-1.1.1w.tar.gz --strip-components=1
# BB10: no probing with deliberate illegal instructions at load time (it crashed inside sigsetjmp on the phone).
# The Passport's CPU (Snapdragon 801, ARMv7-A) has NEON and no ARMv8 crypto extensions.
python3 - <<'PEOF'
p = "crypto/armcap.c"; s = open(p).read()
old = "    if ((e = getenv(\"OPENSSL_armcap\"))) {"
assert old in s
s = s.replace(old, "    OPENSSL_armcap_P = ARMV7_NEON;\n    return;\n    if ((e = getenv(\"OPENSSL_armcap\"))) {", 1)
open(p, "w").write(s)
PEOF
export CC="$QNX_CC" AR=$CLANG/llvm-ar RANLIB=$CLANG/llvm-ranlib
./Configure ${OSSL_TARGET:-linux-armv4} no-shared no-tests no-dso no-engine no-async no-ui-console no-comp no-ssl3 no-weak-ssl-ciphers \
  no-afalgeng no-capieng no-srp no-psk no-idea no-rc2 no-rc5 no-md2 no-mdc2 no-whirlpool no-seed no-camellia no-aria no-sm2 no-sm3 no-sm4 no-gost \
  -O2 -DOPENSSL_NO_SECURE_MEMORY -DNO_SYSLOG -D_XOPEN_SOURCE=600 --prefix=/b/qnx/openssl --openssldir=/etc/ssl > configure.log 2>&1 || { tail -20 configure.log; exit 1; }
make -j12 build_libs > make.log 2>&1 || { grep -E "error|Error" make.log | head -30; exit 1; }
make install_dev > /dev/null 2>&1
ls -la /b/qnx/openssl/lib/*.a; grep OPENSSL_VERSION_TEXT /b/qnx/openssl/include/openssl/opensslv.h | head -1
