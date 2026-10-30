# Building WorldED

Run these commands from the `arcanum-modern-source` repository root. WorldED
has its own CMake project in `tools/worlded`; you do not need to prepare or
compile the Renaissance engine to build the editor.

You need CMake 3.20 or newer, a C++17 compiler, zlib and Python 3 for the tests.
The graphical editor and housing exporter also need Qt 6.4 or newer with
Widgets and Concurrent. GUI tests use Qt Test, supplied with the Qt base package.
Original Arcanum files are needed to use the editor, but not for the synthetic
format and GUI tests below.

## Windows with Visual Studio

Install Git, CMake, Python 3 and Visual Studio Build Tools with **Desktop
development with C++** and a Windows SDK. Install a 64-bit MSVC Qt kit. Keep the
compiler, Qt and zlib on the same architecture; do not mix MinGW Qt with MSVC.

The commands below use Visual Studio 2019, Qt 6.5.3 `msvc2019_64` and zlib 1.3.2.
This combination has been built and tested on Windows. If you use another kit,
change the Qt path and CMake generator to match it. Run the commands in PowerShell.
They assume `cmake`, `ctest`, `git` and `python` are on PATH.

First build a static zlib. Its library will be linked into the editor, so there
is no separate zlib DLL to distribute:

```powershell
$qt = 'C:/Qt/6.5.3/msvc2019_64'
$zlibInstall = Join-Path $PWD 'build/worlded-zlib-install'
git clone --depth 1 --branch v1.3.2 https://github.com/madler/zlib.git build/worlded-zlib-src
cmake -S build/worlded-zlib-src -B build/worlded-zlib `
  -G 'Visual Studio 16 2019' -A x64 `
  -DZLIB_BUILD_SHARED=OFF -DZLIB_BUILD_TESTING=OFF `
  "-DCMAKE_INSTALL_PREFIX=$zlibInstall"
cmake --build build/worlded-zlib --config Release --parallel
cmake --install build/worlded-zlib --config Release
```

Configure and build WorldED:

```powershell
cmake -S tools/worlded -B build/worlded-windows `
  -G 'Visual Studio 16 2019' -A x64 `
  -DWORLDED_BUILD_GUI=ON `
  "-DCMAKE_PREFIX_PATH=$qt" `
  "-DZLIB_INCLUDE_DIR=$zlibInstall/include" `
  "-DZLIB_LIBRARY=$zlibInstall/lib/zs.lib"
cmake --build build/worlded-windows --config Release --parallel
$env:PATH = "$qt/bin;$env:PATH"
$env:PYTHONUTF8 = '1'
ctest --test-dir build/worlded-windows -C Release --output-on-failure
& ./build/worlded-windows/Release/worlded-explorer.exe --game 'D:/Games/Arcanum'
```

zlib 1.3.2 installs the Release static library as `zs.lib` with this configuration.
If you use another zlib package, set `ZLIB_LIBRARY` to its actual library path.
If CMake selects the wrong Python, add `-DPython3_EXECUTABLE=C:/path/to/python.exe`
to the configure command.

Visual Studio places the programs under `build/worlded-windows/Release/`:
`worlded-explorer.exe`, `worlded.exe` and `worlded-house-export.exe`.
`worlded-gui-check.exe` is the test program. CTest runs the format tests, GUI
tests and housing export tests. Keep the Qt DLL directory on PATH while running
these executables from the build tree.

### Make a portable editor folder

Copy the three user programs into a new folder, then run the deployment tool
from the same Qt kit:

```powershell
$package = Join-Path $PWD 'dist/WorldED-Renaissance-windows-x64'
New-Item -ItemType Directory -Force $package | Out-Null
Copy-Item build/worlded-windows/Release/worlded.exe $package
Copy-Item build/worlded-windows/Release/worlded-explorer.exe $package
Copy-Item build/worlded-windows/Release/worlded-house-export.exe $package
& "$qt/bin/windeployqt.exe" --release --no-translations "$package/worlded-explorer.exe"
```

Keep the Qt DLLs and plugin folders together with the executables. In particular,
`platforms/qwindows.dll` is required. The target computer also needs the x64
Microsoft Visual C++ runtime; install the redistributable supplied with your
Visual Studio toolchain if it is absent. Verify the package on a machine or
terminal that does not get Qt DLLs from the SDK's PATH. Copying an `.exe` alone
is not a portable package.

## Linux

On Ubuntu 24.04, install the distribution's compiler and Qt development packages:

```sh
sudo apt update
sudo apt install build-essential cmake ninja-build qt6-base-dev zlib1g-dev python3
cmake -S tools/worlded -B build/worlded-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DWORLDED_BUILD_GUI=ON
cmake --build build/worlded-linux --parallel
ctest --test-dir build/worlded-linux --output-on-failure
build/worlded-linux/worlded-explorer --game /path/to/Arcanum
```

Check the Qt version on older distributions. Ubuntu 22.04 supplies Qt 6.2, which
is below WorldED's minimum. Install a newer Qt SDK and pass its kit directory
when configuring:

```sh
cmake -S tools/worlded -B build/worlded-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DWORLDED_BUILD_GUI=ON \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/gcc_64"
```

Qt 6.8.3 with GCC on Ubuntu 22.04 is another tested configuration. Install the
SDK's runtime dependencies as well, including the matching ICU libraries when
using a downloaded Qt build. Use a fresh build directory when switching Qt kits
or compilers. An error naming a missing `libicu*.so` is a Qt runtime dependency
problem; it is not fixed by rebuilding WorldED.

Ninja puts the three programs directly in `build/worlded-linux/`. To install
them elsewhere:

```sh
cmake --install build/worlded-linux --prefix "$HOME/.local"
```

This installs the executables, not a bundled Qt runtime. The destination machine
must have compatible Qt and zlib libraries. The automated GUI test uses an
offscreen platform; the interactive editor needs a graphical desktop.

## CLI without Qt

If you only need resource inspection, omit the GUI and exporter:

```sh
cmake -S tools/worlded -B build/worlded-cli -DCMAKE_BUILD_TYPE=Release \
  -DWORLDED_BUILD_GUI=OFF
cmake --build build/worlded-cli --config Release
ctest --test-dir build/worlded-cli -C Release --output-on-failure
```

zlib and a C++17 compiler are still required. Set `BUILD_TESTING=OFF` if you
intentionally want a build without Python or tests.

## Optional engine helpers

The editor's normal build does not produce `worlded-native-inventory` or the
real-engine regression programs. These link against a prepared Renaissance
engine build. The helper script currently supports Linux Ninja builds.
Follow the repository's [development guide](development.md) to build the engine
first, then point `--build-dir` at that engine build, not the WorldED build:

```sh
python3 tools/worlded/build_engine_check.py --build-dir build --inventory
python3 tools/worlded/build_engine_check.py --build-dir build --scene
```

The results are `build/worlded/worlded-native-inventory` and
`build/worlded/check-worlded-scene`. Place the inventory helper beside
`worlded-explorer` to use generated NPC inventories in test copies. This helper
is not currently part of the standalone Windows CMake build.

The scene regression accepts a test copy prepared by WorldED:

```sh
build/worlded/check-worlded-scene /path/to/copy/game Slot0001 1200 1150
```

It requires the copy's `.worlded-playtest` marker and writes test save `Slot0098`.
It checks object population and static object order, positions and art across
save/load and sector eviction. Run it only on a disposable copy. Synthetic
CTest results do not replace checking doors, party movement, rendering and
script behavior in the actual game.
