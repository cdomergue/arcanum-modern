#!/usr/bin/env python3
"""Exercise the real HUD tick through new-game, dialogue and modal transitions."""
from pathlib import Path
import os
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / "source/src/ui/modern_ui.c").read_text()
start = source.index("void modern_ui_tick(void)\n{")
end = source.index("\nvoid modern_ui_journal_open(void)", start)
tick = source[start:end]
engine = (Path(__file__).resolve().parents[1] / "source/src/ui/intgame.c").read_text()
start = engine.index("    /* Hidden command bars", engine.index("void intgame_process_event(TigMessage* msg)\n{"))
end = engine.index("    if (msg->type == TIG_MESSAGE_KEYBOARD", start)
dialog_route = engine[start:end]

harness = r"""
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
enum { INTGAME_MODE_MAIN, INTGAME_MODE_DIALOG, INTGAME_MODE_INVEN, INTGAME_MODE_LOGBOOK, INTGAME_MODE_SPELL, INTGAME_MODE_SKILL, INTGAME_MODE_ITEM };
enum { ROTWIN_TYPE_MSG, ROTWIN_TYPE_SPELLS };
static bool enabled=true, in_game, hud, journal;
static bool menu, compact, classic_hidden;
static int mode=INTGAME_MODE_MAIN, rotating=ROTWIN_TYPE_MSG, pending_action=-1;
static void modern_ui_menu_tick(void) {}
static void world_zoom_sync(void) {}
static bool tactical_active(void) { return false; }
static void tactical_tick(void) {}
static void modern_map_action(int action) { assert(action == 8); }
static void modern_sleep_sync(bool native) { (void)native; }
static void modern_fate_sync(bool native) { (void)native; }
static void modern_bubbles_sync(bool native) { (void)native; }
static void modern_written_sync(bool native) { (void)native; }
static void modern_followers_sync(bool native) { (void)native; }
static void party_tick(void) {}
static void panels_tick(void) {}
static int native_panel(void) { return 0; }
static void logbook_ui_close(void) { journal=false; }
static void modern_intgame_hud_hide(bool hide) { classic_hidden=hide; }
static void activate(int action) { (void)action; }
static bool visible(void) { return enabled && in_game && !menu; }
static bool intgame_is_compact_interface(void) { return compact; }
static int intgame_mode_get(void) { return mode; }
static int iso_interface_window_get(void) { return rotating; }
""" + tick + r"""
typedef struct { bool consumed; } TigMessage;
static bool (*intgame_dialog_process_event_func)(TigMessage*);
static int response_events, world_events;
static bool sub_54B5D0(TigMessage* msg) {
    response_events++;
    return msg->consumed;
}
static void remaining_dialog_event(TigMessage* msg) {
""" + dialog_route + r"""
    world_events++;
}
static void check(bool expected) {
    modern_ui_tick();
    assert(hud==expected);
    assert(classic_hidden==expected);
}
int main(void) {
    /* New game, then the scripted conversation several frames later. */
    check(true);
    mode=INTGAME_MODE_DIALOG;
    for(int frame=0;frame<300;frame++) check(true);
    /* Pause/continue during the conversation must restore only our bars. */
    menu=true;check(false);
    menu=false;check(true);
    classic_hidden=false;check(true);
    /* Explicit F9 remains available, including during a conversation. */
    pending_action=999;check(false);
    check(false);
    pending_action=999;check(true);
    /* Conversation ends; original modal screens retain their own controls. */
    mode=INTGAME_MODE_MAIN;check(true);
    mode=INTGAME_MODE_SPELL;check(true);
    mode=INTGAME_MODE_SKILL;check(true);
    mode=INTGAME_MODE_ITEM;check(true);
    mode=INTGAME_MODE_INVEN;check(false);
    mode=INTGAME_MODE_DIALOG;check(true);
    mode=INTGAME_MODE_MAIN;
    rotating=ROTWIN_TYPE_SPELLS;check(false);
    rotating=ROTWIN_TYPE_MSG;check(true);
    compact=true;check(false);
    compact=false;check(true);
    journal=true;mode=INTGAME_MODE_LOGBOOK;check(true);
    pending_action=999;check(false);
    assert(!journal);
    /* Events left over after window filters must reach conversation handling
       before any movement code when the original command bars are hidden. */
    TigMessage click={true}, escape={false};
    mode=INTGAME_MODE_DIALOG;
    intgame_dialog_process_event_func=sub_54B5D0;
    remaining_dialog_event(&click);
    assert(response_events==1 && world_events==0);
    remaining_dialog_event(&escape);
    assert(response_events==2 && world_events==1);
    intgame_dialog_process_event_func=NULL;
    remaining_dialog_event(&click);
    assert(response_events==2 && world_events==2);
    puts("PASS: delayed dialogue, pause/resume, F9, modal screens and response routing");
}
"""

with tempfile.TemporaryDirectory(prefix="arcanum-hud-test-") as directory:
    c_file = Path(directory) / "hud.c"
    binary = Path(directory) / "hud-test"
    c_file.write_text(harness)
    subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra",
                    "-Werror", str(c_file), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
