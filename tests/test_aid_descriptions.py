#!/usr/bin/env python3
"""Exercise native AID selection, localization, identification and buffer limits."""
import importlib.util
from collections import Counter
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
UI = ROOT / 'source/src/ui'
spec = importlib.util.spec_from_file_location('generate_aid', ROOT/'tools/generate-aid.py')
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)
assert (UI/'modern_aid_data.inc').read_text() == generator.generate()
data = json.loads(generator.DATA.read_text())
assert len(data['entries']) == 854 and len(data['variants']) == 201
for e in data['entries']:
    assert not re.search(r'\[(Quest|Crafting|Offering|Gift|Valuable|Junk|Food|Chop|Untraceable)', e['fr']), e
    assert not re.search(r'Bypasses|Random|Aids|Casts|every|Requires|CurrentFT', e['fr']), e
    assert not re.search(r'@[a-z]\d+@[a-zÀ-ÿ]', e['fr']), e
    def numbers(text):
        return Counter(re.findall(r'\d+', generator.TOKEN.sub('', text)))
    en, fr = numbers(e['en']), numbers(e['fr'])
    if e['id'] == 10138:
        fr['120'] -= 1  # The shared limit is repeated for clarity in French.
    if e['id'] == 30759:
        fr['2'] += 1  # "x2" is written "deux fois" in French.
    assert en == fr, ('Changed numeric effects in translation', e)

source = (UI/'modern_ui.c').read_text()
def function(name):
    start = re.search(r'^static [^\n]+\b'+name+r'\([^\n]+\)\n\{', source, re.M)
    assert start, name
    return source[start.start():source.index('\n}', start.end())+2]

code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
typedef uint32_t Uint32;
#define SDL_arraysize(a) (sizeof(a)/sizeof((a)[0]))
#define SDL_strcasecmp strcasecmp
/* Model SDL's UTF-8 iterator; the production encoding boundary is below. */
static Uint32 SDL_StepUTF8(const char** p,void* unused){
    (void)unused;const unsigned char* s=(const unsigned char*)*p;Uint32 c=*s++;
    if(c>=0xc0){int n=c<0xe0?1:c<0xf0?2:3;c&=(1<<(6-n))-1;
        while(n-- && *s)c=(c<<6)|(*s++&63);}
    *p=(const char*)s;return c;
}
enum {STAT_COUNT=28, BASIC_SKILL_COUNT=12, SKILL_COUNT=16, TECH_COUNT=8, SPELL_COUNT=80};
enum {OBJ_TYPE_WEAPON=5,OBJ_TYPE_AMMO,OBJ_TYPE_ARMOR,OBJ_TYPE_GOLD,OBJ_TYPE_FOOD,
    OBJ_TYPE_SCROLL,OBJ_TYPE_KEY,OBJ_TYPE_KEY_RING,OBJ_TYPE_WRITTEN,OBJ_TYPE_GENERIC};
