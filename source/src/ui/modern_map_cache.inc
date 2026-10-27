typedef struct MapCacheTile {
    int x, y, level;
    Uint64 used;
    SDL_Texture* texture;
    size_t bytes;
    bool pending, failed;
} MapCacheTile;

enum {MAP_CACHE_MAX_TILES=2048, MAP_CACHE_JOBS=8};
#define MAP_CACHE_BYTES ((size_t)128*1024*1024)
static MapCacheTile map_cache_tiles[MAP_CACHE_MAX_TILES];
static Uint64 map_cache_clock, map_cache_revision;
static size_t map_cache_bytes;
static char map_cache_folder[1024];
static TownMapInfo map_cache_info;
static int map_cache_id=-1;
static bool map_cache_available, map_texture_hd;

/* Only PNG decoding runs off-thread. Textures and game data stay on the UI
 * thread. Eight jobs bound both outstanding I/O and decoded surface memory. */
typedef struct MapCacheJob {
    int state, slot; /* 0 free, 1 queued, 2 decoding, 3 ready */
    char path[1100];
    SDL_Surface* surface;
} MapCacheJob;
static struct {
    SDL_Thread* thread;
    SDL_Mutex* mutex;
    SDL_Condition* wake;
    bool stop;
    MapCacheJob jobs[MAP_CACHE_JOBS];
} map_loader;

static int map_cache_worker(void* unused)
{
    (void)unused;
    SDL_LockMutex(map_loader.mutex);
    while(!map_loader.stop) {
        int job=-1;
        for(int i=0;i<MAP_CACHE_JOBS;i++)if(map_loader.jobs[i].state==1){job=i;break;}
        if(job<0){SDL_WaitCondition(map_loader.wake,map_loader.mutex);continue;}
        MapCacheJob* work=&map_loader.jobs[job];work->state=2;
        SDL_UnlockMutex(map_loader.mutex);
        SDL_Surface* surface=SDL_LoadPNG(work->path);
        SDL_LockMutex(map_loader.mutex);
        work->surface=surface;work->state=3;
    }
    SDL_UnlockMutex(map_loader.mutex);
    return 0;
}

static void map_cache_stop(void)
{
    if(map_loader.thread) {
        SDL_LockMutex(map_loader.mutex);map_loader.stop=true;
        SDL_SignalCondition(map_loader.wake);SDL_UnlockMutex(map_loader.mutex);
        SDL_WaitThread(map_loader.thread,NULL);
    }
    for(int i=0;i<MAP_CACHE_JOBS;i++)SDL_DestroySurface(map_loader.jobs[i].surface);
    SDL_DestroyCondition(map_loader.wake);SDL_DestroyMutex(map_loader.mutex);
    memset(&map_loader,0,sizeof(map_loader));
}

static bool map_cache_start(void)
{
    if(map_loader.thread)return true;
    map_loader.mutex=SDL_CreateMutex();map_loader.wake=SDL_CreateCondition();
    if(map_loader.mutex && map_loader.wake)
        map_loader.thread=SDL_CreateThread(map_cache_worker,"map-png",NULL);
    if(map_loader.thread)return true;
    map_cache_stop();return false;
}

static void map_cache_evict(int slot)
{
    MapCacheTile* tile=&map_cache_tiles[slot];
    SDL_DestroyTexture(tile->texture);map_cache_bytes-=tile->bytes;
    *tile=(MapCacheTile){0};map_cache_revision++;
}

static int map_cache_oldest(void)
{
    int slot=-1;
    for(int i=0;i<MAP_CACHE_MAX_TILES;i++) {
        MapCacheTile* tile=&map_cache_tiles[i];
        if(tile->pending)continue;
        if(!tile->used)return i;
        if(slot<0 || tile->used<map_cache_tiles[slot].used)slot=i;
    }
    return slot;
}

static void map_cache_pump(void)
{
    if(!map_loader.thread)return;
    Uint64 start=SDL_GetTicksNS();
    for(int i=0;i<MAP_CACHE_JOBS;i++) {
        SDL_LockMutex(map_loader.mutex);
        MapCacheJob job=map_loader.jobs[i];
        if(job.state==3)map_loader.jobs[i]=(MapCacheJob){0};
        SDL_UnlockMutex(map_loader.mutex);
        if(job.state!=3)continue;
        MapCacheTile* tile=&map_cache_tiles[job.slot];
        size_t bytes=job.surface?(size_t)job.surface->w*job.surface->h*4:0;
        while(map_cache_bytes+bytes>MAP_CACHE_BYTES) {
            /* All occupied resident entries have a texture. Pending entries
             * cannot be recycled until their worker result is consumed. */
            int oldest=-1;
            for(int j=0;j<MAP_CACHE_MAX_TILES;j++)if(map_cache_tiles[j].texture
                && (oldest<0 || map_cache_tiles[j].used<map_cache_tiles[oldest].used))oldest=j;
            if(oldest<0)break;
            map_cache_evict(oldest);
        }
        tile->texture=job.surface?SDL_CreateTextureFromSurface(renderer,job.surface):NULL;
        SDL_DestroySurface(job.surface);tile->pending=false;tile->failed=!tile->texture;
        if(tile->texture) {
            tile->bytes=bytes;map_cache_bytes+=bytes;
            SDL_SetTextureScaleMode(tile->texture,SDL_SCALEMODE_LINEAR);
        }
        map_cache_revision++;
        /* Upload at most a small time slice, even after a long render pause. */
        if(SDL_GetTicksNS()-start>=2000000)break;
    }
}

