/* Uses a real SDL virtual device: no keyboard or mouse emulation. */
#undef NDEBUG
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "ui/modern_gamepad.h"

static int vibrations;
static Uint16 last_low,last_high;
static bool SDLCALL rumble(void* data,Uint16 low,Uint16 high)
{
    (void)data;last_low=low;last_high=high;
    if(low || high)vibrations++;
    return true;
}
static void pump(ModernPadState* pad)
{
    SDL_UpdateJoysticks();SDL_Event event;
    while(SDL_PollEvent(&event))modern_pad_event(pad,&event);
}
static SDL_JoystickID attach(void)
{
    SDL_VirtualJoystickDesc desc;SDL_INIT_INTERFACE(&desc);
    desc.vendor_id=0x1209;desc.product_id=0xa11c;
    desc.type=SDL_JOYSTICK_TYPE_GAMEPAD;desc.naxes=SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
    desc.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;
    desc.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;
    desc.name="Renaissance controller regression";desc.Rumble=rumble;
    SDL_JoystickID id=SDL_AttachVirtualJoystick(&desc);assert(id);return id;
}
int main(void)
{
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT,"0x1209/0xa11c");
    ModernPadVector v=modern_pad_vector(1000,-1000,18);assert(v.length==0);
    v=modern_pad_vector(32767,32767,18);assert(fabsf(v.length-1)<0.001f);
    assert(fabsf(v.x-v.y)<0.001f);
    ModernPadVector slow=modern_pad_vector(16000,4000,18),fast=modern_pad_vector(30000,7500,18);
    assert(slow.length<fast.length && fabsf(slow.x/slow.y-4)<0.001f);
    for(int i=0;i<10;i++) {
        float angle=i*6.2831853f/10;
        assert(modern_pad_sector((ModernPadVector){sinf(angle),-cosf(angle),1},10)==i);
    }
    assert(modern_pad_sector((ModernPadVector){0},10)==-1);
    SDL_FRect nodes[]={{0,0,50,30},{60,0,50,30},{0,40,50,30},{60,40,50,30},{200,0,100,70}};
    assert(modern_pad_focus(nodes,5,0,1,0)==1);
    assert(modern_pad_focus(nodes,5,0,0,1)==2);
    assert(modern_pad_focus(nodes,5,3,0,-1)==1);
    assert(modern_pad_focus(nodes,5,0,-1,0)==0);
    assert(modern_pad_focus(nodes,0,-1,0,1)==-1);
    ModernPadState pad;modern_pad_init(&pad);assert(pad.initialized);
    SDL_JoystickID id=attach();pump(&pad);assert(pad.device && pad.id==id);
    SDL_Joystick* stick=SDL_GetGamepadJoystick(pad.device);
    modern_pad_sample(&pad,true);modern_pad_sample(&pad,true);
    assert(SDL_SetJoystickVirtualButton(stick,SDL_GAMEPAD_BUTTON_SOUTH,true));pump(&pad);
    assert(pad.pressed&PAD_BUTTON(SDL_GAMEPAD_BUTTON_SOUTH));assert(pad.active);
    modern_pad_clear_edges(&pad);pump(&pad);assert(!pad.pressed);
    modern_pad_barrier(&pad);pump(&pad);assert(!pad.pressed && !pad.held);
    assert(SDL_SetJoystickVirtualButton(stick,SDL_GAMEPAD_BUTTON_SOUTH,false));pump(&pad);
    assert(!pad.blocked);
    assert(SDL_SetJoystickVirtualButton(stick,SDL_GAMEPAD_BUTTON_SOUTH,true));pump(&pad);
    assert(pad.pressed&PAD_BUTTON(SDL_GAMEPAD_BUTTON_SOUTH));
    modern_pad_sample(&pad,false);assert(!pad.held && !pad.pressed);
    assert(SDL_SetJoystickVirtualAxis(stick,SDL_GAMEPAD_AXIS_LEFTX,25000));pump(&pad);
    modern_pad_sample(&pad,true);assert(!pad.held && !pad.pressed);
    modern_pad_sample(&pad,true);assert(!pad.left.length); /* neutral latch after focus regain */
    assert(SDL_SetJoystickVirtualAxis(stick,SDL_GAMEPAD_AXIS_LEFTX,0));pump(&pad);
    modern_pad_sample(&pad,true);
    assert(SDL_SetJoystickVirtualAxis(stick,SDL_GAMEPAD_AXIS_LEFTX,25000));pump(&pad);
    modern_pad_sample(&pad,true);assert(pad.left.length>0.5f);
    assert(SDL_SetJoystickVirtualAxis(stick,SDL_GAMEPAD_AXIS_LEFT_TRIGGER,32767));pump(&pad);modern_pad_sample(&pad,true);assert(pad.left_trigger);
    assert(SDL_SetJoystickVirtualAxis(stick,SDL_GAMEPAD_AXIS_LEFT_TRIGGER,-32768));pump(&pad);modern_pad_sample(&pad,true);assert(!pad.left_trigger);
    assert(SDL_SetJoystickVirtualAxis(stick,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,32767));pump(&pad);modern_pad_sample(&pad,true);assert(pad.right_trigger);
    modern_pad_barrier(&pad);assert(!pad.right_trigger);
    modern_pad_sample(&pad,true);assert(pad.neutral);
    assert(SDL_SetJoystickVirtualAxis(stick,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,-32768));
    assert(SDL_SetJoystickVirtualAxis(stick,SDL_GAMEPAD_AXIS_LEFTX,0));pump(&pad);modern_pad_sample(&pad,true);assert(!pad.neutral);
    modern_pad_rumble(&pad,0.5f);assert(vibrations==1 && last_low && last_high);
    pad.rumble=false;modern_pad_rumble(&pad,1);assert(vibrations==1);
    pad.rumble=true;modern_pad_sample(&pad,false);modern_pad_rumble(&pad,1);assert(vibrations==1);
    pad.enabled=false;modern_pad_sample(&pad,true);assert(!pad.left.length && !pad.held);
    SDL_JoystickID second=attach();pump(&pad);assert(pad.id==id);
    assert(SDL_DetachVirtualJoystick(id));pump(&pad);
    assert(pad.id==second && !pad.held && !pad.pressed && !pad.left.length);
    assert(SDL_DetachVirtualJoystick(second));pump(&pad);assert(!pad.device && !pad.active);
    modern_pad_exit(&pad);
    puts("PASS: SDL hotplug, single-device ownership, held controls, focus loss, analog dead zone, radial sectors, spatial focus and rumble");
    return 0;
}
