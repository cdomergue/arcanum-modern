# Player houses

Edwin Harrow, the estate agent in the office just inside Tarant Town Hall, sells the
Woodland House for 10,000 coins. The guard at the house gives directions to him.
Consulting the property listing or speaking to the guard reveals its world-map
marker. The furnished house stands directly on the main map in sector 1346, 1024.
The former owner's tame wolf stays there.

Doors, windows and storage are locked before purchase. Buying the house gives
the party its key. The agent can replace a lost key free of charge. After purchase,
the guard uses the game's ordinary greetings. Chests retain their contents in the
normal save system. Beds become usable by the owner.

English and French dialogue resources are included. English is the fallback;
the integrated French UAP selects French automatically. Set
`ARCANUM_HOUSE_LANGUAGE=fr` or `en` to override this selection.

## Adding another property

Edit `resources/house/catalog.json`. Each listing has localized names, standing,
description, disclosures and key name, plus its price and sector. Different
standings need no separate seller script. The catalogue automatically paginates
at three properties per page, leaving room for navigation within Arcanum's
five-response limit.

Keep `id`, `slot`, `ownership_global`, `discovery_global`, `key_id` and scene object
GUIDs stable after distribution. Do not renumber them when changing the order of
listings. Allocate unused globals and keys after checking the installed campaign
and other Renaissance content. The initial property uses globals 1988–1989 and
key 61003. Estate scripts use 48300, 48301 and 48304; the seller description is
61002, and the unidentified estate-agent description is 61005. The seller's
`position` in the catalogue defines his tile, pixel offsets and facing.
Script commands for a property occupy `100 + slot * 10` (show), the next
line (buy) and the next (replace key). The native module checks ownership and
funds again when executing a purchase.

Each scene directory contains native object instances, a `scene.mes` manifest,
`terrain.mes` and `roofs.mes` differences, and `removed.mes` for original scenery
removed by the authored scene. Point the catalogue entry's `scene` at this
directory. Object GUIDs must be distinct from every other property. Scene data
must be authored against the same campaign terrain used by the installation.

Generate content with:

```sh
python3 tools/house/build-content.py --output build/house
python3 tests/test_house_catalog.py
```

The regular Linux build installs the content in the repository's isolated `game/`
installation. Windows, macOS and Linux packaging include the same resources.
The estate transaction is implemented in `modern_house.c`; its SCR files are
native entry points, not editable implementations of the transaction. Catalogue
editing currently uses JSON rather than a dedicated graphical form.

## Save compatibility and verification

Installation adds objects by persistent GUID and leaves base sector object order
intact. This preserves existing sector DIF references. It does not replace the
player's save or reset storage when revisiting the house. Existing scenery removed
in the authored scene is hidden using its location, art and offsets; player items
and other dynamic objects are not removed.

After `./build.sh`, build the native check with
`python3 tools/house/build-check.py`. Run `build/house/check-house COPY/game SLOT`
on an isolated game copy containing the content pack and a `.renaissance-house-test`
marker. The check changes only that copy, creates test slots `Slot0096` (at the guard, before purchase), `Slot0097` (at the agent, before purchase) and `Slot0098` (after purchase), and exercises
migration, insufficient funds, purchase, duplicate purchase, chest storage,
save/reload and the world-map marker. It also captures the complete dialogue UI
in `/tmp/house-LANGUAGE-*.png`.
