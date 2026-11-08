static void town_notes_log(const char* event,int previous,int selected,int count,const char* outcome)
{
    static Uint64 window;
    static int entries;
    Uint64 now=SDL_GetTicks();
    if(now-window>=1000){window=now;entries=0;}
    if(entries++>=20)return;
    int saved_errno=errno;
    FILE* log=fopen("ArcanumModern/map-notes.log","ab+");
    if(log){
        fseek(log,0,SEEK_END);
        if(ftell(log)>=1024*1024){
            fclose(log);log=NULL;
            if(remove("ArcanumModern/map-notes.previous.log")==0 || errno==ENOENT)
                if(rename("ArcanumModern/map-notes.log","ArcanumModern/map-notes.previous.log")==0)
                    log=fopen("ArcanumModern/map-notes.log","ab");
        }
        if(log){
            fprintf(log,"ticks_ms=%llu event=%s previous=%d selected=%d count=%d outcome=%s\n",
                (unsigned long long)now,event,previous,selected,count,outcome);
            fclose(log);
        }
    }
    errno=saved_errno;
}

/* The in-memory array always belongs to dword_66D878. Select its owner before
 * deduplication or insertion, including marks made before a sector notification. */
static bool wmap_ui_town_notes_select(int townmap)
{
    if(dword_66D878==townmap)return true;
    int previous=dword_66D878;
    if(!sub_5650C0())return false;
    if(!townmap)return true;
    const char* name=townmap_name(townmap);
    if(!name){town_notes_log("select",previous,townmap,0,"unknown_map");return false;}
    char path[TIG_MAX_PATH];
    snprintf(path,sizeof(path),"%s\\%s.tmn","Save\\Current",name);
    errno=0;
    TigFile* stream=tig_file_fopen(path,"rb");
    if(!stream){
        int error=errno;
        if(tig_file_exists(path,NULL) || (error && error!=ENOENT)){
            town_notes_log("select",previous,townmap,0,"open_failed");
            return false;
        }
        dword_66D878=townmap;
        town_notes_log("select",previous,townmap,0,"no_file");
        return true;
    }
    WmapNote loaded[200]={0};
    int count=0,next_id=0;
    bool success=tig_file_fread(&count,sizeof(count),1,stream)==1
        && count>=0 && count<=200
        && tig_file_fread(&next_id,sizeof(next_id),1,stream)==1;
    for(int i=0;success && i<count;i++)success=wmap_ui_town_note_load(&loaded[i],stream);
    tig_file_fclose(stream);
    if(!success){
        /* Leave ownership unset: never overwrite an unreadable collection with
         * a partial/empty array on the next switch. */
        town_notes_log("select",previous,townmap,0,"read_failed");
        return false;
    }
    memcpy(wmap_ui_town_notes,loaded,(size_t)count*sizeof(*loaded));
    wmap_ui_num_town_notes=count;
    wmap_ui_mode_info[WMAP_UI_MODE_TOWN].field_194=next_id;
    dword_66D878=townmap;
    town_notes_log("select",previous,townmap,count,"loaded");
    return true;
}
