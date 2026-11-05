/* First-page choices only. Point allocation and shopping are rebuilt by the
 * native creation path on Back. Nothing here is serialized into a save. */
static struct {
    bool valid, resume;
    int64_t object;
    int stats[8], race, gender, portrait, background, background_text;
    int bonuses[2];
    char name[sizeof(mainmenu_ui_textedit_buffer)];
} modern_creation_draft;

static void modern_creation_draft_capture(int64_t obj)
{
    modern_creation_draft.valid = true;
    modern_creation_draft.resume = false;
    modern_creation_draft.object = obj;
    for (int i = 0; i < 8; i++) {
        modern_creation_draft.stats[i] = stat_base_get(obj, STAT_STRENGTH + i);
    }
    modern_creation_draft.race = stat_level_get(obj, STAT_RACE);
    modern_creation_draft.gender = stat_level_get(obj, STAT_GENDER);
    modern_creation_draft.portrait = portrait_get(obj);
    modern_creation_draft.background = background_get(obj);
    modern_creation_draft.background_text = background_text_get(obj);
    for(int i=0;i<2;i++)modern_creation_draft.bonuses[i]=modern_background_bonus_get(obj,i);
    SDL_strlcpy(modern_creation_draft.name, mainmenu_ui_textedit_buffer,
        sizeof(modern_creation_draft.name));
}

static bool modern_creation_draft_take(int64_t obj)
{
    bool restore = modern_creation_draft.resume && modern_creation_draft.valid
        && obj != OBJ_HANDLE_NULL && obj == modern_creation_draft.object
        && obj_handle_is_valid(obj);
    modern_creation_draft.resume = false;
    modern_creation_draft.valid = false;
    return restore;
}

static void modern_creation_draft_restore(int64_t obj)
{
    int race = modern_creation_draft.race;
    object_set_gender_and_race(obj, stru_5C5170[race].body_type,
        modern_creation_draft.gender, race);
    object_set_current_aid(obj, obj_field_int32_get(obj, OBJ_F_CURRENT_AID));
    background_clear(obj);
    background_set(obj, modern_creation_draft.background,
        modern_creation_draft.background_text);
    /* Restore the exact raw starting values, including custom adjustments.
     * A stat setter could clamp an unchanged base value against a background's
     * effective cap. This is a fresh, unequipped creation PC with no allocated
     * skills or spells; only values captured before allocation are restored. */
    for (int i = 0; i < 8; i++) {
        obj_arrayfield_int32_set(obj, OBJ_F_CRITTER_STAT_BASE_IDX,
            STAT_STRENGTH + i, modern_creation_draft.stats[i]);
    }
    obj_field_int32_set(obj, OBJ_F_CRITTER_PORTRAIT, modern_creation_draft.portrait);
    if(modern_background_bonuses_available())for(int i=0;i<2;i++)
        modern_background_bonus_set(obj,i,modern_creation_draft.bonuses[i]);
    SDL_strlcpy(mainmenu_ui_textedit_buffer, modern_creation_draft.name,
        sizeof(mainmenu_ui_textedit_buffer));
    obj_field_string_set(obj, OBJ_F_PC_PLAYER_NAME, mainmenu_ui_textedit_buffer);
    ui_spell_maintain_refresh();
}
