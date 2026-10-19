#!/usr/bin/env bash
# Run from Linux with LLVM-MinGW, or Git Bash on a Windows build machine.
set -euo pipefail
arcanum_project=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
arcanum_build=${ARCANUM_WINDOWS_BUILD_DIR:-"$arcanum_project/build/windows-x64"}
arcanum_output=
arcanum_package_args=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --include-game)
            [[ $# -ge 2 && -d "$2" ]] || { echo '--include-game requires an existing game directory.' >&2; exit 1; }
            arcanum_package_args+=(--include-game "$2"); shift 2 ;;
        -h|--help)
            echo 'Usage: ./build-windows.sh [output.zip] [--include-game GAME_DIRECTORY]'
            exit 0 ;;
        -*) echo "Unknown option: $1" >&2; exit 1 ;;
        *) [[ -z "$arcanum_output" ]] || { echo 'Only one output ZIP may be specified.' >&2; exit 1; }
            arcanum_output=$1; shift ;;
    esac
done
if [[ -z "$arcanum_output" ]]; then
    arcanum_output="$arcanum_project/dist/ArcanumModern-windows-x64.zip"
    [[ ${#arcanum_package_args[@]} -eq 0 ]] || arcanum_output="$arcanum_project/dist/ArcanumModern-windows-x64-full.zip"
fi
arcanum_cmake=${CMAKE:-cmake}
arcanum_python=${PYTHON:-python3}
command -v "$arcanum_cmake" >/dev/null || { echo "CMake is required (set CMAKE if necessary)." >&2; exit 1; }
command -v "$arcanum_python" >/dev/null || { echo "Python 3 is required (set PYTHON if necessary)." >&2; exit 1; }
args=(-DCMAKE_BUILD_TYPE=Release)
case "$(uname -s)" in
    Linux*)
        : "${LLVM_MINGW:?Set LLVM_MINGW to an extracted Linux LLVM-MinGW toolchain}"
        [[ -x "$LLVM_MINGW/bin/x86_64-w64-mingw32-clang" ]] || { echo "Invalid LLVM_MINGW toolchain" >&2; exit 1; }
        args+=(-G Ninja "-DCMAKE_TOOLCHAIN_FILE=$arcanum_project/tools/cmake/windows-llvm.cmake")
        [[ -z ${NINJA:-} ]] || args+=("-DCMAKE_MAKE_PROGRAM=$NINJA")
        ;;
    MINGW*|MSYS*)
        args+=(-G "Visual Studio 17 2022" -A x64 -DCMAKE_POLICY_DEFAULT_CMP0091=NEW '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>')
        ;;
    *) echo "Use Linux with LLVM-MinGW or Git Bash with Visual Studio 2022 C++ tools." >&2; exit 1;;
esac
bash "$arcanum_project/prepare-source.sh"
# Reuse source-only dependency downloads; never reuse Linux binaries.
if [[ -n ${ARCANUM_DEPENDENCY_CACHE:-} ]]; then
    for dep in fpattern sdl3 sdl3_mixer zlib; do
        [[ ! -d "$ARCANUM_DEPENDENCY_CACHE/$dep-src" ]] || args+=("-DFETCHCONTENT_SOURCE_DIR_${dep^^}=$ARCANUM_DEPENDENCY_CACHE/$dep-src")
    done
fi
"$arcanum_cmake" -S "$arcanum_project/source" -B "$arcanum_build" "${args[@]}"
"$arcanum_cmake" --build "$arcanum_build" --config Release --parallel "${ARCANUM_BUILD_JOBS:-8}"
arcanum_exe="$arcanum_build/arcanum-ce.exe"
[[ ! -f "$arcanum_build/Release/arcanum-ce.exe" ]] || arcanum_exe="$arcanum_build/Release/arcanum-ce.exe"
"$arcanum_python" "$arcanum_project/tools/package-windows.py" --exe "$arcanum_exe" --build-dir "$arcanum_build" --output "$arcanum_output" "${arcanum_package_args[@]}"
