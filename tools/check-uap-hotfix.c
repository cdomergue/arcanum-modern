/* Inspect data through the real engine, only inside a marked private copy. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <tig/tig.h>
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/name.h"
#include "game/proto.h"
#include "game/obj.h"
#include "game/object.h"
#include "game/mes.h"
#include "game/townmap.h"
#include "game/area.h"
#include "game/location.h"
#include "ui/iso.h"
#include "ui/modern_ui.h"
#include "ui/gameuilib.h"
#include "ui/intgame.h"
int main(int argc,char** argv)
{
    if(argc<3 || chdir(argv[1]) || access(".hotfix-sandbox",R_OK))return 2;
    setvbuf(stdout,NULL,_IOLBF,0);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER,"software",SDL_HINT_OVERRIDE);
    TigInitInfo init={0};init.width=1280;init.height=720;init.bpp=32;
    init.texture_width=init.texture_height=2048;init.flags=TIG_INITIALIZE_WINDOWED|TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver=name_resolve_path;init.art_id_reset_func=name_normalize_aid;init.sound_file_path_resolver=gsound_resolve_path;
    assert(tig_init(&init)==TIG_OK);intgame_set_iso_window_width(init.width);intgame_set_iso_window_height(init.height);
    GameInitInfo game={0};game.draw_func=iso_redraw;game.invalidate_rect_func=iso_invalidate_rect;assert(intgame_create_iso_window(&game.iso_window_handle)&&gamelib_init(&game));
    assert(gameuilib_init(&game)&&gamelib_mod_load("Arcanum")&&gameuilib_mod_load());
    printf("Hotfix test process %ld, isolated directory %s\n",(long)getpid(),argv[1]);
    int64_t helm=sub_4685A0(8199),perfume=sub_4685A0(10094);assert(helm && perfume);
    printf("Helm AC=%d DR=%d; perfume spell=%d\n",obj_field_int32_get(helm,OBJ_F_ARMOR_AC_ADJ),
        obj_arrayfield_int32_get(helm,OBJ_F_ARMOR_RESISTANCE_ADJ_IDX,0),obj_field_int32_get(perfume,OBJ_F_ITEM_SPELL_1));
    if(!strcmp(argv[2],"baseline"))return 0;
    assert(obj_field_int32_get(helm,OBJ_F_ARMOR_AC_ADJ)==3);
    assert(obj_arrayfield_int32_get(helm,OBJ_F_ARMOR_RESISTANCE_ADJ_IDX,0)==7);
    assert(obj_field_int32_get(perfume,OBJ_F_ITEM_SPELL_1)==114); /* Elven Perfume */
    assert(area_get_location(17)==location_make(82496,45629));
    assert(area_get_location(19)==location_make(20701,55992));
    TownMapInfo info;assert(townmap_info(28,&info));
    assert(info.width==10240 && info.height==5120);
    FILE* list=fopen(".hotfix-resources","r");assert(list);
    char resource[TIG_MAX_PATH],repository[TIG_MAX_PATH];int count=0;
    while(fgets(resource,sizeof(resource),list)) {
        resource[strcspn(resource,"\r\n")]=0;
        const char* archives[]={"french-module.dat","Arcanum.PATCH8","Arcanum8.dat"};
        TigFile* expected=NULL;
        for(int i=0;i<3 && !expected;i++) {
            if(!strcmp(argv[2],"preinstalled")) {
                const char* installed[]={"Arcanum8.dat","./modules/Arcanum.PATCH8","./Arcanum8.dat"};
                snprintf(repository,sizeof(repository),"%s",installed[i]);
            } else snprintf(repository,sizeof(repository),"%s/%s",argv[2],archives[i]);
            expected=tig_file_fopen_repository(repository,resource);
        }
        TigFile* loaded=tig_file_fopen(resource,"rb");assert(expected && loaded);
        char a[4096],b[4096];size_t n,m;
        do {n=tig_file_fread(a,1,sizeof(a),expected);m=tig_file_fread(b,1,sizeof(b),loaded);assert(n==m && !memcmp(a,b,n));}while(n);
        tig_file_fclose(expected);tig_file_fclose(loaded);count++;
    }
    fclose(list);assert(count==405);
    puts("PASS: all 405 hotfix resources resolve to the intended archive; item stats, travel coordinates and Torin town map");
    if(argc==4) {
        gamelib_splash_clear();assert(modern_ui_init());
        assert(gamelib_load(argv[3]));
        assert(gamelib_save("Slot0092","Hotfix isolated round trip"));
        assert(gamelib_load("Slot0092"));
        puts("PASS: existing campaign loaded, saved and reloaded in private fixture");
    }
    return 0;
}
