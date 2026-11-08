"""Exercise the native follower traversal with a bounded synthetic party tree."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'source/src/game/object.c').read_text()
start = source.index('void object_list_all_followers(')
body = source[start:source.index('\n}', start) + 2]
start = source.index('static void object_followers_log(')
logger = source[start:source.index('\n}', start) + 2]
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct ObjectNode {int64_t obj;struct ObjectNode* next;} ObjectNode;
typedef struct ObjectList {int num_sectors;ObjectNode* head;} ObjectList;
static int live,peak,dword_5E2F98;
static int64_t children[8][4];
typedef uint64_t Uint64;
static Uint64 ticks;
static Uint64 SDL_GetTicks(void){return ticks;}
static ObjectNode* object_node_create(void){
    if(++live>64){fputs("FAIL: nested follower list keeps allocating the same entry\n",stderr);exit(93);}
    if(live>peak)peak=live;
    return calloc(1,sizeof(ObjectNode));
}
static void object_list_followers(int64_t obj,ObjectList* list){
    list->num_sectors=0;list->head=NULL;ObjectNode** tail=&list->head;
    for(int i=0;children[obj][i];i++){
        *tail=object_node_create();(*tail)->obj=children[obj][i];tail=&(*tail)->next;
    }
    ++dword_5E2F98;
}
static void object_list_destroy(ObjectList* list){
    ObjectNode* node=list->head;while(node){ObjectNode* next=node->next;free(node);live--;node=next;}
    list->head=NULL;--dword_5E2F98;
}
''' + logger + '\n' + body + r'''
static void expect(const int64_t* ids,int count){
    ticks+=1001;
    ObjectList list;object_list_all_followers(1,&list);ObjectNode* node=list.head;
    for(int i=0;i<count;i++){assert(node && node->obj==ids[i]);node=node->next;}
    assert(!node);object_list_destroy(&list);assert(live==0 && dword_5E2F98==0);
}
int main(void){
    expect(NULL,0);
    children[1][0]=2;children[1][1]=3;
    int64_t direct[]={2,3};expect(direct,2);
    children[2][0]=4;children[2][1]=5;children[4][0]=6;children[3][0]=7;
    int64_t nested[]={2,3,4,5,6,7};
    for(int i=0;i<256;i++)expect(nested,6);
    assert(peak<24);
    errno=EDOM;object_followers_log("errno-test",1,2,4,7);assert(errno==EDOM);
    puts("PASS: empty, direct and three-level follower trees preserve order, terminate and release all list nodes");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-nested-followers-') as directory:
    directory = Path(directory)
    (directory / 'test.c').write_text(code)
    logs = directory / 'ArcanumModern'
    logs.mkdir()
    (logs / 'party-traversal.log').write_bytes(b'x' * (1024 * 1024))
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                    str(directory / 'test.c'), '-o', str(directory / 'test')], check=True)
    subprocess.run([str(directory / 'test')], cwd=directory, check=True, timeout=5)
    text = (logs / 'party-traversal.log').read_text()
    assert 'phase=begin' in text and 'phase=complete' in text
    assert 'direct=2 nested=1' in text
    assert (logs / 'party-traversal.previous.log').stat().st_size == 1024 * 1024
    assert (logs / 'party-traversal.log').stat().st_size < 1024 * 1024
    print('PASS: persistent nested traversal diagnostics, rotation and errno preservation')
