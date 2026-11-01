# Script examples

Read the scripts section of `worlded-tutorial.html` for the setup and expected
behavior. These are author-written examples; no campaign resources are included.

- `49000_reward_chest.wscript`: award 50 coins once, then open an unlocked chest.
- `49001_reward_keeper.wscript` and `.wdlg`: let an NPC offer a one-time payment.
- `reward_chest.native.txt`: reference listing for the four-row native chest
  example. Recreate these rows in the native editor; this listing is not a
  structured source file and is not consumed by Build project.

The numeric IDs are examples, not reserved allocations. Check your game and mods
before using them. Rename a dialogue and its companion script together if you
choose another ID. Keep counter 0 initially zero on each attachment.

To build the structured examples, open this folder as a project in Script Editor
and choose Build project, with an empty output folder outside this directory.
Copy the examples to your own project before editing them. The compiled files
still need to be attached to objects and exported with a scene or mod.
