#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <tig/tig.h>
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/light.h"
#include "game/location.h"
#include "game/map.h"
#include "game/name.h"
#include "game/obj.h"
#include "game/object.h"
#include "game/roof.h"
#include "game/sector.h"
#include "game/tile.h"
#include "game/townmap.h"
#include "ui/intgame.h"

#define TILE_SIZE 1024

void sub_43AAB0(void);

static void write_u32(FILE* f, uint32_t n)
{
    unsigned char b[4] = { n >> 24, n >> 16, n >> 8, n };
    fwrite(b, 1, 4, f);
}

static void png_chunk(FILE* f, const char* type, const unsigned char* data, size_t size)
{
    write_u32(f, size);
    fwrite(type, 1, 4, f);
    if (size) fwrite(data, 1, size, f);
    uLong crc = crc32(0, (const Bytef*)type, 4);
    if (size) crc = crc32(crc, data, size);
    write_u32(f, crc);
}

static bool save_png(TigVideoBuffer* buffer, const char* path, int width, int height, int step)
{
    TigVideoBufferData data;
    if (tig_video_buffer_lock(buffer) != TIG_OK) return false;
    tig_video_buffer_data(buffer, &data);
    int original_width = width, original_height = height;
    width = (width + step - 1) / step;
    height = (height + step - 1) / step;
    size_t stride = width * 3 + 1;
    size_t size = stride * height;
    unsigned char* pixels = malloc(size);
    for (int y = 0; y < height; y++) {
        unsigned char* row = pixels + y * stride;
        int previous[3] = {0};
        row[0] = 1;
        for (int x = 0; x < width; x++) {
            int channels[3] = {0}, count = 0;
            for (int dy = 0; dy < step && y * step + dy < original_height; dy++) {
                uint32_t* src = (uint32_t*)((char*)data.pixels + (y * step + dy) * data.pitch);
                for (int dx = 0; dx < step && x * step + dx < original_width; dx++) {
                    uint32_t pixel = src[x * step + dx];
                    channels[0] += tig_color_get_red(pixel);
                    channels[1] += tig_color_get_green(pixel);
                    channels[2] += tig_color_get_blue(pixel);
                    count++;
                }
            }
            for (int c = 0; c < 3; c++) {
                int value = (channels[c] + count / 2) / count;
                row[x * 3 + c + 1] = value - previous[c];
                previous[c] = value;
            }
        }
    }
    tig_video_buffer_unlock(buffer);
    uLongf packed_size = compressBound(size);
    unsigned char* packed = malloc(packed_size);
    int result = compress2(packed, &packed_size, pixels, size, 3);
    free(pixels);
    if (result != Z_OK) { free(packed); return false; }
    char temp[4096];
    snprintf(temp, sizeof(temp), "%s.tmp", path);
    FILE* f = fopen(temp, "wb");
    if (!f) { free(packed); return false; }
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    unsigned char header[13] = {width >> 24, width >> 16, width >> 8, width,
        height >> 24, height >> 16, height >> 8, height, 8, 2, 0, 0, 0};
    png_chunk(f, "IHDR", header, sizeof(header));
    png_chunk(f, "IDAT", packed, packed_size);
    png_chunk(f, "IEND", NULL, 0);
    free(packed);
    bool ok = !ferror(f);
    if (fclose(f)) ok = false;
    return ok && rename(temp, path) == 0;
}

static bool render(tig_window_handle_t window, int64_t center, int width, int height, int x, int y)
{
    int64_t sx, sy;
    location_xy(center, &sx, &sy);
    location_origin_scroll(width / 2 - x - sx, height / 2 - y - sy);
    TigRect rect = {0, 0, TILE_SIZE, TILE_SIZE};
    TigRect extended = {-256, -256, TILE_SIZE + 512, TILE_SIZE + 512};
    TigRectListNode node = {rect, NULL};
    TigRectListNode* rects = &node;
    LocRect loc_rect;
    SectorRect sector_rect;
    tig_window_fill(window, &rect, tig_color_make(0, 0, 0));
    if (!location_screen_rect_to_loc_rect(&extended, &loc_rect)) return true;
    if (!sector_rect_from_loc_rect(&loc_rect, &sector_rect)) return false;
    SectorListNode* sectors = sector_list_create(&loc_rect);
    GameDrawInfo info = {&extended, &loc_rect, &sector_rect, sectors, &rects};
    if (tig_video_3d_begin_scene() != TIG_OK) return false;
    int64_t lighting_center;
    // The lighting renderer assumes even its expanded sampling corners are inside the map.
    if (location_at(-160, -80, &lighting_center)
        && location_at(TILE_SIZE + 160, -80, &lighting_center)
        && location_at(-160, TILE_SIZE + 80, &lighting_center)
        && location_at(TILE_SIZE + 160, TILE_SIZE + 80, &lighting_center)) light_draw(&info);
    tile_draw(&info);
    object_draw(&info);
    roof_draw(&info);
    tig_video_3d_end_scene();
    sector_list_destroy(sectors);
    return true;
}

