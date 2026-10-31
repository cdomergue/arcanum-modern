    bool editLight(Bytes& bytes) {
        QDialog dialog(this);dialog.setWindowTitle("Light");QFormLayout form(&dialog);
        auto read=[&](unsigned offset){return word(bytes,offset);};
        QSpinBox x,y,dx,dy,rotation,frame;x.setObjectName("lightX");y.setObjectName("lightY");rotation.setObjectName("lightRotation");frame.setObjectName("lightFrame");x.setRange(0,63);y.setRange(0,63);x.setValue(read(8)&63);y.setValue(read(12)&63);
        dx.setRange(-4096,4096);dy.setRange(-4096,4096);dx.setValue(std::int32_t(read(16)));dy.setValue(std::int32_t(read(20)));
        auto aid=read(28);QComboBox art;art.setObjectName("lightArt");
        if(installation->contains("art/light/light.mes"))for(auto m:read_messages(installation->read("art/light/light.mes")))if(m.id<512)art.addItem(utf8(cp1252_utf8(m.text)),m.id);
        auto current=(aid>>19)&511;int index=art.findData(current);if(index<0){art.addItem(QString("Unlisted light #%1").arg(current),current);index=art.count()-1;}art.setCurrentIndex(index);
        rotation.setRange(0,31);rotation.setValue((aid&1)?(aid>>4)&31:(aid>>9)&7);frame.setRange(0,127);frame.setValue((aid>>12)&127);
        QCheckBox off("Disabled"),dark("Dark light / shadow"),animate("Animate"),indoor("Indoors"),outdoor("Outdoors"),projected("Directional projection (32 directions)");
        auto flags=read(24);off.setChecked(flags&1);dark.setChecked(flags&2);animate.setChecked(flags&4);indoor.setChecked(flags&8);outdoor.setChecked(flags&16);projected.setChecked(aid&1);
        QColor color(bytes[32],bytes[34],bytes[33]);QPushButton colorButton(color.name());connect(&colorButton,&QPushButton::clicked,&dialog,[&]{auto chosen=QColorDialog::getColor(color,&dialog,"Light color");if(chosen.isValid()){color=chosen;colorButton.setText(color.name());}});
        form.addRow("Cell X",&x);form.addRow("Cell Y",&y);form.addRow("Pixel offset X",&dx);form.addRow("Pixel offset Y",&dy);form.addRow("Light shape",&art);form.addRow("Color",&colorButton);
        form.addRow(&projected);form.addRow("Direction",&rotation);form.addRow("Initial frame",&frame);form.addRow(&off);form.addRow(&dark);form.addRow(&animate);form.addRow(&indoor);form.addRow(&outdoor);
        QLabel colorHint("Indoor/outdoor lights follow the sector lighting schedule. Leave both unchecked to use the chosen color.");colorHint.setWordWrap(true);form.addRow(&colorHint);
        QLabel error;form.addRow(&error);QDialogButtonBox buttons(QDialogButtonBox::Save|QDialogButtonBox::Cancel);form.addRow(&buttons);connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        connect(&buttons,&QDialogButtonBox::accepted,&dialog,[&]{if(indoor.isChecked() && outdoor.isChecked()){error.setText("Choose either indoor or outdoor ambient color, or neither for a custom color.");return;}if(!projected.isChecked() && rotation.value()>7){error.setText("Ordinary lights have directions 0–7.");return;}try{auto selectedAid=0x90000000u|(art.currentData().toUInt()<<19)|(frame.value()<<12)|((rotation.value()&7)<<9)|(rotation.value()<<4)|(projected.isChecked()?1:0);auto sprite=assets->sprite(selectedAid);Art source(installation->read(sprite.path));if(unsigned(frame.value())>=source.frames())throw std::runtime_error("Initial frame is outside this light animation");}catch(const std::exception& e){error.setText(e.what());return;}dialog.accept();});
        if(dialog.exec()!=QDialog::Accepted)return false;
        auto put=[&](unsigned offset,std::uint32_t value){for(int b=0;b<4;b++)bytes[offset+b]=value>>(8*b);};
        put(8,std::uint32_t((building->sector_id&0x3ffffff)*64+x.value()));put(12,std::uint32_t((building->sector_id>>26)*64+y.value()));put(16,dx.value());put(20,dy.value());
        put(24,(flags&~31u)|(off.isChecked()?1:0)|(dark.isChecked()?2:0)|(animate.isChecked()?4:0)|(indoor.isChecked()?8:0)|(outdoor.isChecked()?16:0));
        put(28,0x90000000u|(art.currentData().toUInt()<<19)|(frame.value()<<12)|((rotation.value()&7)<<9)|(rotation.value()<<4)|(projected.isChecked()?1:0));
        bytes[32]=color.red();bytes[33]=color.blue();bytes[34]=color.green();return true;
    }
    void configureLights() {
        if(!editing())return;
        auto lights=building->ground.lights();QDialog dialog(this);dialog.setWindowTitle("Sector lights");dialog.resize(620,520);QVBoxLayout layout(&dialog);
        QLabel hint("Independent lights in this sector. Color, dark shadows, animation and directional projection use the native engine settings.");hint.setWordWrap(true);layout.addWidget(&hint);
        QListWidget list;layout.addWidget(&list);auto refresh=[&]{int selected=list.currentRow();list.clear();for(auto& bytes:lights){auto flags=word(bytes,24);list.addItem(QString("%1 · cell %2, %3 · #%4%5").arg(flags&2?"Shadow":"Light").arg(word(bytes,8)&63).arg(word(bytes,12)&63).arg((word(bytes,28)>>19)&511).arg(flags&1?" · disabled":""));}if(list.count())list.setCurrentRow(std::clamp(selected,0,list.count()-1));};refresh();
        QHBoxLayout actions;QPushButton add("Add light…"),edit("Edit…"),remove("Remove");actions.addWidget(&add);actions.addWidget(&edit);actions.addWidget(&remove);layout.addLayout(&actions);
        connect(&add,&QPushButton::clicked,&dialog,[&]{Bytes light(48,0);light[31]=0x90;light[32]=light[33]=light[34]=255;light[8]=light[12]=32;if(editLight(light)){lights.push_back(light);refresh();list.setCurrentRow(list.count()-1);}});
        auto editCurrent=[&]{int row=list.currentRow();if(row>=0 && editLight(lights[row]))refresh();};connect(&edit,&QPushButton::clicked,&dialog,editCurrent);connect(&list,&QListWidget::itemDoubleClicked,&dialog,[&]{editCurrent();});
        connect(&remove,&QPushButton::clicked,&dialog,[&]{int row=list.currentRow();if(row>=0){lights.erase(lights.begin()+row);refresh();}});
        QDialogButtonBox buttons(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout.addWidget(&buttons);connect(&buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        if(dialog.exec()==QDialog::Accepted && lights!=building->ground.lights()){auto ground=building->ground;ground.set_lights(lights);commitEdit("Edit lights",building->objects,{},&ground);}
    }
    void drawLights(QGraphicsScene* target,const Sector& sector) {
        for(const auto& bytes:sector.lights()){
            auto flags=word(bytes,24);if(flags&1)continue;
            auto position=Canvas::project({int(word(bytes,8)&63),int(word(bytes,12)&63)})+QPointF(std::int32_t(word(bytes,16)),std::int32_t(word(bytes,20)));
            try{auto sprite=assets->sprite(word(bytes,28));auto image=sprite.pixels.toImage().convertToFormat(QImage::Format_ARGB32);
                QColor tint=flags&2?QColor(0,0,0):QColor(bytes[32],bytes[34],bytes[33]);
                for(int y=0;y<image.height();y++)for(int x=0;x<image.width();x++){auto c=image.pixelColor(x,y);int intensity=std::max({c.red(),c.green(),c.blue()});auto color=tint;color.setAlpha(intensity*c.alpha()/510);image.setPixelColor(x,y,color);}
                auto glow=target->addPixmap(QPixmap::fromImage(image));glow->setPos(position-sprite.hotspot);glow->setZValue(20000);glow->setToolTip("Light preview · final lighting is evaluated in game");
            }catch(const std::exception&){}
            auto marker=target->addEllipse(-5,-5,10,10,QPen(QColor("#ffe69c")),QBrush(QColor(bytes[32],bytes[34],bytes[33])));marker->setPos(position);marker->setZValue(20001);
            marker->setToolTip(QString("Light · %1, %2 · Zone > Lights to edit").arg(word(bytes,8)&63).arg(word(bytes,12)&63));
        }
    }
