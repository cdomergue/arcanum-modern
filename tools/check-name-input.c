#undef NDEBUG
#include <assert.h>
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

enum { MM_WINDOW_NEW_CHAR, MM_WINDOW_SAVE_GAME };
typedef struct { bool editable; const char* name; } ModernCharacter;
static int menu_type, description_tab;
static bool name_replace, editable=true;
static char character_name[24]="Adraenor", save_name[24]="Save";
static int mainmenu_ui_window_type_get(void){return menu_type;}
static bool modern_mainmenu_character(ModernCharacter* c,int tab)
{(void)tab;*c=(ModernCharacter){editable,character_name};return true;}
static const char* modern_mainmenu_save_description(void){return save_name;}
static void modern_mainmenu_name(const char* name){SDL_strlcpy(character_name,name,sizeof(character_name));}
static void modern_mainmenu_save_description_set(const char* name){SDL_strlcpy(save_name,name,sizeof(save_name));}

/* Text entry must work without an optional system encoding converter. */
#define SDL_iconv_string(...) NULL
#include "../source/src/ui/modern_name_input.inc"
#undef SDL_iconv_string

int main(void)
{
    name_replace=true;
    menu_name_input("Alice",false);
    assert(strcmp(character_name,"Alice")==0);
    menu_name_input("a",false);
    assert(strcmp(character_name,"Alicea")==0);
    menu_name_input("",true);
    assert(strcmp(character_name,"Alice")==0);
    name_replace=true;
    menu_name_input("Éléonore",false);
    assert(strcmp(character_name,"\xc9l\xe9onore")==0);
    editable=false;
    menu_name_input("x",false);
    assert(strcmp(character_name,"\xc9l\xe9onore")==0);
    editable=true;
    menu_type=MM_WINDOW_SAVE_GAME;name_replace=true;
    menu_name_input("Château d’Œdipe",false);
    assert(strcmp(save_name,"Ch\xe2teau d\x92\x8c""dipe")==0);
    assert(strcmp(character_name,"\xc9l\xe9onore")==0);
    name_replace=true;
    menu_name_input("1234567890123456789012éà",false);
    assert(strlen(save_name)==23 && (unsigned char)save_name[22]==0xe9);
    menu_name_input("",true);assert(strlen(save_name)==22);
    name_replace=true;menu_name_input("",true);assert(!*save_name);
    menu_name_input("Keep",false);name_replace=true;
    menu_name_input("\xf0\x9f\x98\x80\t\n\x7f",false);
    assert(strcmp(save_name,"Keep")==0 && name_replace);
    menu_name_input("Zoë",false);assert(strcmp(save_name,"Zo\xeb")==0);
    menu_name_input("😀a",false);assert(strcmp(save_name,"Zo\xeb""a")==0);
    char encoded[8];
    to_game("œ€Ÿ",encoded,sizeof(encoded));assert(strcmp(encoded,"\x9c\x80\x9f")==0);
    to_game("\xff\xe2\x82",encoded,sizeof(encoded));assert(!*encoded);
    puts("PASS: character/save names, ASCII, accents, CP1252 punctuation, replacement, deletion and 23-byte limit without iconv");
    return 0;
}
