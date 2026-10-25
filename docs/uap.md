# Unofficial Arcanum Patch

[Back to the README](../README.md)

Arcanum Renaissance can use an English installation with
[Unofficial Arcanum Patch 2.0.2](https://terra-arcanum.com/drog/uap.html).
The UAP is optional; an unmodified Arcanum 1.0.7.4 installation also works.

## Automatic installation

On eligible English installations without UAP, Renaissance offers to download
and prepare **UAP 2.0.2 Base + Race Mod** at startup. **Yes** downloads the official
installer (about 485 MB), verifies its checksum, extracts the selected data and
backs up existing saves before activation. English text is used directly, with
no French translation. Allow about **2 GB of temporary space**, plus the save
backup. Automatic preparation is available on Windows 10+, Linux x64 and Apple
Silicon Macs with macOS 13+.

**No** continues without UAP and remembers the answer. Delete
`modern-ui/uap-en/choice` to display the offer again. The installer checks the
shared original art archives, required game archives and English interface text;
it skips unrecognized installations and additional gameplay patches. An existing
`Arcanum5.dat` installation or completed Renaissance UAP overlay is kept.

Original archives remain unchanged. Prepared English data lives in
`modern-ui/uap-en/active`; the log is `modern-ui/uap-en/preparation.log`.
Only a completed installation is activated. HD maps need preparation again when
the active game data changes. Release packages include no UAP or original game data.

**Start a new campaign after adding UAP.** Existing saves may have incompatible
quest, script or map state. Before activation, saves are copied to a verified ZIP
under `save-backups/before-uap-…/saves.zip`, with checksums and restoration
instructions. If backup fails, installation remains inactive. To restore, close
the game, rename the `active` directory and copy the backed-up saves to their
original paths, keeping newer saves separately. Saves created with UAP may still
require UAP data.

The [French preparation](uap-fr.md) follows the same process and also adapts text
from the installed French game.

## Manual installation and optional components

1. Install the English edition of Arcanum.
2. Install UAP 2.0.2 into that installation and select the components you want.
3. Apply any desired UAP EnginePatcher options to the original `Arcanum.exe`.
4. Install Renaissance into the same game folder.
5. Start a new game.

For macOS or Linux, copy the complete patched game folder from Windows, then
extract the corresponding Renaissance package into it. Keep the archives,
module patches, loose data and original `Arcanum.exe` together.

Automatic preparation selects Base + Race Mod. Use manual installation for
Beta Content, additional campaigns or optional EnginePatcher switches.

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
