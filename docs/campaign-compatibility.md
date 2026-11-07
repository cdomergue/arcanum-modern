# Campaign corrections and save compatibility

## Compatibility contract

Existing saves must remain readable. Renaissance saves must load in pure
[Arcanum CE](https://github.com/alexbatalov/arcanum-ce) with the same content
dependencies, including UAP when used by that campaign. A bug may return under
CE; a crash or unreadable save is unacceptable.

The corrections introduce no serialized fields, resource identifiers, quest
numbers or migration ledger. They use existing quest state, script fields and
a native object flag. Save-writing tests use marked disposable installations.
Personal saves are excluded from the tests and Git.

This compatibility contract covers these four corrections and their test fixtures.
It does not cover every Renaissance feature. Custom portraits and advanced
background effects retain the resource requirements described in
[portraits](portraits.md) and [backgrounds](backgrounds.md).

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

Commands overwrite their named test slots. Keep fixtures, extracted resources,
saves, binaries and generated reports outside Git.
