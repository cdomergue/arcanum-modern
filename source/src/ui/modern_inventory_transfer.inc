static bool modern_inventory_bulk_available(void)
{
    return inven_ui_created && !tig_net_is_active()
        && obj_handle_is_valid(inven_ui_pc_obj) && obj_handle_is_valid(qword_6813A8)
        && inven_ui_pc_obj != qword_6813A8
        && (inven_ui_mode == INVEN_UI_MODE_LOOT
            || (inven_ui_mode == INVEN_UI_MODE_BARTER && dword_6810FC
                && obj_handle_is_valid(qword_682C78)
                && critter_leader_get(qword_682C78) == inven_ui_pc_obj));
}

static void modern_inventory_transfer_all(bool to_player)
{
    if (!modern_inventory_bulk_available() || inven_ui_drag_item_obj
        || combat_input_actor() != inven_ui_pc_obj
        || (combat_turn_based_is_active() && !combat_input_ready(inven_ui_pc_obj))) return;
    int64_t pc = inven_ui_pc_obj, owner = qword_6813A8;
    int64_t from = to_player ? owner : pc, to = to_player ? pc : owner;
    int64_t* items;
    int count = item_list_get(from, &items);
    for (int i = 0; i < count; i++) {
        if (!modern_inventory_bulk_available() || inven_ui_pc_obj != pc || qword_6813A8 != owner) break;
        int64_t parent;
        int slot;
        if (!obj_handle_is_valid(items[i]) || !item_parent(items[i], &parent) || parent != from
            || (obj_field_int32_get(items[i], OBJ_F_FLAGS) & OF_OFF)
            || (obj_field_int32_get(items[i], OBJ_F_ITEM_FLAGS) & OIF_NO_DISPLAY)
            || IS_WEAR_INV_LOC(item_inventory_location_get(items[i]))
            || item_check_remove(items[i]) != ITEM_CANNOT_OK
            || item_check_insert(items[i], to, &slot) != ITEM_CANNOT_OK) continue;
        qword_681450 = from;
        dword_681440 = 0;
        sub_5788C0(items[i], to, slot, 0x21);
    }
    item_list_free(items);
    if (modern_inventory_bulk_available() && inven_ui_pc_obj == pc && qword_6813A8 == owner) {
        qword_681458 = 0;
        dword_681440 = -1;
        item_inventory_slots_get(pc, dword_68111C);
        item_inventory_slots_get(owner, dword_681518);
        redraw_inven(false);
    }
}
