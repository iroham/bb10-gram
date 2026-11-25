#!/usr/bin/env python3
"""Draw the app's icons (white on transparent, 96x96) into assets/icons/. Own simple shapes, no third-party
artwork. Also disc.png (white disc, tinted for avatars without a photo) and ring masks that round off square
photos on the two background colours.  python3 tools/icons.py"""
import math, os, struct, zlib
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "assets", "icons")
N, SS = 96, 4

def poly(pts):
    def f(x, y):
        inside = False
        j = len(pts) - 1
        for i in range(len(pts)):
            xi, yi = pts[i]; xj, yj = pts[j]
            if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi) + xi: inside = not inside
            j = i
        return inside
    return f
def circle(cx, cy, r): return lambda x, y: (x - cx) ** 2 + (y - cy) ** 2 <= r * r
def ring(cx, cy, r, w): return lambda x, y: (r - w) ** 2 <= (x - cx) ** 2 + (y - cy) ** 2 <= r * r
def rect(x0, y0, x1, y1): return lambda x, y: x0 <= x <= x1 and y0 <= y <= y1
def rrect(x0, y0, x1, y1, r):
    def f(x, y):
        if not (x0 <= x <= x1 and y0 <= y <= y1): return False
        cx = min(max(x, x0 + r), x1 - r); cy = min(max(y, y0 + r), y1 - r)
        return (x - cx) ** 2 + (y - cy) ** 2 <= r * r
    return f
def line(x0, y0, x1, y1, w):
    def f(x, y):
        dx, dy = x1 - x0, y1 - y0
        t = max(0, min(1, ((x - x0) * dx + (y - y0) * dy) / (dx * dx + dy * dy)))
        return (x - x0 - t * dx) ** 2 + (y - y0 - t * dy) ** 2 <= (w / 2) ** 2
    return f
def any_(*fs): return lambda x, y: any(f(x, y) for f in fs)
def sub(a, b): return lambda x, y: a(x, y) and not b(x, y)
def flip_v(f): return lambda x, y: f(x, 1 - y)
def flip_h(f): return lambda x, y: f(1 - x, y)
def arc(cx, cy, r, w, a0, a1):
    rg = ring(cx, cy, r, w)
    def f(x, y):
        if not rg(x, y): return False
        a = math.degrees(math.atan2(y - cy, x - cx)) % 360
        return a0 <= a <= a1 if a0 <= a1 else (a >= a0 or a <= a1)
    return f

def rot(f, deg):                     # turn a shape around the centre
    c, sn = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    return lambda x, y: f(0.5 + (x - 0.5) * c + (y - 0.5) * sn, 0.5 - (x - 0.5) * sn + (y - 0.5) * c)
W = 0.06
clip = rot(any_(line(0.36, 0.26, 0.36, 0.72, W), arc(0.47, 0.72, 0.11 + W / 2, W, 0, 180), line(0.58, 0.72, 0.58, 0.20, W),
                arc(0.50, 0.20, 0.08 + W / 2, W, 180, 360), line(0.42, 0.20, 0.42, 0.62, W)), 40)
