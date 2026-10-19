#!/usr/bin/env python3
"""Exercise the engine MES parser at and beyond its string buffer boundary."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'source/src/game/mes.c').read_text()
f=re.search(r'^bool parse_field\([^\n]+\)\n\{.*?^\}',s,re.M|re.S).group()
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#define MAX_STRING 2000
typedef struct {const char* text;int index;} TigFile;
static int mes_file_parse_line=1;
static const char* current_mes_file_path="test.mes";
static int consume_next_char(TigFile* f){return f->text[f->index]?(unsigned char)f->text[f->index++]:EOF;}
#define tig_debug_printf(...) ((void)0)
'''+f+r'''
int main(void){
    char input[4100];
    for(int size=0;size<=4000;size++){
        input[0]='{';memset(input+1,'x',size);strcpy(input+1+size,"}{next}");
        TigFile stream={input,0};
        struct{char before;char text[MAX_STRING];char after;} out;
        memset(&out,'!',sizeof(out));
        assert(parse_field(&stream,out.text));
        assert(out.before=='!'&&out.after=='!');
        assert(strlen(out.text)==(size_t)(size<MAX_STRING?size:MAX_STRING-1));
        assert(parse_field(&stream,out.text)&&!strcmp(out.text,"next"));
        assert(!parse_field(&stream,out.text));
    }
    TigFile malformed={"{unterminated",0};char text[MAX_STRING];
    assert(!parse_field(&malformed,text));
    puts("PASS: empty, exact-capacity, oversized and malformed MES fields; terminator and next entry preserved");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-mes-bounds-') as d:
    p=Path(d);(p/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Wno-unused-variable',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
