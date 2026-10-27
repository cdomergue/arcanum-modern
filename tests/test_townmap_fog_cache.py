#!/usr/bin/env python3
"""Check production fog bitmap caching and exploration invalidation."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'source/src/game/townmap.c').read_text()


def function(name):
    match = re.search(r'^\w+ ' + name + r'\([^\n]*\)\n\{', source, re.M)
    assert match, name
    return source[match.start():source.index('\n}', match.end()) + 2] + '\n'


code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TIG_MAX_PATH 260
#define FREE free
static uint8_t* townmap_kstate;
static uint8_t* townmap_tile_availability;
static int townmap_kstate_size,townmap_kstate_map,townmap_tile_availability_count;
static bool townmap_kstate_modified,loaded=true;
static uint64_t townmap_exploration_generation;
static int probes;
static bool townmap_kstate_load(int map){(void)map;return loaded;}
static const char* townmap_name(int map){(void)map;return "fixture";}
static bool tig_file_exists(const char* path,void* unused){(void)unused;probes++;return strstr(path,"000000.bmp")!=NULL;}
static void fixture(void){townmap_kstate=calloc(1,1);townmap_kstate_size=1;townmap_tile_availability_count=3;townmap_tile_availability=calloc(3,1);}
'''
for name in ['townmap_kstate_reset', 'townmap_tile_known_get', 'townmap_tile_known_set',
             'townmap_set_known_internal', 'townmap_exploration_revision']:
    code += function(name)
code += r'''
int main(void){
 fixture();uint64_t rev=townmap_exploration_revision(1);
 assert(!townmap_tile_known_get(1,0)&&probes==0);
 townmap_tile_known_set(1,0);assert(townmap_exploration_revision(1)!=rev);
 rev=townmap_exploration_revision(1);townmap_tile_known_set(1,0);
 assert(townmap_exploration_revision(1)==rev);
 for(int i=0;i<1000;i++)assert(townmap_tile_known_get(1,0));
 assert(probes==1);
 townmap_tile_known_set(1,1);
 for(int i=0;i<1000;i++)assert(!townmap_tile_known_get(1,1));
 assert(probes==2); /* missing bitmaps must also remain cached */
 assert(!townmap_tile_known_get(1,-1)&&!townmap_tile_known_get(1,3));
 townmap_tile_known_set(1,-1);townmap_tile_known_set(1,3);
 rev=townmap_exploration_revision(1);townmap_set_known_internal(1,false);
 assert(townmap_exploration_revision(1)!=rev&&!townmap_tile_known_get(1,0));
 rev=townmap_exploration_revision(1);townmap_set_known_internal(1,false);
 assert(townmap_exploration_revision(1)==rev);
 townmap_set_known_internal(1,true);assert(townmap_tile_known_get(1,0)&&probes==2);
 loaded=false;assert(!townmap_exploration_revision(1)&&!townmap_tile_known_get(1,0));loaded=true;
 rev=townmap_exploration_revision(1);townmap_kstate_reset();fixture();
 assert(townmap_exploration_revision(1)!=rev);
 townmap_tile_known_set(1,0);assert(townmap_tile_known_get(1,0)&&probes==3);
 townmap_kstate_reset();assert(!townmap_tile_availability&&!townmap_kstate);
 puts("PASS: one repository probe per tile, missing tiles cached, exploration changes invalidate, reset releases cache");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-fog-cache-') as directory:
    path = Path(directory)
    (path / 'test.c').write_text(code)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra', '-Werror',
                    str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