check1 = any_(line(0.20, 0.54, 0.40, 0.72, 0.085), line(0.40, 0.72, 0.80, 0.30, 0.085))
plane = sub(poly([(0.10, 0.46), (0.90, 0.12), (0.62, 0.90), (0.47, 0.60)]), poly([(0.47, 0.60), (0.80, 0.22), (0.40, 0.52)]))
arrow_l = any_(line(0.20, 0.42, 0.44, 0.20, 0.09), line(0.20, 0.42, 0.44, 0.64, 0.09), arc(0.50, 0.72, 0.30, 0.09, 270, 360), line(0.20, 0.42, 0.52, 0.42, 0.09))
bell = any_(sub(circle(0.5, 0.46, 0.26), rect(0, 0.46, 1, 1)), rect(0.24, 0.46, 0.76, 0.70), rrect(0.16, 0.68, 0.84, 0.76, 0.03), circle(0.5, 0.84, 0.07), circle(0.5, 0.18, 0.05))
# phone handset: a C-shaped grip with ear and mouth pieces, turned to the usual diagonal
handset0 = any_(arc(0.62, 0.5, 0.36, 0.13, 112, 248), rrect(0.44, 0.10, 0.70, 0.30, 0.07), rrect(0.44, 0.70, 0.70, 0.90, 0.07))
handset = rot(handset0, -40)
speaker_cone = any_(rrect(0.10, 0.38, 0.26, 0.62, 0.02), poly([(0.24, 0.38), (0.48, 0.18), (0.48, 0.82), (0.24, 0.62)]))
mic0 = any_(rrect(0.38, 0.08, 0.62, 0.58, 0.12), sub(arc(0.5, 0.42, 0.24, 0.06, 0, 180), rect(0, 0, 1, 0.42)), rect(0.47, 0.66, 0.53, 0.84), rect(0.34, 0.84, 0.66, 0.90))
arrow_dl = any_(line(0.74, 0.26, 0.30, 0.70, 0.10), line(0.28, 0.72, 0.28, 0.40, 0.10), line(0.28, 0.72, 0.60, 0.72, 0.10))
ICONS = {
    "backspace": any_(sub(poly([(0.06, 0.5), (0.32, 0.20), (0.94, 0.20), (0.94, 0.80), (0.32, 0.80)]),
                          poly([(0.155, 0.5), (0.355, 0.27), (0.87, 0.27), (0.87, 0.73), (0.355, 0.73)])),
                      line(0.48, 0.38, 0.72, 0.62, 0.07), line(0.48, 0.62, 0.72, 0.38, 0.07)),
    "mic": any_(rrect(0.38, 0.08, 0.62, 0.58, 0.12), sub(arc(0.5, 0.42, 0.24, 0.06, 0, 180), rect(0, 0, 1, 0.42)), rect(0.47, 0.66, 0.53, 0.84), rect(0.34, 0.84, 0.66, 0.90)),
    "lock": any_(sub(rrect(0.30, 0.10, 0.70, 0.62, 0.20), rrect(0.38, 0.18, 0.62, 0.62, 0.12)), sub(rrect(0.20, 0.44, 0.80, 0.92, 0.08), circle(0.5, 0.66, 0.07))),
    # smiling face with a folded corner (stickers)
    "sticker": any_(sub(sub(circle(0.5, 0.5, 0.40), circle(0.5, 0.5, 0.32)), poly([(0.62, 0.90), (0.90, 0.62), (0.95, 0.95)])),
                    line(0.62, 0.90, 0.90, 0.62, 0.07), circle(0.38, 0.42, 0.05), circle(0.62, 0.42, 0.05),
                    arc(0.5, 0.52, 0.18, 0.06, 20, 160)),
    # keyboard: rounded box with keys
    "keyboard": any_(sub(rrect(0.08, 0.24, 0.92, 0.78, 0.08), rrect(0.15, 0.31, 0.85, 0.71, 0.04)),
                     rect(0.22, 0.38, 0.30, 0.46), rect(0.36, 0.38, 0.44, 0.46), rect(0.50, 0.38, 0.58, 0.46), rect(0.64, 0.38, 0.72, 0.46),
                     rect(0.30, 0.56, 0.70, 0.63)),
    "send": plane,
    "attach": clip,
    "sent": check1,
    "read": any_(line(0.06, 0.54, 0.26, 0.72, 0.08), line(0.26, 0.72, 0.62, 0.30, 0.08), line(0.46, 0.66, 0.52, 0.72, 0.08), line(0.52, 0.72, 0.90, 0.30, 0.08)),
    "sending": any_(ring(0.5, 0.5, 0.36, 0.075), line(0.5, 0.5, 0.5, 0.28, 0.075), line(0.5, 0.5, 0.64, 0.58, 0.075)),
    "failed": any_(ring(0.5, 0.5, 0.38, 0.075), rrect(0.455, 0.26, 0.545, 0.56, 0.02), circle(0.5, 0.70, 0.055)),
    "muted": any_(sub(bell, line(0.16, 0.16, 0.84, 0.84, 0.16)), line(0.18, 0.18, 0.82, 0.82, 0.07)),
    "bell": bell,
    "pin": any_(poly([(0.36, 0.12), (0.64, 0.12), (0.60, 0.20), (0.60, 0.46), (0.74, 0.60), (0.26, 0.60), (0.40, 0.46), (0.40, 0.20)]), poly([(0.46, 0.60), (0.54, 0.60), (0.50, 0.90)])),
    "search": any_(ring(0.42, 0.42, 0.27, 0.075), line(0.61, 0.61, 0.84, 0.84, 0.10)),
    "back": any_(line(0.60, 0.20, 0.32, 0.50, 0.10), line(0.32, 0.50, 0.60, 0.80, 0.10)),
    "close": any_(line(0.24, 0.24, 0.76, 0.76, 0.10), line(0.76, 0.24, 0.24, 0.76, 0.10)),
    "delete": any_(rrect(0.20, 0.22, 0.80, 0.30, 0.02), rrect(0.40, 0.12, 0.60, 0.22, 0.02),
                   sub(poly([(0.26, 0.34), (0.74, 0.34), (0.70, 0.88), (0.30, 0.88)]), any_(rect(0.40, 0.44, 0.45, 0.78), rect(0.55, 0.44, 0.60, 0.78)))),
    "reply": arrow_l,
    "forward": flip_h(arrow_l),
    "copy": any_(sub(rrect(0.30, 0.26, 0.82, 0.88, 0.06), rrect(0.38, 0.34, 0.74, 0.80, 0.03)), sub(sub(rrect(0.16, 0.10, 0.68, 0.72, 0.06), rrect(0.24, 0.18, 0.60, 0.64, 0.03)), rrect(0.30, 0.26, 0.82, 0.88, 0.06))),
    "edit": any_(poly([(0.16, 0.84), (0.20, 0.66), (0.34, 0.80)]), poly([(0.24, 0.62), (0.62, 0.24), (0.76, 0.38), (0.38, 0.76)]), poly([(0.66, 0.20), (0.74, 0.12), (0.88, 0.26), (0.80, 0.34)])),
    "download": any_(rrect(0.455, 0.12, 0.545, 0.56, 0.02), poly([(0.26, 0.42), (0.74, 0.42), (0.50, 0.68)]), rrect(0.18, 0.76, 0.82, 0.85, 0.03)),
    "file": sub(poly([(0.22, 0.10), (0.60, 0.10), (0.80, 0.30), (0.80, 0.90), (0.22, 0.90)]), any_(poly([(0.58, 0.14), (0.58, 0.32), (0.76, 0.32)]), rect(0.32, 0.50, 0.70, 0.56), rect(0.32, 0.66, 0.70, 0.72))),
    "play": poly([(0.30, 0.20), (0.30, 0.80), (0.80, 0.50)]),
    "pause": any_(rrect(0.26, 0.20, 0.42, 0.80, 0.02), rrect(0.58, 0.20, 0.74, 0.80, 0.02)),
    "archive": any_(rrect(0.12, 0.16, 0.88, 0.34, 0.04), sub(rrect(0.18, 0.38, 0.82, 0.86, 0.05), rrect(0.38, 0.50, 0.62, 0.58, 0.03))),
    "more": any_(circle(0.5, 0.22, 0.075), circle(0.5, 0.5, 0.075), circle(0.5, 0.78, 0.075)),
    "settings": sub(any_(circle(0.5, 0.5, 0.27), *[line(0.5, 0.5, 0.5 + 0.36 * math.cos(math.radians(a)), 0.5 + 0.36 * math.sin(math.radians(a)), 0.13) for a in range(0, 360, 45)]), circle(0.5, 0.5, 0.11)),
    "done": check1,
    "group": any_(circle(0.36, 0.36, 0.14), sub(circle(0.36, 0.88, 0.30), rect(0, 0.80, 1, 1)), circle(0.68, 0.40, 0.11), sub(sub(circle(0.70, 0.86, 0.24), rect(0, 0.80, 1, 1)), circle(0.36, 0.88, 0.34))),
    "megaphone": any_(poly([(0.14, 0.40), (0.34, 0.40), (0.74, 0.18), (0.74, 0.78), (0.34, 0.58), (0.14, 0.58)]), rrect(0.22, 0.58, 0.34, 0.84, 0.03), rrect(0.80, 0.42, 0.88, 0.54, 0.03)),
    "logout": any_(sub(rrect(0.14, 0.14, 0.60, 0.86, 0.05), any_(rrect(0.22, 0.22, 0.60, 0.78, 0.02), rect(0.52, 0.36, 0.62, 0.64))), line(0.40, 0.50, 0.84, 0.50, 0.08), line(0.70, 0.36, 0.84, 0.50, 0.08), line(0.70, 0.64, 0.84, 0.50, 0.08)),
    "disc": circle(0.5, 0.5, 0.5),
    "call": handset,
    "callend": rot(handset0, 90),
    "speaker": any_(speaker_cone, arc(0.50, 0.5, 0.18, 0.07, 300, 60), arc(0.50, 0.5, 0.32, 0.07, 305, 55)),
    "micoff": any_(sub(mic0, line(0.14, 0.10, 0.86, 0.90, 0.17)), line(0.16, 0.12, 0.84, 0.88, 0.07)),
    "callin": arrow_dl,
    "callout": rot(arrow_dl, 180),
    "like": sub(any_(circle(0.34, 0.36, 0.18), circle(0.66, 0.36, 0.18), poly([(0.17, 0.42), (0.83, 0.42), (0.50, 0.84)])), rect(0, 0, 0, 0)),
    "down": any_(line(0.22, 0.36, 0.50, 0.64, 0.10), line(0.50, 0.64, 0.78, 0.36, 0.10)),
    "info": any_(ring(0.5, 0.5, 0.38, 0.07), circle(0.5, 0.31, 0.055), rrect(0.455, 0.42, 0.545, 0.72, 0.02)),
}
MASKS = {"mask": (255, 255, 255)}   # white ring around the disc; Avatar.qml tints it with the background colour (src/skin.cpp)

def png(f, rgb=(255, 255, 255)):
    rows = []
    for y in range(N):
        row = bytearray([0])
        for x in range(N):
            c = sum(1 for sy in range(SS) for sx in range(SS) if f((x + (sx + 0.5) / SS) / N, (y + (sy + 0.5) / SS) / N))
            row += bytes((rgb[0], rgb[1], rgb[2], c * 255 // (SS * SS)))
        rows.append(bytes(row))
    def chunk(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", N, N, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b"")

os.makedirs(OUT, exist_ok=True)
for name, f in ICONS.items():
    open(os.path.join(OUT, name + ".png"), "wb").write(png(f))
for name, rgb in MASKS.items():                # everything outside the disc painted in the background colour
    open(os.path.join(OUT, name + ".png"), "wb").write(png(lambda x, y: (x - 0.5) ** 2 + (y - 0.5) ** 2 > 0.25, rgb))
print(len(ICONS), "icons ->", os.path.normpath(OUT))
