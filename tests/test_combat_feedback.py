#!/usr/bin/env python3
"""Exercise resolved attack reporting and the bounded scrollable history."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1] / 'source/src'
def function(file, name):
    source = (root / file).read_text()
    match = re.search(r'^(?:static )?(?:void|bool) ' + name + r'\([^;]*?\)\n\{', source, re.M)
    assert match, name
    return source[match.start():source.index('\n}', match.end()) + 2] + '\n'

code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CF_HIT 1
#define CF_CRITICAL 2
#define MES_FILE_HANDLE_INVALID -1
#define TACTICAL_LOG_CAPACITY 128
#define TACTICAL_LOG_ROWS 4
typedef int mes_file_handle_t;
typedef struct {int num;const char* str;} MesFileEntry;
typedef struct {unsigned flags;int64_t attacker_obj,target_obj;} CombatContext;
static int combat_mes_file=1, events,loads,unloads;
static bool active=true;
static char output[2048],tactical_lines[TACTICAL_LOG_CAPACITY][4096];
static int tactical_line_head,tactical_line_count,tactical_log_scroll;
static bool combat_turn_based_is_active(void){return active;}
static bool mes_load(const char* path,int* file){assert(!strcmp(path,"mes\\skill_ui.mes"));*file=2;loads++;return true;}
static void mes_unload(int file){assert(file==2);unloads++;}
static void mes_get_msg(int file,MesFileEntry* e){
    if(file==2)e->str=e->num==500?"native success":"native failure";
    else e->str=e->num==11?"native dodge":e->num==12?"native critical hit":"native critical miss";
}
static const char* modern_intgame_text(int id){return id==20?"native chance":"native armor";}
static int object_get_ac(int64_t target,bool ignored){assert(target==9);(void)ignored;return 20;}
static void modern_combat_event(int64_t actor,int64_t target,const char* raw){assert(actor==7&&target==9);snprintf(output,sizeof(output),"%s",raw);events++;}
static void SDL_strlcpy(char* dst,const char* src,size_t size){snprintf(dst,size,"%s",src);}
'''
code += function('game/combat.c', 'combat_report_attack')
code += function('ui/modern_combat.inc', 'tactical_log_append_line')
code += function('ui/modern_combat.inc', 'tactical_log_append')
code += function('ui/modern_combat.inc', 'tactical_log_move')
code += r'''
int main(void){
    CombatContext attack={0,7,9};
    combat_report_attack(&attack,35,72,10,0,0,false);
    assert(strstr(output,"native failure")&&strstr(output,"35% (72 > 35)")&&strstr(output,"native armor 20 (-10)"));
    attack.flags=CF_HIT;
    combat_report_attack(&attack,75,44,0,10,87,false);
    assert(strstr(output,"native success")&&strstr(output,"75% (44 <= 75)")&&strstr(output,"87 > 10"));
    attack.flags=0;
    combat_report_attack(&attack,75,44,0,60,12,true);
    assert(strstr(output,"native dodge")&&strstr(output,"12 <= 60"));
    attack.flags=CF_HIT|CF_CRITICAL;
    combat_report_attack(&attack,75,44,0,0,0,false);
    assert(strstr(output,"native critical hit"));
    attack.flags=CF_CRITICAL;
    combat_report_attack(&attack,0,0,0,0,0,false);
    assert(!strcmp(output,"native critical miss"));
    attack.flags=0;
    combat_report_attack(&attack,-100,99,0,0,0,false);
    assert(strstr(output,"0% (99 > -100)"));
    assert(loads==unloads&&events==6);
    active=false;combat_report_attack(&attack,0,0,0,0,0,false);assert(events==6);
    for(int i=0;i<10000;i++){char line[40];snprintf(line,sizeof(line),"event %d",i);tactical_log_append(line);}
    assert(tactical_line_count==128&&tactical_line_head>=0&&tactical_line_head<128);
    assert(!strcmp(tactical_lines[tactical_line_head],"event 9872"));
    assert(!strcmp(tactical_lines[(tactical_line_head+127)%128],"event 9999"));
    tactical_log_move(1000);assert(tactical_log_scroll==124);
    tactical_log_move(-1000);assert(tactical_log_scroll==0);
    tactical_log_move(4);tactical_log_append("new");assert(tactical_log_scroll==5);
    char long_text[8192];memset(long_text,'x',sizeof(long_text));long_text[8191]=0;
    tactical_log_scroll=0;tactical_line_count=tactical_line_head=0;
    tactical_log_append(long_text);assert(tactical_line_count==69);
    size_t total=0;for(int i=0;i<69;i++){assert(strlen(tactical_lines[i])<=120);total+=strlen(tactical_lines[i]);}assert(total==8191);
    tactical_line_count=tactical_line_head=0;
    char accents[601];for(int i=0;i<300;i++){accents[2*i]=(char)0xc3;accents[2*i+1]=(char)0xa9;}accents[600]=0;
    tactical_log_append(accents);assert(tactical_line_count==3);
    assert(strlen(tactical_lines[0])==240&&strlen(tactical_lines[1])==240&&strlen(tactical_lines[2])==120);
    puts("PASS: actual attack result, chance/roll, armor, dodge, critical/fumble, bounded history and scroll");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-feedback-') as directory:
    path=Path(directory)
    (path/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra',str(path/'test.c'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)

combat=(root/'game/combat.c').read_text()
attack=combat.split('int sub_4B3170(CombatContext* combat)')[1].split('\n// 0x')[0]
assert attack.count('combat_report_attack(')==1
assert attack.index('combat_report_attack(')<attack.index('combat_process_crit_miss(')
assert 'result_roll' in attack and 'result_chance' in attack
assert 'random_between' not in function('game/combat.c','combat_report_attack')
skill=(root/'game/skill.c').read_text()
assert 'skill_invocation->result_roll = rnd_difficulty_roll;' in skill
assert 'skill_invocation->result_chance = effectiveness - difficulty;' in skill
preview=(root/'ui/intgame.c').read_text().split('bool modern_combat_preview(')[1]
assert 'modern_combat_rejected(' not in preview
print('PASS: reporting uses existing rolls, precedes fumble retargeting and never emits from hover previews')

anim=(root/'game/anim.c').read_text()
for name in ['sub_426560','anim_preview_path']:
    assert 'modern_combat_rejected' not in function('game/anim.c',name)
assert 'modern_combat_rejected' in function('game/anim.c','sub_426040')
print('PASS: path rejection comes from execution after retry, never shared path queries')
