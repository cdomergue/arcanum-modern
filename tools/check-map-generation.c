#undef NDEBUG
#include <assert.h>
#include "ui/modern_map_generator.c"

int main(int argc, char** argv)
{
    assert(argc == 2);
    assert(SDL_CreateDirectory(argv[1]));
    char* data_dir = mapgen_join(argv[1], "data");
    char* saves = mapgen_join(argv[1], "modules/Arcanum/Save");
    assert(SDL_CreateDirectory(data_dir) && SDL_CreateDirectory(saves));
    char* loose = mapgen_join(data_dir, "sample.pro");
    char* save = mapgen_join(saves, "sentinel");
    assert(SDL_SaveFile(loose, "same", 4));
    Uint64 before, after;
    assert(mapgen_fingerprint(argv[1], &before));
    SDL_Delay(20);
    assert(SDL_SaveFile(loose, "same", 4));
    assert(SDL_SaveFile(save, "player progress", 15));
    assert(mapgen_fingerprint(argv[1], &after) && before == after);
    assert(SDL_SaveFile(loose, "edit", 4));
    assert(mapgen_fingerprint(argv[1], &after) && before != after);
    /* English overlays participate in cache invalidation just like French ones. */
    char* english = mapgen_join(argv[1], "modern-ui/uap-en/active");
    assert(SDL_CreateDirectory(english));
    char* ready = mapgen_join(english, "ready");
    before = after;
    assert(SDL_SaveFile(ready, "2.0.2-en-1\n", 11));
    assert(mapgen_fingerprint(argv[1], &after) && before != after);
    before = after;
    assert(SDL_SaveFile(ready, "2.0.2-en-1\n", 11));
    assert(mapgen_fingerprint(argv[1], &after) && before == after);
    assert(SDL_RemovePath(ready));
    assert(mapgen_fingerprint(argv[1], &after) && before != after);
    SDL_free(english); SDL_free(ready);
    SDL_free(data_dir); SDL_free(saves); SDL_free(loose); SDL_free(save);
    unsigned char sector[18000] = {0};
    size_t at = 4 + 4096 * 4;
    sector[at] = 1;
    at += 4;
    sector[at] = 4; sector[at + 2] = 0xaa;
    at += 8 + 12;
    sector[at] = 125;
    assert(mapgen_sector_from_bytes(sector, at + 4) == 125);
    assert(mapgen_sector_from_bytes(sector, at) == 0);
    sector[0] = 255;
    assert(mapgen_sector_from_bytes(sector, at + 4) == 0);
    char* folder = mapgen_join(argv[1], "tile");
    assert(SDL_CreateDirectory(folder));
    assert(!mapgen_tile_valid(folder, 0, 0, 16, 16));
    for (int level = 0; level < 4; level++) {
        char* path;
        SDL_asprintf(&path, "%s/%d_0_0.png", folder, level);
        int side = 16 >> level;
        unsigned char pixels[16 * 49] = {0};
        assert(mapgen_write_png(path, pixels, side, side));
        SDL_free(path);
    }
    assert(mapgen_tile_valid(folder, 0, 0, 16, 16));
    char* corrupt = mapgen_join(folder, "0_0_0.png");
    size_t size;
    unsigned char* png = SDL_LoadFile(corrupt, &size);
    assert(png && size > 40);
    png[40] ^= 1;
    assert(SDL_SaveFile(corrupt, png, size));
    assert(!mapgen_tile_valid(folder, 0, 0, 16, 16));
    png[40] ^= 1;
    assert(SDL_SaveFile(corrupt, png, size));
    SDL_free(png); SDL_free(corrupt);
    char* middle = mapgen_join(folder, "1_0_0.png");
    assert(SDL_RemovePath(middle));
    assert(!mapgen_tile_valid(folder, 0, 0, 16, 16));
    SDL_free(middle);
    SDL_free(folder);
    puts("PASS: source changes, identical rewrites, save exclusion, sector bounds, PNG CRC and complete mip chains");
    return 0;
}
