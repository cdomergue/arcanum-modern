# French UAP installation

Renaissance can install **UAP 2.0.2 Base + Race Mod** over recognized original
French 1.0.7.4 game files. A **Yes / No** prompt appears before the game loads.
This is an experimental French adaptation, separate from the official English
[Unofficial Arcanum Patch](https://terra-arcanum.com/drog/uap.html).

**Yes** downloads the official installer (about 485 MB), verifies its checksum,
extracts the selected data and prepares French text. The game keeps UAP scripts,
quest conditions, item and character data, race rules, animations and audio.
French dialogue conditions and actions use the UAP versions; translated text is
assembled from your installed French game and the adaptation's translations.
The original Windows executable and the UAP interface are not installed.

**No** keeps the game without this UAP adaptation and remembers your answer.
To display the offer again, close the game and delete
`modern-ui/uap-fr/choice` inside the game folder.

Automatic preparation is available on Windows 10 or later, Linux x64, and
Apple Silicon Macs with macOS 13 or later. The game itself can run without the
automatic installer on macOS 11 and 12. Allow about **2 GB of temporary space**,
plus space for your saved-game backup. HD maps are generated from the active
UAP data and need preparation when the installed data changes.

## Existing campaigns and backups

**Existing saves are not guaranteed to be compatible with UAP.** Changed scripts,
quests, objects and rules can cause inconsistencies or block progression. The
installer allows existing campaigns after the warning. If a sector's saved
objects require its original map data, Renaissance uses that sector's original
base while retaining the saved changes. UAP map edits for that sector do not
apply. This does not resolve every possible quest or script incompatibility.

Before activation, all existing saved games are copied into a separate ZIP under
`save-backups/before-uap-fr-…/saves.zip`. The backup is verified against the
original files and never replaces an older backup. If backup or verification
fails, the UAP overlay is not activated. Each ZIP contains a checksum manifest
and restoration instructions.

Original game archives remain unchanged. The UAP data lives in
`modern-ui/uap-fr/active`. Incomplete preparation stays inactive.
The preparation log is `modern-ui/uap-fr/preparation.log`.

To return to the original data, close the game and rename the `active` folder.
Keep newer saves separately before restoring files from the backup to their
matching paths in the game folder. A save created with UAP may require its UAP
data; disabling the overlay does not make that save compatible with the original
game.

## Scope

The automatic offer requires matching French archive fingerprints. It is skipped
for English installations, existing UAP installations and unrecognized data sets.
The English UAP's optional campaigns and other components use the
[manual UAP installation](uap.md). The French installer selects Base + Race Mod;
it does not install Beta Content or optional EnginePatcher switches.

Renaissance runs its own engine. Installing UAP data does not execute or import
binary patches for the original `Arcanum.exe`. The supported optional engine
settings are described in [UAP installation and options](uap.md).
