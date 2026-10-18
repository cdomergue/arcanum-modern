#!/usr/bin/env python3
"""Regression for cached turn portraits outliving destroyed engine objects."""
from pathlib import Path
import os, subprocess, tempfile
s=(Path(__file__).resolve().parents[1]/'source/src/ui/modern_combat.inc').read_text()
a=s.index('static int64_t tactical_order_actor(');f=s[a:s.index('\n}',a)+2]
code=r'''
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#define OBJ_HANDLE_NULL 0
static int64_t tactical_order[128];
static int tactical_count;
static bool live=true;
/* The real engine deliberately considers null a valid empty handle. */
static bool obj_handle_is_valid(int64_t actor){return !actor || (live && actor==42);}
'''+f+r'''
int main(void){
 tactical_count=2;tactical_order[0]=42;tactical_order[1]=43;
 assert(tactical_order_actor(0)==42);assert(!tactical_order_actor(1));
 live=false;assert(!tactical_order_actor(0));
 assert(!tactical_order_actor(-1));assert(!tactical_order_actor(2));
 tactical_count=200;assert(!tactical_order_actor(128));
 tactical_count=0;assert(!tactical_order_actor(0));
}
'''
assert 'int64_t obj=tactical_order_actor(i);if(!obj)continue;' in s
assert 'tactical_order_actor(action-1200)' in s
portrait=s.split('static void tactical_portrait(')[1].split('static bool tactical_active')[0]
assert portrait.index('!obj_handle_is_valid(actor)')<portrait.index('intgame_examine_portrait(')
with tempfile.TemporaryDirectory(prefix='arcanum-lifetime-') as folder:
 p=Path(folder);(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: live/released/null handles, cleared list, bounds, portrait and click guards')
