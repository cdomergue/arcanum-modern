# Campaign fixes and save compatibility

Development audit for `work/campaign-fixes`, based on Renaissance `ece466f`.
These corrections are not released or installed in the main game. AME's
[release notes](https://www.scribd.com/document/679588042/AME-Release) supply bug
reports to investigate; no AME code, scripts, dialogues or assets are imported.
Implementation evidence comes from CE source and locally installed resources.

## Compatibility contract

Existing saves must remain readable. Renaissance saves must load in pure
[Arcanum CE](https://github.com/alexbatalov/arcanum-ce) with the same content
dependencies, including UAP when used by that campaign. A bug may return under
CE; a crash or unreadable save is unacceptable. This reference replaces the
original executable for validation at the user's request.

The corrections introduce no serialized fields, resource identifiers, quest
numbers or migration ledger. They use existing quest state, script fields and
a native object flag. Save-writing tests use marked disposable installations.
Personal saves are excluded from the tests and Git.

## Implemented corrections

| Defect | Independent evidence | Correction and verification |
| --- | --- | --- |
| Willoughsby pays repeatedly | Native dialogue 1565 completes quest 1098 on reward lines 50 and 73; the local exit flag is set later. Interrupting between them allows another payment. Unmodified CE reproduces the second payment. | Skip only the leading money action on those two exact action strings when quest 1098 is completed. Native interpreter tests cover both rewards, interruption, save/reload, reputation and master training. The separate 3,000-gold player response remains payable. |
| Dialogue start overwrites script flags with counters | `SAT_DIALOG` assigns `hdr.counters` to `hdr.flags` before calling the dialogue handler. A regression executing the actual production case fails against the original code. | Copy flags from flags; retain the independent counter copy. Tests cover changed flags, changed counters, clearing flags, unchanged state, a non-player trigger and a missing callback. |
| Script division performs addition | `SAT_DIVIDE` evaluates `value1 + value2`. The actual action case fails a 17/5 regression. Installed scripts use this instruction, including Arronax, a money safe, equipment timers and technical books. | Perform signed integer division. Preserve the zero-divisor no-op and handle `INT_MIN / -1` without a processor exception. Positive, negative, zero-divisor and overflow cases pass. The script inventory does not establish that every affected quest was played. |
| One Tarant sewer arachnid lacks the mechanical flag | Native object reading identifies `maps/tarant sewers-01/g_dec5f747_1580_11d4_8f1d_00e02920220c.mob`, prototype 28343, flags `0x00002003`. Its prototype and neighbors include `OCF_MECHANICAL`. Both base and UAP fixtures show the defect. | At map postprocessing, restore only bit `0x20000000` for that exact module, prototype and GUID. Other flags are preserved. Tests load the actual sewer map, save/reload it, repair a CE save that already visited it, and round-trip the corrected save through pure CE, with and without UAP. |

The reward guard deliberately does not reject every payment after completion:
line 35 completes the quest before the legitimate 3,000-gold branch. Modified
reward strings are left alone. Quest tests use a follower as stand-in speaker
with the real dialogue and interpreter; they do not substitute for playing the
complete negotiations in Caladon.

The arachnid correction uses stored GUID bytes, whose printed order differs
from the MOB filename. It runs after native map loading, including save loading.
The result is an ordinary saved critter flag, readable by CE without the
correction; no prototype or MOB archive is rewritten.

## Remaining candidates

These are investigation leads, not implemented fixes.

| AME report | Installed-resource finding | Next reproduction |
| --- | --- | --- |
| Citizen alignment changes | Candidate 1823 contains reaction changes; no corresponding alignment action was found there. | Identify the precise speaker, line and alignment delta. |
| Theodor's equipment dialogue loops | Script 1033 checks item 2803 and quest 1008. Heartbeat 2705 checks nearby wielders and visibility. The original map instance has dialogue 1033 and death 2119; it does not initially attach that heartbeat. | Trace dynamic attachment; test player/follower equipment and visibility before changing control flow. |
| Witt conversation gets stuck | Resources 1940, 1941, 1957 and 3100 expose random question selection and global state. | Exercise both genders, exhausted questions, wrong answers and interrupted re-entry. |
| Dante leaves unexpectedly | Initial instance attachments: dialogue 1013, death 2462 and leader-killing 1014. Scripts 1016 and 1678 are marked unused and not attached there. Resurrection 2461 changes his dialogue. | Reproduce departure with recorded alignment, quest 1000 and recruitment/death history. |
| Half-ogre island has no escape | Teach resources 1061 and 1163 alone do not establish all transport dependencies. | Trace every exit and captain state before selecting an existing destination for recovery. |
| Air elemental has incorrect combat behavior | Prototype 28330, description 28310: native monster rules omit damage and specify faction 2 and Unseen Force. Native regeneration also shows zero damage. | Establish intended damage/spells without importing AME balance. Resolve localized name parsing before trusting regenerated spell IDs. |
| Kraka-Tur's eye is removed incorrectly | Native 1114 checks items 5619 and 5248. AME notes associate the later removal fix with an eye-crushing option it introduced itself. | Establish an equivalent native bug before changing behavior. |
| Fahrkus's corpse disappears | Actual instance has no no-decay flag; scripts 2791/2792/2799 handle visibility, dialogue and the death-triggered spirit spell. | Advance decay time and test spirit dialogue, quest outcomes and Brehgo comparison. Missing a flag alone does not establish intended lifetime. |
| Map collisions | No exact faulty tile has been identified. | Record map, tile, movement direction and actor size; test already visited sectors. |

Prototype inspection exposed another caveat: English stat/spell names in monster
rules may fail lookup against French labels during regeneration. This was not
corrected. A printed spell number from that path is not sufficient evidence of
the original monster's intended spell list.

## Validation matrix

Reference engine: upstream CE revision
`a7ff41b300ef712f0e7d088183a3d08957110cdd`, Windows x64 Release, Visual Studio 2019.
All 353 C/C++ source/header files in the reference tree match the pinned archive
byte for byte. The normal CE executable is unmodified. A second test executable
uses those same engine sources with a dedicated test entry point and an added
CMake target; it does not link Renaissance's engine patch.

Fixtures use installed French base assets, or UAP `2.0.2-fr-1` with Base, RaceMod
and hotfix `1.1.0`. **Data exception:** CE's existing MES reader overflows on seven
long French journal entries. Only in the disposable fixtures, entries 1261,
1361, 1382, 1415, 1416, 1435 and 1494 in `mes/gamebook.mes` were shortened to
1,999 bytes. Both loose-data and module paths receive the same override. No
engine change was made to the pure CE reference for this workaround. These
entries are prose, not quest actions. Original and fixture hashes are recorded
in `build/session/ce-reference-text-workaround.json`.

| Scenario | Result | Scope |
| --- | --- | --- |
| UAP custom-background character: Renaissance → CE → Renaissance | Passed | Live CE startup, load, character sheet, inventory, movement and save; return load, inspection and save in Renaissance. Strength 11, speed 10, follower limit 1, 12,345 gold and two daggers retained. |
| No-UAP character: Renaissance → CE → Renaissance | Passed | Native engine probe creates a fresh character using existing attributes, effects and items; both engines load/save/reload it. This is not an editor UI test. |
| Completed reward and training: Renaissance → CE → Renaissance | Passed | Native save probes preserve quest completion, gold, persuasion training, reputations, XP, character values, inventory count and location. |
| Arachnid, newly visited map | Passed, base and UAP | Pure CE exposes flags `0x00002003`; Renaissance produces `0x20002003`. Native save/reload retains the exact result. |
| Arachnid, map already visited in CE | Passed, base and UAP | An uncorrected CE save is loaded by Renaissance and repaired; corrected saves load/save/reload in CE and return to Renaissance. |
| Focused C regressions | Passed | Actual script action cases, reward guards and exact object matching; unrelated module/prototype/GUID and repeated application checked. |
| Release build and patch reconstruction | Passed | The integration patch reconstructs all 106 edited upstream files from the pinned revision. |

The fixtures are fresh test characters and generated quest/map states. No
long-running personal campaign was used. Full Caladon quest progression,
long-duration play and all combinations of third-party resources remain outside
the verified scope. The base-game live UI attempt could not proceed because
Computer Use app approval timed out; its native engine checks succeeded.

## Reproduction tools

- `tools/audit-campaign-bugs.py GAME --output REPORT.json` reads installed resources
  and records source paths, SHA-256 hashes, dialogue control flow and script
  instructions. Optional `--overlay ARCHIVE` arguments follow actual load order,
  earliest first. Instruction numbers are zero-based; unused operand slots are
  omitted. Reports contain no dialogue prose and never open saves.
- `tests/test_campaign_reward.py` compiles the campaign guards;
  `tests/test_script_native_actions.py` extracts and executes production action
  cases. Set `CC` to an available compatible C compiler wrapper.
- Build the `check-campaign` CMake target. The test installation must contain
  `.renaissance-campaign-test` with exactly `1` followed by LF. The probe refuses
  unmarked installations. Never add this marker to a player's installation.
- `check-campaign GAME SLOT` exercises rewards using a fresh save with a follower
  and writes `SlotC050` and `SlotC073`.
- `check-campaign GAME --create` creates the native no-UAP fixture (`SlotBASE`).
  `check-campaign GAME SLOT --roundtrip` writes and reloads `SlotCE00`; assertions
  expect the documented character fixture.
- `check-campaign GAME SLOT --arachnid` loads the native sewer map and writes
  `SlotSPDR`. `--arachnid-original` expects the uncorrected flag and writes
  `SlotOLDM`; use it with the pure CE probe to create an old-map fixture.
- `--prototypes` inspects elemental/arachnid prototypes. `--actors` reads native
  MOB paths from `actor-audit.txt` and reports flags and attached scripts.

Commands overwrite their named test slots. Private `build/session` logs and
`build/audit` reports preserve local validation evidence. Extracted resources,
saves, test installations and binaries are excluded from Git.
