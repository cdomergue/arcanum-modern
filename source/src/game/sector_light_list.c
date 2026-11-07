#include "game/sector_light_list.h"

// TODO: Get rid.
#include <stdio.h>

#include "game/obj_find.h"
#include "game/sector.h"
#include "game/tile.h"

static void sector_light_list_node_attach(SectorLightList* list, SectorBlockListNode* node);
static bool sector_light_list_node_detach(SectorLightList* list, Light* light, SectorBlockListNode** node_ptr);

// 0x4F7110
bool sector_light_list_init(SectorLightList* list)
{
    list->head = NULL;
    list->modified = false;

    return true;
}

// 0x4F7130
bool sector_light_list_reset(SectorLightList* list)
{
    SectorBlockListNode* node;
    SectorBlockListNode* next;

    node = list->head;
    while (node != NULL) {
        next = node->next;

        if (sub_4DD110(node->data)) {
            sub_4D9570(node->data);
        } else {
            light_stop_animating(node->data);
        }
        sector_block_list_node_destroy(node);

        node = next;
    }

    list->head = NULL;
    list->modified = false;

    return true;
}

// 0x4F71B0
bool sector_light_list_exit(SectorLightList* list)
{
    return sector_light_list_reset(list);
}

// 0x4F71C0
bool sector_light_list_add(SectorLightList* list, Light* light)
{
    SectorBlockListNode* node;

    node = sector_block_list_node_create();
    node->data = light;
    sector_light_list_node_attach(list, node);

    if (sub_4DD110(light)) {
        list->modified = true;
    }

    light_start_animating(light);

    return true;
}

// 0x4F7200
bool sector_light_list_remove(SectorLightList* list, Light* light)
{
    SectorBlockListNode* node;

    if (!sector_light_list_node_detach(list, light, &node)) {
        return false;
    }

    sector_block_list_node_destroy(node);

    if (sub_4DD110(light)) {
        list->modified = true;
    }

    return true;
}

// 0x4F7250
bool sector_light_list_update(SectorLightList* list, Light* light, int64_t loc, int offset_x, int offset_y)
{
    SectorBlockListNode* node;

    if (!sector_light_list_node_detach(list, light, &node)) {
        return false;
    }

    light_set_location(light, loc);
    light_set_offset(light, offset_x, offset_y);
    sector_light_list_node_attach(list, node);

    if (sub_4DD110(light)) {
        list->modified = true;
    }

    return true;
}

// 0x4F72C0
bool sector_light_list_load(SectorLightList* list, TigFile* stream, int64_t sector_id)
{
    int cnt;
    int idx;
    int x;
    int y;
    Light* light;
    int64_t sec;
    int64_t loc;

    if (tig_file_fread(&cnt, sizeof(cnt), 1, stream) != 1) {
        return false;
    }

    if (sector_id != -1) {
        // TODO: Check.
        x = (int)SECTOR_X(sector_id);
        y = (int)SECTOR_Y(sector_id);
    }

    for (idx = 0; idx < cnt; idx++) {
        if (!light_read(stream, &light)) {
            break;
        }

        if (sector_id != -1) {
            sec = tile_id_from_loc(light_get_location(light));
            // TODO: Check.
            loc = LOCATION_MAKE(SECTOR_X(sec) + x, SECTOR_Y(sec) + y);
            light_set_location(light, loc);
        }

        if (!sector_light_list_add(list, light)) {
            break;
        }
    }

    if (idx < cnt) {
        return false;
    }

    list->modified = false;
    return true;
}

