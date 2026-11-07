# CE issue applicability

Audit baseline: upstream `a7ff41b300ef712f0e7d088183a3d08957110cdd` with
Renaissance's integration patch. A proposed upstream PR is evidence to inspect,
not a patch to merge without checking local behavior. This audit distinguishes
source-level regressions from complete visual/gameplay reproductions.

## Native input fixes

[#64](https://github.com/alexbatalov/arcanum-ce/issues/64) describes duplicating
stacked objects by retrieving a stack while its quantity transfer is pending.
The native inventory filter still dispatched mouse and button operations in
`INTGAME_MODE_QUANTITY`. It now yields to the quantity picker without invoking
inventory handlers. Renaissance already hides the underlying hit regions while
its quantity modal is visible. `test_ce_quantity.py` executes the production
filter and checks mouse down/up/move, right click, button release, and normal
inventory input after leaving quantity mode. It tests dispatch prevention, not
a full game inventory transaction. The issue reports the duplication in the
original engine too; this is a legacy path retained by CE, not introduced by
Renaissance's inventory panel.

[#68](https://github.com/alexbatalov/arcanum-ce/issues/68) has two separate
paths. The native portrait filter now consumes the context click instead of
propagating it. The animation stop was traced further to the inventory availability
check: CE's `ai_can_speak` interrupts the actor, even when called merely to draw a
menu. Both the native menu and Renaissance now use a read-only preview. Actual
inventory/dialogue actions retain their execution behavior.

The public water-form fixture passes the native check in isolated FR and EN
installations: 60 previews preserve slot 17, goal 2 and the art ID; the old
execution path then removes the animation. Live Windows FR menu openings preserve
the goal, with different art-frame IDs in consecutive records. The portable test
checks 2,352 combinations against the production interruption eligibility rules,
plus timestamped diagnostics, missing directories and rotation. See
[follower diagnostics](follower-diagnostics.md) for reproduction and log reading.
This side effect is confirmed in CE and inherited by Renaissance; its presence in
the proprietary original executable has not been verified.

Windows MSVC compile/link and all 45 portable release checks passed on the combined
candidate. These checks do not validate complete packaging on every target OS.
The quantity transaction still needs a full native inventory gameplay reproduction.

## Other reports and required evidence

| Report | Current finding / next validation |
| --- | --- |
| [#167](https://github.com/alexbatalov/arcanum-ce/issues/167), IA dialogue conditions | `area_from_loc` rejects both an absent townmap at the actor and an absent townmap at the area origin. Moving only the first test does not solve both. `get_nearest_area` is not bounded by the area's radius; accepting its result everywhere could enable location-specific dialogue in the wilderness. Native checks on isolated FR and EN installations with UAP 2.0.2 and the active hotfix resolve the installed origin (20701, 55992) to townmap 19 and `area_of_object == 19`. This differs from the reported unpatched origin (20693, 56027). The current English installation has matching base/module archive hashes and matching active UAP resources; loose prototype differences do not establish full installation equivalence. The specific origin failure is not reproduced with these installed area resources. This does not validate every IA dialogue condition or unpatched game data. |
| [#66](https://github.com/alexbatalov/arcanum-ce/issues/66), concave walls | Deferred by user decision; no #66 renderer fix is included. The public Tarant fixture reproduces missing side walls in the CE-derived renderer. The experimental ground-footprint mask obscured outdoor characters along a roof edge; adding actor-shaped clearances produced unacceptable visual cutouts. Both experiments and their wall-rendering change were withdrawn, restoring the pre-experiment engine patch. Upstream reports the original engine is affected, but original-executable attribution for this scene was not independently verified. |
| [#128](https://github.com/alexbatalov/arcanum-ce/issues/128), protective shield | Native isolated FR/EN tests apply the real spell and resolve 12 forced melee hits: no shield loses 120 target HP, protection loses 60, and neither damages the attacker. Body of Fire is a positive control: 60 attacker HP lost. This excludes critical misses and does not reproduce the reported retaliation. The `OSF_SHIELDED` hit branch only adds an effect; critical misses can separately redirect damage to the attacker. Neither observation establishes the cause of the reported real combat. Persistent per-hit context remains needed; no speculative damage change. |
| #128, healthy enemies stop fighting | The four public fixtures load. In `test3` the nearby fighting enemy has 5 HP, zero fatigue and is unconscious; `test4` has dead nearby enemies. This is not the reported healthy-actor stop. Health and fatigue alone do not establish AI eligibility: also inspect focus, turn-list membership, animation and status flags. A delayed dead-actor cleanup log is not evidence that a living actor stopped. No speculative global AI changes made. |
| [PR #72](https://github.com/alexbatalov/arcanum-ce/pull/72) | Mostly renaming. Proposed initializations in `sub_43D9F0` were reviewed: current loops assign the used sector indexes, widths and lock flags before use. No proven uninitialized read established, so no cosmetic bulk import. |

The verified #128 fatigue, spell-cost and stun defects, #162 scripted reaction,
#163 follower waiting, and #148 text parsing already have separate commits and
production-C regression tests. Companion leveling, #147 script-cache release,
#142 quest descriptions and FFmpeg movie playback already have local solutions.
Platform-specific macOS launch/notch reports require native macOS validation;
Windows results do not establish their status.

## Public reproduction fixtures

Downloaded originals are kept outside Git under `output/ce-upstream-fixtures/`
in the local installation workspace:

- `walls.zip`: `Slot0104`, from the attachment on #66.
- `elemental.zip`: `Slot0106`, from the attachment on #68.
- `issue128.tar.gz`: four slots and `debug.log`, from #128.

Do not extract them into the main installation or overwrite existing diagnostic
slots: #128 uses common names such as `Slot0002`. Inspect archive members and
extract into separate staging directories; use a dedicated marked disposable
installation for runtime tests. Downloading/inspecting members is not evidence
that a symptom was reproduced. The archive files and saves must stay out of Git.

For newly reproduced failures without sufficient evidence, add bounded persistent
diagnostics and document how to interpret them. Read `save-diagnostics.md` for
the existing save/crash logs, and `combat-diagnostics.md` for the bounded per-hit
journal. Older reports do not contain the new records. Combat snapshots do not
yet trace the exact transition of an AI actor that silently stops acting.

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

Windows native FR and EN runs both passed: attacker loss `0/0/60`, target loss
`120/60/120` for the three scenarios. This is evidence about these controlled
conditions, not a claim that every #128 combat symptom is resolved. Native runs
with the combat journal each produce 37 attack, 49 damage, 12 fire-retaliation
and one critical-miss record. The critical-miss control loses attacker HP and
leaves the intended victim's HP unchanged. See [combat diagnostics](combat-diagnostics.md)
for record fields, limits and interpretation.

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

Native Windows inspection of the four public #128 slots found:

- `Slot0000` / `test1` and `Slot0001` / `test2`: no active spell slots, no active
  turn-based combat. This alone says nothing about later combat behavior.
- `Slot0002` / `test3`: PC HP 31, fatigue 30, stunned (`cf=40020`, goal 57);
  protective shield definition 30 and Agility of Fire definition 20 are active.
  Virgil is fighting; the adjacent enemy prototype 27356 has HP 5, fatigue 0,
  `unconscious=1`, goal 62. No healthy-stopped-enemy conclusion follows.
- `Slot0003` / `test4`: PC HP 33, fatigue 33, still stunned in the saved state;
  the same two spells are active. Nearby combat-focus holders are dead.

These native observations supersede the upstream comment's claim that none of
the attachments contain active shield state. They do not establish shield
retaliation. Do not infer save-state contents solely from that comment.

A live Windows run of `test3` with candidate `e8ad49d` resumes real-time combat,
clears the PC's stunned flag and accepts a movement command. Its per-hit journal
records ordinary attacks against the protected PC and Virgil attacking enemies;
it also records a later PC critical miss redirected to the PC. This is a
single observed continuation, not proof about every intermittent AI failure or
a comparison with the original executable. No new stun or AI fix was made for
this validation; the existing CE stun fix is already part of the candidate.
