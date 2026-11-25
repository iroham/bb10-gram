#!/usr/bin/env python3
"""Pack the native app into an unsigned BB10 archive (.bar) for the root installer.
Usage (from bb10-tools/): python3 native-tg/package.py  -> native-tg/build/<PKG>.bar
Layout: META-INF/MANIFEST.MF, native/gram (the app), native/lib/libtdjson.so (Telegram library), native/assets/** (QML, icons),
native/icon.png."""
import base64, hashlib, os, struct, sys, zipfile, zlib
HERE = os.path.dirname(os.path.abspath(__file__))
PKG, NAME, VER = "org.bq.gram", "Gram", "0.9.0.6"

def b64(b): return base64.urlsafe_b64encode(b).decode().rstrip("=")
def ident(letter, text): return "bqnt" + letter + b64(hashlib.sha256(text.encode()).digest())[:22]

def icon():
    """The app icon (assets/appicon.png)."""
    return open(os.path.join(HERE, "assets", "appicon.png"), "rb").read()

assets = {"native/gram": open(os.path.join(HERE, "build/gram"), "rb").read(), "native/icon.png": icon(),
          "native/gramd": open(os.path.join(HERE, "build/gramd"), "rb").read(),
          "native/lib/libtdjson.so": open(os.path.join(HERE, "build/libtdjson.so"), "rb").read(),
          "native/lib/libgramanim.so": open(os.path.join(HERE, "build/libgramanim.so"), "rb").read(),
          "native/lib/libgramvoip.so": open(os.path.join(HERE, "build/libgramvoip.so"), "rb").read()}
EXEC = {"native/gram", "native/gramd"}
# TEST_PERMS=1: without the permissions that make BB10 show a consent prompt (for unattended test runs)
UI_PERMS = "access_internet,run_when_backgrounded,use_camera" + ("" if os.environ.get("TEST_PERMS") else ",access_shared,post_notification,record_audio")
SVC_PERMS = "access_internet,run_when_backgrounded" + ("" if os.environ.get("TEST_PERMS") else ",post_notification")
for root, _, files in os.walk(os.path.join(HERE, "assets")):
    for f in files:
        p = os.path.join(root, f)
        assets["native/assets/" + os.path.relpath(p, os.path.join(HERE, "assets"))] = open(p, "rb").read()

m = f"""Archive-Manifest-Version: 1.5
Archive-Created-By: bq native packager 1.0

Package-Type: application
Package-Author: bq
Package-Author-Id: {ident('A', 'bq')}
Package-Name: {PKG}
Package-Id: {ident('B', PKG)}
Package-Version: {VER}
Package-Version-Id: {ident('C', PKG + VER)}
Package-Architecture: armle-v7

Application-Name: {NAME}
Application-Id: {ident('D', PKG)}
Application-Description: Native messaging client
Application-Version: {VER}
Application-Version-Id: {ident('E', PKG + VER)}
Application-Requires-System: BlackBerry 10/10.3.0.0
Application-Development-Mode: false

Entry-Point-Name: {NAME}
Entry-Point-Key: e1
Entry-Point: app/native/gram
Entry-Point-Type: Qnx/Elf
Entry-Point-Icon: {{144x144}}native/icon.png
Entry-Point-Orientation: auto
Entry-Point-User-Actions: {UI_PERMS}
Entry-Point-System-Actions: run_native

Entry-Point-Name: {NAME} service
Entry-Point-Key: gramd
Entry-Point: app/native/gramd
Entry-Point-Type: Qnx/Elf
Entry-Point-Icon: {{144x144}}native/icon.png
Entry-Point-User-Actions: {SVC_PERMS}
Entry-Point-System-Actions: run_native,_sys_run_headless,_sys_headless_nostop,_sys_headless_xlarge

Invoke-Target-Key: org.bq.gram.service
Invoke-Target-Entry-Point-Ref: gramd
Invoke-Target-Type: application.headless
Invoke-Target-Filter: actions=org.bq.gram.START;types=*;
Invoke-Target-Filter: actions=bb.action.system.STARTED;types=application/vnd.blackberry.system.event.STARTED;uris=data://local;
Invoke-Target-Filter: actions=bb.action.autostart.INSTALLED;types=application/vnd.blackberry.system.event.STARTED;uris=data://local;

Invoke-Target-Key: org.bq.gram.open
Invoke-Target-Label: {NAME}
Invoke-Target-Entry-Point-Ref: e1
Invoke-Target-Type: application
Invoke-Target-Filter: actions=bb.action.OPEN;types=application/vnd.bq.gram.chat;

"""
for k, v in assets.items():
    m += f"Archive-Asset-Name: {k}\nArchive-Asset-SHA-512-Digest: {b64(hashlib.sha512(v).digest())}\n"
    if k in EXEC: m += "Archive-Asset-Type: Qnx/Elf\n"         # the installer makes these executable
    m += "\n"
out = os.path.join(HERE, "build", PKG + ".bar")
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    z.writestr("META-INF/MANIFEST.MF", m)
    for k, v in assets.items():
        zi = zipfile.ZipInfo(k)
        zi.external_attr = (0o755 if k in EXEC else 0o644) << 16
        zi.compress_type = zipfile.ZIP_DEFLATED
        z.writestr(zi, v)
print(out, os.path.getsize(out), "bytes;", len(assets), "assets")
