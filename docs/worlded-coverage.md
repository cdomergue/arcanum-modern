# WorldED feature coverage

WorldED edits local sectors and campaign resources in separate projects. The
[editor guide](worlded.md), [inventory guide](worlded-inventory.md) and
[campaign guide](worlded-campaign.md) describe the available workflows. This
page records their scope and the remaining limitations.

## Terrain, facades and collision

The Ground / Roofs palette contains base materials and exact native tiles,
including transitions. The outdoor brush uses the installation's transition
rules; exact tiles bypass those rules. Roof cells cover four by four ground
cells. Availability follows the selected installation and its resource overrides.

The Facades library places complete assemblies, including rugs, from their
native FacWalk footprint and frame order. Native variants supply orientation;
walkability is preserved per piece. Placement includes a preview, boundary
checks, undo/redo and project/housing export. A facade must fit in one sector.
Continuous editing across sector boundaries and a top-down projection are not
available.

Blocking mode edits the SEC's 4,096 manual blocking bits. Bright red hatching
shows manual blocking; dark red shows blocking from terrain or object flags.
Walls and portals keep their own collision rules. The overlay does not simulate
pathfinding. Test entrances and routes with the player and companions in game.

## World terrain and map connections

The world view navigates maps and opens local sector drafts. The separate
campaign editor edits `terrain.tdf` by sector, fills rectangles with a selected
biome or exact code, resizes terrain and imports/exports lossless terrain-code
PNGs. Resizing retains the overlapping terrain rectangle; it does not move
objects, stored SEC files, map artwork, jump points or landmarks. The campaign
terrain brush does not generate biome transitions automatically.

Travel blocking follows the biome's `/b` flag in `terrain/terrain.mes`; code
65535 is also blocked. This is separate from movement blockers within a sector.
Stored SEC files continue to override generated terrain at their locations.

The jump-point editor adds, edits and removes map connections. It validates
destination map IDs and duplicate source tiles while preserving unknown flags
and padding. Return connections remain explicit. The editor does not prove
that a destination can be reached on foot or that a return connection exists.
A townmap wizard creates or assigns native map records and can import a PNG as
BMP. Its generator runs in an independent game copy and renders only the selected town.

## Lights, sound and sector properties

Sector properties selects music, ambient sound, lighting scheme and the
magic/technology adjustment. Lists use installed names and retain unknown IDs.
Existing sound flags are preserved.

**Zone > Lights…** adds, edits and removes independent sector lights. Controls
cover position, pixel offsets, shape, RGB color, dark lights/shadows, animation,
initial frame, ordinary direction and directional projection. Indoor/outdoor
lights follow the sector lighting schedule. **View > Light preview** shows an
approximation; it does not reproduce engine occlusion, animated shadows or
lighting over time.

The campaign lighting-schedule editor exposes 24 hourly outdoor/indoor RGB
entries and color swatches. Sound scheme records are editable as native tables.
Audio audition and a dedicated object-attached sound editor are not provided.
Eye Candy and additive blending are separate from dynamic lights.

## Characters, inventories and scripts

The character editor preserves sparse prototype inheritance and exposes stats,
skills, training, resistance, AI, faction and reaction fields. Daily routine
edits separate day/night positions and ordered, looping patrol waypoints.
Coordinates use world tiles and may cross sectors of the same map. Destinations
in world sector 0,0 from another sector are rejected because they overlap the
project's local coordinate representation. Route editing checks coordinates,
not reachability.

Generator controls expose the campaign ID, spawn interval, concurrent and total
limits, day/night activation, offscreen behavior and batch spawning. Existing
spawn counts are retained. IDs share campaign-wide counters; WorldED does not
allocate a globally unused ID automatically. Spells and technology edits college
levels, mastery and technology degrees using the engine's native fields.

The inventory editor creates individual container/NPC items, assigns equipment
slots and quantities, checks backpack placement, and preserves owner/item
identities. Duplication remaps owned items; deletion and undo include them.
Incomplete inventories with unresolved item references are refused. Native
inventory sources and autoequip definitions can be edited in the campaign
resource tables, using their original syntax. Item restrictions that depend on
race, gender, spells or graphics still need an in-game test.

Object script attachments expose flags and four byte counters, with an explicit
SCR-default reset. Tile and sector attachments support creation, editing and
removal with the same native state. Structured scripts, the native instruction
editor and dialogues are available; validation cannot establish quest behavior.

Dedicated relationship editors for Notify NPC/substitute inventories, shared
prototype creation and specialized item-property forms remain outside these
workflows. A field visible under Advanced is not a complete authoring tool.

## Campaign resources and maintenance

Campaign projects record resource overrides and their original hashes. They
support undo/redo, reopening against the same baseline, override export and an
independent test copy that excludes personal saves. New-map creation supplies
the map properties, terrain, entry point and jump table, with a map-list entry.
Campaign settings edits the adventure identity and starting year/hour.

Dedicated quest forms edit the native numeric fields and normal/low-intelligence
journal text together. Area forms edit world coordinates, names, descriptions
and radius. Encounter forms validate group prototypes and counts, and expose
regional frequency, power and table selection. These forms do not author quest
state graphs or the script logic behind a quest.

Native MES tables cover written text, descriptions, reputations, blessings,
curses, rumors, variables and other installed resources. Editing a record does
not create its dependent scripts or allocate referenced IDs. Campaign-wide
quest/reference validation, bulk level changes and prototype-reset operations
are not provided.

Campaign exports replace resources. They do not migrate a player's existing
quests or merge changes into visited sectors. Keep this distinction when
choosing between a campaign project and an additive housing export.

## Housing and validation

Housing exports changed terrain/roofs, manual blocking, sector properties and
tile/sector script attachments as additive resources. Saved script state takes
priority over authored defaults. Facades export through their native ground
cells. Independent light changes use persistent attached-light objects and
explicit removal records so original independent-light ordering remains intact.

New housing containers and characters can include manual inventories. Starting
contents apply when the owner is first installed; changing the authored contents
later must not refill an existing player's chest. NPC routine, generator and
knowledge fields travel with newly exported NPC instances. Campaign terrain and
shared rule-table replacements belong to campaign exports.

Local format tests, undo/redo and save/reopen checks do not replace a playable
copy. Test additive housing with both fresh and previously visited sectors,
including save/reload and repeated installation. The generic full-sector zone
playtest requires an unvisited sector because it cannot merge saved DIF changes.
