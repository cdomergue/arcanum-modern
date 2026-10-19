#!/usr/bin/env bash
# Native macOS build; arm64 is explicit even when launched through Rosetta.
set -euo pipefail
arcanum_project=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
[[ $(uname -s) == Darwin ]] || { echo 'Ce build nécessite macOS et les outils Xcode (xcode-select --install).' >&2; exit 1; }
arcanum_build=${ARCANUM_MACOS_BUILD_DIR:-"$arcanum_project/build/macos-arm64"}
arcanum_output=${1:-"$arcanum_project/dist/ArcanumModern-macos-arm64.zip"}
arcanum_cmake=${CMAKE:-cmake}
arcanum_python=${PYTHON:-python3}
command -v "$arcanum_cmake" >/dev/null
command -v "$arcanum_python" >/dev/null
xcrun --sdk macosx --show-sdk-path >/dev/null
bash "$arcanum_project/prepare-source.sh"
args=(-DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DBUILD_SHARED_LIBS=OFF)
# Bash 3.2 (shipped by macOS) has no ${variable^^} expansion.
if [[ -n ${ARCANUM_DEPENDENCY_CACHE:-} ]]; then
    for dep in fpattern sdl3 sdl3_mixer zlib; do
        arcanum_upper=$(printf '%s' "$dep" | tr '[:lower:]' '[:upper:]')
        [[ ! -d "$ARCANUM_DEPENDENCY_CACHE/$dep-src" ]] || args+=("-DFETCHCONTENT_SOURCE_DIR_$arcanum_upper=$ARCANUM_DEPENDENCY_CACHE/$dep-src")
    done
fi
"$arcanum_cmake" -S "$arcanum_project/source" -B "$arcanum_build" "${args[@]}"
"$arcanum_cmake" --build "$arcanum_build" --config Release --parallel "${ARCANUM_BUILD_JOBS:-8}"
arcanum_app="$arcanum_build/Arcanum Community Edition.app"
[[ ! -d "$arcanum_build/Release/Arcanum Community Edition.app" ]] || arcanum_app="$arcanum_build/Release/Arcanum Community Edition.app"
"$arcanum_python" "$arcanum_project/tools/package-macos.py" --app "$arcanum_app" --build-dir "$arcanum_build" --output "$arcanum_output"
