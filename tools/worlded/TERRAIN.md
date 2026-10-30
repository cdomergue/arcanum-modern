# Terrain brush format references

The installed `art/tile/tilename.mes` defines materials and legal material pairs.
Material groups begin at 0 (outdoor, flippable), 100 (outdoor, non-flippable),
200 (indoor, flippable) and 300 (indoor, non-flippable). Entries from 400 describe
pairs by concatenating their three-character material names. The file is read
from the installation with normal patch priority; no game text or art is bundled.

The pinned CE implementation in `source/src/game/a_name.c` documents the table
loading, pair lookup, intermediate-material search and filename generation.
`source/first_party/tig/src/art.c` defines tile AID fields and mirror behavior.
Targeted inspection of the original local WorldEd.exe confirmed the same rules:
filename generation at 0x4563e0, intermediate-material lookup at 0x456de0 and
direct adjacency at 0x456ea0; editing code calls the latter two at 0x4581a7 and
0x458107 respectively. Addresses apply to that local executable, not all releases.

`terrain_brush.hpp` implements these format rules with its own brush algorithm.
It stores materials on shared tile corners, propagates required intermediate
materials through the pair graph, and emits tile AIDs with matching corner masks.
It does not claim to reproduce the original editor's complete brush algorithm.
The four mask bits follow north, west, south and east in the isometric view.
Flippable masks are canonicalized before choosing an available ART variant;
the filename mask alphabet is `06b489237ea5dc10`.

For water, the graph can require several intermediate materials between grass
and deep water. A two-material blend alone is insufficient. Every changed tile
is checked against the available art before committing one undo operation.
Unchanged tiles retain their exact bytes. Incompatible transitions and strokes
that would alter the sector's boundary corners are refused atomically.

Tests cover shared-corner consistency, intermediate water materials, boundary
refusal and single-step undo. The optional GUI `--layers` integration test renders
roofs, water transitions, characters and scripts using a local game installation;
it modifies only in-memory drafts and writes its captures to the specified path.
