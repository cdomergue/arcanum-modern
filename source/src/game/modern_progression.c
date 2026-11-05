#include "game/modern_progression.h"

#include <tig/tig.h>
#include "game/ai.h"
#include "game/background.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/effect.h"
#include "game/gamelib.h"
#include "game/level.h"
#include "game/obj.h"
#include "game/obj_pool.h"
#include "game/object.h"
#include "game/player.h"
#include "game/stat.h"
#include "game/timeevent.h"
#include "game/ui.h"

#define PROGRESSION_XP_MAX 2000000000
#define PROGRESSION_GROUPS 128
#define ENCOUNTER_GRACE_MS 5000

typedef struct PendingCombatXP {
    int64_t leader;
    int amount;
} PendingCombatXP;

static PendingCombatXP pending[PROGRESSION_GROUPS];
static int pending_count;
static unsigned grace_remaining;
static bool travelling, sharing;
static Uint64 last_award;
static const char* progression_path="Save\\Current\\modern-progression.dat";

void modern_progression_reset(void)
{
    memset(pending,0,sizeof(pending));pending_count=0;
    grace_remaining=0;travelling=sharing=false;last_award=SDL_GetTicks();
}

bool modern_combat_xp_sharing(void) { return sharing; }
unsigned modern_encounter_grace_remaining(void) { return grace_remaining; }
void modern_encounter_started(void) { grace_remaining=ENCOUNTER_GRACE_MS; }
void modern_encounter_travel(bool moving) { travelling=moving; }

void modern_encounter_advance(unsigned milliseconds)
{
    /* Called with the engine's real-time delta, not accelerated world time. */
    if(travelling && !sub_45B300())
        grace_remaining=milliseconds>=grace_remaining?0:grace_remaining-milliseconds;
}

void modern_combat_xp_add(int64_t actor,int amount)
{
    if(amount<=0 || !actor || !obj_handle_is_valid(actor))return;
    int64_t leader=obj_field_int32_get(actor,OBJ_F_TYPE)==OBJ_TYPE_PC?actor:critter_pc_leader_get(actor);
    if(!leader)return;
    int i;for(i=0;i<pending_count;i++)if(pending[i].leader==leader)break;
    if(i==pending_count) {
        if(pending_count==PROGRESSION_GROUPS)return;
        pending[pending_count++]=(PendingCombatXP){leader,0};
    }
    pending[i].amount=(int)SDL_min((int64_t)PROGRESSION_XP_MAX,(int64_t)pending[i].amount+amount);
    last_award=SDL_GetTicks();
}

static void award_member(int64_t member,int amount)
{
    if(!member || !obj_handle_is_valid(member) || critter_is_dead(member)
        || (obj_field_int32_get(member,OBJ_F_FLAGS)&(OF_DESTROYED|OF_EXTINCT)))return;
    int xp=stat_base_get(member,STAT_EXPERIENCE_POINTS);
    /* Old saves did not track follower XP. Preserve the recruited level and
     * any later level granted by a quest or an advancement script. */
    if(obj_field_int32_get(member,OBJ_F_TYPE)==OBJ_TYPE_NPC)
        xp=SDL_max(xp,level_experience_for_level(stat_base_get(member,STAT_LEVEL)));
    stat_base_set(member,STAT_EXPERIENCE_POINTS,
        (int)SDL_min((int64_t)PROGRESSION_XP_MAX,(int64_t)xp+amount));
}

static bool fighting_near_member(int64_t leader,int64_t member)
{
    if(!critter_is_active(member))return false;
    if(!combat_can_exit_combat_mode(member))return true;
    if(member!=leader && ai_is_fighting(member))return true;
    /* A passive hero can still be under attack, or have companions fighting
     * away from the hero. Combat mode alone is not a reliable end signal. */
    ObjectList nearby;object_list_vicinity(member,OBJ_TM_NPC,&nearby);
    bool fighting=false;
    for(ObjectNode* node=nearby.head;node;node=node->next) {
        if(!critter_is_active(node->obj) || !ai_is_fighting(node->obj))continue;
        int64_t focus=obj_field_handle_get(node->obj,OBJ_F_NPC_COMBAT_FOCUS);
        if(focus && (focus==leader || critter_pc_leader_get(focus)==leader)){fighting=true;break;}
    }
    object_list_destroy(&nearby);return fighting;
}

