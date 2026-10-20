#include "ui/modern_map_generator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef SDL_PLATFORM_WINDOWS
#include <windows.h>
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#include <sys/file.h>
#include <fcntl.h>
#ifdef SDL_PLATFORM_MACOS
#include <mach-o/dyld.h>
#endif
#endif
#include <tig/tig.h>
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/light.h"
#include "game/location.h"
#include "game/map.h"
#include "game/mes.h"
#include "game/name.h"
#include "game/obj.h"
#include "game/object.h"
#include "game/roof.h"
#include "game/sector.h"
#include "game/tile.h"
#include "game/townmap.h"
#include "ui/intgame.h"

#define MAPGEN_TILE 1024
#define MAPGEN_VERSION 2

typedef struct MapGenTask {
    char module[260], source[260];
    int id;
    TownMapInfo info;
} MapGenTask;

typedef struct MapGenWalk {
    const char* destination;
    Uint64 hash;
    bool copy, archives_only, content;
} MapGenWalk;

static char* mapgen_status_path;
static ModernMapProgress mapgen_progress;
static SDL_IOStream* mapgen_log;
void sub_43AAB0(void);

static void SDLCALL mapgen_log_output(void* userdata, int category, SDL_LogPriority priority, const char* message)
{
    (void)userdata; (void)category; (void)priority;
    if (mapgen_log) {
        SDL_WriteIO(mapgen_log, message, strlen(message));
        SDL_WriteIO(mapgen_log, "\n", 1);
        SDL_FlushIO(mapgen_log);
    }
}

static char* mapgen_join(const char* a, const char* b)
{
    char* result = NULL;
    SDL_asprintf(&result, "%s/%s", a, b);
    return result;
}

static bool mapgen_excluded(const char* name)
{
    return name[0] == '.' || !SDL_strcasecmp(name, "Save") || !SDL_strcasecmp(name, "Saves")
        || !SDL_strcasecmp(name, "TIGCache") || !SDL_strcasecmp(name, "Players")
        || !SDL_strcasecmp(name, "screenshots");
}

static bool mapgen_archive(const char* name)
{
    const char* ext = SDL_strrchr(name, '.');
    if (!ext) return false;
    if (!SDL_strcasecmp(ext, ".dat")) return true;
    if (SDL_strncasecmp(ext, ".patch", 6)) return false;
    ext += 6;
    if (!*ext) return false;
    while (*ext) if (!SDL_isdigit(*ext++)) return false;
    return true;
}

static Uint64 mapgen_hash(const void* data, size_t size)
{
    const unsigned char* bytes = data;
    Uint64 hash = 1469598103934665603ULL;
    for (size_t i = 0; i < size; i++) hash = (hash ^ bytes[i]) * 1099511628211ULL;
    return hash;
}

static bool mapgen_link(const char* source, const char* destination)
{
#ifdef SDL_PLATFORM_WINDOWS
    wchar_t* src = (wchar_t*)SDL_iconv_string("UTF-16LE", "UTF-8", source, strlen(source) + 1);
    wchar_t* dst = (wchar_t*)SDL_iconv_string("UTF-16LE", "UTF-8", destination, strlen(destination) + 1);
    bool ok = src && dst && CreateHardLinkW(dst, src, NULL);
    SDL_free(src); SDL_free(dst);
    return ok;
#else
    return link(source, destination) == 0;
#endif
}

static SDL_EnumerationResult SDLCALL mapgen_walk(void* userdata, const char* directory, const char* name)
{
    MapGenWalk* walk = userdata;
    if (mapgen_excluded(name)) return SDL_ENUM_CONTINUE;
    char* source = mapgen_join(directory, name);
    char* target = walk->destination ? mapgen_join(walk->destination, name) : NULL;
    SDL_PathInfo info;
    bool ok = source && SDL_GetPathInfo(source, &info);
    Uint64 item = mapgen_hash(name, strlen(name));
    if (ok && info.type == SDL_PATHTYPE_DIRECTORY && !walk->archives_only) {
        MapGenWalk child = {target, 0, walk->copy, false, walk->content};
        ok = (!walk->copy || SDL_CreateDirectory(target)) && SDL_EnumerateDirectory(source, mapgen_walk, &child);
        item ^= child.hash;
    } else if (ok && info.type == SDL_PATHTYPE_FILE && (!walk->archives_only || mapgen_archive(name))) {
        item ^= mapgen_hash(&info.size, sizeof(info.size));
        if (walk->content && !mapgen_archive(name)) {
            size_t size;
            void* bytes = SDL_LoadFile(source, &size);
            if (!bytes) ok = false;
            else item ^= mapgen_hash(bytes, size);
            SDL_free(bytes);
        } else {
            item ^= mapgen_hash(&info.modify_time, sizeof(info.modify_time));
        }
        if (walk->copy) {
            if (mapgen_archive(name)) {
                SDL_PathInfo previous;
                ok = (SDL_GetPathInfo(target, &previous) && previous.size == info.size)
                    || mapgen_link(source, target) || SDL_CopyFile(source, target);
            } else {
                ok = SDL_CopyFile(source, target);
            }
        }
    } else {
        item = 0;
    }
    walk->hash += item;
    SDL_free(source); SDL_free(target);
    return ok ? SDL_ENUM_CONTINUE : SDL_ENUM_FAILURE;
}

