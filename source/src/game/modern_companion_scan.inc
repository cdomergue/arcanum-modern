/* Read only metadata for visited maps. The base mobile stream supplies object
 * types for the saved diffs; nothing enters the engine's object pool. */
#define COMPANION_SCAN_OBJECTS 65536
bool modern_companions_coordinates(int map,int64_t location,int* west,int* south)
{
    char* name;char path[TIG_MAX_PATH];MapProperties properties;
    if(!map_get_name(map,&name))return false;
    snprintf(path,sizeof(path),"maps\\%s\\map.prp",name);
    TigFile* f=tig_file_fopen(path,"rb");if(!f)return false;
    bool ok=tig_file_fread(&properties,sizeof(properties),1,f)==1;
    tig_file_fclose(f);
    int64_t x=LOCATION_GET_X(location),y=LOCATION_GET_Y(location);
    if(!ok || properties.width<=0 || properties.width>INT32_MAX || properties.height<=0
        || properties.height>INT32_MAX || x>=properties.width || y>=properties.height)return false;
    *west=(int)(properties.width/64-x/64);*south=(int)(y/64);
    return true;
}
static bool companion_scan_stream(TigFile* f,CompanionSnapshot** out,int* count)
{
    *out=NULL;*count=0;int capacity=0;
    if(tig_file_filelength(f)>64*1024*1024)return false;
    while(tig_file_ftell(f)<tig_file_filelength(f)) {
        if(*count>=COMPANION_SCAN_OBJECTS)return false;
        if(*count==capacity) {
            capacity=capacity?capacity*2:256;
            CompanionSnapshot* next=realloc(*out,capacity*sizeof(**out));
            if(!next)return false;*out=next;
        }
        if(!obj_companion_snapshot(f,&(*out)[*count]))return false;
        (*count)++;
    }
    return true;
}
static int companion_snapshot_compare(const void* a,const void* b)
{
    const ObjectID* x=&((const CompanionSnapshot*)a)->id;
    const ObjectID* y=&((const CompanionSnapshot*)b)->id;
    if(x->type!=y->type)return x->type<y->type?-1:1;
    /* Ignore ObjectID padding, just as objid_is_equal does. */
    return memcmp(&x->d,&y->d,sizeof(x->d));
}
static bool companion_scan_map(const char* folder,int map)
{
    char path[TIG_MAX_PATH];bool ok=true;TigFile* f;
    CompanionSnapshot* objects=NULL;int count=0;
    snprintf(path,sizeof(path),"%s\\mobile.md",folder);
    TigFile* diff=tig_file_fopen(path,"rb");
    if(diff) {
        snprintf(path,sizeof(path),"maps\\%s",map_list_info[map-1].name);
        map_obfuscate_name(path+5);
        f=tig_file_fopen(path,"rb");TigGuid guid;
        ok=f && tig_file_fread(&guid,sizeof(guid),1,f)==1 && companion_scan_stream(f,&objects,&count);
        if(f)tig_file_fclose(f);
        if(ok && count)qsort(objects,count,sizeof(*objects),companion_snapshot_compare);
        while(ok && tig_file_ftell(diff)<tig_file_filelength(diff)) {
            CompanionSnapshot key={0};
            if(tig_file_fread(&key.id,24,1,diff)!=1){ok=false;break;}
            CompanionSnapshot* found=count?bsearch(&key,objects,count,sizeof(*objects),companion_snapshot_compare):NULL;
            if(!found || !obj_companion_diff(diff,found)){ok=false;break;}
        }
        tig_file_fclose(diff);
        snprintf(path,sizeof(path),"%s\\mobile.des",folder);
        f=tig_file_fopen(path,"rb");
        if(f) {
            if(tig_file_filelength(f)>COMPANION_SCAN_OBJECTS*24 || tig_file_filelength(f)%24)ok=false;
            CompanionSnapshot key={0};
            while(ok && tig_file_fread(&key.id,24,1,f)==1) {
                CompanionSnapshot* found=count?bsearch(&key,objects,count,sizeof(*objects),companion_snapshot_compare):NULL;
                if(found)found->object_flags|=OF_DESTROYED;
            }
            tig_file_fclose(f);
        }
        /* Do not import a partial map if any diff could not be interpreted. */
        if(ok)for(int i=0;i<count;i++)modern_companions_import(&objects[i],map);
        free(objects);objects=NULL;count=0;
    }
    snprintf(path,sizeof(path),"%s\\mobile.mdy",folder);
    f=tig_file_fopen(path,"rb");
    if(f) {
        bool valid=companion_scan_stream(f,&objects,&count);tig_file_fclose(f);
        if(valid)for(int i=0;i<count;i++)modern_companions_import(&objects[i],map);
        free(objects);ok=ok && valid;
    }
    return ok;
}
void modern_companions_scan(void)
{
    unsigned scanned=0,skipped=0;
    for(int i=1;i<=map_list_info_count;i++) {
        if(i==map_current_map())continue; /* Loaded objects are authoritative. */
        char folder[TIG_MAX_PATH];
        snprintf(folder,sizeof(folder),"Save\\Current\\maps\\%s",map_list_info[i-1].name);
        if(!tig_file_is_directory(folder))continue;
        scanned++;if(!companion_scan_map(folder,i))skipped++;
    }
    tig_debug_printf("Companion journal: visited maps scanned=%u, incomplete=%u\n",scanned,skipped);
}
