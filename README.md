# Arcanum Modern

Arcanum Modern is an unofficial engine and interface project for **Arcanum: Of
Steamworks and Magick Obscura**, based on
[Arcanum Community Edition](https://github.com/alexbatalov/arcanum-ce).
It brings the game to modern displays with sharp text, a redesigned interface,
world zoom and direct companion control in turn-based combat, while keeping the
original artwork and game systems.

This is an **experimental, playable project**. You need the game data from your
own Arcanum installation, updated to **1.0.7.4**. This repository contains the
mod's source code, build scripts and interface resources. It does not contain
commercial game data, compiled executables, ZIP packages or generated map caches.
Build the engine using the instructions below, then add your own game files to
play.

## Features

- **Native-resolution interface:** readable text and controls that keep their
  size while you zoom the world. The visual style uses wood, brass, parchment
  and the game's original artwork.
- **Redesigned screens:** HUD, party portraits, character sheet, inventory,
  trading, dialogue, journal, maps, spell and skill panels, save/load menus and
  display options.
- **Camera and display controls:** mouse-wheel zoom, keyboard camera movement,
  remappable keys, and windowed, desktop fullscreen and exclusive fullscreen
  modes. The world adapts to the window's aspect ratio.
- **Inventory tools:** search, categories, equipment comparison, item details,
  transfer shortcuts and editable quick slots with hover descriptions.
- **Journal and exploration:** quest filters, search, favorites, quest tracking
  and personal map notes.
- **Optional detailed local maps:** maps rendered from the installed game data,
  with opaque roofs, exploration fog and separately drawn localized labels and
  markers. Original maps remain available when the detailed cache is absent.
- **Tactical combat:** direct control of eligible companions during their own
  turns, action previews, spell availability and a combat log. Initiative,
  action points, fatigue, damage and spell restrictions follow the game's rules.
- **Readable game text:** books, letters, newspapers, inscriptions, telegrams
  and floating dialogue use the installed game's content.
- **Maintained spell controls:** hover an active effect to identify it and click
  to stop it when its caster is available.
- **Nearby world sounds:** footsteps, doors and other positional effects are
  limited by their distance from the player.
- **Optional unlimited companions:** disabled by default; recruitment conditions
  still apply.

Names, descriptions and most labels follow the language of your game files.
Additional interface text has partial French and English support. Western game
text is decoded as Windows-1252; other encodings need validation.

## Where to put the game files

Copy the contents of your installed Arcanum game into the repository's `game/`
folder. Keep the original directory structure, including all game archives,
module patches and loose data files. The repository already supplies
`game/modern-ui/`; keep those resources in place.

After adding your own game files, the layout should look like this. The game
archives and module data shown below are not supplied by the repository:

```text
ArcanumModern/
├── README.md
├── build.sh
├── build-macos.sh
├── build-windows.sh
├── source/
└── game/
    ├── arcanum1.dat
    ├── arcanum2.dat
    ├── arcanum3.dat
    ├── Arcanum4.dat                 # If supplied by your installation
    ├── tig.dat
    ├── data/                       # Installed loose data, if present
    ├── modules/
    │   ├── Arcanum.dat
    │   ├── Arcanum.patch0           # Keep all installed module patches
    │   ├── Arcanum/                 # Loose module data and saved games
    │   └── ...                     # Other installed modules
    └── modern-ui/                  # Resources supplied by this project
        ├── Book.ttf
        ├── Heading.ttf
        ├── ...
        └── local-maps/              # Optional cache you generate separately
```

Use a writable folder so the game can create preferences, caches and saves.
A separate copy of your installation is recommended. Back up existing saves
before using a different engine.

After generating a package, the same layout applies without the source files:
`arcanum-modern.exe` and its launcher on Windows, or `Arcanum Modern.app` on macOS,
must sit **beside `arcanum1.dat` and `modern-ui/`**. On macOS, the game data stays
outside the `.app` bundle; keep the application in the game folder.

## Build from source

Clone this repository with Git, then run the commands below from its root.
The packaging tools use Git metadata to record the source revision, so use a
Git checkout when creating packages rather than a downloaded source-only ZIP.

The build scripts call `prepare-source.sh` automatically. It obtains the pinned
upstream engine revision, applies `patches/engine.patch` and leaves the project's
modern interface modules in place. CMake downloads the required dependencies
when they are not cached. An uncached build needs internet access.

Use separate build directories for each target platform. Game data is needed to
play, generate detailed maps or create a complete ZIP; standard engine builds
and mod-only packages use the interface resources already in the repository.

| Target | Build environment | Command | Result |
| --- | --- | --- | --- |
| Linux | Linux with a C17 compiler and SDL development dependencies | `./build.sh` | `game/arcanum-modern` |
| macOS arm64 | macOS with Xcode Command Line Tools | `bash ./build-macos.sh` | `dist/ArcanumModern-macos-arm64.zip` |
| Windows x64 | Windows with Visual Studio 2022 and Git Bash | `PYTHON=python ./build-windows.sh` | `dist/ArcanumModern-windows-x64.zip` |
| Windows x64 | Linux with LLVM-MinGW, or Docker on Linux/macOS | See below | `dist/ArcanumModern-windows-x64.zip` |

These outputs are created by the build scripts; none are included in a fresh
checkout. Generated Windows and macOS packages contain the engine, launcher and
interface resources. By default, they exclude game data, saves and personal
preferences. Running an extracted package does not require a compiler, Python
or a separate SDL installation.

### Linux

Install Git, CMake, a C17 compiler, a build tool and the X11/audio development
packages required by SDL. Python 3.10 or later is useful for packaging and tests.
On Fedora/Nobara, the development packages include:

```sh
sudo dnf install gcc gcc-c++ cmake ninja-build git python3 \
  libX11-devel libXext-devel libXrandr-devel libXi-devel \
  libXinerama-devel libXcursor-devel alsa-lib-devel \
  pulseaudio-libs-devel libXScrnSaver-devel libXtst-devel
```

After placing the game data in `game/`:

```sh
./build.sh
./play.sh
```

The build script compiles the engine and installs `game/arcanum-modern`.
For a windowed session:

```sh
./play.sh -window -geometry=1280x720
```

The launcher uses Vulkan by default and writes `game/modern-launch.log`.
`SDL_RENDER_DRIVER` selects another available SDL renderer. The provided Linux
build configuration uses X11/XWayland and disables SDL's OpenGL backend.

### macOS Apple Silicon

Install Xcode Command Line Tools, Git, CMake and Python 3.10 or later. Apple's
command-line tools can be installed with:

```sh
xcode-select --install
```

Build and extract the package into your prepared `game/` directory:

```sh
bash ./build-macos.sh
ditto -x -k dist/ArcanumModern-macos-arm64.zip game
open "game/Arcanum Modern.app"
```

The script targets Apple Silicon (arm64), macOS 11 or later. Wine and Rosetta
are not required. It builds static dependencies, verifies the
executable's architecture and linked libraries, and signs the application
ad-hoc. Building alone does not install the application into `game/`.
The application is not notarized by Apple.

The repository also includes a **macOS Apple Silicon** GitHub Actions workflow
that can be started manually when Actions is enabled for the repository.

### Windows: native build

The Windows builds target Windows 10/11, x64. After any of the Windows build
methods below finishes, extract the generated ZIP into your prepared game folder
and open **Play Arcanum Modern.bat**. Always extract the archive before launching
the game. The package keeps the original `Arcanum.exe`.

Install:

- Visual Studio 2022 with **Desktop development with C++**, including its Windows SDK.
- CMake and Git for Windows.
- Python 3.10 or later, available as `python` in Git Bash.

From **Git Bash** at the repository root:

```sh
PYTHON=python ./build-windows.sh
```

This selects the Visual Studio x64 generator and static C runtime. Extract
`dist/ArcanumModern-windows-x64.zip` into the prepared game folder and run its
launcher. The native Visual Studio build path still needs validation; the
LLVM-MinGW route below is also available.

### Windows: cross-build from Linux

Install Git, CMake, Ninja and Python 3.10 or later. Download and extract an
appropriate Linux-hosted **UCRT** toolchain from the
[LLVM-MinGW releases](https://github.com/mstorsjo/llvm-mingw/releases), then run:

```sh
export LLVM_MINGW=/path/to/llvm-mingw
./build-windows.sh
```

The package checks the executable's Windows x64 architecture and imported DLLs,
and includes the interface assets, license notices and a file-hash manifest.

### Windows: build with Docker on macOS or Linux

Install and start Docker. The LLVM-MinGW image contains the cross-compiler and
build tools, so the host does not need a Windows SDK.

From the repository root:

```sh
docker run --rm \
  --mount "type=bind,source=$PWD,target=/work" \
  --workdir /work \
  -e LLVM_MINGW=/opt/llvm-mingw \
  mstorsjo/llvm-mingw:latest \
  bash ./build-windows.sh
```

The output is written to the host's `dist/ArcanumModern-windows-x64.zip`.
The container needs network access for the first uncached build. This produces
a Windows executable; Docker is only used during the build.

### Create a complete Windows ZIP

This optional package must be generated locally; it is not included in the
repository. First populate `game/` with your own installed game files as described
above. Add `--include-game` when building to include those files in the ZIP:

```sh
./build-windows.sh --include-game ./game
```

For Docker, append the same option to the command inside the container:

```sh
docker run --rm \
  --mount "type=bind,source=$PWD,target=/work" \
  --workdir /work \
  -e LLVM_MINGW=/opt/llvm-mingw \
  mstorsjo/llvm-mingw:latest \
  bash ./build-windows.sh --include-game ./game
```

This creates **`dist/ArcanumModern-windows-x64-full.zip`** with the game's language
files, installed module patches, loose data and any detailed maps present in
`game/modern-ui/local-maps/`. A map-cache symlink is packaged as actual files.
Generating and including a complete map collection can add several gigabytes.

Saved games, player files, notes, personal preferences, screenshots, temporary
caches and macOS applications are excluded. Once generated, extract the complete
ZIP into a writable folder and open **Play Arcanum Modern.bat**; no separate
game-file copying is needed for that package. Without `--include-game`, packaging
remains mod-only.

To package an existing executable without rebuilding, run the packager in the
same environment used for the build:

```sh
python3 tools/package-windows.py \
  --exe build/windows-x64/arcanum-ce.exe \
  --build-dir build/windows-x64 \
  --include-game ./game
```

For a native Visual Studio build, the executable is under
`build/windows-x64/Release/arcanum-ce.exe`. Use `--output` to choose the ZIP path.

### Build options

Both platform packaging scripts accept an optional output ZIP path:

```sh
bash ./build-macos.sh /path/to/ArcanumModern-macos-arm64.zip
./build-windows.sh /path/to/ArcanumModern-windows-x64.zip
```

Useful environment variables:

| Variable | Purpose |
| --- | --- |
| `ARCANUM_BUILD_JOBS` | Parallel build jobs; defaults to 8. |
| `ARCANUM_BUILD_DIR` | Linux build directory. |
| `ARCANUM_MACOS_BUILD_DIR` | macOS build directory. |
| `ARCANUM_WINDOWS_BUILD_DIR` | Windows build directory. |
| `ARCANUM_DEPENDENCY_CACHE` | Directory containing reusable dependency source folders such as `sdl3-src`. |
| `ARCANUM_ENGINE_SOURCE` | Local upstream Git checkout containing the pinned engine revision. |
| `CMAKE`, `PYTHON` | Tool commands used by the macOS and Windows packaging scripts. |
| `NINJA` | Ninja executable for the Linux-to-Windows cross-build. |

## Detailed local maps

The detailed map cache is optional and is generated from your installed game
files. It contains static scenery with opaque roofs; people, portable objects,
quest markers and localized labels are not baked into the images. Exploration
fog remains active. Door images reflect the initial map state.

Place the cache's module directories under:

```text
game/modern-ui/local-maps/
├── Arcanum/
│   ├── manifest.json
│   └── ...
└── ...
```

To generate and validate the cache on macOS after building the engine:

```sh
python3 tools/build-map-exporter.py
python3 tools/generate-map-cache.py
python3 tools/verify-map-cache.py
```

The output is `dist/local-map-cache/`. Copy its contents into
`game/modern-ui/local-maps/`, or link that directory to the output. The exporter
uses an isolated working installation. Standard mod-only packages exclude the
cache; complete Windows packages include it when installed in the game folder.

See [the detailed map guide](docs/local-map-cache.md) for export options and
storage details. That guide is currently in French.

## Controls and gameplay

Change bindings in **Options → Key bindings**. AZERTY and QWERTY presets are
available.

| Action | Default |
| --- | --- |
| Inventory / journal / character sheet | I / J / C |
| Map | G |
| Spells / skills / schematics | M / K / T |
| Rest | R |
| Stealth / pickpocket | H / P |
| End turn | E or Space |
| Move camera | WASD positions, ZQSD on AZERTY, or arrow keys |
| Zoom world | Mouse wheel over the world |
| Use quick slot | 1–0 |
| Close panel or open pause menu | Escape |

Hover an item, spell or skill and press **1–0** to assign a quick slot.
Double-click an inventory item to transfer it where the game's rules allow;
**Shift-click** transfers a whole stack. Drag the map to pan it. In the journal,
the **star** marks favorites and the **pin** tracks a quest.

Turn-based combat is the default for fresh installations. Each controllable ally
acts during their own turn. Selecting a portrait moves the camera without
transferring the turn or restoring action points. A `~` on an action cost marks
an estimate; the engine determines the final cost.

A [more detailed control guide](docs/controls.md) is available in French.

## Preferences and saves

Paths are relative to the game folder:

| Path | Contents |
| --- | --- |
| `modern-ui/settings.ini` | Interface and gameplay preferences. |
| `modern-ui/graphics.ini` | Display mode and resolution. |
| `modern-ui/keys.ini` | Key bindings. |
| `modern-ui/notebook-*.txt` | Favorites, tracked quests and map notes per character. |
| `arcanum.cfg` | Engine and game settings. |
| `modules/<module>/Save/` | Saved games for that module. |

A separate `HighRes/config.ini` is not required. The world follows the actual
window dimensions, including when no saved display configuration exists.
Notebook files are independent of saves: loading an older save does not roll
back its notes. Keep both when moving your installation.

## Limitations

- The world uses the original sprites and terrain artwork. Detailed local maps
  improve map presentation without replacing those assets.
- The native 64-bit engine cannot load the original 32-bit `binkw32.dll`;
  Bink cinematics are unavailable without a compatible decoder.
- The supplied macOS script targets Apple Silicon. Intel macOS, mobile platforms,
  multiplayer and combinations with other engine patches are not validated.
- Do not apply the original High Resolution Patch to the modern executable.
- Some original interface paths remain as fallbacks. F9 switches presentation
  outside tactical combat.
- Automated checks and successful packaging do not replace gameplay testing on
  the target operating system.

## Contributing

Modern interface modules live in `source/src/ui/modern_*`. Changes to upstream
engine files are stored in `patches/engine.patch`. The pinned engine revision is
`a7ff41b300ef712f0e7d088183a3d08957110cdd`.

After editing an upstream file, regenerate the integration patch:

```sh
ARCANUM_ENGINE_SOURCE=/path/to/arcanum-ce python3 tools/update-engine-patch.py
```

Focused tests are in `tests/` and use Python and, where needed, a C compiler.
Examples:

```sh
python3 tests/test_display_config.py
python3 tests/test_lock_states.py
python3 tests/test_windows_full_package.py
python3 tests/test_windows_package.py dist/ArcanumModern-windows-x64.zip
```

Use an isolated game copy for interactive testing. Keep commercial game data,
generated map caches, builds, personal settings and saves out of Git.

## Credits and licenses

Arcanum was created by **Troika Games**. Arcanum Modern is an unofficial project.

The engine is based on **Arcanum Community Edition**, by **Alex Batalov**, under
its [Sustainable Use License](https://github.com/alexbatalov/arcanum-ce/blob/a7ff41b300ef712f0e7d088183a3d08957110cdd/LICENSE.md).
Packages include upstream and third-party license notices. Noto fonts use the
[SIL Open Font License](game/modern-ui/FONT-LICENSE.txt). SDL, SDL_mixer, zlib,
fpattern and stb retain their respective licenses.

Original game data is separate from the software licenses. It is excluded from
the repository and standard mod-only packages; the complete-package option uses
the installation explicitly supplied to the packaging tool.
