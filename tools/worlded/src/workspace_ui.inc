// Explorer workspace methods; included inside the class to keep scene navigation
// and save-state handling together without duplicating the asset browser.
    bool draftDirty(const CachedDraft& draft) const {return !draft.saved || scene_fingerprint(draft.building)!=scene_fingerprint(*draft.saved);}
    void cacheDraft() {
        if(!building || activeDraft==UINT64_MAX)return;
        auto next=std::make_unique<CachedDraft>(CachedDraft{*building,undoObjects,redoObjects,savedScene,savedPath,draftName,{},{},{},false});
        auto old=worldDrafts.find(activeDraft);
        if(showingBuilding){next->view=canvas->transform();next->center=canvas->mapToScene(canvas->viewport()->rect().center());next->selection=selectedIds();next->hasView=true;}
        else if(old!=worldDrafts.end()){next->view=old->second->view;next->center=old->second->center;next->selection=old->second->selection;next->hasView=old->second->hasView;}
        if(old==worldDrafts.end())draftOrder.push_back(activeDraft);
        worldDrafts[activeDraft]=std::move(next);
    }
    QString changeSummary(const CachedDraft& draft) const {
        if(!draft.saved)return QString("New zone · %1 objects to save").arg(draft.building.objects.size());
        auto delta=scene_changes(draft.building,*draft.saved);QStringList parts;
        if(delta.added)parts<<QString("%1 objects added").arg(delta.added);
        if(delta.removed)parts<<QString("%1 objects deleted").arg(delta.removed);
        if(delta.changed)parts<<QString("%1 objects modified").arg(delta.changed);
        if(delta.tiles)parts<<QString("%1 ground cells").arg(delta.tiles);
        if(delta.roofs)parts<<QString("%1 roof pieces").arg(delta.roofs);
        if(delta.blocks)parts<<QString("%1 blocking cells").arg(delta.blocks);
        if(delta.settings)parts<<"Sector properties modified";
        if(delta.lights)parts<<"Lights modified";
        if(delta.scripts)parts<<"Script attachments modified";
        if(delta.names)parts<<"Location name modified";
        if(delta.content)parts<<"Project scripts / dialogues modified";
        return parts.isEmpty()?"No pending changes":parts.join(" · ");
    }
    void refreshWorkspace() {
        if(workspaceRefreshing)return;
        QScopedValueRollback<bool> guard(workspaceRefreshing,true);cacheDraft();
        QSignalBlocker tabsBlock(zoneTabs),referenceBlock(referenceSelector);
        while(zoneTabs->count())zoneTabs->removeTab(0);
        referenceSelector->clear();int dirtyCount=0;
        for(auto key:draftOrder){auto& draft=*worldDrafts.at(key);bool dirty=draftDirty(draft);dirtyCount+=dirty;
            int index=zoneTabs->addTab((dirty?"● ":"")+draft.name);zoneTabs->setTabData(index,QVariant::fromValue<qulonglong>(key));
            zoneTabs->setTabTextColor(index,dirty?QColor("#d99b37"):QWidget::palette().color(QPalette::WindowText));
            zoneTabs->setTabToolTip(index,changeSummary(draft)+"\n"+(draft.path.isEmpty()?"Game source · no writes to the installation":draft.path));
            if(key==activeDraft)zoneTabs->setCurrentIndex(index);
            if(key!=activeDraft || key==referenceDraft){referenceSelector->addItem((dirty?"● ":"")+draft.name,QVariant::fromValue<qulonglong>(key));if(key==referenceDraft)referenceSelector->setCurrentIndex(referenceSelector->count()-1);}
        }
        workspaceState->setText(QString("%1 zones open · %2 unsaved").arg(draftOrder.size()).arg(dirtyCount));
        zoneTabs->setVisible(!draftOrder.empty());changesList->clear();
        if(building && worldDrafts.count(activeDraft)){
            auto& draft=*worldDrafts.at(activeDraft);bool dirty=draftDirty(draft);
            zoneState->setText((showingBuilding?"ACTIVE ZONE · ":"PRESERVED ZONE · ")+draft.name+"\n"+(dirty?"UNSAVED CHANGES — ":"SAVED / UNCHANGED — ")+changeSummary(draft));
            zoneState->setStyleSheet(dirty?"QLabel {background:#3c3020;color:#ffe1a3;border-left:3px solid #e6ac50;padding:7px;}":"QLabel {background:#20372c;color:#bce4cf;border-left:3px solid #69ba92;padding:7px;}");
            changesList->addItem(changeSummary(draft));changesList->addItem(draft.path.isEmpty()?"No saved project for this zone.":"Project: "+draft.path);
            changesList->addItem("The game installation stays unchanged. Saving preserves the project; testing prepares a game copy.");
            if(!undoObjects.empty())changesList->addItem("Last operation: "+undoObjects.back().label);
            changesList->setWordWrap(true);
            if(showingBuilding)title->setText("Edit: "+draft.name);
        }else zoneState->clear();
        saveAction->setEnabled(building!=nullptr);changeProjectInstallation->setEnabled(building && !savedPath.isEmpty());updateCopyActions();
    }
    void activateDraft(std::uint64_t key) {
        if(!worldDrafts.count(key))return;
        cacheDraft();
        auto draft=*worldDrafts.at(key);auto previous=activeDraft;canvas->cellSelection={};referenceCanvas->cellSelection={};
        canvas->cancelInteraction();construction.stop();
        if(referenceDraft==key && previous!=key && worldDrafts.count(previous))referenceDraft=previous;
        activeDraft=key;building=std::make_unique<Building>(draft.building);savedScene=draft.saved;savedPath=draft.path;draftName=draft.name;
        undoObjects=draft.undo;redoObjects=draft.redo;visibleObjects=building->objects.size();populatePalette();libraryTabs->setCurrentIndex(0);
        showBuilding(!draft.hasView,&draft.selection);
        if(draft.hasView){canvas->setTransform(draft.view);canvas->centerOn(draft.center);}
        refreshWorkspace();if(sideAction->isChecked())renderReference();
    }
    bool saveActive(QString path={},bool saveAs=false) {
        if(!building)return false;
        if(path.isEmpty() && !saveAs)path=savedPath;
        if(path.isEmpty())path=QFileDialog::getSaveFileName(this,"Save zone to a project folder",savedPath.isEmpty()?QDir::homePath()+"/zone-worlded":savedPath,"Project folder (*)",nullptr,QFileDialog::DontConfirmOverwrite);
        if(path.isEmpty())return false;
        try{save_scene_project(*building,fs::u8path(path.toStdString()));savedPath=QFileInfo(path).canonicalFilePath();savedScene=std::make_shared<Building>(*building);refreshWorkspace();
            try{remember_project_installation(savedPath,installationPath);statusBar()->showMessage("Zone saved: "+savedPath);}catch(const std::exception& e){statusBar()->showMessage("Zone saved; cannot link installation: "+utf8(e.what()));}return true;}
        catch(const std::exception& e){QMessageBox::warning(this,"Cannot save",utf8(e.what()));return false;}
    }
    bool closeZone(std::uint64_t key,bool discard=false) {
        cacheDraft();if(!worldDrafts.count(key))return true;
        if(!discard && draftDirty(*worldDrafts.at(key))){auto choice=QMessageBox::warning(this,"Unsaved changes","Save changes to “"+worldDrafts.at(key)->name+"” before closing this zone?",QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Save);
            if(choice==QMessageBox::Cancel)return false;
        if(choice==QMessageBox::Save){activateDraft(key);if(!saveActive())return false;}}
        if(referenceDraft==key){referenceDraft=UINT64_MAX;sideAction->setChecked(false);referenceCanvas->cancelInteraction();referenceScene->clear();referenceItems.clear();}
        worldDrafts.erase(key);draftOrder.erase(std::remove(draftOrder.begin(),draftOrder.end(),key),draftOrder.end());
        if(activeDraft==key){canvas->cancelInteraction();building.reset();activeDraft=UINT64_MAX;showingBuilding=false;
            if(!draftOrder.empty())activateDraft(draftOrder.back());else{scene->clear();objectItems.clear();objectsList->clear();properties->clear();document->setPlainText("Open a zone from the map or a project.");preview->setCurrentWidget(document);title->setText("No zone open");origin->clear();previewStatus->clear();updateBuildingActions();}}
        refreshWorkspace();return true;
    }
    bool confirmLeave() {
        cacheDraft();QStringList names;for(auto key:draftOrder)if(draftDirty(*worldDrafts.at(key)))names<<worldDrafts.at(key)->name;
        if(names.empty())return true;
        auto choice=QMessageBox::warning(this,"Unsaved zones","These zones contain changes:\n\n"+names.join("\n")+"\n\nSave before continuing?",QMessageBox::SaveAll|QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::SaveAll);
        if(choice==QMessageBox::Cancel)return false;
        if(choice==QMessageBox::Discard)return true;
        for(auto key:draftOrder)if(draftDirty(*worldDrafts.at(key))){activateDraft(key);if(!saveActive())return false;}
        return true;
    }
    void closeEvent(QCloseEvent* event) override {if(confirmLeave())event->accept();else event->ignore();}
    void openBeside() {
        if(!installation)return;
        QDialog dialog(this);dialog.setWindowTitle("Open a zone beside this one");QFormLayout layout(&dialog);QSpinBox x,y;x.setRange(0,1999);y.setRange(0,1999);
        if(building && building->existing_map){x.setValue(int(building->sector_id&0x3ffffff));y.setValue(int(building->sector_id>>26));}
        layout.addRow("Sector X",&x);layout.addRow("Sector Y",&y);QDialogButtonBox buttons(QDialogButtonBox::Open|QDialogButtonBox::Cancel);layout.addRow(&buttons);
        connect(&buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        if(dialog.exec()!=QDialog::Accepted){if(referenceDraft==UINT64_MAX)sideAction->setChecked(false);return;}
        openSectorBeside({x.value(),y.value()});
    }
    void openSectorBeside(QPoint point) {
        cacheDraft();auto previous=activeDraft;
        if(worldMapName.empty())showWorld();
        openWorldSector(point);auto opened=activeDraft;
        if(worldDrafts.count(previous) && previous!=opened)activateDraft(previous);
        if(opened==previous){if(referenceDraft==UINT64_MAX){QSignalBlocker block(sideAction);sideAction->setChecked(false);}statusBar()->showMessage("This zone is already active. Choose another reference zone.");return;}
        referenceDraft=opened;{QSignalBlocker block(sideAction);sideAction->setChecked(true);}referencePanel->show();refreshWorkspace();renderReference();zoneViews->setSizes({500,500});QTimer::singleShot(0,this,[this]{if(editing())showBuilding(true);fitReference();});
    }
    void renderReference() {
        if(!worldDrafts.count(referenceDraft) || !assets)return;
        referenceCanvas->cancelInteraction();QSignalBlocker block(referenceScene);referenceScene->clear();referenceItems.clear();
        const auto& draft=*worldDrafts.at(referenceDraft);std::vector<GameObject> effective;for(const auto& o:draft.building.objects)effective.push_back(assets->effective(o));int missing=0;
        drawScene(referenceScene,draft.building.ground,effective,referenceItems,missing);
        try{referenceContext=buildContext(draft.building);drawContext(referenceScene,referenceContext,true);}catch(const std::exception& e){referenceState->setText(utf8(e.what()));}referenceCanvas->setSelectionOnly(true);
        if(draft.hasView){referenceCanvas->setTransform(draft.view);referenceCanvas->centerOn(draft.center);}else referenceCanvas->fit();
        referenceState->setText("Reference · "+draft.name+(canvas->layer==1 || canvas->layer==2?"\nDrag to select the active layer. Ctrl+C to copy.":canvas->layer>=3?"\nCharacters and scripts are read only.":"\nBox-select or Ctrl+click. Copy, then Paste into the active zone."));
        referencePanel->setVisible(sideAction->isChecked());updateCopyActions();
    }
    void updateCopyActions() {
        bool selection=referenceFocused?!referenceScene->selectedItems().empty():editing() && !scene->selectedItems().empty();if(canvas->layer==1 || canvas->layer==2)selection=!(referenceFocused?referenceCanvas:canvas)->cellSelection.isEmpty();copyAction->setEnabled(selection);
        pasteAction->setEnabled(editing() && !copied.empty());
        pasteAction->setToolTip(copied.empty()?"Copy a selection from a zone first.":QString("%1 objects from %2 · click to place · Esc to cancel").arg(copied.objects.size()).arg(copied.source));
    }
    void copySelection(bool reference) {
        if(!installation)return;
        if(canvas->layer==1 || canvas->layer==2){copySurface(reference);return;}
        cacheDraft();const Building* source=nullptr;std::set<std::string> selection;QString name;
        if(reference){if(!worldDrafts.count(referenceDraft))return;source=&worldDrafts.at(referenceDraft)->building;name=worldDrafts.at(referenceDraft)->name;
            for(auto item:referenceScene->selectedItems())if(item->data(0).isValid()){auto index=item->data(0).toUInt();if(index<source->objects.size())selection.insert(objectKey(source->objects[index]));}}
        else if(editing()){source=building.get();selection=selectedIds();name=draftName;}
        if(!source || selection.empty()){statusBar()->showMessage("Select objects to copy using a box or Ctrl+click.");return;}
        try{auto fragment=copy_fragment(*installation,*source,selection,copyFloor->isChecked());fragment.source=name;copied=std::move(fragment);updateCopyActions();
            statusBar()->showMessage(QString("%1 objects copied from %2. Paste, then click in the active zone. Copied chests are empty; quest scripts are not copied.").arg(copied.objects.size()).arg(name));}
        catch(const std::exception& e){QMessageBox::warning(this,"Cannot copy",utf8(e.what()));}
    }
    void beginPaste() {
        if(!editing() || copied.empty())return;
        if(!copied.roofs.empty())editLayer->setCurrentIndex(2);else if(copied.objects.empty())editLayer->setCurrentIndex(1);else editLayer->setCurrentIndex(0);
        canvas->cancelInteraction();auto group=new QGraphicsItemGroup;scene->addItem(group);
        auto addSprite=[&](unsigned aid,QPointF position,bool floor){try{auto sprite=assets->sprite(aid);auto item=new QGraphicsPixmapItem(sprite.pixels,group);item->setPos(position-(floor?QPointF{}:sprite.hotspot));item->setZValue(floor?-10000:position.y());}catch(const std::exception&){auto item=new QGraphicsEllipseItem(-4,-4,8,8,group);item->setBrush(QColor("#e9a25a"));item->setPos(position);}};
        for(const auto& [xy,aid]:copied.tiles)addSprite(aid,QPointF((xy.second-xy.first)*40-40+((aid>>28)==11?1:0),(xy.first+xy.second)*20),true);
        for(const auto& [xy,aid]:copied.roofs)if(aid!=0xffffffff)addSprite(aid,Canvas::project({xy.first+2,xy.second+2})-QPointF(160,220),false);
        for(const auto& o:copied.objects){auto e=assets->effective(o);auto loc=e.number("LOCATION");QPoint tile(loc&63,(loc>>32)&63);addSprite(unsigned(e.number("CURRENT_AID",e.number("AID"))),Canvas::project(tile)+QPointF(std::int32_t(e.number("OFFSET_X")),std::int32_t(e.number("OFFSET_Y"))),false);}
        canvas->beginPlacement(group,{copied.width,copied.height});statusBar()->showMessage(QString("Place %1 objects · click in the active zone · Esc: cancel").arg(copied.objects.size()));
    }
    bool pasteAt(QPoint tile) {
        if(!editing() || copied.empty() || tile.x()<0 || tile.y()<0 || tile.x()+copied.width>64 || tile.y()+copied.height>64)return false;
        if(!copied.roofs.empty() && (tile.x()%4 || tile.y()%4))return false;
        auto next=building->objects;auto ground=building->ground;std::set<std::string> selected;
        for(const auto& source:copied.objects){auto object=clone_instance(source);auto loc=object.number("LOCATION");set_number(object,"LOCATION",std::uint64_t((loc&63)+tile.x())|(std::uint64_t(((loc>>32)&63)+tile.y())<<32));selected.insert(object.id);next.push_back(std::move(object));}
        for(const auto& [xy,aid]:copied.tiles)ground.set_tile(xy.first+tile.x(),xy.second+tile.y(),aid);
        for(const auto& [xy,aid]:copied.roofs)ground.set_roof((xy.first+tile.x())/4,(xy.second+tile.y())/4,aid);
        building->prototypes.insert(copied.prototypes.begin(),copied.prototypes.end());commitEdit(!copied.action.isEmpty()?copied.action:!copied.roofs.empty()?"Paste roofs":copied.objects.empty()?"Paste ground":"Paste building",std::move(next),selected,&ground);if(copied.action=="Place facade")libraryTabs->setCurrentWidget(facadePanel);return true;
    }

    void showCanvasMenu(Canvas* view,QPoint point,bool reference) {
        view->cancelInteraction();referenceFocused=reference;
        if(view->layer==0 || view->layer==3)for(auto item:view->items(point))if(item->data(0).isValid() && (item->flags()&QGraphicsItem::ItemIsSelectable)){
            if(!item->isSelected()){view->scene()->clearSelection();item->setSelected(true);}break;
        }
        updateCopyActions();QMenu menu(this);
        if(!reference && editing()){menu.addAction("Sector properties…",this,[this]{configureSector();});menu.addAction("Sector script…",this,[this]{editGroundScript(UINT32_MAX);});auto cell=view->surfaceTile(point);if(Canvas::valid(cell))menu.addAction("Tile script…",this,[this,cell]{editGroundScript(cell.y()*64+cell.x());});menu.addSeparator();}
        if(view->layer==1 || view->layer==2){menu.addAction("Sample this cell",this,[this,view,point,reference]{sampleSurface(reference,view->surfaceTile(point));});if(!reference)menu.addAction("Apply selected material",this,[this]{applySurface(false);});menu.addSeparator();}
        if(reference){menu.addAction("Copy selection",this,[this]{copySelection(true);})->setEnabled(!referenceScene->selectedItems().empty() || !referenceCanvas->cellSelection.isEmpty());menu.addAction("Edit this zone",this,[this]{activateDraft(referenceDraft);});}
        else if(editing()){menu.addAction(undoHouse);menu.addAction(redoHouse);menu.addSeparator();menu.addAction(copyAction);menu.addAction(pasteAction);menu.addAction(duplicateHouse);menu.addAction(deleteHouse);menu.addSeparator();menu.addAction(saveAction);}
        else if(building)menu.addAction(returnHouse);
        menu.addSeparator();menu.addAction("Fit view",view,[view]{view->fit();});menu.addAction("Zoom 100 %",view,[view]{view->resetTransform();});
        menu.exec(view->viewport()->mapToGlobal(point));
    }

    void fitReference() {
        if(!worldDrafts.count(referenceDraft))return;
        const auto& draft=worldDrafts.at(referenceDraft)->building;QRectF bounds;
        for(std::size_t i=0;i<referenceItems.size();i++)if(draft.objects[i].type<=2)bounds=bounds.united(referenceItems[i]->sceneBoundingRect());
        if(bounds.isEmpty()){referenceCanvas->fit();return;}
        referenceCanvas->fitInView(bounds.adjusted(-60,-90,60,80),Qt::KeepAspectRatio);
    }
