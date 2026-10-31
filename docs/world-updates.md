# Save-compatible world updates

Renaissance updates must change only declared data. Moving a chest must keep its
inventory, lock, scripts and identity. An absent object is not evidence that it
should be created again: the player may have destroyed it or taken it elsewhere.

## Export and install

In WorldED, use **File > Export Renaissance update package…**. Review the listed
operations before choosing a new output folder. The [authoring guide](worlded-updates.md)
explains how to capture a reference, import an existing chest and publish later
revisions. Ordinary campaign export still produces replacement resources for a
separate installation; it is not this workflow.

Close the game before changing installed packages. From the repository, validate
and install an exported folder:

```powershell
python tools/world-updates/package.py check "C:/Mods/my-update-r1"
python tools/world-updates/package.py install "C:/Mods/my-update-r1" "D:/Jeux/Arcanum"
```

The installer copies into `ArcanumModern/world-updates/<package-id>/<revision>`
and publishes `index.mes` last. It keeps earlier revisions and the previous
registry. It does not open saves. Restart Renaissance to load the new registry.
The engine verifies package bytes and resource hashes before loading a party.
Release bundles use a separate `bundled.mes`, so installing a release does not
overwrite your manual registry. A manually installed package pins its revision
over the bundled default; install its newer package explicitly when upgrading.

For a hand-authored manifest, compile into a new folder first:

```sh
python tools/world-updates/package.py compile manifest.json my-update-r1
```

Treat packages as trusted mod content: new MOBs, scripts, prototypes and dialogues
use the game's native formats. WorldED validates its object exports; a SHA-256
hash detects changed bytes, not a malicious or structurally invalid game asset.

## What happens to a saved game

On the first load with pending changes, Renaissance copies the selected save and
package resources into the module's `Save/WorldUpdates/before-…` directory.
Failure to make that backup refuses the update. It does not silently proceed.

Each operation has a recorded result. A different saved value or missing target
produces a conflict and leaves that data alone; see `Save/WorldUpdates/updates.log`.
An unvisited sector stays pending until it loads. After a successful addition,
removing its object in play does not cause the package to spawn it again.
If a save requires a package that is missing from the installation, loading is
refused. Older package revisions and altered published bytes are refused too.

Save publication stages the new archive and writes metadata last. A disk journal
retains the previous complete slot while files are being replaced. If writing
fails, the engine restores it; after a process interruption, it recovers the
pending transaction before loading the module. This is recoverable publication,
not a single atomic rename of every file.

To roll back, close the game, preserve the current slot elsewhere, then restore
all files for the earlier slot together: `.tfaf`, `.tfai`, `.bmp` and its
matching `.gsi`. Restore the registry and resource versions that belonged to that
save as well. Earlier package directories are retained by the installer. Removing
only an update package does not reverse changes already saved in the world.

## Explicit state conversions

Hand-authored `global` operations can conditionally convert a `global_var`,
`global_flag` or `quest_state`. The index identifies the native variable or quest;
`before[0]` and `after[0]` are decimal strings. These run after the player and quest
records load. Quest conversions use the local player's state, preserve the
journal date, and do not award XP or replay alignment, sound or reaction effects.
Completed, externally completed and botched outcomes cannot be reset. Missing
quest definitions and incompatible global outcomes are conflicts.

Use a new operation ID for a later correction. Keep old operations and resource
bytes unchanged, even when skipping a release number. Dependencies declare the
order between packages; overlapping writes without that order are rejected.
New resource IDs are required for changed scripts or dialogues. ABI 1 does not
replace existing campaign resources or merge quest-definition MES tables.

## Native integration audit

The engine loads an original SEC or generated template, then its saved DIF.
Static object differences depend on the original serialization order. TEMP_ID
identifies the original record ordinal; null object IDs are not unique. Changing
a static object's position, render ordering or static/dynamic classification can
invalidate the correspondence. The initial update ABI rejects those operations.
It identifies static scenery with the map, sector, exact base-resource digest,
ordinal, type and prototype. Mobile objects retain their published GUIDs.

The existing sector pre-fold hook runs before sector_lock assigns the sector ID
and publishes it in the cache. It cannot safely call APIs which lock sectors.
The update integration captures provenance there and performs object operations
after the sector is published. During save loading, application waits until all
game and UI loaders have succeeded. Quest and player state are not available
while the Map loader is opening its sectors.

