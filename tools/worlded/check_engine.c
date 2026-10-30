/* Independent integration check: CE editor loading for sector roundtrips,
 * gameplay loading for new buildings. Requires a disposable marked installation. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <tig/tig.h>
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/map.h"
#include "game/name.h"
#include "game/sector.h"
#include "game/obj.h"
#include "game/portal.h"
#include "game/object.h"
#include "ui/intgame.h"

#define REQUIRE(condition) do { if (!(condition)) { fprintf(stderr, "Failed: %s\n", #condition); return 1; } } while (0)
static const char* art_path;
static int resolve_test_art(tig_art_id_t aid, char* path, size_t size)
{
    (void)aid;
    return snprintf(path, size, "%s", art_path) < (int)size ? TIG_OK : TIG_ERR_GENERIC;
}

int main(int argc, char** argv)
{
    if ((argc != 7 && argc != 8) || chdir(argv[1]) || access(".worlded-engine-check", R_OK)) {
        fprintf(stderr, "Usage: check-worlded-engine ISOLATED_ROOT MAP SECTOR_ID EXPECTED_TILES OBJECT_COUNT TOWNMAP\n");
        return 2;
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "software", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    highres_config_load();
    TigInitInfo init = {0};
    init.width = 800; init.height = 600; init.bpp = 32;
    init.texture_width = init.texture_height = 2048;
    init.flags = TIG_INITIALIZE_WINDOWED | TIG_INITIALIZE_NO_SOUND;
    bool art_test = argc == 8 && !strcmp(argv[2], "--art");
    if (art_test) art_path = argv[3];
    init.art_file_path_resolver = art_test ? resolve_test_art : name_resolve_path;
    init.art_id_reset_func = art_test ? NULL : name_normalize_aid;
    init.sound_file_path_resolver = gsound_resolve_path;
    REQUIRE(tig_init(&init) == TIG_OK);
    if (art_test) {
        TigFileList archives;
        tig_file_list_create(&archives, "arcanum*.dat");
        for (unsigned i=0; i<archives.count; ++i) REQUIRE(tig_file_repository_add(archives.entries[i].path));
        tig_file_list_destroy(&archives);
        tig_art_id_t aid = 0x40000000u | ((unsigned)atoi(argv[5]) << 11) | ((unsigned)atoi(argv[6]) << 14) | ((unsigned)atoi(argv[7]) << 4);
        TigArtFrameData frame;
        REQUIRE(tig_art_frame_data(aid, &frame) == TIG_OK);
        FILE* expected = fopen(argv[4], "rb"); REQUIRE(expected != NULL);
        int values[4]; REQUIRE(fread(values, sizeof(int), 4, expected) == 4);
        REQUIRE(frame.width == values[0] && frame.height == values[1] && frame.hot_x == values[2] && frame.hot_y == values[3]);
        for (int y=0;y<frame.height;++y) for (int x=0;x<frame.width;++x) {
            unsigned char rgba[4]; unsigned color;
            REQUIRE(fread(rgba, 1, 4, expected) == 4);
            REQUIRE(sub_502E50(aid, x, y, &color) == TIG_OK);
            REQUIRE(tig_color_get_red(color) == rgba[0] && tig_color_get_green(color) == rgba[1] && tig_color_get_blue(color) == rgba[2]);
        }
        REQUIRE(fgetc(expected) == EOF); fclose(expected);
        printf("PASS: CE RGB pixels and hotspots match, %dx%d, rotation %s, frame %s, palette %s\n", frame.width, frame.height, argv[5], argv[6], argv[7]);
        tig_exit(); return 0;
    }
    intgame_set_iso_window_width(init.width);
    intgame_set_iso_window_height(init.height);
    GameInitInfo game = {0};
    // Building checks use the gameplay loader: containers are mobile objects.
    game.editor = argc == 7;
    REQUIRE(intgame_create_iso_window(&game.iso_window_handle));
    REQUIRE(gamelib_init(&game));
    REQUIRE(gamelib_mod_load("Arcanum"));
    REQUIRE(map_preprocess_mobile(argv[2]));
    char path[260];
    REQUIRE(snprintf(path, sizeof(path), "maps\\%s", argv[2]) < (int)sizeof(path));
    REQUIRE(map_open(path, "worlded-check-state", false));
    const int64_t id = strtoll(argv[3], NULL, 10);
    Sector* sector;
    REQUIRE(sector_lock(id, &sector));
    FILE* expected = fopen(argv[4], "rb");
    REQUIRE(expected != NULL);
    for (int i = 0; i < 4096; ++i) {
        unsigned char b[4];
        REQUIRE(fread(b, 1, 4, expected) == 4);
        uint32_t art = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
        REQUIRE(sector->tiles.art_ids[i] == art);
    }
    REQUIRE(fgetc(expected) == EOF);
    fclose(expected);
    REQUIRE(sector->objects.next_temp_id == atoi(argv[5]));
    REQUIRE(sector->townmap_info == atoi(argv[6]));
    if (argc == 8) {
        FILE* objects = fopen(argv[7], "rb"); REQUIRE(objects != NULL);
        uint32_t expected_count; REQUIRE(fread(&expected_count,4,1,objects)==1);
        for (unsigned n=0;n<expected_count;++n) {
            unsigned char guid[16]; uint32_t fields[9];
            REQUIRE(fread(guid,1,16,objects)==16);
            REQUIRE(fread(fields,4,9,objects)==9);
            int64_t found=OBJ_HANDLE_NULL;
            for (int t=0;t<4096;++t) for (ObjectNode* node=sector->objects.heads[t];node;node=node->next) {
                ObjectID id=obj_get_id(node->obj);
                if (id.type==OID_TYPE_GUID && memcmp(&id.d.g,guid,16)==0) { if (found!=OBJ_HANDLE_NULL) fprintf(stderr,"Duplicate GUID record %d tile %d handles %lld %lld\n",n,t,(long long)found,(long long)node->obj); REQUIRE(found==OBJ_HANDLE_NULL); found=node->obj; }
            }
            REQUIRE(found!=OBJ_HANDLE_NULL);
            ObjectID prototype=obj_get_id(obj_field_handle_get(found,OBJ_F_PROTOTYPE_HANDLE));
            REQUIRE(prototype.type==OID_TYPE_A && (uint32_t)prototype.d.a==fields[0]);
            REQUIRE((uint32_t)obj_field_int32_get(found,OBJ_F_TYPE)==fields[1]);
            REQUIRE((uint64_t)obj_field_int64_get(found,OBJ_F_LOCATION)==((uint64_t)fields[2]|((uint64_t)fields[3]<<32)));
            REQUIRE((uint32_t)obj_field_int32_get(found,OBJ_F_CURRENT_AID)==fields[4]);
            REQUIRE((uint32_t)obj_field_int32_get(found,OBJ_F_FLAGS)==fields[5]);
            TigArtFrameData art; REQUIRE(tig_art_frame_data(fields[4],&art)==TIG_OK);
            REQUIRE(obj_arrayfield_length_get(found,OBJ_F_SCRIPTS_IDX)==0);
            if (fields[1]==OBJ_TYPE_PORTAL || fields[1]==OBJ_TYPE_CONTAINER) {
                REQUIRE((uint32_t)obj_field_int32_get(found,fields[1]==OBJ_TYPE_PORTAL ? OBJ_F_PORTAL_FLAGS : OBJ_F_CONTAINER_FLAGS)==fields[6]);
                REQUIRE(!object_locked_get(found));
            }
            if (fields[1]==OBJ_TYPE_CONTAINER) {
                REQUIRE((uint32_t)obj_field_int32_get(found,OBJ_F_CONTAINER_INVENTORY_NUM)==fields[7]);
                REQUIRE((uint32_t)obj_field_int32_get(found,OBJ_F_CONTAINER_INVENTORY_SOURCE)==fields[8]);
            }
            if (fields[1]==OBJ_TYPE_PORTAL) {
                REQUIRE(!portal_is_open(found));
                for (int f=0;f<10;++f) portal_open(found);
                REQUIRE(portal_is_open(found));
                for (int f=0;f<10;++f) portal_close(found);
                REQUIRE(!portal_is_open(found));
            }
        }
        REQUIRE(fgetc(objects)==EOF); fclose(objects);
        printf("PASS: CE object GUIDs, prototypes, positions, art, flags, empty inventories and door open/close\n");
    }
    printf("PASS: CE loaded sector %lld, 4096 expected tiles, %d objects, townmap %d\n",
        (long long)id, sector->objects.next_temp_id, sector->townmap_info);
    REQUIRE(sector_unlock(id));
    gamelib_exit();
    tig_exit();
    return 0;
}
