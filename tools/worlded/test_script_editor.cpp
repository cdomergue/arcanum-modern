#include "src/script_workspace.hpp"
#include <iostream>
#include <stdexcept>
using namespace worlded;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F> void rejects(F fn,const char* message){bool failed=false;try{fn();}catch(const std::exception&){failed=true;}check(failed,message);}
// A small instruction trace for the tutorial's counter/reward examples. This
// checks emitted control flow; gameplay and event attachments still need playtests.
static bool reward_trace(const Bytes& bytes,int& counter,int& gold,int row=0){
    auto value=[&](std::size_t p,int slot){auto kind=bytes[p+4+slot];auto n=std::int32_t(word(bytes,p+12+slot*4));if(kind==3)return n;check(kind==0 && n==0,"Unexpected tutorial operand");return counter;};
    for(int steps=0;steps<1000;++steps){check(row>=0 && unsigned(row)<inspect_script(bytes).lines,"Tutorial jump out of range");auto p=64+132*row;auto condition=word(bytes,p);check(condition==0 || condition==4,"Unexpected tutorial condition");bool yes=condition==0 || value(p,0)==value(p,1);p+=yes?44:88;auto action=word(bytes,p);
        if(action==1 || action==2)return action==2;
        if(action==3){row=value(p,0);continue;}
        if(action==10){check(bytes[p+4]==0 && word(bytes,p+12)==0,"Unexpected tutorial assignment");counter=value(p,1);}
        else if(action==35){check(bytes[p+4]==21,"Tutorial reward must target player");gold+=value(p,1);}
        else check(action==0,"Unexpected tutorial action");++row;
    }throw std::runtime_error("Tutorial example looped");
}
int main(int argc,char** argv){
    QApplication app(argc,argv);
    try {
        auto bytes=compile_modern_script("let i=0; while(i<3){i++;} return default;").bytes;
        auto header=ScriptProperties::read(bytes);header.description="Native test";header.scriptFlags=0x800001ff;header.localFlags=0xa5a51234;header.counters=0xff00037f;bytes=header.apply(bytes);
        bytes[47]=0x7f;script_put(bytes,60,0x12345678);bytes[64+9]=0x81;script_put(bytes,64+40,0x87654321);
        check(ScriptProperties::read(bytes).apply(bytes)==bytes,"Header roundtrip changed padding");
        ScriptWorkspace native(bytes,false);check(native.compile().bytes==bytes,"Native workspace changed binary bytes");
        auto unknown=bytes;script_put(unknown,64+44,99999);NativeScriptEditor editor;editor.load(unknown);check(editor.value()==unknown,"Unknown opcode was lost");
        editor.select(0);editor.move(1);editor.undoStack()->undo();check(editor.value()==unknown,"Undo did not restore native bytes");
        auto jumps=compile_modern_script("goto end; do_nothing(); label end: return;").bytes;
        auto count=inspect_script(jumps).lines;std::vector<int> order;for(unsigned i=0;i<count;++i)order.push_back(i);std::swap(order[1],order[2]);auto moved=script_reorder(jumps,order);check(word(moved,64+44+12)==1,"Constant jump was not relocated");
        order.clear();order.push_back(0);rejects([&]{script_reorder(jumps,order);},"Deletion accepted a dangling jump");
        auto source=QString("dialog(1); label reward: global(1900)=7; return;");auto compiled=compile_modern_script(source);
        auto dialogue=new_dialogue();dialogue[1].fields[5]="@reward";auto resolved=resolve_dialogue(dialogue,compiled.labels);check(resolved[1].fields[5].toInt()==-compiled.labels["reward"],"Symbolic dialogue target unresolved");
        auto changed=compile_modern_script("do_nothing();"+source);check(resolve_dialogue(dialogue,changed.labels)[1].fields[5]!=resolved[1].fields[5],"Dialogue target did not follow source insertion");
        check(read_dialogue_source(dialogue_source(dialogue))==dialogue,"Dialogue source lost labels");rejects([&]{resolve_dialogue(dialogue,{});},"Missing label accepted");
        check(parse_dialogue(encode_dialogue(resolved))==resolved,"Native dialogue roundtrip failed");
        rejects([]{compile_modern_script("let n=4; n/=2; return;");},"Broken runtime division accepted");
        auto pc=compile_modern_script("pc_var(player, 8) = 3; pc_flag(self, 2) = 1; return;");check(pc.bytes[64+44+4]==5 && word(pc.bytes,64+44+12)==((21u<<16)|8u),"PC focus encoding differs");
        auto bad=header;bad.description=QString(40,'x');rejects([&]{bad.apply(bytes);},"Oversized description accepted");
        QTemporaryDir temp;auto path=temp.filePath("roundtrip.scr");script_write_file(path,bytes);check(script_read_file(path)==bytes,"Atomic file save differs");
        QTemporaryDir project;auto text=source.toUtf8();script_write_file(project.filePath("01999.wscript"),Bytes(text.begin(),text.end()));script_write_file(project.filePath("01999.wdlg"),dialogue_source(dialogue));
        script_write_file(project.filePath("01999.scr"),bytes);script_write_file(project.filePath("01998.scr"),unknown);
        auto build=compile_script_project(project.path());check(build.files.size()==3,"Mixed project lost native or source resources");
        check(build.files["scr/01998.scr"]==unknown,"Project build changed native script bytes");
        check(ScriptProperties::read(build.files["scr/01999.scr"]).localFlags==header.localFlags,"Project build lost script properties");
        check(parse_dialogue(build.files["dlg/01999.dlg"])[1].fields[5]==resolved[1].fields[5],"Project build did not resolve companion labels");
        auto output=temp.filePath("export");export_script_project(output,build);check(script_read_file(output+"/scr/01998.scr")==unknown,"Export changed native bytes");
        rejects([&]{export_script_project(output,build);},"Export overwrote a nonempty folder");
        QDir(project.path()).mkdir("duplicate");script_write_file(project.filePath("duplicate/01999.wscript"),Bytes(text.begin(),text.end()));
        rejects([&]{compile_script_project(project.path());},"Duplicate project source accepted");
        compile_modern_script("if(pc_flag(player, 2)){pc_var(self, 8) += 1;} return;");
        auto delayed=compile_modern_script("call_script_in(40000, 2, triggerer, self, 5); return;");
        check(actionOperands[54]==31 && actionObjectOperands[54]==12 && word(delayed.bytes,64+44+28)==5,"Delayed script call lost operands");
        auto exampleText=[](const QString& name){auto data=script_read_file(QString::fromUtf8(SCRIPT_EXAMPLES_DIR)+"/"+name);return QString::fromUtf8(reinterpret_cast<const char*>(data.data()),int(data.size()));};
        auto chest=compile_modern_script(exampleText("49000_reward_chest.wscript"));
        Bytes nativeRows(64+4*132,0);script_put(nativeRows,52,4);script_put(nativeRows,56,4);
        auto nativeChest=compile_script(nativeRows,exampleText("reward_chest.native.txt"));
        for(const auto& script:{chest.bytes,nativeChest}){int counter=0,gold=100;check(reward_trace(script,counter,gold) && counter==1 && gold==150,"Chest first-use example differs");check(reward_trace(script,counter,gold) && gold==150,"Chest example repeats reward");}
        auto keeper=compile_modern_script(exampleText("49001_reward_keeper.wscript"));
        check(keeper.labels["reward"]==2,"Documented dialogue entry point changed");
        auto keeperDialogue=read_dialogue_source(script_read_file(QString::fromUtf8(SCRIPT_EXAMPLES_DIR)+"/49001_reward_keeper.wdlg"));
        check(resolve_dialogue(keeperDialogue,keeper.labels)[1].fields[5]=="-2","Tutorial dialogue destination differs");
        int keeperCounter=0,keeperGold=100;check(!reward_trace(keeper.bytes,keeperCounter,keeperGold,keeper.labels["reward"]) && keeperGold==150,"NPC payment example differs");reward_trace(keeper.bytes,keeperCounter,keeperGold,keeper.labels["reward"]);check(keeperGold==150,"NPC payment repeats");
        if(argc>1 && QString::fromLocal8Bit(argv[1])!="-platform"){
            Installation game(std::filesystem::u8path(argv[1]));unsigned total=0;for(const auto& entry:game.resources())if(entry.first.size()>4 && entry.first.substr(entry.first.size()-4)==".scr"){
                auto original=game.read(entry.first);ScriptWorkspace script(original,false);check(script.compile().bytes==original,"Campaign script binary roundtrip differs");++total;
            }std::cout<<"Campaign SCR roundtrips: "<<total<<'\n';
            for(bool condition:{true,false}){
                unsigned checked=0;for(const auto& entry:read_messages(game.read(condition?"semes/condition.mes":"semes/action.mes"))){
                    auto count=condition?std::size(conditionNames):std::size(actionNames);if(entry.id>=count)continue;
                    auto mask=condition?conditionOperands[entry.id]:actionOperands[entry.id],objects=condition?conditionObjectOperands[entry.id]:actionObjectOperands[entry.id];
                    auto text=QString::fromStdString(cp1252_utf8(entry.text));auto fields=QRegularExpression("\\((obj|num)\\)").globalMatch(text);
                    for(unsigned i=0;i<8;++i)if(mask&(1u<<i)){check(fields.hasNext(),"Operand absent from original editor label");check((fields.next().captured(1)=="obj")==bool(objects&(1u<<i)),"Object/numeric selector differs from original editor");}
                    check(!fields.hasNext(),"Original editor operand missing from metadata");++checked;
                }check(checked==(condition?52u:142u),"Original opcode catalogue is incomplete");
                // SCT_TRUE is implicit in action-only rows and has no MES entry.
                check(std::size(conditionNames)==53 && conditionOperands[0]==0,"Implicit true condition missing");
            }
            std::cout<<"Original operation selectors: 52 described conditions + implicit true, 142 actions\n";
        }
        if(auto screenshot=qEnvironmentVariable("SCRIPT_EDITOR_TEST_SCREENSHOT");!screenshot.isEmpty()){
            ScriptWorkspace modern(empty_script(),true,"// Reward after a conversation\ndialog(1);\nlabel reward:\nif (global(1900) == 0) {\n    global(1900) = 1;\n}\nreturn;\n");modern.resize(1100,680);modern.show();app.processEvents();modern.compile();modern.grab().save(screenshot);
            native.resize(1100,680);native.show();app.processEvents();native.grab().save(screenshot+"-native.png");
        }
        std::cout<<"PASS: native fidelity, metadata, unknown opcodes, undo, relocation, symbolic dialogues and source validation\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
