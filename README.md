<p align="center">
  <img src="assets/appicon.png" width="96" height="96" alt="Gram icon">
</p>

<h1 align="center">Gram</h1>

<p align="center">
  A native Telegram client for BlackBerry 10.<br>
  Built on TDLib, with push notifications, voice calls, stickers and stories.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/BlackBerry_10-10.3-222222" alt="BlackBerry 10.3">
  <img src="https://img.shields.io/badge/version-0.9.0.6-2f80ed" alt="Version 0.9.0.6">
  <img src="https://img.shields.io/badge/requires-root-e11d48" alt="Requires root">
  <img src="https://img.shields.io/badge/license-MIT-34d399" alt="MIT licence">
</p>

---

Gram brings current Telegram to BlackBerry 10 as a Cascades app. Telegram's own client library, TDLib, is cross-compiled for the phone and runs in a small background service, so messages arrive in the BlackBerry Hub even when the app is closed.

## Features

- Chats, groups and channels, with replies, forwarding, polls, photos, video and files.
- Secret chats.
- Notifications in the BlackBerry Hub, one entry per chat, while the app is closed.
- Voice messages (Opus), WebP stickers, and animated stickers (Lottie `.tgs` and VP9 `.webm`).
- Voice calls with echo cancellation and noise suppression.
- Stories: a story bar on the chat list and a full-screen viewer.
- Archive, contacts, global search, profile editing.
- Admin tools for groups and channels you manage: member rights, admin list, chat details.
- App lock with a 4-digit PIN, or with your face through the [Glance](https://github.com/iroham/bb10-faceunlock) gate. While locked, notifications hide names and message text.
- Light and dark themes with several accent colours.

## Install

> [!IMPORTANT]
> Gram needs a rooted BlackBerry 10.3 device. The package is not signed by BlackBerry, and BB10 only installs unsigned packages through its system installer running as root.

There are two ways to install it.

### Option 1: over SSH

Download `org.bq.gram.bar` from the [latest release](https://github.com/iroham/bb10-gram/releases/latest). You need root SSH access to the device.

```bash
mkdir -p build && cp ~/Downloads/org.bq.gram.bar build/
BB10_HOST=root@192.0.2.10 ./install.sh
```

`install.sh` copies the package to `/var/nt/work` on the device, writes an installer job and prints the result. It calls `grep` and `sleep` from `/var/nt/bin`; change `B=` in the script if your tools live elsewhere. `BB10_HOST` defaults to an SSH alias named `passport`.

### Option 2: with BarStore

[BarStore](https://github.com/iroham/bb10-barstore) is an app store for rooted BlackBerry 10 devices. Once BarStore and its root helper are set up, Gram can be installed and updated from the store, with no computer or SSH needed. See the [BarStore README](https://github.com/iroham/bb10-barstore#install) for setup.

## First run: API credentials

Every Telegram client needs its own API ID and hash. Gram ships without any.

1. Sign in at [my.telegram.org](https://my.telegram.org) and open "API development tools".
2. Create an application and note its `api_id` and `api_hash`.
3. Enter both on Gram's first screen, then sign in with your phone number as usual.

The values are saved only in the app's private data folder (`data/api.json`, readable by the app alone).

## How it works

| Part | Role |
| --- | --- |
| `service/gramd.cpp` | Headless service that owns the TDLib session, posts Hub notifications and talks to the window over a local socket |
| `src/`, `assets/` | The Cascades app: C++ models over TDLib's JSON updates and the QML interface |
| `port/` | Thin C wrappers for TDLib, the sticker renderer and the call engine, each with a small on-device test |
| `scripts/` | Cross-build scripts for OpenSSL, TDLib, codecs, sticker decoders, libtgvoip and the app |
| `tools/` | Icon generators and fake data for testing the screens without an account (`tools/demo.py`) |

The service keeps around 7 MB of heap with one signed-in account, inside BB10's 30 MB limit for headless apps.

## Building from source

You need the BlackBerry 10 Native SDK (QNX toolchain, Qt 4.8, Cascades) and these third-party sources under `third_party/` (not included here):

| Location | Source | Licence | SHA-256 |
| --- | --- | --- | --- |
| `third_party/td` | [TDLib](https://github.com/tdlib/td) 1.8.70, commit `c8828d5` | BSL-1.0 | |
| `third_party/dl/opus-1.4.tar.gz` | [Opus](https://opus-codec.org) 1.4 | BSD-3-Clause | `c9b32b4253be5ae63d1ff16eea06b94b5f0f2951b7a02aceef58e3a3ce49c51f` |
| `third_party/dl/libwebp-1.3.2.tar.gz` | [libwebp](https://chromium.googlesource.com/webm/libwebp) 1.3.2 | BSD-3-Clause | `2a499607df669e40258e53d0ade8035ba4ec0175244869d1025d460562aa09b4` |
| `third_party/dl/rlottie-0.2.tar.gz` | [rlottie](https://github.com/Samsung/rlottie) 0.2 | MIT | `030ccbc270f144b4f3519fb3b86e20dd79fb48d5d55e57f950f12bab9b65216a` |
| `third_party/dl/libvpx-1.13.1.tar.gz` | [libvpx](https://chromium.googlesource.com/webm/libvpx) 1.13.1 | BSD-3-Clause | `00dae80465567272abd077f59355f95ac91d7809a2d3006f9ace2637dd429d14` |
| `third_party/dl/libtgvoip.tar.gz` | [libtgvoip](https://github.com/grishka/libtgvoip) 2.5 | Unlicense | `25ef433ad7d7d3194fe2ee516e74294deb0a389f94e9874e0add1da221caa1d0` |

The build volume (`/b` inside the container) also needs the OpenSSL 1.1.1w source at `/b/dl/openssl-1.1.1w.tar.gz`, the Qt 4.8.7 source for the host `moc`, and a host CMake at `/b/host/cmake`. The phone's own OpenSSL 1.0.1i is too old for TDLib, so Gram links its own.

Build in this order. Each step runs inside the build container through `scripts/in-container.sh`:

```bash
scripts/in-container.sh moc.sh           # host moc for Qt 4.8 -> /b/host/moc
scripts/in-container.sh openssl.sh       # OpenSSL 1.1.1w (static)
scripts/in-container.sh td-build.sh      # TDLib: patch, generate, cross-compile
scripts/in-container.sh td-link.sh       # -> build/libtdjson.so
scripts/in-container.sh media-libs.sh    # Opus and WebP
scripts/in-container.sh anim-libs.sh     # rlottie and libvpx
scripts/in-container.sh anim-link.sh     # -> build/libgramanim.so
scripts/in-container.sh voip-build.sh    # libtgvoip
scripts/in-container.sh voip-link.sh     # -> build/libgramvoip.so
scripts/in-container.sh app-build.sh     # -> build/gram, build/gramd
python3 package.py                       # -> build/org.bq.gram.bar
./install.sh
```

The scripts target a specific Docker setup: Docker context `colima-aosp`, image `a11-build`, the clang toolchain in volume `a11-src`, a build volume `tg-build`, and this repository checked out as `native-tg/` next to a `phase6/` directory holding the SDK and toolchain environment. Edit the `docker run` line in [`scripts/in-container.sh`](scripts/in-container.sh) for your own setup.

`scripts/td-patch.py` adapts TDLib to QNX, including a fix to its poll backend that otherwise keeps the service busy-looping on idle sockets.

## Disclaimer

Gram is an unofficial client. It is not affiliated with or endorsed by Telegram.

## Licence

The code in this repository is MIT. See [LICENSE](LICENSE). TDLib and the other libraries above keep their own licences.
