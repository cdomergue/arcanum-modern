from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'source/src/ui/wmap_ui.c').read_text()
a=s.index('const char* modern_map_local_note(')
s=s[a:s.index('\n}',a)+2]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define WMAP_UI_MODE_TOWN 2
#define WMAP_NOTE_TYPE_COUNT 3
typedef struct { int x,y; } Coords;
typedef struct {int type; Coords coords; char str[250];} WmapNote;
typedef struct {int map_width,map_height; WmapNote* notes; int* num_notes;} WmapInfo;
static bool wmap_ui_created=true;
static int wmap_ui_mode=2,dword_66D878=101,wmap_ui_townmap=101,wmap_ui_num_town_notes=2;
static WmapNote notes[2]={{0,{100,200},"Prison haute s\xe9" "curit\xe9 de Caladon"},{1,{500,400},"English landmark"}};
static WmapInfo wmap_ui_mode_info[3]={[2]={1000,800,notes,&wmap_ui_num_town_notes}};
static struct {bool enabled;int width,height;}wmap_note_type_info[3]={{true,12,20},{true,24,16},{false,12,12}};
static int logs;
static void town_notes_log(const char* e,int p,int s,int c,const char* o){(void)e;(void)p;(void)s;(void)c;(void)o;logs++;}
''' + s + r'''
int main(void){
 WmapNote before[2];memcpy(before,notes,sizeof(notes));
 assert(modern_map_local_note(100,200,10)==notes[0].str);
 for(int i=0;i<1000;i++)assert(modern_map_local_note(100,200,10)==notes[0].str);
 assert(logs==1);
 assert(!modern_map_local_note(111,200,10));
 assert(modern_map_local_note(111,200,20)==notes[0].str); /* zoomed-out screen radius */
 assert(modern_map_local_note(512,400,2)==notes[1].str); /* full icon extent when zoomed in */
 assert(!modern_map_local_note(513,400,2));
 assert(!modern_map_local_note(-1,200,1000));
 assert(!modern_map_local_note(1000,200,1000));
 assert(!modern_map_local_note(100,800,1000));
 dword_66D878=205;assert(!modern_map_local_note(100,200,10));dword_66D878=101;
 wmap_ui_mode=0;assert(!modern_map_local_note(100,200,10));wmap_ui_mode=2;
 wmap_ui_created=false;assert(!modern_map_local_note(100,200,10));wmap_ui_created=true;
 wmap_note_type_info[0].enabled=false;assert(!modern_map_local_note(100,200,10));wmap_note_type_info[0].enabled=true;
 assert(!memcmp(before,notes,sizeof(notes))); /* pure read, no saved annotation changes */
 notes[1].coords=notes[0].coords;assert(modern_map_local_note(100,200,10)==notes[1].str);
 notes[1].str[0]=0;assert(modern_map_local_note(100,200,10)==notes[0].str);
 notes[1].type=99;assert(modern_map_local_note(100,200,10)==notes[0].str);
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-native-notes-') as folder:
 p=Path(folder);(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: native note text, zoom hit radius, bounds, city ownership, disabled/empty/overlapping notes, read-only hover and deduplicated diagnostics')
