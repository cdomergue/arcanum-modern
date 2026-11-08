/* Optional engine-side check of an exported save, in an isolated installation.
 * Usage: check-save-editor /absolute/sandbox Slot0001
 * The sandbox must contain .save-editor-sandbox and a save edited to have
 * all 80 spells and all 16 skills at rank 5 / training 3.
 */
#undef NDEBUG
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <tig/tig.h>
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/name.h"
#include "game/player.h"
#include "game/skill.h"
#include "game/spell.h"
#include "game/stat.h"
#include "ui/gameuilib.h"
#include "ui/intgame.h"

int main(int argc, char** argv)
{
    if (argc != 3 || chdir(argv[1]) || access(".save-editor-sandbox", R_OK)) return 2;
    setvbuf(stdout, NULL, _IOLBF, 0);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "software", SDL_HINT_OVERRIDE);
    TigInitInfo init = {0};
    init.width = 800; init.height = 600; init.bpp = 32;
    init.texture_width = init.texture_height = 2048;
    init.flags = TIG_INITIALIZE_WINDOWED | TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver = name_resolve_path;
    init.art_id_reset_func = name_normalize_aid;
    init.sound_file_path_resolver = gsound_resolve_path;
    puts("Initializing isolated engine...");
    assert(tig_init(&init) == TIG_OK);
    intgame_set_iso_window_width(init.width); intgame_set_iso_window_height(init.height);
    GameInitInfo game = {0};
    assert(intgame_create_iso_window(&game.iso_window_handle) && gamelib_init(&game));
    assert(gameuilib_init(&game) && gamelib_mod_load("Arcanum") && gameuilib_mod_load());
    gamelib_splash_clear();
    puts("Loading exported save...");
    assert(gamelib_load(argv[2]));
    int64_t pc = player_get_local_pc_obj();
    for (int spell = 0; spell < SPELL_COUNT; spell++) assert(spell_is_known(pc, spell));
    for (int skill = 0; skill < BASIC_SKILL_COUNT; skill++) {
        assert(basic_skill_points_get(pc, skill) == 5);
        assert(basic_skill_training_get(pc, skill) == TRAINING_MASTER);
    }
    for (int skill = 0; skill < TECH_SKILL_COUNT; skill++) {
        assert(tech_skill_points_get(pc, skill) == 5);
        assert(tech_skill_training_get(pc, skill) == TRAINING_MASTER);
    }
    printf("PASS: engine loaded 80 spells, 16 skills at rank 5 / training 3; aptitude counters %d / %d\n",
        stat_base_get(pc, STAT_MAGICK_POINTS), stat_base_get(pc, STAT_TECH_POINTS));
    // Extract local presentation text into the sandbox only, never the repository.
    const char* names[] = {"stat", "skill", "spell"};
    assert(mkdir("locale", 0700) == 0 || errno == EEXIST);
    for (int i = 0; i < 3; i++) {
        char source[64], destination[64];
        snprintf(source, sizeof(source), "mes\\%s.mes", names[i]);
        snprintf(destination, sizeof(destination), "locale/%s.mes", names[i]);
        TigFile* in = tig_file_fopen(source, "rb"); assert(in);
        FILE* out = fopen(destination, "wb"); assert(out);
        unsigned char buffer[4096]; size_t count;
        while ((count = tig_file_fread(buffer, 1, sizeof(buffer), in))) assert(fwrite(buffer, 1, count, out) == count);
        assert(!tig_file_ferror(in)); tig_file_fclose(in); assert(fclose(out) == 0);
    }
    return 0;
}
