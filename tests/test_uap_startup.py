#!/usr/bin/env python3
"""Exercise the production startup choice for both detected editions."""
from pathlib import Path
import os, re, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'source/src/ui/modern_uap_startup.c').read_text()
function=re.search(r'^bool modern_uap_startup\(void\)\n\{.*?^\}',source,re.M|re.S).group()
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#define UAP_HELPER "helper"
#define SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT 1
#define SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT 2
#define SDL_MESSAGEBOX_WARNING 4
#define SDL_MESSAGEBOX_ERROR 8
typedef int SDL_Window;
typedef struct {int flags,buttonid;const char* text;} SDL_MessageBoxButtonData;
typedef struct {int flags;SDL_Window* window;const char* title;const char* message;int count;const SDL_MessageBoxButtonData* buttons;void* scheme;} SDL_MessageBoxData;
static bool available=true,prompt_ok=true;
static int probe=21,choice,install_result,calls,prompts,errors;
static bool expected_french;
static const char* expected_mode;
static bool SDL_GetPathInfo(const char* name,void* info){(void)name;(void)info;return available;}
static int helper(const char* mode,bool french){calls++;if(!strcmp(mode,"probe"))return probe;assert(!strcmp(mode,expected_mode)&&french==expected_french);return install_result;}
static void tig_video_window_get(SDL_Window** w){*w=NULL;}
static bool SDL_ShowMessageBox(const SDL_MessageBoxData* box,int* result){
 prompts++;assert(box->count==2);
 assert(strstr(box->message,expected_french?"adapté en français":"for your English game"));
 assert(!strcmp(box->buttons[1].text,expected_french?"Oui":"Yes"));
 *result=choice;return prompt_ok;
}
static void SDL_ShowSimpleMessageBox(int flags,const char* title,const char* message,SDL_Window* window){(void)flags;(void)title;(void)window;errors++;assert(strstr(message,expected_french?"uap-fr/":"uap-en/"));}
'''+function+r'''
int main(void){
 available=false;assert(modern_uap_startup()&&!calls);available=true;
 probe=0;assert(modern_uap_startup()&&!prompts);
 probe=1;assert(modern_uap_startup()&&!prompts);
 probe=-1;assert(!modern_uap_startup()&&!prompts);
 for(int french=0;french<2;french++){
  probe=french?20:21;expected_french=french;choice=0;expected_mode="decline";install_result=0;
  assert(modern_uap_startup());
  choice=1;expected_mode="install";assert(modern_uap_startup());
  int before=errors;install_result=1;assert(modern_uap_startup()&&errors==before+1);
  install_result=-1;assert(!modern_uap_startup());
  prompt_ok=false;int old=calls;assert(modern_uap_startup()&&calls==old+1);prompt_ok=true;
 }
 puts("PASS: startup detection, FR/EN prompts, language forwarding, decline, install, errors and cancellation");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-uap-startup-') as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