void modern_progression_tick(void)
{
    if(!pending_count || gamelib_in_reset() || gamelib_in_load() || gamelib_in_save()
        || sub_45B300() || combat_turn_based_is_active() || SDL_GetTicks()-last_award<500)return;
    for(int i=0;i<pending_count;) {
        PendingCombatXP entry=pending[i];
        if(!obj_handle_is_valid(entry.leader) || critter_is_dead(entry.leader)) {
            pending[i]=pending[--pending_count];continue;
        }
        if(!critter_is_active(entry.leader)){i++;continue;}
        ObjectList followers;object_list_all_followers(entry.leader,&followers);
        bool fighting=fighting_near_member(entry.leader,entry.leader);
        for(ObjectNode* node=followers.head;!fighting && node;node=node->next)
            fighting=fighting_near_member(entry.leader,node->obj);
        if(fighting){object_list_destroy(&followers);i++;continue;}
        /* Clear before awarding: level-up callbacks cannot pay this twice. */
        pending[i]=pending[--pending_count];
        int amount=SDL_max(0,critter_adjust_xp_gain(entry.leader,entry.amount));
        sharing=true;
        award_member(entry.leader,amount);
        for(ObjectNode* node=followers.head;node;node=node->next)
            if(node->obj!=entry.leader)award_member(node->obj,amount);
        sharing=false;
        object_list_destroy(&followers);
        background_educate_followers(entry.leader);
        if(player_is_local_pc_obj(entry.leader))sub_4601C0();
    }
}

/* Optional member of the existing save archive: native data.sav stays intact. */
bool modern_progression_save(void)
{
    TigFile* file=tig_file_fopen(progression_path,"wb");if(!file)return false;
    unsigned version=1;
    bool ok=tig_file_fwrite(&version,sizeof(version),1,file)==1
        && tig_file_fwrite(&grace_remaining,sizeof(grace_remaining),1,file)==1
        && tig_file_fwrite(&pending_count,sizeof(pending_count),1,file)==1;
    for(int i=0;ok && i<pending_count;i++) {
        ObjectID id=obj_handle_is_valid(pending[i].leader)?obj_get_id(pending[i].leader):(ObjectID){.type=OID_TYPE_NULL};
        ok=tig_file_fwrite(&id,sizeof(id),1,file)==1
            && tig_file_fwrite(&pending[i].amount,sizeof(pending[i].amount),1,file)==1;
    }
    if(tig_file_fclose(file)!=0)ok=false;
    return ok;
}

bool modern_progression_load(void)
{
    modern_progression_reset();
    if(!tig_file_exists(progression_path,NULL))return true;
    TigFile* file=tig_file_fopen(progression_path,"rb");if(!file)return false;
    unsigned version=0,grace=0;int count=0;
    bool ok=tig_file_fread(&version,sizeof(version),1,file)==1 && version==1
        && tig_file_fread(&grace,sizeof(grace),1,file)==1 && grace<=ENCOUNTER_GRACE_MS
        && tig_file_fread(&count,sizeof(count),1,file)==1 && count>=0 && count<=PROGRESSION_GROUPS;
    for(int i=0;ok && i<count;i++) {
        ObjectID id;int amount;
        ok=tig_file_fread(&id,sizeof(id),1,file)==1
            && tig_file_fread(&amount,sizeof(amount),1,file)==1 && amount>=0 && amount<=PROGRESSION_XP_MAX;
        if(!ok)break;
        int64_t leader=obj_pool_perm_lookup(id);
        if(!leader)continue;
        if(obj_field_int32_get(leader,OBJ_F_TYPE)!=OBJ_TYPE_PC){ok=false;break;}
        for(int j=0;j<pending_count;j++)if(pending[j].leader==leader)ok=false;
        if(ok)pending[pending_count++]=(PendingCombatXP){leader,amount};
    }
    if(tig_file_fclose(file)!=0)ok=false;
    if(!ok){modern_progression_reset();return false;}
    grace_remaining=grace;return true;
}