// 0x4F7440
bool sector_light_list_load_with_dif(SectorLightList* list, TigFile* sec_stream, TigFile* dif_stream, int64_t sector_id)
{
    int cnt;
    int idx;
    bool dif;
    int extent;
    int64_t x;
    int64_t y;
    Light* light;
    int64_t loc;
    int tile;

    if (tig_file_fread(&cnt, sizeof(cnt), 1, sec_stream) != 1) {
        return false;
    }

    if (sector_id != -1) {
        x = SECTOR_X(sector_id);
        y = SECTOR_Y(sector_id);
    }

    dif = false;
    extent = 0;

    for (idx = 0; idx < cnt; idx++) {
        if (extent == 0) {
            if (tig_file_fread(&extent, sizeof(extent), 1, dif_stream) != 1) {
                break;
            }

            dif = (extent & 0x80000000) != 0;
            extent &= ~0x80000000;
        }

        if (!light_read(sec_stream, &light)) {
            break;
        }

        if (sector_id != -1) {
            loc = light_get_location(light);
            tile = tile_id_from_loc(loc);
            light_set_location(light, LOCATION_MAKE(x + (tile & 0x3F), y + ((tile >> 6) & 0x3F)));
            light_clear_modified(light);
        }

        if (dif) {
            if (!light_read_dif(dif_stream, &light)) {
                break;
            }
        }

        extent--;

        if (!sector_light_list_add(list, light)) {
            break;
        }
    }

    if (idx != cnt) {
        return false;
    }

    list->modified = true;

    return true;
}

// 0x4F7610
bool sector_light_list_save(SectorLightList* list, TigFile* stream)
{
    int cnt;
    int pos_cnt;
    int pos_curr;
    SectorBlockListNode* node;

    pos_cnt = tig_file_ftell(stream);
    if (pos_cnt == -1) {
        return false;
    }

    cnt = 0;
    if (tig_file_fwrite(&cnt, sizeof(cnt), 1, stream) != 1) {
        return false;
    }

    node = list->head;
    while (node != NULL) {
        if (sub_4DD110(node->data)) {
            light_clear_modified(node->data);
            if (!light_write(stream, node->data)) {
                return false;
            }
            cnt++;
        }
        node = node->next;
    }

    pos_curr = tig_file_ftell(stream);
    if (pos_curr == -1) {
        return false;
    }

    if (tig_file_fseek(stream, pos_cnt, SEEK_SET) != 0) {
        return false;
    }

    if (tig_file_fwrite(&cnt, sizeof(cnt), 1, stream) != 1) {
        return false;
    }

    if (tig_file_fseek(stream, pos_curr, SEEK_SET) != 0) {
        return false;
    }

    list->modified = false;

    return true;
}

// 0x4F7710
bool sector_light_list_is_modified(SectorLightList* list)
{
    SectorBlockListNode* node;

    if (list->modified) {
        return true;
    }

    node = list->head;
    while (node != NULL) {
        if (sub_4DD110(node->data)
            && light_is_modified(node->data)) {
            return true;
        }
        node = node->next;
    }

    return false;
}

// 0x4F7760
bool sector_light_list_save_with_dif(SectorLightList* list, TigFile* stream)
{
    SectorBlockListNode* node;
    Light* light;
    bool dif = false;
    int extent = 0;
    int pos = 0;
    int write_pos;

    node = list->head;
    while (node != NULL) {
        light = (Light*)node->data;
        if (sub_4DD110(light)) {
            if (light_is_modified(light)) {
                if (!dif) {
                    if (extent != 0) {
                        if (tig_file_fwrite(&extent, sizeof(extent), 1, stream) != 1) {
                            return false;
                        }
                    }

                    pos = tig_file_ftell(stream);
                    if (pos == -1) {
                        return false;
                    }

                    extent = 0;
                    if (tig_file_fwrite(&extent, sizeof(extent), 1, stream) != 1) {
                        return false;
                    }

                    dif = true;
                }

                if (!light_write_dif(stream, light)) {
                    return false;
                }
            } else {
                if (dif) {
                    write_pos = tig_file_ftell(stream);
                    if (write_pos == -1) {
                        return false;
                    }

                    if (tig_file_fseek(stream, pos, 0) != 0) {
                        return false;
                    }

                    extent |= 0x80000000;
                    if (tig_file_fwrite(&extent, sizeof(extent), 1, stream) != 1) {
                        return false;
                    }

                    if (tig_file_fseek(stream, write_pos, 0) != 0) {
                        return false;
                    }

                    dif = false;
                    extent = 0;
                }
            }

            extent++;
        }
        node = node->next;
    }

    if (dif) {
        write_pos = tig_file_ftell(stream);
        if (write_pos == -1) {
            return false;
        }

        if (tig_file_fseek(stream, pos, 0) != 0) {
            return false;
        }

        extent |= 0x80000000;
        if (tig_file_fwrite(&extent, sizeof(extent), 1, stream) != 1) {
            return false;
        }

        if (tig_file_fseek(stream, write_pos, 0) != 0) {
            return false;
        }
    } else {
        if (tig_file_fwrite(&extent, sizeof(extent), 1, stream) != 1) {
            return false;
        }
    }

    return true;
}

