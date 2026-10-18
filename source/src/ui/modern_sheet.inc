/* Native character sheet: original grouping, live engine values and controls. */
static int sheet_detail_category=1, sheet_detail_index, sheet_degree=-1;

static void sheet_select_detail(int category,int index)
{
    sheet_detail_category=category;sheet_detail_index=index;
    sheet_degree=-1;description_scroll=0;
}
static void sheet_value(const char* label,int value,SDL_FRect r)
{
    char number[32];snprintf(number,sizeof(number),"%d",value);
    float nw=text_measure(1,number);
    text_compact(label,r.x,r.y,r.w-nw-12,false,MUTED);
    text_draw(1,number,r.x+r.w-nw,r.y-3,GOLD);
}
static void sheet_stat_control(int64_t obj,int index,SDL_FRect r)
{
    int value;const char *name,*description;
    sheet_item(obj,0,index,&name,&description,&value);
    button((SDL_FRect){r.x,r.y,r.w,30},690+index,"",-1,
        sheet_detail_category==0 && sheet_detail_index==index,true);
    text_compact(name,r.x+9,r.y+6,r.w-18,false,GOLD);
    char number[32];snprintf(number,sizeof(number),"%d",value);
    button((SDL_FRect){r.x,r.y+36,32,32},620+index*2,"-",-1,false,modern_charedit_editable());
    centered(2,number,r.x+r.w/2,r.y+39,GOLD);
    button((SDL_FRect){r.x+r.w-32,r.y+36,32,32},621+index*2,"+",-1,false,
        modern_charedit_editable() && stat_level_get(obj,STAT_UNSPENT_POINTS)>0);
    rule(r.x,r.y+r.h-1,r.w,(SDL_Color){99,73,41,255});
}
static void sheet_moral(int64_t obj,SDL_FRect r)
{
    int value=stat_level_get(obj,STAT_ALIGNMENT)/10;
    text_compact(game_text(stat_name(STAT_ALIGNMENT)),r.x,r.y,r.w,true,GOLD);
    float cx=r.x+r.w/2,cy=r.y+103,radius=fminf(55,r.w/2-8);
    /* A brass semicircular scale with a needle, rendered at output density. */
    SDL_SetRenderDrawColor(renderer,181,145,78,255);
    for(int i=0;i<180;i++) {
        float a=(float)i*3.14159265f/180,b=(i+1)*3.14159265f/180;
        SDL_RenderLine(renderer,cx-cosf(a)*radius,cy-sinf(a)*radius,cx-cosf(b)*radius,cy-sinf(b)*radius);
        if(i%18==0)SDL_RenderLine(renderer,cx-cosf(a)*(radius-7),cy-sinf(a)*(radius-7),cx-cosf(a)*radius,cy-sinf(a)*radius);
    }
    float angle=(fmaxf(-100,fminf(100,value))+100)*3.14159265f/200;
    SDL_RenderLine(renderer,cx,cy,cx-cosf(angle)*(radius-10),cy-sinf(angle)*(radius-10));
    fill((SDL_FRect){cx-3,cy-3,6,6},GOLD);
    char number[32];snprintf(number,sizeof(number),"%d",value);
    centered(2,number,cx,cy+16,GOLD);
    text_draw(0,"-100",r.x+3,cy+53,MUTED);
    text_draw(0,"+100",r.x+r.w-36,cy+53,MUTED);
}
static void sheet_affinity(int64_t obj,SDL_FRect r)
{
    int value=stat_level_get(obj,STAT_MAGICK_TECH_APTITUDE);
    fill(r,(SDL_Color){21,17,13,245});outline(r,(SDL_Color){115,87,45,255});
    draw_art(473,(SDL_FRect){r.x+r.w/2-18,r.y+12,36,36},true);
    text_compact(character_text(134),r.x+4,r.y+53,r.w-8,true,MUTED);
    char number[32];snprintf(number,sizeof(number),"%d",value>0?value:0);
    centered(2,number,r.x+r.w/2,r.y+78,(SDL_Color){168,182,242,255});
    float top=r.y+120,bottom=r.y+r.h-120,cx=r.x+r.w/2;
    for(int i=0;i<100;i++) {
        SDL_Color color=i<50?(SDL_Color){65,69,95,255}:(SDL_Color){96,72,34,255};
        fill((SDL_FRect){cx-12,top+(bottom-top)*i/100,24,(bottom-top)/100+1},color);
    }
    for(int i=0;i<=20;i++)rule(cx-12,top+(bottom-top)*i/20, i%5?7:24,MUTED);
    float marker=top+(bottom-top)*(100-fmaxf(-100,fminf(100,value)))/200;
    fill((SDL_FRect){cx-23,marker-2,46,4},GOLD);
    text_draw(0,"0",cx+18,(top+bottom)/2-8,MUTED);
    snprintf(number,sizeof(number),"%d",value<0?-value:0);
    centered(2,number,cx,bottom+16,GOLD);
    draw_art(471,(SDL_FRect){cx-18,bottom+53,36,36},true);
    text_compact(character_text(133),r.x+4,r.y+r.h-23,r.w-8,true,MUTED);
}
static void draw_character_sheet(void)
{
    bool playing=play_sheet();
    int64_t obj=playing?modern_charedit_object():player_get_local_pc_obj();
    if(!obj)return;
    if(sheet_category<1 || sheet_category>3)sheet_category=1;
    float w=fminf(1760,canvas_w-48),h=canvas_h-48,x=(canvas_w-w)/2,y=24;
    frame((SDL_FRect){x,y,w,h});
    float left=x+20,top=y+142,body=h-222;
    float aw=90,gap=18,lw=(w-40-aw-2*gap)*.54f;
    float right=left+lw+gap,rw=w-40-lw-aw-2*gap;
    fill((SDL_FRect){left,y+18,w-40,106},(SDL_Color){22,18,13,245});
    draw_portrait(portrait_get(obj),(SDL_FRect){left+12,y+24,76,94});
    char name[512];sub_57CD60(player_get_local_pc_obj(),obj,name);
    const char* display_name=game_text(name);
    if(text_measure(3,display_name)<lw-110)text_draw(3,display_name,left+106,y+30,GOLD);
    else text_compact(display_name,left+106,y+38,lw-110,false,GOLD);
    char identity[512];snprintf(identity,sizeof(identity),"%s : %d  ·  %s  ·  %s  ·  %s : %d",
        character_text(1),stat_level_get(obj,STAT_LEVEL),game_text(race_name(stat_level_get(obj,STAT_RACE))),
        game_text(gender_name(stat_level_get(obj,STAT_GENDER))),character_text(6),stat_level_get(obj,STAT_AGE));
    text_compact(identity,left+106,y+79,lw-110,false,MUTED);
    char points[160];snprintf(points,sizeof(points),"%s : %d",character_text(0),stat_level_get(obj,STAT_UNSPENT_POINTS));
    text_draw(2,points,right,y+39,GOLD);
    snprintf(points,sizeof(points),"%s : %d",character_text(2),level_experience_points_to_next_level(obj));
    text_draw(1,points,right,y+78,MUTED);
    float moral_w=116,stats_x=left+moral_w+16,stats_w=lw-moral_w-16;
    sheet_moral(obj,(SDL_FRect){left,top+57,moral_w,205});
    for(int row=0;row<4;row++)for(int col=0;col<2;col++) {
        int index=2+row+col*4;
        sheet_stat_control(obj,index,(SDL_FRect){stats_x+col*(stats_w+16)/2,top+row*79,(stats_w-16)/2,74});
    }
    for(int i=0;i<2;i++)sheet_stat_control(obj,i,(SDL_FRect){left+i*(lw+18)/2,top+324,(lw-18)/2,75});
    SDL_FRect derived={left,top+416,lw,body-416};
    fill(derived,(SDL_Color){22,18,13,245});
    text_draw(2,character_text(7),derived.x+14,derived.y+10,GOLD);
    const int stats[]={STAT_CARRY_WEIGHT,STAT_DAMAGE_BONUS,STAT_AC_ADJUSTMENT,STAT_SPEED,
        STAT_HEAL_RATE,STAT_POISON_RECOVERY,STAT_REACTION_MODIFIER,STAT_MAX_FOLLOWERS};
    float dw=derived.w*.65f;
    for(int i=0;i<8;i++)sheet_value(game_text(stat_name(stats[i])),stat_level_get(obj,stats[i]),
        (SDL_FRect){derived.x+14+(i/4)*dw/2,derived.y+51+(i%4)*32,dw/2-24,28});
    text_draw(1,character_text(8),derived.x+dw+8,derived.y+12,GOLD);
    const int resist[]={RESISTANCE_TYPE_NORMAL,RESISTANCE_TYPE_MAGIC,RESISTANCE_TYPE_FIRE,RESISTANCE_TYPE_POISON,RESISTANCE_TYPE_ELECTRICAL};
    for(int i=0;i<5;i++)sheet_value(character_text(12+i),object_get_resistance(obj,resist[i],true),
        (SDL_FRect){derived.x+dw+8,derived.y+49+i*26,derived.w-dw-22,25});
    sheet_affinity(obj,(SDL_FRect){right+rw+gap,top,aw,body});
    fill((SDL_FRect){right,top,rw,body},(SDL_Color){22,18,13,245});
    for(int i=0;i<3;i++)button((SDL_FRect){right+i*(rw+6)/3,top,(rw-12)/3,44},651+i,character_text(132+i),-1,sheet_category==i+1,true);
    float content_y=top+54,detail_y=top+body-150;
    if(sheet_category==1) {
        for(int i=0;i<4;i++)button((SDL_FRect){right+i*(rw+5)/4,content_y,(rw-15)/4,42},684+i,character_text(135+i),-1,sheet_page==i,true);
        for(int row=0;row<4;row++) {
            int index=sheet_page*4+row,value;const char *label,*description;
            sheet_item(obj,1,index,&label,&description,&value);
            float ry=content_y+56+row*81;
            button((SDL_FRect){right+10,ry,rw-20,72},660+row,"",-1,
                sheet_detail_category==1 && sheet_detail_index==index,true);
            int skill=sheet_skills[index];
            int training=index<12?basic_skill_training_get(obj,skill):tech_skill_training_get(obj,skill);
            int effective=index<12?basic_skill_level(obj,skill):tech_skill_level(obj,skill);
            char caption[512];snprintf(caption,sizeof(caption),"%s%s%s",label,training?" · ":"",training?game_text(training_name(training)):"");
            text_compact(caption,right+22,ry+9,rw-105,false,GOLD);
            snprintf(caption,sizeof(caption),"%d / 5",value);
            text_draw(0,caption,right+rw-75,ry+9,MUTED);
            button((SDL_FRect){right+21,ry+34,30,28},600+row*2,"-",-1,false,modern_charedit_editable());
            button((SDL_FRect){right+rw-51,ry+34,30,28},601+row*2,"+",-1,false,
                modern_charedit_editable() && stat_level_get(obj,STAT_UNSPENT_POINTS)>0);
            float bar_x=right+64,bar_w=rw-128;
            for(int j=0;j<20;j++)fill((SDL_FRect){bar_x+j*bar_w/20,ry+43,bar_w/20-(j%4==3?5:1),9},
                j<effective?GOLD:(SDL_Color){72,58,39,255});
        }
    } else {
        int count=sheet_category==2?8:16;
        for(int i=0;i<count;i++)button((SDL_FRect){right+(i%4)*(rw+5)/4,content_y+(i/4)*33,(rw-15)/4,29},660+i,
            game_text(sheet_category==2?tech_discipline_name_get(i):spell_college_name(i)),-1,sheet_selected==i,true);
        float sy=content_y+(count/4)*33+12;
        int value;const char *label,*description;
        sheet_item(obj,sheet_category,sheet_selected,&label,&description,&value);
        text_compact(label,right+12,sy+8,rw-126,false,GOLD);
        char number[32];snprintf(number,sizeof(number),"%d / %d",value,sheet_category==2?7:5);
        text_draw(0,number,right+rw-117,sy+10,MUTED);
        button((SDL_FRect){right+rw-70,sy,30,30},640,"-",-1,false,modern_charedit_editable() && value>0);
        button((SDL_FRect){right+rw-34,sy,30,30},641,"+",-1,false,
            modern_charedit_editable() && stat_level_get(obj,STAT_UNSPENT_POINTS)>0 && value<(sheet_category==2?7:5));
        int rows=sheet_category==2?7:5;
        float row_h=(detail_y-sy-43)/rows;
        for(int i=0;i<rows;i++) {
            SDL_FRect r={right+10,sy+40+i*row_h,rw-20,row_h-3};
            const char* title=game_text(sheet_category==2?schematic_ui_learned_schematic_product_name(sheet_selected,i+1):spell_name(sheet_selected*5+i));
            button(r,676+i,"",-1,sheet_detail_category==sheet_category && sheet_degree==i,true);
            char rank[12];snprintf(rank,sizeof(rank),"%d",i+1);
            text_draw(1,rank,r.x+9,r.y+5,i<value?GOLD:MUTED);
            text_compact(title,r.x+35,r.y+6,r.w-47,false,i<value?GOLD:MUTED);
            if(sheet_category==2 && row_h>=39) {
                char item1[2048]={0},item2[2048]={0},components[8192];
                schematic_ui_learned_schematic_component_names(sheet_selected,i+1,item1,item2);
                snprintf(components,sizeof(components),"%s + %s",game_text(item1),game_text(item2));
                text_compact(components,r.x+35,r.y+23,r.w-47,false,MUTED);
            }
        }
    }
    const char *label,*description;int value;
    sheet_item(obj,sheet_detail_category,sheet_detail_index,&label,&description,&value);
    char detail[8192];
    if(sheet_degree>=0 && sheet_detail_category==2) {
        label=game_text(schematic_ui_learned_schematic_product_name(sheet_detail_index,sheet_degree+1));
        description=game_text(tech_degree_description_get(sheet_degree+1,sheet_detail_index));
    } else if(sheet_degree>=0 && sheet_detail_category==3) {
        label=game_text(spell_name(sheet_detail_index*5+sheet_degree));
        description=game_text(spell_description(sheet_detail_index*5+sheet_degree));
    }
    if(sheet_detail_category==0)SDL_strlcpy(detail,description,sizeof(detail));
    else if(sheet_detail_category==2 && sheet_degree>=0) {
        char item1[2048]={0},item2[2048]={0};
        schematic_ui_learned_schematic_component_names(sheet_detail_index,sheet_degree+1,item1,item2);
        snprintf(detail,sizeof(detail),"%s\n%s + %s\n%s",label,game_text(item1),game_text(item2),description);
    } else snprintf(detail,sizeof(detail),"%s\n%s",label,description);
    description_panel(detail,(SDL_FRect){right,detail_y,rw,150});
    float fy=y+h-60;
    if(!playing)menu_button((SDL_FRect){left,fy,230,44},501,menu_text(7002),true);
    if(*message_text && SDL_GetTicks()-message_time<12000)
        menu_paragraph(message_text,(SDL_FRect){left+(playing?0:245),fy,lw-(playing?0:245),54},0,(SDL_Color){240,180,120,255});
    menu_button((SDL_FRect){x+w-260,fy,240,44},playing?720:500,menu_text(playing?260:7003),true);
}
