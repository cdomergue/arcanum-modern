# Player houses

Edwin Harrow, in the office inside Tarant Town Hall, sells the Woodland House
for 10,000 coins. Asking about the property or speaking to its guard reveals its
world-map marker. The furnished house stands on the main map in sector
1346, 1024; the former owner's tame wolf stays there.

Before purchase, doors, windows and storage are locked. Buying the house gives
the party its key and enables the beds. The agent replaces a lost key free of
charge. Storage uses the normal save system. English and French dialogue are
included; English is the fallback and the integrated French UAP selects French
automatically. Set `ARCANUM_HOUSE_LANGUAGE=en` or `fr` to override the selection.

The same catalogue and runtime can handle other houses. Each property supplies
its own scene, price, sector, key, persistent state and localized description.
A guard is optional and can be any NPC prototype. The house's name, location,
architecture and inhabitants belong in content, not in the exporter or engine.

## Creating a house

You need an Arcanum installation, WorldED with `worlded-house-export`, Python 3,
and a Renaissance executable for playtesting. See [Building WorldED](worlded-build.md)
and the [editor guide](worlded.md) if you are starting from source.

1. Open Arcanum's main map in WorldED and choose a sector. The current housing
   runtime supports one property per sector, on `arcanum1-024-fixed`. It does not
   support a separate interior map or a house spanning several sectors.
2. Save the project outside the game installation. Keep the source installation
   unchanged: the exporter compares your project against this baseline.
3. Insert a room or build from the object library. Add floors, walls, a doorway,
   roof pieces and storage. Keep the doorway and its approaches clear. Windows
   are portals too; check them before and after purchase.
4. Add furniture and any inhabitants. Create new instances in the project.
   To change an existing campaign object for housing, duplicate it and remove
   the original. Housing must not take over an existing campaign object's GUID.
5. Add object scripts or dialogues if needed, then save. Tile and sector scripts
   are not supported by the housing exporter. The estate runtime controls the
   guard's dialogue event and bed use event; use other objects for custom events.
6. Test the layout in a WorldED copy with an unvisited sector. Walk through the
   doorway in both directions with every companion, including a dog or another
   companion that cannot jump through windows.

Keep a clean copy of the authored project. Once a house has shipped, editing
its existing object GUIDs does not migrate objects already stored in saves.
Test updates against an old save as well as a fresh discovery.

### Clear trees and plants

Deleting baseline scenery produces removal records during export. A saved
sector can also contain vegetation variants that do not match that baseline.
For areas that must stay clear, create `vegetation.mes` in the project root.
It uses local ground-cell coordinates:

```text
{1300}{62}
{1301}{62}
```

The entry ID is `localY * 64 + localX`; these examples clear cells (20, 20) and
(21, 20). The value is a scenery-type mask: 30 for trees, 32 for plants, or 62
for both. Add one entry for each cell to clear. Apply it only where you intend
to remove vegetation: it also hides vegetation you placed yourself on those
cells. Furniture and inventory items are unaffected. The runtime disables the
hidden vegetation's shadows as well.

### Export the saved project

The exporter is built with WorldED's GUI option. It reads the map and sector
from `building.json`; no house coordinates are compiled into it. Run it against
the same installation used to author the project:

```powershell
& ./build/worlded-windows/Release/worlded-house-export.exe `
  'D:/Games/Arcanum' 'D:/WorldEDProjects/riverside-house' `
  'D:/WorldEDExports/riverside-house'
```

On Linux:

```sh
build/worlded-linux/worlded-house-export \
  /path/to/Arcanum /path/to/riverside-project /path/to/new-riverside-export
```

The output directory must not already exist. Use a new directory for each
export and review the differences before replacing a published scene.

To give an NPC the estate guard dialogue, append `--guard GUID`. Obtain the
persistent instance ID from the saved object's data: `worlded decode FILE.mob`
or `worlded decode FILE.sec` prints object identities. Use the 36-character
instance ID beginning with `0200`, not the numeric prototype ID. You can repeat
`--guard` for multiple NPCs. Omitting it gives no NPC the guard role; creature
species and prototype numbers do not imply a role.

The exporter writes only new objects and terrain/roof differences, plus records
of removed baseline scenery. It rejects reused campaign GUIDs, missing guard
IDs, guards that are not NPCs, objects outside the sector and tile/sector
scripts. Existing object scripts are retained in the exported instances.
Project-owned files from `exports/scr`, `exports/dlg` and `exports/proto` are
copied under the scene's `resources` folder.

Review this output, then copy the whole exported directory to a new folder such
as `resources/house/riverside_house` in the repository. Do not install the raw
WorldED SEC file over the campaign sector; the housing pack is applied additively
so existing save differences keep referring to the correct original objects.

