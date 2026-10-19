enum {SHARED_TURN_CAPACITY=128};
typedef struct SharedTurnMember {
    int64_t actor;
    int points;
    bool started, ended;
} SharedTurnMember;
static SharedTurnMember shared_turn_members[SHARED_TURN_CAPACITY];
static int shared_turn_count;

static SharedTurnMember* shared_turn_member(int64_t actor,bool create)
{
    for(int i=0;i<shared_turn_count;i++)if(shared_turn_members[i].actor==actor)return &shared_turn_members[i];
    if(!create || shared_turn_count==SHARED_TURN_CAPACITY)return NULL;
    SharedTurnMember* member=&shared_turn_members[shared_turn_count++];
    *member=(SharedTurnMember){.actor=actor};
    return member;
}

static void shared_turn_store(void)
{
    if(!tactical_enabled || !combat_turn_based_active || !dword_5FC240)return;
    SharedTurnMember* member=shared_turn_member(dword_5FC240->obj,false);
    if(member && member->started)member->points=combat_action_points;
}

static bool shared_turn_restore(int64_t actor)
{
    if(!tactical_enabled || gamelib_in_load() || !combat_player_controls(actor))return false;
    SharedTurnMember* member=shared_turn_member(actor,true);
    if(!member || !member->started)return false;
    combat_action_points=member->ended?0:member->points;
    return true;
}

static void shared_turn_started(int64_t actor)
{
    if(!tactical_enabled || gamelib_in_load() || !combat_player_controls(actor))return;
    SharedTurnMember* member=shared_turn_member(actor,true);
    if(member){member->started=true;member->points=combat_action_points;}
}

static bool shared_turn_block(ObjectNode** first,ObjectNode** last)
{
    *first=*last=NULL;
    if(!tactical_enabled || !combat_turn_based_active || !dword_5FC240
        || !combat_player_controls(dword_5FC240->obj))return false;
    bool found=false;
    for(ObjectNode* node=combat_critter_list.head;node;node=node->next){
        bool current=node==dword_5FC240;
        if(!current && (!obj_handle_is_valid(node->obj) || !critter_is_active(node->obj) || sub_4B7DC0(node->obj)))continue;
        if(combat_player_controls(node->obj)){
            if(!*first)*first=node;
            *last=node;
            if(current)found=true;
        }else{
            if(found)break;
            *first=*last=NULL;
        }
    }
    return found;
}

bool combat_shared_turn_available(int64_t actor)
{
    ObjectNode *first,*last;
    if(!obj_handle_is_valid(actor) || !critter_is_active(actor) || !combat_player_controls(actor)
        || !shared_turn_block(&first,&last))return false;
    SharedTurnMember* member=shared_turn_member(actor,false);
    if(member && (member->ended || (member->started && member->points<=0 && actor!=combat_input_actor())))return false;
    if(!member && shared_turn_count==SHARED_TURN_CAPACITY)return false;
    for(ObjectNode* node=first;node;node=node->next){
        if(node->obj==actor)return true;
        if(node==last)break;
    }
    return false;
}

bool combat_shared_turn_select(int64_t actor)
{
    int64_t current=combat_input_actor();
    if(actor==current || combat_action_points<=0 || !combat_input_ready(current) || !combat_shared_turn_available(actor))return false;
    AnimID animation,fidget;
    if(sub_423300(actor,&animation) && (!anim_is_current_goal_type(actor,AG_ANIM_FIDGET,&fidget)
        || !anim_id_is_equal(&animation,&fidget)))return false;
    shared_turn_store();
    combat_required_action_points=0;
    qword_5FC258=OBJ_HANDLE_NULL;dword_5FC260=0;dword_5FC264=false;
    combat_turn_based_whos_turn_set(actor);
    return combat_input_actor()==actor;
}

static bool shared_turn_next(void)
{
    if(!tactical_enabled || !combat_turn_based_active || !dword_5FC240)return false;
    SharedTurnMember* member=shared_turn_member(dword_5FC240->obj,false);
    if(!member)return false;
    member->points=0;member->ended=true;
    ObjectNode *first,*last;
    if(!shared_turn_block(&first,&last))return false;
    for(ObjectNode* node=first;node;node=node->next){
        member=shared_turn_member(node->obj,false);
        if(obj_handle_is_valid(node->obj) && critter_is_active(node->obj) && !sub_4B7DC0(node->obj)
            && combat_player_controls(node->obj) && (!member || !member->ended)){
            dword_5FC240=node;
            qword_5FC258=OBJ_HANDLE_NULL;dword_5FC260=0;dword_5FC264=false;
            combat_turn_based_subturn_start();
            return true;
        }
        if(node==last)break;
    }
    dword_5FC240=last;
    return false;
}

