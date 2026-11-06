"""Exercise production text readers with high bytes and bounded destinations."""
from pathlib import Path
import subprocess
import tempfile
import os
root=Path(__file__).resolve().parents[1]/'source'
def function(path,signature):
    s=(root/path).read_text();start=s.index(signature+'\n{');return s[start:s.index('\n}',start)+2]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
typedef struct {const char* text;int pos;} TigFile;
static char* tig_file_fgets(char* out,int size,TigFile* file){
 if(!file->text[file->pos])return NULL;
 int i=0;while(i<size-1&&file->text[file->pos])out[i++]=file->text[file->pos++];
 out[i]=0;return out;
}
static char tig_str_parse_separator=',';
static char dialog_file_tmp_str[2],mes_file_parse_buffer[2];
static int dialog_file_tmp_str_pos,dialog_file_tmp_str_size,mes_file_parse_pos,mes_file_parse_len;
'''
for path,signature in [
 ('first_party/tig/src/str_parse.c','void tig_str_parse_str_value(char** str, char* value, size_t size)'),
 ('first_party/tig/src/str_parse.c','bool tig_str_parse_named_str_value(char** str, const char* name, char* value, size_t size)'),
 ('src/game/dialog.c','int dialog_file_fgetc(TigFile* stream)'),
 ('src/game/mes.c','int consume_next_char(TigFile* stream)')]:
 code+=function(path,signature)+'\n'
code+=r'''
int main(void){
 for(int reader=0;reader<2;reader++){
  char bytes[129];for(int i=0;i<128;i++)bytes[i]=(char)(128+i);bytes[128]=0;
  TigFile file={bytes,0};for(int i=128;i<256;i++)assert((reader?consume_next_char(&file):dialog_file_fgetc(&file))==i);
  assert((reader?consume_next_char(&file):dialog_file_fgetc(&file))==EOF);
 }
 for(int size=0;size<=8;size++){
  unsigned char out[10];memset(out,0xA5,sizeof(out));char input[]="  abcdefghijk,next";char* p=input;
  tig_str_parse_str_value(&p,(char*)out+1,size);
  assert(out[0]==0xA5&&out[size+1]==0xA5&&strcmp(p,"next")==0&&input[13]==',');
  if(size)assert(out[size]==0);
 }
 char input[]="Name: \xE9\xFF,next",out[8];char* p=input;
 assert(tig_str_parse_named_str_value(&p,"Name:",out,sizeof(out)));
 assert((unsigned char)out[0]==233&&(unsigned char)out[1]==255&&out[2]==0);
 p=NULL;assert(!tig_str_parse_named_str_value(&p,"Name:",NULL,0));
 p=input;assert(!tig_str_parse_named_str_value(&p,"Other:",NULL,0));
 puts("PASS: high-byte text, EOF, truncation, separators, zero-sized buffers");
}
'''
with tempfile.TemporaryDirectory(prefix='ce-text-') as folder:
 p=Path(folder)/'test';p.with_suffix('.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(p.with_suffix('.c')),'-o',str(p)],check=True)
 subprocess.run([str(p)],check=True)
