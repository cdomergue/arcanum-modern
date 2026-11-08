# Follower animation diagnostics

The engine writes `ArcanumModern/follower-events.log` relative to the game's
working directory when a native or Renaissance follower context menu opens.
Logging is always enabled in builds containing this fix. Each record is closed
immediately. At 1 MiB the file rotates to `follower-events.previous.log`; the two
files use about 2 MiB at most. Missing or unwritable directories do not block play.
Actor names, save descriptions and save contents are not recorded.

Read recent entries with PowerShell from the relevant installation:

```powershell
Get-Content .\ArcanumModern\follower-events.log -Tail 30
```

Each `event=menu_open` record contains:

| Field | Meaning |
| --- | --- |
| Timestamp | Local wall-clock time, including UTC offset. |
| `actor` | Runtime object handle. Compare only within the same process/load; it is not a permanent character ID. |
| `inventory` | Whether the inventory command is available after the preview check. |
| `active` | Animation presence before/after the preview check. `1/0` indicates that the check removed an animation. |
| `slot` | Animation slot before/after; `-1` means no main animation was found. Slots can be reused. |
| `goal` | Current engine animation goal type before/after; `-1` means absent. Goal `2` is `AG_ANIM_FIDGET`, used by the public water-elemental fixture. |
| `aid` | Current art ID before/after in hexadecimal, including animation frame and orientation. |
| `spell_flags` | Actor spell flags in hexadecimal. These are flags, not spell definition IDs. |

Pairs separated by `/` surround the inventory eligibility check within one menu
opening. Equal pairs show that this check preserved the sampled state. They do
not prove that all animations keep advancing afterwards. To investigate a later
freeze, record the action, time and whether movement resumes it, and compare
successive records. Different `aid` values for the same stationary actor and goal
can establish that its art state advanced between observations. A log with no
menu event cannot identify an unrelated animation stop.

## Reproducing CE #68

