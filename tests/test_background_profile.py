"""Exercise actual annotation I/O and draft transfer, including a disk failure."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
implementation = (root / 'source/src/ui/modern_background_profile.inc').read_text(encoding='utf-8')
source = r'''
#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#define OID_STR_LEN 80
#define SDL_isalnum(c) isalnum((unsigned char)(c))
static bool rename_ok=true;
static bool obj_handle_is_valid(int64_t o){return o>0 && o<4;}
static int64_t obj_get_id(int64_t o){return o;}
static void objid_id_to_str(char* s,int64_t o){snprintf(s,OID_STR_LEN,"test-%d",(int)o);}
static void SDL_strlcpy(char* a,const char* b,size_t n){assert(strlen(b)<n);memcpy(a,b,strlen(b)+1);}
/* Files live only in a fresh temporary test directory. The stub injects rename
 * failure; atomic overwrite itself is provided by SDL in the game. */
static bool SDL_RenamePath(const char* from,const char* to){if(!rename_ok)return false;remove(to);return rename(from,to)==0;}
IMPLEMENTATION
int main(void){
 char title[96],story[1024];
 strcpy(background_title,"First");strcpy(background_story,"Line 1\nLine 2");
 assert(background_profile_write(1));
 modern_background_creation_begin(1);rename_ok=false;modern_background_creation_finish(2);
 assert(background_pending_profile.active);
 assert(background_profile_read(2,title,sizeof(title),story,sizeof(story)));
 assert(!strcmp(title,"First") && !strcmp(story,"Line 1\nLine 2"));
 assert(!modern_background_creation_ready(2));
 strcpy(background_title,"Unsaved edit");assert(!background_profile_write(2));
 assert(background_profile_read(2,title,sizeof(title),story,sizeof(story)) && !strcmp(title,"First"));
 rename_ok=true;assert(modern_background_creation_ready(2));assert(!background_pending_profile.active);
 assert(background_profile_read(2,title,sizeof(title),story,sizeof(story)) && !strcmp(title,"First"));
 /* Failed transfer never removes the original annotation. */
 assert(background_profile_read(1,title,sizeof(title),story,sizeof(story)) && !strcmp(title,"First"));
 modern_background_creation_begin(2);rename_ok=false;modern_background_creation_finish(3);
 rename_ok=true;strcpy(background_title,"Applied edit");assert(background_profile_write(3));
 assert(!background_pending_profile.active && modern_background_creation_ready(3));
 assert(background_profile_read(3,title,sizeof(title),story,sizeof(story)) && !strcmp(title,"Applied edit"));
 /* A fresh creation clears a pending transfer rather than inheriting prose. */
 modern_background_creation_begin(2);rename_ok=false;modern_background_creation_finish(3);
 modern_background_creation_begin(0);assert(!background_pending_profile.active);
 assert(!background_profile_read(0,title,sizeof(title),story,sizeof(story)) && !*title && !*story);
 assert(!background_profile_write(0));
 puts("PASS: profile I/O, transfer, retained draft after disk failure, retry, edit, fresh creation");
}
'''.replace('IMPLEMENTATION', implementation)
with tempfile.TemporaryDirectory(prefix='renaissance-background-profile-') as tmp:
    code = Path(tmp) / 'test.c'
    binary = Path(tmp) / 'test'
    (Path(tmp) / 'modern-ui').mkdir()
    code.write_text(source, encoding='utf-8')
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra', '-Werror', str(code), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], cwd=tmp, check=True)
