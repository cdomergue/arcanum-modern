/* Native AID descriptions. Only presentation is changed; object fields and
 * saves are never rewritten. Included after game_text in modern_ui.c. */
typedef struct ModernAidEntry { int id; const char *fr, *en; } ModernAidEntry;
typedef struct ModernAidVariant { const char* guid; int description, effect; } ModernAidVariant;
#include "modern_aid_data.inc"

static const ModernAidEntry* aid_entry(int id)
{
    size_t lo=0,hi=SDL_arraysize(modern_aid_entries);
    while(lo<hi) {
        size_t mid=lo+(hi-lo)/2;
        if(modern_aid_entries[mid].id<id)lo=mid+1;else hi=mid;
    }
    return lo<SDL_arraysize(modern_aid_entries) && modern_aid_entries[lo].id==id
        ? &modern_aid_entries[lo] : NULL;
}

static const ModernAidEntry* aid_item_entry(int64_t object)
{
    int description=obj_field_int32_get(object,OBJ_F_DESCRIPTION);
    int effect=obj_field_int32_get(object,OBJ_F_ITEM_DESCRIPTION_EFFECTS);
    ObjectID oid=obj_get_id(object);
    if(oid.type==OID_TYPE_GUID) {
        char guid[OID_STR_LEN];objid_id_to_str(guid,oid);
        size_t lo=0,hi=SDL_arraysize(modern_aid_variants);
        while(lo<hi) {
            size_t mid=lo+(hi-lo)/2;
            if(strcmp(modern_aid_variants[mid].guid,guid)<0)lo=mid+1;else hi=mid;
        }
        if(lo<SDL_arraysize(modern_aid_variants)) {
            const ModernAidVariant* v=&modern_aid_variants[lo];
            if(v->description==description && !strcmp(v->guid,guid))return aid_entry(v->effect);
        }
    }
    /* Unique items often inherit a generic effect ID. Prefer their own entry;
     * never attach generic weapon statistics to an unknown unique variant. */
    const ModernAidEntry* entry;
    if(effect>=30000 && (entry=aid_entry(effect))!=NULL)return entry;
    if(description>=30000)return aid_entry(description);
    entry=aid_entry(effect);
    return entry?entry:aid_entry(description);
}

static const char* aid_token(char kind,int number)
{
    switch(kind) {
    case 's': return number<STAT_COUNT?stat_name(number):NULL;
    case 'k': return number<BASIC_SKILL_COUNT?basic_skill_name(number):
        number<SKILL_COUNT?tech_skill_name(number-BASIC_SKILL_COUNT):NULL;
    case 't': return number<TECH_COUNT?tech_discipline_name_get(number):NULL;
    case 'p': return number<SPELL_COUNT?spell_name(number):NULL;
    case 'i': return number<76?modern_intgame_text(number):NULL;
    case 'd': return description_get(number);
    }
    return NULL;
}

static bool aid_expand(const char* text,char* result,size_t capacity)
{
    size_t used=0;
    if(!capacity)return false;
    result[0]=0;
    while(*text) {
        if(*text!='@') {
            /* Copy whole UTF-8 characters, so even a full buffer stays valid. */
            const char* end=text;SDL_StepUTF8(&end,NULL);
            size_t length=(size_t)(end-text);
            if(used+length>=capacity)return false;
            memcpy(result+used,text,length);used+=length;result[used]=0;text=end;
        } else {
            if(!text[1] || !text[2])return false;
            char kind=text[1];char* end;
            long number=strtol(text+2,&end,10);
            if(end==text+2 || *end!='@' || number<0 || number>50000)return false;
            const char* raw=aid_token(kind,(int)number);
            if(!raw || !*raw)return false;
            const char* label=game_text(raw);size_t length=strlen(label);
            if(used+length>=capacity)return false;
            memcpy(result+used,label,length+1);used+=length;text=end+1;
        }
    }
    return true;
}

bool modern_ui_item_description(int64_t object,char* result,size_t capacity)
{
    const char* module=gamelib_current_module_name_get();
    if(!modern_ui_enabled() || !object || !obj_handle_is_valid(object)
        || !module || SDL_strcasecmp(module,"Arcanum") || !item_is_identified(object))return false;
    int type=obj_field_int32_get(object,OBJ_F_TYPE);
    if(type<OBJ_TYPE_WEAPON || type>OBJ_TYPE_GENERIC || type==OBJ_TYPE_GOLD
        || type==OBJ_TYPE_KEY || type==OBJ_TYPE_KEY_RING)return false;
    const ModernAidEntry* entry=aid_item_entry(object);
    if(!entry)return false;
    char utf8[8192],encoded[8192];
    if(!aid_expand(tr(entry->fr,entry->en),utf8,sizeof(utf8)))return false;
    /* This API feeds the engine's CP1252 detail capture. Convert only here,
     * at the UI boundary, and never truncate an existing fallback description. */
    to_game(utf8,encoded,sizeof(encoded));
    size_t length=strlen(encoded);
    if(!length || length>=capacity)return false;
    memcpy(result,encoded,length+1);
    return true;
}
