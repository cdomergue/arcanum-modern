# Arcanum Modern

Arcanum Modern is a native engine and interface project for **Arcanum: Of Steamworks
and Magick Obscura**, built on [Arcanum Community Edition](https://github.com/alexbatalov/arcanum-ce).
It keeps the original game's wood, brass, parchment and artwork while making its
interface readable on modern displays.

The interface renders separately from the world: text and controls stay sharp
and keep their size when you zoom. Tactical combat adds direct companion control
while retaining Arcanum's initiative, action points, fatigue, damage and spell rules.
This is an experimental playable version, not an official remaster.

## What is included

- A modern HUD, party portraits, character sheet, inventory, trading, maps,
  spell and skill panels, menus, save/load screens and display settings.
- A character sheet organized around the original attributes and disciplines,
  including moral alignment and technological/magical aptitude.
- Clear dialogue and floating text, plus readable books, letters, newspapers,
  inscriptions and telegrams using the installed game's text.
- Mouse-wheel world zoom, keyboard camera movement and remappable controls.
- Inventory search and categories, equipment comparison, item descriptions,
  transfer shortcuts and editable quick slots with hover descriptions.
- A searchable journal with quest status filters, favorites, quest tracking
  and personal map annotations.
- Turn-based combat with direct control of eligible companions on their own
  turns, action previews, available spells and a combat log.
- Maintained spell icons under the top bar: hover for the name and click to stop
  an effect when its caster is available.
- An optional unlimited-companion setting, off by default. Recruitment
  requirements still apply.

Original game data is required and is not distributed with this project.
Names, descriptions and most labels come from the installed game. Additional
interface text has partial French and English support; Western game text is
read as Windows-1252. Other languages and encodings need validation.

## Install and play

Use your own Arcanum installation updated to **1.0.7.4**. Back up your saves before
trying this engine. A separate copy of the game is recommended for testing.

### Windows

The intended target is **Windows 10/11, x64**.

1. Extract `ArcanumModern-windows-x64.zip` into your Arcanum game folder, alongside
   `Arcanum.exe` and `arcanum1.dat`.
2. Open **Play Arcanum Modern.bat**.
3. Set display mode, resolution, audio and controls in **Options**.

Players do not need Python, a compiler or a separate SDL installation. The ZIP
contains `arcanum-modern.exe`, the `modern-ui/` resources, a launcher and project
notices. It does not replace `Arcanum.exe` or include game archives, saved games,
or anyone else's preferences.

Close the game before extracting an update. Existing preferences and notebook
files are not part of the ZIP and are retained. Use a writable game folder so the
game can save progress and settings.

### Linux

Place your game files in `game/`, build the engine, then run:

```sh
./build.sh
./play.sh
```

For a windowed session:

```sh
./play.sh -window -geometry=1280x720
```

The Linux launcher selects Vulkan by default and writes `game/modern-launch.log`.
It can use another SDL renderer through `SDL_RENDER_DRIVER`. The current Linux
build uses X11/XWayland. Display settings are stored separately from world zoom;
fullscreen mode uses the monitor's output resolution.

## Controls

These are the defaults for a fresh installation. Change them in **Options → Key
bindings**; AZERTY and QWERTY presets set the camera keys for those layouts.

| Action | Default |
| --- | --- |
| Inventory | I |
| Journal | J |
| Character sheet | C |
| Map | G |
| Spells / skills / schematics | M / K / T |
| Rest | R |
| Stealth / pickpocket | H / P |
| End turn | E or Space |
| Move camera | WASD positions (ZQSD on AZERTY), or arrow keys |
| Zoom the world | Mouse wheel over the world |
| Use a quick slot | 1–0 |
| Close a panel or open pause menu | Escape |

Hover an item, spell or skill in its panel and press **1–0** to assign it to a
quick slot. Hover a quick slot to identify its contents. Text entry suspends game
shortcuts; in inventory search, hovering an item gives quick-slot assignment
priority without clearing the search.

Double-click an item to transfer it between inventory panels where the game's
rules allow it. **Shift-click** transfers a whole stack. Purchases still use the
original prices and restrictions. Drag the map background to pan it.

In the journal, the **star** toggles a favorite and the **pin** toggles quest
tracking. Filters distinguish **In progress**, **Completed**, **Interrupted** and
**Mentioned** quests, alongside **All**. An achieved objective awaiting a final
turn-in stays in progress; a quest completed by a rival is grouped with interrupted
quests. Search and favorites can be combined with these filters.

A detailed French control guide is available in [docs/controls.md](docs/controls.md).

## Tactical combat

Turn-based combat is the default for new installations and new games. Each
controllable ally receives orders during their own turn. Selecting another
portrait moves the camera; it does not transfer the turn or refill action points.
Allies do not share or interleave turns.

Action previews show range, hit chances and action-point estimates. A `~` marks
an estimated movement cost: the original engine determines the final cost from
movement and animation. Spell availability, fatigue costs, fear, incapacitation
and loss of control continue to follow Arcanum's rules.

## Preferences and personal notes

Files below are relative to the game folder (`game/` in the Linux development
layout):

- `modern-ui/settings.ini`: interface and gameplay preferences.
- `modern-ui/graphics.ini`: display mode and resolution.
- `modern-ui/keys.ini`: key bindings.
- `modern-ui/notebook-*.txt`: favorites, tracked quests and map notes per character.

Notebook annotations are separate from saved games. Loading an older save does
not roll those annotations back. Keep these files when moving your installation.
Display changes offer a timed confirmation and revert if not confirmed.

## Build and package

The pinned upstream engine revision is
`a7ff41b300ef712f0e7d088183a3d08957110cdd`. `prepare-source.sh` exports that revision
and applies `patches/engine.patch`; dependencies are fetched from their pinned
upstream repositories. Internet access is needed for an uncached build.

### Linux build

Use a C17 compiler, Git, CMake, a build tool and the X11/audio development packages
required by SDL. On Fedora/Nobara these include `gcc`, `gcc-c++`, `cmake`,
`ninja-build`, `libX11-devel`, `libXext-devel`, `libXrandr-devel`, `libXi-devel`,
`libXinerama-devel`, `libXcursor-devel`, `alsa-lib-devel`, `pulseaudio-libs-devel`,
`libXScrnSaver-devel` and `libXtst-devel`.

`./build.sh` builds and atomically installs `game/arcanum-modern`. Overrides:
`ARCANUM_BUILD_DIR`, `ARCANUM_BUILD_SDK`, `ARCANUM_DEPENDENCY_CACHE` and
`ARCANUM_ENGINE_SOURCE`. The latter points to a checkout containing the pinned
engine revision. The default SDK/cache paths can reuse a local development setup;
otherwise the system toolchain and dependency downloads are used.

### Windows ZIP from Linux

Install CMake, Ninja, Python 3 and Git. Extract an official Linux
[LLVM-MinGW UCRT toolchain](https://github.com/mstorsjo/llvm-mingw/releases), then:

```sh
export LLVM_MINGW=/path/to/llvm-mingw
./build-windows.sh
```

The result is `dist/ArcanumModern-windows-x64.zip`. To choose another destination:

```sh
./build-windows.sh /path/to/ArcanumModern-windows-x64.zip
```

The script builds the Windows executable with static dependencies and packages
only an explicit resource list. It checks the executable architecture and imported
DLLs, includes license notices, and writes a manifest with file hashes and the
source revision. Game data, preferences and saves are excluded.

Overrides: `CMAKE`, `NINJA`, `PYTHON`, `ARCANUM_WINDOWS_BUILD_DIR`,
`ARCANUM_BUILD_JOBS`, `ARCANUM_DEPENDENCY_CACHE`, `ARCANUM_ENGINE_SOURCE`.

### Windows ZIP from Windows

Install Visual Studio 2022 with the C++ desktop tools, CMake, Git for Windows and
Python 3. From **Git Bash**, run:

```sh
PYTHON=python ./build-windows.sh
```

This selects the Visual Studio x64 generator and static C runtime. This native
build route needs Windows validation; the Linux cross-build is the development
packaging route.

## Development

Only the mod's code, resources, scripts, tests and integration patch belong in
Git. Original game files, generated engine sources, binaries, preferences and
saves are excluded. New interface code lives in `source/src/ui/modern_*`.

After changing an upstream engine file, regenerate the integration patch before
building:

```sh
ARCANUM_ENGINE_SOURCE=/path/to/arcanum-ce python3 tools/update-engine-patch.py
```

Focused tests are in `tests/` and use Python 3 and a C compiler. For example:

```sh
python3 tests/test_keyboard_capture.py
python3 tests/test_quest_filters.py
python3 tests/test_maintained_spells.py
python3 tests/test_qol_preferences.py
python3 tests/test_windows_package.py dist/ArcanumModern-windows-x64.zip
```

Use an isolated game copy for interactive testing. Package checks do not replace
playing the Windows build on actual Windows hardware.

## Current limitations

- World sprites and terrain retain their original resolution. This project
  modernizes presentation and interaction rather than replacing every game asset.
- The native 64-bit build cannot load Arcanum's original 32-bit `binkw32.dll`.
  Bink cinematics are currently unavailable without a compatible decoder.
- Multiplayer, mobile use and combinations with other engine/binary patches
  have not been validated. Do not apply the original High Resolution Patch to
  the modern executable.
- Some original presentation paths remain as fallbacks. F9 switches presentation
  outside tactical combat and can be useful for comparisons.

## Credits and notices

Arcanum was created by Troika Games. This is an unofficial project and requires
separately obtained game data.

The engine is a modified version of **Arcanum Community Edition**, by Alex Batalov,
under its [Sustainable Use License](https://github.com/alexbatalov/arcanum-ce/blob/a7ff41b300ef712f0e7d088183a3d08957110cdd/LICENSE.md).
The Windows package includes the upstream license and third-party notices.
Noto fonts use the [SIL Open Font License](game/modern-ui/FONT-LICENSE.txt);
`stb_truetype` carries its license in its header. SDL, SDL_mixer, zlib and fpattern
retain their respective notices. Original game data is not covered by these
software licenses and is not included in release packages.
