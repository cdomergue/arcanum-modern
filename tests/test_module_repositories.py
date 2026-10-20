#!/usr/bin/env python3
"""Module unload releases module archives after their consumers."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root/'source/src/game/gamelib.c').read_text()
functions = []
for name in ('gamelib_unload_module_data', 'gamelib_mod_unload'):
    functions.append(re.search(r'^void '+name+r'\(void\)\n\{.*?^\}', source, re.M | re.S).group())
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#define MODULE_COUNT 2
#define GAMELIB_MAX_PATCH_COUNT 3
static bool gamelib_mod_loaded=true,dword_5D10C4=true;
static char gamelib_mod_dir_path[64]="modules/Time",byte_5D0B58[64]="module-extra";
static char gamelib_mod_dat_path[64]="modules/Time.dat",byte_5D0EA4[64]="Time";
static char gamelib_mod_patch_paths[3][64]={"modules/Time.patch0","","modules/Time.patch9"};
static unsigned char gamelib_mod_guid[16]={1};
static int calls,removed;
static void unload_last(void){assert(!removed);assert(calls++==0);}
static void unload_first(void){assert(!removed);assert(calls++==1);}
static struct {void (*mod_unload_func)(void);} gamelib_modules[2]={{unload_first},{unload_last}};
static bool tig_file_repository_remove(const char* path){
 const char* expected[]={"modules/Time","module-extra","modules/Time.patch9","modules/Time.patch0","modules/Time.dat"};
 assert(calls==2 && removed<5 && !strcmp(path,expected[removed++]));return true;
}
''' + '\n'.join(functions) + r'''
int main(void){
 gamelib_mod_unload();assert(!gamelib_mod_loaded && !dword_5D10C4 && removed==5);
 assert(!gamelib_mod_dat_path[0] && !gamelib_mod_dir_path[0] && !byte_5D0B58[0] && !byte_5D0EA4[0]);
 for(int i=0;i<3;i++)assert(!gamelib_mod_patch_paths[i][0]);
 for(int i=0;i<16;i++)assert(!gamelib_mod_guid[i]);
 gamelib_mod_unload();assert(removed==5 && calls==2);
 puts("PASS: module consumers unload before repositories; all module paths reset; repeated unload is harmless");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-modules-') as directory:
    temp=Path(directory);(temp/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(temp/'test.c'),'-o',str(temp/'test')],check=True)
    subprocess.run([str(temp/'test')],check=True)
