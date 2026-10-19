static SDL_Texture** map_bitmap_tiles;
static int map_bitmap_count,map_bitmap_id=-1;

static void map_bitmap_clear(void)
{
    for(int i=0;i<map_bitmap_count;i++)SDL_DestroyTexture(map_bitmap_tiles[i]);
    free(map_bitmap_tiles);map_bitmap_tiles=NULL;map_bitmap_count=0;map_bitmap_id=-1;
}

static bool map_bitmap_draw(const ModernMap* map,SDL_FRect dest,float scale)
{
    if(map_bitmap_id!=map->townmap) {
        map_bitmap_clear();
        map_bitmap_count=map_local.columns*map_local.rows;
        map_bitmap_tiles=calloc(map_bitmap_count,sizeof(*map_bitmap_tiles));
        if(!map_bitmap_tiles){map_bitmap_count=0;return false;}
        map_bitmap_id=map->townmap;
    }
    float width=(float)map_local.width/map_local.columns,height=(float)map_local.height/map_local.rows;
    int left=(int)fmaxf(0,floorf(map->offset_x/width));
    int top=(int)fmaxf(0,floorf(map->offset_y/height));
    int right=(int)fminf(map_local.columns,ceilf((map->offset_x+map->width)/width));
    int bottom=(int)fminf(map_local.rows,ceilf((map->offset_y+map->height)/height));
    bool waitable=townmap_is_waitable(map->townmap),ok=true;
    SDL_Rect saved_clip;bool clipped=SDL_RenderClipEnabled(renderer);
    SDL_GetRenderClipRect(renderer,&saved_clip);
    SDL_Rect clip={(int)dest.x,(int)dest.y,(int)dest.w,(int)dest.h};
    SDL_SetRenderClipRect(renderer,&clip);
    fill(dest,(SDL_Color){0,0,0,255});
    SDL_BlendMode blend;SDL_GetRenderDrawBlendMode(renderer,&blend);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    for(int row=top;row<bottom;row++)for(int col=left;col<right;col++) {
        int index=col+row*map_local.columns;
        TigVideoBufferBlitInfo fog={0};
        bool known=waitable || townmap_tile_blit_info(map->townmap,index,&fog);
        if(!known)continue;
        if(!map_bitmap_tiles[index]) {
            TigVideoBuffer* buffer=modern_map_local_tile(index);
            if(!buffer){ok=false;continue;}
            float tw,th;map_bitmap_tiles[index]=texture_from_buffer(buffer,&tw,&th);
            modern_map_local_tile_release(index);
        }
        if(!map_bitmap_tiles[index]){ok=false;continue;}
        SDL_FRect rect={dest.x+(col*width-map->offset_x)*scale,
            dest.y+(row*height-map->offset_y)*scale,width*scale,height*scale};
        rect=map_pixel_rect(rect);
        SDL_RenderTexture(renderer,map_bitmap_tiles[index],NULL,&rect);
        if(!fog.flags)continue;
        float xs[4]={rect.x,rect.x+rect.w,rect.x+rect.w,rect.x};
        float ys[4]={rect.y,rect.y,rect.y+rect.h,rect.y+rect.h};
        SDL_Vertex vertices[4];
        for(int i=0;i<4;i++)vertices[i]=(SDL_Vertex){{xs[i],ys[i]},
            {0,0,0,1-tig_color_get_red(fog.lerp_colors[i])/255.0f},{0,0}};
        const int indices[]={0,1,2,0,2,3};
        SDL_RenderGeometry(renderer,NULL,vertices,4,indices,6);
    }
    SDL_SetRenderDrawBlendMode(renderer,blend);
    SDL_SetRenderClipRect(renderer,clipped?&saved_clip:NULL);
    return ok;
}
