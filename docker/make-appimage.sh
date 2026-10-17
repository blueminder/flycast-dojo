#!/bin/bash
# Build Flycast Dojo and make the AppImage inside an Ubuntu 20.04 container.
# The 20.04 ABI matches the library list hard-coded in shell/linux/make-appimage.sh.
set -euo pipefail

cd "$(dirname "$0")/.."
IMAGE=flycast-dojo-builder:20.04
export BUILD_TYPE=${BUILD_TYPE:-RelWithDebInfo}

docker build -t $IMAGE docker/

# Everything runs in the mounted source tree so artifacts land on the host.
docker run --rm -t \
    -v "$(pwd)":/src \
    -e BUILD_TYPE \
    $IMAGE \
    bash -eux -c '
        # Fresh configure (avoids stale cache from other environments)
        rm -rf build appimage flycast-dojo-x86_64.AppImage

        cmake -B build -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
        cmake --build build --parallel "$(nproc)"

        # --- appimagetool FUSE workaround ---
        # Containers have no FUSE. Extract the real tool once, then place a
        # wrapper named appimagetool-x86_64.AppImage in the source root;
        # make-appimage.sh finds it there and uses it as-is.
        mkdir -p /opt/ait && cd /opt/ait
        wget -q https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
        chmod +x appimagetool-x86_64.AppImage
        ./appimagetool-x86_64.AppImage --appimage-extract > /dev/null
        printf "#!/bin/sh\nexec /opt/ait/squashfs-root/AppRun \"\$@\"\n" > /src/appimagetool-x86_64.AppImage
        chmod +x /src/appimagetool-x86_64.AppImage

        cd /src
        ./shell/linux/make-appimage.sh . build appimage
        rm -f /src/appimagetool-x86_64.AppImage

        # Sanity check: all bundled libs resolve
        ./flycast-dojo-x86_64.AppImage --appimage-extract > /dev/null
        OPT_LIBS=$(find squashfs-root/usr/optional -mindepth 1 -type d | tr "\n" ":")
        LD_LIBRARY_PATH="squashfs-root/usr/lib:$OPT_LIBS" \
            ldd squashfs-root/usr/bin/flycast | grep "not found" \
            && { echo "MISSING LIBS"; exit 1; } || echo "ALL LIBS RESOLVE"
        rm -rf squashfs-root
        ls -lh /src/flycast-dojo-x86_64.AppImage
    '
