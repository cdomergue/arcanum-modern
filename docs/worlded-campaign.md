# Campaign projects

Open **Resources > Campaign…** to edit campaign resources. A campaign project is
separate from a zone or house project. It stores only your overrides, together
with the SHA-256 hash of each original resource, in `campaign.json`.

Use a campaign project when changing the world itself: terrain, map connections,
starting settings or shared text and rule tables. Use a zone project for a house
that the estate agent will install later. Campaign exports replace resources;
they do not migrate an existing player's quests or visited sectors.

## Save and test your work

1. Select a resource and choose **Edit selected resource…**, or double-click it.
2. Apply the changes in that resource's dialog. **Undo** and **Redo** work across
   applied resource edits. Discarding an override restores the installation's
   version and can also be undone.
3. Choose **Save project…** and select an empty folder outside the game. Later
   saves can use the same project folder. Saving into another existing campaign
   project is refused; open that project explicitly or choose an empty folder.
   Reopening refuses a project if an
   original resource has changed; resolve that difference before continuing.
4. Choose **Create test copy…**, select the Renaissance executable, then an empty
   folder outside the installation. WorldED copies the game resources and runtime
   libraries, excludes personal saves and caches, and applies your overrides to
   the copy. It records the project and source paths in `.worlded-campaign-test`.
5. Launch the copy when prompted and start a new game. Keep its saves in the copy.

**Export overrides…** writes a `modules/Arcanum` resource tree into an empty
folder. Copy that tree into an independent game installation. Keep the original
resources and saves for rollback. Removing an override does not necessarily undo
changes already recorded in a saved game.

The campaign editor refuses project saves and exports inside the source
installation. Opening another project requires you to save or explicitly discard
unsaved changes. Projects are limited to 128 MiB; an oversized save is refused
before replacing the existing file. Split large sets of overrides into separate
projects.
Creating a new test copy requires a saved project, so the test has an identifiable
authoring source. You can also run the copied executable again from its `game`
folder; use that folder as the working directory.

## Starting settings and new maps

**Campaign settings…** edits the adventure name and ID, starting year and starting
hour in `rules/gameinit.mes`. Existing campaign records outside those four fields
are retained. These values apply when the game starts the campaign.

**New map…** creates `map.prp`, `terrain.tdf`, an empty `map.jmp`, and `startloc.txt`.
It adds the next record in `rules/maplist.mes`. Choose a folder name, dimensions in
sectors, and a base biome from the installation. The default entry point is tile
32,32. Connect the map to another map before trying to enter it in the game.

To change the starting map, edit `rules/maplist.mes` and its `Type: START_MAP`
record. Preserve the native record's comma-separated fields and other options.
The editor does not silently relocate existing objects when you change a map's
start location or dimensions.

## World terrain

Select `maps/<map>/terrain.tdf`. A cell in this grid represents a whole 64×64
sector, not an individual ground tile.

Enter the rectangle's sector coordinates and dimensions, choose a biome, and
press **Fill rectangle**. An optional exact 16-bit native code preserves a
specific transition or variant. The biome brush creates uniform native terrain;
it does not generate transitions between different biomes automatically.
Existing stored SEC files still override generated terrain at their locations.

**Resize…** preserves the overlapping rectangle and fills new cells with the
selected biome. Shrinking crops terrain cells after a warning. Applying the edit
updates `map.prp` dimensions in the same undo operation. Objects, stored sectors,
jump points, worldmap artwork and named locations are not moved or rescaled.
Check their coordinates before using the resized map.

**Export code image…** produces a lossless PNG with one pixel per sector. The red
channel contains the high byte of the terrain code; green contains the low byte;
blue is zero. **Import code image…** reads that format and requires the current
grid dimensions. It is an exact data interchange format, not a painted landscape
or a generated travel-map illustration. Do not save it as JPEG or apply color
correction.

World travel blocking comes from the biome's `/b` flag in `terrain/terrain.mes`.
Edit that resource to change a biome's travel behavior. Code `65535` means no
terrain and is blocked. This is distinct from a zone's individual blocking cells.

## Jump points

