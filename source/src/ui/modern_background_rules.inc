static int background_rules_mode,background_rules_page,background_rules_scroll,background_rules_max_scroll;
static SDL_FRect background_rules_rect;
static char background_rules_error[256];
typedef struct BackgroundRuleControl {const char* key;int kind,id;const char* fr;const char* en;} BackgroundRuleControl;
static const BackgroundRuleControl background_rule_controls[]={
    {"speed",0,STAT_SPEED,NULL,NULL},{"maxfollowers",0,STAT_MAX_FOLLOWERS,NULL,NULL},
    {"alignment",0,STAT_ALIGNMENT,NULL,NULL},{"aptitude",0,STAT_MAGICK_TECH_APTITUDE,NULL,NULL},
    {"magicpts",0,STAT_MAGICK_POINTS,NULL,NULL},{"techpts",0,STAT_TECH_POINTS,NULL,NULL},
    {"maxhps",5,0,"Vie maximale","Maximum health"},{"maxfatigue",5,1,"Fatigue maximale","Maximum fatigue"},
    {"reaction",5,2,"Réaction générale","General reaction"},{"goodreactionadj",5,3,"Réactions positives","Positive reactions"},
    {"badreactionadj",5,4,"Réactions négatives","Negative reactions"},{"beautyreaction",0,STAT_REACTION_MODIFIER,NULL,NULL},
    {"carry",0,STAT_CARRY_WEIGHT,NULL,NULL},{"damage",0,STAT_DAMAGE_BONUS,NULL,NULL},
    {"ac",0,STAT_AC_ADJUSTMENT,NULL,NULL},{"healrate",0,STAT_HEAL_RATE,NULL,NULL},
    {"poisonrate",0,STAT_POISON_RECOVERY,NULL,NULL},{"age",0,STAT_AGE,NULL,NULL},
    {"xpgain",5,5,"Gain d’expérience","Experience gain"},{"crithitchance",5,6,"Chance de coup critique","Critical hit chance"},
    {"crithiteffect",5,7,"Effet des coups critiques","Critical hit severity"},{"critfailchance",5,8,"Chance d’échec critique","Critical failure chance"},
    {"critfaileffect",5,9,"Effet des échecs critiques","Critical failure severity"},
    {"resistdamage",5,10,"Résistance physique","Physical resistance"},{"resistfire",5,11,"Résistance au feu","Fire resistance"},
    {"resistelectrical",5,12,"Résistance électrique","Electrical resistance"},{"resistpoison",5,13,"Résistance au poison","Poison resistance"},
    {"resistmagic",5,14,"Résistance magique","Magic resistance"},
    {"bow",1,0,NULL,NULL},{"dodge",1,1,NULL,NULL},{"melee",1,2,NULL,NULL},{"throwing",1,3,NULL,NULL},
    {"backstab",1,4,NULL,NULL},{"pickpocket",1,5,NULL,NULL},{"prowling",1,6,NULL,NULL},{"spottrap",1,7,NULL,NULL},
    {"gambling",1,8,NULL,NULL},{"haggle",1,9,NULL,NULL},{"heal",1,10,NULL,NULL},{"persuasion",1,11,NULL,NULL},
    {"repair",2,0,NULL,NULL},{"firearms",2,1,NULL,NULL},{"picklock",2,2,NULL,NULL},{"armtrap",2,3,NULL,NULL},
    {"expertiseanatomical",4,0,NULL,NULL},{"expertisechemistry",4,1,NULL,NULL},{"expertiseelectric",4,2,NULL,NULL},
    {"expertiseexplosives",4,3,NULL,NULL},{"expertisegun_smithy",4,4,NULL,NULL},{"expertisemechanical",4,5,NULL,NULL},
    {"expertisesmithy",4,6,NULL,NULL},{"expertisetherapeutics",4,7,NULL,NULL},
};
#define BACKGROUND_RULE_CONTROL_COUNT ((int)(sizeof(background_rule_controls)/sizeof(background_rule_controls[0])))

