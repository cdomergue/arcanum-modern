#pragma once
#include "inventory.hpp"
#include "scene_assets.hpp"
#include <QtWidgets>
#include <climits>

class InventoryEditor : public QDialog {
public:
    std::vector<worlded::GameObject> result,items;
    worlded::GameObject owner;
    std::vector<worlded::GameObject> original;
    SceneAssets& assets;
    QTableWidget* table=new QTableWidget;
    QLabel* error=new QLabel;
    InventoryEditor(const std::vector<worlded::GameObject>& objects,const worlded::GameObject& selected,
        const std::vector<worlded::GameObject>& prototypes,SceneAssets& resources,QWidget* parent=nullptr)
        :QDialog(parent),owner(selected),original(objects),assets(resources) {
        items=worlded::inventory_contents(objects,selected);
        setWindowTitle("Inventory — "+assets.description(assets.effective(owner)));resize(900,600);
        auto layout=new QVBoxLayout(this);auto help=new QLabel("Create the starting inventory of this container or character. Saving a manual inventory disables its random inventory generator. Items keep their own persistent IDs. The editor checks storage space and equipment slots; test race, gender, spell and character-graphic compatibility in a game copy.");help->setWordWrap(true);layout->addWidget(help);
        table->setObjectName("inventoryItems");table->setColumnCount(5);table->setHorizontalHeaderLabels({"Item","Placement","Grid cell","Quantity","Identified"});table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);table->setSelectionBehavior(QAbstractItemView::SelectRows);table->setSelectionMode(QAbstractItemView::SingleSelection);table->verticalHeader()->hide();layout->addWidget(table,1);
        auto controls=new QHBoxLayout;auto add=new QPushButton("Add item…"),remove=new QPushButton("Remove item"),arrange=new QPushButton("Arrange backpack");controls->addWidget(add);controls->addWidget(remove);controls->addWidget(arrange);controls->addStretch();layout->addLayout(controls);
        auto note=new QLabel(owner.type==2?"Backpack cells: 0–959, ten columns and 96 rows. Cell = row × 10 + column.":"Backpack cells: 0–119, ten columns and 12 rows. Cell = row × 10 + column.");note->setWordWrap(true);layout->addWidget(note);error->setWordWrap(true);error->setStyleSheet("color:#ffb080");layout->addWidget(error);
        auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(buttons);
        connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
        connect(buttons,&QDialogButtonBox::accepted,this,[this]{try{readRows();result=worlded::inventory_apply(original,owner.id,items,[this](const auto& o){return assets.effective(o);},[this](const auto& o){return dimensions(o);});accept();}catch(const std::exception& e){error->setText(QString::fromUtf8(e.what()));}});
        connect(remove,&QPushButton::clicked,this,[this]{int row=table->currentRow();if(row<0)return;readRows();items.erase(items.begin()+row);refresh();});
        connect(arrange,&QPushButton::clicked,this,[this]{try{readRows();auto next=items;arrangeItems(next);items=std::move(next);refresh();error->clear();}catch(const std::exception& e){error->setText(QString::fromUtf8(e.what()));}});
        connect(add,&QPushButton::clicked,this,[this,prototypes]{
            QDialog chooser(this);chooser.setWindowTitle("Add inventory item");chooser.resize(600,550);QVBoxLayout layout(&chooser);QLineEdit search;search.setPlaceholderText("Search item name or prototype number");QListWidget list;layout.addWidget(&search);layout.addWidget(&list);std::set<unsigned> seen;
            for(unsigned i=0;i<prototypes.size();i++){const auto& o=prototypes[i];if(!worlded::inventory_item(o) || !seen.insert(o.prototype_id).second)continue;auto item=new QListWidgetItem(assets.description(assets.effective(o))+QString(" · #%1").arg(o.prototype_id),&list);item->setData(Qt::UserRole,i);}
            connect(&search,&QLineEdit::textChanged,&chooser,[&](const QString& text){for(int i=0;i<list.count();i++)list.item(i)->setHidden(!list.item(i)->text().contains(text,Qt::CaseInsensitive));});
            QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout.addWidget(&buttons);connect(&buttons,&QDialogButtonBox::accepted,&chooser,[&]{if(list.currentItem())chooser.accept();});connect(&buttons,&QDialogButtonBox::rejected,&chooser,&QDialog::reject);connect(&list,&QListWidget::itemDoubleClicked,&chooser,[&]{chooser.accept();});
            if(chooser.exec()!=QDialog::Accepted || !list.currentItem())return;
            try{readRows();auto next=items;auto item=worlded::clone_instance(prototypes[list.currentItem()->data(Qt::UserRole).toUInt()]);worlded::set_number(item,"ITEM_INV_LOCATION",0);if(item.type==8)worlded::set_number(item,"GOLD_QUANTITY",1);if(item.type==6)worlded::set_number(item,"AMMO_QUANTITY",1);next.push_back(item);arrangeItems(next);items=std::move(next);refresh();table->setCurrentCell(table->rowCount()-1,0);error->clear();}catch(const std::exception& e){error->setText(QString::fromUtf8(e.what()));}
        });refresh();
    }
    std::pair<unsigned,unsigned> dimensions(const worlded::GameObject& resolved) {
        try{auto pixmap=assets.sprite(unsigned(resolved.number("ITEM_INV_AID"))).pixels;return {unsigned((pixmap.width()+31)/32),unsigned((pixmap.height()+31)/32)};}catch(const std::exception&){return {1,1};}
    }
    static const char* quantityField(const worlded::GameObject& item){return item.type==8?"GOLD_QUANTITY":item.type==6?"AMMO_QUANTITY":nullptr;}
    void refresh() {
        table->setRowCount(0);static const char* names[]={"Helmet","Ring 1","Ring 2","Medallion","Weapon","Shield","Armor","Gauntlets","Boots"};
        for(const auto& item:items){auto resolved=assets.effective(item);int row=table->rowCount();table->insertRow(row);auto label=new QTableWidgetItem(assets.description(resolved)+QString(" · #%1").arg(item.prototype_id));label->setFlags(label->flags()&~Qt::ItemIsEditable);label->setToolTip(QString::fromStdString(item.id));table->setItem(row,0,label);
            auto placement=new QComboBox;placement->addItem("Backpack",-1);if(owner.type!=2){int slot=worlded::inventory_equipment_slot(resolved);for(int n=1000;n<=1008;n++)if(n==slot || (slot==1001 && n==1002))placement->addItem(names[n-1000],n);}
            int location=int(item.number("ITEM_INV_LOCATION"));int index=location>=1000?placement->findData(location):0;if(index<0){placement->addItem(QString("Unsupported slot %1").arg(location),location);index=placement->count()-1;}placement->setCurrentIndex(index);table->setCellWidget(row,1,placement);
            auto cell=new QSpinBox;cell->setRange(0,owner.type==2?959:119);cell->setValue(location<1000?location:0);cell->setEnabled(placement->currentData().toInt()<0);table->setCellWidget(row,2,cell);connect(placement,&QComboBox::currentIndexChanged,this,[cell,placement]{cell->setEnabled(placement->currentData().toInt()<0);});
            auto quantity=new QSpinBox;quantity->setRange(1,INT_MAX);auto field=quantityField(item);quantity->setValue(field?int(resolved.number(field,1)):1);quantity->setEnabled(field!=nullptr);table->setCellWidget(row,3,quantity);
            auto identified=new QCheckBox;identified->setChecked(resolved.number("ITEM_FLAGS")&1);table->setCellWidget(row,4,identified);
        }for(int column=1;column<5;column++)table->resizeColumnToContents(column);
    }
    void readRows() {
        for(int row=0;row<table->rowCount();row++){auto& item=items[row];auto placement=qobject_cast<QComboBox*>(table->cellWidget(row,1));int slot=placement->currentData().toInt();if(slot<0)slot=qobject_cast<QSpinBox*>(table->cellWidget(row,2))->value();worlded::set_number(item,"ITEM_INV_LOCATION",slot);
            if(auto field=quantityField(item))worlded::set_number(item,field,qobject_cast<QSpinBox*>(table->cellWidget(row,3))->value());auto flags=assets.effective(item).number("ITEM_FLAGS");worlded::set_number(item,"ITEM_FLAGS",(flags&~1ull)|(qobject_cast<QCheckBox*>(table->cellWidget(row,4))->isChecked()?1:0));}
    }
    void arrangeItems(std::vector<worlded::GameObject>& next) {
        std::set<unsigned> occupied;unsigned capacity=owner.type==2?960:120;
        for(auto& item:next){if(item.number("ITEM_INV_LOCATION")>=1000)continue;auto size=dimensions(assets.effective(item));bool placed=false;
            for(unsigned cell=0;cell<capacity;cell++){if(cell%10+size.first>10 || cell/10+size.second>capacity/10)continue;bool free=true;for(unsigned y=0;y<size.second;y++)for(unsigned x=0;x<size.first;x++)if(occupied.count(cell+y*10+x))free=false;if(!free)continue;
                for(unsigned y=0;y<size.second;y++)for(unsigned x=0;x<size.first;x++)occupied.insert(cell+y*10+x);worlded::set_number(item,"ITEM_INV_LOCATION",cell);placed=true;break;}
            if(!placed)throw std::runtime_error("No room in this inventory. Remove an item or equip it first.");
        }
    }
};
