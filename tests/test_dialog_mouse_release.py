#!/usr/bin/env python3
"""Exercise actual mouse repeat/capture code across a conversation transition."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
mouse = (root / "source/first_party/tig/src/mouse.c").read_text()
message = (root / "source/first_party/tig/src/message.c").read_text()
intgame = (root / "source/src/ui/intgame.c").read_text()


def block(source, marker):
    start = source.index(marker)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


capture = block(message, "if (native_event_filter != NULL && native_event_filter(&event))")
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
enum { TIG_MOUSE_BUTTON_LEFT, TIG_MOUSE_BUTTON_RIGHT, TIG_MOUSE_BUTTON_MIDDLE, TIG_MOUSE_BUTTON_COUNT };
enum { TIG_MESSAGE_MOUSE, TIG_MESSAGE_MOUSE_IDLE, SDL_EVENT_MOUSE_BUTTON_UP };
enum { SDL_BUTTON_LEFT=1, SDL_BUTTON_MIDDLE, SDL_BUTTON_RIGHT };
enum { INTGAME_MODE_MAIN, INTGAME_MODE_DIALOG };
typedef struct { int type, timestamp; struct { struct {int x,y,event;bool repeat;} mouse; } data; } TigMessage;
typedef struct { int type; struct {int button;} button; } SDL_Event;
static struct {int flags,x,y,frame;} tig_mouse_state;
static int tig_ping_timestamp, tig_mouse_software_button_timestamps[3];
static int tig_mouse_state_button_down_flags[]={2,16,128};
static int tig_mouse_state_button_down_repeat_flags[]={4,32,256};
static int tig_mouse_state_button_up_flags[]={8,64,512};
static int tig_message_mouse_button_down_flags[]={10,11,12};
static int tig_message_mouse_button_up_flags[]={20,21,22};
static int tig_mouse_cursor_art_num_frames=1, tig_mouse_cursor_art_animation_timestamp;
static int tig_mouse_cursor_art_fps, tig_mouse_move_timestamp;
static bool tig_mouse_active=true, tig_mouse_idle_emitted=true;
static TigMessage events[64];static int event_count;
static void tig_message_enqueue(TigMessage* m){events[event_count++]=*m;}
static int tig_timer_between(int a,int b){return b-a;}
static void tig_mouse_cursor_animate(void){}
static void tig_window_invalidate_rect(int* r){(void)r;}
static bool consume(SDL_Event* e){(void)e;return true;}
static bool (*native_event_filter)(SDL_Event*)=consume;
static int dword_64C6D8;
static bool (*intgame_dialog_process_event_func)(TigMessage*);
static void tc_show(void){} static void tc_hide(void){}
static void intgame_mode_set(int mode){(void)mode;}
'''
for name in ["void tig_mouse_ping(void)", "void tig_mouse_set_button(int button, bool pressed)",
             "void tig_mouse_cancel_button(int button)"]:
    code += block(mouse, name) + "\n"
code += "static void captured_release(SDL_Event event) { do {" + capture + "} while(0);}\n"
code += block(intgame, "bool intgame_dialog_begin(bool (*func)(TigMessage* msg))")
code += block(intgame, "void intgame_dialog_end(void)")
code += r'''
int main(void){
    /* World DOWN, dialog owns UP: never repeat movement after the reply. */
    tig_mouse_set_button(TIG_MOUSE_BUTTON_LEFT,true);
    captured_release((SDL_Event){SDL_EVENT_MOUSE_BUTTON_UP,{SDL_BUTTON_LEFT}});
    int count=event_count;
    tig_ping_timestamp=1000;tig_mouse_ping();
    assert(event_count==count); /* Also no synthetic release action. */
    /* A held click is cancelled as soon as dialogue takes over. */
    tig_mouse_set_button(TIG_MOUSE_BUTTON_LEFT,true);dword_64C6D8=true;
    intgame_dialog_begin(NULL);count=event_count;
    tig_ping_timestamp+=1000;tig_mouse_ping();
    assert(event_count==count && !dword_64C6D8);
    tig_mouse_set_button(TIG_MOUSE_BUTTON_LEFT,true);dword_64C6D8=true;
    intgame_dialog_end();count=event_count;
    tig_ping_timestamp+=1000;tig_mouse_ping();
    assert(event_count==count && !dword_64C6D8);
    /* New world presses still repeat; another button remains independent. */
    tig_mouse_set_button(TIG_MOUSE_BUTTON_LEFT,true);
    tig_mouse_set_button(TIG_MOUSE_BUTTON_RIGHT,true);
    captured_release((SDL_Event){SDL_EVENT_MOUSE_BUTTON_UP,{SDL_BUTTON_LEFT}});
    count=event_count;tig_ping_timestamp+=300;tig_mouse_ping();
    assert(event_count==count+1 && events[count].data.mouse.event==11);
    captured_release((SDL_Event){SDL_EVENT_MOUSE_BUTTON_UP,{SDL_BUTTON_RIGHT}});
    tig_mouse_set_button(TIG_MOUSE_BUTTON_LEFT,true);
    count=event_count;tig_ping_timestamp+=300;tig_mouse_ping();
    assert(event_count==count+1 && events[count].data.mouse.repeat);
    tig_mouse_set_button(TIG_MOUSE_BUTTON_LEFT,false);
    count=event_count;tig_ping_timestamp+=300;tig_mouse_ping();
    assert(event_count==count);
    puts("PASS: captured release, delayed dialogue release, no leaked action, independent buttons and fresh movement");
}
'''
with tempfile.TemporaryDirectory(prefix="arcanum-dialog-mouse-") as folder:
    source = Path(folder) / "test.c"
    binary = Path(folder) / "test"
    source.write_text(code)
    subprocess.run([os.environ.get("CC", "cc"), "-std=c17", "-Wall", "-Wextra", "-Werror",
                    str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
