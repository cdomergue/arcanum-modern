# Companion journal

Open the journal with **L**, then select **Companions**. Each entry shows a name,
the latest recorded situation, a last known area when it can be identified, and
map coordinates. Search filters the entries; the journal's text-size and page
controls work in this chapter too.

The journal distinguishes companions in the party, companions waiting somewhere,
departures, expired waiting periods, observed deaths, and companions no longer
available at their recorded location. A departure does not imply a particular
reason: scripts, dismissal and other game events can all remove a follower.
The journal records a waiting period's expiry when that event occurs.

Locations are last known positions. The journal does not move companions,
teleport the player, reveal unexplored maps, or guarantee that a departed
companion is still at the recorded location. Coordinates belong to the recorded
map; an interior's coordinates are not a destination on the continent map.

## Existing games

Current followers and NPCs whose saved state still says they are waiting for
your character can be recovered, including NPCs stored in other visited maps.
Recovered entries have no historical date. This works from the party's object
identities and saved relationships, without a list of named campaign companions.

Some earlier departures cannot be reconstructed. The game's discontent flag
does not by itself establish that an NPC followed your character or explain
why they left. NPCs with no remaining relationship and no journal record are
therefore omitted. Incomplete or unsupported saved object records are skipped.

## Saves and compatibility

Companion entries belong to the loaded save. Loading an earlier save restores
its own journal state; information from a later save is not carried backwards.
The journal is stored inside the normal save archive on the next successful
save. It is separate from personal map annotations and quest favorites.

The register holds up to 128 distinct companions. It stores their most recent
known situation, rather than a chronological conversation or event history.
Saving through an older engine that does not support this chapter may lose
information about subsequent departures.

## Development verification

`tools/check-companion-journal.inc` is part of the native `check-text-size`
harness. Set `ARCANUM_CHECK_COMPANIONS=1` and run the harness with an isolated,
marked game copy, an output directory, and a save containing a follower.
The harness writes test slots `Slot9901` and `Slot9902` in that copy only.

It checks native waiting, rejoining, dismissal and expiry events; save/load
branches; full-object and differential metadata parsing; truncated input;
off-map recovery; exclusion of unrelated or foreign-owned NPCs; and French and
English companion text at multiple sizes. Captures include the first and last
spreads, the search-empty state, and an 800×600 window. Inspect the complete
screens, including pagination and footer text.
