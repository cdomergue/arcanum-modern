/* Creation-only customization. Native statistics, money and items enter the save.
 * No new effect, background, description, script or object identifiers.
 * Keep drafts separate from the character until the player applies them. */
static bool background_editor;
static int background_text_field;
static bool background_text_replace;
static bool background_write_failed;
static int background_story_scroll,background_story_max;
static bool background_story_follow;
static SDL_FRect background_story_rect;
#include "modern_background_loadout.inc"
#include "modern_background_choices.inc"
static void background_editor_text(const char* text,bool backspace)
{
    char* field=background_text_field==1?background_title:background_text_field==3?background_money:
        background_text_field==4?background_item_search:background_story;
    size_t cap=background_text_field==1?sizeof(background_title):background_text_field==3?sizeof(background_money):
        background_text_field==4?sizeof(background_item_search):sizeof(background_story);
    if(!background_text_field)return;
    char normalized[sizeof(background_story)];
    if(!backspace) {
        if(!text || !*text)return;
        size_t used=0;
        for(const char* p=text;*p;p++) {
            char ch=*p;
            if(ch=='\r' || ch=='\n') {
                if(background_text_field!=2)return;
                if(ch=='\r' && p[1]=='\n')p++;
                ch='\n';
            }
            if(used>=sizeof(normalized)-1)return;
            normalized[used++]=ch;
        }
        normalized[used]=0;text=normalized;
    }
    size_t retained=background_text_replace?0:strlen(field);
    if(!backspace && strlen(text)>=cap-retained)return;
    if(!backspace && background_text_field==3) {
        char candidate[sizeof(background_money)];int value;
        SDL_strlcpy(candidate,background_text_replace?"":field,sizeof(candidate));
        SDL_strlcat(candidate,text,sizeof(candidate));
        if(!background_money_value(candidate,&value))return;
    }
    if(background_text_replace)field[0]=0;
    background_text_replace=false;
    background_write_failed=false;
    size_t n=strlen(field);
    if(backspace) {if(n){do {n--;}while(n && ((unsigned char)field[n]&0xc0)==0x80);field[n]=0;}}
    else SDL_strlcat(field,text,cap);
    if(background_text_field==2)background_story_follow=true;
    if(background_text_field==4)background_item_page=0;
}
static int background_draft[8];
static int background_bonuses[2];
static bool background_derived;
static int64_t background_editor_object;
static const int background_stats[8] = {
    STAT_STRENGTH, STAT_CONSTITUTION, STAT_DEXTERITY, STAT_BEAUTY,
    STAT_INTELLIGENCE, STAT_WILLPOWER, STAT_PERCEPTION, STAT_CHARISMA
};

static void background_preview(int64_t obj,int values[16])
{
    int primary[8];
    for(int i=0;i<8;i++)primary[background_stats[i]]=stat_base_get(obj,background_stats[i])+background_draft[i];
    modern_background_preview(obj,primary,background_bonuses,values);
}

static bool background_bonus_step_allowed(int64_t obj,int which,int delta)
{
    if(!modern_background_bonuses_available())return false;
    int stat=modern_background_bonus_stats[which];
    int limit=stat_level_max(obj,stat)-stat_level_min(obj,stat);
    int wanted=background_bonuses[which]+delta;
    if(wanted < -limit || wanted > limit)return false;
    /* Allow undoing an existing adjustment even if another attribute has since
     * moved the effective value to an engine cap. */
    if((background_bonuses[which]>0 && delta<0) || (background_bonuses[which]<0 && delta>0))return true;
    int values[16];background_preview(obj,values);
    return delta<0?values[stat]>stat_level_min(obj,stat):values[stat]<stat_level_max(obj,stat);
}

static bool background_draft_valid(int64_t obj)
{
    if(!*background_title)return false;
    int money;
    if(!background_money_value(background_money,&money))return false;
    for(int i=0;i<8;i++) {
        int value=stat_base_get(obj,background_stats[i])+background_draft[i];
        if(value<stat_level_min(obj,background_stats[i])
            || value>stat_level_max(obj,background_stats[i])) return false;
    }
    return true;
}

static void background_editor_open(int64_t obj)
{
    memset(background_draft,0,sizeof(background_draft));
    for(int i=0;i<2;i++)background_bonuses[i]=modern_background_bonus_get(obj,i);
    background_derived=false;
    background_loadout_open(obj);
    background_profile_read(obj,background_title,sizeof(background_title),background_story,sizeof(background_story));
    if(background_choice_state.object!=obj || background_choice_state.selected<0) {
        if(background_get(obj)) {
            snprintf(background_title,sizeof(background_title),tr("Copie : %s","Copy: %s"),
                game_text(background_description_get_name(background_text_get(obj))));
            SDL_strlcpy(background_story,game_text(background_description_get_body(background_text_get(obj))),sizeof(background_story));
        } else {
            SDL_strlcpy(background_title,tr("Antécédent personnel","Personal background"),sizeof(background_title));
            background_story[0]=0;
        }
    }
    background_text_field=0;
    background_write_failed=false;background_story_scroll=background_story_max=0;background_story_follow=false;
    background_editor_object=obj;
    background_editor=true;
}

