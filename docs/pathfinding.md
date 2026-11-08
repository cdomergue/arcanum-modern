# Tile pathfinding and diagnostics

A ground click is projected into world tile coordinates by the interface, then
submitted to the native movement animation. Straight routes use a collision-
checked fast path. Routes around obstacles use a bounded A* search with a binary
heap. Both paths preserve portal/window permissions, collision queries, known
trap avoidance, fire avoidance and the native stealth lighting costs.

The search covers a box derived from the available step budget, including room
for detours. Each search returns at most the caller's buffer capacity and 200
steps. The existing animation consumes up to eight steps at a time and replans
remaining movement toward the same destination. `AnimPath` and its serialized
layout are unchanged. Very distant destinations or detours requiring more than
200 steps in one plan can still fail; this is not an unlimited world route planner.
The separate world-map sector search is unchanged.

Obstacle searches have a 50 ms budget, checked every 32 expansions, and at most
201 × 201 nodes for a single destination (up to 205 × 205 for nearby
interaction candidates). The budget includes allocation and collision queries; a single
slow sector read cannot be interrupted partway through. Automatic NPC searches
also retain their existing shared count/time quotas. The selected companion
uses the same search eligibility as the local hero rather than an ambient NPC's
quota. Memory belongs to each query and is freed afterwards.

`ArcanumModern/pathfinding.log` records failed, distant or slow queries for the
local hero and selected companion. Fields include SDL uptime, runtime actor
handle, absolute endpoint deltas, buffer capacity, traversal flags, returned
segment length, search result, node count, grid dimensions and elapsed time.
It excludes names, absolute tile positions and save contents. Logging is limited
to 20 records per second, rotating at 1 MiB to `pathfinding.previous.log`.

Reasons include `found`, `range` (endpoint exceeds capacity), `unreachable`,
`time_budget`, `npc_quota`, `capacity` and `allocation`. `direct_or_blocked`
indicates the direct path or an early rejection, such as a blocked destination,
before obstacle search. A successful query is not proof the actor has arrived:
combat, a new command or moving obstacles can interrupt subsequent animation.

Verification:

- `python3 tests/test_pathfinding.py`: actual native entry points on deterministic
  terrain, long straight/detoured routes, repeated eight-step segments, blocked
  targets, doors/windows, traps, adjacency, buffer guards, NPC quotas, time budget,
  map boundaries and diagnostic rotation.
- `python3 tests/test_path_capacity.py`: production animation capacity calculation.
- After `./build.sh`, `python3 tools/build-pathfinding-check.py` builds the native
  probe. Run `build/check-pathfinding GAME` only in an isolated copy bearing
  `.map-export-sandbox`. It creates a fresh party, disables ambient combat in the
  test viewport, expands the world view to 3840 × 2160 and submits projected ground
  clicks through `intgame_process_event`. It checks actual arrival beyond 32 tiles
  for both hero and selected companion, identifies its process/window before input,
  and never writes a save slot. It uses SDL's dummy/software backend, so this probes
  the native click handler and movement rather than physical mouse input.

## Local-map itineraries

The local-map planner validates legs through `anim_preview_route_segment`, using
the same tile pathfinder and shared 200-step capacity as ground movement. It
tries a long leg first, then shorter legs and nearby intermediate candidates
when obstacle detours exceed that capacity. Planning is limited to 128 probes
and 250 ms per click; a failed addition leaves the previous itinerary intact.
Successive user-added destinations remain explicit stops. The map displays links
between these stops; the native walker resolves terrain detours within each leg.

`python3 tests/test_map_route.py` covers fewer automatic waypoints, long-leg
fallback, append preservation, allocation growth, timeout and atomic failure.
`tests/test_map_travel.py` checks the streaming execution and interruptions.
After building, run `python3 tools/build-local-route-check.py` and then
`build/check-local-route GAME` on a marked disposable game copy. It creates a
fresh party, plans a distant destination using the real local-map coordinates,
checks the reduced count, captures the complete map in `local-route-reduced.png`,
and verifies arrival after starting travel from that map. It does not write a
save slot. The world-map sector planner is independent of this local planner.


## Object interaction approaches

Containers, portals, scenery and corpses use reachable standing tiles rather
than an unchecked final step into the target object. Candidate tiles are within
the native use range (one or two tiles); every intervening edge rejects closed
intermediate doors and intact windows. Only a portal that is itself the target
is excluded temporarily, restoring its exact raw flags without text/light side
effects. Ordinary objects are skipped during the reach query. A
single A* search chooses a reachable candidate. Live movement retains eight-step
segments and checks the target again before use, including in combat. Combat use
costs include the full approach route and the two-point action; an unreachable
target is rejected, and a blocked final use does not spend the action's points.

`ArcanumModern/interaction-path.log` records approach outcomes, runtime actor and
target handles, combat state, range, candidate and step counts, and timing. It
excludes names and absolute locations, is limited to 20 records per second, and
rotates at 1 MiB to `interaction-path.previous.log`.

`python3 tests/test_interaction_path.py` exercises wall-edge detours, shared
exploration/combat entry points, streamed arrival, combat use estimates, blocked
targets, flag restoration, two-tile reach checks and diagnostic rotation. After
building, `python3 tools/build-interaction-path-check.py` builds a native probe;
run `build/check-interaction-path GAME` only in a marked disposable copy. It
creates a wall-backed chest, checks both approach entry points and executes the
container-use animation in exploration and turn-based combat. It verifies arrival,
the loot interface and that actual AP consumption matches the full route estimate
plus the use action. It uses SDL's dummy/software backend and never loads or
writes a save slot.
