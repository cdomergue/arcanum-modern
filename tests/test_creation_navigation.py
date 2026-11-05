"""Exercise actual menu stack functions when a starting kit cannot be inserted."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
engine = (root / 'source/src/ui/mainmenu_ui.c').read_text(encoding='utf-8')
functions = []
for signature in ('void mainmenu_ui_open(void)', 'void mainmenu_ui_close(bool back)',
                  'MainMenuWindowType mainmenu_ui_pop_window_stack(void)',
                  'void mainmenu_ui_push_window_stack(MainMenuWindowType window_type)'):
    start = engine.index(signature + '\n{')
    end = engine.index('\n}', start) + 2
    functions.append(engine[start:end])
source = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
typedef enum { MM_WINDOW_0,MM_WINDOW_NEW_CHAR,MM_WINDOW_CHAREDIT,MM_WINDOW_SHOP } MainMenuWindowType;
typedef struct { void (*init_func)(void);void (*exit_func)(void); } Window;
static MainMenuWindowType mainmenu_ui_window_type,mainmenu_ui_window_stack[16];
static int mainmenu_ui_num_windows,dword_5C3FB8;
static bool mainmenu_ui_active,dword_64C38C,modern_creation_loadout_return,mainmenu_ui_start_new_game;
static struct { bool resume; } modern_creation_draft;
static Window windows[4];static Window* main_menu_window_info[4];
static int restored;static bool fail_shop;
void mainmenu_ui_open(void);void mainmenu_ui_close(bool back);
MainMenuWindowType mainmenu_ui_pop_window_stack(void);
void mainmenu_ui_push_window_stack(MainMenuWindowType window_type);
static void mainmenu_ui_create_window(void){mainmenu_ui_active=true;}
static void sub_546DD0(void){mainmenu_ui_active=false;}
IMPLEMENTATION
static void init_character(void){mainmenu_ui_active=true;if(modern_creation_draft.resume){restored++;modern_creation_draft.resume=false;}}
static void init_shop(void){mainmenu_ui_active=true;if(fail_shop)modern_creation_loadout_return=true;else mainmenu_ui_start_new_game=true;}
static void next(MainMenuWindowType type){mainmenu_ui_close(false);mainmenu_ui_window_type=type;mainmenu_ui_open();}
int main(void){
 for(int i=0;i<4;i++)main_menu_window_info[i]=&windows[i];
 windows[MM_WINDOW_NEW_CHAR].init_func=init_character;windows[MM_WINDOW_SHOP].init_func=init_shop;
 mainmenu_ui_window_type=MM_WINDOW_NEW_CHAR;mainmenu_ui_open();
 next(MM_WINDOW_CHAREDIT);assert(mainmenu_ui_num_windows==2);
 fail_shop=true;next(MM_WINDOW_SHOP);
 assert(mainmenu_ui_window_type==MM_WINDOW_NEW_CHAR&&mainmenu_ui_active&&restored==1);
 assert(mainmenu_ui_num_windows==1&&mainmenu_ui_window_stack[0]==MM_WINDOW_NEW_CHAR);
 assert(!modern_creation_loadout_return&&!mainmenu_ui_start_new_game);
 fail_shop=false;next(MM_WINDOW_CHAREDIT);next(MM_WINDOW_SHOP);
 assert(mainmenu_ui_num_windows==3&&mainmenu_ui_window_type==MM_WINDOW_SHOP&&mainmenu_ui_start_new_game);
 mainmenu_ui_close(true);assert(mainmenu_ui_window_type==MM_WINDOW_CHAREDIT&&mainmenu_ui_num_windows==2);
 mainmenu_ui_close(true);assert(mainmenu_ui_window_type==MM_WINDOW_NEW_CHAR&&restored==2);
 assert(mainmenu_ui_num_windows==1&&dword_5C3FB8==-1);
 puts("PASS: shop failure deferred until stack insertion, draft restoration, successful retry and ordinary Back");
}
'''.replace('IMPLEMENTATION', '\n'.join(functions))
with tempfile.TemporaryDirectory(prefix='renaissance-navigation-') as tmp:
    code = Path(tmp) / 'test.c'
    binary = Path(tmp) / 'test'
    code.write_text(source, encoding='utf-8')
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra', '-Werror', str(code), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