enum {OBJ_F_TYPE,OBJ_F_DESCRIPTION,OBJ_F_ITEM_DESCRIPTION_EFFECTS};
#define OID_TYPE_GUID 2
#define OID_STR_LEN 40
typedef struct {int type;} ObjectID;
static bool enabled=true,valid=true,identified=true,french=true,missing_label;
static int fields[3]={OBJ_TYPE_WEAPON,6029,6029};
static const char* module="Arcanum";
static char object_guid[OID_STR_LEN];
static ObjectID obj_get_id(int64_t obj){(void)obj;return (ObjectID){*object_guid?OID_TYPE_GUID:0};}
static void objid_id_to_str(char* dst,ObjectID oid){assert(oid.type==2);strcpy(dst,object_guid);}
static int obj_field_int32_get(int64_t o,int f){assert(o);return fields[f];}
static bool obj_handle_is_valid(int64_t o){assert(o);return valid;}
static bool item_is_identified(int64_t o){assert(o);return identified;}
static bool modern_ui_enabled(void){return enabled;}
static const char* gamelib_current_module_name_get(void){return module;}
static const char* tr(const char* fr,const char* en){return french?fr:en;}
static const char* label(int n){assert(n>=0);return missing_label?NULL:"Libell\xe9";}
#define stat_name label
#define basic_skill_name label
#define tech_skill_name label
#define tech_discipline_name_get label
#define spell_name label
#define description_get label
#define modern_intgame_text label
'''
code += (UI/'modern_encoding.h').read_text().replace('#include <SDL3/SDL.h>', '')
code += function('from_game') + '\n' + function('game_text')
code += '\n#include "modern_aid.inc"\n'
code += r'''
int main(void){
    char out[2000],utf8[8192],roundtrip[8192];
    assert(modern_ui_item_description(1,out,sizeof(out)));
    assert(strstr(out,"Libell\xe9") && strstr(out,"3/3"));
    assert(!strchr(out,'@') && !strstr(out,"MSR"));
    for(size_t i=0;i<SDL_arraysize(modern_aid_entries);i++){
        const ModernAidEntry* e=&modern_aid_entries[i];
        assert(aid_entry(e->id)==e);
        for(int lang=0;lang<2;lang++){
            french=lang;
            assert(aid_expand(tr(e->fr,e->en),utf8,sizeof(utf8)));
            assert(!strchr(utf8,'@'));
            to_game(utf8,out,sizeof(out));from_game(out,roundtrip,sizeof(roundtrip));
            assert(!strcmp(utf8,roundtrip)); /* accents, apostrophes, multiplication */
        }
    }
    french=true;
    /* All unique fixes resolve by persistent identity, independent of location
     * or inherited effect field. Also verify the guard against reused IDs. */
    for(size_t i=0;i<SDL_arraysize(modern_aid_variants);i++){
        const ModernAidVariant* v=&modern_aid_variants[i];
        strcpy(object_guid,v->guid);fields[1]=v->description;fields[2]=6029;
        assert(aid_item_entry(1)==aid_entry(v->effect));
        assert(modern_ui_item_description(1,out,sizeof(out)));
        assert(fields[1]==v->description && fields[2]==6029);
    }
    strcpy(object_guid,modern_aid_variants[0].guid);
    fields[1]=19999;fields[2]=0;assert(!aid_item_entry(1));object_guid[0]=0;
    fields[1]=30022;fields[2]=6034;
    assert(aid_item_entry(1)->id==30022); /* unique Sword of Baltar */
    fields[1]=39999;assert(!aid_item_entry(1)); /* unknown unique: retain game text */
    fields[1]=6029;fields[2]=40011;assert(aid_item_entry(1)->id==40011); /* installed AID */
    fields[2]=0;assert(aid_item_entry(1)->id==6029); /* originally empty effect field */
    assert(!aid_entry(-1) && !aid_entry(0) && !aid_entry(999999));
    strcpy(out,"original");identified=false;
    assert(!modern_ui_item_description(1,out,sizeof(out)) && !strcmp(out,"original"));
    identified=true;enabled=false;assert(!modern_ui_item_description(1,out,sizeof(out)));
    enabled=true;valid=false;assert(!modern_ui_item_description(1,out,sizeof(out)));
    valid=true;assert(!modern_ui_item_description(0,out,sizeof(out)));
    module="Vormantown";assert(!modern_ui_item_description(1,out,sizeof(out)));
    module=NULL;assert(!modern_ui_item_description(1,out,sizeof(out)));
    module="arcanum";assert(modern_ui_item_description(1,out,sizeof(out)));
    for(int t=0;t<=16;t++){
        fields[0]=t;bool supported=t>=5 && t<=14 && t!=8 && t!=11 && t!=12;
        assert(modern_ui_item_description(1,out,sizeof(out))==supported);
    }
    fields[0]=OBJ_TYPE_WEAPON;
    strcpy(out,"original");assert(!modern_ui_item_description(1,out,8));assert(!strcmp(out,"original"));
    assert(!modern_ui_item_description(1,out,0));
    missing_label=true;assert(!modern_ui_item_description(1,out,sizeof(out)));assert(!strcmp(out,"original"));
    assert(!aid_expand("@z1@",utf8,sizeof(utf8)));
    assert(!aid_expand("@",utf8,sizeof(utf8)));
    assert(!aid_expand("@s",utf8,sizeof(utf8)));
    assert(!aid_expand("@s999@",utf8,sizeof(utf8)));
    assert(!aid_expand("éé",utf8,3));
    puts("PASS: 854 bilingual descriptions, 201 unique variants, MES labels, CP1252 round trips, identification, module isolation and bounds");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-aid-test-') as directory:
    p = Path(directory)
    (p/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra', '-Werror',
                    '-I'+str(UI), str(p/'test.c'), '-o', str(p/'test')], check=True)
    subprocess.run([str(p/'test')], check=True)