static bool mapgen_fingerprint(const char* game, Uint64* hash)
{
    MapGenWalk root = {NULL, 0, false, true};
    if (!SDL_EnumerateDirectory(game, mapgen_walk, &root)) return false;
    *hash = root.hash;
    const char* folders[] = {"data", "modules"};
    for (int i = 0; i < 2; i++) {
        char* path = mapgen_join(game, folders[i]);
        MapGenWalk child = {NULL, 0, false, false, true};
        bool ok = !SDL_GetPathInfo(path, NULL) || SDL_EnumerateDirectory(path, mapgen_walk, &child);
        SDL_free(path);
        if (!ok) return false;
        *hash ^= child.hash;
    }
    return true;
}

static bool mapgen_atomic_text(const char* path, const char* text)
{
    char* temporary;
    SDL_asprintf(&temporary, "%s.tmp", path);
    SDL_IOStream* stream = SDL_IOFromFile(temporary, "wb");
    bool ok = stream && SDL_WriteIO(stream, text, strlen(text)) == strlen(text);
    if (stream && !SDL_CloseIO(stream)) ok = false;
    if (ok) ok = SDL_RenamePath(temporary, path);
    if (!ok) SDL_RemovePath(temporary);
    SDL_free(temporary);
    return ok;
}

static void mapgen_report(int stage)
{
    mapgen_progress.stage = stage;
    char text[128];
    SDL_snprintf(text, sizeof(text), "%llu %llu %d\n", mapgen_progress.done, mapgen_progress.total, stage);
    if (mapgen_status_path) mapgen_atomic_text(mapgen_status_path, text);
}

bool modern_maps_ready(void)
{
    char* text = SDL_LoadFile("modern-ui/local-maps/.generation-complete", NULL);
    unsigned long long source, cache;
    int version;
    bool valid = text && sscanf(text, "%d %llx %llx", &version, &source, &cache) == 3 && version == MAPGEN_VERSION;
    SDL_free(text);
    Uint64 current;
    if (!valid || !mapgen_fingerprint(".", &current) || current != source) return false;
    MapGenWalk files = {NULL, 0, false, false};
    return SDL_EnumerateDirectory("modern-ui/local-maps", mapgen_walk, &files) && files.hash == cache;
}

char* modern_maps_executable(const char* argv0)
{
#ifdef SDL_PLATFORM_WINDOWS
    wchar_t path[32768];
    DWORD size = GetModuleFileNameW(NULL, path, SDL_arraysize(path));
    if (!size || size >= SDL_arraysize(path)) return NULL;
    return SDL_iconv_string("UTF-8", "UTF-16LE", (const char*)path, (size + 1) * sizeof(wchar_t));
#elif defined(SDL_PLATFORM_MACOS)
    uint32_t size = 0;
    _NSGetExecutablePath(NULL, &size);
    char* path = SDL_malloc(size);
    if (path && _NSGetExecutablePath(path, &size) == 0) return path;
    SDL_free(path);
    return NULL;
#else
    char path[4096];
    ssize_t size = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (size > 0) {path[size] = 0; return SDL_strdup(path);}
    return SDL_strdup(argv0);
#endif
}

SDL_Process* modern_maps_start(const char* executable)
{
    if (!executable || !SDL_CreateDirectory("modern-ui/.map-work")) return NULL;
    char* game = SDL_GetCurrentDirectory();
    const char* args[] = {executable, "--modern-generate-maps", game, NULL};
    SDL_PropertiesID properties = SDL_CreateProperties();
    SDL_SetPointerProperty(properties, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void*)args);
    SDL_Process* process = SDL_CreateProcessWithProperties(properties);
    SDL_DestroyProperties(properties);
    SDL_free(game);
    return process;
}

