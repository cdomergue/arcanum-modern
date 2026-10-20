#!/usr/bin/env python3
"""Check optional UAP settings without proprietary game files."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
header = (root/'source/src/game/modern_compat.h').read_text()
source = (root/'source/src/game/modern_compat.c').read_text()
source = '\n'.join(line for line in source.splitlines() if not line.startswith('#include'))
code = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define RACE_COUNT 11
#define RACE_DARK_ELF 8
#define SDL_arraysize(a) (sizeof(a)/sizeof((a)[0]))
typedef FILE TigFile;
#define tig_file_fopen fopen
#define tig_file_fclose fclose
#define tig_file_fseek fseek
#define tig_file_fread fread
static int mirrored, overlay;
static int modern_uap_ready(void) { return overlay; }
static int tig_file_exists(const char* name, void* ignored) { (void)ignored; return access(name, R_OK)==0; }
static void tig_art_mirroring_set(int enabled) { mirrored=enabled; }
''' + header + source + r'''
static void touch(const char* name) { FILE* f=fopen(name,"wb"); assert(f); fclose(f); }
static void write_at(long offset, const char* bytes, size_t size) {
 FILE* f=fopen("Arcanum.exe","r+b"); assert(f);
 assert(fseek(f,offset,SEEK_SET)==0); assert(fwrite(bytes,1,size,f)==size); fclose(f);
}
static void check_defaults(void) {
 const ModernCompatibility* c=modern_compat_get();
 assert(c->level_limit==51 && !c->master_educator && !c->no_combat_music && !c->no_xp_per_hit);
}
int main(void) {
 modern_compat_load(); check_defaults(); assert(mirrored && !modern_compat_get()->races);
 assert(!strcmp(modern_compat_race_prefix(8),"EL"));
 assert(!strcmp(modern_compat_race_prefix(9),"HG"));
 assert(!strcmp(modern_compat_race_prefix(10),"HO"));
 assert(!strcmp(modern_compat_race_prefix(-1),""));
 assert(!strcmp(modern_compat_race_prefix(11),""));
 touch("Arcanum5.dat"); touch("ArcanumXRaces.dat"); touch("ArcanumXAnims1Base.dat");
 modern_compat_load(); check_defaults(); assert(!mirrored && modern_compat_get()->races);
 assert(!strcmp(modern_compat_race_prefix(8),"DE"));
 assert(!strcmp(modern_compat_race_prefix(9),"OG"));
 assert(!strcmp(modern_compat_race_prefix(10),"OC"));
 touch("Arcanum.exe");
 write_at(0,"MZ",2); write_at(0x45e0,"\x04\x6a\x07\x6a\0\x6a\x01\x68",8);
 modern_compat_load(); check_defaults();
 const long offsets[]={0xa6642,0xa66a6,0xa68c8,0xa691b,0x1b5248};
 for(int i=0;i<4;i++) write_at(offsets[i],"\x65",1);
 modern_compat_load(); check_defaults();
 write_at(offsets[4],"\x65",1); modern_compat_load(); assert(modern_compat_get()->level_limit==101);
 write_at(0xc2982,"\xeb\x04\0\0",4); write_at(0xc29b1,"\x7c",1);
 write_at(0xc29cd,"\x90",1); write_at(0xc29d0,"\x74",1);
 modern_compat_load(); assert(!modern_compat_get()->master_educator);
 write_at(0xc29e6,"\x74",1); modern_compat_load(); assert(modern_compat_get()->master_educator);
 write_at(0x1c4d0,"\xc3\xd8\x55\x5d\0",5);
 write_at(0xb52ce,"\xeb\x15",2); write_at(0x5db27,"\xeb\x15\0\0",4);
 modern_compat_load(); assert(modern_compat_get()->no_combat_music && !modern_compat_get()->no_xp_per_hit);
 write_at(0x5db3e,"\x50",1); modern_compat_load(); assert(modern_compat_get()->no_xp_per_hit);
 write_at(0x45e0,"\x05",1); modern_compat_load(); check_defaults();
 remove("Arcanum5.dat"); remove("ArcanumXRaces.dat"); remove("ArcanumXAnims1Base.dat");
 modern_compat_load(); check_defaults(); assert(!modern_compat_get()->uap && !modern_compat_get()->races && mirrored);
 overlay=1; modern_compat_load(); check_defaults();
 assert(modern_compat_get()->uap && modern_compat_get()->races && !mirrored);
 assert(!strcmp(modern_compat_race_prefix(8),"DE"));
 overlay=0; modern_compat_load(); assert(!modern_compat_get()->uap && mirrored);
 puts("PASS: vanilla defaults, race prefixes, animation mode, complete/partial engine patches, version guard and reset");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-uap-') as directory:
    temp = Path(directory)
    (temp/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra', '-Werror',
                    str(temp/'test.c'), '-o', str(temp/'test')], check=True)
    subprocess.run([str(temp/'test')], cwd=temp, check=True)
