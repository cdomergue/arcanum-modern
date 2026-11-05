# Custom backgrounds

Choose a background on the first character creation screen and select
**Customize**. Customizing an original creates a separate copy. Customizing one
of your own entries edits that entry in place. **Apply** saves the definition and
selects it; **Cancel** discards the current draft.

The background arrows include each original followed by its custom copies.
Definitions remain available after restarting Renaissance. Editing a definition
does not change characters in existing saved games.

## Name and personal history

Enter a name and write directly in the bordered history field. A copy starts with
the original's name and description; replace them as needed. The selected name
appears during creation, and the personal history appears in the journal.
Text fields support Ctrl+A, Ctrl+V and Backspace. The history field also accepts
line breaks and scrolls through longer text.

## Attributes and equipment

Use the attribute controls to adjust the character's initial values. There is no
point budget or requirement to balance bonuses with penalties. Arcanum's minimum
and maximum values still apply, including racial limits.

The derived attributes tab previews the engine's calculated values. Speed and
maximum followers also have independent bonus and penalty controls: original
backgrounds can modify these directly. The other derived values follow the
game's formulas. Independent controls require the expected original effects to
be available in the installed game data.

In the equipment tab, search for an item and choose its quantity. These items
are added to the character's usual starting equipment. Set starting money to
replace the usual amount; leave it empty to inherit the background's money, or
enter **0** to start without money. Items come from the original game's prototype
ranges, using the installed names and definitions.

A copy retains its original's special rules and race and gender eligibility.
For a background without inherited special rules, customize **No significant
background**. Changing race or gender returns to the native background selection;
choose an eligible custom entry again with the arrows.

Returning from the character sheet to the first creation page preserves the
background draft. Points allocated on the character sheet are reset so they can
be assigned again. Returning to the starting shop rebuilds starting equipment;
it does not repeatedly add the custom kit.

## Saved games and backups

Characters use an existing background identifier, native attributes, original
effects, money and items. The editor adds no new background, effect, script or
prototype identifier to a saved character. It does not reapply starting bonuses
or equipment when a saved game is loaded.

The classic executable displays the original background's name and description.
Renaissance's custom name and history are optional local annotations. Use the
same game resources on both installations: a campaign created with UAP still
requires the corresponding UAP resources. The editor does not remove dependencies
introduced by other mods.

Back up these files alongside your saved games:

- `modern-ui/backgrounds.dat`: the reusable custom background definitions.
- `modern-ui/background-*.txt`: each character's custom name and history.

Copy these files when moving a Renaissance installation if you want to keep the
custom list and prose. They are not required by the classic executable. Close
the game before replacing them.

## Development checks

The Python test harnesses compile the production C routines with mocked engine
boundaries. They require a C compiler; set `CC` if it is not named `cc`.

```sh
python tests/test_background_choices.py
python tests/test_background_creation.py
python tests/test_background_effects.py
python tests/test_background_loadout.py
python tests/test_background_name.py
python tests/test_background_profile.py
python tests/test_creation_draft.py
python tests/test_creation_navigation.py
python tests/test_keyboard_capture.py
```

For manual validation, use disposable installations. Create a copy with attribute
and derived adjustments, money and equipment; edit it again and verify that the
list still contains one copy. Check both arrows, race and gender changes, Back,
the starting shop, restarting the game and loading a saved character. Load that
save with the classic executable and matching game resources, then save there
and load it again in Renaissance. Never use personal saves for these checks.