int main(int argc, char** argv)
{
    if (argc < 5 || chdir(argv[1]) || access(".map-export-sandbox", R_OK)) {
        fprintf(stderr, "Usage: export_maps ISOLATED_ROOT MODULE JOBS_TSV OUTPUT [tile-limit]\n");
        return 2;
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "software", SDL_HINT_OVERRIDE);
    highres_config_load();
    TigInitInfo init = {0};
    init.width = init.height = TILE_SIZE;
    init.bpp = 32;
    init.texture_width = init.texture_height = TILE_SIZE;
    init.flags = TIG_INITIALIZE_WINDOWED | TIG_INITIALIZE_NO_SOUND | TIG_INITIALIZE_SET_WINDOW_NAME;
    init.window_name = "Arcanum map exporter";
    init.art_file_path_resolver = name_resolve_path;
    init.art_id_reset_func = name_normalize_aid;
    init.sound_file_path_resolver = gsound_resolve_path;
    if (tig_init(&init) != TIG_OK) { fprintf(stderr, "tig_init failed: %s\n", SDL_GetError()); return 3; }
    intgame_set_iso_window_width(TILE_SIZE);
    intgame_set_iso_window_height(TILE_SIZE);
    GameInitInfo game = {0};
    game.editor = true;
    if (!intgame_create_iso_window(&game.iso_window_handle) || !gamelib_init(&game)) return 4;
    if (!gamelib_mod_load(argv[2])) return 5;
    roof_blit_flags_set(0);
    sub_43AAB0();
    FILE* jobs = fopen(argv[3], "r");
    if (!jobs) return 6;
    char line[1024], previous[512] = "";
    int count = 0, limit = argc > 5 ? atoi(argv[5]) : 0;
    while (fgets(line, sizeof(line), jobs)) {
        char* tab = strchr(line, '\t');
        if (!tab) return 7;
        *tab++ = '\0';
        tab[strcspn(tab, "\r\n")] = '\0';
        int id = atoi(line);
        TownMapInfo tmi;
        if (!townmap_info(id, &tmi)) return 8;
        if (strcmp(previous, tab)) {
            char base[512];
            snprintf(base, sizeof(base), "maps\\%s", tab);
            printf("OPEN %s\n", tab);
            if (!map_open(base, "export-state", false)) return 9;
            for (int type = OBJ_TYPE_PROJECTILE; type < OBJ_TYPE_COUNT; type++) {
                if (object_type_is_enabled(type)) object_type_toggle(type);
            }
            strcpy(previous, tab);
            light_set_colors(tig_color_make(255, 255, 255), tig_color_make(255, 255, 255));
        }
        char folder[4096];
        snprintf(folder, sizeof(folder), "%s/%d", argv[4], id);
        mkdir(folder, 0755);
        printf("MAP %d %s %dx%d\n", id, townmap_name(id), tmi.width, tmi.height);
        TigVideoBuffer* buffer;
        tig_window_vbid_get(game.iso_window_handle, &buffer);
        for (int y = 0; y < tmi.height; y += TILE_SIZE) {
            for (int x = 0; x < tmi.width; x += TILE_SIZE) {
                char path[4096];
                snprintf(path, sizeof(path), "%s/0_%d_%d.png", folder, x / TILE_SIZE, y / TILE_SIZE);
                char last_mip[4096];
                snprintf(last_mip, sizeof(last_mip), "%s/3_%d_%d.png", folder, x / TILE_SIZE, y / TILE_SIZE);
                if (!access(path, R_OK) && !access(last_mip, R_OK)) continue;
                if (!render(game.iso_window_handle, tmi.loc, tmi.width, tmi.height, x, y)) return 10;
                int w = tmi.width - x < TILE_SIZE ? tmi.width - x : TILE_SIZE;
                int h = tmi.height - y < TILE_SIZE ? tmi.height - y : TILE_SIZE;
                for (int level = 0; level < 4; level++) {
                    snprintf(path, sizeof(path), "%s/%d_%d_%d.png", folder, level, x / TILE_SIZE, y / TILE_SIZE);
                    if (!save_png(buffer, path, w, h, 1 << level)) return 11;
                }
                if (++count % 25 == 0) printf("TILES %d\n", count);
                if (limit && count >= limit) return 0;
            }
        }
        printf("DONE %d\n", id);
        char complete[4096];
        snprintf(complete, sizeof(complete), "%s/complete", folder);
        FILE* marker = fopen(complete, "w");
        if (!marker) return 12;
        fprintf(marker, "1 %d %d 1024 4\n", tmi.width, tmi.height);
        fclose(marker);
    }
    fclose(jobs);
    gamelib_exit();
    tig_exit();
    return 0;
}
