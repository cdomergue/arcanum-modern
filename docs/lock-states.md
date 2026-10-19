# Scripted locks

The original WorldEd contract defines three states: 0 = temporary unlock,
1 = lock, 2 = permanent unlock. See the
[original editor manual](https://terra-arcanum.com/amtut/resources/worlded_manual.htm),
“lock state of (obj): set to (num)”.

`object_locked_set` accepts an integer so state 2 survives the script call.
Permanent unlock clears `LOCKED` and sets `NEVER_LOCKED` for both portals and
containers. Temporary unlock retains the one-hour relock timer. A previously
queued relock may clear a jam, but cannot close a permanently unlocked object.
Other object flags, quest requirements and the original scripts are preserved.

The installed Arcanum data has five scripts using state 2:

- 01060: Bates guard dialogue.
- 01149: secret tomb door.
- 02059: orc uprising door heartbeat.
- 02235: Bates guard heartbeat.
- 02900: prison warden.

Existing saves can already contain a locked door after the one-shot unlock
script has run. Updating the executable does not replay completed scripts.
For the affected Bates save, the repair replays the original guard dialogue
script at line 62 on a copied save, after checking quest 1040 is completed,
global access flag 1024 is set and exactly one matching guard and gate exist.
It saves into a separate slot; original saves remain untouched.

## Verification

`python3 tests/test_lock_states.py` compiles the actual C functions and checks
both object types, states 0/1/2, pending timers and non-lockable targets.

The integration harness requires an isolated game copy with the marker
`.map-export-sandbox`. Never add that marker to the installed game folder.

```sh
python3 tools/build-map-exporter.py --source tools/check-bates-lock.c --name check-bates-lock --exclude-object src/ui/modern_ui.c.o
build/macos-arm64/check-bates-lock "$PWD/build/map-preview" SlotAuto Slot0003
```

Pass `-` as the last argument for verification without creating a repaired slot.
A created slot is reloaded to verify that permanent unlock, quest completion
and access authorization survive saving. The test does not simulate a full
playthrough of the other quests.
