# Exporting updates for existing saves

Use **File > Export Renaissance update package…** to distribute changes that
Renaissance can apply to a world the player has already visited. For example,
an update can clear a blocked tile, add scenery, and move an existing chest
while keeping the chest's contents and lock.

Work in a saved WorldED scene project. The exporter reviews individual changes
and stops if a change has no supported operation. The campaign exporter remains
the tool for replacing map files in a separate campaign installation.

## Keep the original reference

When you open a world sector, WorldED captures the original objects, ground and
source resource hashes before you edit. Saving the scene keeps this reference
alongside the current scene. Opening it again does not adopt a newer copy of the
installation as the baseline.

Older projects have no captured reference. Choose **Zone > Update reference…**
and mount the installation from which the project was created. Adoption checks
object identities and exact original records; ambiguous matches stop adoption.
It cannot prove that your installation matches an old release. Keep a copy of
that release if you intend to maintain updates for it.

The project contains `reference.bin`, `selections.bin`, and, after the first
export, `world-update.json`. Keep these files with the project. They preserve the
original reference, authoring identities and published revision history.

## Example: unblock a tile, add scenery, move a chest

1. Open the sector from the installation you are targeting and save a scene
   project outside the installation.
2. Clear the unwanted blocked tile with the blocking tool.
3. Place the new scenery from the palette.
4. Choose **Zone > Import campaign object for update…**, select the chest, and
   move it. This command retains the campaign object's GUID. Importing a fresh
   copy of an object would create a different identity.
5. Choose **File > Export Renaissance update package…**. Give the package a
   stable lowercase ID, such as `mytown.square-fix`, and a display version.
6. Read the review. It should list the tile change, addition and chest movement.
   Unsupported changes appear in the same window and disable export.
7. Export into a new folder. WorldED saves the publication history back into
   your scene project after writing the package.

The output contains `manifest.json`, the engine's `runtime.rwp`, and any new
resources referenced by the operations. Existing package folders are never
overwritten. Exporting does not install the package in your game.

A dependency line consists of a package ID and a minimum revision, for example
`mytown.foundation 2`. The engine must have that dependency available before
applying this package.

## Publish a correction

Reopen the same scene project, make the next change, and export again. The
package ID stays fixed and the revision increases. Each export includes the
previous operations and their resources unchanged, plus the new operations.
A second chest move starts from its last published position. It does not repeat
the first move against the original position.

If the project cannot be saved after export, keep it open and save it before
closing. The exported folder already exists, but the next revision also needs
the project's publication history. Do not edit that history or reuse a package
ID for an unrelated project.

## Supported changes and limits

The first update format supports ground tiles, roof cells, tile blocking,
new objects, hiding scenery, a small set of object fields and flags, and moving
GUID containers or scenery within their sector. Static records retain their
original ordering. Moving an original static object is rejected.

Existing inventory lists, actor statistics, schedules, scripts attached to
existing objects, independent lights, sector environment settings and world-map
labels are not rewritten by this exporter. The review names unsupported field
changes instead of silently leaving them out. Changed shared prototypes or
existing campaign resources also block export; new resources can be included.

The package records the values each operation expects to find. If a saved game
has changed that value or removed the target, the engine records a conflict.
It does not overwrite unrelated player state to force a match. A successful
addition is recorded so later loads do not spawn it again.

For the installation protocol, conflict handling and explicit global migrations,
see [World updates](world-updates.md). For the binary layout used by the engine,
see [Package format](../tools/world-updates/FORMAT.md).
