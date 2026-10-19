/* Player annotations are separate from game archives and save formats. */
typedef struct JournalMark { int chapter,id; } JournalMark;
typedef struct MapMemo { int mode,map,x,y; char text[192]; } MapMemo;
static JournalMark journal_marks[128],journal_targets[64];
static int journal_mark_count,journal_target_count,tracked_quest=-1;
static bool journal_favorites;
static int current_entry=-1,journal_jump=-1;
static MapMemo map_memos[64];
static int map_memo_count,memo_edit=-1;
static bool memo_placing,memos_visible=true;
static MapMemo memo_draft;
static char notebook_path[256],notebook_identity[OID_STR_LEN];
static char tracked_text[4096];
static Uint64 tracked_refresh;
static bool notebook_save(void)
{
    if(!*notebook_path)return false;
    char temp[280];snprintf(temp,sizeof(temp),"%s.tmp",notebook_path);
    FILE* f=fopen(temp,"w");if(!f)return false;
    fprintf(f,"TRACK %d\n",tracked_quest);
    for(int i=0;i<journal_mark_count;i++)fprintf(f,"FAV %d %d\n",journal_marks[i].chapter,journal_marks[i].id);
    for(int i=0;i<map_memo_count;i++)fprintf(f,"NOTE %d %d %d %d %s\n",map_memos[i].mode,map_memos[i].map,map_memos[i].x,map_memos[i].y,map_memos[i].text);
    if(fclose(f)!=0)return false;
    return SDL_RenamePath(temp,notebook_path);
}
static void notebook_sync(void)
{
    int64_t pc=player_get_local_pc_obj();if(!pc || !obj_handle_is_valid(pc))return;
    char id[OID_STR_LEN];objid_id_to_str(id,obj_get_id(pc));
    if(!strcmp(id,notebook_identity))return;
    SDL_strlcpy(notebook_identity,id,sizeof(notebook_identity));
    for(char* p=id;*p;p++)if(!SDL_isalnum(*p))*p='_';
    snprintf(notebook_path,sizeof(notebook_path),"modern-ui/notebook-%s.txt",id);
    journal_mark_count=map_memo_count=0;tracked_quest=-1;memo_edit=-1;memo_placing=false;tracked_text[0]=0;tracked_refresh=0;
    FILE* f=fopen(notebook_path,"r");if(!f)return;
    char line[512];
    while(fgets(line,sizeof(line),f)){
        JournalMark mark;MapMemo memo={0};int track;
        if(sscanf(line,"TRACK %d",&track)==1)tracked_quest=track;
        else if(sscanf(line,"FAV %d %d",&mark.chapter,&mark.id)==2 && mark.chapter>=0 && mark.chapter<7 && journal_mark_count<128)journal_marks[journal_mark_count++]=mark;
        else if(sscanf(line,"NOTE %d %d %d %d %191[^\n]",&memo.mode,&memo.map,&memo.x,&memo.y,memo.text)==5 && memo.mode>=0 && memo.mode<3 && map_memo_count<64)map_memos[map_memo_count++]=memo;
    }
    fclose(f);dirty=true;
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
    memo_edit=index;SDL_StartTextInput(window);
}
static void memo_close(void){memo_edit=-1;memo_placing=false;SDL_StopTextInput(window);}
static bool memo_event(SDL_Event* event)
{
    if(memo_edit<0)return false;
    if(event->type==SDL_EVENT_TEXT_INPUT){
        if(strlen(memo_draft.text)+strlen(event->text.text)<sizeof(memo_draft.text))SDL_strlcat(memo_draft.text,event->text.text,sizeof(memo_draft.text));
        for(char* p=memo_draft.text;*p;p++)if((unsigned char)*p<32)*p=' ';
        return true;
    }
    if(event->type==SDL_EVENT_KEY_DOWN || event->type==SDL_EVENT_KEY_UP){
        if(event->type==SDL_EVENT_KEY_DOWN){
            if(event->key.scancode==SDL_SCANCODE_ESCAPE)memo_close();
            else if(event->key.scancode==SDL_SCANCODE_RETURN)pending_action=2192;
            else if(event->key.scancode==SDL_SCANCODE_BACKSPACE){size_t n=strlen(memo_draft.text);if(n){do{n--;}while(n && ((unsigned char)memo_draft.text[n]&0xc0)==0x80);memo_draft.text[n]=0;}}
        }return true;
    }
    return false;
}
