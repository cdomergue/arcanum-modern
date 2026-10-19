# Arcanum Modern

An experimental, unofficial modernization of **Arcanum: Of Steamworks and
Magick Obscura**, featuring a redesigned interface and **BG3-inspired tactical
combat**, based on [Arcanum Community Edition](https://github.com/alexbatalov/arcanum-ce).

## Installation

### Windows

**[Download the Windows setup](https://github.com/cdomergue/arcanum-modern/releases/latest/download/ArcanumModern-windows-x64-setup.exe).**

1. Install your own copy of **Arcanum**, updated to **1.0.7.4**.
2. Open the downloaded setup and select the folder where Arcanum is installed.
3. Launch **Arcanum Modern** using the shortcut on your desktop or in the Start menu.

The setup installs the mod for you and keeps your original game, saves and
settings. You do not need to compile anything or install development tools.
Close the game before installing or updating, and use a folder you can write to.

A Windows ZIP is also available in [Releases](https://github.com/cdomergue/arcanum-modern/releases/latest) for manual
installation: extract it into your Arcanum folder, then open **Play Arcanum Modern.bat**.

### macOS

Download the macOS ZIP from [Releases](https://github.com/cdomergue/arcanum-modern/releases/latest), extract it into
your Arcanum game folder, then open **Arcanum Modern.app** beside the game files.
This version requires an Apple Silicon Mac (M1 or newer) with macOS 11 or later.

### Linux

Download the Linux x64 ZIP from [Releases](https://github.com/cdomergue/arcanum-modern/releases/latest) and extract it
into your Arcanum game folder. Run **Play Arcanum Modern.sh** from that folder.
If your file manager does not run shell scripts, open a terminal there and enter
`./"Play Arcanum Modern.sh"`.

### Game files and HD maps

**You must supply your own Arcanum 1.0.7.4 game files.** Release downloads contain
only the mod: no original game data, saves, personal settings or HD map cache.
**On first launch, the game prepares HD local maps automatically**, with a progress
bar, on Windows, macOS and Linux. No extra tools or downloads are needed. Allow
about **8 GB** for Arcanum and Vormantown, plus temporary working space; the time
needed depends on your machine and installed modules. Choose **Play now** to use
the original maps immediately. Preparation resumes at the next launch, keeping
completed tiles. A complete cache is reused on subsequent launches.

If preparation fails, note the error code shown on screen and check
`modern-ui/.map-work/generation.log` inside your game folder. You can still choose
**Play now** and use the original maps.

To compile the mod yourself, see the [build instructions](#build-from-source).
Back up your saved games before trying an experimental engine.

## Features

- **BG3-inspired tactical combat:** take direct control of eligible companions
  on their turns, preview movement and actions, inspect costs and spell
  availability, and follow the fight in a combat log. Turn-based combat is
  enabled by default. Initiative, action points, fatigue, damage and spell
  restrictions follow Arcanum's rules.
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
- **Automatically prepared HD local maps:** maps rendered from the installed game data,
  with opaque roofs, exploration fog and separately drawn localized labels and
  markers. Original maps remain available when the detailed cache is absent.
- **Readable game text:** books, letters, newspapers, inscriptions, telegrams
  and floating dialogue use the installed game's content.
- **Optional unlimited companions:** disabled by default; recruitment conditions
  still apply.

Names, descriptions and most labels follow the language of your game files.
Additional interface text has partial French and English support. Western game
text is decoded as Windows-1252; other encodings need validation.

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

## Build from source

The repository contains source code, build scripts and interface resources.
Compiled executables, ZIP packages, game data and generated map caches are not
stored in Git.

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
| Linux x64 ZIP | Linux x64 with Python 3 and binutils, after building | See below | `dist/ArcanumModern-linux-x64.zip` |
| macOS arm64 | macOS with Xcode Command Line Tools | `bash ./build-macos.sh` | `dist/ArcanumModern-macos-arm64.zip` |
| Windows x64 | Windows with Visual Studio 2022 and Git Bash | `PYTHON=python ./build-windows.sh` | `dist/ArcanumModern-windows-x64.zip` |
| Windows x64 | Linux with LLVM-MinGW, or Docker on Linux/macOS | See below | `dist/ArcanumModern-windows-x64.zip` |

These outputs are created by the build scripts; none are included in a fresh
checkout. Generated packages contain the engine, launcher and
interface resources. By default, they exclude game data, saves and personal
preferences. Running an extracted package does not require a compiler, Python
or a separate SDL installation.

The repository also provides a [Windows setup generator](tools/package-windows-setup.py)
and a [release workflow](.github/workflows/release.yml). They produce downloadable
packages from the source; see [Windows setup](#create-a-windows-setup) and
[GitHub releases](#github-releases) below.

### Prepare your game files

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
        └── local-maps/              # HD cache generated on first launch
```

Use a writable folder so the game can create preferences, caches and saves.
A separate copy of your installation is recommended. Back up existing saves
before using a different engine.

After generating a package, the same layout applies without the source files:
`arcanum-modern.exe` and its launcher on Windows, or `Arcanum Modern.app` on macOS,
must sit **beside `arcanum1.dat` and `modern-ui/`**. On macOS, the game data stays
outside the `.app` bundle; keep the application in the game folder.

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

To create the Linux x64 ZIP, build in a dedicated directory and run the packager:

```sh
ARCANUM_BUILD_DIR=build/linux-x64 ./build.sh
python3 tools/package-linux.py --exe build/linux-x64/arcanum-ce --build-dir build/linux-x64
```

Packaging requires Python 3.10 or later and `readelf` from binutils. The output is
`dist/ArcanumModern-linux-x64.zip`, containing the executable, launcher, interface
resources and license notices. It excludes game data and personal settings.
SDL and the audio codecs are linked into the executable; the packager rejects
additional non-system library dependencies and records the required glibc version.

The release workflow builds on Ubuntu 22.04 for **x86-64 Linux with glibc 2.35
or newer**. Packages built on newer distributions may require a newer glibc.
An X11 or XWayland desktop, graphics drivers and an audio backend are still
required. musl-based distributions are not supported by this package.

Extract the generated ZIP into your writable Arcanum game folder, then run:

```sh
./"Play Arcanum Modern.sh"
```

If your archive tool discards executable permissions, restore them with
`chmod +x arcanum-modern "Play Arcanum Modern.sh"`. The launcher accepts the same
arguments as `play.sh` and writes `modern-launch.log` in the game folder.

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

### Create a Windows setup

First build the standard Windows ZIP using one of the methods above. Install
[NSIS 3](https://nsis.sourceforge.io/Download) on the build machine, then run:

```sh
python3 tools/package-windows-setup.py
```

On Windows, use `python` if that is your Python command. NSIS's `makensis` must
be on `PATH`, or supplied explicitly:

```sh
python tools/package-windows-setup.py --makensis "C:/Program Files (x86)/NSIS/makensis.exe"
```

The script also runs on Linux or macOS with NSIS installed. It reads
`dist/ArcanumModern-windows-x64.zip` and creates
**`dist/ArcanumModern-windows-x64-setup.exe`**. Use `--zip` and `--output` to
override those paths. It checks the ZIP's file list and hashes and refuses
packages containing game data. The setup contains the same engine, interface
resources and license notices as the standard ZIP.

Run the generated setup on Windows and select your existing **Arcanum 1.0.7.4**
folder. It checks for the required game archives, installs the mod, and creates
desktop and Start menu shortcuts. The game folder must be writable by your user
account. Close the game before installing or updating.

The original executable, game data, saves and personal settings are preserved.
The setup also adds an uninstaller to Windows's installed-apps list. Uninstalling
removes the mod's installed files and shortcuts while keeping game data, saves,
preferences and locally generated maps. No Python, NSIS or separate SDL
installation is needed to use the generated setup. The setup is unsigned.

### Create a complete Windows ZIP for local use

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

### Detailed local maps

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

Normal launches prepare this cache automatically using the engine itself. The
worker runs in an isolated folder under `modern-ui/.map-work/`, leaving saves
out of the rendering process. Missing or damaged tiles are repaired; changes to
the source assets trigger regeneration. Finished maps become usable individually.
If preparation fails or is skipped, incomplete maps use the original artwork.
The game folder must be writable.

For an unattended export, run the installed executable with the **absolute** game
folder path (the same option is available in all three builds):

```sh
./arcanum-modern --modern-generate-maps /absolute/path/to/Arcanum
```

On Windows the executable is `arcanum-modern.exe`; on macOS use the executable
inside the app's `Contents/MacOS/` directory. For diagnostics, progress is recorded
in `modern-ui/.map-work/progress`. The optional Python validator can check a cache:

```sh
python3 tools/verify-map-cache.py /absolute/path/to/Arcanum/modern-ui/local-maps
```

The older macOS developer exporter remains available through
`tools/build-map-exporter.py` and `tools/generate-map-cache.py`. Standard packages
include the native generator, not the generated images or commercial game assets.

See [the detailed map guide](docs/local-map-cache.md) for export options and
storage details. That guide is currently in French.

### GitHub releases

The [Release packages workflow](.github/workflows/release.yml) builds the Linux
x64 ZIP, Windows x64 ZIP, Windows setup and macOS Apple Silicon ZIP from a clean checkout. It
never uses `--include-game`, and none of these artifacts contain commercial
game files, saves, personal settings or generated maps.

Run it manually in **Actions → Release packages** to download build artifacts.
Pushing a version tag such as `v0.1.0` runs the same builds and creates a **draft
GitHub Release** with these files and `SHA256SUMS.txt`. The Windows setup must
pass an installation, upgrade and uninstall test on a disposable Windows runner
before the release draft is created. Review the draft before publishing it.
Rerunning the workflow can replace draft assets but will not alter a published
release. No release files are stored in Git; the optional full ZIP is for local
packaging only and is never uploaded by this workflow.

When a release has been published, Windows users can choose the setup to select
their installed game's folder and create shortcuts, or extract the standard ZIP
into that folder themselves. macOS users extract its ZIP beside their game
archives and open `Arcanum Modern.app`. Linux users extract its ZIP beside their
game archives and run `Play Arcanum Modern.sh`.

## Development and contributing

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
