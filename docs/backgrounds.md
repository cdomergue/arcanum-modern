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

The **Effects** tab provides controls for speed, maximum followers, alignment,
magical and technological aptitude, health, fatigue,
reactions, resistances, skills, technological expertise, experience gain and
critical hits and failures. These are persistent modifiers, rather than points
spent on the character sheet. The displayed adjustments are your additions;
inherited effects and the game's formulas also contribute to the final values.

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

## Advanced native effects

The **Advanced** tab accepts the expressions used inside a native `effect.mes`
record. Enter only the expressions, without a `{number}{...}` wrapper. Separate
them with commas or line breaks. **View original** shows the numeric effects of
the selected original background.

Choose whether to keep or replace the original numeric effects. Keeping them
adds your expressions to them. Replacing them removes those numeric effects;
it does not remove special rules implemented by the game for that background,
such as Educator or Day Mage. Start from **No significant background** when you
do not want inherited special rules.

For example, these expressions reproduce the numeric effects of Sold Your Soul
(Pacte diabolique in French):

```text
magicpts +4
reaction -20
alignment -200
alignment max -200
```

The alignment starts at -20 and cannot rise above -20, but can fall lower.
`reaction` changes general reactions, not just reactions from a particular social
group. To replace the effects of that background with this example, select
**Original effects: replaced** so the bonuses and penalties are not added twice.

| Operation | Example | Meaning |
| --- | --- | --- |
| Addition or subtraction | `reaction -20` | Subtract 20 from general reaction |
| Percentage | `maxhps +10%` | Increase maximum health by 10% |
| Multiplication | `carry *2` | Multiply carrying capacity by 2 |
| Division | `xpgain /2` | Divide experience gain by 2 |
| Minimum | `alignment min -200` | Prevent alignment falling below -20 |
| Maximum | `alignment max -200` | Prevent alignment rising above -20 |

The engine combines active effects by operation: percentage, multiplication,
division, addition, minimum, then maximum. Line order does not change that order.
Native units do not always match displayed units: alignment uses 10 units per
displayed point, skills use 4 units per full rank, and each magic point contributes
5 points of magical aptitude before other rules and limits apply.

The following native keys are available:

| Group | Keys |
| --- | --- |
| Primary attributes | `st`, `dx`, `cn`, `be`, `in`, `pe`, `wp`, `ch` |
| Derived attributes | `carry`, `damage`, `ac`, `speed`, `healrate`, `poisonrate`, `beautyreaction`, `maxfollowers`, `age` |
| Alignment and aptitude | `alignment`, `aptitude`, `magicpts`, `techpts` |
| Health and fatigue | `maxhps`, `maxfatigue` |
| Reactions | `reaction`, `goodreactionadj`, `badreactionadj` |
| Critical hits and failures | `crithitchance`, `crithiteffect`, `critfailchance`, `critfaileffect` |
| Experience | `xpgain` |
| Resistances | `resistdamage`, `resistfire`, `resistelectrical`, `resistpoison`, `resistmagic` |
| Combat skills | `bow`, `dodge`, `melee`, `throwing` |
| Thieving skills | `backstab`, `pickpocket`, `prowling`, `spottrap` |
| Social skills | `gambling`, `haggle`, `heal`, `persuasion` |
| Technological skills | `repair`, `firearms`, `picklock`, `armtrap` |
| Expertise | `expertiseanatomical`, `expertisechemistry`, `expertiseelectric`, `expertiseexplosives`, `expertisegun_smithy`, `expertisemechanical`, `expertisesmithy`, `expertisetherapeutics` |

The editor does not accept race, gender, level, current poison, fate, unspent
points or the unused experience-total field as background modifiers. It cannot
create scripts, new spells or new background-specific rules.

Invalid expressions disable **Apply** and show the line and reason. Text is
limited to 2,047 bytes. Values range from -10,000 to 10,000; percentages from
-99% to +1,000%; multipliers from 0 to 10; divisors from 1 to 10. The editor packs
expressions into native effect records, with at most five changes per group in
each record and at most 16 records per definition. Available effect identifiers
also depend on the installed game resources.

## Saved games and backups

Characters retain their original background identifier. Attribute changes,
effects, money and equipment are saved by the game; starting bonuses and items
are not reapplied when loading. Editing a reusable definition leaves existing
characters unchanged.

Additional numeric effects use native effect identifiers. Renaissance stores
new definitions in `modern-ui/background-effects.mes`, without changing the
installed `rules/effect.mes`. Published records are retained so older saves keep
their effects. If no unused identifier remains, the editor refuses the new
effect instead of replacing an existing record.

Back up these files alongside your saved games:

- `modern-ui/backgrounds.dat`: the reusable custom background definitions.
- `modern-ui/background-*.txt`: each character's custom name and history.
- `modern-ui/background-effects.mes`: additional effects referenced by characters
  and reusable definitions. This file is required by saves using those effects.

Copy all three when moving an installation. Keep the same underlying game and
mod resources, and close the game before replacing files. Do not delete or
renumber effect records that saved characters may reference. Renaissance rejects
a conflicting effect resource or a save with missing background effects.

The classic executable does not read Renaissance's additional effect file. A
save using those effects requires the same records at the same identifiers in
its native `rules/effect.mes`, without conflicts with other installed resources.
Do not assume such a save can be loaded unchanged in the classic game. Custom
names and personal histories remain Renaissance annotations; the classic game
shows the original background's name and description.

## Development checks

The Python test harnesses compile the production C routines with mocked engine
boundaries. They require a C compiler; set `CC` if it is not named `cc`.

```sh
python tests/test_background_rules.py
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
save with the classic executable only after supplying matching effect records
and game resources, then save there and load it again in Renaissance. Never use personal saves for these checks.
