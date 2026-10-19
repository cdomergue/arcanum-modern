/* Physical bindings: labels follow the active SDL keyboard layout. */
enum {KB_INVENTORY,KB_JOURNAL,KB_CHARACTER,KB_MAP,KB_SPELLS,KB_SKILLS,
    KB_SCHEMATICS,KB_REST,KB_STEALTH,KB_STEAL,KB_END,
    KB_UP,KB_LEFT,KB_DOWN,KB_RIGHT,KB_COUNT};
static int key_binding[KB_COUNT];
static const int key_defaults[]={SDL_SCANCODE_I,SDL_SCANCODE_J,SDL_SCANCODE_C,SDL_SCANCODE_G,
    SDL_SCANCODE_M,SDL_SCANCODE_K,SDL_SCANCODE_T,SDL_SCANCODE_R,SDL_SCANCODE_H,SDL_SCANCODE_P,
    SDL_SCANCODE_E,SDL_SCANCODE_W,SDL_SCANCODE_A,SDL_SCANCODE_S,SDL_SCANCODE_D};
static int key_edit=-1,key_page;
static char key_status[256];
static const char* tr(const char* fr,const char* en)
{return strcmp(modern_intgame_text(0),"Niveau")==0?fr:en;}
static const char* key_label(int i)
{
    const int labels[]={1006,1004,1005,1007,1000,1001,1003,1009};
    if(i<8)return game_text(modern_intgame_text(labels[i]));
    if(i==KB_STEALTH || i==KB_STEAL)return game_text(sub_57A700(i==KB_STEALTH?0:1));
    const char* fr[]={"Fin du tour","Caméra : haut","Caméra : gauche","Caméra : bas","Caméra : droite"};
    const char* en[]={"End turn","Camera: up","Camera: left","Camera: down","Camera: right"};
    return tr(fr[i-KB_END],en[i-KB_END]);
}
static const char* key_name(int i)
{
    if(!key_binding[i])return tr("Aucune","None");
    return SDL_GetKeyName(SDL_GetKeyFromScancode(key_binding[i],SDL_KMOD_NONE,false));
}
static bool key_is(int i,int key){return key_binding[i]!=0 && key_binding[i]==key;}
static void key_save(void)
{
    FILE* f=fopen("modern-ui/keys.ini.tmp","w");if(!f)return;
    for(int i=0;i<KB_COUNT;i++)fprintf(f,"%d=%d\n",i,key_binding[i]);
    if(fclose(f)==0)SDL_RenamePath("modern-ui/keys.ini.tmp","modern-ui/keys.ini");
}
static bool key_allowed(int key)
{
    /* Keep numbers for quick slots, navigation/modal keys and engine F keys. */
    return (key>=SDL_SCANCODE_A && key<=SDL_SCANCODE_Z) || key==SDL_SCANCODE_SPACE;
}
static void key_load(void)
{
    memcpy(key_binding,key_defaults,sizeof(key_binding));
    FILE* f=fopen("modern-ui/keys.ini","r");if(!f)return;
    int next[KB_COUNT];memcpy(next,key_defaults,sizeof(next));int i,k;
    while(fscanf(f,"%d=%d\n",&i,&k)==2)if(i>=0 && i<KB_COUNT && (!k || key_allowed(k)))next[i]=k;
    fclose(f);
    for(i=0;i<KB_COUNT;i++)for(int j=0;j<i;j++)if(next[i] && next[i]==next[j])return;
    memcpy(key_binding,next,sizeof(next));
}
static bool key_assign(int i,int key)
{
    if(i<0 || i>=KB_COUNT || (key && !key_allowed(key))) {
        SDL_strlcpy(key_status,tr("Touche réservée : utilisez une lettre ou Espace.","Reserved key: use a letter or Space."),sizeof(key_status));return false;
    }
    for(int j=0;j<KB_COUNT;j++)if(j!=i && key && key_binding[j]==key) {
        snprintf(key_status,sizeof(key_status),"%s : %s",tr("Déjà utilisée","Already used"),key_label(j));return false;
    }
    key_binding[i]=key;key_edit=-1;key_status[0]=0;key_save();return true;
}
static bool key_options_event(SDL_Event* event)
{
    if(options_tab!=3 || key_edit<0)return false;
    if(event->type==SDL_EVENT_KEY_DOWN && !event->key.repeat) {
        if(event->key.scancode==SDL_SCANCODE_ESCAPE){key_edit=-1;key_status[0]=0;}
        else if(event->key.scancode==SDL_SCANCODE_BACKSPACE || event->key.scancode==SDL_SCANCODE_DELETE)key_assign(key_edit,0);
        else key_assign(key_edit,event->key.scancode);
    }
    return event->type==SDL_EVENT_KEY_DOWN || event->type==SDL_EVENT_KEY_UP || event->type==SDL_EVENT_TEXT_INPUT;
}
static void draw_key_options(float x,float y,float w,float h)
{
    float ox=x+246,ow=w-280;
    text_draw(2,tr("Raccourcis","Key bindings"),ox,y+142,GOLD);
    text_compact(tr("Cliquez sur une touche, puis appuyez sur une lettre. Échap annule ; Retour arrière efface.","Click a binding, then press a letter. Escape cancels; Backspace clears."),ox,y+182,ow,false,MUTED);
    for(int row=0;row<8;row++) {
        int i=key_page*8+row;if(i>=KB_COUNT)break;
        float yy=y+222+row*46;
        text_compact(key_label(i),ox,yy+8,ow-235,false,GOLD);
        button((SDL_FRect){ox+ow-220,yy,220,38},480+i,key_edit==i?"...":key_name(i),-1,key_edit==i,true);
    }
    text_compact(key_status,ox,y+601,ow,false,(SDL_Color){239,156,108,255});
    button((SDL_FRect){ox,y+638,46,34},419,"<",-1,false,key_page>0);
    button((SDL_FRect){ox+52,y+638,46,34},420,">",-1,false,key_page==0);
    button((SDL_FRect){ox+115,y+638,160,34},422,"AZERTY",-1,false,true);
    button((SDL_FRect){ox+285,y+638,160,34},423,"QWERTY",-1,false,true);
    button((SDL_FRect){ox+455,y+638,ow-455,34},424,tr("Par défaut","Defaults"),-1,false,true);
    menu_button((SDL_FRect){x+40,y+h-82,180,54},405,menu_text(4003),true);
}
static bool key_options_action(int action)
{
    if(action==418){options_tab=3;key_edit=-1;key_status[0]=0;return true;}
    if(options_tab!=3)return false;
    if(action>=480 && action<480+KB_COUNT){key_edit=action-480;key_status[0]=0;return true;}
    if(action==419 || action==420){key_page=action-419;key_edit=-1;return true;}
    if(action>=422 && action<=424){
        memcpy(key_binding,key_defaults,sizeof(key_binding));
        if(action!=424){
            const char* keys=action==422?"zqsd":"wasd";
            for(int j=0;j<4;j++)key_binding[KB_UP+j]=SDL_GetScancodeFromKey(keys[j],NULL);
        }
        key_edit=-1;key_status[0]=0;key_save();return true;
    }
    return false;
}
static int key_for_action(int action)
{
    switch(action){case 2:case 1104:return KB_INVENTORY;case 0:return KB_JOURNAL;
    case 1:case 1105:return KB_CHARACTER;case 3:return KB_MAP;case 6:return KB_SPELLS;
    case 5:return KB_SKILLS;case 8:return KB_SCHEMATICS;case 9:return KB_REST;
    case 951:return KB_STEALTH;case 950:return KB_STEAL;case 1100:return KB_END;default:return -1;}
}
