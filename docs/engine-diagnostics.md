# Native engine diagnostics

Build the `check-campaign` CMake target and use a disposable game installation
containing `.renaissance-campaign-test` with exactly `1` followed by LF.
Never add this marker to a player's installation. Loading can write `Current`
and world-update backups even when a probe does not explicitly save.

Public reproduction fixtures are available from upstream issues
[#66](https://github.com/alexbatalov/arcanum-ce/issues/66),
[#68](https://github.com/alexbatalov/arcanum-ce/issues/68), and
[#128](https://github.com/alexbatalov/arcanum-ce/issues/128).
Inspect archive members before extracting into separate staging directories;
slot names can collide with existing saves. Keep downloaded fixtures outside Git.

For animation checks, see [follower diagnostics](follower-diagnostics.md).
For campaign and script checks, see [campaign compatibility](campaign-compatibility.md).

## Native area diagnostics

Run `check-campaign <marked-disposable-game> <SlotXXXX> --ce-isle-area`
on a main-world-map save. `CE_ISLE` reports the origin in tile coordinates,
its townmap ID, and the area returned by the production dialogue lookup.
The probe changes the PC location field only in memory, restores it, and does
not save the diagnostic state. Loading still writes the disposable Current
directory and can create world-update backups: never run it on a personal installation.

`--ce-inspect` loads a fixture and prints `CE_FIXTURE`, `CE_AREA` and
`CE_TOWNMAP` metadata. Redirect stdout/stderr to a local investigation log.
Area coordinates are tiles; townmap width/height describe map-image metadata.
A passing area probe is not proof that all island dialogues were played.

## Native shield diagnostic

`check-campaign <marked-disposable-game> Slot0106 --ce-shield` loads the
public elemental fixture separately for each of three scenarios: no spell,
Shield of Protection (definition 30), and Body of Fire (23). It creates two
unequipped wolves in memory, casts through the native invocation API, executes
the queued spell's BEGIN action, verifies its active flag, and resolves 12
forced melee hits with 10 normal damage each. The helper does not save.

`CE_SHIELD` records the requested definition, resolved shield definition,
actual attacker/target HP loss and target spell flags. Spell application and
damage resolution use the installed game resources. A separate control reloads
the fixture and activates the native critical-miss fate on the PC: the wolf's
attack is redirected to itself and the PC loses no HP. `CE_FUMBLE` reports the
measured losses and flags. This validates the retargeting path, not the natural
frequency of critical misses. Casting animation timing, equipment, autonomous
AI and other shield types are outside this probe.
`UNRESISTABLE` must not be used to simplify the fixture: the native
lifecycle also ends maintained spells with that flag. The test retains normal
resistance handling and fails if the protective effect was not applied.

## Public combat-state inspection

`check-campaign <marked-disposable-game> <SlotXXXX> --ce-combat-state` loads a
fixture and emits `CE_COMBAT`, `CE_ACTOR` and `CE_SPELL` records without advancing
the AI or saving. Loading still writes the disposable Current directory.
`CE_ACTOR` covers the engine's vicinity query, not every actor on the world map.
It includes prototype, tile distance from the PC, current HP/fatigue, activity,
death/unconsciousness, combat mode, danger/focus, animation slot/goal and the
raw object/critter/NPC/spell flags. Danger values are 0 none, 1 combat focus,
2 fleeing, 3 surrendered. The diagnostic derives these from the same stored
flags/handles used by the native danger query; it does not select a new target.
`CE_SPELL` lists active runtime slots, definition IDs, action, flags, caster and
target handles. It distinguishes an active definition from an orphaned flag.