Select `maps/<map>/map.jmp` to add, edit or remove map connections. Coordinates
are absolute ground tiles. The destination uses the engine's one-based map ID:
record 5000 in `rules/maplist.mes` is map 1, record 5001 is map 2, and so on.
Do not enter 5000 as the destination for map 1.

The editor rejects missing destination map IDs and duplicate source tiles.
It preserves padding and unknown flag bits on existing entries. Add a return
jump in the destination map when the passage should work both ways. Test both
ends; the current editor does not prove that the destination is walkable or that
a return jump exists.

## Text, rules and sound

The resource list includes `.mes` tables under `mes`, `rules`, `worldmap`,
`terrain` and `sound`. Filter by filename to find quest definitions, normal and
low-intelligence journals, descriptions, written text, areas, encounters,
reputations, blessings, curses, rumors, inventory sources, autoequip rules,
variable names or sound schemes supplied by the installation.

The table editor adds, removes and changes records. IDs must be unique unsigned
integers. Text is encoded as Windows-1252 for the game; braces cannot appear
inside a record. Editing a table rewrites its records and removes comments or
formatting outside them. Untouched resource files remain byte-for-byte unchanged.

Rule records retain their native syntax. For example, editing a journal's text
does not create a quest definition, and changing an inventory-source record does
not allocate its item prototypes. Check the accompanying resources and scripts.
Use **Quests / areas / encounters…** for structured authoring of those records.
Other rule tables retain their native record syntax.

Lighting schemes named by `rules/lighting schemes.mes` open a dedicated schedule
editor. It has 24 rows and six channels per row: outdoor RGB followed by indoor
RGB. Leave an entire interior row blank for native interpolation. Specify hours
0 and 23. The swatches show the selected colors; game lighting, occlusion and
animated shadows still require a test in the game.

Sound scheme definitions are editable through the same campaign table editor.
Audio audition, automatic biome transitions, automatic return-jump creation and
campaign-wide script/quest-reference validation are not yet provided by this dialog.

## Quests, areas and encounters

Choose **Quests / areas / encounters…** to open a structured form.

A quest uses an ID from 1000 to 1999. The form edits its experience level,
alignment adjustment, and dialogue entry points for all seven native quest
states. The three dialogue columns cover normal reaction, bad reaction and low
intelligence. `-1` means no line. Both journal variants are edited in the same
form. Applying it updates the definition and two journal resources in one undo
operation. Scripts still decide when the quest changes state.

The area form edits absolute tile coordinates, worldmap label offsets, discovery
radius in sectors, name and description. New areas append after existing records
to preserve their positional indices. It does not rename or renumber existing
areas behind the scenes.

Encounter groups use IDs `50000 + table index * 100 + entry index`. Entries within
a table must be contiguous. Supply a relative selection weight and up to five
NPC prototype/count groups. The editor checks prototype type, group order and
minimum/maximum counts. Optional native conditions remain available for level,
global-flag and trigger-count restrictions. Regional forms edit encounter
frequency, power and table overrides with center coordinates and radius in tiles.
Adding a user table updates the declared table count. Test encounter behavior
in the copy: a definition does not by itself guarantee that the campaign's travel
routes will select that table.

## Townmap assignment and artwork

Choose **Townmaps… > Create and assign a townmap**. Supply a new persistent ID,
resource folder, campaign map and sector rectangle. The editor creates version-2
TMI metadata and assigns the selected sectors. Existing townmap assignments are
protected. A temporary bitmap displays “Generate townmap” until artwork is ready.

Save the project and create an independent campaign test copy. Then choose
**Townmaps… > Generate artwork in a test copy**, select your town, and point it at
the folder containing `.worlded-campaign-test`. WorldED runs the native
Renaissance renderer for that town in the copy. It imports the resulting images
as the native BMP tile resources. Save again, then export or recreate the copy
to include those BMPs. Generation logs remain in the test-copy folder.

This requires the matching Renaissance engine with the town-ID filter. Generation
and import currently support townmaps created by this editor: 1024-pixel render
tiles at one-eighth scale. Original townmaps can be retained and referenced by
existing resources; their image layout is not silently converted.
