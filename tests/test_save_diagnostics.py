"""Exercise production save logging, rotation and unwritable destinations."""
from pathlib import Path
import subprocess
import tempfile
import os
root=Path(__file__).resolve().parents[1]
source=(root/'source/src/game/modern_save_log.inc').read_text()
code=r'''
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>
static bool failed;
static bool sector_save_failed(void){return failed;}
'''+source+r'''
int main(void){
 errno=EDOM;gamelib_save_log("save_begin","SlotTest");assert(errno==EDOM);
 assert(mkdir("ArcanumModern",0700)==0);
 failed=true;save_diagnostic_phase="sector_retry";
 errno=EDOM;gamelib_save_log("save_failure","SlotTest");assert(errno==EDOM);
 FILE* f=fopen("ArcanumModern/save-events.log","rb");assert(f);char line[2048];assert(fgets(line,sizeof(line),f));fclose(f);
 assert(strstr(line,"event=save_failure detail=SlotTest phase=sector_retry sector_failed=1"));
 assert(line[4]=='-'&&line[10]=='T');
 f=fopen("ArcanumModern/save-events.log","ab");assert(f);for(int i=0;i<1024*1024;i++)fputc('x',f);fclose(f);
 failed=false;gamelib_save_log("save_success","SlotTest");
 f=fopen("ArcanumModern/save-events.previous.log","rb");assert(f);fseek(f,0,SEEK_END);assert(ftell(f)>=1024*1024);fclose(f);
 f=fopen("ArcanumModern/save-events.log","rb");assert(f);assert(fgets(line,sizeof(line),f));assert(strstr(line,"save_success")&&strstr(line,"sector_failed=0"));assert(fgetc(f)==EOF);fclose(f);
 puts("PASS: timestamps, save context, errno preservation, bounded rotation, missing directory");
}
'''
with tempfile.TemporaryDirectory(prefix='save-log-') as folder:
 p=Path(folder)/'test';p.with_suffix('.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p.with_suffix('.c')),'-o',str(p)],check=True)
 subprocess.run([str(p)],cwd=folder,check=True)
