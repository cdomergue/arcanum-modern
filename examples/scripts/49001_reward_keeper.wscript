// Attach to SAP_DIALOG on a project NPC.
// Keep the matching .wdlg beside this source when building a project.
dialog(1);
return;

label reward:
if (counter(0) == 0) {
    counter(0) = 1;
    adjust_gold(player, 50);
}
return;