Save version 25 contains a fixed sequence of module records and sentinels. Adding
a mandatory module record would break old saves. Instead, an optional
`Save/Current/renaissance-world.sav` sidecar records update state. TIG includes
that file in the normal TFAI/TFAF archive. No sidecar means a pre-update save;
an invalid sidecar is an error, never an empty ledger. Reset clears memory on
new games, module changes and failed loads.

Ground cells have individual DIF bits. Blockers have a saved mask. Roof DIF
serialization is absent in the native engine, so roofs need a separate saved
state overlay. Their transient visibility/fade bits must not become authored
roof identifiers. A successful ledger write is not sufficient if a sector write
failed: map and sector failures must abort save publication and retain dirty
sectors for retry.

## Package and operation contract

An authoring manifest uses JSON. A bounded binary runtime file carries the same
validated operations; native loading does not evaluate scripts or executable
migration code. A package has a stable ASCII ID, increasing numeric revision,
display version, module name, supported update ABI and dependency revisions.
Published revisions and operation IDs are immutable. A later revision retains
the operation history needed by players who skipped intermediate releases.

Each operation declares its ID, introduction revision, map/sector, target,
precondition, write set and conflict policy. Conditional operations apply when
the current value equals the declared old value. A matching new value is already
satisfied. Any other value produces a conflict and leaves player state intact.
Authoritative policies must be explicit and remain limited to declared fields.

The first ABI supports:

- New dynamic objects with persistent GUIDs and an installation record. A later
  absence never causes the same addition to run again.
- Movement of GUID mobile containers or scenery, through the native movement
  API, without replacing their object records.
- Allowlisted scalar fields and flag masks. Identity, prototype, inventory arrays,
  resurrection and original static ordering are outside this operation.
- Individual ground, roof and blocker cells.
- In-place scenery suppression, including attached shadows and lights, while
  preserving the object and references to it.
- Explicit conditional global-variable, flag and quest migrations, applied only
  after the corresponding saved state has loaded. They do not infer a player's
  choices from missing objects or reset completed quests implicitly.

Dependencies establish ordering. Overlapping write sets without declared order
are conflicts. Deferred sectors retain pending operations; completing one sector
does not mark the whole package applied. The ledger identifies every operation,
its immutable digest and outcome, rather than storing only the highest version.

## Authoring reference and review

A zone update project must retain an immutable original reference independently
of its last-save/undo baseline. It contains the selected SEC or generated-template
bytes, relevant original mobile records and prototypes, source hashes and an
origin record for each original object. Authoring selection IDs are persisted in
this reference but never substitute for native GUIDs.

Before export, WorldED compares against that reference and displays additions,
cell changes, targeted field changes, preserved fields, conflicts and unsupported
changes. Unsupported changes block package export; the editor must not silently
omit them. Old projects need explicit reference adoption and ambiguity checks.
Reference blobs remain authoring data and are not distributed as replacement SECs.

Campaign export remains a separate replacement-resource workflow for independent
installations. It does not claim general compatibility with existing saves.

## Housing adoption

The general system takes responsibility for installation, terrain/roof/block
changes, scenery suppression and migration tracking. Housing retains the seller,
catalogue, purchases, keys, ownership, discovery, beds and dialogue behavior.
Published slots, GUIDs, script IDs, ownership/discovery globals and key IDs remain
unchanged. Existing objects are adopted without replacing their saved records.

New housing exports and the bundled estate use `housing.<slot>` packages. On
first adoption of an older estate, ownership, discovery or an existing estate
GUID preserves missing initial objects and current floor/roof/block values.
Old third-party exports without an update template retain the previous installer
until they are re-exported. Environment settings, sector scripts and removed
native lights remain housing metadata hooks; they are not ABI-1 operation kinds.

## Backup and recovery

Capture the actual loaded slot name; the most recently dated save may be a
different slot. Before application, back up its TFAI/TFAF, screenshot and matching
description-bearing GSI outside Current. Keep the package/resource versions and
backup provenance in its README; the shared operation log is saved beside the
backup directories as `updates.log`. Publication must not replace a good
save with incomplete archives after a failed write.

Rollback restores the complete earlier save and the corresponding resources.
Removing a package alone cannot reverse effects already stored in a save.

## Acceptance checks

The first native milestone uses a visited sector: unblock one passage, add one
decorative object and move a GUID chest containing items. Verify the same chest
identity, inventory and lock, then save/reload and replay without duplication.
Repeat with an unvisited sector, player and companions present, moved storage,
dead or recruited NPCs, skipped revisions, dependency conflicts and interrupted
application. All write tests use disposable copies of saves.
