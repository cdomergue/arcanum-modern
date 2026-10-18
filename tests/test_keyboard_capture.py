#!/usr/bin/env python3
"""Compile the actual event gate with a small UI/SDL event test double.

Run: python3 tests/test_keyboard_capture.py (requires a C compiler).
The live-game check additionally covers a held Escape across several frames.
"""
from pathlib import Path
import os
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / "source/src/ui/modern_ui.c").read_text()
start = source.index("static bool handle_event(SDL_Event* event)\n{")
end = source.index("\n}", start) + 2
gate = source[start:end]

harness = r"""
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
typedef int SDL_Scancode;
enum { SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_J, SDL_SCANCODE_COUNT };
enum { SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_UP, SDL_EVENT_MOUSE_MOTION };
typedef struct { int type; struct { SDL_Scancode scancode; bool repeat; } key; } SDL_Event;
static bool ui_visible;
static int dispatches;
static bool handle_ui_event(SDL_Event* event) {
    (void)event;
    dispatches++;
    return ui_visible;
}
""" + gate + r"""
int main(void) {
    SDL_Event down = {.type=SDL_EVENT_KEY_DOWN, .key={SDL_SCANCODE_ESCAPE, false}};
    SDL_Event up = {.type=SDL_EVENT_KEY_UP, .key={SDL_SCANCODE_ESCAPE, false}};
    SDL_Event repeat = down;
    repeat.key.repeat = true;
    ui_visible = true;
    assert(handle_event(&down));
    assert(dispatches == 1);
    /* The menu closes between physical key down and key up. */
    ui_visible = false;
    assert(handle_event(&repeat));
    assert(handle_event(&up));
    assert(dispatches == 1);
    /* A genuinely new press must reach the game again. */
    assert(!handle_event(&down));
    assert(!handle_event(&up));
    assert(dispatches == 3);
    /* Capture remains independent for simultaneous keys. */
    ui_visible = true;
    assert(handle_event(&down));
    down.key.scancode = SDL_SCANCODE_J;
    assert(handle_event(&down));
    ui_visible = false;
    assert(handle_event(&up));
    up.key.scancode = SDL_SCANCODE_J;
    assert(handle_event(&up));
    assert(!handle_event(&down));
    assert(!handle_event(&up));
    puts("PASS: delayed release, held key, next press and independent key capture");
}
"""

with tempfile.TemporaryDirectory(prefix="arcanum-key-test-") as directory:
    code = Path(directory) / "test.c"
    binary = Path(directory) / "test"
    code.write_text(harness)
    subprocess.run([os.environ.get("CC", "cc"), "-std=c17", "-Wall", "-Wextra",
                    "-Werror", str(code), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
