// Include after Explorer is defined, then call with an editable scene mounted.
static void checkInventoryExplorer(Explorer& window) {
    auto require=[](bool condition,const char* message){if(!condition)throw std::runtime_error(message);};
    require(bool(window.building),"Inventory editor test needs an editable scene");
    auto previous=*window.building;auto undo=window.undoObjects,redo=window.redoObjects;
    window.populatePalette();auto found=std::find_if(window.paletteObjects.begin(),window.paletteObjects.end(),[](const auto& o){return o.type==2;});require(found!=window.paletteObjects.end(),"Inventory editor fixture lacks a container");
    auto owner=worlded::clone_instance(*found);worlded::set_number(owner,"LOCATION",20ull|(20ull<<32));worlded::set_number(owner,"CONTAINER_INVENTORY_NUM",0);worlded::set_null(owner,"CONTAINER_INVENTORY_LIST_IDX");
    found=std::find_if(window.paletteObjects.begin(),window.paletteObjects.end(),[](const auto& o){return worlded::inventory_item(o);});require(found!=window.paletteObjects.end(),"Inventory editor fixture lacks an item");
    auto item=worlded::clone_instance(*found);worlded::set_number(item,"ITEM_INV_LOCATION",0);
    auto next=worlded::inventory_apply({owner},owner.id,{item},[&](const auto& o){return window.assets->effective(o);},[](const auto&){return std::pair<unsigned,unsigned>{1,1};});
    window.building->objects=next;window.canvas->layer=0;std::set<std::string> selected{owner.id};window.showBuilding(false,&selected);
    require(window.objectItems.size()==2 && !window.objectItems[1]->isVisible(),"Inventory item appears on the ground");
    window.duplicateSelection();require(window.building->objects.size()==4,"Duplicate omitted the owned item");
    auto duplicated=window.building->objects[2];require(duplicated.id!=owner.id,"Duplicate reused owner GUID");
    auto contents=worlded::inventory_contents(window.building->objects,duplicated);require(contents.size()==1 && contents[0].id!=item.id,"Duplicate references original item");
    require(worlded::inventory_parent(contents[0])==duplicated.id,"Copied item references original owner");
    window.history(false);require(window.building->objects.size()==2,"Undo did not restore inventory object count");window.history(true);require(window.building->objects.size()==4,"Redo omitted inventory object");
    window.deleteSelection();require(window.building->objects.size()==2,"Deleting owner left an orphan item");window.history(false);require(window.building->objects.size()==4,"Undo deletion lost the owned item");
    window.translateSelection({0},{2,3});require(window.building->objects[0].number("LOCATION")==window.building->objects[1].number("LOCATION"),"Moving owner left item in another location");
    *window.building=previous;window.undoObjects=std::move(undo);window.redoObjects=std::move(redo);window.showBuilding(false);
}
