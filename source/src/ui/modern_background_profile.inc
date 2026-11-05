/* Optional personal prose, like notebook annotations. Never a game resource ID. */
static char background_title[96],background_story[1024];
static struct {
    bool active;
    int64_t object;
    char title[96],story[1024];
} background_pending_profile;
static void background_profile_path(int64_t obj,char path[256])
{
    char id[OID_STR_LEN];objid_id_to_str(id,obj_get_id(obj));
    for(char* p=id;*p;p++)if(!SDL_isalnum(*p))*p='_';
    snprintf(path,256,"modern-ui/background-%s.txt",id);
}
static bool background_profile_read(int64_t obj,char* title,size_t title_size,char* story,size_t story_size)
{
    title[0]=story[0]=0;
    if(!obj || !obj_handle_is_valid(obj))return false;
    if(background_pending_profile.active && background_pending_profile.object==obj) {
        SDL_strlcpy(title,background_pending_profile.title,title_size);
        SDL_strlcpy(story,background_pending_profile.story,story_size);
        return *title!=0;
    }
    char path[256];background_profile_path(obj,path);
    FILE* f=fopen(path,"rb");if(!f)return false;
    bool ok=fgets(title,(int)title_size,f)!=NULL;
    if(ok) {title[strcspn(title,"\r\n")]=0;size_t n=fread(story,1,story_size-1,f);story[n]=0;}
    fclose(f);return ok && *title;
}
static bool background_profile_write_values(int64_t obj,const char* title,const char* story)
{
    if(!obj || !obj_handle_is_valid(obj))return false;
    char path[256],temp[280];background_profile_path(obj,path);
    snprintf(temp,sizeof(temp),"%s.tmp",path);
    FILE* f=fopen(temp,"wb");if(!f)return false;
    bool ok=fprintf(f,"%s\n%s",title,story)>=0;
    if(fclose(f)!=0)ok=false;
    return ok && SDL_RenamePath(temp,path);
}
static bool background_profile_write(int64_t obj)
{
    if(!background_profile_write_values(obj,background_title,background_story))return false;
    if(background_pending_profile.object==obj)background_pending_profile.active=false;
    return true;
}

void modern_background_creation_begin(int64_t previous)
{
    char title[96],story[1024];
    bool found=background_profile_read(previous,title,sizeof(title),story,sizeof(story));
    background_pending_profile.active=found;
    background_pending_profile.object=0;
    SDL_strlcpy(background_pending_profile.title,title,sizeof(background_pending_profile.title));
    SDL_strlcpy(background_pending_profile.story,story,sizeof(background_pending_profile.story));
}

bool modern_background_creation_ready(int64_t current)
{
    if(!background_pending_profile.active || background_pending_profile.object!=current)return true;
    if(!background_profile_write_values(current,background_pending_profile.title,background_pending_profile.story))return false;
    background_pending_profile.active=false;
    return true;
}

void modern_background_creation_finish(int64_t current)
{
    background_pending_profile.object=current;
    /* If the disk is unavailable, reads keep using the in-memory draft. The
     * next creation step retries and refuses to lose this annotation silently. */
    modern_background_creation_ready(current);
}
