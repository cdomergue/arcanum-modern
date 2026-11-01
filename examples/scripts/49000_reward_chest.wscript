// Attach to SAP_USE on a new, unlocked container.
// Counter 0 belongs to this attachment; its initial value must be zero.
if (counter(0) == 0) {
    counter(0) = 1;
    adjust_gold(player, 50);
}
return default;
