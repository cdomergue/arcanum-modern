/* Player annotations are separate from game archives and save formats. */
typedef struct JournalMark { int chapter,id; } JournalMark;
typedef struct MapMemo { int mode,map,townmap,x,y; char text[192]; int west,south; bool precise; } MapMemo;
static JournalMark journal_marks[128],journal_targets[64];
static int journal_mark_count,journal_target_count,tracked_quest=-1;
static bool journal_favorites;
static int current_entry=-1,journal_jump=-1;
static MapMemo map_memos[64];
static int map_memo_count,memo_edit=-1;
static bool memo_placing,memo_menu_open,memos_visible=true;
static MapMemo memo_draft;
static char memo_west[16],memo_south[16];
static int memo_field,memo_error;
static bool memo_coords_changed,memo_select_all;
static char notebook_path[256],notebook_identity[OID_STR_LEN];
static char tracked_text[4096];
static Uint64 tracked_refresh;
/* World-map IDs are shared by multiple cities. A local annotation must also
 * carry its town-map ID. Zero is retained for old records of unknown origin. */
static bool map_memo_matches(const MapMemo* memo,const ModernMap* map)
{
    return memo->mode==map->mode && memo->map==map->map
        && (map->mode!=2 || (memo->townmap>0 && memo->townmap==map->townmap));
}
static void notebook_notes_log(const char* event,bool success)
{
    static Uint64 window;
    static int entries;
    Uint64 now=SDL_GetTicks();
    if(now-window>=1000){window=now;entries=0;}
    if(entries++>=20)return;
    int saved_errno=errno,unassigned=0;
    for(int i=0;i<map_memo_count;i++)if(map_memos[i].mode==2 && !map_memos[i].townmap)unassigned++;
    FILE* log=fopen("ArcanumModern/notebook-notes.log","ab+");
    if(log){
        fseek(log,0,SEEK_END);
        if(ftell(log)>=1024*1024){
            fclose(log);log=NULL;
            if(remove("ArcanumModern/notebook-notes.previous.log")==0 || errno==ENOENT)
                if(rename("ArcanumModern/notebook-notes.log","ArcanumModern/notebook-notes.previous.log")==0)
                    log=fopen("ArcanumModern/notebook-notes.log","ab");
        }
        if(log){
            fprintf(log,"ticks_ms=%llu event=%s notes=%d local_unassigned=%d success=%d\n",
                (unsigned long long)now,event,map_memo_count,unassigned,success);
            fclose(log);
        }
    }
    errno=saved_errno;
}
static bool notebook_save(void)
{
    if(!*notebook_path)return false;
    char temp[280];snprintf(temp,sizeof(temp),"%s.tmp",notebook_path);
    FILE* f=fopen(temp,"w");if(!f){notebook_notes_log("save_open",false);return false;}
    fprintf(f,"TRACK %d\n",tracked_quest);
    for(int i=0;i<journal_mark_count;i++)fprintf(f,"FAV %d %d\n",journal_marks[i].chapter,journal_marks[i].id);
    for(int i=0;i<map_memo_count;i++) {
        MapMemo* m=&map_memos[i];
        if(m->precise)fprintf(f,"NOTE3 %d %d %d %d %d %d %d %s\n",m->mode,m->map,m->townmap,m->x,m->y,m->west,m->south,m->text);
        else fprintf(f,"NOTE2 %d %d %d %d %d %s\n",m->mode,m->map,m->townmap,m->x,m->y,m->text);
    }
    bool success=!ferror(f);
    if(fclose(f)!=0)success=false;
    if(success)success=SDL_RenamePath(temp,notebook_path);
    notebook_notes_log("save",success);
    return success;
}
static void notebook_sync(void)
{
    int64_t pc=player_get_local_pc_obj();if(!pc || !obj_handle_is_valid(pc))return;
    char id[OID_STR_LEN];objid_id_to_str(id,obj_get_id(pc));
    if(!strcmp(id,notebook_identity))return;
    SDL_strlcpy(notebook_identity,id,sizeof(notebook_identity));
    for(char* p=id;*p;p++)if(!SDL_isalnum(*p))*p='_';
    snprintf(notebook_path,sizeof(notebook_path),"modern-ui/notebook-%s.txt",id);
    journal_mark_count=map_memo_count=0;tracked_quest=-1;memo_edit=-1;memo_placing=false;memo_menu_open=false;tracked_text[0]=0;tracked_refresh=0;
    FILE* f=fopen(notebook_path,"r");if(!f){notebook_notes_log("load_open",false);return;}
    char line[512];
    while(fgets(line,sizeof(line),f)){
        JournalMark mark;MapMemo memo={0};int track;
        if(sscanf(line,"TRACK %d",&track)==1)tracked_quest=track;
        else if(sscanf(line,"FAV %d %d",&mark.chapter,&mark.id)==2 && mark.chapter>=0 && mark.chapter<7 && journal_mark_count<128)journal_marks[journal_mark_count++]=mark;
        else if(sscanf(line,"NOTE3 %d %d %d %d %d %d %d %191[^\n]",&memo.mode,&memo.map,&memo.townmap,&memo.x,&memo.y,&memo.west,&memo.south,memo.text)==8
            && memo.mode>=0 && memo.mode<3 && memo.townmap>=0 && memo.west>=0 && memo.south>=0 && map_memo_count<64) {
            memo.precise=true;map_memos[map_memo_count++]=memo;
        }
        else if(sscanf(line,"NOTE2 %d %d %d %d %d %191[^\n]",&memo.mode,&memo.map,&memo.townmap,&memo.x,&memo.y,memo.text)==6
            && memo.mode>=0 && memo.mode<3 && memo.townmap>=0 && map_memo_count<64)map_memos[map_memo_count++]=memo;
        else if(sscanf(line,"NOTE %d %d %d %d %191[^\n]",&memo.mode,&memo.map,&memo.x,&memo.y,memo.text)==5 && memo.mode>=0 && memo.mode<3 && map_memo_count<64)map_memos[map_memo_count++]=memo;
    }
    fclose(f);dirty=true;notebook_notes_log("load",true);
}
static int journal_mark_index(int tab,int id)
{
    for(int i=0;i<journal_mark_count;i++)if(journal_marks[i].chapter==tab && journal_marks[i].id==id)return i;
    return -1;
}
static void notebook_action(int action)
{
    if(action==2300){journal_favorites=!journal_favorites;dirty=true;spread=0;return;}
    if(action==2301){modern_intgame_action(0);chapter=1;search[0]=0;quest_filter=0;journal_favorites=false;journal_jump=tracked_quest;dirty=true;return;}
    int index=(action-2000)/2;if(index<0 || index>=journal_target_count)return;
    JournalMark mark=journal_targets[index];
    if((action-2000)%2){tracked_quest=tracked_quest==mark.id?-1:mark.id;tracked_refresh=0;tracked_text[0]=0;}
    else {
        int i=journal_mark_index(mark.chapter,mark.id);
        if(i>=0)journal_marks[i]=journal_marks[--journal_mark_count];
        else if(journal_mark_count<128)journal_marks[journal_mark_count++]=mark;
    }
    notebook_save();dirty=true;
}
static void memo_focus(int index)
{
    int west,south;
    memo_menu_open=false;memo_edit=index;memo_field=0;memo_select_all=false;memo_coords_changed=false;memo_error=0;
    memo_west[0]=memo_south[0]=0;
    if(memo_draft.precise) {
        snprintf(memo_west,sizeof(memo_west),"%d",memo_draft.west);snprintf(memo_south,sizeof(memo_south),"%d",memo_draft.south);
    } else if(modern_map_point_coords(memo_draft.x,memo_draft.y,&west,&south)) {
        snprintf(memo_west,sizeof(memo_west),"%d",west);snprintf(memo_south,sizeof(memo_south),"%d",south);
    }
    SDL_StartTextInput(window);
}
static void memo_close(void){memo_edit=-1;memo_placing=false;memo_menu_open=false;SDL_StopTextInput(window);}
static char* memo_input_buffer(size_t* size)
{
    if(memo_field==1){*size=sizeof(memo_west);return memo_west;}
    if(memo_field==2){*size=sizeof(memo_south);return memo_south;}
    *size=sizeof(memo_draft.text);return memo_draft.text;
}
static void memo_input(const char* input)
{
    size_t capacity;char* dest=memo_input_buffer(&capacity);
    if(memo_field)for(const char* p=input;*p;p++)if(!SDL_isdigit(*p) && *p!='-')return;
    size_t length=memo_select_all?0:strlen(dest);
    if(length+strlen(input)>=capacity)return;
    if(memo_select_all)*dest=0;
    memo_select_all=false;SDL_strlcat(dest,input,capacity);
    for(char* p=dest;*p;p++)if((unsigned char)*p<32)*p=' ';
    if(memo_field)memo_coords_changed=true;
    memo_error=0;
}
static bool memo_event(SDL_Event* event)
{
    if(memo_edit<0)return false;
    if(event->type==SDL_EVENT_TEXT_INPUT){memo_input(event->text.text);return true;}
    if(event->type==SDL_EVENT_KEY_DOWN || event->type==SDL_EVENT_KEY_UP){
        if(event->type==SDL_EVENT_KEY_DOWN){
            SDL_Scancode key=event->key.scancode;
            if(key==SDL_SCANCODE_ESCAPE)memo_close();
            else if(key==SDL_SCANCODE_RETURN || key==SDL_SCANCODE_KP_ENTER)pending_action=2192;
            else if(key==SDL_SCANCODE_TAB){memo_field=(memo_field+((event->key.mod&SDL_KMOD_SHIFT)?2:1))%3;memo_select_all=true;}
            else if(key==SDL_SCANCODE_A && (event->key.mod&SDL_KMOD_CTRL))memo_select_all=true;
            else if(key==SDL_SCANCODE_V && (event->key.mod&SDL_KMOD_CTRL)) {
                char* pasted=SDL_GetClipboardText();if(pasted){memo_input(pasted);SDL_free(pasted);}
            } else if(key==SDL_SCANCODE_BACKSPACE || (key==SDL_SCANCODE_DELETE && memo_select_all)) {
                size_t capacity;char* dest=memo_input_buffer(&capacity);size_t n=strlen(dest);
                if(memo_select_all)*dest=0;
                else if(n){do{n--;}while(n && ((unsigned char)dest[n]&0xc0)==0x80);dest[n]=0;}
                memo_select_all=false;memo_error=0;if(memo_field)memo_coords_changed=true;
            }
        }return true;
    }
    /* Editing is modal: wheel events must not pan/zoom the map underneath. */
    return event->type==SDL_EVENT_MOUSE_WHEEL;
}