## Add the house to the seller

Edit [resources/house/catalog.json](../resources/house/catalog.json). Append a
property to `properties`; keep existing entries. There is no graphical catalogue
editor. The example below shows the shape of a second listing. Its sector and
IDs are illustrative: reserve values for your content after checking the
campaign and other installed mods, and use the sector recorded in your export.

```json
{
  "id": "riverside_house",
  "slot": 1,
  "price": 12000,
  "standing": "modest",
  "ownership_global": 1800,
  "discovery_global": 1801,
  "key_id": 62000,
  "sector": [1200, 1150],
  "scene": "riverside_house",
  "text": {
    "en": {
      "name": "Riverside House",
      "standing": "Modest",
      "description": "A small furnished house beside the river.",
      "disclosure": "The purchase includes the furniture and storage chests.",
      "key": "Riverside House key"
    },
    "fr": {
      "name": "Maison de la rivière",
      "standing": "Modeste",
      "description": "Une petite maison meublée au bord de la rivière.",
      "disclosure": "Le mobilier et les coffres sont compris dans la vente.",
      "key": "Clé de la maison de la rivière"
    }
  }
}
```

Keep the JSON in UTF-8; the
builder converts game dialogue to Windows-1252 and rejects unrepresentable
characters, braces and line breaks in dialogue fields.

| Field | Meaning and constraints |
| --- | --- |
| `id` | Unique author-facing property name; keep it stable |
| `slot` | Unique runtime identity, 0–99; listings are sorted by slot |
| `price` | Positive integer coin price, up to 2,147,483,647 |
| `standing` | Author-facing category; displayed wording comes from `text` |
| `ownership_global` | Unused global variable recording purchase, 0–1999 |
| `discovery_global` | A different unused global recording discovery, 0–1999 |
| `key_id` | Unique key identifier and localized description ID |
| `sector` | `[sectorX, sectorY]`, each 0–1999, matching `scene.mes` |
| `scene` | Folder under `resources/house`; lowercase letters, digits, `_` or `-`, starting with a letter, at most 63 characters |
| `text.en`, `text.fr` | Name, standing, description, disclosure and key label for each language |

Never reuse or renumber a shipped slot, ownership/discovery global, key ID or
object GUID. Reordering the JSON array is safe; changing those identities can
lose ownership or duplicate content in existing saves. Globals must be distinct
across both ownership and discovery fields. Each property needs its own sector
because guards and beds resolve ownership from their sector.

The supplied Woodland House uses slot 0, globals 1988 and 1989, and key 61003.
Shared estate resources reserve scripts 48300 (guard), 48301 (agent) and 48304
(bed), seller description 61002 and unidentified description 61005. Map labels
use description IDs `70000 + slot`. Leave these shared protocol IDs unchanged
unless you also update the native module. They are separate from any property's
content. The builder checks catalogue conflicts, but cannot prove that an ID
is unused by every installed campaign script or mod.

The catalogue's `seller` object supplies the agent's name, localized directions,
and tile position, offsets and rotation. Its `script_id` and `description_id`
record the shared protocol IDs above; they are not arbitrary per-house settings.
A new listing needs no new seller script or NPC. The builder creates three
listings per page, leaving room for navigation within the five-response limit.

## Generate and install the content pack

From the repository root:

```sh
python tools/house/build-content.py --output build/house-content
python tests/test_house_catalog.py
```

Use `python3` on Linux if `python` is unavailable. For a separate content tree:

```sh
python tools/house/build-content.py --catalog /path/to/catalog.json \
  --scenes /path/to/scene-folders --output /path/to/new-content-pack
```

`--scenes` is the parent of the scene folders, not a particular scene. Its
default stays `resources/house` even when you pass a different catalogue.
Use a fresh output directory when removing or renaming content, so files left
by a previous generation do not remain in the package.

The builder verifies required scene files, matching sectors, object identities,
NPC guard roles and shared resource conflicts before writing the pack. A scene's
`resources/scr`, `resources/dlg` and `resources/proto` files are installed at the
base resource root. Filenames need numeric IDs. Conflicting definitions with
the same resource type and ID are rejected, as are replacements for the estate
service scripts and dialogues. Keep IDs unique across your installed mods too.

The result has three folders:

```text
house-content/
  base/house/       catalogue, seller position and scene folders
  base/scr/        estate entry points and project scripts
  base/dlg/        project dialogues, when present
  base/proto/      project prototypes, when present
  en/              English dialogue and descriptions
  fr/              French dialogue and descriptions
```

Copy the **contents** of `house-content` into `ArcanumModern/house` in a disposable
Renaissance installation. The resulting path must be
`ArcanumModern/house/base/house/catalog.mes`, with `en` and `fr` beside `base`.
Close the test game before replacing resources. Keep the source installation
and saves untouched.

