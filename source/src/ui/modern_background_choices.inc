/* Reusable definitions stay outside game saves. Characters retain the original
 * background ID and reference immutable native-format effects when needed. */
typedef struct BackgroundChoice {
    int base, primary[8], bonuses[2];
    BackgroundRules rules;
    char title[96], story[1024], money[11];
    int quantities[BACKGROUND_ITEM_LIMIT];
} BackgroundChoice;
static BackgroundChoice* background_choices;
static int background_choice_count;
static bool background_choices_loaded,background_choices_failed;
static int background_choices_version=2;
static BackgroundRules background_rules_draft={.inherit=true};
static struct {
    int64_t object;
    int selected, applied[8];
} background_choice_state={0,-1,{0}};

/* Explicit little-endian fields, no compiler padding, pointers or game IDs
 * allocated by this file. Sparse inventory keeps empty definitions small. */
static bool background_choice_number(FILE* f,int* value,bool write)
{
    unsigned char b[4];
    if(write) {
        uint32_t v=(uint32_t)*value;
        for(int i=0;i<4;i++)b[i]=(unsigned char)(v>>(8*i));
        return fwrite(b,1,4,f)==4;
    }
    if(fread(b,1,4,f)!=4)return false;
    uint32_t v=0;for(int i=0;i<4;i++)v|=(uint32_t)b[i]<<(8*i);
    *value=(int32_t)v;return true;
}
static bool background_choice_record(FILE* f,BackgroundChoice* c,bool write)
{
    if(!background_choice_number(f,&c->base,write))return false;
    for(int i=0;i<8;i++)if(!background_choice_number(f,&c->primary[i],write))return false;
    for(int i=0;i<2;i++)if(!background_choice_number(f,&c->bonuses[i],write))return false;
    char* fields[]={c->title,c->story,c->money};
    size_t sizes[]={sizeof(c->title),sizeof(c->story),sizeof(c->money)};
    for(int i=0;i<3;i++) {
        if(write) {if(fwrite(fields[i],1,sizes[i],f)!=sizes[i])return false;}
        else if(fread(fields[i],1,sizes[i],f)!=sizes[i] || !memchr(fields[i],0,sizes[i]))return false;
    }
    int count=0;
    if(write)for(int i=0;i<BACKGROUND_ITEM_LIMIT;i++)if(c->quantities[i])count++;
    if(!background_choice_number(f,&count,write) || count<0 || count>BACKGROUND_ITEM_LIMIT)return false;
    if(write) {
        for(int id=0;id<BACKGROUND_ITEM_LIMIT;id++)if(c->quantities[id])
            if(!background_choice_number(f,&id,true) || !background_choice_number(f,&c->quantities[id],true))return false;
    } else {
        for(int i=0;i<count;i++) {
            int id,n;
            if(!background_choice_number(f,&id,false) || !background_choice_number(f,&n,false)
                || id<0 || id>=BACKGROUND_ITEM_LIMIT || n<=0 || c->quantities[id])return false;
            c->quantities[id]=n;
        }
    }
    if(write || background_choices_version>=2) {
        int inherit=c->rules.inherit?1:0;
        if(!background_choice_number(f,&inherit,write) || inherit<0 || inherit>1)return false;
        c->rules.inherit=inherit!=0;
        if(!background_choice_number(f,&c->rules.count,write) || c->rules.count<0 || c->rules.count>BACKGROUND_RULE_IDS)return false;
        for(int i=0;i<c->rules.count;i++)if(!background_choice_number(f,&c->rules.ids[i],write) || c->rules.ids[i]<50 || c->rules.ids[i]>=1000)return false;
        if(write) {if(fwrite(c->rules.text,1,sizeof(c->rules.text),f)!=sizeof(c->rules.text))return false;}
        else if(fread(c->rules.text,1,sizeof(c->rules.text),f)!=sizeof(c->rules.text) || !memchr(c->rules.text,0,sizeof(c->rules.text)))return false;
        if(!background_rules_validate(c->rules.text,NULL,0))return false;
    } else c->rules.inherit=true;
    int money;
    if(c->base<0 || !*c->title || !background_money_value(c->money,&money))return false;
    for(int i=0;i<8;i++)if(c->primary[i]<-100 || c->primary[i]>100)return false;
    return c->bonuses[0]>=-99 && c->bonuses[0]<=99 && c->bonuses[1]>=-6 && c->bonuses[1]<=6;
}
static void background_choices_load(void)
{
    if(background_choices_loaded)return;
    background_choices_loaded=true;
    FILE* f=fopen("modern-ui/backgrounds.dat","rb");
    if(!f){background_choices_failed=errno!=ENOENT;return;}
    char magic[8];int count=0;
    bool ok=fread(magic,1,8,f)==8 && (!memcmp(magic,"ARCBG01\n",8) || !memcmp(magic,"ARCBG02\n",8));
    background_choices_version=ok && magic[6]=='2'?2:1;
    ok=ok && background_choice_number(f,&count,false) && count>=0;
    /* Bound allocations by actual bytes, including the smallest empty record. */
    long start=ftell(f),end=-1;
    if(fseek(f,0,SEEK_END)==0)end=ftell(f);
    if(start<0 || end<start || fseek(f,start,SEEK_SET)!=0 || count>(end-start)/1179)ok=false;
    BackgroundChoice* loaded=ok && count?calloc((size_t)count,sizeof(*loaded)):NULL;
    if(count && !loaded)ok=false;
    for(int i=0;ok && i<count;i++)ok=background_choice_record(f,&loaded[i],false);
    if(ok && fgetc(f)!=EOF)ok=false;
    if(ferror(f))ok=false;
    fclose(f);
    if(!ok){free(loaded);background_choices_failed=true;return;}
    background_choices=loaded;background_choice_count=count;
}
static bool background_choices_save(BackgroundChoice* candidate,int replace)
{
    if(background_choices_failed)return false;
    int count=background_choice_count+(replace<0);
    FILE* f=fopen("modern-ui/backgrounds.dat.tmp","wb");if(!f)return false;
    bool ok=fwrite("ARCBG02\n",1,8,f)==8 && background_choice_number(f,&count,true);
    for(int i=0;ok && i<count;i++)ok=background_choice_record(f,
        i==replace || i==background_choice_count?candidate:&background_choices[i],true);
    if(fclose(f)!=0)ok=false;
    return ok && SDL_RenamePath("modern-ui/backgrounds.dat.tmp","modern-ui/backgrounds.dat");
}
static void background_choice_profile(int64_t obj,const BackgroundChoice* c)
{
    background_pending_profile.object=obj;background_pending_profile.active=true;
    SDL_strlcpy(background_pending_profile.title,c?c->title:"",sizeof(background_pending_profile.title));
    SDL_strlcpy(background_pending_profile.story,c?c->story:"",sizeof(background_pending_profile.story));
    modern_background_creation_ready(obj);
}
void modern_background_choices_begin(int64_t previous,int64_t current)
{
    background_choices_load();
    if(!previous || previous!=background_choice_state.object) {
        background_choice_state.selected=-1;
        memset(background_choice_state.applied,0,sizeof(background_choice_state.applied));
    }
    background_choice_state.object=current;
}
/* Remove exactly the raw changes made on this creation PC, even when a racial
 * cap clamped a definition. The original must not inherit a copy's adjustments. */
