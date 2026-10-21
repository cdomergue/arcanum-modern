from pathlib import Path
import os
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / 'source/src/ui/modern_ui.c').read_text()


def function(name):
    start = source.index('static void ' + name + '(')
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
typedef enum { TIG_BUTTON_STATE_PRESSED, TIG_BUTTON_STATE_RELEASED,
    TIG_BUTTON_STATE_MOUSE_INSIDE, TIG_BUTTON_STATE_MOUSE_OUTSIDE } TigButtonState;
enum { TIG_OK, TIG_ERROR };
typedef struct { int button_handle; } MenuButton;
typedef struct { int num_buttons; MenuButton buttons[2]; } MainMenuWindowInfo;
typedef struct { int mouse_down_snd_id,mouse_up_snd_id,mouse_enter_snd_id,mouse_exit_snd_id; } TigButtonData;
static MainMenuWindowInfo info={2,{{0},{1}}};
static TigButtonData sounds[]={{3027,-1,3026,-1},{3000,3001,-1,-1}};
static bool active=true,confirming_menu,hidden;
static int menu_hovered_action=-1,failed,played[32],count;
static bool native_menu(void){return active;}
static int mainmenu_ui_window_type_get(void){return 0;}
static MainMenuWindowInfo* sub_5496C0(int type){(void)type;return &info;}
static int tig_button_is_hidden(int handle,bool* value){(void)handle;*value=hidden;return TIG_OK;}
static int tig_button_data(int handle,TigButtonData* data){*data=sounds[handle];return failed;}
static void tig_sound_quick_play(int id){assert(id>=0);played[count++]=id;}
'''
code += function('menu_button_sound') + '\n' + function('menu_hover_sound')
code += r'''
int main(void)
{
    menu_hover_sound(500);
    assert(count==1 && played[0]==3026);
    for(int i=0;i<100;i++)menu_hover_sound(500);
    assert(count==1);
    menu_button_sound(500,TIG_BUTTON_STATE_PRESSED);
    assert(count==2 && played[1]==3027);
    menu_button_sound(500,TIG_BUTTON_STATE_RELEASED);
    assert(count==2);
    menu_hover_sound(-1);menu_hover_sound(500);
    assert(count==3 && played[2]==3026);
    menu_hover_sound(501);
    assert(count==3);
    menu_button_sound(501,TIG_BUTTON_STATE_PRESSED);
    menu_button_sound(501,TIG_BUTTON_STATE_RELEASED);
    assert(count==5 && played[3]==3000 && played[4]==3001);
    menu_button_sound(499,TIG_BUTTON_STATE_PRESSED);
    menu_button_sound(502,TIG_BUTTON_STATE_PRESSED);
    hidden=true;menu_button_sound(500,TIG_BUTTON_STATE_PRESSED);hidden=false;
    failed=TIG_ERROR;menu_button_sound(500,TIG_BUTTON_STATE_PRESSED);failed=TIG_OK;
    confirming_menu=true;menu_button_sound(500,TIG_BUTTON_STATE_PRESSED);confirming_menu=false;
    active=false;menu_button_sound(500,TIG_BUTTON_STATE_PRESSED);active=true;
    assert(count==5);
    menu_hovered_action=-1;menu_hover_sound(500);
    assert(count==6 && played[5]==3026);
    puts("PASS: original hover/click/release IDs, one hover per entry, silent unsupported events, hidden/modal/inactive/invalid buttons");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-menu-feedback-') as folder:
    path = Path(folder) / 'check.c'
    binary = Path(folder) / 'check'
    path.write_text(code)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra',
                    '-Werror', str(path), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
