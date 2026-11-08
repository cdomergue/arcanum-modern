/* Bounded A* for native tile routes. The serialized 200-step AnimPath is unchanged. */
enum {PATH_SEARCH_STEPS=PATH_ROUTE_MAX_STEPS};
typedef struct PathTrace {const char* reason;int expanded,width,height;} PathTrace;
static PathTrace path_trace;
typedef struct PathNode {int cost,parent,steps,rotation,heap_pos;} PathNode;
typedef struct PathHeap {PathNode* nodes;int* items;int count,width,minimum;const int* targets;int target_count;} PathHeap;

static int path_estimate(int a,int b,int width)
{
    int dx=abs(a%width-b%width),dy=abs(a/width-b/width);
    return dx>dy?dx:dy;
}
static int path_goal_estimate(PathHeap* h,int node)
{
    int result=INT_MAX;
    for(int i=0;i<h->target_count;i++){
        int distance=path_estimate(node,h->targets[i],h->width);
        if(distance<result)result=distance;
    }
    return result;
}
static bool path_heap_less(PathHeap* h,int a,int b)
{
    int ha=path_goal_estimate(h,a),hb=path_goal_estimate(h,b);
    int fa=h->nodes[a].cost+ha*h->minimum,fb=h->nodes[b].cost+hb*h->minimum;
    return fa<fb || (fa==fb && ha<hb);
}
static void path_heap_swap(PathHeap* h,int a,int b)
{
    int node=h->items[a];h->items[a]=h->items[b];h->items[b]=node;
    h->nodes[h->items[a]].heap_pos=a;h->nodes[node].heap_pos=b;
}
static void path_heap_update(PathHeap* h,int node)
{
    int i=h->nodes[node].heap_pos;
    if(i<0){i=h->count++;h->items[i]=node;h->nodes[node].heap_pos=i;}
    while(i>0){int parent=(i-1)/2;if(!path_heap_less(h,h->items[i],h->items[parent]))break;path_heap_swap(h,i,parent);i=parent;}
}
static int path_heap_pop(PathHeap* h)
{
    int result=h->items[0];path_heap_swap(h,0,--h->count);h->nodes[result].heap_pos=-1;
    int i=0;
    while(i*2+1<h->count){
        int child=i*2+1;if(child+1<h->count && path_heap_less(h,h->items[child+1],h->items[child]))child++;
        if(!path_heap_less(h,h->items[child],h->items[i]))break;
        path_heap_swap(h,i,child);i=child;
    }
    return result;
}
static bool path_manual_actor(int64_t obj)
{
    return obj && (player_is_local_pc_obj(obj) || obj==modern_party_selected());
}
static void path_trace_log(PathCreateInfo* info,int result,tig_duration_t elapsed)
{
    int64_t dx=llabs(LOCATION_GET_X(info->to)-LOCATION_GET_X(info->from));
    int64_t dy=llabs(LOCATION_GET_Y(info->to)-LOCATION_GET_Y(info->from));
    if(!path_manual_actor(info->obj) || (result>0 && dx<=32 && dy<=32 && elapsed<20))return;
    static Uint64 window;static int count;Uint64 now=SDL_GetTicks();
    if(now-window>=1000){window=now;count=0;}if(count++>=20)return;
    int saved_errno=errno;FILE* log=fopen("ArcanumModern/pathfinding.log","ab+");
    if(log){
        fseek(log,0,SEEK_END);
        if(ftell(log)>=1024*1024){
            fclose(log);log=NULL;
            if(remove("ArcanumModern/pathfinding.previous.log")==0 || errno==ENOENT)
                if(rename("ArcanumModern/pathfinding.log","ArcanumModern/pathfinding.previous.log")==0)
                    log=fopen("ArcanumModern/pathfinding.log","ab");
        }
        if(log){
            fprintf(log,"ticks_ms=%llu actor=%llu dx=%lld dy=%lld capacity=%d flags=%x result=%d reason=%s expanded=%d grid=%dx%d elapsed_ms=%d\n",
                (unsigned long long)now,(unsigned long long)info->obj,(long long)dx,(long long)dy,
                info->max_rotations,info->flags,result,path_trace.reason,path_trace.expanded,path_trace.width,path_trace.height,(int)elapsed);
            fclose(log);
        }
    }
    errno=saved_errno;
}
static int modern_path_search_goals(PathCreateInfo* p,tig_duration_t budget,const int64_t* goals,int goal_count)
{
    tig_timestamp_t started;tig_timer_now(&started);
    int64_t fx=LOCATION_GET_X(p->from),fy=LOCATION_GET_Y(p->from);
    int limit=p->max_rotations<PATH_SEARCH_STEPS?p->max_rotations:PATH_SEARCH_STEPS;
    path_trace.reason="range";
    if(limit<=0 || goal_count<=0 || goal_count>25)return 0;
    int64_t accepted[25],minx=INT64_MAX,miny=INT64_MAX,maxx=INT64_MIN,maxy=INT64_MIN;
    int count=0;
    for(int i=0;i<goal_count;i++){
        int64_t x=LOCATION_GET_X(goals[i]),y=LOCATION_GET_Y(goals[i]);
        if(llabs(x-fx)>limit || llabs(y-fy)>limit)continue;
        accepted[count++]=goals[i];
        if(x<minx)minx=x;
        if(x>maxx)maxx=x;
        if(y<miny)miny=y;
        if(y>maxy)maxy=y;
    }
    if(!count)return 0;
    bool quota=obj_field_int32_get(p->obj,OBJ_F_TYPE)==OBJ_TYPE_NPC && !path_manual_actor(p->obj);
    if(quota){
        if(dword_5DE5F8==0 || tig_timer_elapsed(dword_5DE5F8)>=1000){tig_timer_now(&dword_5DE5F8);dword_5DE5FC=0;dword_5DE600=0;}
        if(dword_5DE5FC>path_limit || dword_5DE600>path_time_limit){path_trace.reason="npc_quota";return 0;}
        dword_5DE5FC++;
    }
    /* Union of endpoint-centered boxes: keep bounded detours to every goal. */
    int64_t ox=(fx<minx?fx:minx)-(limit-llabs(minx-fx))/2;
    int64_t oy=(fy<miny?fy:miny)-(limit-llabs(miny-fy))/2;
    int64_t right=(fx>maxx?fx:maxx)+(limit-llabs(maxx-fx)+1)/2;
    int64_t bottom=(fy>maxy?fy:maxy)+(limit-llabs(maxy-fy)+1)/2;
    int width=(int)(right-ox+1),height=(int)(bottom-oy+1);
    path_trace.width=width;path_trace.height=height;
    int targets[25];
    for(int i=0;i<count;i++)targets[i]=(int)(LOCATION_GET_X(accepted[i])-ox)+(int)(LOCATION_GET_Y(accepted[i])-oy)*width;
    int size=width*height,start=(int)(fx-ox)+(int)(fy-oy)*width;
    PathNode* nodes=malloc((size_t)size*sizeof(*nodes));int* items=malloc((size_t)size*sizeof(*items));
    path_trace.reason="allocation";
    if(!nodes || !items){free(nodes);free(items);return 0;}
    for(int i=0;i<size;i++)nodes[i]=(PathNode){INT_MAX,-1,0,0,-1};
    PathHeap heap={nodes,items,0,width,(p->flags&PATH_FLAG_0x0200)?4:10,targets,count};
    nodes[start].cost=0;path_heap_update(&heap,start);
    unsigned flags=path_translate_flags(p->flags);bool loose=(p->flags&PATH_FLAG_0x0008)!=0;
    int result=0;path_trace.reason="unreachable";
    while(heap.count){
        if((path_trace.expanded&31)==0 && tig_timer_elapsed(started)>=budget){path_trace.reason="time_budget";break;}
        int current=path_heap_pop(&heap);PathNode* node=&nodes[current];path_trace.expanded++;
        if(path_goal_estimate(&heap,current)==0){
            int steps=0,n=current;
            while(n!=start && n>=0 && steps<=limit){steps++;n=nodes[n].parent;}
            if(n!=start || steps>limit){path_trace.reason="capacity";continue;}
            for(int i=steps-1,n=current;i>=0;i--){p->rotations[i]=(int8_t)nodes[n].rotation;n=nodes[n].parent;}
            p->to=location_make(ox+current%width,oy+current/width);
            result=steps-((p->flags&PATH_FLAG_0x0001)!=0);path_trace.reason="found";break;
        }
        int64_t loc=location_make(ox+current%width,oy+current/width);
        for(int rot=0;rot<8;rot++){
            int64_t next;if(!location_in_dir(loc,rot,&next))continue;
            int64_t x=LOCATION_GET_X(next)-ox,y=LOCATION_GET_Y(next)-oy;
            if(x<0 || x>=width || y<0 || y>=height)continue;
            int neighbor=(int)x+(int)y*width;
            int steps=node->steps+1;
            if(steps+path_goal_estimate(&heap,neighbor)>limit)continue;
            bool adjacent=next==p->to && (p->flags&PATH_FLAG_0x0001),window=false;
            if(!adjacent && trap_party_avoids_location(p->obj,next))continue;
            if(!(p->flags&PATH_FLAG_0x0040) && !adjacent){
                if(!loose && tile_is_blocking(next,loose))continue;
                int64_t block=0;int type=0;
                if(object_calc_traversal_cost(p->obj,loc,rot,flags,&block,&type,&window) || block)continue;
            }
            int cost=10;int64_t trap=get_trap_at_location(next);
            if(trap && trap_is_spotted(p->obj,trap))cost+=80;
            if(window)cost+=50;
            if((p->flags&PATH_FLAG_0x0400) && get_fire_at_location(next))cost+=80;
            if(p->flags&PATH_FLAG_0x0200){
                int src=sub_4D9240(loc,0,0),dst=sub_4D9240(next,0,0);
                if(dst-src>20)cost+=40;else if(src-dst>20)cost-=6;
            }
            cost+=node->cost+(node->parent>=0 && node->rotation!=rot);
            if(cost>=nodes[neighbor].cost)continue;
            nodes[neighbor].cost=cost;nodes[neighbor].parent=current;nodes[neighbor].rotation=rot;nodes[neighbor].steps=steps;
            path_heap_update(&heap,neighbor);
        }
    }
    free(items);free(nodes);
    if(quota)dword_5DE600+=tig_timer_elapsed(started);
    return result;
}

static int modern_path_search(PathCreateInfo* p,tig_duration_t budget)
{
    int64_t goal=p->to;
    return modern_path_search_goals(p,budget,&goal,1);
}

/* Full route to the cheapest reachable standing tile, not an unchecked final
 * edge into an occupied object. Callers retain the usual eight-step streaming. */
int path_make_to_goals(PathCreateInfo* p,const int64_t* goals,int count)
{
    tig_timestamp_t started;tig_timer_now(&started);
    path_trace=(PathTrace){"interaction",0,0,0};
    p->field_24=0;
    p->flags&=~PATH_FLAG_0x0001;
    int result=modern_path_search_goals(p,50,goals,count);
    path_trace_log(p,result,tig_timer_elapsed(started));
    return result;
}