Use the public `Slot0106` attachment linked from
[CE issue #68](https://github.com/alexbatalov/arcanum-ce/issues/68) in a marked,
disposable installation. Never place it over personal saves. Load `body-water`,
observe the water-form companion at rest, open and close its portrait menu, and
compare the animation and log. Move the group before repeating if a loaded or
previously interrupted animation is already stopped. Test selecting Inventory
separately: executing that action still retains the native interruption behavior.

The native helper also supports:

```text
check-campaign <marked-disposable-game> Slot0106 --ce-portrait
```

It loads the fixture, checks 60 inventory previews preserve the existing
animation ID, goal, flags and art ID, then verifies that the execution path can
still interrupt that animation. `CE_PORTRAIT` and `PASS`/`FAIL` go to stdout/stderr;
redirect both to a local investigation log. The helper does not pump the rendering
loop, so this validates animation-state preservation rather than frame timing.
Loading writes the disposable `Current` directory; it must not run in a personal
installation. The original fixture has an interruptible fidget goal and is
required by this probe; an arbitrary combat save is not an equivalent fixture.

## Cause and scope

CE's menu refresh used `inven_ui_can_open_inven`, which calls `ai_can_speak` and
then `sub_424070` to interrupt animations. Renaissance reused that function when
drawing its own menu. The preview path now retains the same status, danger,
reaction and animation-priority restrictions without interrupting an actor.
Executing an inventory or dialogue action retains the original behavior.

This cause is confirmed in the pinned CE source and in native FR/EN tests.
Renaissance inherited the side effect in its own menu. The original proprietary
executable was not inspected for this cause; do not claim a CE-only origin or an
original-Arcanum origin without further evidence. The earlier native click
consumption fix addresses event propagation and is not the fix for this side
effect.

## Nested followers and deployable constructs

A construct deployed by a controlled companion follows that companion. Party
queries must therefore include followers of followers. The traversal advances
through each nested list before releasing its temporary nodes; repeating the
same entry would allocate nodes indefinitely.

`ArcanumModern/party-traversal.log` records sampled nested expansions with
`begin` and `complete` phases, SDL uptime, root actor handle, direct and nested
counts, and elapsed milliseconds. Sampling is limited to once per second. The
log rotates at 1 MiB to `party-traversal.previous.log`; it contains no names or
serialized save data. An unfinished `begin` record identifies a traversal that
did not return, without establishing why it stopped.

Run `python3 tests/test_nested_followers.py` for empty, direct and three-level
parties, repeated traversal, node cleanup and log rotation. For the native Linux
regression, build with `python3 tools/build-companion-arachnid-check.py`, then run
`build/check-companion-arachnid MARKED_COPY/game SLOT`. The copy must contain
`.map-export-sandbox` and a copied save with an active companion. The check
selects the companion, binds a mechanized arachnid to slot 1, activates the
shortcut and runs the native throw animation and deployment script. It checks
the resulting hierarchy, repeated party queries and save/load through
`SlotArac` in that copy. Its address space is capped after loading so an old
engine's runaway traversal cannot exhaust the host's memory.

## Follow-camera keyboard routing

`ArcanumModern/follow-input.log` records movement routing changes (`route`),
native path submission (`path`) and failed path searches (`blocked`). It rotates
at 1 MiB to `follow-input.previous.log`; logging failure does not block input.
Records contain milliseconds since SDL startup, runtime actor and hero handles,
direction, held/tapped direction masks (up=1, left=2, down=4, right=8), and path
submission outcome (1=accepted, 0=failed; not applicable for routing events).
They contain no actor names, positions or serialized save data. An accepted path
is not proof of completed movement.

Run `python3 tests/test_follow_input.py` for selection changes during held input,
companion release, invalid selection, modal gating, path failure and log rotation.
For a live Linux/X11 check, build with `python3 tools/build-follow-input-check.py`
after `./build.sh`. Run `build/check-follow-input GAME --fresh` only on an
isolated copy containing `.map-export-sandbox`. The check creates a temporary
party in the game map, identifies its process/window, focuses that window and
uses XTest to hold/release movement keys across selection changes and a pause
menu transition. It requires X11/XWayland and libXtst, writes a screenshot and
diagnostics inside the isolated copy, and does not save the party.

## Distant manual control

The selected companion remains exempt from the normal 30-tile follower catchup
and from the out-of-range AI animation interruption. Manual selection retains
normal nearby AI tick frequency. Ending selection, selecting another companion,
or regrouping applies the usual catchup check immediately; combat, death,
inactivity, paralysis and the native Wait Here flag still prevent recall.
Free-camera scrolling uses the selected companion's location and perception.
When explicit control release recalls a distant companion, the camera centers
on the current controlled actor (the hero on deselection), including free mode.
Automatic catchup and releases without recall do not move the camera.

`ArcanumModern/party-distance.log` records `manual_remote` (at most once per five
seconds) and `catchup` with SDL uptime, actor handle and distance in tiles. It
rotates at 1 MiB to `party-distance.previous.log`. It excludes names and save
contents. `release_camera_centered` / `release_camera_failed` record the immediate
camera outcome after explicit recall. `catchup` records a native relocation request, not a save operation.
Run `python3 tests/test_party_distance.py` and `tests/test_party_grouping.py` for
AI guards, release callbacks and log rotation. The live `check-follow-input`
fixture additionally tests 45-tile separation, AI processing without recall,
free-camera scrolling in all four cardinal directions and recall on deselection.

## Camera after dialogue

Dialogue exit restores the selected companion's camera position in both free
and follow camera modes, falling back to the hero when no companion is selected.
`ArcanumModern/dialog-camera.log` records SDL uptime, runtime actor/hero/selection
handles and whether the target tile is centered after the request. It rotates
at 1 MiB to `dialog-camera.previous.log`; it excludes names and save contents.
Logging failure does not prevent camera restoration. Run
`python3 tests/test_dialog_camera.py` for target selection and log rotation. The
live `check-follow-input GAME --fresh` fixture also opens and closes the native
dialogue interface in both camera modes with the companion 45 tiles away,
checking camera position and selection immediately after closing.