/* The optional archive member keeps the native data.sav layout compatible. */
static bool shared_turn_save(void)
{
    shared_turn_store();
    TigFile* file=tig_file_fopen("Save\\Current\\modern-turns.dat","wb");
    if(!file)return false;
    int version=1,count=tactical_enabled && combat_turn_based_active?shared_turn_count:0;
    bool ok=tig_file_fwrite(&version,sizeof(version),1,file)==1
        && tig_file_fwrite(&combat_turn_based_turn,sizeof(combat_turn_based_turn),1,file)==1
        && tig_file_fwrite(&count,sizeof(count),1,file)==1;
    for(int i=0;ok && i<count;i++){
        SharedTurnMember* member=&shared_turn_members[i];
        ObjectID oid=obj_handle_is_valid(member->actor)?obj_get_id(member->actor):(ObjectID){.type=OID_TYPE_NULL};
        int state=(member->started?1:0)|(member->ended?2:0);
        ok=tig_file_fwrite(&oid,sizeof(oid),1,file)==1
            && tig_file_fwrite(&member->points,sizeof(member->points),1,file)==1
            && tig_file_fwrite(&state,sizeof(state),1,file)==1;
    }
    int order_count=0;
    if(count)for(ObjectNode* node=combat_critter_list.head;node;node=node->next)order_count++;
    ok=ok && order_count<=4096 && tig_file_fwrite(&order_count,sizeof(order_count),1,file)==1;
    if(count)for(ObjectNode* node=combat_critter_list.head;ok && node;node=node->next){
        ObjectID oid=obj_handle_is_valid(node->obj)?obj_get_id(node->obj):(ObjectID){.type=OID_TYPE_NULL};
        ok=tig_file_fwrite(&oid,sizeof(oid),1,file)==1;
    }
    if(tig_file_fclose(file)!=0)ok=false;
    return ok;
}

static bool shared_turn_load(void)
{
    shared_turn_count=0;
    if(!tactical_enabled || !combat_turn_based_active)return true;
    TigFile* file=tig_file_fopen("Save\\Current\\modern-turns.dat","rb");
    if(file){
        int version=0,round=0,count=0;
        bool ok=tig_file_fread(&version,sizeof(version),1,file)==1 && version==1
            && tig_file_fread(&round,sizeof(round),1,file)==1 && round==combat_turn_based_turn
            && tig_file_fread(&count,sizeof(count),1,file)==1 && count>=0 && count<=SHARED_TURN_CAPACITY;
        for(int i=0;ok && i<count;i++){
            ObjectID oid;int points,state;
            ok=tig_file_fread(&oid,sizeof(oid),1,file)==1
                && tig_file_fread(&points,sizeof(points),1,file)==1
                && tig_file_fread(&state,sizeof(state),1,file)==1 && points>=0 && state>=0 && state<=3;
            if(!ok)break;
            int64_t actor=obj_pool_perm_lookup(oid);
            if(actor){
                if(shared_turn_member(actor,false)){ok=false;break;}
                SharedTurnMember* member=shared_turn_member(actor,true);
                member->points=points;member->started=(state&1)!=0;member->ended=(state&2)!=0;
            }
        }
        int order_count=0;
        int64_t order[4096];
        ok=ok && tig_file_fread(&order_count,sizeof(order_count),1,file)==1 && order_count>=0 && order_count<=4096;
        for(int i=0;ok && i<order_count;i++){
            ObjectID oid;
            ok=tig_file_fread(&oid,sizeof(oid),1,file)==1;
            order[i]=ok?obj_pool_perm_lookup(oid):OBJ_HANDLE_NULL;
        }
        tig_file_fclose(file);
        if(!ok){shared_turn_count=0;return false;}
        ObjectNode* ordered=NULL;
        ObjectNode** tail=&ordered;
        for(int i=0;i<order_count;i++){
            ObjectNode** link=&combat_critter_list.head;
            while(*link && (*link)->obj!=order[i])link=&(*link)->next;
            if(*link){
                ObjectNode* node=*link;*link=node->next;
                *tail=node;tail=&node->next;
            }
        }
        *tail=combat_critter_list.head;
        combat_critter_list.head=ordered;
    }else{
        for(ObjectNode* node=combat_critter_list.head;node && node!=dword_5FC240;node=node->next){
            if(combat_player_controls(node->obj)){
                SharedTurnMember* member=shared_turn_member(node->obj,true);
                if(member){member->started=true;member->ended=true;}
            }
        }
    }
    if(dword_5FC240 && combat_player_controls(dword_5FC240->obj)){
        SharedTurnMember* member=shared_turn_member(dword_5FC240->obj,true);
        if(!member)return false;
        member->started=true;member->ended=false;member->points=combat_action_points;
    }
    return true;
}
