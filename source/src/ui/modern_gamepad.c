#include "ui/modern_gamepad.h"

#include <math.h>
#include <float.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

void modern_pad_log(const char* event, int context, int action)
{
    int saved_errno=errno;
    FILE* log=fopen("ArcanumModern/gamepad.log","ab+");
    if(log) {
        fseek(log,0,SEEK_END);
        if(ftell(log)>=1024*1024) {
            fclose(log);log=NULL;
            if(remove("ArcanumModern/gamepad.previous.log")==0 || errno==ENOENT)
                if(rename("ArcanumModern/gamepad.log","ArcanumModern/gamepad.previous.log")==0)
                    log=fopen("ArcanumModern/gamepad.log","ab");
        }
        if(log){fprintf(log,"ticks_ms=%llu event=%s context=%d action=%d\n",(unsigned long long)SDL_GetTicks(),event,context,action);fclose(log);}
    }
    errno=saved_errno;
}

ModernPadVector modern_pad_vector(Sint16 x, Sint16 y, int deadzone)
{
    float fx=x/32768.0f, fy=y/32768.0f;
    float length=sqrtf(fx*fx+fy*fy);
    float dead=SDL_clamp(deadzone,5,40)/100.0f;
    if(length<=dead)return (ModernPadVector){0};
    float scaled=(fminf(length,1)-dead)/(1-dead);
    return (ModernPadVector){fx/length*scaled,fy/length*scaled,scaled};
}

int modern_pad_sector(ModernPadVector vector, int count)
{
    if(vector.length<0.35f || count<=0)return -1;
    float angle=atan2f(vector.x,-vector.y);
    if(angle<0)angle+=6.28318530718f;
    return (int)floorf(angle*count/6.28318530718f+0.5f)%count;
}

/* Spatial focus preserves rows and columns, including differently sized cells. */
int modern_pad_focus(const SDL_FRect* rects, int count, int current, int x, int y)
{
    if(!count)return -1;
    if(current<0 || current>=count)return 0;
    SDL_FRect from=rects[current];
    float cx=from.x+from.w/2, cy=from.y+from.h/2, best=FLT_MAX;
    int selected=current;
    for(int i=0;i<count;i++) {
        if(i==current)continue;
        SDL_FRect to=rects[i];
        float dx=to.x+to.w/2-cx, dy=to.y+to.h/2-cy;
        float forward=dx*x+dy*y;
        if(forward<=1)continue;
        float side=fabsf(dx*y-dy*x);
        bool aligned=x ? to.y<from.y+from.h && to.y+to.h>from.y
                       : to.x<from.x+from.w && to.x+to.w>from.x;
        float score=forward+side*3+(aligned?0:10000);
        if(score<best){best=score;selected=i;}
    }
    return selected;
}

static void pad_open(ModernPadState* pad)
{
    if(pad->device)return;
    int count=0;
    SDL_JoystickID* ids=SDL_GetGamepads(&count);
    for(int i=0;i<count && !pad->device;i++) {
        pad->device=SDL_OpenGamepad(ids[i]);
        if(pad->device)pad->id=ids[i];
    }
    SDL_free(ids);
    modern_pad_barrier(pad);
}

void modern_pad_init(ModernPadState* pad)
{
    memset(pad,0,sizeof(*pad));
    pad->enabled=true;pad->rumble=true;pad->deadzone=18;pad->cursor_speed=100;
    pad->initialized=SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    if(pad->initialized)pad_open(pad);
    else SDL_Log("Gamepad initialization failed: %s",SDL_GetError());
}

void modern_pad_exit(ModernPadState* pad)
{
    if(pad->device){SDL_RumbleGamepad(pad->device,0,0,0);SDL_CloseGamepad(pad->device);}
    if(pad->initialized)SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    memset(pad,0,sizeof(*pad));
}

void modern_pad_clear_edges(ModernPadState* pad)
{
    pad->pressed=pad->released=0;
}

void modern_pad_barrier(ModernPadState* pad)
{
    /* A held control never becomes a fresh press in the next screen. */
    pad->blocked=0;
    if(pad->device)for(int i=0;i<SDL_GAMEPAD_BUTTON_COUNT;i++)
        if(SDL_GetGamepadButton(pad->device,i))pad->blocked|=PAD_BUTTON(i);
    pad->held=0;pad->left=pad->right=(ModernPadVector){0};pad->left_trigger=pad->right_trigger=false;
    pad->neutral=true;
    modern_pad_clear_edges(pad);
}

bool modern_pad_event(ModernPadState* pad, const SDL_Event* event)
{
    if(!pad->initialized)return false;
    if(event->type==SDL_EVENT_GAMEPAD_ADDED){pad_open(pad);return true;}
    if(event->type==SDL_EVENT_GAMEPAD_REMOVED) {
        if(event->gdevice.which==pad->id) {
            SDL_CloseGamepad(pad->device);pad->device=NULL;pad->id=0;
            pad->active=false;modern_pad_barrier(pad);pad_open(pad);
        }
        return true;
    }
    if(event->type==SDL_EVENT_GAMEPAD_REMAPPED) {
        if(event->gdevice.which==pad->id)modern_pad_barrier(pad);
        return true;
    }
    if(event->type==SDL_EVENT_WINDOW_FOCUS_LOST) {
        pad->focused=false;modern_pad_barrier(pad);
        if(pad->device)SDL_RumbleGamepad(pad->device,0,0,0);
    }
    if(event->type==SDL_EVENT_KEY_DOWN || event->type==SDL_EVENT_MOUSE_BUTTON_DOWN
        || (event->type==SDL_EVENT_MOUSE_MOTION && (fabsf(event->motion.xrel)>1 || fabsf(event->motion.yrel)>1)))
        pad->active=false;
    if(event->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN || event->type==SDL_EVENT_GAMEPAD_BUTTON_UP) {
        if(event->gbutton.which==pad->id && event->gbutton.button<SDL_GAMEPAD_BUTTON_COUNT) {
            Uint32 bit=PAD_BUTTON(event->gbutton.button);
            bool down=event->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN;
            if(!down)pad->blocked&=~bit;
            if(pad->enabled && pad->focused && !(pad->blocked&bit)) {
                if(down){pad->held|=bit;pad->pressed|=bit;pad->active=true;}
                else {pad->held&=~bit;pad->released|=bit;}
            }
        }
        return true;
    }
    return event->type>=SDL_EVENT_GAMEPAD_AXIS_MOTION && event->type<=SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED;
}

