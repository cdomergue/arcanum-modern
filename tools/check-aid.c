/* Live engine/rendering check. Run only on an isolated, marked game copy.
 * Link in place of main.c and modern_ui.c, as with check-inventory-transfer.c. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <tig/tig.h>
#include "game/descriptions.h"
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/name.h"
#include "game/proto.h"
#include "ui/gameuilib.h"
#include "../source/src/ui/modern_ui.c"

void sub_549A70(void);

static bool detail_has(ModernDetails* detail,const char* text)
{
    for(int i=0;i<detail->count;i++)if(strstr(detail->lines[i],text))return true;
    return false;
}

int main(int argc,char** argv)
{
    if(argc!=3 || chdir(argv[1]) || access(".map-export-sandbox",R_OK))return 2;
    setvbuf(stdout,NULL,_IOLBF,0);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER,"software",SDL_HINT_OVERRIDE);
    highres_config_load();
    TigInitInfo init={0};init.width=1360;init.height=900;init.bpp=32;
    init.texture_width=init.texture_height=2048;
    init.flags=TIG_INITIALIZE_WINDOWED|TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver=name_resolve_path;init.art_id_reset_func=name_normalize_aid;
    init.sound_file_path_resolver=gsound_resolve_path;
    assert(tig_init(&init)==TIG_OK);
    intgame_set_iso_window_width(init.width);intgame_set_iso_window_height(init.height);
    GameInitInfo game={0};
    assert(intgame_create_iso_window(&game.iso_window_handle) && gamelib_init(&game));
    sub_549A70();
    assert(gameuilib_init(&game) && gamelib_mod_load("Arcanum") && gameuilib_mod_load());
    gamelib_splash_clear();assert(modern_ui_init());
    assert(!strcmp(modern_intgame_text(0),"Niveau"));
    size_t longest=0;int longest_id=0;
    for(size_t i=0;i<SDL_arraysize(modern_aid_entries);i++) {
        const ModernAidEntry* entry=&modern_aid_entries[i];
        for(int lang=0;lang<2;lang++) {
            char expanded[8192],encoded[2000],roundtrip[8192];
            assert(aid_expand(lang?entry->fr:entry->en,expanded,sizeof(expanded)));
            to_game(expanded,encoded,sizeof(encoded));from_game(encoded,roundtrip,sizeof(roundtrip));
            assert(!strcmp(expanded,roundtrip));
            if(strlen(encoded)>longest){longest=strlen(encoded);longest_id=entry->id;}
        }
    }
    printf("PASS: 854 descriptions in both languages with real French MES labels; longest %d: %zu bytes\n",longest_id,longest);

    int64_t pc=player_get_local_pc_obj(),dagger,magic,ammo,unique;
    assert(pc && object_create(sub_4685A0(BP_DAGGER),obj_field_int64_get(pc,OBJ_F_LOCATION),&dagger));
    assert(object_create(sub_4685A0(BP_ENCHANTED_SWORD),obj_field_int64_get(pc,OBJ_F_LOCATION),&magic));
    assert(object_create(sub_4685A0(7040),obj_field_int64_get(pc,OBJ_F_LOCATION),&ammo));
    ModernDetails detail;char expected[2000];
    assert(modern_ui_item_description(dagger,expected,sizeof(expected)));
    assert(strstr(expected,modern_intgame_text(28)) && strstr(expected,"3/3"));
    int effect=obj_field_int32_get(dagger,OBJ_F_ITEM_DESCRIPTION_EFFECTS);
    modern_inventory_details(pc,dagger,0,&detail);assert(detail_has(&detail,expected));
    assert(obj_field_int32_get(dagger,OBJ_F_ITEM_DESCRIPTION_EFFECTS)==effect);
    obj_field_int32_set(magic,OBJ_F_ITEM_FLAGS,obj_field_int32_get(magic,OBJ_F_ITEM_FLAGS)&~OIF_IDENTIFIED);
    strcpy(expected,"fallback");assert(!modern_ui_item_description(magic,expected,sizeof(expected)));
    assert(!strcmp(expected,"fallback"));
    modern_inventory_details(pc,magic,0,&detail);assert(!detail_has(&detail,"Complexit"));
    obj_field_int32_set(magic,OBJ_F_ITEM_FLAGS,obj_field_int32_get(magic,OBJ_F_ITEM_FLAGS)|OIF_IDENTIFIED);
    modern_inventory_details(pc,magic,0,&detail);assert(detail_has(&detail,"Complexit"));
    modern_inventory_details(pc,ammo,0,&detail);
    assert(detail_has(&detail,modern_intgame_text(6)) && detail_has(&detail,"Fabrication"));
    int64_t bandages;
    assert(object_create(sub_4685A0(15066),obj_field_int64_get(pc,OBJ_F_LOCATION),&bandages));
    obj_field_int32_set(bandages,OBJ_F_GENERIC_FLAGS,OGF_IS_HEALING_ITEM);
    obj_field_int32_set(bandages,OBJ_F_GENERIC_USAGE_BONUS,17);
    modern_inventory_details(pc,bandages,0,&detail);
    assert(detail_has(&detail,modern_intgame_text(72)) && detail_has(&detail,"+17%"));
    assert(detail_has(&detail,"Fabrication"));
    for(size_t i=0;i<SDL_arraysize(modern_aid_variants);i++) {
        const ModernAidVariant* variant=&modern_aid_variants[i];ObjectID oid;
        assert(objid_id_from_str(&oid,variant->guid));
        assert(object_create_ex(sub_4685A0(BP_DAGGER),obj_field_int64_get(pc,OBJ_F_LOCATION),oid,&unique));
        obj_field_int32_set(unique,OBJ_F_DESCRIPTION,variant->description);
        obj_field_int32_set(unique,OBJ_F_ITEM_DESCRIPTION_EFFECTS,6029);
        assert(aid_item_entry(unique)->id==variant->effect);
        assert(modern_ui_item_description(unique,expected,sizeof(expected)));
        assert(obj_field_int32_get(unique,OBJ_F_ITEM_DESCRIPTION_EFFECTS)==6029);
        obj_deallocate(unique);
    }
    enabled=false;assert(!modern_ui_item_description(dagger,expected,sizeof(expected)));enabled=true;
    printf("PASS: identification, ammunition quantities, 201 persistent GUIDs and unchanged object fields\n");

    /* Render the normal scrollable item card with an accented name and one of
     * the longest cursed-item descriptions, at both ends of its scroll range. */
    obj_field_int32_set(dagger,OBJ_F_DESCRIPTION,30739);
    obj_field_int32_set(dagger,OBJ_F_ITEM_DESCRIPTION_EFFECTS,6030);
    modern_inventory_details(pc,dagger,0,&detail);
    assert(detail_has(&detail,"al\xe9" "atoire"));
    canvas_w=1360;canvas_h=900;
    SDL_Texture* target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_TARGET,1360,900);
    assert(target && SDL_SetRenderTarget(renderer,target));
    SDL_SetRenderLogicalPresentation(renderer,1360,900,SDL_LOGICAL_PRESENTATION_DISABLED);
    SDL_SetRenderDrawColor(renderer,24,19,14,255);SDL_RenderClear(renderer);
    button_count=0;
    int maximum=inventory_detail_card((SDL_FRect){30,50,390,300},&detail,0,2490);
    assert(maximum>0);
    inventory_detail_card((SDL_FRect){460,50,390,300},&detail,maximum,2492);
    modern_inventory_details(pc,ammo,0,&detail);
    inventory_detail_card((SDL_FRect){890,50,440,300},&detail,0,2494);
    SDL_Surface* surface=SDL_RenderReadPixels(renderer,NULL);
    assert(surface && SDL_SavePNG(surface,argv[2]));SDL_DestroySurface(surface);
    printf("PASS: accented names, wrapped descriptions, scroll range %d, capture %s\n",maximum,argv[2]);
    return 0;
}
