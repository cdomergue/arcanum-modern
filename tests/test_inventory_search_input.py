#!/usr/bin/env python3
"""A search must not swallow item assignment or the following SDL text event."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'source/src/ui/modern_panels.inc').read_text()
a=s.index('static bool inventory_search_event(');body=s[a:s.index('\n}',a)+2]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
enum {SDL_EVENT_KEY_DOWN,SDL_EVENT_KEY_UP,SDL_EVENT_TEXT_INPUT,SDL_EVENT_MOUSE_BUTTON_DOWN};
enum {SDL_SCANCODE_1=30,SDL_SCANCODE_0=39,SDL_SCANCODE_ESCAPE=41,SDL_SCANCODE_RETURN,SDL_SCANCODE_BACKSPACE};
#define HOTKEY_NONE 0
typedef struct {int type;struct {int scancode;bool repeat;} key;struct {const char* text;} text;} SDL_Event;
typedef struct {int type;} QuickBinding;
static bool inventory_searching;static float mouse_x,mouse_y;
static int hovered,bindings,action=2400;static char query[80];
static QuickBinding binding_hover(void){return (QuickBinding){hovered};}
static bool binding_event(SDL_Event* e){(void)e;bindings++;return true;}
static void inventory_search_focus(bool on){inventory_searching=on;}
static int hit(float x,float y){(void)x;(void)y;return action;}
static void inventory_query_edit(const char* s,bool backspace){if(backspace){if(*query)query[strlen(query)-1]=0;}else strcat(query,s);}
'''+body+r'''
int main(void){
 SDL_Event key={.type=SDL_EVENT_KEY_DOWN,.key={SDL_SCANCODE_1,false}};
 SDL_Event text={.type=SDL_EVENT_TEXT_INPUT,.text={"&"}};
 inventory_searching=true;strcpy(query,"potion");hovered=1;
 assert(inventory_search_event(&key));assert(bindings==1&&!inventory_searching);
 assert(!inventory_search_event(&text));assert(!strcmp(query,"potion"));
 key.key.repeat=true;assert(!inventory_search_event(&key));assert(bindings==1);
 key.type=SDL_EVENT_KEY_UP;assert(!inventory_search_event(&key));
 inventory_searching=true;hovered=0;key.type=SDL_EVENT_KEY_DOWN;key.key.repeat=false;
 assert(inventory_search_event(&key));text.text.text="2";assert(inventory_search_event(&text));assert(!strcmp(query,"potion2")&&bindings==1);
 key.key.scancode=SDL_SCANCODE_BACKSPACE;inventory_search_event(&key);assert(!strcmp(query,"potion"));
 SDL_Event click={.type=SDL_EVENT_MOUSE_BUTTON_DOWN};inventory_search_event(&click);assert(inventory_searching);
 action=-1;inventory_search_event(&click);assert(!inventory_searching);
 inventory_searching=true;key.key.scancode=SDL_SCANCODE_ESCAPE;assert(inventory_search_event(&key));assert(!inventory_searching);
 puts("PASS: hovered assignment, AZERTY text suppression, repeat/release, numeric search, backspace and click-outside focus");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-search-input-') as folder:
 p=Path(folder);(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
