# Starting inventories and equipment

Select a container or character in an editable zone, then choose **Edit >
Inventory…**. This edits the objects supplied with your project. It does not
open or change a player's saved inventory.

Click **Add item…**, search by the installed item name or prototype number,
and choose an item. Use **Quantity** for gold and ammunition. Other items are
separate instances: add a second sword if the character needs two swords.
The **Identified** checkbox controls whether the item starts identified.

For a character, **Placement** offers the equipment slots that match the item.
A ring can use either ring slot. Containers have backpack storage only.
**Arrange backpack** packs the unequipped items without changing their equipment
slots. You can also enter a grid cell explicitly. The inventory has ten columns;
cell 23 means row 2, column 3, counting from zero. Characters have 12 rows and
containers have 96. An item's inventory graphic determines how many cells it
occupies.

Save checks for overlapping items, items extending outside the grid, duplicate
equipment slots, and a shield combined with a weapon that requires both hands.
WorldED does not validate restrictions involving race, gender, spells, injuries
or available character graphics. Test an equipped character in an independent
game copy before distributing it.

Saving a manual inventory sets its inventory generator to zero. That prevents a
random source from adding a second starting loadout. Cancel discards your edits;
after saving, the editor's Undo command restores the whole inventory change.

Items retain their own persistent identifiers and references to their owner.
An existing inventory can be edited only when all its referenced item objects
are present in the project. If an item is missing, WorldED reports its identifier
instead of replacing the incomplete inventory with an empty one. Gold and
ammunition on a character use one stack per ammunition type; change that stack's
quantity rather than creating another stack.

## Random inventory sources

For a generated loadout, set **Inventory generator** in the character's
properties instead of saving a manual inventory. The number refers to an entry
in `rules/invensource.mes`. A container has its own **Content generator** field.

The campaign resource editor exposes the installation's `rules/` and `mes/`
tables, including inventory sources and autoequip definitions. Copy an existing
entry before adapting it, keep its native syntax, and use an unused ID. These
tables affect every object that refers to the edited entry. Project overrides
belong to the campaign export; validate generated contents in a test copy.

## Housing

A newly installed housing container or character can carry the manual items
authored in WorldED. The housing export records each item's owner, keeps both
identifiers, and loads their references together. Starting contents are applied
when the owner is first installed. Changing an authored inventory later is not
a way to refill a player's existing chest or reset an NPC's equipment.

For a housing test, check the first installation, move an item into the player's
inventory, save, reload, and revisit the property. Confirm that the transferred
item stays with the player and does not reappear in the original container.

## Put a ground item into an inventory

Hold **Shift**, drag a ground item onto a project character or container, then
release the mouse button while still holding Shift. WorldED finds a free backpack
position and keeps the item's identity and properties. Select several items first
to transfer them together. The whole transfer is refused if an item cannot fit.

Use **Edit > Inventory** to inspect the result, or **Ctrl+Z** to return the item
to its previous ground position. **Escape** cancels a drag. Objects from the
read-only campaign context cannot receive items until they belong to the project.
