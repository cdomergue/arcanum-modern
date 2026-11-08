# Arcanum save editor

Open `index.html` in a recent browser. No server, account, dependency installation
or network access is needed. Keep the JS and CSS files beside the HTML file.

To build a **standalone HTML file**, run this from the repository root:

```sh
python3 tools/build-save-editor.py
```

Then open `dist/arcanum-save-editor.html`. The mod's fonts are embedded if they
are available when the file is built.

To include localized names from your own installation, add
`--mes-directory /path/to/mes` to the command. All three files — `stat.mes`,
`skill.mes` and `spell.mes` — are required. Only the relevant names are embedded
in the generated HTML under `dist/`, which remains excluded from Git. Do not
add extracted game text to the repository. The editor's interface is in English;
imported game labels retain their original language.

## Edit your character

In **Character**, enter a name or choose a race and gender. Names support up to
23 game characters, including accents. Import the `.gsi` file with the
`.tfai` / `.tfaf` pair to update the name shown in the **Load** menu. Without
that file, the new name is stored in the saved game and the menu updates the
next time you save in game.

Select **My game uses the UAP Race Mod** if that component is installed. It
provides additional races and changes the half-elf body model. The editor checks
which race and gender combinations have playable models. It adjusts the body,
voice, and race and gender effects. The character's portrait and background
are preserved.

## Usage

1. Back up the files from the save slot you want to keep.
2. Select `Slot….tfai` and `Slot….tfaf` together from
   `modules/Arcanum/Save/`. Their names must match. Optionally include the `.gsi`
   and `.bmp` files from the same slot.
3. Edit the values and review your changes.
4. Export the ZIP, close the game, and extract the files into the `Save` folder,
   preserving their names. If you did not import the `.gsi` and `.bmp` files,
   keep the originals from that slot. The game cannot open the ZIP itself
   as a save.

The editor never writes to the original files. Imports are limited to 128 MiB
in total. Changes do not persist after the page is closed.

## Fields and compatibility

- Base attributes, experience, alignment, fate points and available points.
- Bank balance if saved; gold and ammunition if the stack exists and is
  referenced by the character.
- All **16 skills**, with rank from 0 to 5 and training from 0 to 3.
- All **80 spells**, across 16 colleges: selecting a spell also learns earlier
  ranks; clearing it removes later ranks. A button learns all 80 spells.
  Missing skill and spell college entries can be added.
- Character selection when the save contains multiple player objects.
- Optional import of **`stat.mes`, `skill.mes` and `spell.mes` from your game's
  `mes` folder** for attribute, skill and spell names. Otherwise, engine
  identifiers are displayed. No extracted game text is tracked in the
  repository. Resources keep their identifiers.

Existing attributes and resources are edited in place. For skills and spells,
the editor can extend sparse arrays, add missing fields and update file sizes
in the index. Other bytes are preserved. TIG `.tfai`/`.tfaf` archives containing
version 119 mobile objects (`.mob`, `.mdy`) are supported. Unknown or inconsistent
formats are rejected. The BMP file remains unchanged. Only the character's name
can change in the GSI. The TIG format has no fingerprint that proves two files
with matching names and sizes belong to the same save: import an intact pair.

Quests, companions, full inventory items and level are not editable. Values
are those saved before equipment, race, background and magic modifiers. The
tool does not run all engine rules: it does not recalculate derived effects,
trigger a level-up from experience or apply bonuses for crossing attribute
thresholds. Editing bounds are technical limits; they do not guarantee a
character that follows every mod's rules. Unresolved resource stacks are
not invented.

Spells and skills are added without spending available points. Magic and
technology aptitude counters are adjusted by the difference, as they are when
points are added in the engine. Technology disciplines, college mastery and
unrelated skill bits are preserved. Game requirements still apply, and changing
the training degree does not complete master training quests.

## Verification and maintenance

```sh
node tests/test_save_editor.cjs
ARCANUM_SAVE_TEST_DIR=game/modules/Arcanum/Save node tests/test_save_editor.cjs
```

The second command reads existing saves and checks changes on copies in memory.
It does not write any save files. Tests also cover truncated archives, sparse
arrays, numeric bounds, accented characters, adding spells and skills, aptitude
counters, preservation of training bits, stack resolution and ZIP integrity
(using Python's independent `zipfile` reader).

`tools/check-save-editor.c` is an optional verifier to link against the engine's
object files. It loads a prepared save with all 80 spells and all 16 skills at
rank 5 / training 3, then checks those values through the game's accessors.
It requires an isolated test installation and the `.save-editor-sandbox` marker.

`schema.js` contains only format metadata generated by
`python3 tools/generate-save-schema.py` from the pinned engine revision.
References: `source/first_party/tig/src/file.c`,
`source/src/game/{obj,obj_private,sa,stat,item}.c` and their headers, in
[Arcanum CE](https://github.com/alexbatalov/arcanum-ce).
