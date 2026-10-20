enum { COMPANION_HOTKEY_BANKS = 128, COMPANION_HOTKEY_SLOTS = 10 };
typedef struct CompanionHotkeys {
    ObjectID owner;
    Hotkey slots[COMPANION_HOTKEY_SLOTS];
} CompanionHotkeys;
static CompanionHotkeys companion_hotkeys[COMPANION_HOTKEY_BANKS];
static int companion_hotkey_count;

static CompanionHotkeys* companion_hotkey_bank(int64_t actor, bool create)
{
    ObjectID owner = obj_get_id(actor);
    for (int i = 0; i < companion_hotkey_count; i++) {
        if (objid_is_equal(owner, companion_hotkeys[i].owner)) return &companion_hotkeys[i];
    }
    if (!create || companion_hotkey_count == COMPANION_HOTKEY_BANKS) return NULL;
    CompanionHotkeys* bank = &companion_hotkeys[companion_hotkey_count++];
    memset(bank, 0, sizeof(*bank));
    bank->owner = owner;
    for (int i = 0; i < COMPANION_HOTKEY_SLOTS; i++) {
        bank->slots[i].slot = -1;
        bank->slots[i].art_id = TIG_ART_ID_INVALID;
    }
    return bank;
}

Hotkey* modern_hotkey_get(int index)
{
    int64_t actor = combat_input_actor();
    if (index < 0 || index >= COMPANION_HOTKEY_SLOTS || !actor || !obj_handle_is_valid(actor)) return NULL;
    if (player_is_local_pc_obj(actor)) return sub_57F240(index);
    CompanionHotkeys* bank = companion_hotkey_bank(actor, false);
    if (!bank) return NULL;
    Hotkey* hotkey = &bank->slots[index];
    if (hotkey->type == HOTKEY_ITEM) {
        int64_t item = obj_pool_perm_lookup(hotkey->item_obj.field_8.objid), owner;
        if (!item || !item_parent(item, &owner) || owner != actor) {
            sub_57E5A0(hotkey);
        } else {
            sub_4440E0(item, &hotkey->item_obj);
            hotkey->art_id = sub_554BE0(item);
            hotkey->count = item_count_items_matching_prototype(actor, item);
        }
    } else if (hotkey->type == HOTKEY_SPELL && !spell_is_known(actor, hotkey->data)) {
        sub_57E5A0(hotkey);
    }
    return hotkey;
}

void modern_hotkey_activate(int index)
{
    Hotkey* hotkey = modern_hotkey_get(index);
    if (!hotkey || hotkey->type == HOTKEY_NONE) return;
    if (player_is_local_pc_obj(combat_input_actor())) {
        intgame_hotkey_activate(hotkey);
    } else {
        Hotkey action = *hotkey;
        intgame_hotkey_activate(&action);
    }
}

static bool companion_hotkey_assign(int64_t actor, int index, int type, int data, int64_t item)
{
    CompanionHotkeys* bank = companion_hotkey_bank(actor, true);
    if (!bank) return false;
    Hotkey* hotkey = &bank->slots[index];
    *hotkey = (Hotkey){ .slot = -1, .type = type, .data = data, .count = -1, .art_id = TIG_ART_ID_INVALID };
    if (type == HOTKEY_ITEM) {
        sub_4440E0(item, &hotkey->item_obj);
        hotkey->art_id = sub_554BE0(item);
        hotkey->count = item_count_items_matching_prototype(actor, item);
    } else {
        hotkey->info.art_num = type == HOTKEY_SPELL ? spell_icon(data) : sub_579F70(data);
        tig_art_interface_id_create(hotkey->info.art_num, 0, 0, 0, &hotkey->art_id);
    }
    return true;
}

/* A separate archive member preserves the native hero hotkey save layout. */
static bool companion_hotkeys_save(void)
{
    TigFile* file = tig_file_fopen("Save\\Current\\modern-hotkeys.dat", "wb");
    if (!file) return false;
    int version = 1;
    bool ok = tig_file_fwrite(&version, sizeof(version), 1, file) == 1
        && tig_file_fwrite(&companion_hotkey_count, sizeof(companion_hotkey_count), 1, file) == 1;
    for (int i = 0; ok && i < companion_hotkey_count; i++) {
        CompanionHotkeys* bank = &companion_hotkeys[i];
        ok = tig_file_fwrite(&bank->owner, sizeof(ObjectID), 1, file) == 1;
        for (int j = 0; ok && j < COMPANION_HOTKEY_SLOTS; j++) {
            Hotkey* hotkey = &bank->slots[j];
            ok = tig_file_fwrite(&hotkey->type, sizeof(hotkey->type), 1, file) == 1
                && tig_file_fwrite(&hotkey->data, sizeof(hotkey->data), 1, file) == 1
                && tig_file_fwrite(&hotkey->item_obj.field_8.objid, sizeof(ObjectID), 1, file) == 1;
        }
    }
    if (tig_file_fclose(file) != 0) ok = false;
    return ok;
}

static bool companion_hotkeys_load(void)
{
    companion_hotkey_count = 0;
    TigFile* file = tig_file_fopen("Save\\Current\\modern-hotkeys.dat", "rb");
    if (!file) return true;
    int version, count;
    bool ok = tig_file_fread(&version, sizeof(version), 1, file) == 1 && version == 1
        && tig_file_fread(&count, sizeof(count), 1, file) == 1 && count >= 0 && count <= COMPANION_HOTKEY_BANKS;
    for (int i = 0; ok && i < count; i++) {
        CompanionHotkeys* bank = &companion_hotkeys[i];
        memset(bank, 0, sizeof(*bank));
        ok = tig_file_fread(&bank->owner, sizeof(ObjectID), 1, file) == 1;
        for (int previous = 0; ok && previous < i; previous++)
            if (objid_is_equal(bank->owner, companion_hotkeys[previous].owner)) ok = false;
        for (int j = 0; ok && j < COMPANION_HOTKEY_SLOTS; j++) {
            Hotkey* hotkey = &bank->slots[j];
            hotkey->slot = -1;
            hotkey->count = -1;
            hotkey->art_id = TIG_ART_ID_INVALID;
            ok = tig_file_fread(&hotkey->type, sizeof(hotkey->type), 1, file) == 1
                && tig_file_fread(&hotkey->data, sizeof(hotkey->data), 1, file) == 1
                && tig_file_fread(&hotkey->item_obj.field_8.objid, sizeof(ObjectID), 1, file) == 1;
            if (!ok) break;
            switch (hotkey->type) {
            case HOTKEY_NONE:
            case HOTKEY_ITEM:
                break;
            case HOTKEY_SKILL:
            case HOTKEY_SPELL:
                ok = hotkey->data >= 0 && hotkey->data < (hotkey->type == HOTKEY_SPELL ? SPELL_COUNT : 4);
                if (ok) {
                    hotkey->info.art_num = hotkey->type == HOTKEY_SPELL ? spell_icon(hotkey->data) : sub_579F70(hotkey->data);
                    tig_art_interface_id_create(hotkey->info.art_num, 0, 0, 0, &hotkey->art_id);
                }
                break;
            default:
                ok = false;
            }
        }
    }
    tig_file_fclose(file);
    if (ok) companion_hotkey_count = count;
    return ok;
}
