# Unofficial Arcanum Patch

[Back to the README](../README.md)

Arcanum Renaissance can use an English installation with
[Unofficial Arcanum Patch 2.0.2](https://terra-arcanum.com/drog/uap.html).
The UAP is optional; an unmodified Arcanum 1.0.7.4 installation also works.

## Installation

1. Install the English edition of Arcanum.
2. Install UAP 2.0.2 into that installation and select the components you want.
3. Apply any desired UAP EnginePatcher options to the original `Arcanum.exe`.
4. Install Renaissance into the same game folder.
5. Start a new game. The UAP requires a new campaign rather than a vanilla save.

For macOS or Linux, copy the complete patched game folder from Windows, then
extract the corresponding Renaissance package into it. Keep the archives,
module patches, loose data and original `Arcanum.exe` together.

Renaissance reads the installed files. It does not download, install or
redistribute the UAP, and its packages contain no original game data.

## Components

| Component | Behavior in Renaissance |
| --- | --- |
| UAP Base | Loads the patched game data, additional animation directions, ambient tracks, music and town maps. |
| Race Mod | Offers Dark Elves, Ogres and Orcs, their portraits, backgrounds, dialogue data and additional pregenerated characters. |
| Official DLC | Adds the installed campaigns to the module selector. Each campaign uses its own maps and scripts. |
| Beta Content | Loads the selected restored artwork, creatures and portal data. Renaissance retains its own redesigned screens when Beta UI is installed. |
| Misc Mods | Loads the selected Virgil debug dialogue, Pyrotechnic Bow, Riddlers, logo skipping and sound suppression archives. |
| High Resolution Patch | Renaissance manages its own display and interface settings. Its engine can read an installation containing the UAP High Resolution Patch's data. Apply the UAP utility only to the original `Arcanum.exe`. |

HD local maps are prepared from the installed modules and patches. Each
additional campaign needs its own cache space. Preparation can be resumed,
and the original maps remain available through **Play now**.

## EnginePatcher options

These options are detected from the original UAP-patched `Arcanum.exe` and
applied by Renaissance on Windows, macOS and Linux:

| Option | Effect |
| --- | --- |
| LevelCap100 | Uses the extended level progression. |
| MasterEducator | Allows the hero to teach followers up to the hero's own training rank, without requiring the Educator background. |
| NoCombatMusic | Keeps the area's music during combat. |
| NoXPPerHit | Awards combat experience on kills rather than individual hits. |

Restart Renaissance after applying an option. Keep `Arcanum.exe` in the game
folder: the presence of an EnginePatcher script alone does not enable its option.
Without a recognized patched executable, these settings retain Arcanum's normal
behavior.

Renaissance supplies its own interface and combat controls. UAP changes embedded
only in the original executable are not automatically inherited; the table above
identifies the supported optional executable patches. Other UAP releases and
third-party executable patches require separate validation.
