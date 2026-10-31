    Facade facade(unsigned id) const {
        return read_facade(installation->read(QString("art/facade/facwalk.%1").arg(id,2,16,QChar('0')).toStdString()),id);
    }
    QPixmap facadePreview(unsigned id) {
        auto shape=facade(id);QGraphicsScene preview;
        for(const auto& [cell,aid]:shape.tiles){auto sprite=assets->sprite(aid);auto item=preview.addPixmap(sprite.pixels);
            item->setPos(Canvas::project({cell.first,cell.second})+QPointF(-39,0));}
        auto bounds=preview.itemsBoundingRect();QPixmap result(220,160);result.fill(Qt::transparent);
        QPainter painter(&result);preview.render(&painter,QRectF(0,0,220,160),bounds,Qt::KeepAspectRatio);return result;
    }
    void populateFacades() {
        if(!installation || facadePalette->count())return;
        if(!installation->contains("art/facade/facadename.mes"))return;
        for(const auto& entry:read_messages(installation->read("art/facade/facadename.mes"))) {
            if(entry.id>=512)continue;
            auto item=new QListWidgetItem(utf8(cp1252_utf8(entry.text)),facadePalette);item->setData(Qt::UserRole,entry.id);item->setSizeHint(QSize(200,106));
            try{auto shape=facade(entry.id);unsigned walkable=0;for(auto cell:shape.tiles)walkable+=cell.second&1;
                item->setToolTip(QString("%1 × %2 cells · %3 pieces · %4 walkable\nNative orientation and collision mask. Transparent cells keep their ground.")
                    .arg(shape.width).arg(shape.height).arg(shape.tiles.size()).arg(walkable));}
            catch(const std::exception& e){item->setFlags(item->flags()&~Qt::ItemIsEnabled);item->setToolTip(utf8(e.what()));}
        }
        facadePalette->sortItems();filterFacades();
    }
    void filterFacades(){for(int i=0;i<facadePalette->count();i++){auto item=facadePalette->item(i);item->setHidden(!item->text().contains(facadeSearch->text(),Qt::CaseInsensitive));}loadFacadeIcons();}
    void loadFacadeIcons(){if(!assets)return;for(int i=0;i<facadePalette->count();i++){auto item=facadePalette->item(i);
        if(item->isHidden() || !(item->flags()&Qt::ItemIsEnabled) || !item->icon().isNull() || !facadePalette->visualItemRect(item).intersects(facadePalette->viewport()->rect()))continue;
        try{item->setIcon(QIcon(facadePreview(item->data(Qt::UserRole).toUInt())));}catch(const std::exception& e){item->setToolTip(utf8(e.what()));item->setFlags(item->flags()&~Qt::ItemIsEnabled);}
    }}
    void placeFacade() {
        if(!editing()){statusBar()->showMessage("Open an editable zone before placing a facade.");return;}
        auto item=facadePalette->currentItem();if(!item || !(item->flags()&Qt::ItemIsEnabled))return;
        try{auto shape=facade(item->data(Qt::UserRole).toUInt());copied={};copied.tiles=shape.tiles;copied.width=shape.width;copied.height=shape.height;
            copied.source=item->text();copied.action="Place facade";paintSurface->setChecked(false);beginPaste();libraryTabs->setCurrentWidget(facadePanel);
            statusBar()->showMessage(QString("Place %1 · %2 × %3 cells · click to place · Esc to cancel").arg(item->text()).arg(shape.width).arg(shape.height));}
        catch(const std::exception& e){QMessageBox::warning(this,"Cannot place facade",utf8(e.what()));}
    }
