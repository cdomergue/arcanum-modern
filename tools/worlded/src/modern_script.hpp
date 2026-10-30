#pragma once
#include "script_editor.hpp"
#include <memory>

namespace worlded {
inline Bytes empty_script() {return Bytes(64,0);}
struct ModernScriptResult {Bytes bytes;QVector<int> sourceLines;QMap<QString,int> labels;};
// Structured source lowers to the original 132-byte instructions. No runtime
// extension and no reserved campaign globals; temporaries use CE's ten locals.
class ModernCompiler {
    struct Token {QString text;int line,column;};
    struct Expr {QString op;Token at;std::vector<Expr> args;};
    struct Statement {QString kind,name;Token at;std::vector<Expr> expressions;std::vector<Statement> body,otherwise,init,step;};
    struct Value {int type=3;std::int32_t value=0;bool object=false,temp=false;};
    struct Op {unsigned id=0;std::vector<Value> args;};
    struct Row {Op condition,yes,no;int source=1;};
    QVector<Token> tokens;int cursor=0,depth=0;std::vector<Row> rows;
    QMap<QString,int> locals;std::set<int> registers;int nextLabel=0;
    QMap<int,int> labels;struct Fixup{int row;bool yes;int label;};std::vector<Fixup> fixups;
    QMap<QString,int> namedLabels;std::vector<std::pair<int,int>> loops;
    [[noreturn]] void fail(const Token& at,const QString& text)const {throw std::runtime_error(QString("Line %1, column %2: %3").arg(at.line).arg(at.column).arg(text).toStdString());}
    Token peek()const{return tokens[std::min(cursor,int(tokens.size()-1))];}
    bool take(const QString& text){if(peek().text==text){cursor++;return true;}return false;}
    Token get(){auto t=peek();if(t.text=="<end>")fail(t,"Unexpected end of source");cursor++;return t;}
    void need(const QString& text){if(!take(text))fail(peek(),"Expected '"+text+"'");}
    QString identifier(){auto t=get();if(!QRegularExpression("^[A-Za-z_][A-Za-z_0-9]*$").match(t.text).hasMatch())fail(t,"Expected an identifier");return t.text;}
    static int priority(const QString& op){if(op=="||")return 1;if(op=="&&")return 2;if(op=="==" || op=="!=")return 3;if(op=="<" || op==">" || op=="<=" || op==">=")return 4;if(op=="+" || op=="-")return 5;if(op=="*" || op=="/")return 6;return 0;}
    Expr expression(int min=1){if(++depth>100)fail(peek(),"Expression nesting is too deep");auto at=get();Expr left{at.text,at,{}};
        if(at.text=="(" ){left=expression();need(")");}
        else if(at.text=="!" || at.text=="-")left={"unary"+at.text,at,{expression(7)}};
        else if(QRegularExpression("^[A-Za-z_][A-Za-z_0-9]*$").match(at.text).hasMatch()){
            if(take("(")){left.op="call:"+at.text;if(!take(")")){do{left.args.push_back(expression());}while(take(","));need(")");}}
        }else if(!QRegularExpression("^(0[xX][0-9a-fA-F]+|[0-9]+)$").match(at.text).hasMatch())fail(at,"Expected a value or function call");
        while(priority(peek().text)>=min){auto op=get();auto right=expression(priority(op.text)+1);left={op.text,op,{left,right}};}
        depth--;return left;
    }
    Statement simple(bool semicolon=true){auto at=peek();Statement s;s.at=at;
        if(take("let")){s.kind="let";s.name=identifier();need("=");s.expressions.push_back(expression());}
        else{auto lhs=expression();auto op=peek().text;if(op=="=" || op=="+=" || op=="-=" || op=="*=" || op=="/=" || op=="++" || op=="--"){
                get();s.kind=op;s.expressions.push_back(lhs);if(op!="++" && op!="--")s.expressions.push_back(expression());
            }else{s.kind="call";s.expressions.push_back(lhs);}}
        if(semicolon)need(";");
        return s;
    }
    std::vector<Statement> block(){need("{");if(++depth>100)fail(peek(),"Block nesting is too deep");std::vector<Statement> result;while(!take("}")){if(peek().text=="<end>")fail(peek(),"Unclosed block");result.push_back(statement());}depth--;return result;}
    Statement statement(){Statement s;s.at=peek();
        if(take("if")){s.kind="if";need("(");s.expressions.push_back(expression());need(")");s.body=block();if(take("else")){if(peek().text=="if")s.otherwise.push_back(statement());else s.otherwise=block();}}
        else if(take("while")){s.kind="while";need("(");s.expressions.push_back(expression());need(")");s.body=block();}
        else if(take("for")){s.kind="for";need("(");if(!take(";")){s.init.push_back(simple());}if(!take(";")){s.expressions.push_back(expression());need(";");}if(!take(")")){s.step.push_back(simple(false));need(")");}s.body=block();}
        else if(take("break")){s.kind="break";need(";");}else if(take("continue")){s.kind="continue";need(";");}
        else if(take("return")){s.kind=take("default")?"default":"return";need(";");}
        else if(take("label")){s.kind="label";s.name=identifier();need(":");}
        else if(take("goto")){s.kind="goto";s.name=identifier();need(";");}
        else s=simple();
        return s;
    }
    int allocate(const Token& at){for(int i=0;i<10;i++)if(!registers.count(i)){registers.insert(i);return i;}fail(at,"The engine supports ten local variables, including expression temporaries. Simplify this expression or reduce active locals.");}
    void release(Value v){if(v.temp)registers.erase(v.value);}
    int label(){return nextLabel++;}void mark(int id){if(labels.contains(id))throw std::runtime_error("Duplicate compiler label");labels[id]=rows.size();}
    Op jumpOp()const{return {3,{{3,0}}};}
    void jump(int target,const Token& at){fixups.push_back({int(rows.size()),true,target});rows.push_back({{0,{}},jumpOp(),{},at.line});}
    void action(Op op,const Token& at){rows.push_back({{0,{}},std::move(op),{},at.line});}
    int native(const QString& name,bool condition,const Token& at){auto names=condition?conditionNames:actionNames;auto count=condition?std::size(conditionNames):std::size(actionNames);
        for(unsigned i=0;i<count;i++)if(name==names[i] || name==QString::fromLatin1(names[i]).mid(4).toLower())return i;
        fail(at,"Unknown "+QString(condition?"condition":"action")+": "+name);}
    std::int64_t literal(const Expr& e){if(e.op=="unary-")return -literal(e.args[0]);bool ok;auto n=e.op.toLongLong(&ok,e.op.startsWith("0x",Qt::CaseInsensitive)?16:10);if(!ok)fail(e.at,"Expected a constant integer");return n;}
    Value value(const Expr& e){
        if(e.op=="/")fail(e.at,"Division is unavailable: the original runtime implements SAT_DIVIDE incorrectly.");
        if(e.op=="unary-" && QRegularExpression("^(0[xX][0-9a-fA-F]+|[0-9]+)$").match(e.args[0].op).hasMatch()){auto n=literal(e);if(n<INT32_MIN)fail(e.at,"Integer outside signed 32-bit range");return {3,std::int32_t(n)};}
        if(e.op=="true")return {3,1};
        if(e.op=="false")return {3,0};
        static const QMap<QString,int> objects={{"triggerer",0},{"attachee",1},{"self",1},{"current",10},{"extra",12},{"player",21}};
        if(objects.contains(e.op))return {objects[e.op],0,true};
        if(locals.contains(e.op))return {2,locals[e.op]};
        if(e.op=="+" || e.op=="-" || e.op=="*" || e.op=="/" || e.op=="unary-"){
            Value a=e.op=="unary-"?Value{3,0}:value(e.args[0]),b=value(e.args[e.op=="unary-"?0:1]);if(a.object || b.object)fail(e.at,"Arithmetic requires numbers");if(e.op=="/" && b.type==3 && b.value==0)fail(e.at,"Division by zero");
            int r=allocate(e.at);action({e.op=="+"?11u:e.op=="*"?13u:e.op=="/"?14u:12u,{{2,r},a,b}},e.at);release(a);release(b);return {2,r,false,true};}
        if(e.op.startsWith("call:")){auto name=e.op.mid(5);static const QMap<QString,std::pair<int,int>> access={{"global",{1,2000}},{"flag",{4,3200}},{"counter",{0,4}},{"local",{2,10}}};
            if(access.contains(name)){if(e.args.size()!=1)fail(e.at,"Expected one index");auto n=literal(e.args[0]);auto spec=access[name];if(n<0 || n>=spec.second)fail(e.at,"Index outside the engine's range");if(name=="local")fail(e.at,"Use a named 'let' variable instead of a raw local slot");return {spec.first,std::int32_t(n)};}
            if(name=="raw"){if(e.args.size()!=2)fail(e.at,"raw(type, value) requires two constants");auto type=literal(e.args[0]),n=literal(e.args[1]);if(type<0 || type>255 || n<INT32_MIN || n>INT32_MAX)fail(e.at,"Invalid raw operand");return {int(type),std::int32_t(n)};}
            fail(e.at,"Expected a value; actions are statements and conditions belong in if/while");}
        if(QRegularExpression("^(0[xX][0-9a-fA-F]+|[0-9]+)$").match(e.op).hasMatch()){auto n=literal(e);if(n>INT32_MAX)fail(e.at,"Integer outside signed 32-bit range");return {3,std::int32_t(n)};}
        fail(e.at,"Unknown variable: "+e.op);
    }
    Op call(const Expr& e,bool cond){if(!e.op.startsWith("call:"))fail(e.at,"Expected a function call");int id=native(e.op.mid(5),cond,e.at);auto mask=cond?conditionOperands[id]:actionOperands[id];unsigned count=0;for(unsigned i=0;i<8;i++)if(mask&(1u<<i))count++;
        if(e.args.size()!=count)fail(e.at,QString("Expected %1 arguments for %2").arg(count).arg(e.op.mid(5)));
        Op op{unsigned(id),{}};for(const auto& arg:e.args)op.args.push_back(value(arg));return op;}
    void branch(const Expr& e,int yes,int no){
        if(e.op=="unary!"){branch(e.args[0],no,yes);return;}
        if(e.op=="&&" || e.op=="||"){int mid=label();branch(e.args[0],e.op=="&&"?mid:yes,e.op=="&&"?no:mid);mark(mid);branch(e.args[1],yes,no);return;}
        if(e.op=="true" || e.op=="false"){jump(e.op=="true"?yes:no,e.at);return;}
        Op cond;
        if(e.op=="==" || e.op=="!=" || e.op=="<=" || e.op==">=" || e.op=="<" || e.op==">"){
            auto a=value(e.args[0]),b=value(e.args[1]);if(a.object || b.object)fail(e.at,"Numeric comparison requires numbers");cond.id=(e.op=="==" || e.op=="!=")?4:5;
            if(e.op==">=" || e.op=="<")std::swap(a,b);
            if(e.op=="!=" || e.op=="<" || e.op==">")std::swap(yes,no);
            cond.args={a,b};
        }else if(e.op.startsWith("call:") && e.op!="call:flag" && e.op!="call:global" && e.op!="call:counter")cond=call(e,true);
        else {cond={4,{value(e),{3,0}}};std::swap(yes,no);}
        int row=rows.size();fixups.push_back({row,true,yes});fixups.push_back({row,false,no});rows.push_back({cond,jumpOp(),jumpOp(),e.at.line});for(auto v:cond.args)release(v);
    }
    void statements(const std::vector<Statement>& list,bool scoped=true){auto previous=locals;for(const auto& s:list)emitStatement(s);if(scoped){for(auto it=locals.begin();it!=locals.end();++it)if(!previous.contains(it.key()))registers.erase(it.value());locals=previous;}}
    void emitStatement(const Statement& s){if(s.kind=="/=")fail(s.at,"Division is unavailable in the original runtime.");if(rows.size()>20000)fail(s.at,"Script is too large");
        if(s.kind=="let"){if(locals.contains(s.name))fail(s.at,"Variable already declared: "+s.name);static const QStringList reserved={"player","self","attachee","triggerer","extra","current","true","false"};if(reserved.contains(s.name))fail(s.at,"Reserved name");auto v=value(s.expressions[0]);if(v.object)fail(s.at,"let stores numbers; use object names directly");int r=allocate(s.at);locals[s.name]=r;action({10,{{2,r},v}},s.at);release(v);}
        else if(s.kind=="if"){int yes=label(),no=label(),end=label();branch(s.expressions[0],yes,no);mark(yes);statements(s.body);jump(end,s.at);mark(no);statements(s.otherwise);mark(end);}
        else if(s.kind=="while" || s.kind=="for"){auto previous=locals;statements(s.init,false);int test=label(),body=label(),step=label(),end=label();mark(test);if(s.expressions.empty())jump(body,s.at);else branch(s.expressions[0],body,end);mark(body);loops.push_back({end,step});statements(s.body);loops.pop_back();mark(step);statements(s.step,false);jump(test,s.at);mark(end);for(auto it=locals.begin();it!=locals.end();++it)if(!previous.contains(it.key()))registers.erase(it.value());locals=previous;}
        else if(s.kind=="break" || s.kind=="continue"){if(loops.empty())fail(s.at,"This statement needs a loop");jump(s.kind=="break"?loops.back().first:loops.back().second,s.at);}
        else if(s.kind=="return" || s.kind=="default")action({s.kind=="default"?2u:1u,{}},s.at);
        else if(s.kind=="goto" || s.kind=="label"){if(!namedLabels.contains(s.name))namedLabels[s.name]=label();auto target=namedLabels[s.name];if(s.kind=="goto")jump(target,s.at);else {if(labels.contains(target))fail(s.at,"Duplicate label: "+s.name);mark(target);}}
        else if(s.kind=="call"){auto op=call(s.expressions[0],false);if(op.id==14)fail(s.at,"Division is unavailable in the original runtime.");if(op.id==3 || (op.id>=18 && op.id<=20))fail(s.at,"Use structured loops or goto labels instead of native control-flow opcodes");action(op,s.at);for(auto v:op.args)release(v);}
        else {auto lhs=value(s.expressions[0]);if(lhs.object || lhs.temp || (lhs.type!=0 && lhs.type!=1 && lhs.type!=2 && lhs.type!=4))fail(s.at,"Assignment needs a variable, global(), flag() or counter()");Value rhs=(s.kind=="++" || s.kind=="--")?Value{3,1}:value(s.expressions[1]);if(rhs.object)fail(s.at,"Assignment requires a number");if(s.kind=="/=" && rhs.type==3 && rhs.value==0)fail(s.at,"Division by zero");
            if(s.kind=="=")action({10,{lhs,rhs}},s.at);else action({s.kind=="+=" || s.kind=="++"?11u:s.kind=="*="?13u:s.kind=="/="?14u:12u,{lhs,lhs,rhs}},s.at);release(rhs);}
    }
public:
    ModernScriptResult compile(const QString& source,const Bytes& header=empty_script()) {
        if(source.size()>1024*1024)throw std::runtime_error("Source exceeds 1 MiB");
        int line=1,col=1;for(int i=0;i<source.size();){QChar c=source[i];if(c.isSpace()){if(c=='\n'){line++;col=1;}else col++;i++;continue;}
            if(source.mid(i,2)=="//"){while(i<source.size() && source[i]!='\n'){i++;col++;}continue;}
            int start=i,atcol=col;if(c.isLetter() || c=='_'){while(i<source.size() && (source[i].isLetterOrNumber() || source[i]=='_'))i++;}
            else if(c.isDigit()){while(i<source.size() && source[i].isLetterOrNumber())i++;}
            else {QString two=source.mid(i,2);if(QStringList{"==","!=","<=",">=","&&","||","+=","-=","*=","/=","++","--"}.contains(two))i+=2;else if(QString("{}();,:+-*/=!<>").contains(c))i++;else fail({QString(c),line,col},"Unexpected character");}
            tokens.push_back({source.mid(start,i-start),line,atcol});col+=i-start;}
        tokens.push_back({"<end>",line,col});std::vector<Statement> tree;while(peek().text!="<end>")tree.push_back(statement());statements(tree,false);action({1,{}},tokens.back());
        for(auto f:fixups){if(!labels.contains(f.label))throw std::runtime_error("Unknown goto label");(f.yes?rows[f.row].yes:rows[f.row].no).args[0].value=labels[f.label];}
        ModernScriptResult result;result.bytes=Bytes(header.begin(),header.begin()+std::min<std::size_t>(64,header.size()));result.bytes.resize(64,0);
        auto put=[&](std::size_t p,unsigned n){for(int i=0;i<4;i++)result.bytes[p+i]=n>>(i*8);};put(52,rows.size());put(56,rows.size());put(60,0);
        for(auto& row:rows){result.sourceLines<<row.source;std::size_t base=result.bytes.size();result.bytes.resize(base+132,0);int part=0;for(auto* op:{&row.condition,&row.yes,&row.no}){auto p=base+44*part++;put(p,op->id);unsigned mask=(part==1?conditionOperands[op->id]:actionOperands[op->id]),n=0;for(int i=0;i<8;i++)if(mask&(1u<<i)){auto v=op->args.at(n++);result.bytes[p+4+i]=v.type;put(p+12+i*4,std::uint32_t(v.value));}}}
        for(auto it=namedLabels.begin();it!=namedLabels.end();++it)result.labels[it.key()]=labels[it.value()];
        inspect_script(result.bytes);return result;
    }
};
inline ModernScriptResult compile_modern_script(const QString& source,const Bytes& original=empty_script()){return ModernCompiler{}.compile(source,original);}
}
