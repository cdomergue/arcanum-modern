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
| [#66](https://github.com/alexbatalov/arcanum-ce/issues/66), concave walls | Reported in the original engine. Requires the supplied Tarant scene and camera position to distinguish visibility geometry from art assets. No speculative renderer changes made. |
| [#128](https://github.com/alexbatalov/arcanum-ce/issues/128), protective shield | The `OSF_SHIELDED` branch of `magictech_anim_play_hit_fx` adds a visual effect; it does not itself apply retaliatory damage. A visible impact is not proof of HP loss. Record active spell definition IDs, attacker/target and actual HP changes; distinguish protective, fire and electrical shields. No damage change without reproduction. |
| #128, healthy enemies stop fighting | Health and fatigue alone do not establish AI eligibility. Need the actor, active target, combat membership, animation goal, status flags and time of disengagement. A delayed dead-actor cleanup log is not evidence that a living actor stopped. No speculative global AI changes made. |
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
the existing save/crash logs; these do not currently trace per-hit spell damage
or AI disengagement. Do not invent those records when reading an old report.

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
