#!/bin/bash
# Run a script from native-tg/scripts inside the build container. From bb10-tools/: native-tg/scripts/in-container.sh <script> [args]
# Mounts: /src AOSP tree (clang), /qnxsdk BB10 SDK, /w phase6 (toolchain), /t native-tg, /b build volume (tg-build).
cd "$(dirname "$0")/../.."
DOCKER_CONTEXT=colima-aosp docker run --rm -v a11-src:/src -v $PWD/phase6/qnxsdk:/qnxsdk:ro -v $PWD/phase6:/w -v $PWD/native-tg:/t -v tg-build:/b -w /b a11-build bash /t/scripts/"$@" 2>&1 | grep -v "requested image"
exit ${PIPESTATUS[0]}
