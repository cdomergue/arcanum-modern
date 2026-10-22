# Arcanum Renaissance

An unofficial interface and gameplay mod for **Arcanum: Of Steamworks and
Magick Obscura**, based on
[Arcanum Community Edition](https://github.com/alexbatalov/arcanum-ce).

## Installation

You need your own **Arcanum 1.0.7.4** installation. Downloads contain the mod;
original game files are required separately. Close the game before installing
or updating, and use a writable game folder.

Download the package for your platform from
[Releases](https://github.com/cdomergue/arcanum-modern/releases/latest).

| Platform | Install and launch |
| --- | --- |
| Windows x64 | Run [the setup](https://github.com/cdomergue/arcanum-modern/releases/latest/download/ArcanumModern-windows-x64-setup.exe) and select your Arcanum folder. Launch from the desktop or Start menu shortcut. For the ZIP, extract into the game folder and run **Play Arcanum Renaissance.bat**. |
| macOS | Requires Apple Silicon and macOS 11 or later. Extract the ZIP into the game folder and open **Arcanum Modern.app**. |
| Linux x64 | Extract the ZIP into the game folder and run **Play Arcanum Renaissance.sh**, or enter `./"Play Arcanum Renaissance.sh"` in a terminal there. |

Installation preserves the original executable, saved games and personal settings.
Back up your saves before changing engines or installing game patches.

### Local maps

On first launch, Renaissance prepares detailed local maps from your game files.
Allow about **8 GB** for Arcanum and Vormantown, plus temporary working space.
Choose **Play now** to use the original maps; preparation resumes next launch.
Completed maps are reused.

If preparation fails, check `modern-ui/.map-work/generation.log` in the game
folder. **Play now** remains available.

### Unofficial Arcanum Patch (UAP)

- **French:** recognized original installations receive an optional startup
  prompt to install **UAP 2.0.2 Base + Race Mod**. It downloads the patch,
  prepares French text and backs up saves before activation. Existing campaigns
  may be incompatible. See [installation and restoration](docs/uap-fr.md).
- **English:** install **UAP 2.0.2** and choose its components before installing
  Renaissance, then start a new game. Race Mod, Beta Content, additional
  campaigns and optional data packs are supported.
  See [supported options and installation](docs/uap.md).

## Features

- Turn-based combat with direct companion control, switching between allies
  with consecutive turns, action previews and a scrollable combat log showing
  attack rolls, hit chances and outcomes. Combat uses Arcanum's rules.
- Redesigned HUD, character sheet, inventory, trading, dialogue, journal, maps,
  spell and skill panels, save/load menus and display options.
- World zoom, keyboard camera movement, remappable keys, and windowed,
  desktop fullscreen and exclusive fullscreen modes.
- Inventory search, categories, equipment comparison, item details, stack
  transfers and ten personal quick slots per controlled character.
- Integrated **AID descriptions in French and English** for the Arcanum
  campaign: item effects, strength requirements, quest and crafting tags,
  offerings and gifts. Supports existing saves and installations with AID
  already present. Magical properties remain hidden until identification.
- Independent companion positioning, regrouping and control outside combat.
- Three rotating autosaves before and after combat and before eligible area
  transitions, separate from manual saves.
- Journal search, quest filters, favorites, quest tracking and personal map notes.
- Zoomable local maps with exploration fog, localized labels and route planning.
- Books, letters, newspapers, inscriptions, telegrams and floating dialogue
  using the installed game's text, plus playback of its original cinematics.
- Optional unlimited companions; disabled by default. Recruitment conditions apply.

Game text follows the installed game's language. Additional interface text has
partial French and English support.

## Controls

Change bindings in **Options → Key bindings**. AZERTY and QWERTY presets are
available.

| Action | Default |
| --- | --- |
| Inventory / journal / character sheet | I / J / C |
| Map | G |
| Spells / skills / schematics | M / K / T |
| Rest | R |
| Stealth / pickpocket | H / P |
| Toggle combat mode | Shift + Space |
| End turn | E or Space |
| Move camera | WASD positions, ZQSD on AZERTY, or arrow keys |
| Zoom world or local map | Mouse wheel over the world or map |
| Use quick slot | 1–0 |
| Close panel or open pause menu | Escape |

- **Quick slots:** hover an item, spell or skill and press **1–0**, or drag it
  onto the bar. Assignments are saved separately for each controlled character.
- **Inventory:** Shift-click transfers a whole stack. **Transfer all** moves bag
  contents to or from a companion or container, leaving equipped or immovable items.
- **Companions:** outside combat, click a portrait to select and detach a
  companion. Use the portrait's **`=` / `/`** control to toggle grouping.
  Click the portrait again, **Player**, or right-click the world to return to
  your main character.
- **Combat:** select an available ally in the initiative bar to act. Allies
  with consecutive turns can switch while they have action points. **End turn**
  finishes the selected character; enemies act after the group finishes.
  Left-click highlighted scenery in combat mode to attack it.
- **Maps and journal:** drag the map to pan. Set a route and use the travel
  button to follow it; the cancel control clears it. In the journal, the star
  marks favorites and the pin tracks a quest.

## Preferences and saves

Paths are relative to the game folder.

| Path | Contents |
| --- | --- |
| `modern-ui/settings.ini` | Interface and gameplay preferences |
| `modern-ui/graphics.ini` | Display mode and resolution |
| `modern-ui/keys.ini` | Key bindings |
| `modern-ui/notebook-*.txt` | Favorites, tracked quests and map notes per character |
| `arcanum.cfg` | Engine and game settings |
| `modules/<module>/Save/` | Saved games |

Autosaves run when the game permits saving; dialogue, menus and some transitions
can defer or skip them. Notebook files are independent of saves: loading an
older save does not restore older notes. Keep both when moving an installation.

## Compatibility

- The game uses its original sprites and terrain artwork.
- Do not apply the original High Resolution Patch to the Renaissance executable.
- AID descriptions cover the Arcanum campaign and UAP. Item rebalance mods may
  make their reference statistics inaccurate. Other campaigns use their installed
  descriptions. Quest tags remain after quest completion.
- Intel macOS, mobile platforms, multiplayer and engine patches beyond the
  documented UAP options are not validated.
- F9 switches between the modern and original interfaces outside tactical combat.

## Video

[Watch the gameplay video](https://www.youtube.com/watch?v=v1U5BpJ3y8U).

## Screenshots

| Tactical combat | Inventory |
| --- | --- |
| [![Companion control and turn order](docs/screenshots/tactical-combat.jpg)](docs/screenshots/tactical-combat.jpg) | [![Inventory search and equipment details](docs/screenshots/inventory.jpg)](docs/screenshots/inventory.jpg) |

Screenshots use French game files.

## Build and contribute

See the [development guide](docs/development.md#build-from-source) for build
requirements, platform commands, packaging and tests. On Linux, `./build.sh`
compiles and installs the executable into `game/`. Supply your own game files
to run it.

## Credits and licenses

Arcanum was created by **Troika Games**. Renaissance is unofficial and includes
no original game data in its standard packages.

- **Arcanum Community Edition**, by **Alex Batalov**:
  [Sustainable Use License](https://github.com/alexbatalov/arcanum-ce/blob/a7ff41b300ef712f0e7d088183a3d08957110cdd/LICENSE.md).
- **Arcanum Improved Descriptions (AID)**, by **SCARaw and contributors**, with
  a French translation for Renaissance:
  [original mod](https://www.nexusmods.com/arcanumofsteamworksandmagickobscura/mods/13),
  [source](https://github.com/SCARaw/Arcanum-Item-Description-Project),
  [MIT license](resources/aid/LICENSE.txt).
- **Noto fonts**: [SIL Open Font License](game/modern-ui/FONT-LICENSE.txt).
- **FFmpeg**: LGPL 2.1 or later. Packages include its source, build script and
  license in `ArcanumModern/licenses/ffmpeg/`.

Packages include license notices for SDL, SDL_mixer, zlib, fpattern, stb and
other bundled dependencies.