static bool background_editor_action(int action)
{
    if(!background_editor) return false;
    ModernCharacter c;
    if(mainmenu_ui_window_type_get()!=MM_WINDOW_NEW_CHAR
        || !modern_mainmenu_character(&c,2) || c.object!=background_editor_object) {
        background_editor=false;background_text_field=0;SDL_StopTextInput(window); return true;
    }
    if(action==591 || action==592) {background_story_scroll+=action==591?-1:1;
        if(background_story_scroll<0)background_story_scroll=0;
        if(background_story_scroll>background_story_max)background_story_scroll=background_story_max;return true;}
    if(action==589 || action==590 || action==595 || action==596) {
        background_text_field=action==589?1:action==590?2:action==595?3:4;
        if(background_text_field==2)background_story_follow=true;
        background_text_replace=false;SDL_StartTextInput(window);return true;
    }
    background_text_field=0;SDL_StopTextInput(window);
    if(action==593 || action==594 || action==599) {
        background_equipment=action==594;background_derived=action==599;return true;
    }
    if(action>=614 && action<618 && background_derived) {
        int which=(action-614)/2,delta=action%2?1:-1;
        if(background_bonus_step_allowed(c.object,which,delta))background_bonuses[which]+=delta;
        return true;
    }
    if(action==597 || action==598) {
        if(action==597 && background_item_page>0)background_item_page--;
        if(action==598 && background_item_page+1<background_item_pages)background_item_page++;
        return true;
    }
    if(action>=600 && action<614 && background_equipment) {
        int id=background_item_rows[(action-600)/2];
        if(id && background_item_allowed(id)) {
            if(action%2 && background_quantities[id]<INT_MAX)background_quantities[id]++;
            if(!(action%2) && background_quantities[id]>0)background_quantities[id]--;
        }
        return true;
    }
    if(action>=570 && action<586) {
        int i=(action-570)/2,delta=(action%2)?1:-1;
        int wanted=stat_base_get(c.object,background_stats[i])+background_draft[i]+delta;
        if(wanted>=stat_level_min(c.object,background_stats[i]) && wanted<=stat_level_max(c.object,background_stats[i]))
            background_draft[i]+=delta;
    } else if(action==586) {
        background_editor=false;
    } else if(action==587 && background_draft_valid(c.object)) {
        int before[8];
        for(int i=0;i<8;i++) before[i]=stat_base_get(c.object,background_stats[i]);
        bool ok=true;
        for(int i=0;i<8;i++) {
            int wanted=before[i]+background_draft[i];
            if(stat_base_set(c.object,background_stats[i],wanted)!=wanted) {ok=false;break;}
        }
        int delta[8];for(int i=0;i<8;i++)delta[background_stats[i]]=background_draft[i];
        if(ok && !background_choice_commit(c.object,delta,background_bonuses))ok=false;
        if(!ok) {
            background_write_failed=true;
            for(int i=0;i<8;i++) stat_base_set(c.object,background_stats[i],before[i]);
        } else {
            for(int i=0;i<2;i++)modern_background_bonus_set(c.object,i,background_bonuses[i]);
            background_loadout_commit();background_editor=false;
        }
    }
    return true;
}

static void draw_background_history(float x,float y,float w)
{
    bool focused=background_text_field==2;
    background_story_rect=(SDL_FRect){x,y,w,187};
    fill(background_story_rect,(SDL_Color){12,9,6,235});
    outline(background_story_rect,focused?GOLD:(SDL_Color){151,114,65,210});
    if(focused)outline((SDL_FRect){x+2,y+2,w-4,183},GOLD);
    if(button_count<MAX_BUTTONS)buttons[button_count++]=(Button){background_story_rect,590,true};
    SDL_FRect content={x+12,y+12,w-64,162};
    char visible[sizeof(background_story)+2];
    SDL_strlcpy(visible,background_story,sizeof(visible));
    if(focused)SDL_strlcat(visible,"|",sizeof(visible));
    const char* display=*visible?visible:tr("Écrivez ici l’histoire de votre personnage…","Write your character's history here…");
    int rows=menu_paragraph(display,(SDL_FRect){content.x,content.y,content.w,0},0,GOLD);
    background_story_max=rows-6;if(background_story_max<0)background_story_max=0;
    if(background_story_follow)background_story_scroll=background_story_max;
    background_story_follow=false;
    if(background_story_scroll>background_story_max)background_story_scroll=background_story_max;
    if(focused && background_text_replace && *background_story)
        fill(content,(SDL_Color){110,80,34,100});
    menu_paragraph(display,content,background_story_scroll,*visible?GOLD:MUTED);
    if(background_story_max>0) {
        menu_button((SDL_FRect){x+w-46,y+12,34,34},591,"^",background_story_scroll>0);
        menu_button((SDL_FRect){x+w-46,y+141,34,34},592,"v",background_story_scroll<background_story_max);
    }
    text_draw(0,tr("Entrée : nouvelle ligne","Enter: new line"),x,y+197,MUTED);
}