void modern_background_choice_leave(int64_t obj)
{
    if(background_choice_state.object!=obj || background_choice_state.selected<0)return;
    const BackgroundRules* old_rules=&background_choices[background_choice_state.selected].rules;
    background_rules_remove(obj,old_rules);
    if(!old_rules->inherit) {
        BackgroundRules original={.inherit=true};
        background_rules_apply(obj,background_effect_get(background_get(obj)),&original);
    }
    for(int i=0;i<8;i++)obj_arrayfield_int32_set(obj,OBJ_F_CRITTER_STAT_BASE_IDX,i,
        stat_base_get(obj,i)-background_choice_state.applied[i]);
    for(int i=0;i<2;i++)modern_background_bonus_set(obj,i,0);
    memset(background_choice_state.applied,0,sizeof(background_choice_state.applied));
    background_choice_state.selected=-1;
    modern_background_loadout_begin(0,obj);
    background_choice_profile(obj,NULL);
}
static bool background_choice_usable(const BackgroundChoice* c)
{
    if(!background_rules_available(&c->rules))return false;
    if((c->bonuses[0] || c->bonuses[1]) && !modern_background_bonuses_available())return false;
    for(int i=0;i<BACKGROUND_ITEM_LIMIT;i++)if(c->quantities[i] && !background_item_allowed(i))return false;
    return true;
}
static void background_choice_select(int64_t obj,int base,int selected)
{
    modern_background_choice_leave(obj);
    background_clear(obj);background_set(obj,base,background_get_description(base));
    if(selected<0)return;
    const BackgroundChoice* c=&background_choices[selected];
    for(int i=0;i<8;i++) {
        int before=stat_base_get(obj,i);
        int after=c->primary[i]?stat_base_set(obj,i,before+c->primary[i]):before;
        background_choice_state.applied[i]=after-before;
    }
    for(int i=0;i<2;i++)modern_background_bonus_set(obj,i,c->bonuses[i]);
    background_rules_apply(obj,background_effect_get(base),&c->rules);
    background_choice_state.selected=selected;
    SDL_strlcpy(background_loadout.money,c->money,sizeof(background_loadout.money));
    memcpy(background_loadout.quantities,c->quantities,sizeof(background_loadout.quantities));
    background_loadout.failed=false;
    background_choice_profile(obj,c);
}
/* Order: original, its copies in creation order, next original. Native race and
 * gender eligibility is inherited by each copy. No synthetic ID enters the PC. */
