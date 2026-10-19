from pathlib import Path
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'source/src/game/object.c').read_text()
header = (root / 'source/src/game/object.h').read_text()
start = source.index('bool object_locked_get(')
end = source.index('// 0x441F10', start)
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include "game/obj_flags.h"
typedef int tig_art_id_t;
typedef struct { int milliseconds; } DateTime;
typedef struct { int type; struct { int64_t object_value; } params[1]; } TimeEvent;
enum { OBJ_TYPE_PORTAL=1, OBJ_TYPE_CONTAINER=2, OBJ_F_TYPE,
    OBJ_F_PORTAL_FLAGS, OBJ_F_CONTAINER_FLAGS, OBJ_F_CURRENT_AID, TIMEEVENT_TYPE_LOCK };
static int type, hour=12, timers, closes;
static unsigned flags;
static bool lockable=true, opened=true;
static bool object_is_lockable(int64_t obj){return lockable;}
static int obj_field_int32_get(int64_t obj,int field){
    if(field==OBJ_F_TYPE)return type;
    if(field==OBJ_F_CURRENT_AID)return opened;
    assert(field==(type==OBJ_TYPE_PORTAL?OBJ_F_PORTAL_FLAGS:OBJ_F_CONTAINER_FLAGS));
    return flags;
}
static void obj_field_int32_set(int64_t obj,int field,int value){
    assert(field==(type==OBJ_TYPE_PORTAL?OBJ_F_PORTAL_FLAGS:OBJ_F_CONTAINER_FLAGS));flags=value;
}
static int datetime_current_hour(void){return hour;}
static void sub_45A950(DateTime* dt,int ms){dt->milliseconds=ms;}
static void timeevent_add_delay(TimeEvent* event,DateTime* dt){assert(dt->milliseconds==3600000);timers++;}
static bool object_jammed_set(int64_t obj,bool jammed){assert(!jammed);flags&=~OPF_JAMMED;return true;}
static bool portal_is_open(int64_t obj){return opened;}
static void portal_toggle(int64_t obj){opened=!opened;closes++;}
static int tig_art_id_frame_get(int aid){return aid;}
static void object_dec_current_aid(int64_t obj){opened=false;closes++;}
''' + re.search(r'bool object_locked_set\([^;]+;', header)[0] + '\n' + source[start:end] + r'''
int main(void){
    TimeEvent event={0};event.params[0].object_value=1;
    for(type=OBJ_TYPE_PORTAL;type<=OBJ_TYPE_CONTAINER;type++) {
        flags=OPF_LOCKED|OPF_LOCKED_DAY;timers=closes=0;opened=true;
        assert(!object_locked_set(1,2));
        assert((flags&OPF_NEVER_LOCKED) && !(flags&OPF_LOCKED));assert(timers==0);
        assert(flags&OPF_LOCKED_DAY);
        flags|=OPF_JAMMED;
        assert(object_lock_timeevent_process(&event));
        assert(!object_locked_get(1) && opened && closes==0 && !(flags&OPF_JAMMED));
        flags=OPF_LOCKED;timers=closes=0;opened=true;
        assert(!object_locked_set(1,false));assert(timers==1 && !(flags&OPF_NEVER_LOCKED));
        assert(object_lock_timeevent_process(&event));
        assert(object_locked_get(1) && !opened && closes==1);
        assert(!object_locked_set(1,0));assert(object_locked_set(1,1));
        flags=OPF_LOCKED;lockable=false;timers=0;
        assert(!object_locked_set(1,2));assert(flags==OPF_LOCKED && timers==0);
        lockable=true;
    }
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-locks-') as directory:
    p = Path(directory)
    (p / 'test.c').write_text(code)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-I', str(root / 'source/src'),
                    str(p / 'test.c'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True)
print('PASS: permanent and temporary unlocks, pending relock, doors, containers, invalid targets')