static SDL_Texture* map_cache_tile(int x,int y,int level)
{
    SDL_Texture* fallback=NULL;int distance=100;
    bool requested=false;
    for(int i=0;i<MAP_CACHE_MAX_TILES;i++) {
        MapCacheTile* tile=&map_cache_tiles[i];
        if(!tile->used || tile->x!=x || tile->y!=y)continue;
        if(tile->level==level) {
            tile->used=++map_cache_clock;requested=true;
            if(tile->failed)map_cache_available=false;
            if(tile->texture)return tile->texture;
        } else if(tile->texture && abs(tile->level-level)<distance) {
            distance=abs(tile->level-level);fallback=tile->texture;
        }
    }
    if(requested || !map_loader.thread)return fallback;
    int slot=map_cache_oldest();if(slot<0)return fallback;
    SDL_LockMutex(map_loader.mutex);
    for(int i=0;i<MAP_CACHE_JOBS;i++)if(!map_loader.jobs[i].state) {
        /* Do not return a fallback texture that this reservation evicts. */
        if(map_cache_tiles[slot].texture==fallback)fallback=NULL;
        map_cache_evict(slot);
        map_cache_tiles[slot]=(MapCacheTile){.x=x,.y=y,.level=level,
            .used=++map_cache_clock,.pending=true};
        MapCacheJob* job=&map_loader.jobs[i];
        snprintf(job->path,sizeof(job->path),"%s/%d_%d_%d.png",map_cache_folder,level,x,y);
        job->slot=slot;job->state=1;SDL_SignalCondition(map_loader.wake);break;
    }
    SDL_UnlockMutex(map_loader.mutex);
    return fallback;
}

static SDL_Texture* map_cache_view;
static Uint64 map_cache_view_revision;
static bool map_cache_view_valid;

static void map_cache_clear(void)
{
    map_cache_stop();
    for(int i=0;i<MAP_CACHE_MAX_TILES;i++)map_cache_evict(i);
    SDL_DestroyTexture(map_cache_view);map_cache_view=NULL;map_cache_view_valid=false;
    map_cache_id=-1;map_cache_clock=0;map_cache_available=false;
}

static bool map_cache_prepare(const ModernMap* map)
{
    char folder[1024];
    snprintf(folder,sizeof(folder),"modern-ui/local-maps/%s/%d",
        gamelib_current_module_name_get(),map->townmap);
    if(map_cache_id==map->townmap && !strcmp(folder,map_cache_folder))return map_cache_available;
    map_cache_clear();
    map_cache_id=map->townmap;
    SDL_strlcpy(map_cache_folder,folder,sizeof(map_cache_folder));
    map_cache_available=false;
    if(!map->townmap || !townmap_info(map->townmap,&map_cache_info))return false;
    char path[1100];snprintf(path,sizeof(path),"%s/complete",folder);
    FILE* f=fopen(path,"r");
    if(!f)return false;
    int version,width,height,tile,levels;
    map_cache_available=fscanf(f,"%d %d %d %d %d",&version,&width,&height,&tile,&levels)==5
        && version==1 && width==map_cache_info.width && height==map_cache_info.height
        && tile==1024 && levels==4;
    fclose(f);
    if(map_cache_available && !map_cache_start())map_cache_available=false;
    return map_cache_available;
}

static SDL_Texture* map_overlay_texture(TigVideoBuffer* buffer)
{
    if(tig_video_buffer_lock(buffer)!=TIG_OK)return NULL;
    TigVideoBufferData data;
    SDL_Texture* texture=NULL;
    if(tig_video_buffer_data(buffer,&data)==TIG_OK) {
        Uint32* pixels=malloc((size_t)data.width*data.height*4);
        if(pixels) {
            Uint32 key=tig_color_make(255,0,255)&0xffffff;
            for(int y=0;y<data.height;y++)for(int x=0;x<data.width;x++) {
                Uint32 p=((Uint32*)((char*)data.pixels+y*data.pitch))[x];
                pixels[y*data.width+x]=(p&0xffffff)==key?0:p|0xff000000;
            }
            texture=rgba_texture(data.width,data.height,pixels);free(pixels);
        }
    }
    tig_video_buffer_unlock(buffer);
    return texture;
}

