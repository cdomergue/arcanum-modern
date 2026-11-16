# ART Editor Renaissance

ART Editor opens Arcanum's indexed sprites and animations, edits their anchors,
offsets and palettes, and converts PNG sequences into game-ready `.art` files.
It is included in the WorldED Windows package alongside WorldED and Script Editor.
The three programs share the resource library and Qt runtime. The editor uses
C++17 and Qt 6 and builds on Windows, Linux and macOS with the WorldED project.

## Open a sprite

Run `art-editor.exe`, then choose **File > Open ART** for a loose file or
**File > Choose Arcanum installation** to browse installed resources. The resource
list combines loose files, DAT archives, module resources and active patches in
the same priority order as WorldED. Filter by resource path and double-click to
open a sprite. Files opened from this list are saved to a location you choose;
the editor does not write back into DAT archives.

In WorldED, select an ART resource and choose **Resources > ART editor**. The
editor opens that resource with the same installation. Script Editor's **Tools**
menu also opens ART Editor. ART Editor's **Tools** menu opens WorldED and Script
Editor with the current installation.

```text
art-editor [FILE.art] [--game DIRECTORY] [--resource art/PATH.art]
```

Keep the executables beside their shared DLLs and plugin folders. Game assets
are read from your own installation and are not supplied in the editor package.

## Preview and edit

- **Zoom** selects an integer scale from 1× to 16× without smoothing. The
  checkerboard shows transparency. Drag the view to pan.
- **Play / Pause**, the frame slider and frame number navigate the animation.
  Frame numbers start at zero. **Frames / second** changes its playback rate.
- **Direction** selects one of the stored directions: one for static art or
  eight for directional art.
- **Critter mirroring** previews directions 1, 2 and 3 using horizontally
  mirrored directions 7, 6 and 5, as the game does for characters. This is a
  preview option; it does not replace the stored directions.
- Drag the cyan crosshair to change **Anchor X / Y**, or enter coordinates in
  the fields. **Offset X / Y** stores movement offsets. With mirroring enabled,
  fields show the original source direction; dragging the crosshair converts
  back to those coordinates. The status line identifies that direction.
- **Action frame** stores the frame associated with the animation's action.
  Use −1 for no action frame.
- Select a palette and double-click a cell in its 16×16 grid to edit a color.
  Index 0 remains transparent regardless of its RGB color. **Copy palette to
  another slot** creates or replaces one of the four palette slots.

**Save** writes the current document; **Save as** creates a separate ART file.
Existing ART image indices, compression, unused header fields and trailing bytes
are preserved when editing metadata or colors. Saves are written atomically.
Closing or replacing an edited document offers Save, Discard and Cancel.

## Import a PNG animation

Choose **File > Import PNG animation**, select the frames, and select one or
eight directions. The selected filenames are sorted lexically. For eight
directions, provide equal-sized groups in direction order 0–7; for example,
`r0_f0000.png`, `r0_f0001.png`, `r1_f0000.png`, `r1_f0001.png`, and so on.
Each image becomes a complete frame; sprite sheets must be split beforehand.

The entire sequence receives a common palette with at most 255 opaque colors
and transparent index 0. Images with at most 255 distinct opaque colors keep
their RGB values exactly. More colorful images are quantized with a shared
weighted median-cut palette. Alpha below 128 becomes transparent; all other
pixels become fully opaque. ART does not support partial alpha. Imported frames
start with a bottom-center anchor, zero offsets and a playback rate of 10 FPS.
Adjust these fields before saving.

New ART files use RLE for frames where it is smaller than raw pixels. Different
frame dimensions are supported. Import is bounded to 16 million total pixels,
4096 frames per direction and dimensions no larger than 16384 pixels per axis.

## Export PNGs

**Export frame as PNG** writes the selected stored source frame with the selected
palette, including transparency. **Export all frames as PNG** writes every stored
direction and frame into an empty folder, using names such as `r0_f0000.png`.
Exports contain the stored images rather than the mirrored preview. PNGs do not
carry ART anchors, offsets, action-frame settings or additional palettes; keep
the ART file when those properties need to be preserved.

## Build and checks

Follow [Building WorldED](worlded-build.md) with `WORLDED_BUILD_GUI=ON`.
This builds `art-editor` and the `art-editor-check` test executable using the
existing `worlded_formats` library. No Go or Fyne runtime is needed.

```sh
ctest --test-dir build/worlded -C Release -R art_editor --output-on-failure
```

The tests cover lossless edits, raw and RLE data, malformed input, four palettes,
eight directions, shared PNG quantization, mirroring, anchor dragging and saving.
`art-editor --smoke-test` checks startup with a synthetic sprite. For an optional
read-only check of up to 1000 installed sprites:

```sh
art-editor-check -platform offscreen --game /path/to/Arcanum
```
