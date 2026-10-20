#include "modern_encoding.h"

static void menu_name_input(const char* utf8,bool backspace)
{
    bool saving=mainmenu_ui_window_type_get()==MM_WINDOW_SAVE_GAME;
    ModernCharacter c;
    if(!saving && (!modern_mainmenu_character(&c,description_tab) || !c.editable))return;
    char name[24],encoded[24];
    if(!backspace) {
        to_game(utf8,encoded,sizeof(encoded));
        if(!*encoded)return;
    }
    SDL_strlcpy(name,saving?modern_mainmenu_save_description():c.name,sizeof(name));
    if(name_replace)name[0]=0;
    size_t n=strlen(name);
    if(backspace) {if(n)name[--n]=0;}
    else {
        SDL_strlcat(name,encoded,sizeof(name));
    }
    name_replace=false;
    if(saving)modern_mainmenu_save_description_set(name);else modern_mainmenu_name(name);
}
