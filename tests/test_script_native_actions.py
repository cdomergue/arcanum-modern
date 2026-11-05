"""Exercise production script actions; no reimplementation of their operations."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
engine = (root / 'source/src/game/script.c').read_text(encoding='utf-8')
cases = []
for first, following in [('SAT_DIALOG', 'SAT_REMOVE_THIS_SCRIPT'),
                         ('SAT_DIVIDE', 'SAT_ASSIGN_OBJ')]:
    start = engine.index('    case ' + first + ':')
    end = engine.index('    case ' + following + ':', start)
    cases.append(engine[start:end])
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
enum {SAT_DIALOG,SAT_DIVIDE,OBJ_F_SCRIPTS_IDX=17,NEXT=-1,RETURN_AND_SKIP_DEFAULT=-2};
typedef struct { unsigned flags,counters; } Header;
typedef struct { Header hdr; int num; } Script;
typedef struct { int64_t triggerer_obj,attachee_obj;int attachment_point;Script* script; } Invocation;
typedef struct { Invocation* invocation; } ScriptState;
typedef struct { int type,op_type[3],op_value[3]; } ScriptAction;
static Script attached;
static int writes,dialogs,assigned,assignments;
static unsigned wanted_flags,wanted_counters;
static bool player_is_pc_obj(int64_t obj){return obj==1;}
static int script_get_value(int type,int value,ScriptState* state){(void)type;(void)state;return value;}
static void script_set_value(int type,int dest,ScriptState* state,int value){(void)type;(void)dest;(void)state;assigned=value;assignments++;}
static void obj_arrayfield_script_get(int64_t obj,int field,int at,Script* scr){assert(obj==2&&field==17&&at==9);*scr=attached;}
static void obj_arrayfield_script_set(int64_t obj,int field,int at,Script* scr){assert(obj==2&&field==17&&at==9);attached=*scr;writes++;}
static void start_dialog(int64_t pc,int64_t npc,int script,int line,int reply){
 assert(pc==1&&npc==2&&script==1565&&line==12&&reply==50);
 /* The callback must see the correct flags already synchronized. */
 assert(attached.hdr.flags==wanted_flags&&attached.hdr.counters==wanted_counters);
 dialogs++;
}
static void (*script_start_dialog_func)(int64_t,int64_t,int,int,int)=start_dialog;
static int execute(ScriptAction* action,int line,ScriptState* state){switch(action->type){
CASES
}return 0;}
int main(int argc,char** argv){
 Script running={{0x20,0x030201},1565};Invocation invocation={1,2,9,&running};ScriptState state={&invocation};
 ScriptAction action={SAT_DIALOG,{0},{50}};
 if(argc>1&&argv[1][0]=='d'){
   action.type=SAT_DIVIDE;action.op_value[1]=17;action.op_value[2]=5;
   assert(execute(&action,12,&state)==NEXT&&assigned==3);
   action.op_value[1]=-17;assert(execute(&action,12,&state)==NEXT&&assigned==-3);
   action.op_value[2]=-5;assert(execute(&action,12,&state)==NEXT&&assigned==3);
   int before=assignments;action.op_value[2]=0;execute(&action,12,&state);assert(assignments==before);
   action.op_value[1]=INT_MIN;action.op_value[2]=-1;execute(&action,12,&state);assert(assigned==INT_MIN);
   puts("PASS: native division, signed truncation, zero divisor and overflow");return 0;
 }
 attached=(Script){{0,0},1565};wanted_flags=running.hdr.flags;wanted_counters=running.hdr.counters;
 assert(execute(&action,12,&state)==RETURN_AND_SKIP_DEFAULT&&writes==1&&dialogs==1);
 execute(&action,12,&state);assert(writes==1&&dialogs==2);
 running.hdr.counters++;wanted_counters++;execute(&action,12,&state);assert(writes==2);
 running.hdr.flags=0;wanted_flags=0;execute(&action,12,&state);assert(writes==3);
 invocation.triggerer_obj=3;execute(&action,12,&state);assert(writes==3&&dialogs==4);
 invocation.triggerer_obj=1;script_start_dialog_func=NULL;execute(&action,12,&state);assert(writes==3&&dialogs==4);
 puts("PASS: dialogue preserves flags and counters independently before callback; no-op and non-PC guards");
}
'''.replace('CASES', '\n'.join(cases))
with tempfile.TemporaryDirectory(prefix='renaissance-script-actions-') as temporary:
    source = Path(temporary) / 'test.c'
    binary = Path(temporary) / 'test'
    source.write_text(code, encoding='utf-8')
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra', '-Werror',
                    str(source), '-o', str(binary)], check=True)
    results = [subprocess.run([str(binary), mode]).returncode for mode in ('flags', 'divide')]
    assert results == [0, 0], results
