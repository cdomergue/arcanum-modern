# Free camera and local-map annotation diagnostics

In free camera mode, a moving controlled actor approaching the viewport edge
triggers one immediate, broad camera advance. There is no interpolated translation.
The destination places the actor on the opposite side of the center to leave
space ahead, then the view stays still until the next edge crossing. Both useful
axes advance together for diagonal movement. Margin and advance distance scale
with the viewport. The selected companion is the anchor while directly controlled.
A stationary actor does not undo manual pans. Follow camera, tactical camera and
modal interfaces retain their own behavior.

`ArcanumModern/free-camera.log` records each jump, viewport dimensions, relative
actor position, requested advance and resulting relative position. `frames=1`
and `duration_ms=0` identify the immediate behavior. It excludes absolute world
positions and names. It rotates at 1 MiB to `free-camera.previous.log` and is
limited to 20 records per second.

Local-map annotations use one in-memory collection owned by a town-map ID. A
change of owner saves the old collection before loading the requested city's
file. Marking scenery selects that city's collection before checking for a
duplicate. Reloading the same city retains unsaved edits; loading another game
session resets the owner before closing the map, avoiding writing the previous
session into the restored working files. Runtime image buffers are discarded
when switching and are never restored from serialized flags.

A failed write retains the old collection in memory. It is not displayed or
editable on another city's map. A failed read is not treated as an empty file;
malformed headers and strings are rejected without publishing a partial list.
`ArcanumModern/map-notes.log` records owner IDs, counts and read/write outcomes,
without annotation text. It rotates at 1 MiB to `map-notes.previous.log`, with at
most 20 records per second.

Existing TMN files store the city only in their filename, not in individual
annotations. Notes previously written into another city's file cannot be
reassigned reliably from their relative coordinates. They are preserved; the
engine does not remove all automatic annotations to conceal the corruption.

Verification:

- `python3 tests/test_free_camera.py`: production edge trigger, immediate broad advances without repeated
  edge nudges, diagonal lookahead, actor selection, viewport scaling, context
  isolation and log rotation.
- `python3 tests/test_map_notes.py`: production marking and serialization,
  A/B/A switching, matching coordinates, unsaved personal annotations,
  same-city session reload, sparse IDs, resource cleanup, failed reads/writes,
  invalid headers and bounded diagnostics.
- `tools/build-pathfinding-check.py` and `build/check-pathfinding GAME`: native
  distant clicks for hero and companion, visibility while moving and a camera
  pan without centering. Run only in a marked disposable game copy.
- `tools/build-local-route-check.py` and `build/check-local-route GAME`: native
  TMN round-trip between two town-map IDs, local-map rendering and route travel,
  also only in a marked disposable copy. No save slot is loaded or written.

## Personal notes in the modern interface

Modern personal map notes are a separate notebook sidecar. Their identity now
includes both the engine map ID and the town-map ID, because several cities share
one engine map. New `NOTE2` records preserve this identity through creation,
serialization, display, hover and editing. World/continent notes keep their
existing behavior, journal favorites and tracked quests are retained.

Legacy `NOTE` records are still read and preserved. Old local notes have no city
identifier and are hidden until assigned; guessing from their relative coordinates
would display them in unrelated cities again. Their text is never discarded by
this migration. `ArcanumModern/notebook-notes.log` records load/save outcomes and
the count of local notes without a city, without text, character IDs or paths. It
rotates at 1 MiB to `notebook-notes.previous.log`, at most 20 records per second.

`python3 tests/test_notebook_map_scope.py` runs production notebook read/write,
map-note drawing and editor actions. It checks different towns on the same engine
map, old-note preservation, new-record round trips, world notes, journal data and
stale clicks. The native local-route probe also renders one local personal note
and checks that a foreign-city note and an unassigned legacy note are absent,
while both remain in the isolated notebook file.