void modern_maps_progress(ModernMapProgress* progress)
{
    char* text = SDL_LoadFile("modern-ui/.map-work/progress", NULL);
    if (text) sscanf(text, "%llu %llu %d", &progress->done, &progress->total, &progress->stage);
    SDL_free(text);
}

static uint32_t mapgen_u32(const unsigned char* p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static int mapgen_sector_from_bytes(const unsigned char* data, size_t size)
{
    if (size < 4) return 0;
    Uint64 at = 4 + (Uint64)mapgen_u32(data) * 48 + 4096 * 4;
    if (at + 4 > size) return 0;
    at += 4 + (mapgen_u32(data + at) ? 0 : 256 * 4);
    if (at + 8 > size) return 0;
    uint32_t version = mapgen_u32(data + at);
    if (version < 0xaa0003 || version > 0xaa0004) return 0;
    at += 8 + (Uint64)mapgen_u32(data + at + 4) * 24 + 12;
    return at + 4 <= size ? (int)mapgen_u32(data + at) : 0;
}

static int mapgen_sector(const char* path)
{
    TigFile* file = tig_file_fopen(path, "rb");
    if (!file) return 0;
    int length = tig_file_filelength(file), id = 0;
    if (length > 0 && length < 64 * 1024 * 1024) {
        unsigned char* data = SDL_malloc(length);
        if (data && tig_file_fread(data, 1, length, file) == (size_t)length)
            id = mapgen_sector_from_bytes(data, length);
        SDL_free(data);
    }
    tig_file_fclose(file);
    return id;
}

static bool mapgen_catalog(const char* module, MapGenTask** tasks, size_t* count)
{
    mes_file_handle_t towns, maps;
    if (!mes_load("rules\\townmap.mes", &towns)) return true;
    if (!mes_load("rules\\maplist.mes", &maps)) {mes_unload(towns); return false;}
    size_t first = *count;
    MesFileEntry entry;
    if (mes_find_first(towns, &entry)) do {
        TownMapInfo info;
        if (!townmap_info(entry.num, &info)) continue;
        if (info.width <= 0 || info.height <= 0 || info.width > 1000000 || info.height > 1000000) return false;
        MapGenTask* next = SDL_realloc(*tasks, (*count + 1) * sizeof(**tasks));
        if (!next) return false;
        *tasks = next;
        MapGenTask* task = &next[(*count)++];
        SDL_zero(*task);
        SDL_strlcpy(task->module, module, sizeof(task->module));
        task->id = entry.num; task->info = info;
    } while (mes_find_next(towns, &entry));
    for (int pass = 0; pass < 2; pass++) {
        if (mes_find_first(maps, &entry)) do {
            if (entry.num < 5000) continue;
            char name[260];
            SDL_strlcpy(name, entry.str, sizeof(name));
            char* comma = strchr(name, ',');
            if (comma) *comma = 0;
            size_t length = strlen(name);
            while (length && SDL_isspace(name[length - 1])) name[--length] = 0;
            char path[1024];
            if (!pass) {
                for (size_t i = first; i < *count; i++) {
                    MapGenTask* task = &(*tasks)[i];
                    Uint64 x = (Uint32)task->info.loc, y = (Uint64)task->info.loc >> 32;
                    Uint64 sector = (x / 64) | ((y / 64) << 26);
                    SDL_snprintf(path, sizeof(path), "maps\\%s\\%llu.sec", name, (unsigned long long)sector);
                    if (mapgen_sector(path) == task->id) SDL_strlcpy(task->source, name, sizeof(task->source));
                }
            } else {
                bool missing = false;
                for (size_t i = first; i < *count; i++) if (!(*tasks)[i].source[0]) missing = true;
                if (!missing) break;
                SDL_snprintf(path, sizeof(path), "maps\\%s\\*.sec", name);
                TigFileList list;
                tig_file_list_create(&list, path);
                for (unsigned j = 0; j < list.count; j++) {
                    SDL_snprintf(path, sizeof(path), "maps\\%s\\%s", name, list.entries[j].path);
                    int id = mapgen_sector(path);
                    for (size_t i = first; i < *count; i++)
                        if ((*tasks)[i].id == id && !(*tasks)[i].source[0])
                            SDL_strlcpy((*tasks)[i].source, name, sizeof((*tasks)[i].source));
                }
                tig_file_list_destroy(&list);
            }
        } while (mes_find_next(maps, &entry));
    }
    mes_unload(maps); mes_unload(towns);
    for (size_t i = first; i < *count; i++) {
        MapGenTask* task = &(*tasks)[i];
        if (!task->source[0] && !SDL_strcasecmp(module, "Arcanum") && task->id == 18 && first < *count)
            SDL_strlcpy(task->source, (*tasks)[first].source, sizeof(task->source));
        if (!task->source[0]) {
            SDL_Log("No source sector found for module %s, town map %d", module, task->id);
            return false;
        }
    }
    return true;
}

static bool mapgen_chunk(SDL_IOStream* file, const char* kind, const unsigned char* data, size_t size)
{
    Uint32 length = SDL_Swap32BE((Uint32)size);
    uLong crc = crc32(0, (const Bytef*)kind, 4);
    if (size) crc = crc32(crc, data, (uInt)size);
    Uint32 checksum = SDL_Swap32BE((Uint32)crc);
    return SDL_WriteIO(file, &length, 4) == 4 && SDL_WriteIO(file, kind, 4) == 4
        && (!size || SDL_WriteIO(file, data, size) == size) && SDL_WriteIO(file, &checksum, 4) == 4;
}

static bool mapgen_write_png(const char* path, const unsigned char* pixels, int width, int height)
{
    size_t size = ((size_t)width * 3 + 1) * height;
    uLongf packed_size = compressBound(size);
    unsigned char* packed = SDL_malloc(packed_size);
    if (!packed) return false;
    bool ok = compress2(packed, &packed_size, pixels, size, 3) == Z_OK;
    char* temporary;
    SDL_asprintf(&temporary, "%s.tmp", path);
    SDL_IOStream* file = ok ? SDL_IOFromFile(temporary, "wb") : NULL;
    unsigned char header[13] = {width >> 24, width >> 16, width >> 8, width,
        height >> 24, height >> 16, height >> 8, height, 8, 2, 0, 0, 0};
    ok = file && SDL_WriteIO(file, "\x89PNG\r\n\x1a\n", 8) == 8
        && mapgen_chunk(file, "IHDR", header, 13)
        && mapgen_chunk(file, "IDAT", packed, packed_size) && mapgen_chunk(file, "IEND", NULL, 0);
    if (file && !SDL_CloseIO(file)) ok = false;
    if (ok) ok = SDL_RenamePath(temporary, path);
    if (!ok) SDL_RemovePath(temporary);
    SDL_free(temporary); SDL_free(packed);
    return ok;
}

static bool mapgen_png_valid(const char* path, int width, int height)
{
    size_t size;
    unsigned char* data = SDL_LoadFile(path, &size);
    bool valid = data && size >= 57 && !memcmp(data, "\x89PNG\r\n\x1a\n", 8);
    size_t offset = 8;
    int chunks = 0;
    while (valid && offset + 12 <= size) {
        Uint32 length;
        memcpy(&length, data + offset, 4); length = SDL_Swap32BE(length);
        if ((Uint64)offset + 12 + length > size) {valid = false; break;}
        const unsigned char* kind = data + offset + 4;
        Uint32 expected;
        memcpy(&expected, data + offset + 8 + length, 4);
        valid = crc32(0, kind, length + 4) == SDL_Swap32BE(expected);
        if (chunks == 0) {
            Uint32 w, h;
            if (length != 13 || memcmp(kind, "IHDR", 4)) {valid = false; break;}
            memcpy(&w, kind + 4, 4); memcpy(&h, kind + 8, 4);
            valid = valid && SDL_Swap32BE(w) == (Uint32)width && SDL_Swap32BE(h) == (Uint32)height
                && !memcmp(kind + 12, "\x08\x02\0\0\0", 5);
        } else if (chunks == 1) {
            valid = valid && !memcmp(kind, "IDAT", 4);
        } else {
            valid = valid && chunks == 2 && !memcmp(kind, "IEND", 4) && length == 0;
        }
        offset += 12 + length; chunks++;
    }
    SDL_free(data);
    return valid && offset == size && chunks == 3;
}

static bool mapgen_tile_valid(const char* folder, int x, int y, int width, int height)
{
    for (int level = 0; level < 4; level++) {
        char* path;
        SDL_asprintf(&path, "%s/%d_%d_%d.png", folder, level, x, y);
        int step = 1 << level;
        bool valid = mapgen_png_valid(path, (width + step - 1) / step, (height + step - 1) / step);
        SDL_free(path);
        if (!valid) return false;
    }
    return true;
}

static bool mapgen_save_tile(TigVideoBuffer* buffer, const char* folder, int x, int y, int width, int height)
{
    TigVideoBufferData data;
    if (tig_video_buffer_lock(buffer) != TIG_OK) return false;
    if (tig_video_buffer_data(buffer, &data) != TIG_OK) {tig_video_buffer_unlock(buffer); return false;}
    bool ok = true;
    for (int level = 0; level < 4 && ok; level++) {
        int step = 1 << level, w = (width + step - 1) / step, h = (height + step - 1) / step;
        size_t stride = w * 3 + 1;
        unsigned char* pixels = SDL_malloc(stride * h);
        if (!pixels) {ok = false; break;}
        for (int row = 0; row < h; row++) {
            unsigned char* dest = pixels + row * stride;
            int previous[3] = {0};
            dest[0] = 1;
            for (int col = 0; col < w; col++) {
                int channels[3] = {0}, count = 0;
                for (int dy = 0; dy < step && row * step + dy < height; dy++) {
                    Uint32* src = (Uint32*)((char*)data.pixels + (row * step + dy) * data.pitch);
                    for (int dx = 0; dx < step && col * step + dx < width; dx++) {
                        Uint32 pixel = src[col * step + dx];
                        channels[0] += tig_color_get_red(pixel);
                        channels[1] += tig_color_get_green(pixel);
                        channels[2] += tig_color_get_blue(pixel);
                        count++;
                    }
                }
                for (int c = 0; c < 3; c++) {
                    int value = (channels[c] + count / 2) / count;
                    dest[col * 3 + c + 1] = value - previous[c]; previous[c] = value;
                }
            }
        }
        char* path;
        SDL_asprintf(&path, "%s/%d_%d_%d.png", folder, level, x, y);
        ok = mapgen_write_png(path, pixels, w, h);
        SDL_free(path); SDL_free(pixels);
    }
    tig_video_buffer_unlock(buffer);
    return ok;
}

static bool mapgen_render(tig_window_handle_t window, const TownMapInfo* map, int x, int y)
{
    int64_t sx, sy;
    location_xy(map->loc, &sx, &sy);
    location_origin_scroll(map->width / 2 - x - sx, map->height / 2 - y - sy);
    TigRect rect = {0, 0, MAPGEN_TILE, MAPGEN_TILE};
    TigRect extended = {-256, -256, MAPGEN_TILE + 512, MAPGEN_TILE + 512};
    TigRectListNode node = {rect, NULL};
    TigRectListNode* rects = &node;
    LocRect loc_rect;
    SectorRect sector_rect;
    tig_window_fill(window, &rect, tig_color_make(0, 0, 0));
    if (!location_screen_rect_to_loc_rect(&extended, &loc_rect)) return true;
    if (!sector_rect_from_loc_rect(&loc_rect, &sector_rect)) return false;
    SectorListNode* sectors = sector_list_create(&loc_rect);
    GameDrawInfo info = {&extended, &loc_rect, &sector_rect, sectors, &rects};
    if (tig_video_3d_begin_scene() != TIG_OK) {sector_list_destroy(sectors); return false;}
    int64_t lighting_center;
    if (location_at(-160, -80, &lighting_center) && location_at(MAPGEN_TILE + 160, -80, &lighting_center)
        && location_at(-160, MAPGEN_TILE + 80, &lighting_center)
        && location_at(MAPGEN_TILE + 160, MAPGEN_TILE + 80, &lighting_center)) light_draw(&info);
    tile_draw(&info); object_draw(&info); roof_draw(&info);
    tig_video_3d_end_scene();
    sector_list_destroy(sectors);
    return true;
}


static bool mapgen_chdir(const char* path)
{
#ifdef SDL_PLATFORM_WINDOWS
    wchar_t* wide = (wchar_t*)SDL_iconv_string("UTF-16LE", "UTF-8", path, strlen(path) + 1);
    bool ok = wide && _wchdir(wide) == 0;
    SDL_free(wide);
    return ok;
#else
    return chdir(path) == 0;
#endif
}

typedef struct MapGenModules {char** names; int count;} MapGenModules;

static SDL_EnumerationResult SDLCALL mapgen_modules(void* userdata, const char* directory, const char* name)
{
    MapGenModules* modules = userdata;
    size_t size = strlen(name);
    if (size <= 4 || SDL_strcasecmp(name + size - 4, ".dat")) return SDL_ENUM_CONTINUE;
    char** names = SDL_realloc(modules->names, (modules->count + 1) * sizeof(*names));
    if (!names) return SDL_ENUM_FAILURE;
    modules->names = names;
    names[modules->count] = SDL_strndup(name, size - 4);
    if (!names[modules->count]) return SDL_ENUM_FAILURE;
    modules->count++;
    return SDL_ENUM_CONTINUE;
}

static SDL_EnumerationResult SDLCALL mapgen_clear_tiles(void* userdata, const char* directory, const char* name)
{
    if (!strcmp(name, "complete") || strstr(name, ".png") || !strcmp(name, ".source")) {
        char* path = mapgen_join(directory, name);
        bool ok = SDL_RemovePath(path);
        SDL_free(path);
        if (!ok) return SDL_ENUM_FAILURE;
    }
    return SDL_ENUM_CONTINUE;
}

static bool mapgen_manifest(const char* folder, const char* module, MapGenTask* tasks, size_t count)
{
    char* path = mapgen_join(folder, "manifest.json");
    char* temporary = mapgen_join(folder, "manifest.json.tmp");
    SDL_IOStream* file = SDL_IOFromFile(temporary, "wb");
    bool ok = file != NULL;
    const char* head = "{\"format\":1,\"tile_size\":1024,\"levels\":[1,2,4,8],\"roofs\":true,\"text_baked\":false,\"dynamic_objects\":false,\"module\":\"";
    if (ok) ok = SDL_WriteIO(file, head, strlen(head)) == strlen(head);
    for (const char* c = module; ok && *c; c++) {
        if (*c == '"' || *c == '\\') ok = SDL_WriteIO(file, "\\", 1) == 1;
        if ((unsigned char)*c < 32) {ok = false; break;}
        if (ok) ok = SDL_WriteIO(file, c, 1) == 1;
    }
    const char* middle = "\",\"maps\":[";
    if (ok) ok = SDL_WriteIO(file, middle, strlen(middle)) == strlen(middle);
    bool first = true;
    for (size_t i = 0; ok && i < count; i++) {
        if (strcmp(tasks[i].module, module)) continue;
        char line[200];
        SDL_snprintf(line, sizeof(line), "%s{\"id\":%d,\"width\":%d,\"height\":%d}",
            first ? "" : ",", tasks[i].id, tasks[i].info.width, tasks[i].info.height);
        ok = SDL_WriteIO(file, line, strlen(line)) == strlen(line);
        first = false;
    }
    if (ok) ok = SDL_WriteIO(file, "]}\n", 3) == 3;
    if (file && !SDL_CloseIO(file)) ok = false;
    if (ok) ok = SDL_RenamePath(temporary, path);
    SDL_free(path); SDL_free(temporary);
    return ok;
}

static int mapgen_run(const char* game_root, int tile_limit)
{
    Uint64 signature;
    if (!mapgen_fingerprint(game_root, &signature)) return 10;
    char stamp[80];
    SDL_snprintf(stamp, sizeof(stamp), "%d %llx\n", MAPGEN_VERSION, (unsigned long long)signature);
    char* cache = mapgen_join(game_root, "modern-ui/local-maps");
    char sandbox_name[100];
    SDL_snprintf(sandbox_name, sizeof(sandbox_name), "modern-ui/.map-work/sandbox-%llx", (unsigned long long)signature);
    char* sandbox = mapgen_join(game_root, sandbox_name);
    if (!SDL_CreateDirectory(cache) || !SDL_CreateDirectory(sandbox)) return 11;
    MapGenWalk root = {sandbox, 0, true, true};
    if (!SDL_EnumerateDirectory(game_root, mapgen_walk, &root)) return 12;
    const char* folders[] = {"data", "modules"};
    for (int i = 0; i < 2; i++) {
        char* from = mapgen_join(game_root, folders[i]);
        char* to = mapgen_join(sandbox, folders[i]);
        MapGenWalk copy = {to, 0, true, false};
        bool ok = SDL_CreateDirectory(to) && (!SDL_GetPathInfo(from, NULL) || SDL_EnumerateDirectory(from, mapgen_walk, &copy));
        SDL_free(from); SDL_free(to);
        if (!ok) return 13;
    }
    if (!mapgen_chdir(sandbox)) return 14;
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "software", SDL_HINT_OVERRIDE);
    highres_config_load();
    TigInitInfo init = {0};
    init.width = init.height = MAPGEN_TILE;
    init.bpp = 32;
    init.texture_width = init.texture_height = MAPGEN_TILE;
    init.flags = TIG_INITIALIZE_WINDOWED | TIG_INITIALIZE_NO_SOUND | TIG_INITIALIZE_SET_WINDOW_NAME;
    init.window_name = "Arcanum HD map preparation";
    init.art_file_path_resolver = name_resolve_path;
    init.art_id_reset_func = name_normalize_aid;
    init.sound_file_path_resolver = gsound_resolve_path;
    if (tig_init(&init) != TIG_OK) return 15;
    intgame_set_iso_window_width(MAPGEN_TILE);
    intgame_set_iso_window_height(MAPGEN_TILE);
    GameInitInfo game = {0};
    game.editor = true;
    if (!intgame_create_iso_window(&game.iso_window_handle) || !gamelib_init(&game)) return 16;
    MapGenModules modules = {0};
    if (!SDL_EnumerateDirectory("modules", mapgen_modules, &modules) || !modules.count) return 17;
    MapGenTask* tasks = NULL;
    size_t count = 0;
    for (int m = 0; m < modules.count; m++) {
        if (!gamelib_mod_load(modules.names[m]) || !mapgen_catalog(modules.names[m], &tasks, &count)) {
            SDL_Log("Cannot index HD maps for module %s", modules.names[m]);
            return 18;
        }
    }
    for (size_t i = 0; i < count; i++)
        mapgen_progress.total += (unsigned long long)((tasks[i].info.width + MAPGEN_TILE - 1) / MAPGEN_TILE)
            * ((tasks[i].info.height + MAPGEN_TILE - 1) / MAPGEN_TILE);
    if (!count) return 19;
    mapgen_report(1);
    SDL_Log("HD map catalog: %zu maps, %llu tiles", count, mapgen_progress.total);
    char previous_module[260] = "", previous_map[260] = "";
    int generated = 0;
    for (size_t i = 0; i < count; i++) {
        MapGenTask* task = &tasks[i];
        char* module_folder = mapgen_join(cache, task->module);
        char* folder;
        SDL_asprintf(&folder, "%s/%d", module_folder, task->id);
        if (!SDL_CreateDirectory(folder)) return 20;
        char* source_stamp = mapgen_join(folder, ".source");
        char* old_stamp = SDL_LoadFile(source_stamp, NULL);
        bool reusable = old_stamp && !strcmp(old_stamp, stamp);
        if (!old_stamp) {
            char* manifest_path = mapgen_join(module_folder, "manifest.json");
            char* manifest = SDL_LoadFile(manifest_path, NULL);
            reusable = manifest && (strstr(manifest, "\"roofs\": true") || strstr(manifest, "\"roofs\":true"));
            SDL_free(manifest_path); SDL_free(manifest);
        }
        SDL_free(old_stamp);
        if (!reusable && !SDL_EnumerateDirectory(folder, mapgen_clear_tiles, NULL)) return 21;
        if (!mapgen_atomic_text(source_stamp, stamp)) return 22;
        char* complete = mapgen_join(folder, "complete");
        SDL_RemovePath(complete);
        for (int y = 0; y < task->info.height; y += MAPGEN_TILE) {
            for (int x = 0; x < task->info.width; x += MAPGEN_TILE) {
                int w = SDL_min(MAPGEN_TILE, task->info.width - x), h = SDL_min(MAPGEN_TILE, task->info.height - y);
                if (!mapgen_tile_valid(folder, x / MAPGEN_TILE, y / MAPGEN_TILE, w, h)) {
                    if (strcmp(previous_module, task->module)) {
                        if (!gamelib_mod_load(task->module)) return 23;
                        SDL_strlcpy(previous_module, task->module, sizeof(previous_module)); previous_map[0] = 0;
                        roof_blit_flags_set(0); sub_43AAB0();
                    }
                    if (strcmp(previous_map, task->source)) {
                        SDL_Log("Preparing HD map: module %s, map %s, local map %d", task->module, task->source, task->id);
                        map_reset();
                        if (!map_preprocess_mobile(task->source)) {
                            SDL_Log("Cannot prepare mobile objects for map %s", task->source);
                            return 24;
                        }
                        char base[300]; SDL_snprintf(base, sizeof(base), "maps\\%s", task->source);
                        if (!map_open(base, "export-state", false)) {
                            SDL_Log("Cannot open map %s in module %s", task->source, task->module);
                            return 24;
                        }
                        for (int type = OBJ_TYPE_PROJECTILE; type < OBJ_TYPE_COUNT; type++)
                            if (object_type_is_enabled(type)) object_type_toggle(type);
                        light_set_colors(tig_color_make(255,255,255), tig_color_make(255,255,255));
                        SDL_strlcpy(previous_map, task->source, sizeof(previous_map));
                    }
                    TigVideoBuffer* buffer;
                    tig_window_vbid_get(game.iso_window_handle, &buffer);
                    if (!mapgen_render(game.iso_window_handle, &task->info, x, y)
                        || !mapgen_save_tile(buffer, folder, x / MAPGEN_TILE, y / MAPGEN_TILE, w, h)) return 25;
                    generated++;
                }
                mapgen_progress.done++; mapgen_report(1);
                if (tile_limit > 0 && generated >= tile_limit) return 75;
            }
        }
        char marker[100]; SDL_snprintf(marker, sizeof(marker), "1 %d %d 1024 4\n", task->info.width, task->info.height);
        if (!mapgen_atomic_text(complete, marker)) return 26;
        SDL_free(complete); SDL_free(source_stamp); SDL_free(folder); SDL_free(module_folder);
    }
    for (int m = 0; m < modules.count; m++) {
        char* folder = mapgen_join(cache, modules.names[m]);
        if (!SDL_CreateDirectory(folder) || !mapgen_manifest(folder, modules.names[m], tasks, count)) return 27;
        SDL_free(folder); SDL_free(modules.names[m]);
    }
    SDL_free(modules.names); SDL_free(tasks);
    gamelib_exit(); tig_exit();
    MapGenWalk files = {NULL, 0, false, false};
    if (!SDL_EnumerateDirectory(cache, mapgen_walk, &files)) return 28;
    char text[100]; SDL_snprintf(text, sizeof(text), "%d %llx %llx\n", MAPGEN_VERSION,
        (unsigned long long)signature, (unsigned long long)files.hash);
    char* marker = mapgen_join(cache, ".generation-complete");
    bool ok = mapgen_atomic_text(marker, text);
    SDL_free(marker); SDL_free(cache); SDL_free(sandbox);
    return ok ? 0 : 29;
}

