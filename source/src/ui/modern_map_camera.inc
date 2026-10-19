typedef struct MapCamera {
    float x,y,width,height,scale,min_scale,max_scale;
} MapCamera;

static void map_camera_clamp(MapCamera* camera,float width,float height)
{
    camera->x=fmaxf(0,fminf(camera->x,width-camera->width));
    camera->y=fmaxf(0,fminf(camera->y,height-camera->height));
}

static void map_camera_zoom(MapCamera* camera,float factor,float anchor_x,float anchor_y,
    float viewport_width,float viewport_height,float width,float height)
{
    float x=camera->x+anchor_x/camera->scale,y=camera->y+anchor_y/camera->scale;
    camera->scale=fmaxf(camera->min_scale,fminf(camera->max_scale,camera->scale*factor));
    camera->width=fminf(width,viewport_width/camera->scale);
    camera->height=fminf(height,viewport_height/camera->scale);
    camera->x=x-anchor_x/camera->scale;camera->y=y-anchor_y/camera->scale;
    map_camera_clamp(camera,width,height);
}
