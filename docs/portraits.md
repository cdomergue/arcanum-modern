# Custom portraits

Choose **Import portrait** below the portrait on the character creation screen.
Select a PNG, JPEG or BMP from your computer, then drag the image to frame the
face. Zoom with the mouse wheel or the **+** and **-** buttons. **Reset crop**
restores the centered crop.

The two previews show the converted portrait at the game's large and small
sizes. Select **Import and use** to add it to the portrait catalogue and select
it immediately. **Cancel** leaves the current portrait and catalogue unchanged.
Importing is local; images are not sent to a server. The source image is never
modified.

Images may be up to 8,192 pixels on either side, 24 megapixels in total and
64 MB on disk. Transparent areas are flattened onto a dark brown background.

## Changing a portrait during a game

Open your hero's character sheet and click the portrait. Use the arrows to
browse existing portraits, then choose **Use this portrait**, or select
**Import an image** to open the crop editor. **Cancel** keeps the current
portrait. Changing a portrait does not change the character's attributes.

Save your game to keep the change. This option is available for your own hero
in single-player games.

## Classic game compatibility

Renaissance generates the native portrait pair: indexed, 256-color BMP images
at 64 × 64 and 128 × 128 pixels. The large image has the `_b.bmp` suffix. Both
are placed in `data/portrait`, and an entry is added to `userport.mes` in the same
folder. Generated names use the `ANY` prefix, so the portrait is available for
every race and gender.

The import interface belongs to Renaissance; the generated files use the
classic game's custom portrait format. The classic executable can read them
from the same installation. This does not remove any other mod dependencies a
character or saved game may have.

Saved games store a portrait identifier, not the image. When moving a character
to another installation, copy the portrait BMP files and the corresponding
`userport.mes` entries, preserving their identifiers. Do not overwrite another
installation's catalogue or reuse an identifier already assigned to a different
portrait.

## Existing portraits and backups

Importing appends an entry without renumbering or replacing existing portraits.
The catalogue supports 999 user portraits. Its identifiers must be consecutive,
starting at 1, because classic portrait navigation stops at gaps. If the
catalogue was edited externally while the game was open, restart the game before
importing again.

Each import that updates an existing catalogue keeps a copy alongside it with
an `ANYren*.mes.bak` filename. Back up `data/portrait` with
your saved games. Do not delete a portrait used by an existing character.

## Development checks

```sh
python tests/test_portrait_image.py
python tests/test_portrait_catalogue.py
python tests/test_keyboard_capture.py
```

The tests compile production C code with a C compiler. Manual validation should
use an isolated installation and cover the native file picker, accented paths,
cropping, zoom, cancellation, both previews, portrait selection and persistence
after restarting. Verify that output files are 8-bit indexed BMPs with the
expected dimensions and that existing catalogue entries remain unchanged.
