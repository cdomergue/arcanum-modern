/* Included by effect.c: native effect metadata is shared with the advanced
 * editor. Published records are immutable; old saves keep their original IDs. */
#include "game/modern_background_rules.h"
#include "game/obj_pool.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#define BG_RULE_FILE "modern-ui/background-effects.mes"
#define BG_RULE_FIRST 50
#define BG_RULE_LAST 399
#define BG_RULE_SCRATCH 1000
#define BG_RULE_CHANGES 5

typedef struct BackgroundRulePlan {
    Effect groups[BACKGROUND_RULE_IDS][6];
    int count;
} BackgroundRulePlan;
static bool background_effect_used[1000];
static char* background_effect_records[400];
static bool background_effect_store_ok;
static struct { int64_t obj; unsigned* effects; unsigned* causes; int count; } background_rule_preview;

static Effect* background_rule_table(int group)
{
    Effect* tables[]={effect_stat_effects,effect_basic_skill_effects,effect_tech_skill_effects,
        effect_resistance_effects,effect_tech_effects,effect_special_effects};
    return tables[group];
}
static const char* background_rule_key(int group,int id)
{
    const char* const* keys[]={effect_stat_lookup_tbl,effect_basic_skill_lookup_tbl,
        effect_tech_skill_lookup_tbl,effect_resistance_lookup_tbl,
        effect_tech_discipline_lookup_tbl,effect_special_attributes_lookup_tbl};
    return keys[group][id];
}
static bool background_rule_error(char* error,size_t size,int line,const char* reason)
{
    if(error && size)snprintf(error,size,"%d: %s",line,reason);
    return false;
}
static bool background_rule_parse(const char* text,BackgroundRulePlan* plan,char* error,size_t size)
{
    memset(plan,0,sizeof(*plan));
    if(error && size)*error=0;
    if(!text || strlen(text)>=BACKGROUND_RULE_TEXT)return background_rule_error(error,size,1,"text too long");
    const int counts[]={STAT_COUNT,BASIC_SKILL_COUNT,TECH_SKILL_COUNT,RESISTANCE_TYPE_COUNT,TECH_COUNT,EFFECT_SPECIAL_COUNT};
    const char* p=text;int line=1;
    while(*p) {
        while(*p && (isspace((unsigned char)*p) || *p==',')) {if(*p=='\n')line++;p++;}
        if(!*p)break;
        char key[48];int length=0;
        while(isalnum((unsigned char)*p) || *p=='_') {
            if(length==sizeof(key)-1)return background_rule_error(error,size,line,"unknown attribute");
            key[length++]=(char)tolower((unsigned char)*p++);
        }
        key[length]=0;
        int group=-1,id=-1;
        for(int g=0;g<6;g++)for(int n=0;n<counts[g];n++)if(!strcmp(key,background_rule_key(g,n))){group=g;id=n;}
        if(group<0)return background_rule_error(error,size,line,"unknown attribute");
        if(group==0 && (id==STAT_LEVEL || id==STAT_EXPERIENCE_POINTS || id==STAT_FATE_POINTS
            || id==STAT_UNSPENT_POINTS || id==STAT_POISON_LEVEL || id==STAT_GENDER || id==STAT_RACE))
            return background_rule_error(error,size,line,"attribute not supported for a background");
        while(*p==' ' || *p=='\t')p++;
        int op;bool signed_value=false;
        if(tolower((unsigned char)p[0])=='m' && tolower((unsigned char)p[1])=='i' && tolower((unsigned char)p[2])=='n' && isspace((unsigned char)p[3])){op=EFFECT_OPERATOR_MIN;p+=3;}
        else if(tolower((unsigned char)p[0])=='m' && tolower((unsigned char)p[1])=='a' && tolower((unsigned char)p[2])=='x' && isspace((unsigned char)p[3])){op=EFFECT_OPERATOR_MAX;p+=3;}
        else if(*p=='*'){op=EFFECT_OPERATOR_MULTIPLY;p++;}
        else if(*p=='/'){op=EFFECT_OPERATOR_DIVIDE;p++;}
        else if(*p=='+' || *p=='-'){op=EFFECT_OPERATOR_ADD;signed_value=true;}
        else return background_rule_error(error,size,line,"expected +, -, *, /, min or max");
        while(*p==' ' || *p=='\t')p++;
        errno=0;char* end;long value=strtol(p,&end,10);
        if(end==p || errno || value<-10000 || value>10000)return background_rule_error(error,size,line,"invalid number (-10000 to 10000)");
        p=end;
        if(*p=='%' && signed_value){op=EFFECT_OPERATOR_PERCENT;p++;}
        if(*p && *p!=',' && *p!='\n' && *p!='\r' && *p!=' ' && *p!='\t')return background_rule_error(error,size,line,"unexpected text after number");
        if((op==EFFECT_OPERATOR_DIVIDE && (value<1 || value>10))
            || (op==EFFECT_OPERATOR_MULTIPLY && (value<0 || value>10)))return background_rule_error(error,size,line,"invalid multiplier or divisor (0..10; divisor cannot be zero)");
        if(op==EFFECT_OPERATOR_PERCENT && (value<-99 || value>1000))return background_rule_error(error,size,line,"percentage must be between -99 and 1000");
        int part=0;
        while(part<BACKGROUND_RULE_IDS && plan->groups[part][group].count>=BG_RULE_CHANGES)part++;
        if(part==BACKGROUND_RULE_IDS)return background_rule_error(error,size,line,"too many effects");
        Effect* e=&plan->groups[part][group];int n=e->count++;
        e->ids[n]=id;e->operators[n]=op;e->params[n]=(int)value;
        if(plan->count<=part)plan->count=part+1;
    }
    return true;
}
bool background_rules_validate(const char* text,char* error,size_t size)
{
    BackgroundRulePlan plan;return background_rule_parse(text,&plan,error,size);
}
static bool background_rule_format(const Effect groups[6],char* text,size_t size)
{
    size_t used=0;*text=0;
    for(int g=0;g<6;g++)for(int n=0;n<groups[g].count;n++) {
        const Effect* e=&groups[g];const char* key=background_rule_key(g,e->ids[n]);
        char operation[48];int value=e->params[n];
        switch(e->operators[n]) {
        case EFFECT_OPERATOR_ADD:snprintf(operation,sizeof(operation),"%+d",value);break;
        case EFFECT_OPERATOR_PERCENT:snprintf(operation,sizeof(operation),"%+d%%",value);break;
        case EFFECT_OPERATOR_MULTIPLY:snprintf(operation,sizeof(operation),"*%d",value);break;
        case EFFECT_OPERATOR_DIVIDE:snprintf(operation,sizeof(operation),"/%d",value);break;
        case EFFECT_OPERATOR_MIN:snprintf(operation,sizeof(operation),"min %d",value);break;
        default:snprintf(operation,sizeof(operation),"max %d",value);break;
        }
        int wrote=snprintf(text+used,size-used,"%s%s %s",used?", ":"",key,operation);
        if(wrote<0 || (size_t)wrote>=size-used)return false;
        used+=(size_t)wrote;
    }
    return true;
}
bool background_rules_describe(int effect,char* text,size_t size)
{
    if(!size)return false;
    *text=0;
    if(effect<0 || effect>=1000 || !effect_initialized)return false;
    Effect groups[6];for(int g=0;g<6;g++)groups[g]=background_rule_table(g)[effect];
    return background_rule_format(groups,text,size);
}
int background_rules_adjustment(const char* text,const char* key)
{
    BackgroundRulePlan plan;if(!background_rule_parse(text,&plan,NULL,0))return 0;
    int value=0;
    for(int p=0;p<plan.count;p++)for(int g=0;g<6;g++)for(int n=0;n<plan.groups[p][g].count;n++) {
        Effect* e=&plan.groups[p][g];
        if(e->operators[n]==EFFECT_OPERATOR_ADD && !strcmp(key,background_rule_key(g,e->ids[n])))value+=e->params[n];
    }
    return value;
}
bool background_rules_step(char* text,size_t size,const char* key,int delta)
{
    BackgroundRulePlan plan;if(!background_rule_parse(text,&plan,NULL,0))return false;
    int value=background_rules_adjustment(text,key)+delta;
    if(value<-10000 || value>10000)return false;
    for(int p=0;p<plan.count;p++)for(int g=0;g<6;g++) {
        Effect* e=&plan.groups[p][g];int out=0;
        for(int n=0;n<e->count;n++)if(e->operators[n]!=EFFECT_OPERATOR_ADD || strcmp(key,background_rule_key(g,e->ids[n]))) {
            e->ids[out]=e->ids[n];e->operators[out]=e->operators[n];e->params[out++]=e->params[n];
        }
        e->count=out;
    }
    char result[BACKGROUND_RULE_TEXT]="";
    for(int p=0;p<plan.count;p++) {
        char part[BACKGROUND_RULE_TEXT];if(!background_rule_format(plan.groups[p],part,sizeof(part)))return false;
        if(*part) {
            if(strlen(result)+strlen(part)+3>=sizeof(result))return false;
            if(*result)strcat(result,", ");
            strcat(result,part);
        }
    }
    if(value) {
        size_t used=strlen(result);int n=snprintf(result+used,sizeof(result)-used,"%s%s %+d",used?", ":"",key,value);
        if(n<0 || (size_t)n>=sizeof(result)-used)return false;
    }
    if(strlen(result)>=size || !background_rules_validate(result,NULL,0))return false;
    strcpy(text,result);return true;
}
static bool background_rule_same(int id,const Effect groups[6])
{
    for(int g=0;g<6;g++)if(memcmp(&background_rule_table(g)[id],&groups[g],sizeof(Effect)))return false;
    return true;
}
static void background_rule_install(int id,const Effect groups[6])
{
    for(int g=0;g<6;g++)background_rule_table(g)[id]=groups[g];
}
static void background_rules_store_clear(void)
{
    for(int i=0;i<400;i++){free(background_effect_records[i]);background_effect_records[i]=NULL;}
    memset(background_effect_used,0,sizeof(background_effect_used));
    background_effect_store_ok=true;
}
static char* background_rule_duplicate(const char* text)
{
    size_t n=strlen(text)+1;char* copy=malloc(n);
    if(copy)memcpy(copy,text,n);
    return copy;
}
static bool background_rules_store_load(void)
{
    FILE* f=fopen(BG_RULE_FILE,"rb");
    if(!f)return errno==ENOENT;
    char line[BACKGROUND_RULE_TEXT+64];bool ok=true;
    while(fgets(line,sizeof(line),f)) {
        if(line[0]=='\n' || line[0]=='\r' || line[0]=='#')continue;
        char* end;long id=strtol(line+1,&end,10);
        if(line[0]!='{' || end==line+1 || id<BG_RULE_FIRST || id>BG_RULE_LAST || strncmp(end,"}{",2)) {ok=false;break;}
        char* text=end+2;char* close=strchr(text,'}');
        if(!close || (close[1] && close[1]!='\r' && close[1]!='\n')){ok=false;break;}
        *close=0;BackgroundRulePlan plan;
        if(background_effect_records[id] || !background_rule_parse(text,&plan,NULL,0) || plan.count!=1
            || (background_effect_used[id] && !background_rule_same((int)id,plan.groups[0]))) {ok=false;break;}
        background_effect_records[id]=background_rule_duplicate(text);
        if(!background_effect_records[id]){ok=false;break;}
        background_rule_install((int)id,plan.groups[0]);background_effect_used[id]=true;
    }
    if(ferror(f))ok=false;
    fclose(f);return ok;
}
bool background_rules_available(const BackgroundRules* rules)
{
    if(!background_effect_store_ok || rules->count<0 || rules->count>BACKGROUND_RULE_IDS)return false;
    BackgroundRulePlan plan;
    if(!background_rule_parse(rules->text,&plan,NULL,0) || plan.count!=rules->count)return false;
    for(int i=0;i<rules->count;i++)if(rules->ids[i]<50 || rules->ids[i]>=1000
        || !background_effect_used[rules->ids[i]] || !background_rule_same(rules->ids[i],plan.groups[i]))return false;
    return true;
}
bool background_rules_prepare(BackgroundRules* rules,char* error,size_t size)
{
    BackgroundRulePlan plan;
    if(!background_rule_parse(rules->text,&plan,error,size))return false;
    if(!background_effect_store_ok)return background_rule_error(error,size,1,"effect resource unavailable");
    int ids[BACKGROUND_RULE_IDS]={0};char* additions[BACKGROUND_RULE_IDS]={0};bool ok=true;
    for(int part=0;part<plan.count;part++) {
        for(int id=50;id<1000;id++)if(id!=297 && id!=298 && id!=307 && id!=308 && background_effect_used[id] && background_rule_same(id,plan.groups[part])){ids[part]=id;break;}
        if(ids[part])continue;
        for(int prior=0;prior<part;prior++)if(!memcmp(plan.groups[prior],plan.groups[part],sizeof(plan.groups[part]))){ids[part]=ids[prior];break;}
        if(ids[part])continue;
        for(int id=BG_RULE_LAST;id>=BG_RULE_FIRST;id--) {
            bool reserved=background_effect_used[id];for(int prior=0;prior<part;prior++)if(ids[prior]==id)reserved=true;
            if(!reserved){ids[part]=id;break;}
        }
        if(!ids[part]){background_rule_error(error,size,1,"no free native effect IDs; existing saves are preserved");ok=false;break;}
        char text[BACKGROUND_RULE_TEXT];
        if(!background_rule_format(plan.groups[part],text,sizeof(text)) || !(additions[part]=background_rule_duplicate(text))){ok=false;break;}
    }
    bool changed=false;for(int i=0;i<plan.count;i++)if(additions[i])changed=true;
    if(ok && changed) {
        FILE* f=fopen(BG_RULE_FILE ".tmp","wb");ok=f!=NULL;
        if(f) {
            for(int id=BG_RULE_FIRST;ok && id<=BG_RULE_LAST;id++)if(background_effect_records[id])
                ok=fprintf(f,"{%d}{%s}\n",id,background_effect_records[id])>0;
            for(int i=0;ok && i<plan.count;i++)if(additions[i])ok=fprintf(f,"{%d}{%s}\n",ids[i],additions[i])>0;
            if(fclose(f)!=0)ok=false;
            if(ok)ok=SDL_RenamePath(BG_RULE_FILE ".tmp",BG_RULE_FILE);
        }
    }
    if(ok) {
        rules->count=plan.count;memcpy(rules->ids,ids,sizeof(ids));
        for(int i=0;i<plan.count;i++)if(additions[i]) {
            background_effect_records[ids[i]]=additions[i];additions[i]=NULL;
            background_rule_install(ids[i],plan.groups[i]);background_effect_used[ids[i]]=true;
        }
    }
    for(int i=0;i<BACKGROUND_RULE_IDS;i++)free(additions[i]);
    if(!ok && error && size && !*error)background_rule_error(error,size,1,"cannot save effect resource");
    return ok;
}
void background_rules_remove(int64_t obj,const BackgroundRules* rules)
{
    for(int i=0;i<rules->count;i++)effect_remove_one_with_cause(obj,rules->ids[i],EFFECT_CAUSE_BACKGROUND);
}
static bool background_rule_present(int64_t obj,int effect)
{
    for(int i=0;i<obj_arrayfield_length_get(obj,OBJ_F_CRITTER_EFFECTS_IDX);i++)
        if(obj_arrayfield_uint32_get(obj,OBJ_F_CRITTER_EFFECTS_IDX,i)==(unsigned)effect
            && obj_arrayfield_uint32_get(obj,OBJ_F_CRITTER_EFFECT_CAUSE_IDX,i)==EFFECT_CAUSE_BACKGROUND)return true;
    return false;
}
void background_rules_apply(int64_t obj,int base_effect,const BackgroundRules* rules)
{
    if(rules->inherit) {if(!background_rule_present(obj,base_effect))effect_add(obj,base_effect,EFFECT_CAUSE_BACKGROUND);}
    else effect_remove_one_with_cause(obj,base_effect,EFFECT_CAUSE_BACKGROUND);
    for(int i=0;i<rules->count;i++)effect_add(obj,rules->ids[i],EFFECT_CAUSE_BACKGROUND);
}
bool background_rules_preview_begin(int64_t obj,int base_effect,const BackgroundRules* old_rules,const char* text,bool inherit)
{
    BackgroundRulePlan plan;
    if(background_rule_preview.obj || !background_rule_parse(text,&plan,NULL,0))return false;
    int n=obj_arrayfield_length_get(obj,OBJ_F_CRITTER_EFFECTS_IDX);
    unsigned* effects=malloc((size_t)(n+1)*sizeof(unsigned));unsigned* causes=malloc((size_t)(n+1)*sizeof(unsigned));
    if(!effects || !causes){free(effects);free(causes);return false;}
    for(int i=0;i<n;i++){effects[i]=obj_arrayfield_uint32_get(obj,OBJ_F_CRITTER_EFFECTS_IDX,i);causes[i]=obj_arrayfield_uint32_get(obj,OBJ_F_CRITTER_EFFECT_CAUSE_IDX,i);}
    background_rule_preview.obj=obj;background_rule_preview.count=n;background_rule_preview.effects=effects;background_rule_preview.causes=causes;
    int remain[BACKGROUND_RULE_IDS]={0};if(old_rules)for(int i=0;i<old_rules->count;i++)remain[i]=1;
    int out=0;bool base_seen=false;
    for(int i=0;i<n;i++) {
        bool omit=false;
        if(causes[i]==EFFECT_CAUSE_BACKGROUND) {
            if(old_rules)for(int j=0;j<old_rules->count;j++)if(remain[j] && effects[i]==(unsigned)old_rules->ids[j]){remain[j]=0;omit=true;break;}
            if(!omit && effects[i]==(unsigned)base_effect){base_seen=true;if(!inherit)omit=true;}
        }
        if(!omit){obj_arrayfield_uint32_set(obj,OBJ_F_CRITTER_EFFECTS_IDX,out,effects[i]);obj_arrayfield_uint32_set(obj,OBJ_F_CRITTER_EFFECT_CAUSE_IDX,out++,causes[i]);}
    }
    for(int i=n-1;i>=out;i--){obj_arrayfield_length_set(obj,OBJ_F_CRITTER_EFFECTS_IDX,i);obj_arrayfield_length_set(obj,OBJ_F_CRITTER_EFFECT_CAUSE_IDX,i);}
    if(inherit && !base_seen){obj_arrayfield_uint32_set(obj,OBJ_F_CRITTER_EFFECTS_IDX,out,base_effect);obj_arrayfield_uint32_set(obj,OBJ_F_CRITTER_EFFECT_CAUSE_IDX,out++,EFFECT_CAUSE_BACKGROUND);}
    for(int i=0;i<plan.count;i++) {
        background_rule_install(BG_RULE_SCRATCH+i,plan.groups[i]);
        obj_arrayfield_uint32_set(obj,OBJ_F_CRITTER_EFFECTS_IDX,out,BG_RULE_SCRATCH+i);
        obj_arrayfield_uint32_set(obj,OBJ_F_CRITTER_EFFECT_CAUSE_IDX,out++,EFFECT_CAUSE_BACKGROUND);
    }
    return true;
}
void background_rules_preview_end(int64_t obj)
{
    if(background_rule_preview.obj!=obj)return;
    int n=obj_arrayfield_length_get(obj,OBJ_F_CRITTER_EFFECTS_IDX);
    for(int i=n-1;i>=0;i--){obj_arrayfield_length_set(obj,OBJ_F_CRITTER_EFFECTS_IDX,i);obj_arrayfield_length_set(obj,OBJ_F_CRITTER_EFFECT_CAUSE_IDX,i);}
    for(int i=0;i<background_rule_preview.count;i++) {
        obj_arrayfield_uint32_set(obj,OBJ_F_CRITTER_EFFECTS_IDX,i,background_rule_preview.effects[i]);
        obj_arrayfield_uint32_set(obj,OBJ_F_CRITTER_EFFECT_CAUSE_IDX,i,background_rule_preview.causes[i]);
    }
    free(background_rule_preview.effects);free(background_rule_preview.causes);memset(&background_rule_preview,0,sizeof(background_rule_preview));
}
bool background_rules_saved_objects_valid(void)
{
    int64_t obj;int iter;
    if(obj_pool_walk_first(&obj,&iter))do {
        if(!obj_type_is_critter(obj_field_int32_get(obj,OBJ_F_TYPE)))continue;
        for(int i=0;i<obj_arrayfield_length_get(obj,OBJ_F_CRITTER_EFFECTS_IDX);i++) {
            unsigned id=obj_arrayfield_uint32_get(obj,OBJ_F_CRITTER_EFFECTS_IDX,i);
            if(id>=1000 || (obj_arrayfield_uint32_get(obj,OBJ_F_CRITTER_EFFECT_CAUSE_IDX,i)==EFFECT_CAUSE_BACKGROUND && !background_effect_used[id])) {
                tig_debug_printf("Background effect %u is missing. Restore modern-ui/background-effects.mes and matching game resources before loading this save.\n",id);return false;
            }
        }
    }while(obj_pool_walk_next(&obj,&iter));
    return true;
}