// 0x4F7940
void sector_light_list_fold(SectorLightList* list, int64_t sector_id, SectorTileList* tiles)
{
    int hour;
    bool is_day;
    int64_t obj;
    FindNode* iter;
    unsigned int flags;
    int64_t loc;
    int tile;
    Light* light;
    int overlay;
    bool add;

    hour = datetime_current_hour();
    is_day = hour >= 6 && hour < 18;

    if (obj_find_walk_first(sector_id, &obj, &iter)) {
        do {
            flags = obj_field_int32_get(obj, OBJ_F_FLAGS);
            if ((flags & OF_INVENTORY) == 0) {
                add = true;
                if (obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_SCENERY
                    && (obj_field_int32_get(obj, OBJ_F_SCENERY_FLAGS) & OSCF_NOCTURNAL) != 0) {
                    loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
                    tile = tile_id_from_loc(loc);
                    if (tig_art_tile_id_type_get(tiles->art_ids[tile]) == 0) {
                        if ((flags & OF_OFF) != 0) {
                            obj_field_int32_set(obj, OBJ_F_FLAGS, flags & ~OF_OFF);
                            light_unset_flags(obj, LF_OFF);
                        }
                    } else {
                        if (is_day) {
                            if ((flags & OF_OFF) == 0) {
                                obj_field_int32_set(obj, OBJ_F_FLAGS, flags | OF_OFF);
                                light_set_flags(obj, LF_OFF);
                            }
                            add = false;
                        } else {
                            if ((flags & OF_OFF) != 0) {
                                obj_field_int32_set(obj, OBJ_F_FLAGS, flags & ~OF_OFF);
                                light_unset_flags(obj, LF_OFF);
                            }
                        }
                    }
                }

                if (add) {
                    sub_4D9590(obj, false);
                    light = (Light*)obj_field_ptr_get(obj, OBJ_F_LIGHT_HANDLE);
                    if (light != NULL) {
                        sector_light_list_add(list, light);
                    }

                    for (overlay = 0; overlay < 4; overlay++) {
                        light = (Light*)obj_arrayfield_ptr_get(obj, OBJ_F_OVERLAY_LIGHT_HANDLES, overlay);
                        if (light != NULL) {
                            sector_light_list_add(list, light);
                        }
                    }
                }
            }
        } while (obj_find_walk_next(&obj, &iter));
    }
}

// 0x4F7B40
void sector_light_list_node_attach(SectorLightList* list, SectorBlockListNode* node)
{
    node->next = list->head;
    list->head = node;
}

// 0x4F7B50
bool sector_light_list_node_detach(SectorLightList* list, Light* light, SectorBlockListNode** node_ptr)
{
    SectorBlockListNode* node;
    SectorBlockListNode* prev;

    prev = list->head;
    node = list->head;
    while (node != NULL) {
        if (node->data == light) {
            if (prev == node) {
                list->head = node->next;
            } else {
                prev->next = node->next;
            }
            *node_ptr = node;
            node->next = NULL;
            return true;
        }
        prev = node;
        node = node->next;
    }

    return false;
}
