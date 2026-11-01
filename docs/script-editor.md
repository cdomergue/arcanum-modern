# Script editing

Script Editor Renaissance edits Arcanum scripts and conversations without opening
a map. It runs on Windows and Linux and shares its script editor and compiler with
WorldED. New scripts use structured `.wscript` source. Existing `.scr` files open
as native instructions, with their condition, then action and else action visible
on each row.

## Start a project

On Windows, extract `worlded-renaissance-win.zip` and run `script-editor.exe`
from the same folder as `worlded-explorer.exe`. The archive contains both editors,
their shared runtime and this guide. Keep the libraries and plugin folders beside
the executables. You can launch the script editor without opening WorldED.

On Linux, build the `script-editor` target with the instructions below and run
it from the build folder. It uses the Qt libraries installed for that build.

Choose **File > Open project folder** for a folder containing your scripts and
dialogues. **Choose Arcanum installation** adds campaign resources to the library
and loads the game's operation descriptions. The installation is remembered.
Opening a game resource creates a copy that needs a destination when saved;
the editor does not write back into an archive.

Use the library filter to find a filename or resource ID. Double-click to open
it in a tab. You can work on several scripts and dialogues together.

In WorldED, **Resources > Script editor** opens the standalone application.
WorldED's own script dialogs use the same editor. Save and close a project before
editing its files in the other application; open tabs are not synchronized.

## Write a structured script

Choose **File > New structured script**. Statements end with a semicolon; blocks
use braces. For example:

```text
let total = 0;
for (let i = 0; i < 5; i++) {
    if (i == 2) {
        continue;
    }
    total += i;
}
return default;
```

Press **F7** to compile. Errors include a source line and column. The compiled
instructions tab lets you inspect the result. **Ctrl+Space** lists completions.

The language supports `if/else`, `while`, `for`, `break`, `continue`, named labels,
`goto`, comparisons, `&&`, `||`, `!`, addition, subtraction and multiplication.
Use `//` for a comment. Integer values are signed 32-bit numbers; hexadecimal
constants use `0x`. Division is rejected because the pinned engine implements
its division instruction incorrectly.

Native actions and conditions accept their full `SAT_…` or `SCT_…` name, or the
lowercase suffix. Actions are statements; conditions belong in `if` and loops.
Use structured loops and `goto label;` for control flow. The compiler manages
instruction numbers.

| Expression | Meaning |
| --- | --- |
| `let count = 0;` | Named numeric local |
| `global(1900)` | Campaign global variable; valid indices 0–1999 |
| `flag(3000)` | Campaign global flag; valid indices 0–3199 |
| `counter(0)` | One of the script's four counters |
| `pc_var(player, 8)` | Per-character variable with an explicit object focus |
| `pc_flag(self, 2)` | Per-character flag with an explicit object focus |
| `player`, `self`, `attachee`, `triggerer`, `current`, `extra` | Object context |
| `object(11, 0)` | Indexed object focus; here, local object zero |
| `raw(type, value)` | Explicit native operand for experienced authors |

Reserve global and flag indices for your project before using them. Example
indices in this guide are not allocations. The engine has ten numeric local
slots, shared by named variables and expression temporaries. Simplify an
expression if compilation runs out of slots. Keep loops short: the runtime
limits an invocation to 1,000 executed instructions.

`return;` skips the default event behavior. `return default;` allows it. Choose
according to the event you attach the script to.

Saving a structured script keeps the `.wscript` and writes a companion `.scr`.
That companion also holds the script properties. Keep both files together.
Use **Build project** after changing source used by a dialogue so both compiled
resources use the same instruction numbers.

## Edit a native script

Open a `.scr` file. Double-click its condition, then action or else action to
choose an operation and edit its operands. The picker lists all 53 conditions
or 142 actions, with a search field. Operand controls distinguish numbers and
variable indices from object focuses. PC variables and flags have a character
focus selector.

**Add action** and **Add condition** insert after the selected row. **Duplicate**
copies it. **Up**, **Down** and **Delete** change the instruction order. Undo and
redo restore native instruction edits. Constant jumps within this script follow
their destination when rows move. Deleting a destination still referenced by a
jump is rejected; change the jump first.

Review computed jumps, incoming calls from other scripts and numeric dialogue
targets after inserting, deleting or moving rows. Those references cannot all
be resolved from the current document. Unknown opcodes, unknown operand types
and unused binary fields are retained. A native file is not converted into
structured source on opening.

**Script > Properties** edits the description, four counters, 32 local flags and
trap, speech, radius and teleport flags. The description must fit 39
Windows-1252 bytes when changed. Local flags and behavior flags are separate
fields. Untouched unknown bits remain intact.

