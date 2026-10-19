typedef struct MapCacheTile {
    int x, y, level;
    Uint64 used;
    SDL_Texture* texture;
} MapCacheTile;

static MapCacheTile map_cache_tiles[32];
static char map_cache_folder[1024];
static TownMapInfo map_cache_info;
static int map_cache_id = -1;
static bool map_cache_available, map_texture_hd;

static void map_cache_clear(void)
{
    for(int i=0;i<32;i++) {
        SDL_DestroyTexture(map_cache_tiles[i].texture);
        map_cache_tiles[i]=(MapCacheTile){0};
    }
    map_cache_id=-1;
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
    return map_cache_available;
}

static SDL_Texture* map_cache_tile(int x,int y,int level)
{
    int slot=0;
    for(int i=0;i<32;i++) {
        MapCacheTile* tile=&map_cache_tiles[i];
        if(tile->texture && tile->x==x && tile->y==y && tile->level==level) {
            tile->used=SDL_GetTicks();return tile->texture;
        }
        if(!tile->texture || tile->used<map_cache_tiles[slot].used)slot=i;
    }
    char path[1100];snprintf(path,sizeof(path),"%s/%d_%d_%d.png",map_cache_folder,level,x,y);
    SDL_Surface* surface=SDL_LoadPNG(path);
    if(!surface)return NULL;
    SDL_Texture* texture=SDL_CreateTextureFromSurface(renderer,surface);
    SDL_DestroySurface(surface);
    if(!texture)return NULL;
    SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_LINEAR);
    SDL_DestroyTexture(map_cache_tiles[slot].texture);
    map_cache_tiles[slot]=(MapCacheTile){x,y,level,SDL_GetTicks(),texture};
    return texture;
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

static bool map_cache_draw(const ModernMap* map,SDL_FRect dest,float scale)
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
    if(presentation_mode!=SDL_LOGICAL_PRESENTATION_DISABLED && logical_width>0)
        density_x*=presentation.w/logical_width;
    int level=0;
    while(level<3 && factor*density_x*(1<<(level+1))<=1)level++;
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
        if(!texture){ok=false;break;}
        float tw,th;SDL_GetTextureSize(texture,&tw,&th);
        SDL_FRect rectangle={left+tx*1024*factor,top+ty*1024*factor,
            tw*(1<<level)*factor,th*(1<<level)*factor};
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
