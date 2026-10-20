#include "game/modern_compat.h"

#include <tig/tig.h>
#include "game/stat.h"
#include "game/modern_uap.h"

static ModernCompatibility compatibility = { .level_limit = 51 };

static bool bytes_match(TigFile* file, long offset, const unsigned char* expected, size_t size)
{
    unsigned char bytes[16];
    return size <= sizeof(bytes)
        && tig_file_fseek(file, offset, SEEK_SET) == 0
        && tig_file_fread(bytes, 1, size, file) == size
        && memcmp(bytes, expected, size) == 0;
}

void modern_compat_load(void)
{
    compatibility = (ModernCompatibility){ .level_limit = 51 };
    compatibility.uap = modern_uap_ready() || tig_file_exists("Arcanum5.dat", NULL);
    compatibility.races = modern_uap_ready() || tig_file_exists("ArcanumXRaces.dat", NULL);
    tig_art_mirroring_set(!modern_uap_ready() && !tig_file_exists("ArcanumXAnims1Base.dat", NULL));
    if (!compatibility.uap) return;

    TigFile* exe = tig_file_fopen("Arcanum.exe", "rb");
    if (!exe) return;
    const unsigned char version[] = { 4, 0x6a, 7, 0x6a, 0, 0x6a, 1, 0x68 };
    if (bytes_match(exe, 0, (const unsigned char*)"MZ", 2)
        && bytes_match(exe, 0x45e0, version, sizeof(version))) {
        const long limits[] = { 0xa6642, 0xa66a6, 0xa68c8, 0xa691b, 0x1b5248 };
        bool level100 = true;
        for (size_t i = 0; i < SDL_arraysize(limits); i++)
            level100 = level100 && bytes_match(exe, limits[i], (const unsigned char*)"\x65", 1);
        if (level100) compatibility.level_limit = 101;
        compatibility.master_educator =
            bytes_match(exe, 0xc2982, (const unsigned char*)"\xeb\x04\0\0", 4)
            && bytes_match(exe, 0xc29b1, (const unsigned char*)"\x7c", 1)
            && bytes_match(exe, 0xc29cd, (const unsigned char*)"\x90", 1)
            && bytes_match(exe, 0xc29d0, (const unsigned char*)"\x74", 1)
            && bytes_match(exe, 0xc29e6, (const unsigned char*)"\x74", 1);
        compatibility.no_combat_music =
            bytes_match(exe, 0x1c4d0, (const unsigned char*)"\xc3\xd8\x55\x5d\0", 5);
        compatibility.no_xp_per_hit =
            bytes_match(exe, 0xb52ce, (const unsigned char*)"\xeb\x15", 2)
            && bytes_match(exe, 0x5db27, (const unsigned char*)"\xeb\x15\0\0", 4)
            && bytes_match(exe, 0x5db3e, (const unsigned char*)"\x50", 1);
    }
    tig_file_fclose(exe);
}

const ModernCompatibility* modern_compat_get(void)
{
    return &compatibility;
}

const char* modern_compat_race_prefix(int race)
{
    static const char* original[RACE_COUNT] = {
        "HU", "DW", "EL", "HE", "GN", "HA", "HO", "HG", "EL", "HG", "HO"
    };
    static const char* extra[] = { "DE", "OG", "OC" };
    if (race < 0 || race >= RACE_COUNT) return "";
    if (compatibility.races && race >= RACE_DARK_ELF) return extra[race - RACE_DARK_ELF];
    return original[race];
}
