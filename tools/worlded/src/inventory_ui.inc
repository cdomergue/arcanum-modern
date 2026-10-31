    void editInventory() {
        if(!editing())return;auto selected=selectedIds();if(selected.size()!=1){statusBar()->showMessage("Select one container or character to edit its inventory.");return;}
        try{for(const auto& owner:building->objects)if(selected.count(objectKey(owner))){
            if(!inventory_owner(owner)){statusBar()->showMessage("Only containers and characters have an inventory.");return;}
            populatePalette();InventoryEditor dialog(building->objects,owner,paletteObjects,*assets,this);
            if(dialog.exec()==QDialog::Accepted){for(const auto& item:dialog.result)if(inventory_item(item)){const auto& path=assets->prototypePath(item.prototype_id);if(!building->prototypes.count(path))building->prototypes[path]=installation->read(path);}commitEdit("Edit inventory",std::move(dialog.result),selected);}return;
        }}catch(const std::exception& e){QMessageBox::warning(this,"Cannot edit inventory",QString::fromUtf8(e.what()));}
    }