The regular `build.sh` generates this content for the repository's `game/`
installation, and the Windows, Linux and macOS packaging scripts include it.
The native transaction lives in `modern_house.c`. The small generated SCR files
are entry points to that code; editing them does not change the purchase logic.

## How the runtime applies a house

Each object is added once by persistent GUID. The original sector's static
object order stays intact, preserving saved DIF references. Re-entering the
sector does not regenerate storage. `removed.mes` hides matching original
scenery by location, art and offsets. `vegetation.mes` additionally handles
saved vegetation variants on specified cells.

All exported portals and containers get the property's key and purchase
protection. This includes windows. Before ownership, the runtime protects them
against ordinary access, unlocking and destruction. After purchase they use
normal key operation. Design and test with this rule in mind: a public unlocked
container should not be included as property storage.

The `guard` role attaches the estate dialogue until purchase, then clears that
dialogue event so ordinary greetings can resume. Beds (scenery kind 22) receive
the estate bed-use event. New property NPCs are made non-hostile and invulnerable
by the housing runtime. A house requiring hostile encounters needs additional
runtime design; changing its catalogue description is not enough.

A purchase rechecks funds and ownership, issues the key, then charges the player.
If key delivery fails, the player is not charged. Repeat purchases do not charge
again; replacement keys are free. Discovery through the listing or guard sets
the property's map marker.

For contributors working on the native interface, commands for a slot start at
`100 + slot * 10`: show, buy and replace key occupy offsets 0, 1 and 2.
Property dialogue blocks start at `3000 + slot * 100`.

### Scene file reference

| File | Records |
| --- | --- |
| `scene.mes` | Header `{0}{1 sectorX sectorY}`; entries 100 and above contain `role GUID` |
| `objects/GUID.mob` | Native version 119 object instance with the matching persistent GUID |
| `terrain.mes` | Changed ground cells: key `y * 64 + x`, value unsigned decimal tile art ID |
| `roofs.mes` | Changed roof cells: key `y * 16 + x`, value unsigned decimal roof art ID |
| `removed.mes` | Packed location, current art ID, signed pixel offset X and Y for removed baseline scenery |
| `vegetation.mes` | Optional local-cell clearance masks, described above |
| `resources/` | Optional project SCR, DLG and PRO files |

The first four MES files are required even when a difference file is empty.
Object GUIDs are 36 hexadecimal characters beginning with `0200` and must be
unique across all properties. The exporter uses roles `portal`, `chest`,
`object` and explicit `guard`; lock behavior also follows the native object type.
Sector IDs in WorldED filenames are `sectorX | (sectorY << 26)`. Global ground
coordinates are `sectorX * 64 + localX` and `sectorY * 64 + localY`.

## Test before distribution

Use copies of saves for both first discovery and a sector visited before the
house pack was installed. The WorldED test-copy dialog covers the unvisited
case; install the additive housing pack into another disposable game copy for
the visited case. Do not overwrite a player's save to make the test pass.

For each new house, check:

- The seller lists the right name, price and disclosure in English and French.
  Discovery shows the right map marker. The seller remains available day and night.
- Insufficient funds, the exact price and surplus funds behave correctly.
  Repeating a purchase after save/load does not charge twice. Losing the key
  permits a free replacement.
- Doors, windows and chests enforce ownership before purchase, including
  lockpicking, magic and damage attempts. Beds become usable after purchase.
- Every companion crosses the open doorway in both directions without using
  windows. Include a companion that cannot jump. Repeat after loading a save.
- Trees, plants and their shadows are absent from cleared cells on first entry
  and in a previously visited sector. Furniture and dropped items remain.
- Storage preserves stacked, unique and enchanted items after travel, several
  days, save/load and a full game restart. No objects or occupants duplicate.

The native `tools/house/check-house.c` is a regression fixture for the supplied
Woodland House. Its fixed coordinates and expected contents are test data;
it is not a general validator for a new catalogue property. The exporter tests
use multiple sectors and an unrelated NPC prototype to check generic behavior.

On Linux, build the existing-house regression against a built Renaissance engine:

```sh
python3 tools/house/build-check.py --build-dir build
build/house/check-house /path/to/copy/game Slot0001
```

The copy must contain `.renaissance-house-test`. The check writes slots 0096,
0097 and 0098 in that copy and exercises purchases, storage and save/load.
`--check-door` uses slot 0094 and requires a copied party at the house;
`--check-shadows` uses slot 0093. On Windows, build the `check-house` target in
the **engine** CMake build; Visual Studio puts it in `Release/check-house.exe`.
These regressions cover the shipped fixture. Record the manual results for your
new house separately, including the engine version, language, platform and
whether the sector had already been visited.