bool modern_background_choice_cycle(int64_t obj,int direction)
{
    background_choices_load();
    if(!background_choice_count)return false;
    if(background_choice_state.object!=obj)modern_background_choices_begin(0,obj);
    int base=background_get(obj),selected=background_choice_state.selected;
    if(direction>0) {
        for(int i=selected+1;i<background_choice_count;i++)
            if(background_choices[i].base==base && background_choice_usable(&background_choices[i])) {
                background_choice_select(obj,base,i);return true;
            }
        if(!background_find_next(obj,&base) && !background_find_first(obj,&base))return true;
        background_choice_select(obj,base,-1);
    } else {
        if(selected<0) {
            if(!background_find_prev(obj,&base)) {
                if(!background_find_first(obj,&base))return true;
                while(background_find_next(obj,&base)){}
            }
            selected=background_choice_count;
        }
        for(int i=selected-1;i>=0;i--)
            if(background_choices[i].base==base && background_choice_usable(&background_choices[i])) {
                background_choice_select(obj,base,i);return true;
            }
        background_choice_select(obj,base,-1);
    }
    return true;
}
/* The editor already applied and checked the raw draft. Persist the definition
 * atomically before making it the selected entry; a failure leaves the list intact. */
static bool background_choice_commit(int64_t obj,const int delta[8],const int bonuses[2])
{
    background_choices_load();
    if(background_choice_state.object!=obj)modern_background_choices_begin(0,obj);
    BackgroundChoice* c=calloc(1,sizeof(*c));if(!c)return false;
    c->base=background_get(obj);
    c->rules=background_rules_draft;
    for(int i=0;i<8;i++)c->primary[i]=background_choice_state.applied[i]+delta[i];
    memcpy(c->bonuses,bonuses,sizeof(c->bonuses));
    SDL_strlcpy(c->title,background_title,sizeof(c->title));
    SDL_strlcpy(c->story,background_story,sizeof(c->story));
    SDL_strlcpy(c->money,background_money,sizeof(c->money));
    memcpy(c->quantities,background_quantities,sizeof(c->quantities));
    int selected=background_choice_state.selected;
    if(selected<0) {
        BackgroundChoice* grown=realloc(background_choices,((size_t)background_choice_count+1)*sizeof(*grown));
        if(!grown){free(c);return false;}background_choices=grown;
    }
    if(!background_choices_save(c,selected)){free(c);return false;}
    if(selected<0)selected=background_choice_count++;
    background_choices[selected]=*c;
    background_choice_state.selected=selected;
    memcpy(background_choice_state.applied,c->primary,sizeof(c->primary));
    background_choice_profile(obj,c);free(c);return true;
}
