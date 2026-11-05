/* Creation draft only. Inventory is rebuilt by item_generate_inventory before
 * each shopping visit. No custom prototype or serialized extension is used. */
#define BACKGROUND_ITEM_LIMIT 16066
static struct {
    int64_t object;
    char money[11];
    int quantities[BACKGROUND_ITEM_LIMIT];
    bool failed;
} background_loadout;
static char background_money[11],background_item_search[96];
static int background_quantities[BACKGROUND_ITEM_LIMIT];
static int background_item_page,background_item_pages;
static bool background_equipment;

/* Stock 1.0.7.4 prototype intervals, excluding currency (edited separately).
 * Resolve names and objects from the installed game, including UAP overrides.
 * Mod-only prototypes must not leak into a save destined for the base game. */
static bool background_item_allowed(int id)
{
    static const int ranges[][2]={{6029,6179},{7038,7042},{8042,8293},
        {10057,10145},{11059,11143},{12060,12063},{13061,13062},
        {14062,14130},{15065,15213}};
    for(size_t i=0;i<sizeof(ranges)/sizeof(ranges[0]);i++) {
        if(id>=ranges[i][0] && id<ranges[i][1]) {
            int64_t proto=sub_4685A0(id);
            return proto!=OBJ_HANDLE_NULL && item_is_item(proto);
        }
    }
    return false;
}

static bool background_money_value(const char* text,int* value)
{
    int n=0;
    for(const char* p=text;*p;p++) {
        if(*p<'0' || *p>'9' || n>(INT_MAX-(*p-'0'))/10)return false;
        n=n*10+(*p-'0');
    }
    *value=n;return true;
}

void modern_background_loadout_begin(int64_t previous,int64_t current)
{
    if(!previous || previous!=background_loadout.object)
        memset(&background_loadout,0,sizeof(background_loadout));
    background_loadout.object=current;
}

static void background_loadout_open(int64_t obj)
{
    if(background_loadout.object!=obj)modern_background_loadout_begin(0,obj);
    SDL_strlcpy(background_money,background_loadout.money,sizeof(background_money));
    memcpy(background_quantities,background_loadout.quantities,sizeof(background_quantities));
    background_item_search[0]=0;background_item_page=0;background_equipment=false;
}

static void background_loadout_commit(void)
{
    SDL_strlcpy(background_loadout.money,background_money,sizeof(background_loadout.money));
    memcpy(background_loadout.quantities,background_quantities,sizeof(background_quantities));
    background_loadout.failed=false;
}

/* Called only after the native inventory reset, never on save load. Returning
 * false sends the player back to creation instead of starting with a partial kit. */
bool modern_background_loadout_generate(int64_t obj)
{
    if(obj!=background_loadout.object)return true;
    background_loadout.failed=true;
    for(int id=0;id<BACKGROUND_ITEM_LIMIT;id++) {
        int count=background_loadout.quantities[id];
        if(!count)continue;
        if(count<0 || !background_item_allowed(id))return false;
        int64_t proto=sub_4685A0(id);
        int quantity_field;
        int stack_field=sub_462410(proto,&quantity_field);
        if(stack_field!=-1) {
            int64_t stack=obj_field_handle_get(obj,stack_field);
            int have=stack?obj_field_int32_get(stack,quantity_field):0;
            if(have<0 || count>INT_MAX-have)return false;
        }
        int copies=stack_field!=-1?1:count;
        for(int i=0;i<copies;i++) {
            int64_t item=OBJ_HANDLE_NULL;
            if(!object_create(proto,obj_field_int64_get(obj,OBJ_F_LOCATION),&item))return false;
            if(stack_field!=-1)obj_field_int32_set(item,quantity_field,count);
            obj_field_int32_set(item,OBJ_F_ITEM_FLAGS,obj_field_int32_get(item,OBJ_F_ITEM_FLAGS)|OIF_IDENTIFIED);
            if(!item_transfer(item,obj)) {object_destroy(item);return false;}
        }
    }
    if(*background_loadout.money) {
        int wanted;
        if(!background_money_value(background_loadout.money,&wanted))return false;
        int have=item_gold_get(obj);
        if(have<0)return false;
        if(wanted>have) {
            if(!item_gold_transfer(OBJ_HANDLE_NULL,obj,wanted-have,OBJ_HANDLE_NULL))return false;
        } else if(wanted<have) {
            if(!item_gold_transfer(obj,OBJ_HANDLE_NULL,have-wanted,OBJ_HANDLE_NULL))return false;
        }
        if(item_gold_get(obj)!=wanted)return false;
    }
    background_loadout.failed=false;
    return true;
}

static int background_item_rows[7];
static void draw_background_equipment(float x,float y,float w)
{
    text_draw(0,tr("Argent de départ (vide = antécédent)","Starting money (empty = background)"),x,y+99,MUTED);
    text_in_button((SDL_FRect){x+w-195,y+86,195,42},595,background_money,background_text_field==3,true);
    text_in_button((SDL_FRect){x,y+145,w,42},596,*background_item_search?background_item_search:
        tr("Rechercher un objet…","Search items…"),background_text_field==4,true);
    int count=0;
    for(int id=0;id<BACKGROUND_ITEM_LIMIT;id++)if(background_item_allowed(id)) {
        const char* name=game_text(description_get(obj_field_int32_get(sub_4685A0(id),OBJ_F_DESCRIPTION)));
        if(!*background_item_search || SDL_strcasestr(name,background_item_search))count++;
    }
    background_item_pages=(count+6)/7;
    if(background_item_page>=background_item_pages)background_item_page=background_item_pages-1;
    if(background_item_page<0)background_item_page=0;
    memset(background_item_rows,0,sizeof(background_item_rows));
    int index=0,row=0;
    for(int id=0;id<BACKGROUND_ITEM_LIMIT && row<7;id++)if(background_item_allowed(id)) {
        const char* name=game_text(description_get(obj_field_int32_get(sub_4685A0(id),OBJ_F_DESCRIPTION)));
        if(*background_item_search && !SDL_strcasestr(name,background_item_search))continue;
        if(index++<background_item_page*7)continue;
        background_item_rows[row]=id;
        float ry=y+201+row*40;
        menu_paragraph(name,(SDL_FRect){x,ry+8,w-205,28},0,GOLD);
        menu_button((SDL_FRect){x+w-190,ry,36,36},600+row*2,"-",background_quantities[id]>0);
        char quantity[24];snprintf(quantity,sizeof(quantity),"%d",background_quantities[id]);
        text_draw(0,quantity,x+w-146,ry+8,GOLD);
        menu_button((SDL_FRect){x+w-36,ry,36,36},601+row*2,"+",background_quantities[id]<INT_MAX);
        row++;
    }
    menu_button((SDL_FRect){x,y+490,42,36},597,"<",background_item_page>0);
    char page[40];snprintf(page,sizeof(page),"%d / %d",count?background_item_page+1:0,background_item_pages);
    text_draw(0,page,x+58,y+499,MUTED);
    menu_button((SDL_FRect){x+w-42,y+490,42,36},598,">",background_item_page+1<background_item_pages);
    text_draw(0,tr("Quantités ajoutées à l’équipement habituel.","Quantities added to the usual equipment."),x,y+535,MUTED);
}
