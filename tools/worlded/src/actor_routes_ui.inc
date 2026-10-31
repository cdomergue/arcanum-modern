    std::uint64_t routeSector=0;
    QTableWidget* routeTable=nullptr;
    QStringList initialRoutes;
    struct Standpoint {const char* field;QLineEdit *x,*y;QString initialX,initialY;};
    std::vector<Standpoint> standpoints;
    static std::uint64_t routeValue(const QString& x,const QString& y){
        bool okX=false,okY=false;auto vx=x.toULongLong(&okX),vy=y.toULongLong(&okY);
        if(!okX || !okY || vx>INT32_MAX || vy>INT32_MAX)throw std::runtime_error("Route coordinates must be between 0 and 2147483647.");
        return vx|(vy<<32);
    }
    std::uint64_t authoredRoute(const QString& x,const QString& y)const{
        auto loc=routeValue(x,y);
        if(routeSector && std::uint32_t(loc)<64 && std::uint32_t(loc>>32)<64)
            throw std::runtime_error("A destination in world sector 0, 0 cannot be represented from this sector: those coordinates are reserved for local project positions.");
        return worlded::npc_route_local(loc,routeSector);
    }
    void routes(){
        auto page=new QWidget;auto layout=new QVBoxLayout(page);
        auto hint=new QLabel("Positions use world tile coordinates, including destinations outside this sector. Day runs from 06:00 to 21:00. Patrol follows the rows in order and repeats; enable Day waypoints or Night waypoints in Behavior.");hint->setWordWrap(true);layout->addWidget(hint);
        auto positions=new QFormLayout;layout->addLayout(positions);
        for(auto name:{"NPC_STANDPOINT_DAY","NPC_STANDPOINT_NIGHT"}){
            auto loc=worlded::npc_route_global(effective.number(name,original.number("LOCATION")),routeSector);
            auto row=new QWidget;auto box=new QHBoxLayout(row);box->setContentsMargins(0,0,0,0);
            auto sx=new QLineEdit(QString::number(std::uint32_t(loc))),sy=new QLineEdit(QString::number(std::uint32_t(loc>>32)));
            sx->setObjectName(QString(name)+"_X");sy->setObjectName(QString(name)+"_Y");box->addWidget(new QLabel("X"));box->addWidget(sx);box->addWidget(new QLabel("Y"));box->addWidget(sy);
            positions->addRow(QString(name).endsWith("DAY")?"Day position":"Night position",row);standpoints.push_back({name,sx,sy,sx->text(),sy->text()});
        }
        routeTable=new QTableWidget;routeTable->setObjectName("NPC_WAYPOINTS_IDX");routeTable->setColumnCount(2);routeTable->setHorizontalHeaderLabels({"World X","World Y"});routeTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);layout->addWidget(routeTable);
        if(auto field=effective.field("NPC_WAYPOINTS_IDX"))for(auto& e:field->elements){if(e.data.size()!=8)continue;std::uint64_t loc=0;for(int i=0;i<8;i++)loc|=std::uint64_t(e.data[i])<<(i*8);appendWaypoint(worlded::npc_route_global(loc,routeSector));}
        initialRoutes=tableText(routeTable);
        auto actions=new QHBoxLayout;layout->addLayout(actions);auto add=new QPushButton("Add waypoint"),remove=new QPushButton("Remove"),up=new QPushButton("Move up"),down=new QPushButton("Move down");for(auto b:{add,remove,up,down})actions->addWidget(b);
        connect(add,&QPushButton::clicked,this,[this]{appendWaypoint(worlded::npc_route_global(original.number("LOCATION"),routeSector));routeTable->setCurrentCell(routeTable->rowCount()-1,0);});
        connect(remove,&QPushButton::clicked,this,[this]{if(routeTable->currentRow()>=0)routeTable->removeRow(routeTable->currentRow());});
        auto move=[this](int delta){int from=routeTable->currentRow(),to=from+delta;if(from<0 || to<0 || to>=routeTable->rowCount())return;for(int c=0;c<2;c++){auto a=routeTable->takeItem(from,c),b=routeTable->takeItem(to,c);routeTable->setItem(from,c,b);routeTable->setItem(to,c,a);}routeTable->setCurrentCell(to,0);};
        connect(up,&QPushButton::clicked,this,[move]{move(-1);});connect(down,&QPushButton::clicked,this,[move]{move(1);});tabs->addTab(page,"Daily routine");
    }
    void appendWaypoint(std::uint64_t loc){int row=routeTable->rowCount();routeTable->insertRow(row);routeTable->setItem(row,0,new QTableWidgetItem(QString::number(std::uint32_t(loc))));routeTable->setItem(row,1,new QTableWidgetItem(QString::number(std::uint32_t(loc>>32))));}
    void applyRoutes(worlded::GameObject& object){
        for(auto& p:standpoints)if(p.x->text()!=p.initialX || p.y->text()!=p.initialY)worlded::set_number(object,p.field,authoredRoute(p.x->text(),p.y->text()));
        if(!routeTable || tableText(routeTable)==initialRoutes)return;
        if(routeTable->rowCount()>4096)throw std::runtime_error("A patrol may contain at most 4096 waypoints.");
        worlded::ObjectField field;field.name="NPC_WAYPOINTS_IDX";field.kind="UINT64_ARRAY";field.present=true;
        for(int row=0;row<routeTable->rowCount();row++){auto loc=authoredRoute(routeTable->item(row,0)->text(),routeTable->item(row,1)->text());worlded::Bytes bytes(8);for(int i=0;i<8;i++)bytes[i]=std::uint8_t(loc>>(8*i));field.elements.push_back({unsigned(row),bytes});}
        put(object,std::move(field));worlded::set_number(object,"NPC_WAYPOINT_CURRENT",0);
    }

    QCheckBox* generatorEnabled=nullptr;
    QSpinBox *generatorId=nullptr,*generatorConcurrent=nullptr,*generatorTotal=nullptr;
    QComboBox* generatorRate=nullptr;
    std::vector<QCheckBox*> generatorFlags;
    QStringList initialGenerator;
    QStringList generatorState()const{QStringList state;state<<QString::number(generatorEnabled->isChecked())<<QString::number(generatorId->value())<<QString::number(generatorConcurrent->value())<<QString::number(generatorTotal->value())<<QString::number(generatorRate->currentIndex());for(auto box:generatorFlags)state<<QString::number(box->isChecked());return state;}
    void generator(){
        auto page=form("Generator");auto hint=new QLabel("A generator is an invisible NPC template that spawns copies. Its ID is shared across the campaign: reserve an unused ID (0–255). Existing save counters are not reset by editing these limits.");hint->setWordWrap(true);page->addRow(hint);
        auto data=std::uint32_t(effective.number("NPC_GENERATOR_DATA"));auto flags=effective.number("NPC_FLAGS");
        generatorEnabled=new QCheckBox("Use this NPC as a generator");generatorEnabled->setObjectName("generatorEnabled");generatorEnabled->setChecked(flags&0x80000);page->addRow(generatorEnabled);
        auto spin=[&](const char* name,const char* label,int maximum,int value){auto field=new QSpinBox;field->setObjectName(name);field->setRange(0,maximum);field->setValue(value);page->addRow(label,field);return field;};
        generatorId=spin("generatorId","Campaign generator ID",255,(data>>19)&255);generatorConcurrent=spin("generatorConcurrent","Maximum alive at once",31,(data>>14)&31);generatorTotal=spin("generatorTotal","Maximum spawned in total",127,(data>>7)&127);
        generatorRate=new QComboBox;generatorRate->addItems({"Every second","Every 30 seconds","Every minute","Every hour","Every day","Every week","Every month","Every year"});generatorRate->setCurrentIndex((flags>>21)&7);page->addRow("Spawn interval",generatorRate);
        unsigned bit=27;for(auto text:{"Spawn during daytime","Spawn at night","Pause while on screen","Fill the alive quota at once","Ignore the total limit"}){auto box=new QCheckBox(text);box->setChecked(data&(1u<<bit++));generatorFlags.push_back(box);page->addRow(box);}
        initialGenerator=generatorState();
    }
    void applyGenerator(worlded::GameObject& object){
        if(!generatorEnabled || generatorState()==initialGenerator)return;
        auto data=std::uint32_t(effective.number("NPC_GENERATOR_DATA"))&127u;data|=unsigned(generatorTotal->value())<<7;data|=unsigned(generatorConcurrent->value())<<14;data|=unsigned(generatorId->value())<<19;for(unsigned i=0;i<generatorFlags.size();i++)if(generatorFlags[i]->isChecked())data|=1u<<(27+i);
        auto flags=object.number("NPC_FLAGS",effective.number("NPC_FLAGS"));flags=(flags&~0xe80000ull)|(unsigned(generatorRate->currentIndex())<<21)|(generatorEnabled->isChecked()?0x80000:0);worlded::set_number(object,"NPC_FLAGS",flags);worlded::set_number(object,"NPC_GENERATOR_DATA",data);
        // Match monstergen_set/remove. Do not disturb unrelated object flags.
        if(generatorEnabled->isChecked()!=bool(effective.number("NPC_FLAGS")&0x80000)){auto f=object.number("FLAGS",effective.number("FLAGS"));worlded::set_number(object,"FLAGS",generatorEnabled->isChecked()?f|0x400002ull:f&~0x400002ull);}
    }

    struct Knowledge {unsigned index;QSpinBox* value;int initial;};
    std::vector<Knowledge> knowledgeFields;
    void knowledge(const worlded::Installation* installation){
        auto page=form("Spells and technology");auto hint=new QLabel("College level grants the first 0–5 spells in that college. Technology degrees range from 0 to 7. These values do not spend character points.");hint->setWordWrap(true);page->addRow(hint);
        std::map<unsigned,std::string> magic,tech;auto read=[&](const char* path,std::map<unsigned,std::string>& messages){if(installation && installation->contains(path))for(auto& entry:worlded::read_messages(installation->read(path)))messages[entry.id]=worlded::cp1252_utf8(entry.text);};read("mes/spell.mes",magic);read("mes/tech.mes",tech);
        auto field=effective.field("CRITTER_SPELL_TECH_IDX");
        for(unsigned i=0;i<25;i++){unsigned raw=i==16?UINT32_MAX:0;if(field)for(auto& e:field->elements)if(e.index==i && e.data.size()==4)raw=worlded::word(e.data,0);auto value=new QSpinBox;value->setObjectName(QString("knowledge_%1").arg(i));value->setRange(i==16?-1:0,i<16?5:i==16?15:7);if(i==16)value->setSpecialValueText("None");value->setValue(qint32(raw));QString name;if(i<16)name=magic.count(500+i)?QString::fromStdString(magic[500+i]):QString("Spell college %1").arg(i);else if(i==16)name="Mastered college (0–15)";else name=tech.count(i-17)?QString::fromStdString(tech[i-17]):QString("Technology %1").arg(i-17);page->addRow(name,value);knowledgeFields.push_back({i,value,value->value()});}
    }
    void applyKnowledge(worlded::GameObject& object){
        if(std::none_of(knowledgeFields.begin(),knowledgeFields.end(),[](auto& k){return k.value->value()!=k.initial;}))return;
        worlded::ObjectField field;if(auto existing=effective.field("CRITTER_SPELL_TECH_IDX"))field=*existing;field.name="CRITTER_SPELL_TECH_IDX";field.kind="UINT32_ARRAY";field.present=true;
        for(auto& k:knowledgeFields)if(k.value->value()!=k.initial){auto it=std::find_if(field.elements.begin(),field.elements.end(),[&](auto& e){return e.index==k.index;});worlded::Bytes bytes(4);auto value=std::uint32_t(k.value->value());for(int i=0;i<4;i++)bytes[i]=std::uint8_t(value>>(8*i));if(it==field.elements.end())field.elements.push_back({k.index,bytes});else it->data=bytes;}
        std::sort(field.elements.begin(),field.elements.end(),[](auto& a,auto& b){return a.index<b.index;});put(object,std::move(field));
    }