static SDL_FRect map_pixel_rect(SDL_FRect rect)
{
    int width,height;SDL_RendererLogicalPresentation mode;
    SDL_FRect presentation;float sx,sy;
    SDL_GetRenderLogicalPresentation(renderer,&width,&height,&mode);
    SDL_GetRenderLogicalPresentationRect(renderer,&presentation);
    SDL_GetRenderScale(renderer,&sx,&sy);
    if(mode!=SDL_LOGICAL_PRESENTATION_DISABLED && width>0 && height>0) {
        sx*=presentation.w/width;sy*=presentation.h/height;
    }
    float l=floorf(rect.x*sx),t=floorf(rect.y*sy);
    float r=floorf((rect.x+rect.w)*sx),b=floorf((rect.y+rect.h)*sy);
    return (SDL_FRect){(l+0.001f)/sx,(t+0.001f)/sy,(r-l+0.001f)/sx,(b-t+0.001f)/sy};
}

static bool map_cache_render(const ModernMap* map,SDL_FRect dest,float scale)
{
    TownMapInfo* info=&map_cache_info;
    float factor=info->scale*scale;
    float left=dest.x+(map->origin_x-map->offset_x)*scale;
    float top=dest.y+(map->origin_y-map->offset_y)*scale;
    float density_x,density_y;SDL_GetRenderScale(renderer,&density_x,&density_y);
    int logical_width,logical_height;SDL_RendererLogicalPresentation presentation_mode;
    SDL_FRect presentation;
    SDL_GetRenderLogicalPresentation(renderer,&logical_width,&logical_height,&presentation_mode);
    SDL_GetRenderLogicalPresentationRect(renderer,&presentation);
    if(presentation_mode!=SDL_LOGICAL_PRESENTATION_DISABLED && logical_width>0 && logical_height>0) {
        density_x*=presentation.w/logical_width;density_y*=presentation.h/logical_height;
    }
    int level=0;
    while(level<3 && factor*fmaxf(density_x,density_y)*(1<<(level+1))<=1)level++;
    int cell_w=info->width/info->num_hor_tiles,cell_h=info->height/info->num_vert_tiles;
    int min_x=(int)fmaxf(0,floorf((dest.x-left)/factor/cell_w));
    int min_y=(int)fmaxf(0,floorf((dest.y-top)/factor/cell_h));
    int max_x=(int)fminf(info->num_hor_tiles,ceilf((dest.x+dest.w-left)/factor/cell_w));
    int max_y=(int)fminf(info->num_vert_tiles,ceilf((dest.y+dest.h-top)/factor/cell_h));
    bool waitable=townmap_is_waitable(map->townmap),ok=true;
    SDL_Rect saved_clip;bool clipped=SDL_RenderClipEnabled(renderer);
    SDL_GetRenderClipRect(renderer,&saved_clip);
    SDL_Rect clip={(int)ceilf(dest.x),(int)ceilf(dest.y),(int)floorf(dest.w),(int)floorf(dest.h)};
    SDL_SetRenderClipRect(renderer,&clip);
    fill(dest,(SDL_Color){0,0,0,255});
    int tx0=(int)fmaxf(0,floorf((dest.x-left)/factor/1024));
    int ty0=(int)fmaxf(0,floorf((dest.y-top)/factor/1024));
    int tx1=(int)fminf((info->width+1023)/1024,ceilf((dest.x+dest.w-left)/factor/1024));
    int ty1=(int)fminf((info->height+1023)/1024,ceilf((dest.y+dest.h-top)/factor/1024));
    for(int ty=ty0;ty<ty1 && ok;ty++)for(int tx=tx0;tx<tx1 && ok;tx++) {
        SDL_Texture* texture=map_cache_tile(tx,ty,level);
        if(!map_cache_available){ok=false;break;}
        if(!texture)continue; /* Still decoding; keep the HD cache active. */
        SDL_FRect rectangle={left+tx*1024*factor,top+ty*1024*factor,
            fminf(1024,info->width-tx*1024)*factor,fminf(1024,info->height-ty*1024)*factor};
        rectangle=map_pixel_rect(rectangle);
        SDL_RenderTexture(renderer,texture,NULL,&rectangle);
    }
    SDL_BlendMode blend;SDL_GetRenderDrawBlendMode(renderer,&blend);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    for(int row=min_y;row<max_y && ok;row++)for(int col=min_x;col<max_x && ok;col++) {
        if(waitable)continue;
        TigVideoBufferBlitInfo fog={0};
        bool known=townmap_tile_blit_info(map->townmap,col+row*info->num_hor_tiles,&fog);
        if(known && !fog.flags)continue;
        SDL_FRect rectangle={left+col*cell_w*factor,top+row*cell_h*factor,cell_w*factor,cell_h*factor};
        rectangle=map_pixel_rect(rectangle);
        if(!known){fill(rectangle,(SDL_Color){0,0,0,255});continue;}
        float xs[4]={rectangle.x,rectangle.x+rectangle.w,rectangle.x+rectangle.w,rectangle.x};
        float ys[4]={rectangle.y,rectangle.y,rectangle.y+rectangle.h,rectangle.y+rectangle.h};
        SDL_Vertex vertices[4];
        for(int i=0;i<4;i++) {
            float alpha=1-tig_color_get_red(fog.lerp_colors[i])/255.0f;
            vertices[i]=(SDL_Vertex){{xs[i],ys[i]},{0,0,0,alpha},{0,0}};
        }
        const int indices[]={0,1,2,0,2,3};
        SDL_RenderGeometry(renderer,NULL,vertices,4,indices,6);
    }
    SDL_SetRenderDrawBlendMode(renderer,blend);
    SDL_SetRenderClipRect(renderer,clipped?&saved_clip:NULL);
    if(!ok)map_cache_available=false;
    return ok;
}

