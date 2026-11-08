/* Native local-route fixture linked instead of wmap_ui.c. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "../source/src/ui/wmap_ui.c"

int64_t check_local_route_plan(void)
{
    int64_t pc=player_get_local_pc_obj(),from=obj_field_int64_get(pc,OBJ_F_LOCATION);
    int town=townmap_get(sector_id_from_loc(from));assert(town>0);
    assert(townmap_set_known(town,true));townmap_mark_visited(from);
    wmap_ui_open();wmap_ui_mode_set(WMAP_UI_MODE_TOWN);
    assert(wmap_ui_mode==WMAP_UI_MODE_TOWN);
    WmapRoute* route=&wmap_ui_routes[WMAP_ROUTE_TYPE_TOWN];route->length=0;
    for(int distance=80;distance>=40;distance-=20)for(int direction=1;direction<8;direction+=2){
        int64_t target;if(!location_in_range(from,direction,distance,&target))continue;
        if(townmap_get(sector_id_from_loc(target))!=town)continue;
        WmapCoords coords;townmap_loc_to_coords(&wmap_ui_tmi,target,&coords.x,&coords.y);
        if(coords.x<0||coords.y<0||coords.x>=wmap_ui_mode_info[wmap_ui_mode].map_width||coords.y>=wmap_ui_mode_info[wmap_ui_mode].map_height)continue;
        townmap_coords_to_loc(&wmap_ui_tmi,coords.x,coords.y,&target);
        TownRouteSearch search={.actor=pc,.deadline=SDL_GetTicks()+250};int64_t points[128];
        int count=town_route_plan(&search,from,target,128,points);
        if(count!=1)continue;
        assert(modern_town_route_add(&coords));assert(route->length==1);
        assert(route->waypoints[0].loc==target);
        int64_t dist=town_route_distance(from,target);
        printf("LOCAL_ROUTE distance=%lld previous_minimum=%lld points=%d probes=%d\n",(long long)dist,(long long)((dist+31)/32),route->length,search.probes);
        /* Reveal the test corridor and suppress incidental combat in this fixture. */
        for(int i=0;i<=distance;i+=4){
            int64_t loc;assert(location_in_range(from,direction,i,&loc));townmap_mark_visited(loc);
            LocRect bounds={LOCATION_GET_X(loc)-32,LOCATION_GET_Y(loc)-32,LOCATION_GET_X(loc)+32,LOCATION_GET_Y(loc)+32};
            ObjectList nearby;object_list_rect(&bounds,OBJ_TM_NPC,&nearby);
            for(ObjectNode* n=nearby.head;n;n=n->next)if(critter_pc_leader_get(n->obj)!=pc)object_flags_set(n->obj,OF_OFF);
            object_list_destroy(&nearby);
        }
        return target;
    }
    assert(!"no long reachable local route in fixture");return 0;
}
void check_local_route_start(void)
{
    assert(modern_town_route_start());assert(town_travel_actor==player_get_local_pc_obj());
}
bool check_local_route_active(void){return town_travel_actor!=0;}

/* Round-trip native TMN files only inside this tool's marked disposable copy. */
void check_local_route_notes(void)
{
    int64_t pc=player_get_local_pc_obj();
    int town=townmap_get(sector_id_from_loc(obj_field_int64_get(pc,OBJ_F_LOCATION)));
    int other=0;
    for(int i=1;i<512 && !other;i++)if(i!=town && townmap_name(i))other=i;
    assert(town && other);
    assert(wmap_ui_town_notes_select(town));
    for(int i=0;i<wmap_ui_num_town_notes;i++)sub_5642F0(&wmap_ui_town_notes[i]);
    wmap_ui_num_town_notes=0;
    WmapNote note={0};note.type=WMAP_NOTE_TYPE_NOTE;
    assert(townmap_info(town,&wmap_ui_tmi));
    townmap_loc_to_coords(&wmap_ui_tmi,obj_field_int64_get(pc,OBJ_F_LOCATION),&note.coords.x,&note.coords.y);
    strcpy(note.str,"Fixture local note");assert(wmap_note_add_mode(&note,WMAP_UI_MODE_TOWN));
    assert(wmap_ui_town_notes_select(other));
    for(int i=0;i<wmap_ui_num_town_notes;i++)sub_5642F0(&wmap_ui_town_notes[i]);
    wmap_ui_num_town_notes=0;
    strcpy(note.str,"Fixture other town");assert(wmap_note_add_mode(&note,WMAP_UI_MODE_TOWN));
    assert(wmap_ui_town_notes_select(town));assert(wmap_ui_num_town_notes==1);
    assert(!strcmp(wmap_ui_town_notes[0].str,"Fixture local note"));
    assert(wmap_ui_town_notes_select(other));assert(wmap_ui_num_town_notes==1);
    assert(!strcmp(wmap_ui_town_notes[0].str,"Fixture other town"));
    assert(wmap_ui_town_notes_select(town));assert(wmap_ui_num_town_notes==1);
    assert(!strcmp(wmap_ui_town_notes[0].str,"Fixture local note"));
    printf("NOTES native TMN round-trip town=%d other=%d local_count=%d foreign_count=0\n",town,other,wmap_ui_num_town_notes);
}