## Write a conversation

Choose **File > New dialogue**, or open a `.dlg` or `.wdlg`. The table contains
the native seven fields: line ID, text, alternate text, intelligence requirement,
condition, destination and effect. Use the editor's validation and preview to
check the conversation. Preview follows text and links; it does not execute
conditions or effects. These fields use Arcanum's native dialogue syntax.

Give the dialogue and its companion script the same filename stem. For example,
`01999.wdlg` and `01999.wscript` belong together. **Open companion** finds the
matching file in the same folder or a WorldED project's source/export folders.

A positive destination selects another dialogue line; zero ends the conversation.
A negative destination resumes a numbered script instruction. To keep that last
kind of destination stable while editing structured source, use a named label:

```text
dialog(1);
label reward:
global(1900) = 1;
return;
```

Set the dialogue response's **Target** to `@reward`. Build the project to
resolve it into the native negative instruction number. Labels are case-sensitive.
A missing label stops compilation. A label at instruction zero cannot be used:
zero means “end conversation” in a DLG. Put an initial instruction before the
entry point, as in this example.

Save named destinations in `.wdlg` source. Exported `.dlg` files contain numbers;
keep the source for later edits. When saving a native dialogue containing a named
destination, the editor asks for a `.wdlg` destination so the label is retained.
Text must fit Windows-1252. The game supports at most five player responses at
a time.

## Build and use the resources

Save all open documents, then choose **Script > Build project**. Select a new or
empty export folder **outside the source project**. The editor compiles structured
scripts, resolves dialogue labels and retains native-only scripts and dialogues.
It writes `scr/` and `dlg/` directories. Duplicate source names or duplicate
native resources stop the build. A source file takes precedence over its compiled
companion. Building never overwrites a populated export folder.

For one document, **Export compiled file** writes a `.scr` or `.dlg` at a chosen
path. Exporting one script does not rebuild a linked dialogue. Prefer a project
build when they share named destinations.

WorldED stores structured scripts and dialogue sources in `script-sources/`, and
compiled resources in `exports/scr/` and `exports/dlg/`. Applying a structured
script in WorldED also recompiles a matching project `.wdlg` when present. The
files can be included in housing and campaign-update exports. Attach the script
to the intended object or event; a resource file alone does not attach itself.

Test in a separate game copy. Check the event, conversation branches and effects
in the game, including repeat visits and a saved game. Compilation checks data
and references; it cannot establish that a quest behaves as intended.

## Example: an unlocked chest that awards 50 coins once

This script belongs on **SAP_USE** of a new, unlocked container. It awards the
player 50 coins on first use, remembers that reward in counter 0, and allows the
normal container interaction. Each attachment has its own counter; two chests
using this script can each award one payment.

```text
if (counter(0) == 0) {
    counter(0) = 1;
    adjust_gold(player, 50);
}
return default;
```

The complete source is
[49000_reward_chest.wscript](../examples/scripts/49000_reward_chest.wscript).
Choose an unused ID before adding it to your mod; 49000 is an example, not a
reserved allocation.

In WorldED, select the chest, create the script through **Resources > New
script…**, paste the source, and Validate. Enable **Attach to selected object
on apply**, choose **SAP_USE**, then apply and save the zone. Check that counter
0 is initially zero on the attachment. Use **Load defaults from SCR** only when
you intend to reset its initial flags and counters.

In a disposable game copy, note the player's gold and open the chest. Expect
50 more coins and an open inventory. Open it again: gold must stay unchanged.
Save, leave the area, return and reload; the reward must still be consumed.
Restoring a copy of a save from before first use lets you repeat that test.

`return default;` matters here. Replacing it with `return;` suppresses the normal
container interaction. An attachment on **SAP_EXAMINE** would run on examining
the chest rather than using it. Neither recompiling nor replacing the SCR resets
the counter already recorded in a saved game.

## The same chest in the native editor

Choose **File > New native script** in Script Editor. Press **Add action** four
times so rows 0 through 3 exist before entering their jump targets. Double-click
each cell to enter the following operations. Search by the enum name if your
installation displays a translated description.

| Row | Condition | Then action | Else action |
| --- | --- | --- | --- |
| 0 | `SCT_EQ`: Counter 0 equals Number 0 | `SAT_GOTO`: Number 1 | `SAT_GOTO`: Number 3 |
| 1 | `SCT_TRUE` | `SAT_ASSIGN_NUM`: Counter 0 = Number 1 | `SAT_DO_NOTHING` |
| 2 | `SCT_TRUE` | `SAT_ADJUST_GOLD`: Player, Number 50 | `SAT_DO_NOTHING` |
| 3 | `SCT_TRUE` | `SAT_RETURN_AND_RUN_DEFAULT` | `SAT_DO_NOTHING` |

