#!/usr/bin/env bash
set -euo pipefail
arcanum_project=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
bash "$arcanum_project/prepare-source.sh"
arcanum_build=${ARCANUM_BUILD_DIR:-"$arcanum_project/build"}
arcanum_sdk=${ARCANUM_BUILD_SDK:-/tmp/arcanum-ce-sdk}
arcanum_cache=${ARCANUM_DEPENDENCY_CACHE:-/tmp/arcanum-ce-build/_deps}
args=(-DCMAKE_BUILD_TYPE=RelWithDebInfo -DSDL_WAYLAND=OFF -DSDL_OPENGL=OFF)
if [[ -x "$arcanum_sdk/usr/bin/cmake" ]]; then
    arcanum_cmake="$arcanum_sdk/usr/bin/cmake"
    export LD_LIBRARY_PATH="$arcanum_sdk/usr/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export PKG_CONFIG_PATH="$arcanum_sdk/usr/lib64/pkgconfig:$arcanum_sdk/usr/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
    args+=(-G Ninja "-DCMAKE_MAKE_PROGRAM=$arcanum_sdk/usr/bin/ninja"
        "-DCMAKE_PREFIX_PATH=$arcanum_sdk/usr" "-DCMAKE_C_FLAGS=-I$arcanum_sdk/usr/include")
else
    arcanum_cmake=$(command -v cmake) || { echo 'CMake et les dépendances de compilation sont nécessaires (voir README.md).' >&2; exit 1; }
fi
for dep in fpattern sdl3 sdl3_mixer zlib; do
    if [[ -d "$arcanum_cache/$dep-src" ]]; then
        args+=("-DFETCHCONTENT_SOURCE_DIR_${dep^^}=$arcanum_cache/$dep-src")
    fi
done
"$arcanum_cmake" -S "$arcanum_project/source" -B "$arcanum_build" "${args[@]}"
"$arcanum_cmake" --build "$arcanum_build" --parallel "${ARCANUM_BUILD_JOBS:-8}"
cp "$arcanum_build/arcanum-ce" "$arcanum_project/game/arcanum-modern.new"
mv "$arcanum_project/game/arcanum-modern.new" "$arcanum_project/game/arcanum-modern"
printf '%s\n' 'Compilation installée. Elle sera utilisée au prochain lancement.'
