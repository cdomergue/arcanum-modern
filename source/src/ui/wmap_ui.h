#ifndef ARCANUM_UI_WMAP_UI_H_
#define ARCANUM_UI_WMAP_UI_H_

#include "game/context.h"
#include "game/timeevent.h"

bool wmap_ui_init(GameInitInfo* init_info);
void wmap_ui_exit(void);
void wmap_ui_reset(void);
bool wmap_ui_save(TigFile* stream);
bool wmap_ui_load(GameLoadInfo* load_info);
void wmap_ui_encounter_start(void);
bool wmap_ui_is_encounter(void);
void wmap_ui_encounter_end(void);
void wmap_ui_open(void);
void wmap_ui_select(int64_t obj, int spell);
void wmap_ui_close(void);
int wmap_ui_is_created(void);
void wmap_ui_scroll(int direction);
void sub_564000(int a1);
void wmap_ui_mark_townmap(int64_t obj);
bool wmap_ui_bkg_process_callback(TimeEvent* timeevent);
void wmap_ui_notify_sector_changed(int64_t pc_obj, int64_t sec);
void wmap_ui_get_current_location(int64_t* loc_ptr);

typedef struct ModernLocalMap {
    int width,height,columns,rows,count;
} ModernLocalMap;
bool modern_map_local_state(ModernLocalMap* state);
bool modern_map_local_point(int index,int* x,int* y);
const char* modern_map_local_note(int x,int y,int radius);
void modern_map_local_mouse(int x,int y,int event,int radius);
void* modern_map_local_tile(int index);
void modern_map_local_tile_release(int index);
void modern_map_route_clear(void);
void modern_map_route_tick(void);
void modern_map_route_cancel_tracking(void);

typedef struct ModernTravelMap {
    int width,height,x,y;
} ModernTravelMap;
bool modern_map_travel_state(ModernTravelMap* state);
void* modern_map_travel_buffer(int x,int y,int width,int height);
void modern_map_travel_mouse(int x,int y,int event);
void* modern_map_continent_buffer(int* width,int* height);
typedef struct ModernMapLabel {
    int x,y,offset_x,offset_y;
    const char* text;
} ModernMapLabel;
int modern_map_travel_labels(ModernMapLabel* labels,int capacity);
typedef struct ModernMapMarker {
    int x,y;
    tig_art_id_t art_id;
    bool selectable;
} ModernMapMarker;
int modern_map_travel_markers(ModernMapMarker* markers,int capacity);

#endif /* ARCANUM_UI_WMAP_UI_H_ */