For a **Counter** operand, enter its index, **0**, in the adjacent value field.
For a **Number** operand, enter the literal value. On row 2, choose **Player**
as the object focus; its index value can remain zero. Leave all four counters
at zero in **Properties**. Row numbers in this table are zero-based, as in the
editor's **Line** column.

On first use, row 0 selects row 1, which marks the reward consumed; row 2 grants
the coins. On subsequent uses, row 0 jumps directly to row 3. Both paths allow
the normal chest behavior. This is the same behavior as the structured example,
although the compiler may generate a different number of instructions.

Validate and save as an unused numeric resource name, for example
`49002_native_reward_chest.scr`. To add it to a WorldED scene, save and close
the scene first, place the SCR in its `exports/scr/` folder, then reopen the scene.
Find it in **Resources > Scripts & dialogues…**, attach it to the chest's
**SAP_USE** event and save. Use this native script **or** the structured chest
script on that event; they are two ways to implement the same behavior.

[reward_chest.native.txt](../examples/scripts/reward_chest.native.txt) contains
the native listing for comparison. It is a reference for the table above, not
`.wscript` source. The native listing tab is read-only; edit through the
instruction cells.

## Example: a payment offered in a conversation

Create a project NPC and attach this script to **SAP_DIALOG**:

```text
dialog(1);
return;

label reward:
if (counter(0) == 0) {
    counter(0) = 1;
    adjust_gold(player, 50);
}
return;
```

Create a matching dialogue with these three rows. Leave conditions and effects
blank. On the NPC line, the alternate text may repeat the same sentence.

| ID | Speaker | Text | Intelligence | Target |
| --- | --- | --- | --- | --- |
| 1 | NPC | I have fifty coins set aside for you. | Blank | 0 |
| 2 | Player response | I will collect them now. | 1 | `@reward` |
| 3 | Player response | I will come back later. | 1 | 0 |

The supplied
[49001_reward_keeper.wscript](../examples/scripts/49001_reward_keeper.wscript)
and [49001_reward_keeper.wdlg](../examples/scripts/49001_reward_keeper.wdlg)
contain this example. Copy both into a working folder and use **Open project
folder**, then **Build project**. Use the same filename stem for both if you
rename them. The native output is a SCR/DLG pair; keep the WScript/WDlg sources.

In WorldED, apply the script and dialogue under the same resource name, attach
the script to the NPC's **SAP_DIALOG** event and save the scene. If importing
the supplied pair into a saved scene, close it first, put the source files under
`script-sources/`, and copy the compiled SCR and DLG into `exports/scr/` and
`exports/dlg/`. Reopen the scene before attaching or exporting them.

The first two compiled instructions are the dialogue call and the return, so
`reward` resolves to instruction 2 in this example. The exported response target
is therefore **-2**. A native-only conversation can use -2 directly when its
companion SCR has that entry point, but you must update the number if rows move.
Keeping `@reward` in `.wdlg` source lets a project build recalculate it.

Test “come back later” first: the conversation should close without a payment.
Talk again and collect it: gold should rise by 50. Talk and collect a second
time, then save/reload and repeat: no further payment should be granted. The
response remains visible in this example; the counter prevents repeat rewards.
There is no quest-completion check in this script. Add the intended native quest
condition before using it as a real quest reward.

## Command line and builds

The command-line compiler uses the same code as the editors and needs no display
server. On Windows use `script-editor.exe` in these commands:

```sh
./script-editor --compile 01999.wscript --output 01999.scr
./script-editor --compile-project ./my-project --output ./compiled-project
./script-editor --validate 01999.scr
```

Single-file compilation creates default script properties. A project build reads
properties from the source's companion SCR, or `exports/scr/` in a WorldED project.
Validation reports unknown operations and references that need review. Keep the
source files and properties under version control together.

Follow [Building WorldED](worlded-build.md) to configure Qt, zlib and CMake on
Windows or Linux. The same CMake project provides `script-editor` and
`script-editor-check`; the standalone editor needs no engine build or map.

```sh
cmake --build build/worlded-linux --target script-editor script-editor-check
ctest --test-dir build/worlded-linux -R script_editor --output-on-failure
```

On Windows add `--config Release` to the build and `-C Release` to CTest.
The script editor and its guide are included in `worlded-renaissance-win.zip`.
Follow the Windows packaging steps in [Building WorldED](worlded-build.md) to
produce that shared archive. The packager checks executable startup and the Qt
platform plugin without the SDK on PATH.
