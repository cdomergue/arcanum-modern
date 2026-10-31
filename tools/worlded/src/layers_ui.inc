    void setLayer(int layer) {
        canvas->cancelInteraction();referenceCanvas->cancelInteraction();
        int previous=canvas->layer;
        canvas->layer=referenceCanvas->layer=layer;canvas->cellSelection={};referenceCanvas->cellSelection={};
        scene->clearSelection();referenceScene->clearSelection();
        bool surface=layer==1 || layer==2;surfaceTools->setVisible(surface);
        terrainTransitions->setVisible(layer==1);brushSize->setSuffix(layer==2?" pieces":" cells");
        roofVisible->setEnabled(layer!=2);
        if(surface){libraryTabs->setCurrentWidget(surfacePanel);populateSurfaces();updateSurfaceMaterial();}
        else {libraryTabs->setCurrentIndex(0);if(layer==3 && previous!=3)paletteCategory->setCurrentIndex(paletteCategory->findData(16));if(layer==3 || layer==4)inspectorTabs->setCurrentWidget(layer==3?peopleList:scriptsPanel);}
        if(editing())showBuilding(false);
        if(sideAction->isChecked())renderReference();
        updateBuildingActions();
    }
    void populateSurfaces() {
        if(!assets || (canvas->layer!=1 && canvas->layer!=2))return;
        QSignalBlocker block(surfacePalette);surfacePalette->clear();auto entries=assets->surfaces(canvas->layer==2);
        if(canvas->layer==1)for(unsigned i=0;i<paletteObjects.size();i++)if(paletteObjects[i].type==100){auto aid=std::uint32_t(paletteObjects[i].number("CURRENT_AID"));if(!entries.count(aid))entries[aid]="Exact tile · "+objectPalette->item(i)->text();}
        auto collect=[&](const Building& b){if(canvas->layer==2){for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++){auto id=b.ground.roof(x,y);if(id!=0xffffffff && !entries.count(id))entries[id]=QString("Existing roof · %1").arg(id,8,16,QChar('0'));}}
            else for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++){auto id=b.ground.tile(x,y);if(!entries.count(id))entries[id]=QString("Existing ground · %1").arg(id,8,16,QChar('0'));}};
        if(building)collect(*building);
        if(worldDrafts.count(referenceDraft))collect(worldDrafts.at(referenceDraft)->building);
        std::vector<std::pair<std::uint32_t,QString>> sorted(entries.begin(),entries.end());
        if(canvas->layer==1)std::stable_sort(sorted.begin(),sorted.end(),[](const auto& a,const auto& b){return QString::compare(a.second,b.second,Qt::CaseInsensitive)<0;});
        for(const auto& [aid,label]:sorted){auto item=new QListWidgetItem(label,surfacePalette);item->setData(Qt::UserRole,aid);item->setToolTip(label+QString("\nART 0x%1").arg(aid,8,16,QChar('0')));}
        filterSurfaces();loadSurfaceIcons();
    }
    void configureSector() {
        if(!editing())return;
        QDialog dialog(this);dialog.setWindowTitle("Sector properties");QFormLayout form(&dialog);
        auto original=building->ground.settings();QSpinBox aptitude;aptitude.setObjectName("sectorAptitude");aptitude.setRange(INT_MIN,INT_MAX);aptitude.setValue(original.aptitude);
        QComboBox music,ambient,light;music.setObjectName("sectorMusic");ambient.setObjectName("sectorAmbient");light.setObjectName("sectorLight");
        auto populate=[&](QComboBox& box,const char* path,int current){
            box.addItem("None / default · #0",0);
            if(installation->contains(path))for(const auto& entry:read_messages(installation->read(path)))if(entry.id && entry.id<=INT_MAX)
                box.addItem(QString::fromStdString(cp1252_utf8(entry.text)).section('#',0,0).trimmed()+QString(" · #%1").arg(entry.id),int(entry.id));
            if(box.findData(current)<0)box.addItem(QString("Unlisted scheme · #%1").arg(current),current);
            box.setCurrentIndex(box.findData(current));box.setMinimumContentsLength(32);box.setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        };
        try{populate(music,"sound/schemeindex.mes",original.music);populate(ambient,"sound/schemeindex.mes",original.ambient);populate(light,"rules/lighting schemes.mes",original.light);}
        catch(const std::exception& e){QMessageBox::warning(this,"Cannot read sector schemes",utf8(e.what()));return;}
        form.addRow("Music scheme",&music);form.addRow("Ambient scheme",&ambient);form.addRow("Lighting scheme",&light);form.addRow("Magic / technology adjustment",&aptitude);
        QLabel note("Changes affect this sector. Sound playback and lighting are evaluated in the game.");note.setWordWrap(true);form.addRow(&note);
        QDialogButtonBox buttons(QDialogButtonBox::Save|QDialogButtonBox::Cancel);form.addRow(&buttons);
        connect(&buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        if(dialog.exec()!=QDialog::Accepted)return;
        auto ground=building->ground;ground.set_settings({aptitude.value(),light.currentData().toInt(),music.currentData().toInt(),ambient.currentData().toInt()});
        if(ground.encode()!=building->ground.encode())commitEdit("Change sector properties",building->objects,{},&ground);
    }
    void filterSurfaces(){for(int i=0;i<surfacePalette->count();i++)surfacePalette->item(i)->setHidden(!surfacePalette->item(i)->text().contains(surfaceSearch->text(),Qt::CaseInsensitive));loadSurfaceIcons();}
    void updateSurfaceMaterial() {
        auto item=surfacePalette->currentItem();
        if(canvas->layer==2){surfaceMaterialStatus->setText(item?"Roof piece selected.":"Choose a roof piece.");return;}
        bool indoor=item && TerrainBrush::material(item->data(Qt::UserRole).toUInt(),true)>=200;
        bool exact=item && (item->text().startsWith("Exact tile") || (item->data(Qt::UserRole).toUInt()>>28)==11);
        {QSignalBlocker block(terrainTransitions);terrainTransitions->setEnabled(!indoor && !exact);terrainTransitions->setChecked(!indoor && !exact && terrainTransitionsPreference);}
        terrainTransitions->setToolTip(exact?"This tile is placed exactly as selected.":indoor?"This indoor floor has no automatic transitions.":"Connect outdoor terrain using the game's rules.");
        surfaceMaterialStatus->setText(!item?"Choose a material from the library.":exact?"Exact tile: place this shape directly, without automatic transitions.":indoor?"Indoor floor: automatic transitions are unavailable. Tiles will be placed directly.":"Outdoor terrain: enable automatic transitions to blend edges.");
    }
    void loadSurfaceIcons(){if(!assets)return;for(int i=0;i<surfacePalette->count();i++){auto item=surfacePalette->item(i);if(item->isHidden() || !item->icon().isNull() || !surfacePalette->visualItemRect(item).intersects(surfacePalette->viewport()->rect()))continue;
            try{item->setIcon(QIcon(assets->sprite(item->data(Qt::UserRole).toUInt()).pixels));}catch(const std::exception& e){item->setToolTip(item->toolTip()+"\n"+utf8(e.what()));}}}
    const Building* surfaceSource(bool reference) const {return reference?(worldDrafts.count(referenceDraft)?&worldDrafts.at(referenceDraft)->building:nullptr):building.get();}
    void surfaceSelected(bool reference) {
        auto view=reference?referenceCanvas:canvas;referenceFocused=reference;updateCopyActions();
        auto r=view->cellSelection;auto count=r.width()*r.height()/(view->layer==2?16:1);
        selectionLabel->setText(QString("%1 %2 selected · Ctrl+C to copy").arg(count).arg(view->layer==2?"roof pieces":"ground cells"));
        deleteHouse->setEnabled(editing() && !reference && view->layer==2 && count>0);
    }
    void sampleSurface(bool reference,QPoint tile) {
        auto source=surfaceSource(reference);if(!source || !Canvas::valid(tile))return;
        auto aid=canvas->layer==2?source->ground.roof(tile.x()/4,tile.y()/4):source->ground.tile(tile.x(),tile.y());if(aid==0xffffffff)return;
        populateSurfaces();surfaceSearch->clear();for(int i=0;i<surfacePalette->count();i++)if(surfacePalette->item(i)->data(Qt::UserRole).toUInt()==aid){surfacePalette->setCurrentRow(i);surfacePalette->scrollToItem(surfacePalette->item(i));loadSurfaceIcons();break;}
        statusBar()->showMessage("Material sampled. Select cells and Apply, or enable Paint.");
    }
    void applySurface(bool erase) {
        if(!editing() || (canvas->layer!=1 && canvas->layer!=2) || canvas->cellSelection.isEmpty())return;
        if(erase && canvas->layer!=2){statusBar()->showMessage("To replace the ground, choose a material and Apply.");return;}
        auto material=surfacePalette->currentItem();if(!erase && !material){statusBar()->showMessage("Choose a material from the Ground / Roofs library.");return;}
        if(!erase && canvas->layer==1 && terrainTransitions->isChecked()) {std::set<std::pair<int,int>> cells;auto r=canvas->cellSelection;for(int y=r.top();y<=r.bottom();y++)for(int x=r.left();x<=r.right();x++)cells.insert({x,y});paintCells(cells);return;}
        auto ground=building->ground;auto rect=canvas->cellSelection;auto aid=erase?0xffffffff:material->data(Qt::UserRole).toUInt();bool changed=false;
        int step=canvas->layer==2?4:1;
        for(int y=rect.top();y<=rect.bottom();y+=step)for(int x=rect.left();x<=rect.right();x+=step) {
            if(step==4){changed|=ground.roof(x/4,y/4)!=aid;ground.set_roof(x/4,y/4,aid);}else{changed|=ground.tile(x,y)!=aid;ground.set_tile(x,y,aid);}}
        if(changed)commitEdit(erase?"Remove roofs":step==4?"Paint roofs":"Paint ground",building->objects,{},&ground);
        canvas->cellSelection=rect;canvas->viewport()->update();
    }
    void copySurface(bool reference) {
        const auto* source=surfaceSource(reference);auto view=reference?referenceCanvas:canvas;auto rect=view->cellSelection;
        if(!source || rect.isEmpty())return;
        copied={};copied.width=rect.width();copied.height=rect.height();copied.source=reference?worldDrafts.at(referenceDraft)->name:draftName;
        int step=view->layer==2?4:1;for(int y=rect.top();y<=rect.bottom();y+=step)for(int x=rect.left();x<=rect.right();x+=step) {
            if(step==4)copied.roofs[{x-rect.left(),y-rect.top()}]=source->ground.roof(x/4,y/4);
            else copied.tiles[{x-rect.left(),y-rect.top()}]=source->ground.tile(x,y);}
        updateCopyActions();statusBar()->showMessage("Surface copied. Ctrl+V in the active zone, then click to place.");
    }
    void paintCells(const std::set<std::pair<int,int>>& cells) {
        if(!editing() || cells.empty() || (canvas->layer!=1 && canvas->layer!=2))return;
        auto item=surfacePalette->currentItem();if(!item){statusBar()->showMessage("Choose a material before painting.");return;}
        try{auto ground=building->ground;auto aid=item->data(Qt::UserRole).toUInt();
            bool transitions=canvas->layer==1 && terrainTransitions->isChecked() && TerrainBrush::material(aid,true)<200;
            if(transitions){TerrainBrush brush(*installation);ground=brush.paint(ground,cells,aid,[this](unsigned id){return assets->available(id);});}
            else for(auto [x,y]:cells)if(canvas->layer==2)ground.set_roof(x/4,y/4,aid);else ground.set_tile(x,y,aid);
            if(ground.encode()!=building->ground.encode())commitEdit(canvas->layer==2?"Paint roofs":transitions?"Paint terrain and transitions":"Paint ground",building->objects,{},&ground);
        }catch(const std::exception& e){QMessageBox::warning(this,"Stroke not applied",utf8(e.what()));}
    }
    void drawRoofs(QGraphicsScene* target,const Sector& ground) {
        if(!roofVisible->isChecked() && canvas->layer!=2)return;
        for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++){auto aid=ground.roof(x,y);if(aid==0xffffffff)continue;
            auto position=Canvas::project({int(x*4+2),int(y*4+2)})-QPointF(160,220);
            try {auto sprite=assets->sprite(aid);auto item=target->addPixmap(sprite.pixels);item->setPos(position-sprite.hotspot);item->setZValue(10000+position.y());item->setOpacity(canvas->layer==2?1:.65);item->setToolTip(QString("Roof %1, %2 · ART %3").arg(x).arg(y).arg(aid,8,16,QChar('0')));}
            catch(const std::exception& e){auto item=target->addRect(QRectF(position,QSizeF(320,160)),QPen(QColor("#e9a25a")));item->setToolTip(utf8(e.what()));item->setZValue(10000+position.y());}}
    }
