if (obj_following_pc(self, player)) {
    dialog(10);
    return;
}
dialog(1);
return;

label recruit:
if (!obj_following_pc(self, player)) {
    critter_follow(self, player);
}
return;