int modern_maps_generate(const char* game_root, int tile_limit)
{
    if (!mapgen_chdir(game_root)) return 2;
    if (!SDL_CreateDirectory("modern-ui/.map-work")) return 3;
#ifdef SDL_PLATFORM_WINDOWS
    HANDLE lock = CreateFileW(L"modern-ui/.map-work/lock", GENERIC_READ | GENERIC_WRITE, 0,
        NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (lock == INVALID_HANDLE_VALUE) return 4;
#else
    int lock = open("modern-ui/.map-work/lock", O_CREAT | O_RDWR, 0600);
    if (lock < 0 || flock(lock, LOCK_EX | LOCK_NB)) {if (lock >= 0) close(lock); return 4;}
#endif
    SDL_LogOutputFunction previous_log;
    void* previous_userdata;
    SDL_GetLogOutputFunction(&previous_log, &previous_userdata);
    mapgen_log = SDL_IOFromFile("modern-ui/.map-work/generation.log", "wb");
    SDL_SetLogOutputFunction(mapgen_log_output, NULL);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);
    SDL_Log("HD map preparation: %s", game_root);
    mapgen_status_path = mapgen_join(game_root, "modern-ui/.map-work/progress");
    mapgen_report(0);
    int result = modern_maps_ready() ? 0 : mapgen_run(game_root, tile_limit);
    if (result) SDL_Log("HD map preparation stopped: code %d (%s)", result, SDL_GetError());
    mapgen_report(result ? 3 : 2);
    SDL_free(mapgen_status_path); mapgen_status_path = NULL;
    SDL_SetLogOutputFunction(previous_log, previous_userdata);
    if (mapgen_log) SDL_CloseIO(mapgen_log);
    mapgen_log = NULL;
#ifdef SDL_PLATFORM_WINDOWS
    CloseHandle(lock);
#else
    close(lock);
#endif
    return result;
}
