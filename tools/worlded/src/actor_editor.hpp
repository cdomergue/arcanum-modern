#pragma once
#include "worlded/building.hpp"
#include "worlded/npc_routes.hpp"
#include "script_inspector.hpp"
#include <QtWidgets>
#include <functional>
#include <algorithm>
#include <climits>

// Edit a sparse instance; untouched prototype fields remain inherited.
class ActorEditor : public QDialog {
public:
    worlded::GameObject original, effective, result;
    QTabWidget* tabs=new QTabWidget;
    QLabel* error=new QLabel;
    struct Number {std::string name;QLineEdit* edit;QString initial;};
    struct Array {worlded::ObjectField field;QTableWidget* table;QStringList initial;};
    struct EffectBlend {std::string field;unsigned index;QCheckBox* box;bool initial;};
    std::vector<EffectBlend> effectBlends;
    std::vector<Number> numbers;
    std::vector<Array> arrays;
    QStringList trainingNames;
    QSpinBox *x=new QSpinBox,*y=new QSpinBox;
    QComboBox* facing=new QComboBox;
    std::function<void(unsigned)> openScript;
    std::function<worlded::Bytes(unsigned)> scriptDefaults;
    ActorEditor(const worlded::GameObject& instance,const worlded::GameObject& resolved,
                const QString& title,QWidget* parent=nullptr,const worlded::Installation* installation=nullptr,std::uint64_t sectorId=0):QDialog(parent),original(instance),effective(resolved),result(instance),routeSector(sectorId) {
        bool actor=instance.type==15 || instance.type==16;
        setWindowTitle((actor?"Configure character — ":"Configure object — ")+title);resize(860,690);setMinimumSize(650,480);
        auto layout=new QVBoxLayout(this);auto heading=new QLabel(title+QString(" · prototype #%1").arg(instance.prototype_id));heading->setObjectName("sectionTitle");layout->addWidget(heading);
        auto hint=new QLabel("Values come from the prototype. Only modified fields are stored on this object.");hint->setWordWrap(true);layout->addWidget(hint);layout->addWidget(tabs,1);
        auto general=form("General");auto location=new QWidget;auto row=new QHBoxLayout(location);row->setContentsMargins(0,0,0,0);x->setRange(0,63);y->setRange(0,63);x->setValue(instance.number("LOCATION")&63);y->setValue((instance.number("LOCATION")>>32)&63);
        x->setObjectName("actorX");y->setObjectName("actorY");row->addWidget(new QLabel("X"));row->addWidget(x);row->addWidget(new QLabel("Y"));row->addWidget(y);general->addRow("Tile within sector",location);
        for(int i=0;i<8;i++)facing->addItem(QString::number(i));
        facing->setCurrentIndex((effective.number("CURRENT_AID")>>11)&7);general->addRow("Facing (0–7)",facing);
        scalar(general,"DESCRIPTION","Name (MES ID)");if(actor){scalar(general,"CRITTER_DESCRIPTION_UNKNOWN","Unknown name (MES)");scalar(general,"CRITTER_PORTRAIT","Portrait");}scalar(general,"BLIT_SCALE","Scale (%)");scalar(general,"OFFSET_X","Horizontal offset (px)");scalar(general,"OFFSET_Y","Vertical offset (px)");
        if(installation && installation->contains("mes/description.mes")){QStringList choices;for(auto m:worlded::read_messages(installation->read("mes/description.mes")))choices<<QString("%1 · %2").arg(m.id).arg(QString::fromStdString(worlded::cp1252_utf8(m.text)));auto choose=new QPushButton("Choose an existing name…");general->addRow(choose);connect(choose,&QPushButton::clicked,this,[this,choices]{bool ok;auto choice=QInputDialog::getItem(this,"Character name","Installation descriptions",choices,0,true,&ok);if(ok && !choice.isEmpty())findChild<QLineEdit*>("DESCRIPTION")->setText(choice.section(" · ",0,0));});}
        flags(general,"FLAGS",{{1,"Destroyed"},{2,"Off"},{0x100,"Don't draw"},{0x200,"Invisible"},{0x400,"No block"},{0x800,"Click through"},{0x400000,"Invulnerable"}});
        if(actor){
        auto health=form("Vitality");for(auto f:{"HP_PTS","HP_ADJ","HP_DAMAGE","AC","CRITTER_FATIGUE_PTS","CRITTER_FATIGUE_ADJ","CRITTER_FATIGUE_DAMAGE"})scalar(health,f,label(f));
        auto names=[installation](const char* path,int first,int count){QStringList result;std::map<unsigned,std::string> messages;if(installation && installation->contains(path))for(auto m:worlded::read_messages(installation->read(path)))messages[m.id]=m.text;for(int i=0;i<count;i++){auto found=messages.find(first+i);result<< (found==messages.end()?QString::number(i):QString::fromStdString(worlded::cp1252_utf8(found->second)));}return result;};
        trainingNames=names("mes/skill.mes",16,4);if(trainingNames[0].trimmed().isEmpty())trainingNames[0]="—";
        array("Stats","CRITTER_STAT_BASE_IDX","INT32_ARRAY",names("mes/stat.mes",0,28));
        array("Skills","CRITTER_BASIC_SKILL_IDX","INT32_ARRAY",names("mes/skill.mes",0,12));
        array("Technical skills","CRITTER_TECH_SKILL_IDX","INT32_ARRAY",names("mes/skill.mes",12,4));
        array("Resistances","RESISTANCE_IDX","INT32_ARRAY");
        auto behavior=form("Behavior");for(auto f:{"NPC_AI_DATA","NPC_FACTION","NPC_REACTION_BASE","NPC_SOCIAL_CLASS","NPC_EXPERIENCE_WORTH","CRITTER_AUTO_LEVEL_SCHEME","NPC_RETAIL_PRICE_MULTIPLIER"})if(instance.type==16 || std::string(f).rfind("NPC_",0)!=0)scalar(behavior,f,label(f));
        flags(behavior,"CRITTER_FLAGS",{{4,"Undead"},{8,"Animal"},{0x1000,"Sleeping"},{0x2000,"Mute"},{0x8000,"Monster"},{0x8000000,"No flee"},{0x10000000,"Non-lethal combat"},{0x20000000,"Mechanical"}});flags(behavior,"CRITTER_FLAGS2",{{0x40,"No decay"},{0x80,"No pickpocket"},{0x800,"Dark sight"}});if(instance.type==16)flags(behavior,"NPC_FLAGS",{{2,"Day waypoints"},{4,"Night waypoints"},{8,"Wait here"},{0x100,"Kill on sight"},{0x800,"KOS override"},{0x1000,"Wanders"},{0x2000,"Wanders in dark"},{0x4000,"Fence"},{0x20000,"Aloof"}});
        auto inventory=form("Inventory");auto info=new QLabel("The inventory generator uses Rules/InvenSource.mes. References to existing items are preserved and can be inspected under Advanced.");info->setWordWrap(true);inventory->addRow(info);scalar(inventory,"CRITTER_INVENTORY_SOURCE","Inventory generator");
        }else {
            auto specific=form("Properties");
            const std::map<std::string,QString> labels={{"PORTAL_FLAGS","Door flags (bits)"},{"PORTAL_LOCK_DIFFICULTY","Lock difficulty"},{"PORTAL_KEY_ID","Key ID"},{"CONTAINER_FLAGS","Chest flags (bits)"},{"CONTAINER_LOCK_DIFFICULTY","Lock difficulty"},{"CONTAINER_KEY_ID","Key ID"},{"CONTAINER_INVENTORY_SOURCE","Content generator"}};
            for(const auto& entry:labels)if(effective.field(entry.first))scalar(specific,entry.first.c_str(),entry.second,entry.first.find("FLAGS")!=std::string::npos);
            scalar(specific,"HP_PTS","Hit points");scalar(specific,"HP_DAMAGE","Damage taken");
        }
        if(actor) knowledge(installation);
        if(instance.type==16){routes();generator();}
        effects();scripts();advanced();
        error->setWordWrap(true);error->setStyleSheet("color:#ffb080");layout->addWidget(error);
        auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Save)->setText("Apply to object");layout->addWidget(buttons);
        connect(buttons,&QDialogButtonBox::accepted,this,[this]{try{result=values();accept();}catch(const std::exception& e){error->setText(QString::fromUtf8(e.what()));}});connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    }
    static QString label(const char* text){static const std::map<std::string,QString> labels={{"HP_PTS","Base hit points"},{"HP_ADJ","Hit point bonus"},{"HP_DAMAGE","Damage taken"},{"AC","Armor class"},{"CRITTER_FATIGUE_PTS","Maximum fatigue"},{"CRITTER_FATIGUE_ADJ","Fatigue bonus"},{"CRITTER_FATIGUE_DAMAGE","Fatigue spent"},{"NPC_AI_DATA","AI profile"},{"NPC_FACTION","Faction"},{"NPC_REACTION_BASE","Base reaction"},{"NPC_SOCIAL_CLASS","Social class"},{"NPC_EXPERIENCE_WORTH","Experience awarded"},{"CRITTER_AUTO_LEVEL_SCHEME","Auto-level scheme"},{"NPC_RETAIL_PRICE_MULTIPLIER","Price multiplier"}};auto it=labels.find(text);return it==labels.end()?QString::fromLatin1(text):it->second;}
    QFormLayout* form(const QString& title){auto scroll=new QScrollArea;scroll->setWidgetResizable(true);auto page=new QWidget;auto f=new QFormLayout(page);f->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);f->setRowWrapPolicy(QFormLayout::WrapLongRows);scroll->setWidget(page);tabs->addTab(scroll,title);return f;}
    QLineEdit* scalar(QFormLayout* form,const char* name,const QString& title,bool hex=false){auto n=effective.number(name);auto value=hex?QString("0x%1").arg(quint32(n),8,16,QChar('0')):QString::number(qint32(n));auto edit=new QLineEdit(value);edit->setObjectName(QString::fromLatin1(name));edit->setToolTip(QString::fromLatin1(name)+(original.field(name)?" · instance value":" · inherited from prototype"));form->addRow(title,edit);numbers.push_back({name,edit,value});if(std::string(name).find("INVENTORY_NUM")!=std::string::npos)edit->setReadOnly(true);return edit;}
    void flags(QFormLayout* form,const char* name,std::initializer_list<std::pair<unsigned,const char*>> options){auto edit=scalar(form,name,QString::fromLatin1(name),true);if(!options.size())return;auto widget=new QWidget;auto grid=new QGridLayout(widget);int i=0;for(auto [mask,name]:options){auto box=new QCheckBox(name);box->setChecked(effective.number(numbers.back().name)&mask);grid->addWidget(box,i/3,i%3);++i;
            connect(box,&QCheckBox::toggled,this,[edit,mask](bool on){bool ok;auto value=edit->text().toUInt(&ok,0);if(ok)edit->setText(QString("0x%1").arg(on?value|mask:value&~mask,8,16,QChar('0')));});connect(edit,&QLineEdit::textChanged,box,[box,mask](const QString& text){bool ok;auto value=text.toUInt(&ok,0);if(ok){QSignalBlocker b(box);box->setChecked(value&mask);}});}
        form->addRow(widget);}
    static QStringList tableText(QTableWidget* table){QStringList result;for(int r=0;r<table->rowCount();r++)for(int c=0;c<table->columnCount();c++){if(auto combo=qobject_cast<QComboBox*>(table->cellWidget(r,c)))result<<QString::number(combo->currentIndex());else result<<(table->item(r,c)?table->item(r,c)->text():QString{});}return result;}
    void array(const QString& title,const char* name,const char* kind,const QStringList& labels={}){
        worlded::ObjectField field;if(auto f=effective.field(name))field=*f;else {field.name=name;field.kind=kind;field.present=true;}
        bool skills=field.name=="CRITTER_BASIC_SKILL_IDX" || field.name=="CRITTER_TECH_SKILL_IDX";
        auto panel=new QWidget;auto layout=new QVBoxLayout(panel);auto table=new QTableWidget;table->setObjectName(QString::fromLatin1(name));table->setColumnCount(skills?3:2);table->setHorizontalHeaderLabels(skills?QStringList{"Skill","Points (0–63)","Training"}:QStringList{"Stat","Value"});table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);if(skills)table->setColumnWidth(2,150);layout->addWidget(table);
        auto add=[table,labels,skills,trainingLabels=trainingNames](unsigned index,const QString& value){int r=table->rowCount();table->insertRow(r);auto key=new QTableWidgetItem(index<unsigned(labels.size())?QString("%1 · %2").arg(index).arg(labels[index]):QString::number(index));key->setData(Qt::UserRole,index);key->setFlags(key->flags()&~Qt::ItemIsEditable);table->setItem(r,0,key);table->setItem(r,1,new QTableWidgetItem(skills?QString::number(value.toUInt()&63):value));if(skills){auto training=new QComboBox;training->addItems(trainingLabels);training->setCurrentIndex((value.toUInt()>>6)&3);table->setCellWidget(r,2,training);}};
        for(auto& entry:field.elements){if(entry.data.size()!=4)continue;auto n=worlded::word(entry.data,0);add(entry.index,field.kind=="INT32_ARRAY"?QString::number(qint32(n)):QString::number(n));}
        // Absent entries must remain absent until the user supplies a value.
        auto buttons=new QHBoxLayout;auto plus=new QPushButton("Add value…"),minus=new QPushButton("Remove value");buttons->addWidget(plus);buttons->addWidget(minus);buttons->addStretch();layout->addLayout(buttons);
        connect(plus,&QPushButton::clicked,this,[this,table,add,labels]{bool ok;int index;if(labels.isEmpty())index=QInputDialog::getInt(this,"New value","Index",0,0,4095,1,&ok);else{auto choice=QInputDialog::getItem(this,"New value","Stat",labels,0,false,&ok);index=labels.indexOf(choice);}if(!ok || index<0)return;for(int r=0;r<table->rowCount();r++)if(table->item(r,0)->data(Qt::UserRole).toInt()==index){table->setCurrentCell(r,1);return;}add(index,"0");table->setCurrentCell(table->rowCount()-1,1);});connect(minus,&QPushButton::clicked,this,[table]{if(table->currentRow()>=0)table->removeRow(table->currentRow());});
        arrays.push_back({field,table,tableText(table)});tabs->addTab(panel,title);
    }
    QTableWidget* scriptTable=nullptr;QStringList initialScripts;worlded::ObjectField scriptField;
    void scripts(){auto page=new QWidget;auto layout=new QVBoxLayout(page);auto hint=new QLabel("Set a script ID for each event; 0 disables it. Flags accept decimal or 0x hexadecimal. Counters are four independent bytes (0–255). Double-click an event name to open its script.");hint->setWordWrap(true);layout->addWidget(hint);
        scriptTable=new QTableWidget;scriptTable->setObjectName("objectScripts");scriptTable->setColumnCount(7);scriptTable->setHorizontalHeaderLabels({"Event","Script ID","Flags","Counter 1","Counter 2","Counter 3","Counter 4"});scriptTable->setRowCount(int(std::size(worlded::attachmentNames)));scriptTable->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);for(int col=1;col<7;col++)scriptTable->setColumnWidth(col,col==2?110:85);layout->addWidget(scriptTable);
        if(auto f=effective.field("SCRIPTS_IDX"))scriptField=*f;else {scriptField.name="SCRIPTS_IDX";scriptField.kind="SCRIPT_ARRAY";scriptField.present=true;}
        for(int r=0;r<scriptTable->rowCount();r++){unsigned id=0,flags=0,counters=0;for(const auto& e:scriptField.elements)if(e.index==unsigned(r)){flags=worlded::word(e.data,0);counters=worlded::word(e.data,4);id=worlded::word(e.data,8);}auto event=new QTableWidgetItem(worlded::attachmentNames[r]);event->setFlags(event->flags()&~Qt::ItemIsEditable);scriptTable->setItem(r,0,event);scriptTable->setItem(r,1,new QTableWidgetItem(QString::number(id)));scriptTable->setItem(r,2,new QTableWidgetItem(QString("0x%1").arg(flags,8,16,QChar('0'))));for(int c=0;c<4;c++)scriptTable->setItem(r,c+3,new QTableWidgetItem(QString::number((counters>>(8*c))&255)));}
        initialScripts=tableText(scriptTable);connect(scriptTable,&QTableWidget::cellDoubleClicked,this,[this](int r,int c){if(c==0 && openScript){bool ok;auto id=scriptTable->item(r,1)->text().toUInt(&ok);if(ok && id)openScript(id);}});
        auto defaults=new QPushButton("Load selected attachment defaults from SCR");layout->addWidget(defaults);connect(defaults,&QPushButton::clicked,this,[this]{try{int row=scriptTable->currentRow();if(row<0)throw std::runtime_error("Select an event first.");auto id=number(scriptTable->item(row,1)->text(),"Script ID");if(!id || !scriptDefaults)throw std::runtime_error("Choose an available script before loading defaults.");auto bytes=scriptDefaults(id);scriptTable->item(row,2)->setText(QString("0x%1").arg(worlded::word(bytes,0),8,16,QChar('0')));auto counters=worlded::word(bytes,4);for(int c=0;c<4;c++)scriptTable->item(row,c+3)->setText(QString::number((counters>>(8*c))&255));error->clear();}catch(const std::exception& e){error->setText(QString::fromUtf8(e.what()));}});tabs->addTab(page,"Scripts");
    }
    void effects(){
        auto page=form("Effects");auto hint=new QLabel("Additive blending removes the black background from flames and light halos. Each effect is configured independently.");hint->setWordWrap(true);page->addRow(hint);
        if((effective.number("CURRENT_AID")>>28)!=14){
            auto box=new QCheckBox("Additive blending — fire, light");box->setObjectName("BLIT_FLAGS_blend_0");bool initial=(effective.number("BLIT_FLAGS")&0x10)!=0;box->setChecked(initial);box->setToolTip("Apply additive blending to this object's graphic (BLIT_FLAGS).");page->addRow("Object graphic",box);effectBlends.push_back({"BLIT_FLAGS",0,box,initial});
        }
        auto add=[&](const std::string& field,unsigned index,unsigned aid,const QString& title){
            if((aid>>28)!=14)return;
            auto box=new QCheckBox("Additive blending — fire, light");box->setObjectName(QString::fromStdString(field)+QString("_blend_%1").arg(index));box->setChecked(aid&0x100);box->setToolTip(QString("%1 · ID 0x%2").arg(QString::fromStdString(field)).arg(aid,8,16,QChar('0')));page->addRow(title,box);effectBlends.push_back({field,index,box,bool(aid&0x100)});
        };
        add("CURRENT_AID",0,unsigned(effective.number("CURRENT_AID")),"Main effect");
        for(auto name:{"OVERLAY_FORE","OVERLAY_BACK","UNDERLAY"})if(auto field=effective.field(name))for(const auto& e:field->elements)if(e.data.size()==4)add(name,e.index,worlded::word(e.data,0),QString("%1 · %2").arg(QString(name)=="OVERLAY_FORE"?"Foreground":QString(name)=="OVERLAY_BACK"?"Background":"Under object").arg(e.index+1));
    }
    void applyEffectBlends(worlded::GameObject& o){
        for(const auto& effect:effectBlends){if(effect.box->isChecked()==effect.initial)continue;
            auto toggle=[&](unsigned aid){if((aid>>28)!=14)throw std::runtime_error("The graphic is no longer an Eye Candy effect. Reopen the object editor.");return (aid&~0x100u)|(effect.box->isChecked()?0x100u:0u);};
            if(effect.field=="BLIT_FLAGS"){
                auto flags=o.number("BLIT_FLAGS",effective.number("BLIT_FLAGS"));worlded::set_number(o,"BLIT_FLAGS",(flags&~0x10ull)|(effect.box->isChecked()?0x10ull:0));
            }else if(effect.field=="CURRENT_AID"){
                worlded::set_number(o,"CURRENT_AID",toggle(unsigned(o.number("CURRENT_AID",effective.number("CURRENT_AID")))));
                auto base=unsigned(o.number("AID",effective.number("AID",effective.number("CURRENT_AID"))));if((base>>28)==14)worlded::set_number(o,"AID",toggle(base));
            }else {
                auto existing=o.field(effect.field);if(!existing)existing=effective.field(effect.field);if(!existing)continue;auto field=*existing;
                for(auto& e:field.elements)if(e.index==effect.index && e.data.size()==4){auto aid=toggle(worlded::word(e.data,0));for(int b=0;b<4;b++)e.data[b]=aid>>(8*b);}
                put(o,std::move(field));
            }
        }
    }
    #include "actor_routes_ui.inc"
    void advanced(){auto page=form("Advanced");auto hint=new QLabel("Additional engine fields. Object references and complex arrays are read only; campaign IDs are not copied.");hint->setWordWrap(true);page->addRow(hint);
        for(const auto& field:effective.fields){if(field.name=="LOCATION" || field.name=="SCRIPTS_IDX" || field.name=="NPC_WAYPOINTS_IDX" || field.name=="NPC_WAYPOINT_CURRENT" || field.name=="NPC_STANDPOINT_DAY" || field.name=="NPC_STANDPOINT_NIGHT" || field.name=="NPC_GENERATOR_DATA" || field.name=="CRITTER_SPELL_TECH_IDX")continue;if(std::any_of(numbers.begin(),numbers.end(),[&](auto& n){return n.name==field.name;}) || std::any_of(arrays.begin(),arrays.end(),[&](auto& a){return a.field.name==field.name;}))continue;
            if(field.kind=="INT32")scalar(page,field.name.c_str(),QString::fromStdString(field.name),field.name.find("FLAGS")!=std::string::npos);
            else if(!field.elements.empty()){auto view=new QPlainTextEdit;view->setReadOnly(true);view->setMaximumHeight(120);QStringList lines;for(const auto& element:field.elements){QByteArray bytes(reinterpret_cast<const char*>(element.data.data()),int(element.data.size()));lines<<QString("[%1]  %2").arg(element.index).arg(QString::fromLatin1(bytes.toHex(' ')));}view->setPlainText(lines.join('\n'));page->addRow(QString::fromStdString(field.name),view);}else {auto view=new QLineEdit(QString::fromStdString(worlded::object_field_text(field)));view->setReadOnly(true);view->setToolTip(view->text());page->addRow(QString::fromStdString(field.name),view);}}
    }
    static std::uint32_t number(const QString& text,const std::string& name){bool ok=false;auto value=text.trimmed();auto n=value.toLongLong(&ok,value.startsWith("0x",Qt::CaseInsensitive)?16:10);if(!ok || n<INT32_MIN || n>UINT32_MAX)throw std::runtime_error(name+": enter a 32-bit integer (decimal or 0x…).");return std::uint32_t(n);}
    static void put(worlded::GameObject& o,worlded::ObjectField field){for(auto& f:o.fields)if(f.name==field.name){f=std::move(field);return;}o.fields.push_back(std::move(field));}
    worlded::GameObject values(){auto o=original;for(auto& n:numbers)if(n.edit->text()!=n.initial)worlded::set_number(o,n.name,number(n.edit->text(),n.name));
        auto old=original.number("LOCATION"),loc=std::uint64_t(x->value())|(std::uint64_t(y->value())<<32);if(old!=loc){worlded::set_number(o,"LOCATION",loc);if(o.type==16)for(auto name:{"CRITTER_TELEPORT_DEST","NPC_STANDPOINT_DAY","NPC_STANDPOINT_NIGHT"})if(o.field(name) && o.number(name)==old)worlded::set_number(o,name,loc);}
        auto aid=effective.number("CURRENT_AID");if(unsigned(facing->currentIndex())!=((aid>>11)&7)){auto rotated=(aid&~0x3800ull)|(std::uint64_t(facing->currentIndex())<<11);worlded::set_number(o,"CURRENT_AID",rotated);worlded::set_number(o,"AID",rotated);}
        for(auto& a:arrays)if(tableText(a.table)!=a.initial){auto field=a.field;field.present=true;field.elements.clear();for(int r=0;r<a.table->rowCount();r++){auto value=number(a.table->item(r,1)->text(),field.name);if(a.table->columnCount()==3){if(value>63)throw std::runtime_error("Skill points must be between 0 and 63.");unsigned previous=0;auto index=a.table->item(r,0)->data(Qt::UserRole).toUInt();for(auto& e:a.field.elements)if(e.index==index)previous=worlded::word(e.data,0);value|=(previous&~255u)|(unsigned(qobject_cast<QComboBox*>(a.table->cellWidget(r,2))->currentIndex())<<6);}worlded::Bytes bytes;for(int b=0;b<4;b++)bytes.push_back(value>>(8*b));field.elements.push_back({a.table->item(r,0)->data(Qt::UserRole).toUInt(),bytes});}std::sort(field.elements.begin(),field.elements.end(),[](auto& a,auto& b){return a.index<b.index;});put(o,std::move(field));}
        if(tableText(scriptTable)!=initialScripts || (!scriptField.present && std::any_of(scriptField.elements.begin(),scriptField.elements.end(),[](const auto& e){return e.data.size()==12 && worlded::word(e.data,8)!=0;}))){auto field=scriptField;field.present=true;
            for(int r=0;r<scriptTable->rowCount();r++){auto id=number(scriptTable->item(r,1)->text(),"Script ID"),flags=number(scriptTable->item(r,2)->text(),"Script flags");if(id>999999)throw std::runtime_error("Script ID must be between 0 and 999999.");unsigned counters=0;for(int c=0;c<4;c++){auto n=number(scriptTable->item(r,c+3)->text(),"Script counter");if(n>255)throw std::runtime_error("Script counters must be between 0 and 255.");counters|=n<<(8*c);}
                auto it=std::find_if(field.elements.begin(),field.elements.end(),[r](auto& e){return e.index==unsigned(r);});if(it==field.elements.end()){if(!id && !flags && !counters)continue;field.elements.push_back({unsigned(r),worlded::Bytes(12,0)});it=std::prev(field.elements.end());}it->data.resize(12);for(int b=0;b<4;b++){it->data[b]=flags>>(b*8);it->data[4+b]=counters>>(b*8);it->data[8+b]=id>>(b*8);}}
            std::sort(field.elements.begin(),field.elements.end(),[](auto& a,auto& b){return a.index<b.index;});put(o,std::move(field));}
        applyRoutes(o);applyGenerator(o);applyKnowledge(o);
        applyEffectBlends(o);worlded::encode_instance(o);return o;
    }
};
