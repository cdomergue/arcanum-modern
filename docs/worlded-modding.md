# Create a mod for Arcanum Renaissance

A mod needs authored content, an installation method and a test that exercises
what the player will do. Start with a small change that you can recognize in the
game. This walkthrough adds an unlocked chest that awards 50 coins on its first
use, then remains an ordinary storage chest.

You need an Arcanum installation, a Renaissance executable compatible with world
updates, WorldED, and Python 3 for the package installer. The Windows editor ZIP
contains WorldED, Script Editor, the installer and the example sources.

## Choose the export before building the mod

| What you want to distribute | Project and export | Existing saves |
| --- | --- | --- |
| New objects and supported changes to an existing zone | Scene project; **Export Renaissance update package…** | Applies supported operations to visited and unvisited sectors; conflicts are reported |
| A property sold by the estate agent | House scene, housing export and catalogue entry | Uses housing ownership and installation rules; follow the housing sections |
| Campaign settings, maps or replacements of shared resources | **Resources > Campaign…**, then **Export overrides…** | Test in an independent installation with a new game; replacement files do not migrate visited sectors or quest state |

The chest walkthrough uses a Renaissance update package. Existing NPC statistics,
inventories, schedules and script attachments are outside that exporter's supported
changes. A new container can carry its initial inventory and script attachment.
If the export review reports an unsupported edit, resolve it before distributing
the mod. A raw sector export is not a substitute for a save-compatible update.

## 1. Create the source project

Open the target installation in WorldED, choose a sector where the player can
reach the chest, and save the scene outside the installation. For example:

```text
D:/Mods/reward-chest/
    source/       WorldED scene project
    exports/      Versioned packages ready for installation
    tests/        Disposable game copies
```

The scene captures the original sector as its reference. Keep `reference.bin`
and `selections.bin` with `building.json` and the rest of the project. For an
older project without a reference, use **Zone > Update reference…** with the
installation from which it was created. Do not adopt a different release just
to dismiss a mismatch.

## 2. Add the chest and its behavior

Place a **new container** from the palette. Use an unlocked prototype or clear
the lock in its properties. Put it on a reachable cell and keep its approach
clear. An empty chest makes the first test easier to read.

Select the chest and choose **Resources > New script…**. Pick an unused numeric
ID and a name such as `reward_chest`. Check the script library and any installed
mods before reserving an ID; 49000 in the example is not guaranteed to be free.

Paste the complete reward-chest example from the scripts section, then Validate.
Enable **Attach to selected object on apply**, choose **SAP_USE**, and apply the
script to the project. Check that the attachment's counter 0 starts at zero.
Save the scene. Creating a script file by itself does not attach it to the chest.

If you work in Script Editor separately, save the WorldED scene first, edit its
matching source under `script-sources/`, then reopen the scene and apply the
script in WorldED. Do not edit the same project simultaneously in both windows.
The scene's compiled resources must reflect the source before export.

## 3. Try the scene in a disposable copy

Use **Test in a copy…**, with a named save that has never loaded this sector,
the Renaissance executable, and a new output folder outside the installation.
This checks the scene as a whole. It does not exercise installation of the final
update package into an already visited sector.

Record the player's gold, use the chest, and check that it opens and gold rises
by 50. Close and use it again: gold must not rise again. Put an item inside, save,
leave the area, return and reload. The item should remain and the reward should
stay consumed. Also check that companions can reach the chest.

For a fresh first-use test, restore a disposable save from before the first use.
Recompiling a script does not reset a counter already stored in a save.

## 4. Export the installable package

Choose **File > Export Renaissance update package…**. Set a stable package ID,
for example `myname.reward-chest`, and display version `1.0.0`. The first revision
is 1. Use your own namespace so the ID does not collide with another author's mod.

Read the review. For this example it should include the new chest and its new
script resource, without changes to unrelated campaign objects. Export into a
new folder such as `D:/Mods/reward-chest/exports/revision-1`. WorldED writes the
package and saves its publication history in the source project.

The package contains `manifest.json`, `runtime.rwp` and the resources it needs.
Keep the entire folder. Do not hand-edit a published revision or remove files
because they look unused. Exporting does not install the mod.

## 5. Test installation of the actual package

Prepare a separate copy of the game and a copy of a named save, including all
files belonging to that slot. Close that copy before installing. Run these
commands from the extracted WorldED folder:

```powershell
python tools/world-updates/package.py check "D:/Mods/reward-chest/exports/revision-1"
python tools/world-updates/package.py install "D:/Mods/reward-chest/exports/revision-1" "D:/ArcanumTests/reward-chest"
```

On Linux use `python3` and the corresponding paths. Start Renaissance in the
test installation and load the copied save. Repeat the chest test with a save
that has already visited the sector, then with one that has never visited it.
Save and reload, including after putting an item in the chest. The package must
not add a second chest or refill an existing one.

The installer registers the revision under
`ArcanumModern/world-updates/<package-id>/<revision>`. The engine applies pending
operations when their sector loads. If an expected old value differs, it records
a conflict instead of overwriting the player's state. Check the module's
`Save/WorldUpdates/updates.log` when an operation does not appear. The engine
also makes a pre-update backup there; keep your independent test backups.

## 6. Give players a complete download

Zip the exported package folder with a short README. Include the mod name and
version, required Renaissance version, dependencies, installation commands and
whether a new game is needed. State that players must close the game before
installation and keep a backup of the complete save slot.

For this package workflow, direct players to extract the mod and run the same
`check` and `install` commands against their own installation. They need Python 3
and `tools/world-updates/package.py` from the editor ZIP. Do not tell them to copy
`manifest.json` into the game root or overwrite a SEC file.

Distribute your exported resources, README and any assets you have permission to
share. Keep game archives, personal saves, test installations, cache files and
machine-specific settings out of the download. If you share editable sources,
provide them separately with their `.wscript` and `.wdlg` files. They are useful
to authors but are not loaded by the game.

## Maintain and remove a mod

Keep the scene's `world-update.json` and original reference. Reopen that same
project to publish a later revision under the same package ID. The exporter
retains previous operations and adds supported changes, so players can skip an
intermediate release.

Published scripts and dialogues are immutable resources in this update format.
A changed script needs a new resource ID and a supported way to attach it;
replacing an existing object's script attachment is not supported by this
exporter. Plan those changes explicitly rather than overwriting old SCR files.

Removing the package folder does not undo changes already saved in the world.
A save that depends on a missing package may refuse to load. To roll back a test,
restore the complete earlier save slot together with the registry and resource
versions it used. For a distributed mod, document this limitation in the README;
do not advertise deletion of its files as a clean uninstall.
