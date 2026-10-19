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
#include <sys/stat.h>
#define OID_STR_LEN 100
typedef uint64_t Uint64;
#define SDL_isalnum(c) isalnum((unsigned char)(c))
static void SDL_strlcpy(char* dst,const char* src,size_t size){snprintf(dst,size,"%s",src);}
static const char* tr(const char* fr,const char* en){(void)en;return fr;}
static const char* key_label(int i){(void)i;return "Other action";}
static bool dirty;
static int64_t pc=1;
static int64_t player_get_local_pc_obj(void){return pc;}
static bool obj_handle_is_valid(int64_t o){return o>0;}
static int64_t obj_get_id(int64_t o){return o;}
static void objid_id_to_str(char* s,int64_t id){sprintf(s,"PC:%ld",(long)id);}
'''
prelude+='enum {'+','.join('SDL_SCANCODE_'+chr(65+i)+'='+str(i+4) for i in range(26))+',SDL_SCANCODE_SPACE=44};\n'
code=prelude+keys[keys.index('enum {KB_'):keys.index('static const char* tr(')]
code+='\n'.join(function(keys,n) for n in ['key_save','key_allowed','key_load','key_assign'])
code+=notes[notes.index('typedef struct JournalMark'):notes.index('static void notebook_action')]
code+=r'''
int main(void){
    assert(mkdir("modern-ui",0700)==0);
    key_load();assert(key_binding[KB_INVENTORY]==SDL_SCANCODE_I);
    assert(!key_assign(KB_INVENTORY,SDL_SCANCODE_J));
    assert(key_binding[KB_INVENTORY]==SDL_SCANCODE_I && *key_status);
    assert(!key_assign(KB_INVENTORY,40)); /* Return belongs to modal controls. */
    assert(key_assign(KB_INVENTORY,SDL_SCANCODE_B));
    assert(key_assign(KB_UP,0)); /* An unbound camera direction must stay unbound. */
    memset(key_binding,0,sizeof(key_binding));key_load();
    assert(key_binding[KB_INVENTORY]==SDL_SCANCODE_B && key_binding[KB_UP]==0);
    FILE* f=fopen("modern-ui/keys.ini","w");assert(f);fputs("0=13\n1=13\n",f);fclose(f);
    key_load();assert(key_binding[KB_INVENTORY]==SDL_SCANCODE_I); /* Reject ambiguous file. */
    notebook_sync();assert(tracked_quest==-1 && !map_memo_count);
    tracked_quest=1024;journal_marks[journal_mark_count++]=(JournalMark){1,1024};
    map_memos[map_memo_count++]=(MapMemo){2,504,120,-53,"Entrée derrière l’église"};
    assert(notebook_save());
    pc=2;notebook_sync();assert(!journal_mark_count && !map_memo_count && tracked_quest==-1);
    map_memos[map_memo_count++]=(MapMemo){0,0,32,17,"Second character"};assert(notebook_save());
    pc=1;notebook_sync();assert(tracked_quest==1024 && journal_mark_index(1,1024)==0);
    assert(map_memo_count==1 && map_memos[0].map==504 && map_memos[0].x==120 && map_memos[0].y==-53);
    assert(!strcmp(map_memos[0].text,"Entrée derrière l’église"));
    assert(journal_mark_index(0,1024)==-1); /* IDs are scoped by chapter. */
    puts("PASS: conflicts, reserved keys, rebinding/unbinding round-trip, corrupt-file fallback and per-character UTF-8 notes");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-preferences-') as d:
    p=Path(d);(p/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Wno-unused-variable',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],cwd=p,check=True)
