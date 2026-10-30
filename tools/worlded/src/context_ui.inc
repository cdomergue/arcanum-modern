    const std::vector<GameObject>& campaignObjects(const Building& b) {
        auto found=campaignCache.find(b.map_name);
        if(found==campaignCache.end()) {
            std::map<std::string,GameObject> byId;auto bundle=mobile_bundle_name(b.map_name);
            if(installation->contains(bundle))for(auto& object:read_objects(installation->read(bundle),16))byId[object.id]=std::move(object);
            auto prefix="maps/"+b.map_name+"/";
            for(const auto& [path,sources]:installation->resources())if(path.rfind(prefix,0)==0 && fs::path(path).extension()==".mob")
                for(auto& object:read_objects(installation->read(path)))byId[object.id]=std::move(object);
            std::map<std::uint64_t,std::vector<GameObject>> sectors;
            for(auto& [id,object]:byId){auto loc=object.number("LOCATION");auto sector=(std::uint64_t(std::uint32_t(loc)>>6))|((loc>>38)<<26);
                set_number(object,"LOCATION",(loc&63)|(((loc>>32)&63)<<32));sectors[sector].push_back(std::move(object));}
            found=campaignCache.emplace(b.map_name,std::move(sectors)).first;
        }
        return found->second[b.sector_id];
    }
    ContextData buildContext(const Building& b) {
        ContextData result;std::set<std::string> present;for(const auto& o:b.objects)present.insert(o.id);
        auto append=[&](const GameObject& o,bool campaign){if(campaign && (o.number("FLAGS")&0x1000))return; // Inventory items have no independent map position.
            auto object=o;try{object=assets->effective(o);}catch(const std::exception& e){result.warnings<<utf8(e.what());}
            auto loc=object.number("LOCATION");QPoint tile(loc&63,(loc>>32)&63);QString name=assets->description(object);
            if(object.type==15 || object.type==16)result.actors.push_back({object,campaign});
            if(auto scripts=object.field("SCRIPTS_IDX"))for(const auto& entry:scripts->elements)if(entry.data.size()==12){auto id=word(entry.data,8);if(id){QString event=entry.index<std::size(attachmentNames)?attachmentNames[entry.index]:QString::number(entry.index);result.scripts.push_back({id,name+" · "+event,tile,campaign?1:0,objectKey(o),entry.index,0,o.field("SCRIPTS_IDX")==nullptr});}}
        };
        for(const auto& o:b.objects)append(o,false);
        if(b.existing_map)for(const auto& o:campaignObjects(b))if(!present.count(o.id))append(o,true);
        for(std::size_t i=0;i<b.ground.scripts().size();i++){const auto& script=b.ground.scripts()[i];if(script.number) {QPoint tile=script.tile==UINT32_MAX?QPoint(-1,-1):QPoint(script.tile%64,script.tile/64);
            result.scripts.push_back({script.number,script.tile==UINT32_MAX?"Sector":QString("Tile %1, %2").arg(tile.x()).arg(tile.y()),tile,script.tile==UINT32_MAX?3:2,{},0,i,false});}}
        return result;
    }
    void drawContext(QGraphicsScene* target,ContextData& context,bool reference) {
        for(std::size_t i=0;i<context.actors.size();i++){const auto& actor=context.actors[i];if(!actor.campaign)continue;
            auto loc=actor.object.number("LOCATION");QPointF pos=Canvas::project({int(loc&63),int((loc>>32)&63)});pos+=QPointF(std::int32_t(actor.object.number("OFFSET_X")),std::int32_t(actor.object.number("OFFSET_Y")));
            QGraphicsItem* item;
            try {auto sprite=assets->sprite(unsigned(actor.object.number("CURRENT_AID",actor.object.number("AID"))));auto pix=target->addPixmap(sprite.pixels);pix->setPos(pos-sprite.hotspot);if(canvas->layer==3)pix->setShapeMode(QGraphicsPixmapItem::BoundingRectShape);item=pix;}
            catch(const std::exception&){auto placeholder=target->addEllipse(-10,-26,20,26,QPen(QColor("#89cfff")),QBrush(QColor("#375572")));placeholder->setPos(pos);item=placeholder;}
            item->setData(2,int(i));item->setZValue(pos.y());item->setToolTip(assets->description(actor.object)+" · campaign character · inspect");
            if(canvas->layer==1 || canvas->layer==2)item->setOpacity(.3);
        }
        if(canvas->layer==4)for(std::size_t i=0;i<context.scripts.size();i++){const auto& script=context.scripts[i];if(!Canvas::valid(script.tile))continue;
            auto pos=Canvas::project(script.tile);auto marker=target->addRect(-12,-12,24,24,QPen(QColor("#ffe59a")),QBrush(QColor("#774b7c")));marker->setPos(pos);marker->setZValue(25000);marker->setData(2,-int(i)-1);marker->setToolTip(script.label+QString(" · script %1").arg(script.id));
            auto text=new QGraphicsSimpleTextItem("S",marker);text->setBrush(Qt::white);text->setPos(-4,-9);}
        if(!reference){QSignalBlocker block(peopleList),blockScripts(scriptsList);peopleList->clear();scriptsList->clear();
            auto selection=selectedIds();int selectedActor=-1;
            for(std::size_t i=0;i<context.actors.size();i++){const auto& actor=context.actors[i];peopleList->addItem(assets->description(actor.object)+(actor.campaign?" · campaign":" · project"));if(!actor.campaign && selection.count(objectKey(actor.object)))selectedActor=int(i);}
            if(selectedActor>=0)peopleList->setCurrentRow(selectedActor);
            for(const auto& script:context.scripts){auto item=new QListWidgetItem(script.label+QString(" → %1").arg(script.id),scriptsList);item->setData(Qt::UserRole,script.id);item->setToolTip(item->text());}
            auto summary=QString("%1 characters · %2 attached scripts\nInitial campaign positions; double-click to open.").arg(context.actors.size()).arg(context.scripts.size());
            if(!context.warnings.isEmpty())summary+=QString("\n%1 incomplete resources (hover for details).").arg(context.warnings.size());
            contextStatus->setText(summary);contextStatus->setToolTip(context.warnings.join('\n'));scriptDetails->setText("Select a script to inspect its attachment.");locateScript->setEnabled(false);openScript->setEnabled(false);removeScript->setEnabled(false);}
        target->setSceneRect(target->itemsBoundingRect().adjusted(-50,-50,50,50));
    }
    void inspectContextObject(bool reference,int index) {
        auto& context=reference?referenceContext:activeContext;
        if(index<0){auto script=-index-1;if(script<int(context.scripts.size())){if(reference)showScript(context.scripts[script].id);else{inspectorTabs->setCurrentWidget(scriptsPanel);scriptsList->setCurrentRow(script);}}return;}
        if(index>=int(context.actors.size()))return;
        const auto& actor=context.actors[index];const auto& object=actor.object;
        if(!reference && canvas->layer==3){QSignalBlocker block(scene);scene->clearSelection();if(!actor.campaign)for(std::size_t i=0;i<building->objects.size() && i<objectItems.size();i++)if(objectKey(building->objects[i])==objectKey(actor.object)){objectItems[i]->setSelected(true);break;}}
        if(!reference && canvas->layer==3)syncSceneSelection();
        auto target=reference?referenceScene:scene;for(auto item:target->items())if(item->data(3).toBool())delete item;
        auto location=object.number("LOCATION");auto highlight=target->addEllipse(-16,-9,32,18,QPen(QColor("#7ed5ff"),2),QBrush(QColor(70,170,255,45)));
        highlight->setPos(Canvas::project({int(location&63),int((location>>32)&63)})+QPointF(std::int32_t(object.number("OFFSET_X")),std::int32_t(object.number("OFFSET_Y"))));highlight->setZValue(26000);highlight->setData(3,true);
        properties->clear();addProperty("Character",assets->description(object));addProperty("Source",actor.campaign?"Campaign · read only":"Project");addProperty("Prototype",QString::number(object.prototype_id));
        auto loc=object.number("LOCATION");addProperty("Position",QString("%1, %2").arg(loc&63).arg((loc>>32)&63));
        for(const auto& field:object.fields){auto node=new QTreeWidgetItem(properties,{utf8(field.name),utf8(object_field_text(field))});
            if(field.name=="SCRIPTS_IDX")for(const auto& entry:field.elements)if(entry.data.size()==12 && word(entry.data,8)){auto child=new QTreeWidgetItem(node,{entry.index<std::size(attachmentNames)?QString(attachmentNames[entry.index]):QString::number(entry.index),QString::number(word(entry.data,8))});child->setData(0,Qt::UserRole,word(entry.data,8));node->setExpanded(true);}}
        if(!reference){QSignalBlocker block(peopleList);peopleList->setCurrentRow(index);inspectorTabs->setCurrentWidget(peopleList);}
        placementControls->setEnabled(false);statusBar()->showMessage("Character inspected · double-click a script in properties to open it.");
    }
    void selectScript(int index,bool center) {
        if(index<0 || index>=int(activeContext.scripts.size())){locateScript->setEnabled(false);openScript->setEnabled(false);removeScript->setEnabled(false);if(canvas->layer==4)deleteHouse->setEnabled(false);return;}
        const auto& script=activeContext.scripts[index];QString scope=script.origin==3?"Sector-wide; no single position.":script.origin==2?"Tile attachment.":script.origin==1?"Campaign mobile object; read only.":script.inherited?"Project object; inherited from its prototype.":"Project object attachment.";
        if(Canvas::valid(script.tile))scope+=QString(" Position: %1, %2.").arg(script.tile.x()).arg(script.tile.y());
        QString path;for(const auto& [name,sources]:installation->resources())if(name.rfind("scr/",0)==0 && fs::path(name).extension()==".scr"){
            auto filename=QString::fromStdString(name).mid(4);auto digits=QRegularExpression("^([0-9]+)").match(filename);if(digits.hasMatch() && digits.captured(1).toUInt()==script.id){path=filename;break;}}
        scriptDetails->setText(scope);
        properties->clear();addProperty("Script",QString::number(script.id));addProperty("Attachment",script.label);addProperty("Scope",scope);addProperty("File",path.isEmpty()?"SCR file not found":path);
        locateScript->setEnabled(Canvas::valid(script.tile));openScript->setEnabled(true);removeScript->setEnabled(editing() && script.origin!=1);
        if(canvas->layer==4)deleteHouse->setEnabled(removeScript->isEnabled());
        removeScript->setToolTip(script.origin==1?"Campaign mobile objects cannot yet be edited in this project.":"Remove only this attachment; the SCR file remains unchanged. Undo restores it.");
        if(center && Canvas::valid(script.tile))canvas->centerOn(Canvas::project(script.tile));
        if(Canvas::valid(script.tile)){for(auto item:scene->items())if(item->data(3).toBool())delete item;auto highlight=scene->addEllipse(-20,-12,40,24,QPen(QColor("#7ed5ff"),2),QBrush(QColor(70,170,255,45)));highlight->setPos(Canvas::project(script.tile));highlight->setZValue(26000);highlight->setData(3,true);}
    }
    void removeSelectedScript() {
        int index=scriptsList->currentRow();if(!editing() || index<0 || index>=int(activeContext.scripts.size()))return;
        auto script=activeContext.scripts[index];if(script.origin==1)return;
        try {
            if(script.origin>=2){auto ground=building->ground;ground.remove_script(script.sectorIndex);commitEdit("Remove script attachment",building->objects,{},&ground);}
            else {auto next=building->objects;auto it=std::find_if(next.begin(),next.end(),[&](const auto& o){return objectKey(o)==script.objectKey;});if(it==next.end())throw std::runtime_error("Object no longer exists");
                auto effective=assets->effective(*it);auto field=effective.field("SCRIPTS_IDX");if(!field || !field->present)throw std::runtime_error("Script attachment no longer exists");
                auto replacement=*field;bool found=false;for(auto& entry:replacement.elements)if(entry.index==script.slot && entry.data.size()==12 && word(entry.data,8)==script.id){std::fill(entry.data.begin()+8,entry.data.begin()+12,0);found=true;break;}
                if(!found)throw std::runtime_error("Script attachment no longer exists");
                auto original=std::find_if(it->fields.begin(),it->fields.end(),[](const auto& f){return f.name=="SCRIPTS_IDX";});if(original==it->fields.end())it->fields.push_back(std::move(replacement));else *original=std::move(replacement);
                commitEdit("Remove script attachment",std::move(next),{});}
            statusBar()->showMessage("Script attachment removed. Undo restores it; the SCR file is unchanged.");
        }catch(const std::exception& e){QMessageBox::warning(this,"Cannot remove script attachment",utf8(e.what()));}
    }