static void draw_background_editor(ModernCharacter* c,float x,float y,float w)
{
    text_draw(2,tr("Personnaliser l’antécédent","Customize background"),x,y,GOLD);
    float tab=(w-24)/3;
    text_in_button((SDL_FRect){x,y+42,tab,38},593,tr("Initiales","Primary"),!background_equipment && !background_derived,true);
    text_in_button((SDL_FRect){x+tab+12,y+42,tab,38},599,tr("Dérivées","Derived"),background_derived,true);
    text_in_button((SDL_FRect){x+2*(tab+12),y+42,tab,38},594,tr("Équipement","Equipment"),background_equipment,true);
    if(background_equipment)draw_background_equipment(x,y,w);
    else if(background_derived) {
        menu_paragraph(tr("Aperçu calculé. La vitesse et le nombre de compagnons acceptent aussi un bonus ou un malus, comme dans les antécédents d’origine.",
            "Calculated preview. Speed and follower limit also support a bonus or penalty, as in the original backgrounds."),
            (SDL_FRect){x,y+94,w,54},0,MUTED);
        int values[16];background_preview(c->object,values);
        for(int i=0;i<8;i++) {
            int stat=STAT_CARRY_WEIGHT+i,which=stat==STAT_SPEED?0:stat==STAT_MAX_FOLLOWERS?1:-1;
            float row=y+151+i*46;char value[64];
            text_draw(0,game_text(stat_name(stat)),x,row+9,GOLD);
            if(which>=0) {
                snprintf(value,sizeof(value),"%d  (%+d)",values[stat],background_bonuses[which]);
                menu_button((SDL_FRect){x+w-194,row,40,38},614+which*2,"-",background_bonus_step_allowed(c->object,which,-1));
                menu_button((SDL_FRect){x+w-40,row,40,38},615+which*2,"+",background_bonus_step_allowed(c->object,which,1));
            } else snprintf(value,sizeof(value),"%d",values[stat]);
            text_draw(0,value,x+w-144,row+9,GOLD);
        }
        if(!modern_background_bonuses_available())text_draw(0,tr("Effets d’origine indisponibles : aperçu uniquement.","Original effects unavailable: preview only."),x,y+524,MUTED);
    }
    else {
    menu_paragraph(tr("Ajustez librement les valeurs dans les limites du moteur. Les règles de l’antécédent sélectionné restent actives.",
        "Adjust values freely within engine limits. The selected background's rules remain active."),
        (SDL_FRect){x,y+94,w,54},0,MUTED);
    int values[16];background_preview(c->object,values);
    for(int i=0;i<8;i++) {
        float row=y+151+i*46;
        int stat=background_stats[i];
        char value[64];
        int adjustment=background_draft[i];
        if(background_choice_state.object==c->object && background_choice_state.selected>=0)
            adjustment+=background_choice_state.applied[stat];
        snprintf(value,sizeof(value),"%d  (%+d)",values[stat],adjustment);
        text_draw(0,game_text(stat_name(stat)),x,row+9,GOLD);
        int base=stat_base_get(c->object,stat)+background_draft[i];
        menu_button((SDL_FRect){x+w-194,row,40,38},570+i*2,"-",base>stat_level_min(c->object,stat));
        text_draw(0,value,x+w-144,row+9,GOLD);
        menu_button((SDL_FRect){x+w-40,row,40,38},571+i*2,"+",base<stat_level_max(c->object,stat));
    }
    }
    if(background_write_failed)text_draw(0,tr("Enregistrement impossible. Aucune modification appliquée.","Could not save. No changes applied."),x,y+554,GOLD);
    menu_button((SDL_FRect){x,y+578,w/2-8,48},586,tr("Annuler","Cancel"),true);
    menu_button((SDL_FRect){x+w/2+8,y+578,w/2-8,48},587,tr("Appliquer","Apply"),background_draft_valid(c->object));
}