void modern_pad_sample(ModernPadState* pad, bool focused)
{
    if(!pad->initialized)return;
    /* Movies also poll before the main interface installs its event filter. */
    if(pad->device && !SDL_GamepadConnected(pad->device)) {
        SDL_CloseGamepad(pad->device);pad->device=NULL;pad->id=0;pad->active=false;
        modern_pad_barrier(pad);
    }
    if(!pad->device)pad_open(pad);
    if(focused!=pad->focused){
        pad->focused=focused;modern_pad_barrier(pad);
        if(!focused && pad->device)SDL_RumbleGamepad(pad->device,0,0,0);
    }
    if(!focused || !pad->enabled || !pad->device) {
        modern_pad_barrier(pad);return;
    }
    ModernPadVector left=modern_pad_vector(SDL_GetGamepadAxis(pad->device,SDL_GAMEPAD_AXIS_LEFTX),
        SDL_GetGamepadAxis(pad->device,SDL_GAMEPAD_AXIS_LEFTY),pad->deadzone);
    ModernPadVector right=modern_pad_vector(SDL_GetGamepadAxis(pad->device,SDL_GAMEPAD_AXIS_RIGHTX),
        SDL_GetGamepadAxis(pad->device,SDL_GAMEPAD_AXIS_RIGHTY),pad->deadzone);
    if(pad->neutral) {
        if(!left.length && !right.length && SDL_GetGamepadAxis(pad->device,SDL_GAMEPAD_AXIS_LEFT_TRIGGER)<8192
            && SDL_GetGamepadAxis(pad->device,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)<8192)pad->neutral=false;
        return;
    }
    pad->left=left;pad->right=right;
    int trigger=SDL_GetGamepadAxis(pad->device,SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    pad->left_trigger=trigger>(pad->left_trigger?12288:16384);
    trigger=SDL_GetGamepadAxis(pad->device,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    pad->right_trigger=trigger>(pad->right_trigger?12288:16384);
    if(left.length>0.1f || right.length>0.1f || pad->left_trigger || pad->right_trigger)pad->active=true;
}

bool modern_pad_movie_skip(ModernPadState* pad, bool focused)
{
    modern_pad_sample(pad,focused);
    if(!pad->device || !pad->focused)return false;
    Uint32 buttons=0;
    const SDL_GamepadButton skip[]={SDL_GAMEPAD_BUTTON_SOUTH,SDL_GAMEPAD_BUTTON_EAST,SDL_GAMEPAD_BUTTON_START};
    for(int i=0;i<3;i++)if(SDL_GetGamepadButton(pad->device,skip[i]))buttons|=PAD_BUTTON(skip[i]);
    bool pressed=(buttons&~pad->blocked)!=0;
    pad->blocked=buttons;pad->held=buttons;
    return pressed;
}

void modern_pad_rumble(ModernPadState* pad, float strength)
{
    if(!pad->enabled || !pad->rumble || !pad->focused || !pad->device)return;
    float intensity=SDL_clamp(strength,0.0f,1.0f);
    SDL_RumbleGamepad(pad->device,(Uint16)(16000+26000*intensity),
        (Uint16)(22000+34000*intensity),(Uint32)(90+90*intensity));
}

void modern_pad_menu_pulse(ModernPadState* pad)
{
    if(!pad->enabled || !pad->active || !pad->rumble || !pad->focused || !pad->device)return;
    bool played=SDL_RumbleGamepad(pad->device,3500,5000,35);
    modern_pad_log("menu-pulse",played,35);
}

const char* modern_pad_button_name(ModernPadState* pad, SDL_GamepadButton button)
{
    SDL_GamepadButtonLabel label=pad->device ? SDL_GetGamepadButtonLabel(pad->device,button)
        : SDL_GetGamepadButtonLabelForType(SDL_GAMEPAD_TYPE_XBOXONE,button);
    switch(label) {
    case SDL_GAMEPAD_BUTTON_LABEL_A:return "A";
    case SDL_GAMEPAD_BUTTON_LABEL_B:return "B";
    case SDL_GAMEPAD_BUTTON_LABEL_X:return "X";
    case SDL_GAMEPAD_BUTTON_LABEL_Y:return "Y";
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS:return "Cross";
    case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE:return "Circle";
    case SDL_GAMEPAD_BUTTON_LABEL_SQUARE:return "Square";
    case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE:return "Triangle";
    default:break;
    }
    switch(button) {
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:return "L3";
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK:return "R3";
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:return "LB / L1";
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:return "RB / R1";
    case SDL_GAMEPAD_BUTTON_START:return "Start / Options";
    case SDL_GAMEPAD_BUTTON_BACK:return "Back / View";
    default:return SDL_GetGamepadStringForButton(button);
    }
}