static const char* background_rule_label(const BackgroundRuleControl* c)
{
    switch(c->kind) {
    case 0:return game_text(stat_name(c->id));
    case 1:return game_text(basic_skill_name(c->id));
    case 2:return game_text(tech_skill_name(c->id));
    case 4:return game_text(tech_discipline_name_get(c->id));
    default:return tr(c->fr,c->en);
    }
}
static const BackgroundRules* background_current_rules(int64_t obj)
{
    if(background_choice_state.object==obj && background_choice_state.selected>=0)
        return &background_choices[background_choice_state.selected].rules;
    return NULL;
}
static bool background_rules_action(int action)
{
    if(action==618 || action==619 || action==620) {
        background_rules_mode=action==618?1:action==619?2:3;
        background_equipment=false;background_rules_scroll=0;return true;
    }
    if(!background_rules_mode)return false;
    if(action==621){background_rules_draft.inherit=!background_rules_draft.inherit;return true;}
    if(action==622){background_text_field=5;background_text_replace=false;SDL_StartTextInput(window);return true;}
    if(action==623 || action==624) {
        int last=(BACKGROUND_RULE_CONTROL_COUNT-1)/6;
        if(action==623 && background_rules_page>0)background_rules_page--;
        if(action==624 && background_rules_page<last)background_rules_page++;
        return true;
    }
    if(action==625 || action==626) {
        background_rules_scroll+=action==625?-1:1;
        if(background_rules_scroll<0)background_rules_scroll=0;
        if(background_rules_scroll>background_rules_max_scroll)background_rules_scroll=background_rules_max_scroll;
        return true;
    }
    if(action>=630 && action<642 && background_rules_mode==1) {
        int index=background_rules_page*6+(action-630)/2;
        if(index<BACKGROUND_RULE_CONTROL_COUNT) {
            const BackgroundRuleControl* c=&background_rule_controls[index];
            int step=!strcmp(c->key,"alignment")?10:1;
            background_rules_step(background_rules_draft.text,sizeof(background_rules_draft.text),c->key,(action%2)?step:-step);
        }
        return true;
    }
    return false;
}
static const char* background_rule_error_label(const char* error)
{
    const struct {const char* reason;const char* fr;} messages[]={
        {"text too long","Texte trop long."},{"unknown attribute","Attribut inconnu."},
        {"attribute not supported for a background","Cet attribut ne convient pas à un antécédent."},
        {"expected +, -, *, /, min or max","Opérateur attendu : +, -, *, /, min ou max."},
        {"invalid number (-10000 to 10000)","Nombre attendu entre -10000 et 10000."},
        {"unexpected text after number","Texte inattendu après le nombre."},
        {"invalid multiplier or divisor (0..10; divisor cannot be zero)","Facteur de 0 à 10 ; le diviseur doit être non nul."},
        {"percentage must be between -99 and 1000","Pourcentage attendu entre -99 et 1000."},
        {"too many effects","Trop d’effets."},
    };
    static char result[256];const char* reason=strstr(error,": ");
    reason=reason?reason+2:error;
    const char* label=reason;
    for(size_t i=0;i<sizeof(messages)/sizeof(messages[0]);i++)
        if(!strcmp(reason,messages[i].reason)){label=tr(messages[i].fr,messages[i].reason);break;}
    snprintf(result,sizeof(result),tr("Ligne %d : %s","Line %d: %s"),atoi(error),label);
    return result;
}
static void draw_background_rules(int64_t obj,float x,float y,float w)
{
    text_in_button((SDL_FRect){x,y+94,w-166,38},621,
        background_rules_draft.inherit?tr("Effets d’origine : conservés","Original effects: kept"):tr("Effets d’origine : remplacés","Original effects: replaced"),
        background_rules_draft.inherit,true);
    menu_button((SDL_FRect){x+w-156,y+94,156,38},620,tr("Voir l’origine","View original"),true);
    bool valid=background_rules_validate(background_rules_draft.text,background_rules_error,sizeof(background_rules_error));
    if(background_rules_mode==1) {
        for(int row=0;row<6;row++) {
            int index=background_rules_page*6+row;if(index>=BACKGROUND_RULE_CONTROL_COUNT)break;
            const BackgroundRuleControl* c=&background_rule_controls[index];float top=y+149+row*53;
            text_draw(1,background_rule_label(c),x,top+2,GOLD);
            text_draw(0,c->key,x,top+25,MUTED);
            int which=!strcmp(c->key,"speed")?0:!strcmp(c->key,"maxfollowers")?1:-1;
            int value=background_rules_adjustment(background_rules_draft.text,c->key);
            if(which>=0)value+=background_bonuses[which];
            if(!strcmp(c->key,"alignment"))value/=10;
            char number[32];snprintf(number,sizeof(number),"%+d",value);
            int action=which>=0?614+which*2:630+row*2;
            menu_button((SDL_FRect){x+w-190,top,38,38},action,"-",valid && (which<0 || background_bonus_step_allowed(obj,which,-1)));
            text_draw(0,number,x+w-135,top+8,GOLD);
            menu_button((SDL_FRect){x+w-38,top,38,38},action+1,"+",valid && (which<0 || background_bonus_step_allowed(obj,which,1)));
        }
        char page[32];snprintf(page,sizeof(page),"%d / %d",background_rules_page+1,(BACKGROUND_RULE_CONTROL_COUNT+5)/6);
        menu_button((SDL_FRect){x,y+479,44,36},623,"<",background_rules_page>0);
        text_draw(0,page,x+62,y+487,GOLD);
        menu_button((SDL_FRect){x+156,y+479,44,36},624,">",(background_rules_page+1)*6<BACKGROUND_RULE_CONTROL_COUNT);
        menu_paragraph(tr("Bonus ajoutés aux effets conservés. Pour les pourcentages et les bornes, utilisez Avancé.",
            "Bonuses added to retained effects. Use Advanced for percentages and bounds."),(SDL_FRect){x,y+526,w,44},0,MUTED);
    } else {
        char inherited[BACKGROUND_RULE_TEXT]="";
        if(background_rules_mode==3)background_rules_describe(background_effect_get(background_get(obj)),inherited,sizeof(inherited));
        bool editable=background_rules_mode==2,focused=editable && background_text_field==5;
        const char* text=editable?background_rules_draft.text:inherited;
        text_draw(0,editable?tr("Expressions natives supplémentaires","Additional native expressions"):tr("Effets de l’antécédent d’origine","Original background effects"),x,y+149,GOLD);
        background_rules_rect=(SDL_FRect){x,y+180,w,244};
        fill(background_rules_rect,(SDL_Color){12,9,6,235});outline(background_rules_rect,focused?GOLD:MUTED);
        if(editable && button_count<MAX_BUTTONS)buttons[button_count++]=(Button){background_rules_rect,622,true};
        char visible[BACKGROUND_RULE_TEXT+2];SDL_strlcpy(visible,text,sizeof(visible));
        if(focused)SDL_strlcat(visible,"|",sizeof(visible));
        if(!*visible)SDL_strlcpy(visible,editable?tr("Exemple : reaction -20, alignment max -200","Example: reaction -20, alignment max -200"):tr("Aucun effet numérique.","No numeric effects."),sizeof(visible));
        SDL_FRect content={x+12,y+192,w-66,218};
        int rows=menu_paragraph(visible,(SDL_FRect){content.x,content.y,content.w,0},0,GOLD);
        background_rules_max_scroll=SDL_max(0,rows-(int)(content.h/text_line_height()));
        if(background_rules_scroll>background_rules_max_scroll)background_rules_scroll=background_rules_max_scroll;
        if(focused && background_text_replace && *text)fill(content,(SDL_Color){110,80,34,100});
        menu_paragraph(visible,content,background_rules_scroll,GOLD);
        menu_button((SDL_FRect){x+w-46,y+190,34,32},625,"^",background_rules_scroll>0);
        menu_button((SDL_FRect){x+w-46,y+382,34,32},626,"v",background_rules_scroll<background_rules_max_scroll);
        menu_paragraph(editable?tr("Séparez les effets par une virgule ou un retour à la ligne. Opérateurs : +, -, %, *, /, min, max. Alignement : 10 unités = 1 point affiché.",
            "Separate effects with commas or line breaks. Operators: +, -, %, *, /, min, max. Alignment: 10 units = 1 displayed point."):
            tr("Les règles spéciales (Éducateur, mage diurne…) restent celles de l’antécédent choisi, même si ses effets numériques sont remplacés.",
            "Special rules (Educator, Day Mage…) still come from the chosen background, even when its numeric effects are replaced."),
            (SDL_FRect){x,y+437,w,70},0,MUTED);
        menu_paragraph(valid?tr("Les effets personnalisés sont conservés dans modern-ui/background-effects.mes. Sauvegardez ce fichier avec vos parties.",
            "Custom effects are stored in modern-ui/background-effects.mes. Back up this file with your saves."):background_rule_error_label(background_rules_error),
            (SDL_FRect){x,y+515,w,55},0,valid?MUTED:UI_RED);
    }
}
