#pragma once
#include "script_inspector.hpp"
#include <QtWidgets>
namespace worlded {
inline QString script_source(const Bytes& data) {
    auto info=inspect_script(data);QString result;
    auto op=[&](std::size_t p,bool cond){unsigned id=word(data,p);auto count=cond?std::size(conditionNames):std::size(actionNames);
        auto names=cond?conditionNames:actionNames;auto masks=cond?conditionOperands:actionOperands;
        QString name=id<count?names[id]:QString("OPCODE_%1").arg(id);unsigned mask=id<count?masks[id]:255;QStringList args;
        for(unsigned i=0;i<8;i++)if(mask&(1u<<i))args<<QString("%1:%2:%3").arg(i).arg(data[p+4+i]).arg(std::int32_t(word(data,p+12+4*i)));
        return name+"("+args.join(", ")+")";};
    for(unsigned i=0;i<info.lines;i++){auto p=64+i*132;result+=QString("%1 IF %2 THEN %3 ELSE %4\n").arg(i).arg(op(p,true),op(p+44,false),op(p+88,false));}return result;
}
inline Bytes compile_script(const Bytes& original,const QString& source) {
    auto info=inspect_script(original);Bytes result(original.begin(),original.begin()+64);std::set<unsigned> used;
    auto put=[](Bytes& b,std::size_t p,std::uint32_t n){for(unsigned i=0;i<4;i++)b[p+i]=std::uint8_t(n>>(8*i));};
    QRegularExpression lineRx(R"(^\s*(NEW|[0-9]+)\s+IF\s+(\w+\([^)]*\))\s+THEN\s+(\w+\([^)]*\))\s+ELSE\s+(\w+\([^)]*\))\s*$)");
    QRegularExpression opRx(R"(^(\w+)\(([^)]*)\)$)"),argRx(R"(^\s*([0-7]):([0-9]+):(-?[0-9]+)\s*$)");unsigned line=0;
    auto operation=[&](const QString& str,std::size_t p,bool cond){auto m=opRx.match(str);auto name=m.captured(1);unsigned count=cond?std::size(conditionNames):std::size(actionNames);auto names=cond?conditionNames:actionNames;auto masks=cond?conditionOperands:actionOperands;
        unsigned id=count;for(unsigned i=0;i<count;i++)if(name==names[i]){id=i;break;}if(id==count){bool ok=false;if(name.startsWith("OPCODE_"))id=name.mid(7).toUInt(&ok);if(!ok || id<count)throw std::runtime_error("Unknown opcode: "+name.toStdString());}
        unsigned mask=id<count?masks[id]:255,seen=0;auto arguments=m.captured(2).trimmed();if(!arguments.isEmpty())for(auto arg:arguments.split(',')){auto a=argRx.match(arg);if(!a.hasMatch())throw std::runtime_error("Arguments must use index:type:value");unsigned i=a.captured(1).toUInt();bool typeOk=false,valueOk=false;auto type=a.captured(2).toUInt(&typeOk);auto value=a.captured(3).toLongLong(&valueOk);if(!typeOk || type>255 || !valueOk || value<INT32_MIN || value>INT32_MAX || (seen&(1u<<i)) || !(mask&(1u<<i)))throw std::runtime_error("Invalid, duplicate or unused argument");seen|=1u<<i;result[p+4+i]=type;put(result,p+12+4*i,std::uint32_t(value));}
        if(seen!=mask)throw std::runtime_error("Missing required arguments for "+name.toStdString());
        put(result,p,id);};
    for(const auto& text:source.split('\n')){if(text.trimmed().isEmpty() || text.trimmed().startsWith('#'))continue;line++;auto m=lineRx.match(text);if(!m.hasMatch())throw std::runtime_error("Line "+std::to_string(line)+": expected index IF condition THEN action ELSE action");
        auto index=m.captured(1);std::size_t p=result.size();if(index=="NEW")result.resize(p+132,0);else{bool ok=false;auto n=index.toUInt(&ok);if(!ok || n>=info.lines || !used.insert(n).second)throw std::runtime_error("Line "+std::to_string(line)+": invalid or duplicate original index");result.insert(result.end(),original.begin()+64+n*132,original.begin()+64+(n+1)*132);}
        try{operation(m.captured(2),p,true);operation(m.captured(3),p+44,false);operation(m.captured(4),p+88,false);}catch(const std::exception& e){throw std::runtime_error("Line "+std::to_string(line)+": "+e.what());}}
    put(result,52,line);put(result,56,std::max(unsigned(info.capacity),line));inspect_script(result);return result;
}
class ScriptHighlighter : public QSyntaxHighlighter {
public:
    using QSyntaxHighlighter::QSyntaxHighlighter;
    void highlightBlock(const QString& text) override {QTextCharFormat keyword;keyword.setForeground(QColor("#8ecbff"));keyword.setFontWeight(QFont::Bold);auto words=QRegularExpression(R"(\b(if|else|while|for|let|break|continue|return|default|label|goto|IF|THEN|ELSE|NEW)\b)").globalMatch(text);while(words.hasNext()){auto m=words.next();setFormat(m.capturedStart(),m.capturedLength(),keyword);}QTextCharFormat number;number.setForeground(QColor("#dfbe84"));auto values=QRegularExpression(R"(\b(0x[0-9a-fA-F]+|[0-9]+)\b)").globalMatch(text);while(values.hasNext()){auto m=values.next();setFormat(m.capturedStart(),m.capturedLength(),number);}int p=text.indexOf("//");if(p>=0){QTextCharFormat comment;comment.setForeground(QColor("#86a087"));setFormat(p,text.size()-p,comment);}}
};
class ScriptEditor : public QPlainTextEdit {
    QCompleter* completer;
public:
    explicit ScriptEditor(QWidget* parent=nullptr):QPlainTextEdit(parent){QStringList words;for(auto s:conditionNames)words<<s;for(auto s:actionNames)words<<s;words<<"IF"<<"THEN"<<"ELSE"<<"NEW";completer=new QCompleter(words,this);completer->setWidget(this);completer->setCaseSensitivity(Qt::CaseInsensitive);completer->setCompletionMode(QCompleter::PopupCompletion);
        connect(completer,QOverload<const QString&>::of(&QCompleter::activated),this,[this](const QString& value){auto c=textCursor();c.select(QTextCursor::WordUnderCursor);c.insertText(value);setTextCursor(c);});setLineWrapMode(NoWrap);setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));new ScriptHighlighter(document());}
    void modernCompletion(){QStringList words={"if","else","while","for","let","break","continue","return","default","label","goto","true","false","player","self","attachee","triggerer","extra","current","global","flag","counter","raw"};for(auto s:conditionNames)words<<QString::fromLatin1(s).mid(4).toLower()<<s;for(auto s:actionNames)words<<QString::fromLatin1(s).mid(4).toLower()<<s;completer->setModel(new QStringListModel(words,completer));}
protected:
    void keyPressEvent(QKeyEvent* e) override {if(completer->popup()->isVisible() && (e->key()==Qt::Key_Return || e->key()==Qt::Key_Enter || e->key()==Qt::Key_Escape || e->key()==Qt::Key_Tab)){e->ignore();return;}bool manual=e->modifiers()==Qt::ControlModifier && e->key()==Qt::Key_Space;if(!manual)QPlainTextEdit::keyPressEvent(e);auto c=textCursor();c.select(QTextCursor::WordUnderCursor);auto prefix=c.selectedText();if(!manual && (e->text().isEmpty() || prefix.size()<2)){completer->popup()->hide();return;}completer->setCompletionPrefix(prefix);auto r=cursorRect();r.setWidth(380);completer->complete(r);}
};
class ScriptDialog : public QDialog {
public:
    QPlainTextEdit* editor=nullptr;
    std::function<bool()> canClose;
    using QDialog::QDialog;
    void reject() override {
        if(canClose){if(canClose())QDialog::reject();return;}
        if(editor && editor->document()->isModified() && QMessageBox::question(this,"Unsaved script","Discard unsaved changes?")!=QMessageBox::Yes)return;
        QDialog::reject();
    }
};
}
