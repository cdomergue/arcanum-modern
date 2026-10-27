/* Companion fixture for check-map-zoom.c; replaces wmap_ui.c at test link time. */
#undef NDEBUG
#include <assert.h>
#include "../source/src/ui/wmap_ui.c"

void check_travel_map_open(void)
{
    /* A town save can exercise the world UI without changing the saved game. */
    wmap_ui_mode_set(WMAP_UI_MODE_WORLD);
    dword_66D880 = 1;
}

void check_travel_map_backend(void (*moving_frame)(void))
{
    assert(wmap_ui_mode == WMAP_UI_MODE_WORLD);
    WmapInfo* info = &wmap_ui_mode_info[WMAP_UI_MODE_WORLD];
    tig_window_handle_t window = wmap_ui_window;
    TigRect rect = info->rect;
    int x = info->field_34, y = info->field_38;
    WmapRoute route_before = wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD];
    assert(modern_map_travel_buffer(0, 0, info->map_width, info->map_height));
    assert(wmap_ui_window == window && !memcmp(&rect, &info->rect, sizeof(rect)));
    assert(info->field_34 == x && info->field_38 == y);
    assert(!memcmp(&route_before, &wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD], sizeof(route_before)));
    assert(!modern_map_travel_buffer(0, 0, 0, info->map_height));
    assert(wmap_ui_window == window);

    WmapRoute* route = &wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD];
    route->length = 0;
    bool added = false;
    for (int dy = -20; dy <= 20 && !added; dy += 10) {
        for (int dx = -20; dx <= 20 && !added; dx += 10) {
            if (!dx && !dy) continue;
            int xx = info->field_3C.x + dx, yy = info->field_3C.y + dy;
            route->field_3C8 = 1;
            modern_map_travel_mouse(xx, yy, TIG_MESSAGE_MOUSE_MOVE);
            modern_map_travel_mouse(xx, yy, TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN);
            modern_map_travel_mouse(xx, yy, TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP);
            if (route->length == 1) {
                assert(route->waypoints[0].coords.x == xx && route->waypoints[0].coords.y == yy);
                added = true;
            }
        }
    }
    assert(added);
    assert(modern_map_travel_buffer(0, 0, info->map_width, info->map_height));
    modern_map_action(3);
    assert(wmap_ui_state == WMAP_UI_STATE_MOVING);
    ModernTravelMap travel;
    assert(modern_map_travel_state(&travel));
    moving_frame();
    int old_x=travel.x,old_y=travel.y;
    wmap_ui_bkg_process_callback(NULL);
    assert(wmap_ui_created && wmap_ui_state==WMAP_UI_STATE_MOVING);
    assert(modern_map_travel_state(&travel)&&(travel.x!=old_x||travel.y!=old_y));
    moving_frame();
    modern_map_travel_mouse(route->waypoints[0].coords.x, route->waypoints[0].coords.y, TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP);
    assert(route->length == 0 && wmap_ui_state == WMAP_UI_STATE_NORMAL);
    assert(modern_map_travel_buffer(0, 0, info->map_width, info->map_height));
    printf("PASS: travel render restores native viewport; absolute route clicks, live travel movement and route cancellation\n");
}