/* The terrain and exploration shading are static while inspecting a map.
 * Compose them once per view instead of submitting hundreds of textures and
 * fog polygons every frame. Route points, actors and notes remain live above. */
static bool map_cache_draw(const ModernMap* map,SDL_FRect dest,float scale)
{
    map_cache_pump();
    if(!map_cache_available)return false;
    float sx,sy;SDL_GetRenderScale(renderer,&sx,&sy);
    int lw,lh;SDL_RendererLogicalPresentation mode;SDL_FRect presentation;
    SDL_GetRenderLogicalPresentation(renderer,&lw,&lh,&mode);
    SDL_GetRenderLogicalPresentationRect(renderer,&presentation);
    float dx=sx,dy=sy;
    if(mode!=SDL_LOGICAL_PRESENTATION_DISABLED && lw>0 && lh>0) {
        dx*=presentation.w/lw;dy*=presentation.h/lh;
    }
    int width=(int)ceilf(dest.w*dx),height=(int)ceilf(dest.h*dy);
    if(width<=0 || height<=0)return true;
    float key[]={map->offset_x,map->offset_y,map->origin_x,map->origin_y,
        dest.w,dest.h,scale,dx,dy,(float)map->townmap};
    static float previous_key[SDL_arraysize(key)];
    static Uint64 previous_fog;
    Uint64 fog_hash=townmap_is_waitable(map->townmap)?0:townmap_exploration_revision(map->townmap);
    float old_width=0,old_height=0;
    if(map_cache_view)SDL_GetTextureSize(map_cache_view,&old_width,&old_height);
    if(old_width!=width || old_height!=height) {
        SDL_DestroyTexture(map_cache_view);
        map_cache_view=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_TARGET,width,height);
        map_cache_view_valid=false;
        if(map_cache_view)SDL_SetTextureScaleMode(map_cache_view,SDL_SCALEMODE_LINEAR);
    }
    if(!map_cache_view)return map_cache_render(map,dest,scale);
    if(!map_cache_view_valid || map_cache_view_revision!=map_cache_revision
        || previous_fog!=fog_hash || memcmp(key,previous_key,sizeof(key))) {
        SDL_Texture* previous_target=SDL_GetRenderTarget(renderer);
        SDL_Rect clip;bool clipped=SDL_RenderClipEnabled(renderer);SDL_GetRenderClipRect(renderer,&clip);
        if(!SDL_SetRenderTarget(renderer,map_cache_view))return map_cache_render(map,dest,scale);
        SDL_SetRenderLogicalPresentation(renderer,0,0,SDL_LOGICAL_PRESENTATION_DISABLED);
        SDL_SetRenderScale(renderer,dx,dy);SDL_SetRenderClipRect(renderer,NULL);
        bool ok=map_cache_render(map,(SDL_FRect){0,0,dest.w,dest.h},scale);
        SDL_SetRenderTarget(renderer,previous_target);
        SDL_SetRenderLogicalPresentation(renderer,lw,lh,mode);SDL_SetRenderScale(renderer,sx,sy);
        SDL_SetRenderClipRect(renderer,clipped?&clip:NULL);
        if(!ok)return false;
        memcpy(previous_key,key,sizeof(key));previous_fog=fog_hash;
        map_cache_view_revision=map_cache_revision;map_cache_view_valid=true;
    }
    SDL_RenderTexture(renderer,map_cache_view,NULL,&dest);
    return true;
}
