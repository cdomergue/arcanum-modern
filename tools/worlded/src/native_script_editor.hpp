#pragma once
#include "script_document.hpp"

namespace worlded {
class NativeScriptEditor : public QWidget {
    Bytes bytes_;
    QUndoStack history;
    QTableWidget* table=new QTableWidget;
    QLabel* status=new QLabel;
    bool refreshing=false;
    struct Change : QUndoCommand {
        NativeScriptEditor* editor;Bytes before,after;
        Change(NativeScriptEditor* e,Bytes a,QString title):QUndoCommand(title),editor(e),before(e->bytes_),after(std::move(a)){}
        void undo()override{editor->restore(before);}
        void redo()override{editor->restore(after);}
    };
    void restore(const Bytes& bytes){bytes_=bytes;refresh();if(changed)changed();}
    QString operationText(std::size_t p,bool condition)const {
        auto id=word(bytes_,p);auto text=operationLabel?operationLabel(id,condition):QString{};
        auto count=condition?std::size(conditionNames):std::size(actionNames);unsigned mask=id<count?(condition?conditionOperands[id]:actionOperands[id]):255;
        QStringList args;for(unsigned i=0;i<8;++i)if(mask&(1<<i)){
            auto type=bytes_[p+4+i];auto raw=word(bytes_,p+12+i*4);auto value=std::int32_t(raw);bool object=id<count && ((condition?conditionObjectOperands[id]:actionObjectOperands[id])&(1u<<i));QString arg;
            if(object){const QStringList focuses={"triggerer","self","every follower","any follower","everyone in party","anyone in party","everyone in team","anyone in team","everyone nearby","anyone nearby","current object","local object","extra object","everyone in group","anyone in group","every scenery nearby","any scenery nearby","every container nearby","any container nearby","every portal nearby","any portal nearby","player","every item nearby","any item nearby"};arg=type<focuses.size()?focuses[type]:QString("focus(%1)").arg(type);if(type==11)arg+=QString("[%1]").arg(value);}
            else{const QStringList types={"counter","global","local","number","flag","pc_var","pc_flag"};if(type==3)arg=QString::number(value);else if(type==5 || type==6)arg=QString("%1(focus %2, %3)").arg(types[type]).arg(raw>>16).arg(raw&65535);else arg=QString("%1[%2]").arg(type<types.size()?types[type]:QString("type%1").arg(type)).arg(value);}
            args<<arg;
        }
        if(text.isEmpty()){text=script_action_name(id,condition);if(id<count)text=text.mid(4).toLower().replace('_',' ');return text+(args.isEmpty()?"":" ("+args.join(", ")+")");}
        QRegularExpression placeholder("\\((obj|num)\\)");for(const auto& arg:args){auto match=placeholder.match(text);if(!match.hasMatch())break;text.replace(match.capturedStart(),match.capturedLength(),arg);}return text;
    }
    void run(const std::function<void()>& action){try{action();}catch(const std::exception& e){status->setText(QString::fromUtf8(e.what()));}}
public:
    std::function<void()> changed;
    std::function<QString(unsigned,bool)> operationLabel;
    explicit NativeScriptEditor(QWidget* parent=nullptr):QWidget(parent){
        auto layout=new QVBoxLayout(this);auto bar=new QHBoxLayout;layout->addLayout(bar);
        auto button=[&](const QString& name,const std::function<void()>& action){auto b=new QPushButton(name);bar->addWidget(b);connect(b,&QPushButton::clicked,this,[this,action]{run(action);});return b;};
        button("Add action",[this]{insert(false);});button("Add condition",[this]{insert(true);});button("Duplicate",[this]{duplicate();});button("Up",[this]{move(-1);});button("Down",[this]{move(1);});button("Delete",[this]{remove();});
        auto undo=new QToolButton;undo->setDefaultAction(history.createUndoAction(this,"Undo"));bar->addWidget(undo);auto redo=new QToolButton;redo->setDefaultAction(history.createRedoAction(this,"Redo"));bar->addWidget(redo);
        table->setColumnCount(4);table->setHorizontalHeaderLabels({"Line","Condition","Then","Else"});table->setSelectionBehavior(QAbstractItemView::SelectRows);table->setSelectionMode(QAbstractItemView::SingleSelection);table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);for(int i=1;i<4;++i)table->horizontalHeader()->setSectionResizeMode(i,QHeaderView::Stretch);layout->addWidget(table,1);
        status->setWordWrap(true);layout->addWidget(status);connect(table,&QTableWidget::cellDoubleClicked,this,[this](int row,int column){if(column>0)run([&]{editOperation(row,column-1);});});
        table->setToolTip("Double-click a condition or action to edit it. Choose the operand type and value in the operation dialog.");
    }
    void load(const Bytes& bytes){inspect_script(bytes);bytes_=bytes;history.clear();refresh();}
    const Bytes& value()const{return bytes_;}
    QUndoStack* undoStack(){return &history;}
    int selected()const{return table->currentRow();}
    void select(int row){if(row>=0 && row<table->rowCount())table->selectRow(row);}
    void replace(Bytes bytes,const QString& name){inspect_script(bytes);if(bytes!=bytes_)history.push(new Change(this,std::move(bytes),name));}
    void refresh(){int current=selected();refreshing=true;auto count=inspect_script(bytes_).lines;table->setRowCount(count);for(unsigned i=0;i<count;++i){table->setItem(i,0,new QTableWidgetItem(QString::number(i)));for(int part=0;part<3;++part)table->setItem(i,part+1,new QTableWidgetItem(operationText(64+i*132+part*44,part==0)));}refreshing=false;select(std::min(current,int(count)-1));status->clear();}
    std::vector<int> order()const{std::vector<int> rows;for(unsigned i=0;i<inspect_script(bytes_).lines;++i)rows.push_back(i);return rows;}
    void insert(bool condition){auto rows=order();int at=selected()<0?int(rows.size()):selected()+1;rows.insert(rows.begin()+at,-1);auto next=script_reorder(bytes_,rows);if(condition){script_put(next,64+at*132,4);next[64+at*132+4]=3;next[64+at*132+5]=3;}replace(next,"Insert instruction");select(at);status->setText("Instruction numbers changed. Review numeric dialogue targets and calls from other scripts.");}
    void duplicate(){int at=selected();if(at<0)return;auto rows=order();rows.insert(rows.begin()+at+1,-1);auto next=script_reorder(bytes_,rows);std::copy_n(next.begin()+64+at*132,132,next.begin()+64+(at+1)*132);replace(next,"Duplicate instruction");select(at+1);status->setText("Review numeric dialogue targets and external calls after inserting a line.");}
    void move(int delta){int at=selected(),to=at+delta;auto rows=order();if(at<0 || to<0 || to>=int(rows.size()))return;std::swap(rows[at],rows[to]);replace(script_reorder(bytes_,rows),"Move instruction");select(to);status->setText("Constant local jumps were updated. Review computed jumps, dialogue targets and external calls.");}
    void remove(){int at=selected();if(at<0)return;auto rows=order();rows.erase(rows.begin()+at);replace(script_reorder(bytes_,rows),"Delete instruction");select(std::min(at,int(rows.size())-1));status->setText("Review numeric dialogue targets and external calls after deleting a line.");}
    void editOperation(int row,int part){
        auto p=std::size_t(64+132*row+44*part);bool condition=part==0;auto oldId=word(bytes_,p);unsigned count=condition?std::size(conditionNames):std::size(actionNames);
        QDialog dialog(this);dialog.setWindowTitle(condition?"Condition":part==1?"Then action":"Else action");dialog.resize(750,520);auto layout=new QVBoxLayout(&dialog);auto search=new QLineEdit;search->setPlaceholderText("Filter actions or conditions");layout->addWidget(search);auto names=new QComboBox;layout->addWidget(names);
        auto fill=[&]{QSignalBlocker block(names);auto keep=names->currentData().isValid()?names->currentData().toUInt():oldId;names->clear();for(unsigned id=0;id<count;++id){auto label=operationLabel?operationLabel(id,condition):QString{};auto native=script_action_name(id,condition);auto text=label.isEmpty()?native:label+"  — "+native;if(text.contains(search->text(),Qt::CaseInsensitive))names->addItem(text,id);}if(oldId>=count)names->addItem(script_action_name(oldId,condition),oldId);int index=names->findData(keep);if(index>=0)names->setCurrentIndex(index);};fill();
        auto form=new QFormLayout;layout->addLayout(form);QComboBox* types[8];QComboBox* focuses[8];bool objectOperand[8]={};QSpinBox* values[8];QLabel* labels[8];
        const QStringList numeric={"Counter","Global variable","Local variable","Number","Global flag","PC variable","PC flag"};
        const QStringList objects={"Triggerer","Attachee","Every follower","Any follower","Everyone in party","Anyone in party","Everyone in team","Anyone in team","Everyone in vicinity","Anyone in vicinity","Current loop object","Local object","Extra object","Everyone in group","Anyone in group","Every scenery nearby","Any scenery nearby","Every container nearby","Any container nearby","Every portal nearby","Any portal nearby","Player","Every item nearby","Any item nearby"};
        for(int i=0;i<8;++i){labels[i]=new QLabel(QString("Operand %1").arg(i));types[i]=new QComboBox;focuses[i]=new QComboBox;for(int f=0;f<objects.size();++f)focuses[i]->addItem(objects[f],f);focuses[i]->setToolTip("PC focus for this per-character variable or flag");focuses[i]->hide();values[i]=new QSpinBox;values[i]->setRange(INT32_MIN,INT32_MAX);auto rowLayout=new QHBoxLayout;rowLayout->addWidget(types[i]);rowLayout->addWidget(focuses[i]);rowLayout->addWidget(values[i]);form->addRow(labels[i],rowLayout);}
        auto populate=[&]{if(names->currentIndex()<0)return;auto id=names->currentData().toUInt();unsigned mask=id<count?(condition?conditionOperands[id]:actionOperands[id]):255;auto label=operationLabel?operationLabel(id,condition):QString{};auto placeholders=QRegularExpression("\\((obj|num)\\)").globalMatch(label);QStringList kinds;while(placeholders.hasNext())kinds<<placeholders.next().captured(1);int ordinal=0;
            for(int i=0;i<8;++i){bool active=mask&(1<<i);labels[i]->setVisible(active);types[i]->setVisible(active);values[i]->setVisible(active);focuses[i]->setVisible(false);if(!active)continue;bool object=id<count?((condition?conditionObjectOperands[id]:actionObjectOperands[id])&(1u<<i))!=0:(ordinal<kinds.size() && kinds[ordinal]=="obj");objectOperand[i]=object;++ordinal;labels[i]->setText(QString("%1 %2").arg(object?"Object":"Value").arg(ordinal));QSignalBlocker blocker(types[i]);types[i]->clear();auto options=object?objects:numeric;for(int t=0;t<options.size();++t)types[i]->addItem(options[t],t);
                int current=id==oldId?bytes_[p+4+i]:(object?1:3);int index=types[i]->findData(current);if(index<0){types[i]->addItem(QString("Raw type %1 (preserved)").arg(current),current);index=types[i]->count()-1;}types[i]->setCurrentIndex(index);auto encoded=id==oldId?word(bytes_,p+12+i*4):0;bool pc=!object && (current==5 || current==6);int focus=int(encoded>>16);if(focuses[i]->findData(focus)<0)focuses[i]->addItem(QString("Raw focus %1").arg(focus),focus);focuses[i]->setCurrentIndex(focuses[i]->findData(focus));focuses[i]->setVisible(pc);values[i]->setValue(pc?int(encoded&0xffff):std::int32_t(encoded));values[i]->setToolTip(object?"Local-object index; other focus types usually ignore this value":"Constant value or variable/flag/counter index. PC operands use the native encoded PC focus and index.");}}
        ;populate();for(int i=0;i<8;++i)connect(types[i],&QComboBox::currentIndexChanged,&dialog,[&,i]{int t=types[i]->currentData().toInt();focuses[i]->setVisible(!objectOperand[i] && (t==5 || t==6));});connect(names,&QComboBox::currentIndexChanged,&dialog,[&]{populate();});connect(search,&QLineEdit::textChanged,&dialog,[&]{fill();populate();});auto error=new QLabel;error->setWordWrap(true);layout->addWidget(error);auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{if(names->currentIndex()<0){error->setText("Choose an operation.");return;}auto id=names->currentData().toUInt();auto next=bytes_;script_put(next,p,id);unsigned mask=id<count?(condition?conditionOperands[id]:actionOperands[id]):255;for(int i=0;i<8;++i)if(mask&(1<<i)){unsigned type=types[i]->currentData().toUInt();int value=values[i]->value();std::uint32_t encoded=std::uint32_t(value);if(!objectOperand[i] && (type==5 || type==6)){if(value<0 || value>65535){error->setText("PC variable/flag indices must be 0–65535.");return;}encoded=(focuses[i]->currentData().toUInt()<<16)|unsigned(value);}if(type!=bytes_[p+4+i] || encoded!=word(bytes_,p+12+i*4) || id!=oldId){int limit=objectOperand[i]?(type==11?10:-1):type==0?4:type==1?2000:type==2?10:type==4?3200:-1;if(limit>=0 && (value<0 || value>=limit)){error->setText(QString("Operand %1 requires an index from 0 to %2.").arg(i).arg(limit-1));return;}}next[p+4+i]=std::uint8_t(type);script_put(next,p+12+i*4,encoded);}replace(next,"Edit operation");dialog.accept();});dialog.exec();
    }
};
}
