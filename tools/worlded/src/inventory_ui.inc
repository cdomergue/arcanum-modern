    void editInventory() {
        if(!editing())return;auto selected=selectedIds();if(selected.size()!=1){statusBar()->showMessage("Select one container or character to edit its inventory.");return;}
        try{for(const auto& owner:building->objects)if(selected.count(objectKey(owner))){
            if(!inventory_owner(owner)){statusBar()->showMessage("Only containers and characters have an inventory.");return;}
            populatePalette();InventoryEditor dialog(building->objects,owner,paletteObjects,*assets,this);
            if(dialog.exec()==QDialog::Accepted){for(const auto& item:dialog.result)if(inventory_item(item)){const auto& path=assets->prototypePath(item.prototype_id);if(!building->prototypes.count(path))building->prototypes[path]=installation->read(path);}commitEdit("Edit inventory",std::move(dialog.result),selected);}return;
        }}catch(const std::exception& e){QMessageBox::warning(this,"Cannot edit inventory",QString::fromUtf8(e.what()));}
    }

    void transferToInventory(const std::vector<int>& indices,int ownerIndex) {
        if(!editing() || ownerIndex<0 || ownerIndex>=int(building->objects.size()))return;
        try{
            const auto owner=building->objects[ownerIndex];std::set<std::string> ids;
            for(int i:indices){if(i<0 || i>=int(building->objects.size()))throw std::runtime_error("Selected item is no longer available.");ids.insert(building->objects[i].id);}
            auto next=inventory_transfer(building->objects,owner.id,ids,[this](const auto& o){return assets->effective(o);},[this](const auto& o){
                try{auto pixels=assets->sprite(unsigned(o.number("ITEM_INV_AID"))).pixels;return std::pair<unsigned,unsigned>{unsigned((pixels.width()+31)/32),unsigned((pixels.height()+31)/32)};}catch(const std::exception&){return std::pair<unsigned,unsigned>{1,1};}
            });
            commitEdit("Put items in inventory",std::move(next),{objectKey(owner)});
            statusBar()->showMessage(QString("%1 item(s) placed in %2's inventory - Edit > Inventory to inspect - Ctrl+Z to undo").arg(ids.size()).arg(assets->description(owner)));
        }catch(const std::exception& e){statusBar()->showMessage(QString::fromUtf8(e.what()));}
    }
