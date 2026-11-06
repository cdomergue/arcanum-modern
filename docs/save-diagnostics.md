# Reading save and crash diagnostics

## Locate the installation that produced the log

Logs belong to the game installation that ran the engine. A disposable test copy
has its own logs. Never assume the Steam installation, French installation and
test copy share them. Check the running executable path and its SHA-256 first.
Updating source code does not update an installed executable.

For the French installation in this workspace:

```powershell
Get-Process *arcanum* -ErrorAction SilentlyContinue | Select-Object Id,Path,StartTime
Get-FileHash 'D:\Jeux\Arcanum\arcanum-modern.exe'
Get-Content 'D:\Jeux\Arcanum\ArcanumModern\save-events.log' -Tail 80
Get-Content 'D:\Jeux\Arcanum\ArcanumModern\save-events.log' -Tail 20 -Wait
```

Stop live tailing with Ctrl+C. The save logger uses the game's working directory;
the normal launcher runs it from the installation directory. If launching the
engine manually, use that directory as the working directory. `ArcanumModern`
must exist and be writable. A missing log can mean an older executable, a
different working directory or a logging write failure; it does not prove that
no save was attempted. Logging failure never blocks saving.

## Save events

`ArcanumModern/save-events.log` is the persistent save journal. Each line contains
local date/time with timezone offset, `event`, `detail`, `phase` and
`sector_failed`. It is closed after each entry. At 1 MiB it rotates to
`save-events.previous.log`, replacing the older rotated file. Preserve both files
when collecting a report. Records contain slot IDs and engine context, not save
descriptions or serialized character data. Timing has one-second resolution.

| Event | Meaning |
| --- | --- |
| `autosave_request` | Trigger: `before_combat`, `before_map_change` or `after_combat_deferred`. This does not mean a save succeeded. The deferred request waits for a safe state and can be cancelled by load/reset. |
| `autosave_skipped` | The controller rejected the request because gameplay was in an unsafe state. No write was attempted. |
| `autosave_begin` | Starts creation of the next rotating automatic slot. |
| `save_begin` | Enters the engine save operation. `MdrnAuto` is the autosave staging name, not the final slot. |
| `sector_error` | Gives the map, sector identifier and reference count, or a sector write failure. |
| `save_failure` | Engine save failed. Read `phase` and preceding sector errors. |
| `save_success` | Engine save was published. For autosaves, this is still the intermediate staging save. |
| `autosave_failure` | The overall autosave failed, potentially during final file renaming even if the engine save succeeded. |
| `autosave_success` | Final automatic slot was published successfully. |

`phase` identifies the latest save step: eligibility, retained-sector retry,
staging allocation, current directory, data open/version write, an engine module
name, data close, extra save callback, world updates/progression/sectors, metadata,
archive creation or final publication. It is diagnostic context, not an OS error
code. A module phase also covers its following sentinel write. The combined
world-updates/progression phase does not identify which sub-operation failed.

`sector_failed=1` is a retained failure flag, not evidence of permanent corruption.
A later save retries dirty sectors and clears it only after successful recovery.
A `sector_error` can show `sector_failed=0` because it is logged before the flag
is set. The phase can be `idle` for sector errors outside a save attempt.

Read events in file order. For an intermittent pre-combat failure, look for:

1. `autosave_request detail=before_combat` and `autosave_begin`.
2. `save_begin detail=MdrnAuto`.
3. `sector_error` with `Sector flush blocked` and a nonzero `references` count.
4. `save_failure`, then `autosave_failure`.
5. A later successful save: distinguish automatic final publication from manual
   engine publication.

`references` describes live engine references to a map sector. It is **not** a
Windows file lock, disk permission failure or proof of a memory leak. Do not
force reference counts to zero or bypass the save failure guard. That could
publish an inconsistent save.

The legacy `ArcanumModern/sector-save-errors.log` also records sector failures,
but its old entries have no timestamps. File modification time only dates the
last write; it cannot timestamp every entry. Prefer the new event journal for
correlation. Ordinary SDL/TIG debug output is not automatically a persistent log.

## Crash reports are separate

`ArcanumModern/CrashReports` contains `.txt`, `.dmp` and, when available, an
installed-manifest copy. If the installation is not writable, check
`%LOCALAPPDATA%\ArcanumRenaissance\CrashReports`. The text identifies the exception,
module and offset; the dump provides thread/context/module information. Analyze
it against the matching executable and symbols. A crash filename uses the
session's startup time; do not treat it as the precise crash time.

A save failure without a crash need not produce a dump. Forced termination,
power loss and some early failures cannot be captured by the crash handler.
Dumps can contain fragments of process memory: do not commit or publish them
without reviewing what will be shared.

## Investigation procedure and known observation

Collect the exact message, trigger, approximate time, running executable hash,
both save-event logs and relevant crash reports. Read personal saves only; copy
them into a marked disposable installation before any load/save tests. Record
which binary performed the test. Never write diagnostic saves in the main game.

On 2026-11-06, the French installation reported an intermittent failure of the
automatic save before combat. Later saves succeeded. The legacy log last changed
at 23:12:56 and contained blocked sectors with nonzero references; automatic slots
were created at 23:12:24 and 23:12:37. Because the old entries lack timestamps,
this is a strong lead, not a proven association with that precise popup.

The latest slot `SlotA0NL` loaded on an isolated copy and passed the native
`--save-retry` check: save, simulated locked-sector failure, recovery after
unlocking, and reload with unchanged player snapshot. This is not reproduction
of the actual pre-combat timing. The generic `--roundtrip` fixture expects fixed
character stats and daggers; its fixture assertion failure on a personal save
must not be reported as save corruption.

The main FR binary at investigation start had SHA-256
`13E71A71DDA53AA29F3ABB0CFA4D6981D78A44C3E762A030668E536E3E71354C`.
Before-combat autosave is a Renaissance integration; the sector cache is inherited
from CE. Attribution of this intermittent failure remains unconfirmed until the
trigger and live reference lifetime are established. These logs improve evidence;
they do not themselves fix the scheduling issue.

For companion-menu animation stops, see [follower diagnostics](follower-diagnostics.md). The save journal does not record animation state.
