# HD local maps

The engine renders the cache from the installed game scenery at one pixel per
world pixel. It generates maps for locations that have an original local map;
coverage depends on the installed modules and game data.

## Automatic preparation

On Windows, macOS and Linux, the first launch prepares missing maps and displays
a progress bar. No Python installation, compiler or additional download is
needed. **Play now** or Escape stops preparation and opens the game with the
original maps. The next launch resumes preparation, reusing completed tiles.
Closing the window also stops generation.

The cache is stored in `modern-ui/local-maps/` inside the game folder, with a
subfolder for each module, such as `Arcanum`. Allow approximately **8 GB** for
Arcanum and Vormantown, plus temporary working space. Actual size depends on the
installed game data. Generated maps are local files and are not included in
release packages.

Deleting the cache causes preparation to run again at the next launch. You can
choose **Play now** to continue using the original maps. If preparation fails,
check the error code on screen and `modern-ui/.map-work/generation.log`.

## Map content and display

Images contain static scenery, including opaque roofs, without characters,
portable objects, added annotations or location names. Covered buildings remain
hidden; interior maps retain their own geometry. Doors appear in their initial
state: changes during play are not reflected in the cache. Text that is part of
a scenery sprite remains part of that sprite.

The interface displays tiles at its native resolution. Exploration fog is
calculated by `townmap_tile_blit_info`. Party markers, routes, notes and localized
labels are drawn separately by the game. A missing or incomplete map uses the
original rendering. The texture cache holds up to 32 tiles.

## Cache format and validation

Each tile covers up to 1024 × 1024 world pixels. Four lossless PNG files store
the same area at full, half, quarter and eighth resolution. Files are named
`level_column_row.png`; edge tiles may be smaller. `manifest.json` records map
identifiers, dimensions and rendering settings.

All four levels are checked for dimensions and PNG CRCs before reuse. A map is
available only after its `complete` marker has been written. A complete cache is
reused; missing or changed cache files trigger validation, and source changes
trigger regeneration. Rewriting loose source files with identical contents does
not invalidate the cache solely because their timestamps changed.

Generation runs in a separate process using `modern-ui/.map-work/`. Archives are
hard-linked when possible and copied otherwise; loose files are copied and saves
are excluded. A lock prevents simultaneous generators from writing to the same
cache. The working directory can be deleted while the game is closed.

## Command-line generation

Run the engine with the absolute path to the game folder:

```sh
./arcanum-modern --modern-generate-maps /absolute/path/to/Arcanum
```

On Windows, use `arcanum-modern.exe`. On macOS, use the executable inside
`Arcanum Modern.app/Contents/MacOS/`.

An optional final argument limits the number of newly rendered tiles. Reaching
that limit returns exit code 75. The progress file at
`modern-ui/.map-work/progress` contains `validated_tiles total stage`, where the
stage is:

| Value | Meaning |
| --- | --- |
| 0 | Preparing |
| 1 | Rendering or validating |
| 2 | Complete |
| 3 | Stopped by an error or the tile limit |

## Developer tools on macOS

After building the engine, run these commands from the repository root:

```sh
python3 tools/build-map-exporter.py
python3 tools/generate-map-cache.py
python3 tools/verify-map-cache.py
```

The exporter renders offscreen in an isolated installation under
`build/map-export`. It links DAT archives, copies loose data and excludes saves.
Source game files are read-only inputs, and module patches use their normal
load order. Output and working directories can be selected with `--output` and
`--work`.

Interrupted exports resume tiles whose four levels are incomplete. A map's
`complete` marker is written only after all its tiles are ready. Use fresh output
and working directories when changing the source data or rendering settings.
Manifests must declare `roofs: true`.

`tools/verify-map-cache.py` checks expected tiles, dimensions, PNG CRCs and the
absence of textual PNG metadata. `tests/test_map_archive.py` checks compressed
DAT reading and local-map identifiers in sectors.

`tools/check-map-cache.c` loads a copied save in a test installation, captures
the native map, checks the fallback without a cache, and checks that unexplored
areas remain black until explored. Run it only in an isolated installation with
the required `.map-export-sandbox` marker:

```sh
python3 tools/build-map-exporter.py --source tools/check-map-cache.c --name check-map-cache --exclude-object src/ui/modern_ui.c.o
build/macos-arm64/check-map-cache "$PWD/build/map-preview" Slot0001 "$PWD/build/map-preview.png"
```

The native generation check covers truncated sectors, PNG validation and resume:

```sh
python3 tools/build-map-exporter.py --source tools/check-map-generation.c --name check-map-generation --exclude-object src/ui/modern_map_generator.c.o
build/macos-arm64/check-map-generation /tmp/arcanum-map-generation-check
```

To check first-launch preparation and resume without preprocessed map objects,
provide an engine executable and a game installation:

```sh
python3 tools/check-map-first-launch.py /absolute/path/to/arcanum-modern /absolute/path/to/Arcanum
```

The check uses temporary installations, renders two tiles per module across two
launches, and leaves the supplied game installation unchanged.
