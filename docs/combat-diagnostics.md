# Reading combat diagnostics

Builds with combat diagnostics write `ArcanumModern/combat-events.log` relative
to the game's working directory. The launcher normally uses the installation
directory. Each event is written and closed immediately. At 1 MiB, the file
rotates to `combat-events.previous.log`: total storage is approximately 2 MiB,
plus at most one record per file. A failed rotation stops appending until it can
succeed. Missing or unwritable directories do not prevent combat.

Check the executable path and SHA-256 before interpreting logs. Updating source
does not update a running or installed engine. Logs from FR, EN and disposable
test installations are separate. No character names or save contents are logged.

```powershell
Get-Process *arcanum* -ErrorAction SilentlyContinue | Select-Object Id,Path,StartTime
Get-FileHash .\arcanum-modern.exe
Get-Content .\ArcanumModern\combat-events.log -Tail 40
Select-String -Path .\ArcanumModern\combat-events*.log -Pattern 'event=critical_miss|event=body_of_fire'
```

## Events and fields

The timestamp is local wall-clock time with UTC offset. `seq` orders records
within the process and restarts at process launch. It is not an attack ID.
Runtime handles are only meaningful within the same process and loaded game;
loads can replace objects. `tb` indicates turn-based combat.

| Event | Meaning |
| --- | --- |
| `attack` | Hit/dodge resolution before critical-miss retargeting. `detail` includes the existing hit chance/roll, armor penalty, dodge chance/roll and dodge outcome. No new random rolls are made for logging. A zero roll can mean a forced or otherwise non-rolled result. |
| `critical_miss` | The engine is about to redirect this attack to the attacker. `t` still identifies the intended victim. This is not evidence of shield retaliation. |
| `body_of_fire` | The melee branch explicitly requests 5 fire damage in retaliation. Attacker and target are reversed compared with the incoming hit. The following damage result determines actual HP loss after resistance. |
| `damage` | A call to the damage processor has returned, including its early-return paths. `before_a` and `before_t` are snapshots at entry; `a` and `t` are snapshots after it returned. Compare HP, not merely the requested damage array. |

`flags` and `damage_flags` are hexadecimal engine bitfields. In `flags`, hit is
`0x2`, critical is `0x4`, critical-miss processing adds `0x8`, forced damage is
`0x100`, and ranged is `0x200`. Consult `game/combat.h` for the full damage-flag
definitions. On `damage`, these are the final fields; `detail` preserves their
input values and input damage. `damage` lists normal, poison, electrical, fire,
and fatigue amounts in that order. These amounts are not guaranteed to equal
HP loss: resistances, scripts, status effects and lethal damage rules intervene.

`weapon` is the runtime weapon handle, or zero. `original_target` is the engine
context's `field_28`, normally initialized to the intended victim. Prefer the
preceding `attack`/`critical_miss` record when establishing intent; not every
damage source follows the normal weapon-attack path.

Each actor snapshot contains:

| Field | Meaning |
| --- | --- |
| `obj`, `valid`, `type` | Runtime handle, whether it still exists, and native object type. Null/deleted handles produce `valid=0 type=-1`; other zero fields in such a snapshot are unavailable, not observed zero HP. |
| `hp`, `fatigue` | Current values for critters. Not measured for non-critter objects. |
| `mode`, `focus` | Critter combat-mode flag and NPC combat-focus handle. Combat mode alone does not establish turn-list membership or AI eligibility. |
| `slot`, `goal` | Main animation slot and current goal; `-1` means absent. Slot numbers are reused. |
| `spells`, `shield` | Hexadecimal spell flags and the definition ID resolved for `OSF_SHIELDED`, or `-1`. Protection is definition `30`; Body of Fire is flag `0x10`. Spell flags are not definition IDs. This is not a list of every active spell. |
| `flags`, `flags2` | Critter status bitfields from `game/obj_flags.h`, useful for stun, unconsciousness and other restrictions. |

## Investigating reported retaliation

Find the incoming `attack`, inspect the target's `shield` and spell flags, then
follow any `critical_miss`, `body_of_fire` and `damage` records. Compare HP for
the same valid handle before and after the damage call. An impact animation or
floating message alone does not establish damage or its cause.

The native `--ce-shield` probe is documented in [engine diagnostics](engine-diagnostics.md).
FR and EN tests show no retaliation from protection under controlled forced hits,
with Body of Fire as a positive control. A separate native fate-forced critical
miss verifies that the journal records the intended victim before retargeting
and the attacker's HP loss afterwards. This does not resolve every symptom in
[CE #128](https://github.com/alexbatalov/arcanum-ce/issues/128).

## Limits and verification

These are event traces, not a per-frame AI monitor. A healthy actor that stops
acting without another attack can leave no record of the exact transition.
Nested damage from scripts can produce intervening records before the outer
`damage` result. Its HP delta includes any nested effects during that call.
A crash inside the damage processor can prevent its result record; preserve
the separate crash report and matching symbols. See [save/crash diagnostics](save-diagnostics.md).

`tests/test_combat_diagnostics.py` executes the production logger and wrapper
with controlled engine accessors. It verifies snapshots, HP deltas, missing and
deleted handles, timestamp, rotation, failed rotation, missing directory, errno
preservation and exactly one call to the damage implementation. Native Windows
FR/EN shield probes validate the integrated engine separately. None of these
tests proves that all autonomous AI failures or critical failures are fixed.
