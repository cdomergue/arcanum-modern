# Using WorldED Renaissance

For a complete offline tutorial, open [worlded-tutorial.html](../worlded-tutorial.html).

WorldED edits Arcanum sectors, objects, scripts and dialogues in a separate
project. It reads art and other resources from your own game installation.
The interface is in English; names taken from the game keep the game's language.

To build the editor, start with [Building WorldED](worlded-build.md).
To sell a finished house through the estate agent, follow
[Creating and selling houses](housing.md#creating-a-house).
For world terrain, map connections and campaign tables, see
[Campaign projects](worlded-campaign.md).
To distribute changes to existing saves, see
[Exporting world updates](worlded-updates.md).
For the remaining differences from the original editor, see
[feature coverage](worlded-coverage.md).

## Open the world and choose a site

Launch `worlded-explorer`, then select your Arcanum installation. You can also
open the world directly from a terminal:

```sh
worlded-explorer --game "/path/to/Arcanum" --world
worlded-explorer --game "/path/to/Arcanum" --world-sector 1200,1150
```

On Windows, use `worlded-explorer.exe` and a Windows path. Select the game folder
containing the archives and `modules`, not a folder containing only the
Renaissance executable.

Click a sector on the world map. WorldED reads its saved SEC file or generates
it from the campaign's terrain template, as the game does. Each sector contains
64 by 64 ground cells. The neighboring-sector buttons let you inspect nearby
land. Named landmarks come from the installation.

Keep a house inside one sector, away from its edges. Leave room for its roof
and the party's approach. The preview does not calculate the game's paths:
a door that looks clear still needs a test with the player and companions.

Use Ctrl+S to save the zone to a project folder **outside the game installation**.
For example, keep the game in `D:\Games\Arcanum` and the project in
`D:\WorldEDProjects\riverside-house`. WorldED refuses exports into the source
installation. Keep the installation used as a baseline unchanged while working.

Open the saved project with:

```sh
worlded-explorer --project "/path/to/riverside-house"
```

`worlded.local.json` remembers the installation on this computer. Leave it out
when sharing a project. If the game moves, choose its new location when prompted,
or use **File > Change project installation…**. Supplying both `--game` and
`--project` selects the installation explicitly.

## Build and furnish a room

Open an editable sector, paint its floor, then drag walls, doors and furniture
from the object library into the scene. Select unwanted scenery and delete it
before building over it. Leave the doorway clear and check the entrance in game
with companions as well as the player.

The wall library includes the
structures, faces and variants in your installation. Selecting a wall in the
scene locates its library entry.

Select and drag objects to move them. Ctrl+D duplicates the selection, Delete
removes it, and Ctrl+Z / Ctrl+Shift+Z undo or redo. Double-click a placed object
to configure its position, appearance, properties and script events. Applying
the dialog is undoable; Ctrl+S writes the changes to the project.

Open a second sector as a side-by-side reference to copy walls, portals,
containers and scenery into the active zone. Copied construction objects receive
new identities. Quest scripts and chest contents are cleared when copying from
the reference, so inspect the result before using it. Ground can be copied with
the structure. Each open zone has its own history and unsaved changes.

The standalone **New zone** workflow is useful for experiments. A property sold
by the estate agent must be authored on an existing sector of Arcanum's main map.
The [housing exporter](housing.md#export-the-saved-project) checks this.

## Paint ground and roofs

The **Mode** selector changes what you select: **Objects**, **Ground**, **Roofs**,
**Characters**, **Scripts** or **Blocking**. In Ground or Roofs mode, drag to select cells.
Ctrl+C and Ctrl+V also work from the reference view. Right-click a cell and use
**Sample this cell** to find its material in the **Ground / Roofs** library.

Choose a material, then **Apply to selection**, or enable **Paint** and drag.
Brush size controls the painted width. A completed stroke is one undo step;
Escape cancels a stroke still in progress. **Grid** shows the ground cells.

**Automatic transitions** joins outdoor materials using the installed tile
rules, including shores and intermediate water depths. Indoor floors use exact
placement. Turn transitions off when you want a specific outdoor tile as well.
A stroke is refused when graphics are missing, materials cannot connect or a
transition would change the sector boundary. Edit neighboring sectors separately.
Entries labelled **Exact tile** include the catalogue's individual transition
pieces. Selecting one disables automatic transitions so you can paint its exact
shape. Search by its ART name to find a particular piece.
Rugs and other existing facades render as their original multi-cell images.
Sampling one of their cells selects its exact piece and disables automatic
terrain transitions. Use the **Facades** library to place a complete assembly;
see [Place a facade or rug](#place-a-facade-or-rug).
The [terrain reference](../tools/worlded/TERRAIN.md) explains the format rules.

Each roof piece covers four by four ground cells, giving a 16 by 16 roof grid.
Paint or copy roof pieces, and use Delete to remove them. Assemble the outline
manually. **Show roofs** controls the preview in the other modes. WorldED does
not simulate the engine's roof occlusion or dynamic lighting; check both in game.

## Edit blocking and sector properties

Choose **Blocking** mode to see blocked cells. Left-click a cell to toggle its
manual blocker; click again to remove it. Bright red hatching marks manual
blocking. Dark red marks blocking from the terrain or object flags, which stays
until you change the material or blocking object. Walls and portals have their
own collision rules. This overlay is a diagnostic, not a pathfinding simulation.
Undo/redo and saving the zone include manual blocking changes.

Right-click the scene and choose **Sector properties…**, or choose **Zone > Sector properties…**.
Select independent music and ambient schemes, a lighting scheme and a
magic/technology adjustment. The lists use names from your installation and
preserve an existing ID even if its definition is missing. Save applies one
undoable edit; Cancel leaves the sector untouched. Check sound playback and
lighting in an isolated game copy.

Full-sector projects and playtests include these changes. Housing exports also
include changed blockers and sector properties as additive resources. A sector's
lighting, music and ambient scheme affect the whole sector, including the area
outside the house.

## Place lights

Choose **Zone > Lights…** to add, edit or remove independent lights in the
sector. Set the cell, pixel offset, shape, color and initial animation frame.
Directional lights have 32 directions; ordinary lights have eight. The editor
checks that the selected art and frame exist before accepting a change.

Use **Dark light / shadow** for a darkening light and **Animate** for a moving
light texture. **Indoors** and **Outdoors** use the sector's lighting schedule;
leave both unchecked for a fixed custom color. **Disabled** keeps the record
without illuminating the scene.

**View > Light preview** shows light positions and an approximate colored halo.
Check occlusion, animation and the final lighting in the game. The preview does
not evaluate the engine's shadow or time-of-day calculations.

Housing exports new lights as invisible, nonblocking objects with persistent
identities. Removing an original light disables it without changing the order
of the sector's light records, which existing save differences rely on.

## Add characters and effects

The Characters view shows campaign NPCs at their initial positions. These are
a read-only overlay, not the occupants of your current save. To add a character
to the project, drag a character or creature from the library into the scene.

Double-click it or choose **Configure character…** to edit its facing,
description and portrait IDs, stats, skills, training, resistances, AI, faction,
reaction, inventory source and script events. Untouched values stay inherited
from the prototype. For individual items and equipment, see
[Inventories and equipment](worlded-inventory.md).

In **Daily routine**, enter separate day and night positions using world tile
coordinates. Day runs from 06:00 to 21:00. Add waypoints in the order the NPC
should visit them, then enable **Day waypoints** or **Night waypoints** in
**Behavior**. The patrol repeats after the last row. **Move up** and **Move down**
change the route order; editing a route resets its starting waypoint to the first
row. Check that the NPC can reach every stop in the test copy.

Routes may cross sectors on the same map. There is one coordinate limitation:
from another sector, you cannot author a destination in world sector 0,0. Its
coordinates overlap the project's local position format, so WorldED rejects
that destination instead of moving it somewhere else.

**Spells and technology** sets the NPC's knowledge. A college level from 0 to 5
grants that many spells in the college; technology degrees range from 0 to 7.
The mastered college uses its native index, 0–15, or None. Editing these values
does not spend character points. College and discipline names come from the
installed game's text.

The **Generator** tab turns an NPC into an invisible template that spawns copies.
Reserve an unused campaign generator ID, 0–255: two generators with the same ID
share counters. Set the maximum alive at once, the total limit and the spawn
interval. Enable daytime, nighttime or both. A zero alive limit, or no enabled
time of day, produces no spawns. **Ignore the total limit** allows continued
spawning while still respecting the alive limit. Editing these settings preserves
the stored number already spawned. Check generated creatures in game before
publishing a house that uses them.

The **Effects** tab also applies to scenery. Use **Additive blending — fire, light**
for a flame whose black background should disappear in game. Foreground and
background Eye Candy effects can be set independently.

WorldED remembers the last installation it opened successfully and reloads it
on the next launch. Opening a project or passing `--game` takes priority. If the
folder has moved or its drive is disconnected, choose **File > Installation >
Open installation…**. Opening an archive does not replace the remembered game
folder. This preference stays on your computer, outside shared projects.

## Place a facade or rug

Open an editable zone and select **Facades** in the left library. Search by
name, choose the orientation supplied by the game (flipped variants are listed
separately), then click **Place facade** or double-click its preview. Move the
preview over the scene and click to place the whole assembly. Escape cancels.

WorldED reads the native FacWalk footprint, frame order and walkability for
each piece. Empty cells in the footprint keep their existing ground. A facade
must fit inside the active sector; an out-of-bounds click makes no changes.
Undo/redo treats the placement as one edit. The resulting ground cells are
saved with the project and included in housing terrain exports.

## Menus and shortcuts

**File** opens, saves and exports zone projects. Its **Installation** submenu
opens a game installation or changes the installation linked to a project.
**Edit** contains undo/redo and selection commands. **Zone** opens local sectors
and edits their environment, map marker and sector script.

**Resources** contains the script/dialogue library, creation commands and archive
browser. **View** switches between the world and scene, controls side-by-side
viewing, framing, zoom, the grid and roofs. **Test** creates an isolated playable
copy or resumes an existing one.

The top toolbar keeps common project, view and test commands at hand. The second
row holds the editing mode and selection commands. Right-click a ground cell
for its tile script and other commands tied to that position.

## Write scripts and dialogues

Open an editable zone, then use **Resources > New script…** or **New dialogue…**.
Choose an unused resource ID. **Resources > Scripts & dialogues…** also opens existing
resources; project resources are marked `[project]`.

Use **Validate**, then **Apply to project**. You can attach the resource to the
selected object's event in the same dialog. Save the zone afterwards. The
Scripts view lists object, tile and sector attachments. Double-click an entry
to inspect it, or use **Locate** for an attachment with a position.
**Remove attachment** detaches a script without deleting the shared resource.

To attach a script to the ground, right-click a cell and choose **Tile script…**.
Use **Sector script…** for the whole sector. Choose an installed or project SCR,
then save the attachment. In Scripts view, **Edit attachment…** changes an
existing tile or sector attachment. Both workflows support undo/redo.

The attachment stores flags and four counters, each from 0 to 255. A new
attachment takes its defaults from the chosen SCR header. **Load defaults from
SCR** resets those controls explicitly; editing an existing attachment keeps
its current values. Flags accept decimal or `0x` hexadecimal values. These are
initial values for a new game instance, not edits to a running save.

For an object or NPC, open its configuration dialog and select **Scripts**.
Each event has a script ID, flags and four counters. Setting the script ID to 0
disables the event while retaining its flags and counters. To use a script's
initial values, select the event and click **Load selected attachment defaults
from SCR**. Changing its ID alone preserves the current values. Apply the object
and save the project when finished.

For a conversation, the usual NPC event is `SAP_DIALOG`; an interactive object
usually uses `SAP_USE`. A new dialogue gets a companion script when none exists.
If you reuse an existing script, add the dialogue call to its intended path:

```text
dialog(1);
return;
```

New scripts use structured source, with Ctrl+Space completion and line
diagnostics. For example:

```text
let total = 0;
for (let i = 0; i < 5; i++) {
    if (i == 2) {
        continue;
    }
    total += i;
}
return;
```

The language supports `if/else`, `while`, `for`, `break`, `continue`, labels,
`goto`, comparisons, Boolean operators, addition, subtraction and multiplication.
Native actions and conditions accept their full `SAT_…` / `SCT_…` name or its
lowercase suffix. Completion lists the available calls. `player`, `self`,
`triggerer`, `current` and `extra` refer to the script's object context.
`global(n)`, `flag(n)` and `counter(n)` access persistent engine values; reserve
their indices before using them in distributed content.

`return;` skips the default behavior; `return default;` preserves it. The engine
allows ten numeric locals, including compiler temporaries, and 1,000 executed
instructions per invocation. Keep loops short. Division is rejected because
the pinned runtime's division opcode gives incorrect results.

Existing SCR files open in the native instruction editor. They are not
converted to structured source. Dialogue conditions and effects also use native
Arcanum syntax. Text must fit Windows-1252, and the game supports at most five
player responses at a time. Negative dialogue targets refer to SCR instruction
numbers: check them again after recompiling. **Preview conversation** follows
text and links; it does not execute conditions or effects.

Source is saved in `script-sources/*.wscript`, compiled scripts in `exports/scr/`,
and dialogues in `exports/dlg/`. Keep the source with your project so another
author can continue editing it. Validation checks the format, not whether a
quest behaves as intended.

## Test in a separate game copy

Use **Test in a copy…** and choose a named save, a compatible Renaissance engine
executable, a new output folder and the game's language. The executable must
support `--worlded-test`; an original Arcanum executable or a shell launcher
will be rejected. Configure the marker with **Map location…** if needed.

WorldED copies the installation and selected save, exports the current scene
and moves the party to the sector. The destination must be outside the source
installation. Allow enough disk space for another game installation. Your
original save stays untouched.

Choose a save that has never loaded the edited sector. WorldED refuses a sector
already cached in that save because this full-sector test cannot merge saved
DIF changes. The housing pack uses a separate, additive installation path and
must also be tested with previously visited sectors; see
[housing verification](housing.md#test-before-distribution).

NPCs that need generated inventories require the
`worlded-native-inventory` engine helper next to `worlded-explorer`. It runs
inside the copy, generates and equips inventories, and preserves permanent
object references for later saves. This preparation belongs to test copies;
it does not populate project exports. See
[native inventory helper](worlded-build.md#native-inventory-helper). If the helper is
missing, preparation of a scene that needs it fails with an explanation.

Use **Resume test…** with the copy's `playtest.json` to launch it again. Linux
and macOS copies also get a launcher script. Later edits need a new copy.
`test-launch.log` records launch output; `inventory-generation.log` records
inventory preparation when the helper is used.

## Project files and sharing

| File or folder | Purpose |
| --- | --- |
| `building.json` | Scene metadata, map and sector identity |
| `exports/maps/` | Saved SEC files and project mobile objects |
| `exports/scr/`, `exports/dlg/` | Compiled project scripts and dialogues |
| `script-sources/` | Editable structured scripts |
| `location-names.json` | Localized test marker names |
| `worlded.local.json` | This computer's game path; leave out of shared projects |
| `vegetation.mes` | Optional housing clearance rules, added by the author |

Raw sector exports can contain original game resources and are intended for
local tests. For a Renaissance house, use the housing exporter and review its
output before distribution. Keep game archives, saves and local configuration
out of the source repository.

WorldED reads DAT/DAT1 archives, SEC versions `0xAA0000` through `0xAA0004`,
version 119 MOB/PRO objects, ART graphics, MES tables and native scripts. ART
is read-only. Saved games and DIF files are not editable, structures spanning
sectors are not assembled automatically, and some character graphics fall back
to an idle sprite or a marker.
