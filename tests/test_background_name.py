"""Run the engine's background-name reader against missing and long records."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
engine = (root / 'source/src/game/background.c').read_text(encoding='utf-8')
start = engine.index('char* background_description_get_name(int num)\n{')
implementation = engine[start:engine.index('\n}\n', start) + 3]
code = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define BACKGROUND_NAME_LENGTH 32
static char storage[BACKGROUND_NAME_LENGTH];
static char* background_description_name=storage;
static char* record;
static char* background_description_get_text(int num){(void)num;return record;}
static void tig_debug_printf(const char* text){(void)text;}
IMPLEMENTATION
int main(void){
 record=NULL;assert(!strcmp(background_description_get_name(9999),""));
 record="Name\nDescription";assert(!strcmp(background_description_get_name(1001),"Name"));
 record="12345678901234567890123456789012345678901234567890";
 memset(storage,'X',sizeof(storage));background_description_get_name(1002);
 assert(storage[31]==0 && strlen(storage)==31);
 record="";assert(!strcmp(background_description_get_name(1003),""));
 puts("PASS: missing, normal, empty and overlong background names");
}
'''.replace('IMPLEMENTATION', implementation)
with tempfile.TemporaryDirectory(prefix='renaissance-background-name-') as tmp:
    source = Path(tmp) / 'test.c'
    binary = Path(tmp) / 'test'
    source.write_text(code, encoding='utf-8')
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra',
                    '-Werror', str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
