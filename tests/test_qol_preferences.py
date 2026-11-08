#!/usr/bin/env python3
"""Exercise persisted bindings and character-separated notes using production C."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[1]/'source/src/ui'
keys=(root/'modern_keys.inc').read_text()
notes=(root/'modern_notebook.inc').read_text()
def function(source,name):
    start=source.index('static ',source.rfind('\n',0,source.index(name+'('))+1)
    end=source.index('\n}',source.index('{',start))+2
    return source[start:end]
prelude=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#define OID_STR_LEN 100
enum {SDL_BUTTON_MIDDLE=2,SDL_BUTTON_X1=4,SDL_BUTTON_X2=5};
typedef uint64_t Uint64;
typedef struct ModernMap { int mode,map,townmap; } ModernMap;
static Uint64 SDL_GetTicks(void){return 0;}
#define SDL_isalnum(c) isalnum((unsigned char)(c))
static void SDL_strlcpy(char* dst,const char* src,size_t size){snprintf(dst,size,"%s",src);}
static const char* tr(const char* fr,const char* en){(void)en;return fr;}
static const char* key_label(int i){(void)i;return "Other action";}
static bool SDL_RenamePath(const char* a,const char* b){return rename(a,b)==0;}
static bool dirty;
static int64_t pc=1;
static int64_t player_get_local_pc_obj(void){return pc;}
static bool obj_handle_is_valid(int64_t o){return o>0;}
static int64_t obj_get_id(int64_t o){return o;}
static void objid_id_to_str(char* s,int64_t id){sprintf(s,"PC:%ld",(long)id);}
'''
prelude+='enum {'+','.join('SDL_SCANCODE_'+chr(65+i)+'='+str(i+4) for i in range(26))+',SDL_SCANCODE_SPACE=44,SDL_SCANCODE_RIGHT=79,SDL_SCANCODE_LEFT=80,SDL_SCANCODE_DOWN=81,SDL_SCANCODE_UP=82};\n'
prelude+=r"""
typedef int SDL_Keycode;
enum {SDL_KMOD_NONE=0,SDLK_A=97,SDLK_Z=122};
static bool azerty;
static int SDL_GetScancodeFromKey(int key,void* mods){
 (void)mods;
 if(azerty){if(key=='m')return 51;if(key=='a')key='q';else if(key=='q')key='a';else if(key=='z')key='w';else if(key=='w')key='z';}
 return key-'a'+SDL_SCANCODE_A;
}
static SDL_Keycode SDL_GetKeyFromScancode(int key,int mods,bool event){(void)mods;(void)event;return azerty&&key==51?'m':key>=4&&key<=29?key-4+'a':0;}
"""
code=prelude+keys[keys.index('enum {KB_'):keys.index('static const char* tr(')]
code+='\n'.join(function(keys,n) for n in ['key_default','key_reset_defaults','key_save','key_allowed','key_load','key_assign'])
code+=notes[notes.index('typedef struct JournalMark'):notes.index('static void notebook_action')]
code+=r'''
int main(void){
    assert(mkdir("modern-ui",0700)==0);
    key_load();assert(key_binding[KB_INVENTORY]==SDL_SCANCODE_I && key_binding[KB_SORT]==(KEY_MOUSE|SDL_BUTTON_MIDDLE));
    assert(KB_COMBAT==15 && key_binding[KB_COMBAT]==SDL_SCANCODE_R);
    assert(!key_assign(KB_INVENTORY,SDL_SCANCODE_L));
    assert(key_binding[KB_INVENTORY]==SDL_SCANCODE_I && *key_status);
    assert(!key_assign(KB_INVENTORY,40)); /* Return belongs to modal controls. */
    assert(key_assign(KB_INVENTORY,SDL_SCANCODE_B));
    assert(key_assign(KB_COMBAT,SDL_SCANCODE_E|KEY_SHIFT));
    assert(!key_assign(KB_INVENTORY,SDL_SCANCODE_E|KEY_SHIFT));
    assert(!key_assign(KB_COMBAT,SDL_SCANCODE_E|(1<<20)));
    assert(key_assign(KB_UP,0)); /* An unbound camera direction must stay unbound. */
    assert(KB_SORT==16);
    assert(key_assign(KB_SORT,SDL_SCANCODE_B|KEY_SHIFT));
    assert(!key_assign(KB_SORT,SDL_SCANCODE_L));
    memset(key_binding,0,sizeof(key_binding));key_load();assert(key_binding[KB_SORT]==(SDL_SCANCODE_B|KEY_SHIFT));
    assert(key_assign(KB_SORT,KEY_MOUSE|SDL_BUTTON_X1|KEY_SHIFT));
    key_load();assert(key_binding[KB_SORT]==(KEY_MOUSE|SDL_BUTTON_X1|KEY_SHIFT));
    assert(!key_assign(KB_COMBAT,KEY_MOUSE|SDL_BUTTON_MIDDLE));
    assert(!key_assign(KB_SORT,KEY_MOUSE|1));
    assert(key_assign(KB_SORT,0));key_load();assert(!key_binding[KB_SORT]);
    assert(key_binding[KB_INVENTORY]==SDL_SCANCODE_B && key_binding[KB_UP]==0);
    assert(key_binding[KB_END]==SDL_SCANCODE_E && key_binding[KB_COMBAT]==(SDL_SCANCODE_E|KEY_SHIFT));
    FILE* f=fopen("modern-ui/keys.ini","w");assert(f);fputs("0=13\n1=13\n",f);fclose(f);
    key_load();assert(key_binding[KB_INVENTORY]==SDL_SCANCODE_I && key_binding[KB_SORT]==(KEY_MOUSE|SDL_BUTTON_MIDDLE)); /* Reject ambiguous file. */
    f=fopen("modern-ui/keys.ini","w");assert(f);fputs("inventory_middle_sort=0\n",f);fclose(f);
    key_load();assert(key_binding[KB_SORT]==0); /* Migrate an old disabled gesture to no binding. */
    key_save();f=fopen("modern-ui/keys.ini","r");assert(f);char line[128];while(fgets(line,sizeof(line),f))assert(!strstr(line,"inventory_middle_sort"));fclose(f);
    f=fopen("modern-ui/keys.ini","w");assert(f);fprintf(f,"16=%d\ninventory_middle_sort=0\n",KEY_MOUSE|SDL_BUTTON_MIDDLE);fclose(f);
    key_load();assert(key_binding[KB_SORT]==(KEY_MOUSE|SDL_BUTTON_MIDDLE));
    /* Upgrade a complete previous default profile without changing its sort preference. */
    f=fopen("modern-ui/keys.ini","w");assert(f);
    for(int i=0;i<KB_SORT;i++)fprintf(f,"%d=%d\n",i,key_legacy_defaults[i]);
    fprintf(f,"16=%d\n",KEY_MOUSE|SDL_BUTTON_X2);fclose(f);
    key_load();assert(key_binding[KB_COMBAT]==SDL_SCANCODE_R && key_binding[KB_REST]==SDL_SCANCODE_H);
    assert(key_binding[KB_JOURNAL]==SDL_SCANCODE_L && key_binding[KB_LAST]==(SDL_SCANCODE_A|KEY_SHIFT));
    assert(key_binding[KB_SORT]==(KEY_MOUSE|SDL_BUTTON_X2));
    /* A custom sort gesture conflicting with a new default retains the old profile. */
    f=fopen("modern-ui/keys.ini","w");assert(f);
    for(int i=0;i<KB_SORT;i++)fprintf(f,"%d=%d\n",i,key_legacy_defaults[i]);
    fprintf(f,"16=%d\n",SDL_SCANCODE_H|KEY_SHIFT);fclose(f);
    key_load();assert(key_binding[KB_SORT]==(SDL_SCANCODE_H|KEY_SHIFT));
    assert(key_binding[KB_REST]==SDL_SCANCODE_R);
    /* A customized old profile retains every existing assignment, even when it owns Shift+A. */
    f=fopen("modern-ui/keys.ini","w");assert(f);
    for(int i=0;i<=KB_SORT;i++)fprintf(f,"%d=%d\n",i,i==KB_INVENTORY?(SDL_SCANCODE_A|KEY_SHIFT):key_legacy_defaults[i]);
    fclose(f);key_load();assert(key_binding[KB_INVENTORY]==(SDL_SCANCODE_A|KEY_SHIFT));
    assert(key_binding[KB_COMBAT]==(SDL_SCANCODE_SPACE|KEY_SHIFT) && key_binding[KB_LAST]==0);
    /* Logical menu letters follow AZERTY; camera directions retain physical WASD. */
    assert(remove("modern-ui/keys.ini")==0);azerty=true;key_load();
    assert(key_binding[KB_LAST]==(SDL_SCANCODE_Q|KEY_SHIFT));
    assert(key_binding[KB_UP]==SDL_SCANCODE_W && key_binding[KB_LEFT]==SDL_SCANCODE_A);
    for(int i=0;i<KB_COUNT;i++)for(int j=0;j<i;j++)assert(!key_binding[i] || key_binding[i]!=key_binding[j]);
    assert(key_binding[KB_SPELLS]==51);
    assert(key_assign(KB_SPELLS,51));key_load();assert(key_binding[KB_SPELLS]==51);
    azerty=false;
    notebook_sync();assert(tracked_quest==-1 && !map_memo_count);
    tracked_quest=1024;journal_marks[journal_mark_count++]=(JournalMark){1,1024};
    map_memos[map_memo_count++]=(MapMemo){.mode=2,.map=504,.townmap=17,.x=120,.y=-53,.text="Entrée derrière l’église"};
    assert(notebook_save());
    tracked_quest=1025;assert(notebook_save());tracked_quest=1024;assert(notebook_save());
    pc=2;notebook_sync();assert(!journal_mark_count && !map_memo_count && tracked_quest==-1);
    map_memos[map_memo_count++]=(MapMemo){.mode=0,.map=0,.x=32,.y=17,.text="Second character"};assert(notebook_save());
    pc=1;notebook_sync();assert(tracked_quest==1024 && journal_mark_index(1,1024)==0);
    assert(map_memo_count==1 && map_memos[0].map==504 && map_memos[0].townmap==17 && map_memos[0].x==120 && map_memos[0].y==-53);
    assert(!strcmp(map_memos[0].text,"Entrée derrière l’église"));
    assert(journal_mark_index(0,1024)==-1); /* IDs are scoped by chapter. */
    puts("PASS: conflicts, reserved keys, rebinding/unbinding round-trip, corrupt-file fallback and per-character UTF-8 notes");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-preferences-') as d:
    p=Path(d);(p/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Wno-unused-variable',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],cwd=p,check=True)
