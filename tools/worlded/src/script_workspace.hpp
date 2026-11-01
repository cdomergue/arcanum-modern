#pragma once
#include "native_script_editor.hpp"

namespace worlded {
class ScriptWorkspace : public QWidget {
    Bytes original;
    bool modern_=true;
    ScriptProperties properties;
    QLabel* feedback=new QLabel;
    QPlainTextEdit* listing=new QPlainTextEdit;
    QTabWidget* tabs=new QTabWidget;
    NativeScriptEditor* native=nullptr;
public:
    ScriptEditor* editor=new ScriptEditor;
    std::function<void()> changed;
    std::function<QString(unsigned,bool)> operationLabel;
    explicit ScriptWorkspace(const Bytes& bytes,bool modern,const QString& source={},QWidget* parent=nullptr):QWidget(parent),original(bytes),modern_(modern),properties(ScriptProperties::read(bytes)) {
        auto layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);auto toolbar=new QHBoxLayout;auto validate=new QPushButton("Compile / validate"),props=new QPushButton("Properties"),find=new QPushButton("Find…");toolbar->addWidget(validate);toolbar->addWidget(props);toolbar->addWidget(find);toolbar->addStretch();layout->addLayout(toolbar);layout->addWidget(tabs,1);
        if(modern_){editor->modernCompletion();editor->setPlainText(source);tabs->addTab(editor,"Structured source");connect(editor,&QPlainTextEdit::textChanged,this,[this]{if(changed)changed();});}
        else {native=new NativeScriptEditor;native->operationLabel=[this](unsigned id,bool condition){return operationLabel?operationLabel(id,condition):QString{};};native->load(bytes);native->changed=[this]{editor->setPlainText(script_source(native->value()));if(changed)changed();};tabs->addTab(native,"Instructions");editor->setPlainText(script_source(bytes));editor->setReadOnly(true);tabs->addTab(editor,"Native listing");}
        editor->document()->setModified(false);listing->setReadOnly(true);listing->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));tabs->addTab(listing,"Compiled instructions");feedback->setWordWrap(true);layout->addWidget(feedback);
        connect(validate,&QPushButton::clicked,this,[this]{try{auto result=compile();auto warnings=script_warnings(result.bytes);feedback->setText(warnings.isEmpty()?"Valid script. Test its behavior in a separate game copy.":warnings.join('\n'));}catch(const std::exception& e){feedback->setText(QString::fromUtf8(e.what()));}});
        connect(props,&QPushButton::clicked,this,[this]{editProperties();});connect(find,&QPushButton::clicked,this,[this]{bool ok=false;auto term=QInputDialog::getText(this,"Find","Text",QLineEdit::Normal,{},&ok);if(!ok || term.isEmpty())return;auto view=tabs->currentWidget()==listing?listing:static_cast<QPlainTextEdit*>(editor);if(view==editor)tabs->setCurrentWidget(editor);if(!view->find(term)){auto cursor=view->textCursor();cursor.movePosition(QTextCursor::Start);view->setTextCursor(cursor);view->find(term);}});
    }
    bool modern()const{return modern_;}
    NativeScriptEditor* nativeEditor(){return native;}
    void refreshLabels(){if(native)native->refresh();}
    ScriptProperties header()const{return properties;}
    void setHeader(const ScriptProperties& value){value.apply(original);properties=value;if(changed)changed();}
    ModernScriptResult compile(){ModernScriptResult result;if(modern_)result=compile_modern_script(editor->toPlainText(),original);else result.bytes=native->value();result.bytes=properties.apply(result.bytes);inspect_script(result.bytes);listing->setPlainText(script_listing(result.bytes));return result;}
    void editProperties(){
        QDialog dialog(this);dialog.setWindowTitle("Script properties");auto form=new QFormLayout(&dialog);QLineEdit description(properties.description);form->addRow("Description (39 bytes)",&description);QLineEdit flags(QString("0x%1").arg(properties.scriptFlags,8,16,QChar('0'))),local(QString("0x%1").arg(properties.localFlags,8,16,QChar('0')));form->addRow("Script behavior flags",&flags);form->addRow("Local flags (32 bits)",&local);
        const QStringList names={"Nonmagical trap","Magical trap","Auto-removing","Death speech","Surrender speech","Radius two","Radius three","Radius five","Teleport trigger"};QCheckBox* checks[9];for(int i=0;i<9;++i){checks[i]=new QCheckBox(names[i]);checks[i]->setChecked(properties.scriptFlags&(1<<i));form->addRow(checks[i]);connect(checks[i],&QCheckBox::toggled,&dialog,[&,i](bool checked){bool ok;auto value=flags.text().toUInt(&ok,0);if(!ok)return;value=checked?value|(1u<<i):value&~(1u<<i);flags.setText(QString("0x%1").arg(value,8,16,QChar('0')));});}
        connect(&flags,&QLineEdit::textChanged,&dialog,[&]{bool ok;auto value=flags.text().toUInt(&ok,0);if(ok)for(int i=0;i<9;++i){QSignalBlocker block(checks[i]);checks[i]->setChecked(value&(1<<i));}});
        auto localGrid=new QGridLayout;QCheckBox* localChecks[32];for(int i=0;i<32;++i){localChecks[i]=new QCheckBox(QString::number(i));localChecks[i]->setChecked(properties.localFlags&(1u<<i));localGrid->addWidget(localChecks[i],i/8,i%8);connect(localChecks[i],&QCheckBox::toggled,&dialog,[&,i](bool checked){bool ok;auto value=local.text().toUInt(&ok,0);if(!ok)return;value=checked?value|(1u<<i):value&~(1u<<i);local.setText(QString("0x%1").arg(value,8,16,QChar('0')));});}form->addRow(localGrid);
        connect(&local,&QLineEdit::textChanged,&dialog,[&]{bool ok;auto value=local.text().toUInt(&ok,0);if(ok)for(int i=0;i<32;++i){QSignalBlocker block(localChecks[i]);localChecks[i]->setChecked(value&(1u<<i));}});
        QSpinBox counters[4];for(int i=0;i<4;++i){counters[i].setRange(0,255);counters[i].setValue((properties.counters>>(8*i))&255);form->addRow(QString("Counter %1").arg(i),&counters[i]);}
        QLabel error;error.setWordWrap(true);form->addRow(&error);QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(&buttons);connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);connect(&buttons,&QDialogButtonBox::accepted,&dialog,[&]{try{bool a,b;ScriptProperties next{description.text(),flags.text().toUInt(&a,0),local.text().toUInt(&b,0),0};if(!a || !b)throw std::runtime_error("Flags must be unsigned 32-bit decimal or hexadecimal values");for(int i=0;i<4;++i)next.counters|=std::uint32_t(counters[i].value())<<(8*i);setHeader(next);dialog.accept();}catch(const std::exception& e){error.setText(e.what());}});dialog.exec();
    }
};
}
